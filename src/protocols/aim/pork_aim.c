/*
** pork_aim.c - Interface to libfaim.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This file is heavily based on src/protocols/oscar/oscar.c from gaim,
** which is more or less to say that chunks of code have been mostly
** copied from there. In turn, I think that oscar.c file was mostly lifted
** from faimtest. Any copyrights there apply here.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <config.h>

#include <unistd.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <time.h>
#include <sys/time.h>

#include <libfaim/faimconfig.h>
#include <libfaim/aim.h>

#include <pork.h>
#include <pork_missing.h>
#include <pork_util.h>
#include <pork_list.h>
#include <pork_buddy.h>
#include <pork_imwindow.h>
#include <pork_proto.h>
#include <pork_acct.h>
#include <pork_misc.h>
#include <pork_cstr.h>
#include <pork_color.h>
#include <pork_conf.h>
#include <pork_html.h>
#include <pork_events.h>
#include <pork_io.h>
#include <pork_format.h>
#include <pork_screen.h>
#include <pork_screen_io.h>
#include <pork_chat.h>
#include <pork_inet.h>
#include <pork_imsg.h>
#include <pork_transfer.h>
#include <pork_msg.h>
#include <pork_opt.h>

#include <pork_aim.h>
#include <pork_aim_proto.h>

static u_int32_t pork_caps =	AIM_CAPS_CHAT | AIM_CAPS_INTEROPERATE |
								AIM_CAPS_SENDFILE;
unsigned short aimport = 0;

static char *msgerrreason[] = {
	"Invalid error",
	"Invalid SNAC",
	"Rate to host",
	"Rate to client",
	"Not logged in",
	"Service unavailable",
	"Service not defined",
	"Obsolete SNAC",
	"Not supported by host",
	"Not supported by client",
	"Refused by client",
	"Reply too big",
	"Responses lost",
	"Request denied",
	"Busted SNAC payload",
	"Insufficient rights",
	"In local permit/deny",
	"Sender's warning level is too high",
	"Receiver's warning level is too high",
	"User temporarily unavailable",
	"No match",
	"List overflow",
	"Request ambiguous",
	"Queue full",
	"Not while on AOL"
};


static FAIM_CB(aim_connerr);
static FAIM_CB(aim_parse_login);
static FAIM_CB(aim_parse_authresp);

static void aim_debug(	aim_session_t *session __notused,
						int level __notused,
						const char *format __notused,
						va_list va __notused)
{
#ifdef ENABLE_DEBUGGING
	char buf[8192];

	vsnprintf(buf, sizeof(buf), format, va);
	debug("LIBFAIM DEBUG: lev: %d: %s", level, buf);
#endif
}

int aim_sock_connect(	const char *ip,
						struct sockaddr_storage *laddr,
						int *sock)
{
	struct sockaddr_storage ss;
	char *addr;
	char *port;
	in_port_t port_num = FAIM_LOGIN_PORT;

	port = getenv("AIM_PORT");
	if (port != NULL) {
		u_int32_t temp;

		if (str_to_uint(port, &temp) != -1)
			port_num = temp;
	}

	addr = xstrdup(ip);

	port = strchr(addr, ':');
	if (port != NULL) {
		*port++ = '\0';

		if (get_port(port, &port_num) != 0) {
			screen_err_msg("Error: Invalid port: %s", port);
			goto err_out;
		}
	}

	if (get_addr(addr, &ss) != 0) {
		screen_err_msg("Error: Invalid host: %s", addr);
		goto err_out;
	}

	free(addr);
	return (nb_connect(&ss, laddr, port_num, sock));

err_out:
	free(addr);
	return (-1);
}

static void aim_print_info(	char *user,
							char *profile,
							char *away_msg,
							u_int32_t warn_level,
							u_int32_t idle_time,
							u_int32_t online_since,
							u_int32_t member_since,
							int print_all)
{
	char buf[8192];
	struct imwindow *win = cur_window();

	if (print_all) {
		int ret;

		ret = fill_format_str(OPT_FORMAT_WHOIS_NAME, buf, sizeof(buf),
				user, warn_level, idle_time, online_since, member_since);
		if (ret > 0)
			screen_print_str(win, buf, (size_t) ret, MSG_TYPE_CMD_OUTPUT);

		if (idle_time > 0) {
			ret = fill_format_str(OPT_FORMAT_WHOIS_IDLE, buf, sizeof(buf),
					user, warn_level, idle_time, online_since, member_since);
			if (ret > 0)
				screen_print_str(win, buf, (size_t) ret, MSG_TYPE_CMD_OUTPUT);
		}

		if (warn_level > 0) {
			ret = fill_format_str(OPT_FORMAT_WHOIS_WARNLEVEL, buf,
				sizeof(buf), user, warn_level, idle_time,
				online_since, member_since);

			if (ret > 0)
				screen_print_str(win, buf, (size_t) ret, MSG_TYPE_CMD_OUTPUT);
		}

		if (online_since) {
			ret = fill_format_str(OPT_FORMAT_WHOIS_SIGNON, buf, sizeof(buf),
					user, warn_level, idle_time, online_since, member_since);
			if (ret > 0)
				screen_print_str(win, buf, (size_t) ret, MSG_TYPE_CMD_OUTPUT);
		}

		if (member_since) {
			ret = fill_format_str(OPT_FORMAT_WHOIS_MEMBER, buf, sizeof(buf),
					user, warn_level, idle_time, online_since, member_since);

			if (ret > 0)
				screen_print_str(win, buf, (size_t) ret, MSG_TYPE_CMD_OUTPUT);
		}

		if (profile) {
			ret = fill_format_str(OPT_FORMAT_WHOIS_USERINFO, buf, sizeof(buf),
					user, warn_level, idle_time, online_since,
					member_since, profile);

			if (ret > 0)
				screen_print_str(win, buf, (size_t) ret, MSG_TYPE_CMD_OUTPUT);
		}
	}

	if (away_msg) {
		int ret;

		ret = fill_format_str(OPT_FORMAT_WHOIS_AWAY, buf, sizeof(buf),
				user, warn_level, idle_time, online_since,
				member_since, away_msg);

		if (ret > 0)
			screen_print_str(win, buf, (size_t) ret, MSG_TYPE_CMD_OUTPUT);
	}
}

static inline void aim_disconnected_chat(	struct pork_acct *acct,
											struct chatroom *chat)
{
	chat_forced_leave(acct, chat->title, "the server", "disconnected");
}

void aim_listen_conn_event(int sock, u_int32_t cond, void *data) {
	aim_conn_t *conn = (aim_conn_t *) data;
	aim_session_t *session = conn->sessv;
	struct pork_acct *acct = session->aux_data;
	struct aim_priv *priv = acct->data;

	if (cond == IO_COND_DEAD ||
		aim_handlerendconnect(&priv->aim_session, conn) < 0)
	{
		struct file_transfer *xfer = conn->priv;

		pork_sock_err(acct, sock);
		close(sock);
		transfer_lost(xfer);
	}
}

static void aim_kill_pending_chats(struct pork_acct *acct) {
	struct aim_priv *priv = acct->data;
	dlist_t *cur;

	cur = priv->chat_create_list;
	while (cur != NULL) {
		struct chatroom_info *info = cur->data;
		dlist_t *next = cur->next;

		free(info->name);
		free(info);
		free(cur);

		cur = next;
	}
}

static void aim_conn_event(int sock, u_int32_t cond, void *data) {
	aim_conn_t *conn = (aim_conn_t *) data;
	aim_session_t *session = conn->sessv;
	struct pork_acct *acct = session->aux_data;
	struct aim_priv *priv = acct->data;

	if (cond == IO_COND_DEAD) {
		pork_sock_err(acct, sock);

		switch (conn->type) {
			case AIM_CONN_TYPE_BOS:
				debug("bos con for %s died", acct->username);
				aim_connect_abort(acct);
				pork_acct_disconnected(acct);
				break;

			case AIM_CONN_TYPE_CHAT:
				debug("chat con for %s died", acct->username);
				aim_disconnected_chat(acct, conn->priv);
				break;

			case AIM_CONN_TYPE_CHATNAV:
				debug("chatnav con for %s died", acct->username);
				pork_io_del(conn);
				aim_kill_pending_chats(acct);
				aim_conn_kill(session, &conn);
				priv->chatnav_conn = NULL;
				break;

			case AIM_CONN_TYPE_RENDEZVOUS:
				debug("rendezvous con for %s died", acct->username);
				if (conn->subtype == AIM_CONN_SUBTYPE_OFT_SENDFILE)
					transfer_lost(conn->priv);
				else {
					debug("rendezvous con for %s died (not sendfile)",
						acct->username);
					pork_io_del(conn);
					aim_conn_kill(session, &conn);
				}

				break;
		}

		return;
	}

	if (cond & IO_COND_READ) {
		if (aim_get_command(session, conn) >= 0) {
			aim_rxdispatch(session);

			/*
			** If the whole session is terminated (e.g. after a failed
			** login, we can't destroy the account inside the callback
			** (before returning from aim_rxdispatch), or it will crash
			** the client. Instead, this will have to do.
			*/

			if (session->connlist == NULL) {
				pork_acct_disconnected(acct);
				return;
			}
		} else {
			if (conn->type == AIM_CONN_TYPE_RENDEZVOUS &&
				conn->subtype == AIM_CONN_SUBTYPE_OFT_SENDFILE)
			{
				aim_conn_kill(session, &conn);
			}
		}
	}

	if (cond & IO_COND_WRITE)
		aim_tx_flushqueue(session);
}

void aim_connected(int sock, u_int32_t cond __notused, void *data) {
	aim_conn_t *conn = data;
	aim_session_t *session = conn->sessv;
	struct pork_acct *acct = session->aux_data;
	int ret;

	pork_io_del(conn);

	ret = sock_is_error(sock);
	if (ret != 0) {
		close(sock);
		screen_err_msg("network error: %s: %s", acct->username, strerror(ret));

		switch (conn->type) {
			case AIM_CONN_TYPE_CHATNAV:
				aim_kill_pending_chats(acct);
				aim_conn_kill(session, &conn);
				screen_err_msg("%s is unable to connect to the chatnav server",
					acct->username);
				break;

			case AIM_CONN_TYPE_CHAT: {
				struct chatroom *chat = conn->priv;

				chat_forced_leave(acct, chat->title,
					"the server", "can't connect");
				break;
			}

			case AIM_CONN_TYPE_AUTH:
			case AIM_CONN_TYPE_BOS:
				screen_err_msg("Unable to login as %s", acct->username);
				pork_acct_disconnected(acct);
				break;

			case AIM_CONN_TYPE_RENDEZVOUS:
				if (conn->subtype == AIM_CONN_SUBTYPE_OFT_SENDFILE)
					transfer_lost(conn->priv);
				break;
		}
	} else {
		conn->fd = sock;
		sock_setflags(sock, 0);
		aim_conn_completeconnect(session, conn);

		pork_io_add(sock, IO_COND_RW, conn, conn,
			aim_conn_event);

		if (conn->type == AIM_CONN_TYPE_RENDEZVOUS &&
			conn->subtype == AIM_CONN_SUBTYPE_OFT_SENDFILE)
		{
			struct file_transfer *xfer = conn->priv;
			struct aim_oft_info *oft_info = xfer->data;

			xfer->sock = sock;
			aim_im_sendch2_sendfile_accept(oft_info->sess, oft_info);
		}
	}
}

static int aim_send_buddy_list(aim_session_t *session, struct pork_acct *acct) {
	dlist_t *cur;

	cur = acct->buddy_pref->group_list;
	while (cur != NULL) {
		struct bgroup *group = cur->data;
		dlist_t *cur_buddy = group->member_list;

		while (cur_buddy != NULL) {
			struct buddy *buddy = cur_buddy->data;

			if (aim_ssi_itemlist_exists(session->ssi.local, buddy->nname)) {
				char *alias;

				alias = aim_ssi_getalias(session->ssi.local,
							group->name, buddy->nname);

				if (alias != NULL) {
					free(buddy->name);
					buddy->name = alias;
				} else {
					aim_ssi_aliasbuddy(session,
						group->name, buddy->nname, buddy->name);
				}
			} else {
				aim_ssi_addbuddy(session, buddy->nname,
					group->name, buddy->name, NULL, NULL, 0);
			}

			cur_buddy = cur_buddy->next;
		}

		cur = cur->next;
	}

	return (1);
}

static int aim_send_permit_list(aim_session_t *session, struct pork_acct *acct)
{
	if (acct->buddy_pref->permit_list != NULL) {
		dlist_t *cur;

		cur = acct->buddy_pref->permit_list;
		while (cur != NULL) {
			aim_ssi_addpermit(session, (char *) cur->data);
			cur = cur->next;
		}
	}

	return (1);
}

static int aim_send_block_list(aim_session_t *session, struct pork_acct *acct) {
	if (acct->buddy_pref->block_list != NULL) {
		dlist_t *cur;

		cur = acct->buddy_pref->block_list;
		while (cur != NULL) {
			aim_ssi_adddeny(session, (char *) cur->data);
			cur = cur->next;
		}
	}

	return (1);
}

int aim_login(struct pork_acct *acct, char *s) {
	struct aim_priv *priv = acct->data;
	int ret;
	int sock;
	aim_conn_t *auth_conn;
	char *server = s;
	char *ptr;

	if (!server)
		server = FAIM_LOGIN_SERVER;
	ptr = strchr(server, ':');
	if (!ptr) {
		server = getenv("AIM_SERVER");
		if (server == NULL);
			server = FAIM_LOGIN_SERVER;
	}

	acct->connected = 0;
	screen_win_msg(cur_window(), 1, 1, 0,
		MSG_TYPE_STATUS, "Logging in as %s...", acct->username);

	auth_conn = aim_newconn(&priv->aim_session, AIM_CONN_TYPE_AUTH, NULL);
	if (auth_conn == NULL) {
		screen_err_msg("Connection error while logging in as %s",
			acct->username);

		return (-1);
	}

	ret = aim_sock_connect(server, &acct->laddr, &sock);
	if (ret == 0) {
		aim_connected(sock, 0, auth_conn);
	} else if (ret == -EINPROGRESS) {
		auth_conn->status |= AIM_CONN_STATUS_INPROGRESS;
		pork_io_add(sock, IO_COND_WRITE, auth_conn, auth_conn, aim_connected);
	} else {
		screen_err_msg("Error connecting to the authorizer server as %s",
			acct->username);
		aim_conn_kill(&priv->aim_session, &auth_conn);
		return (-1);
	}

	aim_conn_addhandler(&priv->aim_session, auth_conn,
		AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR, aim_connerr, 0);

	aim_conn_addhandler(&priv->aim_session, auth_conn,
		AIM_CB_FAM_ATH, AIM_CB_ATH_AUTHRESPONSE, aim_parse_login, 0);

	aim_conn_addhandler(&priv->aim_session, auth_conn,
		AIM_CB_FAM_ATH, AIM_CB_ATH_LOGINRESPONSE, aim_parse_authresp, 0);

	aim_request_login(&priv->aim_session, auth_conn, acct->username);
	return (1);
}

static dlist_t *aim_find_chat(	struct pork_acct *acct,
								const char *name,
								int exchange)
{
	dlist_t *cur;

	cur = acct->chat_list;
	while (cur != NULL) {
		struct chatroom *chat = cur->data;
		struct aim_chat *a_chat = chat->data;

		if (!strcasecmp(name, a_chat->title) && exchange == a_chat->exchange)
			return (cur);

		cur = cur->next;
	}

	return (NULL);
}

static char *get_chatname(const char *orig) {
	char *p;
	char *s;
	char *ret;

	if (orig == NULL)
		return (xstrdup(orig));

	p = strrchr(orig, '-');
	if (p == NULL)
		return (xstrdup(orig));

	s = xstrdup(++p);
	ret = s;

	/*
	** The length of s will always be less than or equal to the length of p.
	*/

	while (*p != '\0') {
		if (*p != '%')
			*s = *p++;
		else {
			char buf[4];

			if (xstrncpy(buf, ++p, sizeof(buf)) == -1) {
				free(ret);
				return (NULL);
			}

			p += 2;

			*s = (char) strtol(buf, NULL, 16);
		}

		s++;
	}

	*s = '\0';
	return (ret);
}

int aim_kill_all_conn(struct pork_acct *acct) {
	struct aim_priv *priv = acct->data;
	aim_conn_t *conn;
	dlist_t *cur;

	if (priv == NULL)
		return (-1);

	conn = priv->aim_session.connlist;

	if (acct->connected) {
		aim_rxdispatch(&priv->aim_session);
		aim_tx_flushqueue(&priv->aim_session);
	}

	while (conn != NULL) {
		pork_io_del(conn);
		conn = conn->next;
	}

	aim_kill_pending_chats(acct);

	cur = acct->chat_list;
	while (cur != NULL) {
		dlist_t *next = cur->next;
		struct chatroom *chat = cur->data;

		aim_chat_free(acct, chat->data);
		cur = next;
	}

	aim_session_kill(&priv->aim_session);
	return (0);
}

int aim_setup(struct pork_acct *acct) {
	struct aim_priv *priv = acct->data;
	aim_session_t *session = &priv->aim_session;

	memset(session, 0, sizeof(*session));

	aim_session_init(session, 0, 0);
	aim_setdebuggingcb(session, aim_debug);

	aim_tx_setenqueue(session, AIM_TX_IMMEDIATE, NULL);
	session->aux_data = acct;

	return (0);
}

int aim_chat_parse_name(const char *name, struct chatroom_info *info) {
	int exchange;
	char *p;

	p = strchr(name, '/');
	if (p != NULL) {
		p++;

		if (str_to_int(p, &exchange) != 0)
			return (-1);

		if (exchange < 4)
			exchange = 4;
		else if (exchange > 16)
			exchange = 16;

		info->name = xstrndup(name, p - name - 1);
		info->exchange = exchange;
	} else {
		info->name = xstrdup(name);
		info->exchange = AIM_DEFAULT_CHAT_EXCHANGE;
	}

	return (0);
}

dlist_t *aim_find_chat_name(struct pork_acct *acct, char *name) {
	dlist_t *ret;
	struct chatroom_info info;

	if (aim_chat_parse_name(name, &info) != 0) {
		debug("aim_chat_parse_name failed for %s", name);
		return (NULL);
	}

	ret = aim_find_chat(acct, info.name, info.exchange);
	free(info.name);
	return (ret);
}


/*
** libfaim callback handlers.
*/

static FAIM_CB(aim_conn_dead) {
	aim_conn_t *conn;

	va_list ap;

	va_start(ap, fr);
	conn = va_arg(ap, aim_conn_t *);
	va_end(ap);

	if (conn != NULL) {
		aim_session_t *session = aim_conn_getsess(conn);

		if (session != NULL && session->aux_data != NULL && conn->fd >= 0)
			pork_sock_err(session->aux_data, conn->fd);

		pork_io_dead(conn);
	}

	return (1);
}

static FAIM_CB(aim_connerr) {
	u_int16_t code;
	char *msg;
	va_list ap;

	va_start(ap, fr);
	code = (u_int16_t) va_arg(ap, int);
	msg = va_arg(ap, char *);
	va_end(ap);

	debug("aim_connerr: msg=%s code=%hu", msg, code);

	if (fr == NULL || fr->conn == NULL)
		return (1);

	debug("conn=%p, fd=%d, type=%d", fr->conn, fr->conn->fd, fr->conn->type);

	if (fr->conn->type == AIM_CONN_TYPE_BOS) {
		if (code == 0x01) {
			debug("somebody signed on from another location with this screenname.");
		} else {
			debug("Unknown error");
		}
	}

	return (1);
}

FAIM_CB(aim_file_transfer_dead) {
	aim_conn_t *conn;
	va_list ap;
	struct file_transfer *xfer;

	va_start(ap, fr);
	conn = va_arg(ap, aim_conn_t *);
	va_end(ap);

	if (conn == NULL) {
		debug("file xfer conn null");
		return (-1);
	}

	xfer = conn->priv;
	if (xfer != NULL) {
		pork_sock_err(xfer->acct, conn->fd);
		pork_io_del(xfer);
	} else {
		debug("xfer died with conn->priv NULL");
	}

	pork_io_del(conn);
	xfer->protocol_flags |= AIM_XFER_IN_HANDLER;
	transfer_lost(xfer);
	return (-1);
}

static FAIM_CB(aim_recv_bos_rights) {
	va_list ap;
	struct pork_acct *acct = session->aux_data;
	struct aim_priv *priv = acct->data;

	priv->bos_conn = fr->conn;

	va_start(ap, fr);
	priv->rights.max_permit = va_arg(ap, unsigned int);
	priv->rights.max_deny = va_arg(ap, unsigned int);
	va_end(ap);

	aim_ssi_reqrights(session);
	aim_ssi_reqdata(session);

	return (1);
}

static FAIM_CB(aim_recv_buddy_rights) {
	va_list ap;
	struct pork_acct *acct = session->aux_data;
	struct aim_priv *priv = acct->data;

	va_start(ap, fr);
	priv->rights.max_buddies = va_arg(ap, unsigned int);
	priv->rights.max_watchers = va_arg(ap, unsigned int);
	va_end(ap);

	return (1);
}

static FAIM_CB(aim_recv_conn_complete) {
	pork_io_set_cond(fr->conn, IO_COND_READ);

	aim_reqpersonalinfo(session, fr->conn);
	aim_locate_reqrights(session);
	aim_buddylist_reqrights(session, fr->conn);
	aim_im_reqparams(session);
	aim_bos_reqrights(session, fr->conn);

	return (1);
}

static FAIM_CB(aim_recv_typing) {
	va_list ap;
	u_int16_t type1;
	u_int16_t type2;
	char *source;
	struct imwindow *conv_window;
	struct pork_acct *acct = session->aux_data;

	va_start(ap, fr);
	type1 = (u_int16_t) va_arg(ap, unsigned int);
	source = va_arg(ap, char *);
	type2 = (u_int16_t) va_arg(ap, unsigned int);
	va_end(ap);

	conv_window = imwindow_find(acct, source);
	if (conv_window == NULL)
		return (0);

	if (type2 > 2)
		return (0);

	conv_window->typing = type2;
	return (0);
}

static FAIM_CB(aim_recv_err_loc) {
	va_list ap;
	char *dest;
	char *err_str;
	u_int16_t code;

	va_start(ap, fr);
	code = (u_int16_t) va_arg(ap, unsigned int);
	dest = va_arg(ap, char *);
	va_end(ap);

	if (code < array_elem(msgerrreason))
		err_str = msgerrreason[code];
	else
		err_str = "Reason unknown";

	screen_err_msg("User information for %s is unavailable: %s", dest, err_str);
	return (1);
}

static FAIM_CB(aim_recv_err_msg) {
	va_list ap;
	char *dest;
	char *err_str;
	u_int16_t code;
	struct pork_acct *acct = session->aux_data;

	va_start(ap, fr);
	code = (u_int16_t) va_arg(ap, unsigned int);
	dest = va_arg(ap, char *);
	va_end(ap);

	if (code < array_elem(msgerrreason))
		err_str = msgerrreason[code];
	else
		err_str = "Reason unknown";

	if (dest != NULL) {
		struct imwindow *win;

		win = imwindow_find(acct, dest);
		if (win == NULL)
			win = cur_window();

		screen_win_msg(win, 1, 1, 0, MSG_TYPE_ERROR,
			"%s's message to %s was not sent: %s",
			acct->username, dest, err_str);
	} else {
		screen_err_msg("The last message by %s was not sent: %s",
			acct->username, err_str);
	}

	return (1);
}

static FAIM_CB(aim_recv_err_other) {
	va_list ap;
	u_int16_t code;
	char *err_str;

	va_start(ap, fr);
	code = (u_int16_t) va_arg(ap, unsigned int);
	va_end(ap);

	if (code < array_elem(msgerrreason))
		err_str = msgerrreason[code];
	else
		err_str = "Reason unknown";

	screen_err_msg("AIM Error: %s", err_str);
	return (1);
}

static FAIM_CB(aim_recv_evil) {
	va_list ap;
	u_int16_t warn_level;
	aim_userinfo_t *userinfo;
	struct pork_acct *acct = session->aux_data;
	char buf[4096];

	va_start(ap, fr);
	warn_level = va_arg(ap, unsigned int);
	userinfo = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	warn_level = (float) warn_level / 10;
	if (warn_level <= acct->warn_level) {
		acct->warn_level = warn_level;
		return (1);
	}

	acct->warn_level = warn_level;

	if (event_generate(acct->events, EVENT_RECV_WARN, userinfo->sn, warn_level,
		acct->refnum))
	{
		return (1);
	}

	if (blank_str(userinfo->sn)) {
		int ret;

		ret = fill_format_str(OPT_FORMAT_WARN, buf, sizeof(buf), acct->username,
				opt_get_str(OPT_TEXT_WARN_ANONYMOUS), warn_level);

		if (ret > 0)
			screen_print_str(cur_window(), buf, (size_t) ret, MSG_TYPE_STATUS);
	} else {
		int ret;

		ret = fill_format_str(OPT_FORMAT_WARN, buf, sizeof(buf),
				acct->username, buddy_name(acct, userinfo->sn), warn_level);

		if (ret > 0) {
			struct imwindow *win;

			win = imwindow_find(acct, userinfo->sn);
			if (win == NULL)
				win = cur_window();

			screen_print_str(win, buf, (size_t) ret, MSG_TYPE_STATUS);
		}
	}

	return (1);
}

static FAIM_CB(aim_recv_icbm_param_info) {
	va_list ap;
	struct aim_icbmparameters *params;

	va_start(ap, fr);
	params = va_arg(ap, struct aim_icbmparameters *);
	va_end(ap);

	params->flags = 0x0000000b;
	params->maxmsglen = 8000;
	params->minmsginterval = 0;

	aim_im_setparams(session, params);
	return (1);
}

static int parse_im_chan1(	aim_session_t *session __notused,
							aim_conn_t *conn __notused,
							aim_userinfo_t *userinfo,
							struct aim_incomingim_ch1_args *args)
{
	char *buf;
	int autoresp = 0;
	int ret;
	struct pork_acct *acct = session->aux_data;
	char *p;

	if (args->msg == NULL || args->msglen == 0)
		return (-1);

	if (args->icbmflags & AIM_IMFLAGS_AWAY)
		autoresp = 1;

	if (args->charset == AIM_CHARSET_UNICODE) {
		int i;
		char *msg;

		buf = xmalloc(args->msglen + 1);
		msg = buf;

		for (i = 0 ; i < args->msglen ; i += 2) {
			u_int16_t uc_char;

			uc_char = ((args->msg[i] & 0xff) << 8) | (args->msg[i + 1] & 0xff);

			if (uc_char < 255)
				*msg++ = (char) uc_char;
			else
				*msg++ = '?';
		}

		*msg = '\0';
	} else
		buf = xstrdup(args->msg);

	if (!strncasecmp(buf, "/me ", 3)) {
		ret = pork_recv_action(acct, acct->username,
				userinfo->sn, NULL, &buf[4]);
	} else if ((p = strstr(buf, "/me ")) != NULL) {
		char *tmp = strip_html(buf);

		if (!strncasecmp(tmp, "/me ", 4)) {
			memmove(p, &p[4], strlen(p) - 4 + 1);
			ret = pork_recv_action(acct, acct->username,
					userinfo->sn, NULL, buf);
		} else {
			ret = pork_recv_msg(acct, acct->username,
					userinfo->sn, NULL, buf, autoresp);
		}

		free(tmp);
	} else {
		ret = pork_recv_msg(acct, acct->username,
				userinfo->sn, NULL, buf, autoresp);
	}

	free(buf);
	return (ret);
}

static int parse_im_chan2(	aim_session_t *session,
							aim_conn_t *conn,
							aim_userinfo_t *userinfo,
							struct aim_incomingim_ch2_args *args)
{
	struct pork_acct *acct = session->aux_data;

	if (args->reqclass & AIM_CAPS_CHAT) {
		char *chat_name;
		char chat_fullname[512];
		int ret;

		/*
		** They accepted our invitation.
		*/

		if (args->info.chat.roominfo.name == NULL && args->msg == NULL) {
			struct chatroom *chat = conn->priv;

			if (chat != NULL)
				chat_user_joined(acct, chat, userinfo->sn, NULL, 0);

			return (1);
		}

		chat_name = get_chatname(args->info.chat.roominfo.name);
		if (chat_name == NULL)
			chat_name = xstrdup(args->info.chat.roominfo.name);

		ret = snprintf(chat_fullname, sizeof(chat_fullname), "%s/%d",
				chat_name, args->info.chat.roominfo.exchange);
		free(chat_name);

		if (ret > 0 && (size_t) ret < sizeof(chat_fullname)) {
			chat_got_invite(acct, chat_fullname,
				userinfo->sn, NULL, (char *) args->msg);
		}

		return (1);
	} else if (args->reqclass & AIM_CAPS_SENDFILE) {
		if (args->status == AIM_RENDEZVOUS_PROPOSE) {
			struct file_transfer *xfer;
			struct aim_oft_info *oft_info;
			char *p;

			if (!args->cookie ||
				!VALID_PORT(args->port) ||
				args->verifiedip == NULL ||
				args->info.sendfile.filename == NULL ||
				args->info.sendfile.totsize == 0||
				args->info.sendfile.totfiles == 0)
			{
				screen_err_msg(
					"%s [%s:%d] has sent an invalid request to send a file",
					userinfo->sn, args->verifiedip, args->port);
				return (0);
			}

			p = strrchr(args->info.sendfile.filename, '\\');
			if (p != NULL && p[1] == '*') {
				screen_err_msg("%s [%s:%d] has attempted to send you a directory (%s). This isn't supported yet.",
					userinfo->sn, args->verifiedip, args->port,
					args->info.sendfile.filename);

				oft_info = aim_oft_createinfo(session, args->cookie,
							userinfo->sn, args->clientip, args->port,
							0, 0, NULL);
				oft_info->verifiedip = xstrdup(args->verifiedip);

				aim_im_sendch2_sendfile_cancel(oft_info->sess, oft_info);
				aim_oft_destroyinfo(oft_info);
				return (0);
			}

			xfer = transfer_new(acct, userinfo->sn, TRANSFER_DIR_RECV,
					args->info.sendfile.filename, args->info.sendfile.totsize);
			if (xfer == NULL) {
				debug("transfer_new failed for %s from %s (size: %u)",
					args->info.sendfile.filename,
					userinfo->sn, args->info.sendfile.totsize);
				return (0);
			}

			xfer->fport = args->port;
			xstrncpy(xfer->faddr_ip, args->verifiedip, sizeof(xfer->faddr_ip));
			if (get_addr(args->verifiedip, &xfer->faddr) != 0) {
				screen_err_msg(
					"%s [%s:%d] has sent an invalid request to send a file",
					userinfo->sn, args->verifiedip, args->port);
			}

			oft_info = aim_oft_createinfo(session, args->cookie, userinfo->sn,
						args->clientip, args->port, 0, 0, NULL);
			oft_info->verifiedip = xstrdup(args->verifiedip);
			xfer->data = oft_info;

			transfer_request_recv(xfer);
		}
	}

	return (0);
}

static FAIM_CB(aim_recv_msg) {
	va_list ap;
	int channel;
	aim_userinfo_t *userinfo;
	int ret = 0;

	va_start(ap, fr);
	channel = va_arg(ap, int);
	userinfo = va_arg(ap, aim_userinfo_t *);

	switch (channel) {
		case 1: {
			struct aim_incomingim_ch1_args *args;

			args = va_arg(ap, struct aim_incomingim_ch1_args *);
			ret = parse_im_chan1(session, fr->conn, userinfo, args);
			break;
		}

		case 2: {
			struct aim_incomingim_ch2_args *args;

			args = va_arg(ap, struct aim_incomingim_ch2_args *);
			ret = parse_im_chan2(session, fr->conn, userinfo, args);
			break;
		}

		default:
			debug("unhandled case in recv_msg: chan=%d", channel);
			break;
	}

	va_end(ap);
	return (ret);
}

static FAIM_CB(aim_recv_locrights) {
	va_list ap;
	u_int16_t max_len;
	struct pork_acct *acct = session->aux_data;
	struct aim_priv *priv = acct->data;

	va_start(ap, fr);
	max_len = va_arg(ap, int);
	va_end(ap);

	priv->rights.max_profile_len = max_len;
	priv->rights.max_away_len = max_len;

	if (acct->profile != NULL) {
		char *profile_html;
		size_t profile_len;

		profile_html = text_to_html(acct->profile);
		profile_len = strlen(profile_html);

		aim_locate_setprofile(session, "us-ascii", profile_html,
			profile_len, NULL, NULL, 0);
	}

	if (acct->away_msg != NULL) {
		char *away_html;
		size_t away_len;

		away_html = text_to_html(acct->away_msg);
		away_len = strlen(away_html);

		aim_locate_setprofile(session, NULL, NULL, 0,
			"us-ascii", away_html, away_len);
	}

	if (acct->marked_idle)
		aim_srv_setidle(&priv->aim_session, acct->idle_time);

	aim_locate_setcaps(session, pork_caps);
	return (1);
}

static FAIM_CB(aim_recv_missed) {
	va_list ap;
	u_int16_t chan;
	u_int16_t num_missed;
	u_int16_t reason;
	aim_userinfo_t *userinfo;
	char buf[1024];
	struct pork_acct *acct = session->aux_data;
	int ret;

	va_start(ap, fr);
	chan = (u_int16_t) va_arg(ap, unsigned int);
	userinfo = va_arg(ap, aim_userinfo_t *);
	num_missed = (u_int16_t) va_arg(ap, unsigned int);
	reason = (u_int16_t) va_arg(ap, unsigned int);
	va_end(ap);

	switch(reason) {
		case 0:
			ret = snprintf(buf, sizeof(buf),
					num_missed == 1 ?
						"%s missed %u message from %s because it was invalid" :
						"%s missed %u messages from %s because they were invalid",
					acct->username, num_missed, userinfo->sn);
			break;

		case 1:
			ret = snprintf(buf, sizeof(buf),
					num_missed == 1 ?
						"%s missed %u message from %s because it was too large" :
						"%s missed %u messages from %s because they were too large",
					acct->username, num_missed, userinfo->sn);
			break;

		case 2:
			ret = snprintf(buf, sizeof(buf),
					num_missed == 1 ?
						"%s missed %u message from %s because the rate limit has been exceeded" :
						"%s missed %u messages from %s because the rate limit has been exceeded",
					acct->username, num_missed, userinfo->sn);
			break;

		case 3:
			ret = snprintf(buf, sizeof(buf),
					num_missed == 1 ?
						"%s missed %u message from %s because the sender's warning level is too high" :
						"%s missed %u messages from %s because the sender's warning level is too high",
					acct->username, num_missed, userinfo->sn);
			break;

		case 4:
			ret = snprintf(buf, sizeof(buf),
					num_missed == 1 ?
						"%s missed %u message from %s because your warning level is too high" :
						"%s missed %u messages from %s because your warning level is too high",
					acct->username, num_missed, userinfo->sn);
			break;

		default:
			ret = snprintf(buf, sizeof(buf),
					num_missed == 1 ?
						"%s missed %u message from %s for unknown reasons" :
						"%s missed %u messages from %s for unknown reasons",
					acct->username, num_missed, userinfo->sn);
				break;
	}

	screen_err_msg("Error: %s", buf);
	return (1);
}

static FAIM_CB(aim_recv_selfinfo) {
	va_list ap;
	aim_userinfo_t *info;
	struct pork_acct *acct = session->aux_data;

	va_start(ap, fr);
	info = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	acct->warn_level = (float) info->warnlevel / 10;
	acct->idle_time = info->idletime;

	return (1);
}

static FAIM_CB(aim_recv_motd) {
	char *msg;
	u_int16_t id;
	va_list ap;

	va_start(ap, fr);
	id = (u_int16_t) va_arg(ap, unsigned int);
	msg = va_arg(ap, char *);
	va_end(ap);

	if (msg != NULL) {
		screen_win_msg(cur_window(), 0, 1, 0,
			MSG_TYPE_STATUS, "AIM MOTD: %s", msg);
	}

	return (1);
}

static FAIM_CB(aim_recv_offgoing) {
	va_list ap;
	aim_userinfo_t *userinfo;
	struct pork_acct *acct = session->aux_data;

	va_start(ap, fr);
	userinfo = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	if (userinfo == NULL) {
		debug("recv offgoing is NULL");
		return (-1);
	}

	return (buddy_went_offline(acct, userinfo->sn));
}

static FAIM_CB(aim_recv_oncoming) {
	va_list ap;
	struct buddy *buddy;
	aim_userinfo_t *userinfo;
	struct pork_acct *acct = session->aux_data;

	va_start(ap, fr);
	userinfo = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	if (userinfo == NULL) {
		debug("recv oncoming is NULL");
		return (-1);
	}

	buddy = buddy_find(acct, userinfo->sn);
	if (buddy == NULL) {
		debug("recv oncoming for unknown user: %s", userinfo->sn);
		return (-1);
	}

	if (buddy->status == STATUS_OFFLINE)
		buddy_came_online(acct, userinfo->sn, userinfo);
	else {
		int was_away = buddy->status == STATUS_AWAY;
		int is_away = (userinfo->flags & AIM_FLAG_AWAY) != 0;

		if (was_away != is_away) {
			if (was_away)
				buddy_went_unaway(acct, userinfo->sn);
			else
				buddy_went_away(acct, userinfo->sn);
		} else if (buddy->idle_time && !userinfo->idletime)
			buddy_went_unidle(acct, userinfo->sn);
		else if (!buddy->idle_time && userinfo->idletime)
			buddy_went_idle(acct, userinfo->sn, userinfo->idletime);

		buddy_update(acct, buddy, userinfo);
	}

	return (1);
}

static FAIM_CB(aim_recv_rate_change) {
	struct pork_acct *acct = session->aux_data;
	va_list ap;
	u_int16_t rate_code;
	u_int16_t rate_class;
	u_int32_t window_size;
	u_int32_t rate_clear;
	u_int32_t alert;
	u_int32_t limit;
	u_int32_t disconnect;
	u_int32_t current_avg;
	u_int32_t max_avg;

	va_start(ap, fr);

	rate_code = (u_int16_t) va_arg(ap, unsigned int);
	rate_class = (u_int16_t) va_arg(ap, unsigned int);
	window_size = (u_int32_t) va_arg(ap, u_int32_t);
	rate_clear = (u_int32_t) va_arg(ap, u_int32_t);
	alert = (u_int32_t) va_arg(ap, u_int32_t);
	limit = (u_int32_t) va_arg(ap, u_int32_t);
	disconnect = (u_int32_t) va_arg(ap, u_int32_t);
	current_avg = (u_int32_t) va_arg(ap, u_int32_t);
	max_avg = (u_int32_t) va_arg(ap, u_int32_t);

	va_end(ap);

	switch (rate_code) {
		case AIM_RATE_CODE_CHANGE:
			if (current_avg >= rate_clear)
				aim_conn_setlatency(fr->conn, 0);
			break;

		case AIM_RATE_CODE_WARNING:
			aim_conn_setlatency(fr->conn, window_size / 4);
			break;

		case AIM_RATE_CODE_LIMIT:
			aim_conn_setlatency(fr->conn, 10);
			screen_err_msg("The last message from %s was not sent because you are over the rate limit. Please wait 10 seconds, and then try again", acct->username);
			break;

		case AIM_RATE_CODE_CLEARLIMIT:
			aim_conn_setlatency(fr->conn, 0);
			break;

		default:
			debug("unknown rate change code: %hu", rate_code);
			break;
	}

	return (1);
}

static FAIM_CB(aim_recv_chatnav_info) {
	va_list ap;
	u_int16_t type;
	struct pork_acct *acct = session->aux_data;
	struct aim_priv *priv = acct->data;

	va_start(ap, fr);
	type = (u_int16_t) va_arg(ap, unsigned int);

	switch (type) {
		case 0x0002: {
			u_int8_t max_rooms;
			int exchange_count;
			struct aim_chat_exchangeinfo *exchanges;
			dlist_t *cur;

			max_rooms = (u_int8_t) va_arg(ap, unsigned int);
			exchange_count = va_arg(ap, int);
			exchanges = va_arg(ap, struct aim_chat_exchangeinfo *);

			cur = priv->chat_create_list;
			while (cur != NULL) {
				struct chatroom_info *chat = cur->data;
				dlist_t *next = cur->next;

				aim_chatnav_createroom(session, fr->conn, chat->name,
					chat->exchange);

				priv->chat_create_list =
					dlist_remove(priv->chat_create_list, cur);

				free(chat->name);
				free(chat);

				cur = next;
			}

			break;
		}

		case 0x0008: {
			char *fqcn;
			char *name;
			char *ck;
			u_int16_t instance;
			u_int16_t flags;
			u_int16_t maxmsglen;
			u_int16_t maxoccupancy;
			u_int16_t unknown;
			u_int16_t exchange;
			u_int8_t createperms;
			u_int32_t createtime;

			fqcn = va_arg(ap, char *);
			instance = (u_int16_t) va_arg(ap, unsigned int);
			exchange = (u_int16_t) va_arg(ap, unsigned int);
			flags = (u_int16_t) va_arg(ap, unsigned int);
			createtime = va_arg(ap, u_int32_t);
			maxmsglen = (u_int16_t) va_arg(ap, unsigned int);
			maxoccupancy = (u_int16_t) va_arg(ap, unsigned int);
			createperms = (u_int8_t) va_arg(ap, unsigned int);
			unknown = (u_int16_t) va_arg(ap, unsigned int);
			name = va_arg(ap, char *);
			ck = va_arg(ap, char *);

			aim_chat_join(session, priv->bos_conn, exchange, ck, instance);
			break;
		}

		default:
			debug("unknown chatnav type: %x", type);
			break;
	}

	va_end(ap);
	return (1);
}

int elist_add_tail(dlist_t **list, char *str) {
	int ret = 0;

	if (!dlist_find(*list, str, (void*)strcmp)) {
		*list = dlist_add_tail(*list, xstrdup(str));
		ret = 1;
	}

	return ret;
}

int read_room(struct aim_chat *chat, char *title) {
	char filename[65536];
	char input[MAXSNLEN+3];
	char *sn;
	char *pork_dir = opt_get_str(OPT_PORK_DIR);
	FILE *file;

	if (pork_dir == NULL) {
		sprintf(filename, ".");
		opt_set(OPT_PORK_DIR, filename);
	}

	snprintf(filename, 65535, "%s/%s", pork_dir, title);

	file = fopen(filename, "rb");
	if (file == NULL)
		return 0;
	while (fgets(input, sizeof(input), file)) {
		input[strcspn(input,"\n")] = '\0';
		if ((strlen(input) < 4) || (input[1] != ':'))
			continue;
		sn = &input[2];
		switch (input[0]) {
			case 'f':
				if (elist_add_tail(&chat->fullops, sn))
					screen_err_msg("f:%s", sn);
				break;
			case 'o':
				if (elist_add_tail(&chat->oparray, sn))
					screen_err_msg("o:%s", sn);
				break;
			case 'h':
				if (elist_add_tail(&chat->halfops, sn))
					screen_err_msg("h:%s", sn);
				break;
			case 'i':
				if (elist_add_tail(&chat->immlist, sn))
					screen_err_msg("i:%s", sn);
				break;
			case 'b':
				if (elist_add_tail(&chat->abarray, sn))
					screen_err_msg("b:%s", sn);
				break;
			case 'k':
				if (elist_add_tail(&chat->akarray, sn))
					screen_err_msg("k:%s", sn);
				break;
			case 'w':
				if (elist_add_tail(&chat->awarray, sn))
					screen_err_msg("w:%s", sn);
				break;
			default:
				break;
		}
	}
	fclose(file);
	return 1;
}

static FAIM_CB(aim_recv_chat_join) {
	struct pork_acct *acct = session->aux_data;
	struct chatroom *chat = fr->conn->priv;
	aim_userinfo_t *userinfo;
	va_list ap;
	u_int32_t num_users;
	u_int32_t old_num_users = chat->num_users;

	char sn[MAXSNLEN+32]; //EBC
	struct aim_chat *a_chat = chat->data;

	va_start(ap, fr);
	num_users = va_arg(ap, int);
	userinfo = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	if (old_num_users == 0) {
		if (num_users == 1) {
			chat_created(acct, chat);
			chat_user_joined(acct, chat, userinfo[0].sn, NULL, 1);
			a_chat->owned = 1;
			a_chat->chatsends = 2;
		} else {
			u_int32_t i;

			for (i = 0 ; i < num_users ; i++) {
				if (!aim_sncmp(userinfo[i].sn, acct->username))
					chat_user_joined(acct, chat, userinfo[i].sn, NULL, 0);
				else
					chat_user_joined(acct, chat, userinfo[i].sn, NULL, 1);
			}

			aim_chat_print_users(acct, chat);
			a_chat->owned = 0;
			a_chat->chatsends = 2;
		}
		if (!read_room(a_chat, a_chat->title))
			screen_err_msg("Unable to load data for chatroom: %s", a_chat->title);
	} else {
		dlist_t *cur = a_chat->abarray;
		
		chat_user_joined(acct, chat, userinfo->sn, NULL, 0);
		normalize(sn, userinfo->sn, strlen(userinfo->sn) + 1);

		if (!check_if_imm(sn, chat) && a_chat->banjoin) {
			if (!dlist_find(a_chat->banlist, sn, (void*)strcmp))
				a_chat->banlist = dlist_add_tail(a_chat->banlist, xstrdup(sn));
			aim_chat_ban(&((struct aim_priv*)acct->data)->aim_session, a_chat->conn, sn);
		}
		
		while (cur != NULL) {
			if (strstr(sn, cur->data)) {
				a_chat->abqueue = dlist_add_tail(a_chat->abqueue, xstrdup(sn));
				break;
			}
			cur = cur->next;
		}
	}

	return (1);
}

static FAIM_CB(aim_recv_chat_leave) {
	struct pork_acct *acct = session->aux_data;
	struct chatroom *chat = fr->conn->priv;
	va_list ap;
	int num_users;
	aim_userinfo_t *info;
	struct aim_chat *a_chat;

	va_start(ap, fr);
	num_users = va_arg(ap, int);
	info = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	/*
	** Sometimes we'll receive a notice that we're leaving
	** a chat room. I don't know what makes this happen, but
	** it means that we've been disconnected somehow.
	*/

	if (!aim_sncmp(info->sn, acct->username)) {
		debug("chat_recv_leave: user is us");
		aim_disconnected_chat(acct, chat);
		return (1);
	}

	chat_user_left(acct, chat, info->sn, 0);
	a_chat = chat->data;
	if ((chat->num_users == 1) && (!a_chat->owned)) //EBC
		aim_chat_rejoin(acct, chat);
	return (1);
}

static FAIM_CB(aim_recv_chat_info_update) {
	va_list ap;
	aim_userinfo_t *userinfo;
	struct aim_chat_roominfo *roominfo;
	char *roomname;
	int usercount;
	char *roomdesc;
	u_int16_t unknown_c9, unknown_d2, unknown_d5, maxmsglen, maxvisiblemsglen;
	u_int32_t creationtime;
	struct chatroom *chat = fr->conn->priv;
	struct aim_chat *a_chat = chat->data;

	va_start(ap, fr);
	roominfo = va_arg(ap, struct aim_chat_roominfo *);
	roomname = va_arg(ap, char *);
	usercount = va_arg(ap, int);
	userinfo = va_arg(ap, aim_userinfo_t *);
	roomdesc = va_arg(ap, char *);
	unknown_c9 = (u_int16_t) va_arg(ap, unsigned int);
	creationtime = (u_int32_t) va_arg(ap, u_int32_t);
	maxmsglen = (u_int16_t) va_arg(ap, unsigned int);
	unknown_d2 = (u_int16_t) va_arg(ap, unsigned int);
	unknown_d5 = (u_int16_t) va_arg(ap, unsigned int);
	maxvisiblemsglen = (u_int16_t) va_arg(ap, unsigned int);
	va_end(ap);

	a_chat->max_msg_len = maxmsglen;
	a_chat->max_visible_len = maxvisiblemsglen;
	a_chat->created = (time_t) creationtime;

	return (1);
}

static FAIM_CB(aim_recv_chat_msg) {
	va_list ap;
	aim_userinfo_t *info;
	char *msg;
	struct pork_acct *acct = session->aux_data;
	struct chatroom *chat = fr->conn->priv;
	struct aim_chat *a_chat = chat->data;
	dlist_t *aw = a_chat->awarray;
	int len;
	char *charset;
	char *p;
	char sn[MAXSNLEN+32];

	va_start(ap, fr);
	info = va_arg(ap, aim_userinfo_t *);
	len = va_arg(ap, int);
	msg = va_arg(ap, char *);
	charset = va_arg(ap, char *);
	va_end(ap);

	if (!strncasecmp(msg, "/me ", 4)) {
		chat_recv_action(acct, chat,
			chat->title_quoted, info->sn, NULL, &msg[4]);
	} else if ((p = strstr(msg, "/me ")) != NULL) {
		char *tmp = strip_html(msg);

		if (!strncasecmp(tmp, "/me ", 4)) {
			memmove(p, &p[4], strlen(p) - 4 + 1);
			chat_recv_action(acct, chat,
				chat->title_quoted, info->sn, NULL, msg);
		} else
			chat_recv_msg(acct, chat, chat->title_quoted, info->sn, NULL, msg);

		free(tmp);
	} else
		chat_recv_msg(acct, chat, chat->title_quoted, info->sn, NULL, msg);

	p = xstrdup(strip_html(msg));
	if ((p = strchr(p, '!')) != NULL) {
		p++;
		if (check_if_fullop(info->sn, chat))
			parse_ccom(p, chat, acct, FULLOPS);
		else if (check_if_op(info->sn, chat))
			parse_ccom(p, chat, acct, OPS);
		else if (check_if_halfop(info->sn, chat))
			parse_ccom(p, chat, acct, HALFOPS);
	}

	if (!check_if_imm(info->sn, chat))
		while (aw != NULL) {
			if (strstr(msg, aw->data))
				a_chat->abqueue = dlist_add_tail(a_chat->abqueue, xstrdup(info->sn));
			aw = aw->next;
		}

	if (chat_find_user(acct, chat, info->sn) == NULL) {
		normalize(sn, info->sn, MAXSNLEN);
		if (!dlist_find(a_chat->banlist, sn, (void*)strcmp))
			a_chat->banlist = dlist_add_tail(a_chat->banlist, xstrdup(sn));
		aim_chat_ban(&((struct aim_priv*)acct->data)->aim_session, a_chat->conn, sn);
	}

	return (1);
}

static FAIM_CB(recv_chat_conn) {
	pork_io_set_cond(fr->conn, IO_COND_READ);

	aim_conn_addhandler(session, fr->conn,
		AIM_CB_FAM_CHT, AIM_CB_CHT_ERROR, aim_recv_err_other, 0);
	aim_conn_addhandler(session, fr->conn,
		AIM_CB_FAM_CHT, AIM_CB_CHT_USERJOIN, aim_recv_chat_join, 0);
	aim_conn_addhandler(session, fr->conn,
		AIM_CB_FAM_CHT, AIM_CB_CHT_USERLEAVE, aim_recv_chat_leave, 0);
	aim_conn_addhandler(session, fr->conn,
		AIM_CB_FAM_CHT, AIM_CB_CHT_ROOMINFOUPDATE, aim_recv_chat_info_update, 0);
	aim_conn_addhandler(session, fr->conn,
		AIM_CB_FAM_CHT, AIM_CB_CHT_INCOMINGMSG, aim_recv_chat_msg, 0);

	aim_clientready(session, fr->conn);
	return (1);
}

static FAIM_CB(recv_chatnav_conn) {
	struct pork_acct *acct = session->aux_data;
	struct aim_priv *priv = acct->data;

	pork_io_set_cond(fr->conn, IO_COND_READ);

	aim_conn_addhandler(session, fr->conn,
		AIM_CB_FAM_CTN, AIM_CB_CTN_ERROR, aim_recv_err_other, 0);
	aim_conn_addhandler(session, fr->conn,
		AIM_CB_FAM_CTN, AIM_CB_CTN_INFO, aim_recv_chatnav_info, 0);

	aim_clientready(session, fr->conn);
	aim_chatnav_reqrights(session, fr->conn);

	priv->chatnav_conn = fr->conn;

	return (1);
}

static FAIM_CB(aim_recv_redirect) {
	struct aim_redirect_data *redirect;
	va_list ap;
	struct pork_acct *acct = session->aux_data;

	va_start(ap, fr);
	redirect = va_arg(ap, struct aim_redirect_data *);
	va_end(ap);

	if (redirect->group == AIM_CONN_TYPE_CHATNAV) {
		aim_conn_t *chatnav;
		int sock;
		int ret;

		chatnav = aim_newconn(session, AIM_CONN_TYPE_CHATNAV, NULL);
		if (chatnav == NULL) {
			screen_err_msg("Unable to connect to the chatnav server: %s",
				strerror(errno));
			return (0);
		}

		ret = aim_sock_connect(redirect->ip, &acct->laddr, &sock);
		if (ret == 0) {
			aim_connected(sock, 0, chatnav);
		} else if (ret == -EINPROGRESS) {
			chatnav->status |= AIM_CONN_STATUS_INPROGRESS;
			pork_io_add(sock, IO_COND_WRITE, chatnav, chatnav, aim_connected);
		} else {
			aim_conn_kill(session, &chatnav);
			screen_err_msg("Unable to connect to the chatnav server: %s",
				strerror(errno));
			return (0);
		}

		aim_conn_addhandler(session, chatnav,
			AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNDEAD, aim_conn_dead, 0);
		aim_conn_addhandler(session, chatnav,
			AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR, aim_connerr, 0);
		aim_conn_addhandler(session, chatnav,
			AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNINITDONE, recv_chatnav_conn, 0);

		aim_sendcookie(session, chatnav, redirect->cookielen, redirect->cookie);
	} else if (redirect->group == AIM_CONN_TYPE_CHAT) {
		aim_conn_t *chat_conn;
		struct chatroom *chat;
		struct aim_chat *a_chat;
		struct imwindow *win;
		char buf[128];
		int sock;
		int ret;
		char *chat_title;

		chat_conn = aim_newconn(session, AIM_CONN_TYPE_CHAT, NULL);
		if (chat_conn == NULL) {
			screen_err_msg("Unable to connect to the chat server");
			return (0);
		}

		ret = aim_sock_connect(redirect->ip, &acct->laddr, &sock);
		if (ret == 0) {
			aim_connected(sock, 0, chat_conn);
		} else if (ret == -EINPROGRESS) {
			chat_conn->status |= AIM_CONN_STATUS_INPROGRESS;
			pork_io_add(sock, IO_COND_WRITE, chat_conn, chat_conn,
				aim_connected);
		} else {
			aim_conn_kill(session, &chat_conn);
			screen_err_msg("Unable to connect to the chat server");
			return (0);
		}

		chat_title = get_chatname(redirect->chat.room);
		if (chat_title == NULL)
			chat_title = xstrdup(redirect->chat.room);

		ret = snprintf(buf, sizeof(buf), "%s/%d",
				chat_title, redirect->chat.exchange);

		if (ret < 0 || (size_t) ret >= sizeof(buf)) {
			free(chat_title);
			pork_io_del(chat_conn);
			aim_conn_kill(session, &chat_conn);
			return (-1);
		}

		win = imwindow_find_chat_target(acct, buf);
		if (win == NULL) {
			free(chat_title);
			pork_io_del(chat_conn);
			aim_conn_kill(session, &chat_conn);
			debug("unable to find chat window for %s (acct: %s)",
				buf, acct->username);
			return (-1);
		}

		chat = chat_new(acct, buf, (char *) redirect->chat.room, win);

		a_chat = xcalloc(1, sizeof(*a_chat));
		a_chat->fullname = xstrdup(redirect->chat.room);
		a_chat->fullname_quoted = color_quote_codes(a_chat->fullname);
		a_chat->title = chat_title;
		a_chat->exchange = redirect->chat.exchange;
		a_chat->instance = redirect->chat.instance;
		a_chat->conn = chat_conn;

		chat->data = a_chat;
		chat_conn->priv = chat;

		aim_conn_addhandler(session, chat_conn,
			AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNDEAD, aim_conn_dead, 0);
		aim_conn_addhandler(session, chat_conn,
			AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR, aim_connerr, 0);
		aim_conn_addhandler(session, chat_conn,
			AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNINITDONE, recv_chat_conn, 0);

		aim_sendcookie(session, chat_conn, redirect->cookielen, redirect->cookie);
	}

	return (1);
}

static FAIM_CB(aim_recv_search_error) {
	va_list ap;
	char *address;
	struct pork_acct *acct = session->aux_data;

	va_start(ap, fr);
	address = va_arg(ap, char *);
	va_end(ap);

	if (event_generate(acct->events, EVENT_RECV_SEARCH_RESULT, address, NULL,
		acct->refnum))
	{
		return (1);
	}

	screen_cmd_output("No results were found for %s", address);
	return (1);
}

static FAIM_CB(aim_recv_search_reply) {
	va_list ap;
	char *address;
	char *usernames;
	char buf[2048];
	int i = 0;
	size_t len = sizeof(buf);
	int j;
	int num;
	int ret;
	struct pork_acct *acct = session->aux_data;

	va_start(ap, fr);
	address = va_arg(ap, char *);
	num = va_arg(ap, int);
	usernames = va_arg(ap, char *);
	va_end(ap);

	if (num < 1) {
		if (event_generate(acct->events, EVENT_RECV_SEARCH_RESULT,
			address, NULL, acct->refnum))
		{
			return (1);
		}

		screen_cmd_output("No results were found for %s", address);
		return (1);
	}

	ret = snprintf(buf, len, "%s has registered the following screen names: %s",
			address, &usernames[0]);
	if (ret < 0 || (size_t) ret >= len) {
		screen_err_msg("The results for %s were too long to display", address);
		return (1);
	}
	len -= ret;
	i += ret;

	for (j = 1 ; j < num ; j++) {
		ret = snprintf(&buf[i], len, ", %s", &usernames[j * (MAXSNLEN + 1)]);
		if (ret < 0 || (size_t) ret >= len) {
			screen_err_msg("The results for %s were too long to display",
				address);
			return (1);
		}

		len -= ret;
		i += ret;
	}

	if (event_generate(acct->events, EVENT_RECV_SEARCH_RESULT, address, buf,
		acct->refnum))
	{
		return (1);
	}

	screen_cmd_output("%s", buf);
	return (1);
}

static FAIM_CB(aim_ssi_recv_rights) {
	struct pork_acct *acct = session->aux_data;
	struct aim_priv *priv = acct->data;
	int num;
	u_int16_t *items;
	va_list ap;

	va_start(ap, fr);
	num = va_arg(ap, int);
	items = va_arg(ap, u_int16_t *);
	va_end(ap);

	if (num >= 0)
		priv->rights.max_buddies = items[0];

	if (num >= 1)
		priv->rights.max_groups = items[1];

	if (num >= 2)
		priv->rights.max_permit = items[2];

	if (num >= 3)
		priv->rights.max_deny = items[3];

	return (1);
}

static FAIM_CB(aim_ssi_recv_list) {
	struct aim_ssi_item *cur;
	struct pork_acct *acct = session->aux_data;

	aim_ssi_enable(session);
	aim_ssi_cleanlist(session);

	/*
	** Merge the buddy list on the server with the locally
	** stored buddy list. By this point, the local
	** buddy list will have already been processed.
	*/

	for (cur = session->ssi.local ; cur != NULL ; cur = cur->next) {
		if (cur->type == 0) {
			char *group_name;
			struct buddy *buddy;
			struct bgroup *group;
			char *alias;

			if (cur->name == NULL)
				continue;

			group_name = aim_ssi_itemlist_findparentname(session->ssi.local,
							cur->name);

			alias = aim_ssi_getalias(session->ssi.local, group_name, cur->name);

			if (group_name == NULL)
				group_name = "orphans";

			buddy = buddy_find(acct, cur->name);
			if (buddy != NULL) {
				if (alias != NULL) {
					free(buddy->name);
					buddy->name = alias;
				}

				continue;
			}

			group = group_add(acct, group_name);
			buddy = buddy_add(acct, cur->name, group, 0);
			if (alias != NULL)
				buddy_alias(acct, buddy, alias, 0);
			else
				buddy_alias(acct, buddy, cur->name, 0);

			free(alias);
		} else if (cur->type == 2) {
			if (cur->name == NULL)
				continue;

			buddy_add_permit(acct, cur->name, 0);
		} else if (cur->type == 3) {
			/* Deny */
			if (cur->name == NULL)
				continue;

			buddy_add_block(acct, cur->name, 0);
		} else if (cur->type == 4) {
			u_int32_t privacy_mode = aim_ssi_getpermdeny(session->ssi.local);

			acct->buddy_pref->privacy_mode = privacy_mode;
		} else if (cur->type == 5) {
			u_int32_t report_idle = aim_ssi_getpresence(session->ssi.local);

			if (report_idle & 0x0000400)
				acct->report_idle = 1;
			else
				acct->report_idle = 0;
		}
	}

	if (!acct->connected) {
		aim_report_idle(acct, acct->report_idle);

		aim_ssi_setpermdeny(session,
			acct->buddy_pref->privacy_mode, 0xffffffff);
	}

	aim_send_buddy_list(session, acct);
	aim_send_permit_list(session, acct);
	aim_send_block_list(session, acct);

	if (!acct->connected) {
		struct aim_priv *priv = acct->data;

		aim_clientready(session, fr->conn);

		pork_acct_connected(acct);
		time(&priv->last_update);
	}

	return (1);
}

static FAIM_CB(aim_recv_userinfo) {
	struct pork_acct *acct = session->aux_data;
	aim_userinfo_t *userinfo;
	va_list ap;
	char *profile = NULL;
	char *away_msg = NULL;
	u_int32_t idle_time = 0;
	u_int32_t online_since = 0;
	u_int32_t member_since = 0;
	u_int32_t warn_level;
	int print_all = 1;

	va_start(ap, fr);
	userinfo = va_arg(ap, aim_userinfo_t *);
	va_end(ap);

	if (userinfo->present & AIM_USERINFO_PRESENT_ONLINESINCE)
		online_since = userinfo->onlinesince;

	if (userinfo->present & AIM_USERINFO_PRESENT_MEMBERSINCE)
		member_since = userinfo->membersince;

	if (userinfo->present & AIM_USERINFO_PRESENT_IDLE)
		idle_time = userinfo->idletime;

	warn_level = (float) userinfo->warnlevel / 10;

	if ((userinfo->flags & AIM_FLAG_AWAY) && userinfo->away_len > 0 &&
		userinfo->away != NULL)
	{
		char *temp = NULL;

		if (*userinfo->away != '\0') {
			temp = xmalloc(userinfo->away_len + 1);

			/*
			** libfaim isn't nul-terminating the string anymore,
			** so we can't call xstrncpy.
			*/

			memcpy(temp, userinfo->away, userinfo->away_len);
			temp[userinfo->away_len] = '\0';
		}

		if (event_generate(acct->events, EVENT_RECV_AWAYMSG, userinfo->sn,
			member_since, online_since, idle_time, warn_level,
			temp, acct->refnum))
		{
			free(temp);
			return (1);
		}

		if (temp != NULL) {
			away_msg = strip_html(temp);
			free(temp);
		}
	}

	if (userinfo->info_len > 0 && userinfo->info != NULL) {
		char *temp = NULL;

		if (*userinfo->info != '\0') {
			temp = xmalloc(userinfo->info_len + 1);
			memcpy(temp, userinfo->info, userinfo->info_len);
			temp[userinfo->info_len] = '\0';
		}

		if (event_generate(acct->events, EVENT_RECV_PROFILE,
			userinfo->sn, member_since, online_since, idle_time,
			warn_level, temp, acct->refnum))
		{
			free(temp);
			return (1);
		}

		if (temp != NULL) {
			profile = strip_html(temp);
			free(temp);
		}
	}

	aim_print_info(userinfo->sn, profile, away_msg, warn_level,
		idle_time, online_since, member_since, print_all);

	free(away_msg);
	free(profile);
	return (1);
}

static FAIM_CB(aim_parse_login) {
	struct pork_acct *acct = session->aux_data;
	char *key;
	va_list ap;
	static struct client_info_s client_info = CLIENTINFO_AIM_KNOWNGOOD;

	va_start(ap, fr);
	key = va_arg(ap, char *);
	va_end(ap);

	aim_send_login(session, fr->conn,
		acct->username, acct->passwd, &client_info, key);

	return (1);
}

static FAIM_CB(aim_file_send_done) {
	va_list ap;
	aim_conn_t *conn;
	fu8_t *cookie;
	struct aim_fileheader_t *fh;
	struct file_transfer *xfer;

	va_start(ap, fr);
	conn = va_arg(ap, aim_conn_t *);
	cookie = va_arg(ap, fu8_t *);
	fh = va_arg(ap, struct aim_fileheader_t *);
	va_end(ap);

	xfer = conn->priv;
	pork_io_del(conn);
	return (transfer_send_complete(xfer));
}

static FAIM_CB(aim_file_send_ready) {
	va_list ap;
	fu8_t *cookie;
	struct aim_fileheader_t *header;
	aim_conn_t *conn;
	struct file_transfer *xfer;

	va_start(ap, fr);
	conn = va_arg(ap, aim_conn_t *);
	cookie = va_arg(ap, fu8_t *);
	header = va_arg(ap, struct aim_fileheader_t *);
	va_end(ap);

	xfer = conn->priv;
	pork_io_del(conn);
	pork_io_add(xfer->sock, IO_COND_WRITE, xfer, xfer,
		transfer_send_data);

	return (0);
}

FAIM_CB(aim_file_send_accepted) {
	aim_conn_t *conn;
	aim_conn_t *listen_conn;
	va_list ap;
	struct file_transfer *xfer;
	struct aim_oft_info *oft_info;

	va_start(ap, fr);
	conn = va_arg(ap, aim_conn_t *);
	listen_conn = va_arg(ap, aim_conn_t *);
	va_end(ap);

	xfer = listen_conn->priv;
	xfer->sock = conn->fd;

	oft_info = xfer->data;
	oft_info->conn = conn;
	oft_info->conn->priv = xfer;
	xfer->data = oft_info;

	pork_io_del(listen_conn);
	aim_conn_kill(session, &listen_conn);

	if (get_peer_addr(xfer->sock, &xfer->faddr) != 0) {
		xfer->protocol_flags |= AIM_XFER_IN_HANDLER;
		transfer_abort(xfer);
		return (-1);
	}

	get_ip(&xfer->faddr, xfer->faddr_ip, sizeof(xfer->faddr_ip));
	xfer->fport = sin_port(&xfer->faddr);

	pork_io_add(xfer->sock, IO_COND_READ, conn, conn, aim_conn_event);

	aim_clearhandlers(conn);

	aim_conn_addhandler(session, conn,
		AIM_CB_FAM_OFT, AIM_CB_OFT_ACK, aim_file_send_ready, 0);

	aim_conn_addhandler(session, conn,
		AIM_CB_FAM_OFT, AIM_CB_OFT_DONE, aim_file_send_done, 0);

	aim_conn_addhandler(session, conn,
		AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNDEAD, aim_file_transfer_dead, 0);

	aim_oft_sendheader(session, AIM_CB_OFT_PROMPT, oft_info);
	transfer_send_accepted(xfer);
	return (0);
}

FAIM_CB(aim_file_recv_accept) {
	fu8_t *cookie;
	aim_conn_t *conn;
	struct aim_fileheader_t *header;
	va_list ap;
	struct file_transfer *xfer;
	struct aim_oft_info *oft_info;

	va_start(ap, fr);
	conn = va_arg(ap, aim_conn_t *);
	cookie = va_arg(ap, fu8_t *);
	header = va_arg(ap, struct aim_fileheader_t *);
	va_end(ap);

	if (conn == NULL || cookie == NULL || header == NULL)
		return (1);

	xfer = conn->priv;
	oft_info = xfer->data;

	memcpy(&oft_info->fh, header, sizeof(oft_info->fh));
	memcpy(&oft_info->fh.bcookie, oft_info->cookie,
		sizeof(oft_info->fh.bcookie));

	aim_oft_sendheader(oft_info->sess, AIM_CB_OFT_ACK, oft_info);

	pork_io_del(conn);
	aim_clearhandlers(conn);

	if (get_local_addr(xfer->sock, &xfer->laddr) != 0) {
		xfer->protocol_flags |= AIM_XFER_IN_HANDLER;
		transfer_abort(xfer);
		return (-1);
	}

	xfer->lport = sin_port(&xfer->laddr);

	pork_io_add(xfer->sock, IO_COND_READ, xfer, xfer,
		transfer_recv_data);

	return (0);
}

static FAIM_CB(aim_parse_authresp) {
	va_list ap;
	struct aim_authresp_info *authresp;
	aim_conn_t *bos_conn;
	struct pork_acct *acct = session->aux_data;
	int ret;
	int sock;
	struct sockaddr_storage local;

	pork_io_del(fr->conn);
	aim_conn_kill(session, &fr->conn);

	va_start(ap, fr);
	authresp = va_arg(ap, struct aim_authresp_info *);
	va_end(ap);

	if (authresp->errorcode || !authresp->bosip || !authresp->cookie) {
		switch (authresp->errorcode) {
			case 0x05:
				screen_err_msg("Error logging in %s: Incorrect username or password",
					acct->username);
				break;

			case 0x11:
				screen_err_msg("Error logging in %s: This account has been suspended",
					acct->username);
				break;

			case 0x14:
				screen_err_msg("Error logging in %s: This service is temporarily unavailable",
					acct->username);
				break;

			case 0x18:
				screen_err_msg("Error logging in %s: This account has been connecting too frequently",
					acct->username);
				break;

			case 0x1c:
				screen_err_msg("Error logging in %s: Client is too old",
					acct->username);
				break;

			default:
				screen_err_msg("Error logging in %s: Authentication failed",
					acct->username);
				break;
		}

		return (1);
	}

	bos_conn = aim_newconn(session, AIM_CONN_TYPE_BOS, NULL);
	if (bos_conn == NULL) {
		aim_conn_kill(session, &bos_conn);
		screen_err_msg("Unable to connect to the AIM BOS server");
		return (1);
	}

	memcpy(&local, &acct->laddr, sizeof(local));
	sin_set_port(&local, acct->lport);

	ret = aim_sock_connect(authresp->bosip, &local, &sock);
	if (ret == 0) {
		aim_connected(sock, 0, bos_conn);
	} else if (ret == -EINPROGRESS) {
		bos_conn->status |= AIM_CONN_STATUS_INPROGRESS;
		pork_io_add(sock, IO_COND_WRITE, bos_conn, bos_conn,
			aim_connected);
	} else {
		aim_conn_kill(session, &bos_conn);
		screen_err_msg("Unable to connect to the BOS server");
		return (0);
	}

	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNDEAD, aim_conn_dead, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNERR, aim_connerr, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNINITDONE, aim_recv_conn_complete, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_BOS, AIM_CB_BOS_RIGHTS, aim_recv_bos_rights, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_ACK, AIM_CB_ACK_ACK, NULL, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_GEN, AIM_CB_GEN_REDIRECT, aim_recv_redirect, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_LOC, AIM_CB_LOC_RIGHTSINFO, aim_recv_locrights, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_BUD, AIM_CB_BUD_RIGHTSINFO, aim_recv_buddy_rights, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_BUD, AIM_CB_BUD_ONCOMING, aim_recv_oncoming, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_BUD, AIM_CB_BUD_OFFGOING, aim_recv_offgoing, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_MSG, AIM_CB_MSG_INCOMING, aim_recv_msg, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_LOC, AIM_CB_LOC_ERROR, aim_recv_err_loc, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_MSG, AIM_CB_MSG_MISSEDCALL, aim_recv_missed, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_GEN, AIM_CB_GEN_RATECHANGE, aim_recv_rate_change, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_GEN, AIM_CB_GEN_EVIL, aim_recv_evil, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_LOK, AIM_CB_LOK_ERROR, aim_recv_search_error, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_LOK, 0x0003, aim_recv_search_reply, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_MSG, AIM_CB_MSG_ERROR, aim_recv_err_msg, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_MSG, AIM_CB_MSG_MTN, aim_recv_typing, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_LOC, AIM_CB_LOC_USERINFO, aim_recv_userinfo, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_MSG, AIM_CB_MSG_PARAMINFO, aim_recv_icbm_param_info, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_GEN, AIM_CB_GEN_ERROR, aim_recv_err_other, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_BUD, AIM_CB_BUD_ERROR, aim_recv_err_other, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_BOS, AIM_CB_BOS_ERROR, aim_recv_err_other, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_GEN, AIM_CB_GEN_SELFINFO, aim_recv_selfinfo, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_GEN, AIM_CB_GEN_MOTD, aim_recv_motd, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_SSI, AIM_CB_SSI_RIGHTSINFO, aim_ssi_recv_rights, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_SSI, AIM_CB_SSI_LIST, aim_ssi_recv_list, 0);
	aim_conn_addhandler(session, bos_conn,
		AIM_CB_FAM_SSI, AIM_CB_SSI_NOLIST, aim_ssi_recv_list, 0);

	aim_sendcookie(session, bos_conn, authresp->cookielen, authresp->cookie);

	return (1);
}
