/*
** pork_irc_input.c
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
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
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#include <pork.h>
#include <pork_util.h>
#include <pork_list.h>
#include <pork_misc.h>
#include <pork_queue.h>
#include <pork_color.h>
#include <pork_inet.h>
#include <pork_acct.h>
#include <pork_proto.h>
#include <pork_imsg.h>
#include <pork_imwindow.h>
#include <pork_screen.h>
#include <pork_screen_io.h>
#include <pork_chat.h>
#include <pork_msg.h>
#include <pork_missing.h>

#include <pork_irc.h>
#include <pork_irc_dcc.h>

static struct irc_input *irc_tokenize(char *buf) {
	struct irc_input *in = xcalloc(1, sizeof(*in));
	size_t i = 0;
	size_t len = array_elem(in->tokens);
	char *p;
	int cmd_offset = 0;
	int numeric;

	in->orig = xstrdup(buf);

	if (*buf == ':') {
		cmd_offset = 1;
		buf++;
	}

	p = buf;
	while ((p = strchr(p, ':')) != NULL) {
		if (p[-1] == ' ') {
			*p++ = '\0';
			in->args = p;
			break;
		} else
			p++;
	}

	while (i < len && (p = strsep(&buf, " ")) != NULL && *p != '\0')
		in->tokens[i++] = p;

	if (i < 1) {
		debug("invalid input from server: %s", in->orig);
		free(in->orig);
		free(in);
		return (NULL);
	}

	in->cmd = in->tokens[cmd_offset];
	in->num_tokens = i;

	numeric = strtol(in->cmd, &p, 10);
	if (*p == '\0')
		in->numeric = numeric;
	else
		in->numeric = -1;

	return (in);
}

static int irc_get_chanmode(irc_session_t *session,
								struct irc_chan_data *data,
								char *buf,
								size_t len)
{
	int ret;
	char *p;

	if (len < 2) {
		buf[0] = '\0';
		return (-1);
	}

	ret = xstrncpy(buf, data->mode_str, len);
	if (ret < 0) {
		buf[0] = '\0';
		return (-1);
	}

	buf += ret;
	len -= ret;

	p = data->mode_str;
	while (*p != '\0') {
		if (irc_chanmode_has_arg(session, *p)) {
			char *arg;

			arg = irc_get_chanmode_arg(data, *p);
			if (arg != NULL) {
				ret = snprintf(buf, len, " %s", arg);
				if (ret < 0 || (size_t) ret >= len)
					return (-1);

				buf += ret;
				len -= ret;
			}
		}
		p++;
	}

	return (0);
}

static int irc_callback_compare(void *l, void *r) {
	char *str = l;
	struct callback_handler *cb = r;

	return (strcasecmp(str, cb->str));
}

static void irc_callback_cleanup(void *p __notused, void *data) {
	struct callback_handler *cb = data;

	free(cb->str);
	free(cb);
}

int irc_callback_init(irc_session_t *session) {
	int ret;

	ret = hash_init(&session->callbacks, 5,
			irc_callback_compare, irc_callback_cleanup);

	return (ret);
}

int irc_callback_clear(irc_session_t *session) {
	hash_destroy(&session->callbacks);
	return (0);
}

int irc_callback_add(	irc_session_t *session,
						char *cmd,
						int (*handler)(struct pork_acct *, struct irc_input *))
{
	struct callback_handler *cb;
	u_int32_t hash;

	cb = xcalloc(1, sizeof(*cb));
	cb->str = xstrdup(cmd);
	cb->handler = handler;

	hash = string_hash(cmd, session->callbacks.order);
	hash_add(&session->callbacks, cb, hash);
	return (0);
}

static int irc_handler_err_msg(struct pork_acct *acct, struct irc_input *in) {
	char *msg;
	char *str;

	if (in->num_tokens < 4) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	str = irc_text_filter(in->tokens[3]);

	if (in->args == NULL)
		msg = xstrdup("Unknown error");
	else
		msg = irc_text_filter(in->args);

	screen_err_msg("%s: %s", str, msg);

	free(str);
	free(msg);
	return (0);
}

static int irc_handler_dcc(struct pork_acct *acct, struct irc_input *in) {
	char *cmd;
	char *p;
	int ret;

	p = in->args;
	cmd = strsep(&p, " ");

	if (cmd == NULL || p == NULL) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	in->args = p;

	if (!strcasecmp(cmd, "SEND")) {
		ret = irc_handler_dcc_send(acct, in);
	} else if (!strcasecmp(cmd, "RESUME")) {
		ret = irc_handler_dcc_resume(acct, in);
	} else if (!strcasecmp(cmd, "ACCEPT")) {
		ret = irc_handler_dcc_accept(acct, in);
	} else {
		/* add more later */
		ret = -1;
	}

	return (ret);
}

static int irc_handler_ctcp_action(	struct pork_acct *acct,
									struct irc_input *in)
{
	char *host;

	if (in->args == NULL)
		in->args = "";

	host = strchr(in->tokens[0], '!');
	if (host != NULL)
		*host++ = '\0';

	if (!strcasecmp(acct->username, in->tokens[2]))
		pork_recv_action(acct, in->tokens[2], in->tokens[0], host, in->args);
	else {
		struct chatroom *chat;
		char *p;

		p = in->tokens[2];
		while (*p == '@' || *p == '+' || *p == '%')
			p++;

		chat = chat_find(acct, p);
		if (chat == NULL) {
			debug("action for unjoined chan %s \"%s\"", p, in->orig);
			return (-1);
		}

		chat_recv_action(acct, chat, in->tokens[2],
			in->tokens[0], host, in->args);
	}

	return (1);
}

static int irc_handler_ctcp_version(struct pork_acct *acct,
									struct irc_input *in)
{
	char buf[256];
	int ret;
	char *p;

	ret = snprintf(buf, sizeof(buf),
			"\x01%s %s %s - http://dev.ojnk.net\x01", in->cmd,
			PACKAGE_NAME, PACKAGE_VERSION);

	if (ret < 0 || (size_t) ret >= sizeof(buf))
		return (-1);

	p = strchr(in->tokens[0], '!');
	if (p != NULL)
		*p = '\0';

	irc_send_notice(acct->data, in->tokens[0], buf);
	return (0);
}

static int irc_handler_ctcp_time(	struct pork_acct *acct,
									struct irc_input *in)
{
	char tbuf[256];
	char buf[256];
	int ret;
	time_t time_now = time(NULL);
	char *p;

	if (date_to_str(time_now, tbuf, sizeof(tbuf)) == -1)
		return (-1);

	ret = snprintf(buf, sizeof(buf), "\x01%s %s\x01", in->cmd, tbuf);
	if (ret < 0 || (size_t) ret >= sizeof(buf))
		return (-1);

	p = strchr(in->tokens[0], '!');
	if (p != NULL)
		*p = '\0';

	irc_send_notice(acct->data, in->tokens[0], buf);
	return (0);
}

static int irc_handler_ctcp_echo(	struct pork_acct *acct,
									struct irc_input *in)
{
	char buf[2048];
	int ret;
	char *p;

	if (in->args == NULL)
		return (-1);

	ret = snprintf(buf, sizeof(buf), "\x01" "%s %s\x01", in->cmd, in->args);
	if (ret < 0 || (size_t) ret >= sizeof(buf))
		return (-1);

	p = strchr(in->tokens[0], '!');
	if (p != NULL)
		*p = '\0';

	irc_send_notice(acct->data, in->tokens[0], buf);
	return (0);
}

static int irc_handler_print_arg(struct pork_acct *acct, struct irc_input *in) {
	char *str;

	if (in->args == NULL) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	if (!strncasecmp(in->args, "End of /", 8))
		return (0);

	str = irc_text_filter(in->args);
	screen_win_msg(cur_window(), 0, 1, 1, MSG_TYPE_CMD_OUTPUT, "%s", str);

	free(str);
	return (0);
}

static int irc_callback_run(irc_session_t *session,
							struct irc_input *in,
							char *set)
{
	dlist_t *node;
	u_int32_t hash;
	struct callback_handler *cb;

	if (set == NULL) {
		hash = string_hash(in->cmd, session->callbacks.order);
		node = hash_find(&session->callbacks, in->cmd, hash);
	} else {
		int ret;
		char buf[1024];

		ret = snprintf(buf, sizeof(buf), "%s %s",
				set, in->cmd);
		if (ret < 0 || (size_t) ret >= sizeof(buf))
			return (-1);

		hash = string_hash(buf, session->callbacks.order);
		node = hash_find(&session->callbacks, buf, hash);
	}

	if (node == NULL || node->data == NULL) {
		if (set == NULL)
			irc_handler_print_arg(session->data, in);
		return (0);
	}

	cb = node->data;

	return (cb->handler(session->data, in));
}

static ssize_t irc_read_data(int sock, char *buf, size_t len) {
	int i;
	ssize_t ret = 0;

	for (i = 0 ; i < 5 ; i++) {
		ret = read(sock, buf, len - 1);
		if (ret == -1) {
			if (errno == EINTR)
				continue;

			debug("sock err: %d:%s", sock, strerror(errno));
			return (-1);
		}

		if (ret == 0) {
			debug("sock err: %d:%s", sock, strerror(errno));
			return (-1);
		}

		buf[ret] = '\0';
		return (ret);
	}

	return (-1);
}

/*
** Returns -1 if the connection died, 0 otherwise.
*/

int irc_input_dispatch(irc_session_t *session) {
	int ret;
	char *p;
	char *cur;
	struct pork_acct *acct = session->data;

	ret = irc_read_data(session->sock,
			&session->input_buf[session->input_offset],
			sizeof(session->input_buf) - session->input_offset);

	if (ret < 1) {
		pork_sock_err(acct, session->sock);
		return (-1);
	}

	cur = session->input_buf;
	while ((p = strchr(cur, '\n')) != NULL) {
		struct irc_input *in;
		char *q;

		*p++ = '\0';
		q = strchr(cur, '\r');
		if (q != NULL)
			*q = '\0';

		in = irc_tokenize(cur);

		if (in == NULL) {
			debug("invalid input from server: %s", cur);
			continue;
		}

		if (!event_generate(acct->events, EVENT_RECV_RAW,
			in->cmd, in->orig, acct->refnum))
		{
			irc_callback_run(session, in, NULL);
		}

		cur = p;
		free(in->orig);
		free(in);
	}

	if (*cur != '\0') {
		size_t leftover;

		leftover = strlen(cur);

		/* Move the '\0', too */
		memmove(session->input_buf, cur, leftover + 1);
		session->input_offset = leftover;
	} else
		session->input_offset = 0;

	return (0);
}

static int irc_handler_001(struct pork_acct *acct, struct irc_input *in) {
	if (in->num_tokens < 3) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	free(acct->server);
	acct->server = xstrdup(in->tokens[0]);

	free(acct->username);
	acct->username = xstrdup(in->tokens[2]);

	pork_acct_connected(acct);

	/* Restore away message */
	if (acct->away_msg != NULL)
		irc_set_away(acct->data, acct->away_msg);

	/* Reset user mode */
	if (acct->umode[0] != '\0') {
		char mode[256];

		snprintf(mode, sizeof(mode), "%s +%s", acct->username, acct->umode);
		irc_send_mode(acct->data, mode);
	}

	return (irc_handler_print_arg(acct, in));
}

static int irc_handler_print_num(struct pork_acct *acct, struct irc_input *in) {
	char *str;

	if (in->args == NULL || in->num_tokens < 4) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	str = irc_text_filter(in->args);
	screen_win_msg(cur_window(), 0, 1, 1,
		MSG_TYPE_CMD_OUTPUT, "There are %s %s", in->tokens[3], str);

	free(str);
	return (0);
}

static int irc_handler_print_tok(struct pork_acct *acct, struct irc_input *in) {
	u_int32_t i;
	char buf[2048];
	size_t len = sizeof(buf);
	u_int32_t off = 0;

	if (in->num_tokens < 4) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	for (i = 3 ; i < in->num_tokens ; i++) {
		int ret;

		ret = snprintf(&buf[off], len, "%s ", in->tokens[i]);
		if (ret < 0 || (size_t) ret >= len)
			return (-1);

		len -= ret;
		off += ret;
	}

	if (off > 0) {
		char *str;

		buf[off - 1] = '\0';

		str = irc_text_filter(buf);
		screen_win_msg(cur_window(), 0, 1, 1, MSG_TYPE_CMD_OUTPUT, "%s", str);
		free(str);
	}

	return (0);
}

static int irc_handler_315(struct pork_acct *acct, struct irc_input *in) {
	struct chatroom *chat;
	struct irc_chan_data *irc_data;

	if (in->num_tokens < 4) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	chat = chat_find(acct, in->tokens[3]);
	if (chat != NULL) {
		irc_data = chat->data;
		irc_data->join_complete = 1;
	}

	return (0);
}

static int irc_handler_352(struct pork_acct *acct, struct irc_input *in) {
	int silent = 0;

	if (in->num_tokens < 9 || in->args == NULL) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	/* Grab userhosts for users in channels we're in if we don't have them. */
	if (strcmp(in->tokens[3], "*")) {
		struct chatroom *chat;

		chat = chat_find(acct, in->tokens[3]);
		if (chat != NULL) {
			struct chat_user *chat_user;
			struct irc_chan_data *irc_data = chat->data;

			if (!irc_data->join_complete)
				silent = 1;

			chat_user = chat_find_user(acct, chat, in->tokens[7]);
			if (chat_user != NULL && chat_user->host == NULL) {
				int ret;
				char buf[256];

				ret = snprintf(buf, sizeof(buf), "%s@%s",
						in->tokens[4], in->tokens[5]);

				if (ret > 0 && (size_t) ret < sizeof(buf))
					chat_user->host = xstrdup(buf);
			}
		}
	}

	if (!silent) {
		int ret;
		char *info;
		char buf[2048];

		info = strchr(in->args, ' ');
		if (info != NULL)
			info++;
		else
			info = in->args;

		info = irc_text_filter(info);

		ret = snprintf(buf, sizeof(buf), "%s\t%-9s %-3s %s@%s (%s)",
				in->tokens[3], in->tokens[7], in->tokens[8],
				in->tokens[4], in->tokens[5], info);

		free(info);

		if (ret < 0 || (size_t) ret >= sizeof(buf))
			return (-1);

		screen_win_msg(cur_window(), 0, 0, 0, MSG_TYPE_CMD_OUTPUT, "%s", buf);
	}

	return (0);
}

static int irc_handler_367(struct pork_acct *acct, struct irc_input *in) {
	char *str;

	if (in->num_tokens < 4) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	str = str_from_tok(in->orig, 4);
	if (str == NULL) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	screen_win_msg(cur_window(), 0, 0, 0, MSG_TYPE_CMD_OUTPUT, "%s", str);
	return (0);
}

static int irc_handler_433(struct pork_acct *acct, struct irc_input *in) {
	char buf[128];
	int ret;

	irc_handler_err_msg(acct, in);
	if (acct->connected) {
		debug("acct not connected");
		return (0);
	}

	if (in->num_tokens < 4) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	/*
	** Try appending a '_' to the end of the nick if we haven't yet
	** connected, as per the behavior of almost every other IRC
	** client in existence.
	*/

	ret = snprintf(buf, sizeof(buf), "%s_", in->tokens[3]);
	if (ret < 0 || (size_t) ret >= sizeof(buf))
		return (-1);

	pork_change_nick(acct, buf);
	return (0);
}

static int irc_handler_353(struct pork_acct *acct, struct irc_input *in) {
	struct chatroom *chat;
	char *p = in->args;
	char *tok;
	char buf[2048];
	size_t offset = 0;
	size_t len = sizeof(buf);
	int ret;
	int add = 1;
	char *chat_name;
	struct imwindow *win = cur_window();

	if (in->num_tokens < 5 || in->args == NULL) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	chat = chat_find(acct, in->tokens[4]);
	if (chat == NULL)
		add = 0;
	else
		win = chat->win;

	tok = strsep(&p, " ");
	while (tok != NULL && tok[0] != '\0') {
		u_int32_t status = 0;

		while (tok[0] != '\0' && !isalpha(tok[0])) {
			ret = snprintf(&buf[offset], len, "%c", tok[0]);
			if (ret < 0 || (size_t) ret >= len)
				return (-1);

			switch (tok[0]) {
				case '@':
					status |= CHAT_STATUS_OP;
					break;

				case '+':
					status |= CHAT_STATUS_VOICE;
					break;

				case '%':
					status |= CHAT_STATUS_HALFOP;
					break;
			}

			offset += ret;
			len -= ret;
			tok++;
		}

		if (tok[0] == '\0')
			return (-1);

		if (add) {
			struct chat_user *user;

			user = chat_user_joined(acct, chat, tok, NULL, 1);
			if (user == NULL) {
				user = chat_find_user(acct, chat, tok);
				add = 0;
			}

			if (user != NULL)
				user->status = status;
		}

		ret = snprintf(&buf[offset], len, "%s ", tok);
		if (ret < 0 || (size_t) ret >= len)
			return (-1);

		offset += ret;
		len -= ret;
		tok = strsep(&p, " ");
	}

	if (offset <= 0)
		return (0);

	buf[offset - 1] = '\0';

	p = irc_text_filter(buf);
	chat_name = irc_text_filter(in->tokens[4]);

	/* XXX */
	screen_win_msg(win, 1, 0, 1, MSG_TYPE_CMD_OUTPUT,
		"Users on %%c%s%%W:%%x %s", chat_name, p);

	free(p);
	free(chat_name);
	return (0);
}

static int irc_chan_mode_comp(void *l, void *r) {
	int mode = POINTER_TO_INT(l);
	struct irc_chan_arg *chat_arg = r;

	return (mode - chat_arg->arg);
}

static void irc_chan_mode_cleanup(void *param __notused, void *data) {
	struct irc_chan_arg *chat_arg = data;

	free(chat_arg->val);
	free(chat_arg);
}

static int irc_handler_join(struct pork_acct *acct, struct irc_input *in) {
	char *p;
	char *channel_name;

	/* I love IRC */
	if (in->args == NULL)
		channel_name = in->tokens[in->num_tokens - 1];
	else
		channel_name = in->args;

	p = strchr(in->tokens[0], '!');
	if (p != NULL)
		*p++ = '\0';

	if (!acct->proto->user_compare(acct->username, in->tokens[0])) {
		struct imwindow *win;
		struct irc_chan_data *chat_data;

		win = imwindow_find_chat_target(acct, channel_name);
		if (win == NULL) {
			/*
			** Support dircproxy and the like.
			** This will result in another JOIN being sent to the server, but
			** as far as I know, it's harmless.
			*/
			if (chat_join(acct, channel_name) == -1)
				return (-1);

			win = imwindow_find_chat_target(acct, channel_name);
			if (win == NULL)
				return (-1);
		}

		/*
		** We don't want to create another one of these if we were
		** disconnected and the join message is a result of a rejoin.
		*/

		if (win->data == NULL) {
			struct chatroom *chat;

			chat = chat_new(acct, channel_name, channel_name, win);
			if (chat == NULL)
				return (-1);

			chat_data = xcalloc(1, sizeof(struct irc_chan_data));

			hash_init(&chat_data->mode_args, 2,
				irc_chan_mode_comp, irc_chan_mode_cleanup);

			chat->data = chat_data;
		} else
			chat_data = ((struct chatroom *) win->data)->data;

		chat_data->joined = 1;
		irc_send_mode(acct->data, channel_name);
		if (!chat_data->join_complete)
			irc_send_who(acct->data, channel_name);
	} else {
		struct chatroom *chat;

		chat = chat_find(acct, channel_name);
		if (chat == NULL)
			return (-1);

		chat_user_joined(acct, chat, in->tokens[0], p, 0);
	}

	return (0);
}

static int irc_handler_privmsg(struct pork_acct *acct, struct irc_input *in) {
	char *p;
	char *host;

	if (in->num_tokens < 3 || in->args == NULL) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	p = strchr(in->tokens[0], '!');
	if (p != NULL)
		*p++ = '\0';
	host = p;

	/* ^A */
	if (in->args[0] == 0x01) {
		char *dest;

		p = strrchr(&in->args[1], 0x01);
		if (p == NULL)
			goto no_ctcp;
		*p++ = '\0';

		in->cmd = &in->args[1];
		p = strchr(in->cmd, ' ');
		if (p != NULL)
			*p++ = '\0';
		in->args = p;

		if (host != NULL)
			host[-1] = '!';

		if (irc_callback_run(acct->data, in, "CTCP") == 1)
			return (0);
		else if (host != NULL)
			host[-1] = '\0';

		/* XXX */
		dest = irc_text_filter(in->tokens[2]);
		if (in->args != NULL) {
			char *msg;

			msg = irc_text_filter(in->args);
			screen_win_msg(cur_window(), 1, 0, 1, MSG_TYPE_CHAT_MSG_RECV,
				"%%WCTCP%%M %s %%D[%%x%s%%D]%%x from %%C%s%%D(%%c%s%%D)%%x to %%W%s",
				in->cmd, msg, in->tokens[0], host, dest);

			free(msg);
		} else {
			screen_win_msg(cur_window(), 1, 0, 1, MSG_TYPE_PRIVMSG_RECV,
				"%%WCTCP%%M %s%%x from %%C%s%%D(%%c%s%%D)%%x to %%W%s",
				in->cmd, in->tokens[0], host, dest);
		}

		free(dest);
		return (0);
	}

no_ctcp:
	if (!acct->proto->user_compare(acct->username, in->tokens[2]))
		pork_recv_msg(acct, in->tokens[2], in->tokens[0], host, in->args, 0);
	else {
		struct chatroom *chat;
		char *p = in->tokens[2];

		/* these should be displayed in a way that lets the user know they aren't ordinary privmsgs */
		while (*p == '@' || *p == '+' || *p == '%')
			p++;

		chat = chat_find(acct, p);
		if (chat == NULL) {
			debug("receviced msg for unjoined chan %s \"%s\"", p, in->orig);
			return (-1);
		}

		chat_recv_msg(acct, chat, in->tokens[2], in->tokens[0], host, in->args);
	}

	return (0);
}

static int irc_handler_ping(struct pork_acct *acct, struct irc_input *in) {
	if (in->args == NULL) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	return (irc_send_pong(acct->data, in->args));
}

static int irc_handler_311(struct pork_acct *acct, struct irc_input *in) {
	struct imwindow *win = cur_window();
	char *info;

	if (in->num_tokens < 6) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	/* XXX */
	screen_win_msg(win, 0, 0, 1, MSG_TYPE_CMD_OUTPUT,
		"%%D-%%m-%%M-%%W%s%%M-%%D(%%c%s%%W@%%c%s%%D)%%M-%%m-%%D-",
		in->tokens[3], in->tokens[4], in->tokens[5]);

	info = irc_text_filter(in->args);
	screen_win_msg(win, 0, 0, 1, MSG_TYPE_CMD_OUTPUT,
		"%%D-%%Ci%%crcname%%W:%%x %s", info);

	free(info);
	return (0);
}

static int irc_handler_319(struct pork_acct *acct, struct irc_input *in) {
	char *chans;

	if (in->args == NULL) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	chans = irc_text_filter(in->args);

	/* XXX */
	screen_win_msg(cur_window(), 0, 0, 1, MSG_TYPE_CMD_OUTPUT,
		"%%Cc%%channels%%W:%%x %s", chans);

	free(chans);
	return (0);
}

static int irc_handler_312(struct pork_acct *acct, struct irc_input *in) {
	char *info;

	if (in->num_tokens < 5) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	/* XXX */
	info = irc_text_filter(in->args);
	screen_win_msg(cur_window(), 0, 0, 1, MSG_TYPE_CMD_OUTPUT,
		"%%D-%%m-%%Cs%%cerver%%W:%%x %s %%D(%%x%s%%D)%%x",
		in->tokens[4], info);

	free(info);
	return (0);
}

static int irc_handler_317(struct pork_acct *acct, struct irc_input *in) {
	char timebuf[128];
	u_int32_t idle_time;
	u_int32_t temp;
	struct imwindow *win = cur_window();
	time_t signon;
	char *p;
	int ret;

	if (in->num_tokens < 6) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	if (str_to_uint(in->tokens[4], &idle_time) != 0) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	time_to_str_full(idle_time, timebuf, sizeof(timebuf));

	/* XXX */
	screen_win_msg(win, 0, 0, 1, MSG_TYPE_CMD_OUTPUT,
		"%%D-%%m-%%M--%%Ci%%cdle%%W:%%x %s", timebuf);

	if (str_to_uint(in->tokens[5], &temp) != 0) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	signon = (time_t) temp;

	ret = snprintf(timebuf, sizeof(timebuf), "%s", asctime(localtime(&signon)));
	if (ret < 0 || (size_t) ret >= sizeof(timebuf))
		return (-1);

	p = strchr(timebuf, '\n');
	if (p != NULL)
		*p = '\0';

	screen_win_msg(win, 0, 0, 1, MSG_TYPE_CMD_OUTPUT,
		"%%D-%%m-%%Cs%%cignon%%W:%%x %s", timebuf);
	return (0);
}

static int irc_handler_301(struct pork_acct *acct, struct irc_input *in) {
	char *msg;

	msg = irc_text_filter(in->args);

	/* XXX */
	screen_win_msg(cur_window(), 0, 0, 1, MSG_TYPE_CMD_OUTPUT,
		"%%D-%%m-%%M--%%Ca%%cway%%W:%%x %s", msg);

	free(msg);
	return (0);
}

static int irc_handler_313(struct pork_acct *acct, struct irc_input *in) {
	char *msg;

	if (in->num_tokens < 4) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	if (in->args == NULL)
		msg = xstrdup("is an IRC operator");
	else
		msg = irc_text_filter(in->args);

	/* XXX */
	screen_win_msg(cur_window(), 0, 0, 1, MSG_TYPE_CMD_OUTPUT,
		"%%Co%%cperator%%W:%%x %s %s", in->tokens[3], msg);

	free(msg);
	return (0);
}

static int irc_handler_314(struct pork_acct *acct, struct irc_input *in) {
	char *info;
	struct imwindow *win = cur_window();

	if (in->num_tokens < 6) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	info = irc_text_filter(in->args);

	/* XXX */
	screen_win_msg(win, 0, 0, 1, MSG_TYPE_CMD_OUTPUT,
		"%%D-%%b-%%B-%%W%s%%B-%%D(%%c%s%%W@%%c%s%%D)%%B-%%D(%%Cw%%cho%%Cw%%cas%%D)%%b-%%D-",
		in->tokens[3], in->tokens[4], in->tokens[5]);

	screen_win_msg(win, 0, 0, 1, MSG_TYPE_CMD_OUTPUT,
		"%%D-%%Ci%%crcname%%W:%%x %s", info);

	free(info);
	return (0);
}

static int irc_handler_nick(struct pork_acct *acct, struct irc_input *in) {
	char *p;

	if (in->args == NULL) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	p = strchr(in->tokens[0], '!');
	if (p != NULL)
		*p = '\0';

	if (!acct->proto->user_compare(acct->username, in->tokens[0])) {
		free(acct->username);
		acct->username = xstrdup(in->args);
	}

	return (chat_nick_change(acct, in->tokens[0], in->args));
}

static int irc_handler_332(struct pork_acct *acct, struct irc_input *in) {
	struct chatroom *chat;
	struct imwindow *win;
	char *topic;
	char *chan;

	if (in->num_tokens < 4 || in->args == NULL) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	chat = chat_find(acct, in->tokens[3]);
	if (chat != NULL)
		win = chat->win;
	else
		win = cur_window();

	chan = irc_text_filter(in->tokens[3]);
	topic = irc_text_filter(in->args);

	/* XXX */
	screen_win_msg(win, 0, 1, 1, MSG_TYPE_CHAT_STATUS,
		"Topic for %%c%s%%W:%%x %s", chan, topic);

	free(chan);
	free(topic);
	return (0);
}

static int irc_handler_221(struct pork_acct *acct, struct irc_input *in) {
	char *p;

	if (in->num_tokens < 4) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	if (strcasecmp(acct->username, in->tokens[2])) {
		debug("umode for other user: %s", in->orig);
		return (-1);
	}

	p = str_from_tok(in->orig, 4);

	screen_win_msg(cur_window(), 1, 0, 1, MSG_TYPE_STATUS,
		"Mode for %%c%s%%W:%%x %s", acct->username, p);

	if (*p == '+')
		p++;
	xstrncpy(acct->umode, p, sizeof(acct->umode));

	return (0);
}

static int irc_handler_324(struct pork_acct *acct, struct irc_input *in) {
	struct chatroom *chat;
	struct imwindow *win;
	char *chan;
	char *p;

	if (in->num_tokens < 5) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	chat = chat_find(acct, in->tokens[3]);
	if (chat != NULL)
		win = chat->win;
	else
		win = cur_window();

	chan = irc_text_filter(in->tokens[3]);

	p = str_from_tok(in->orig, 5);
	str_trim(p);

	/* XXX */
	screen_win_msg(win, 1, 0, 1, MSG_TYPE_CHAT_STATUS,
		"Mode for %%c%s%%W:%%x %s", chan, p);
	free(chan);

	if (chat != NULL) {
		struct irc_chan_data *data = chat->data;
		u_int32_t arg_num = 5;

		if (!data->joined)
			return (1);

		if (*p == '+')
			p++;
		xstrncpy(chat->mode, p, sizeof(chat->mode));

		hash_clear(&data->mode_args);

		p = in->tokens[4];
		if (*p == '+')
			p++;

		xstrncpy(data->mode_str, p, sizeof(data->mode_str));

		while (*p != '\0') {
			if (irc_chanmode_has_arg(acct->data, *p) &&
				arg_num < in->num_tokens)
			{
				struct irc_chan_arg *chat_arg;
				u_int32_t temp_hash;

				chat_arg = xcalloc(1, sizeof(*chat_arg));
				chat_arg->arg = (u_int32_t) *p;
				chat_arg->val = xstrdup(in->tokens[arg_num++]);

				temp_hash = int_hash(chat_arg->arg, data->mode_args.order);
				hash_add(&data->mode_args, chat_arg, temp_hash);
			}

			p++;
		}
	}

	return (0);
}

static int irc_handler_329(struct pork_acct *acct, struct irc_input *in) {
	struct chatroom *chat;
	struct imwindow *win;
	int time_set;
	char buf[64];
	char *chan;

	if (in->num_tokens < 5) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	if (str_to_int(in->tokens[4], &time_set) == -1) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	if (date_to_str((time_t) time_set, buf, sizeof(buf)) == -1) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	chat = chat_find(acct, in->tokens[3]);
	if (chat != NULL)
		win = chat->win;
	else
		win = cur_window();

	chan = irc_text_filter(in->tokens[3]);

	/* XXX */
	screen_win_msg(win, 1, 0, 1, MSG_TYPE_CHAT_STATUS,
		"%%c%s%%x created%%W:%%x %s", chan, buf);

	free(chan);
	return (0);
}

static int irc_handler_302(struct pork_acct *acct, struct irc_input *in) {
	char *str;
	char *tok;

	if (in->args == NULL) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	str = in->args;
	while ((tok = strsep(&str, " ")) != NULL) {
		char *p;
		dlist_t *cur;

		p = strchr(tok, '=');
		if (p == NULL)
			continue;

		if (&p[-1] > tok && p[-1] == '*')
			p[-1] = '\0';
		else
			*p = '\0';

		if (p[1] != '+' && p[1] != '-')
			continue;

		if (tok[0] == '\0')
			continue;
		p += 2;

		cur = acct->chat_list;
		while (cur != NULL) {
			struct chat_user *user;

			user = chat_find_user(acct, cur->data, tok);
			if (user == NULL)
				continue;

			free(user->host);
			user->host = xstrdup(p);

			cur = cur->next;
		}
	}

	return (0);
}

static int irc_handler_321(struct pork_acct *acct, struct irc_input *in) {
	screen_win_msg(cur_window(), 0, 1, 0, MSG_TYPE_CHAT_STATUS,
		"Channel\t\t\tUsers\t\tTopic");
	return (0);
}

static int irc_handler_322(struct pork_acct *acct, struct irc_input *in) {
	struct chatroom *chat;
	struct imwindow *win;
	char *title_q;
	char *topic_q;

	if (in->num_tokens < 5) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	chat = chat_find(acct, in->tokens[3]);
	if (chat != NULL)
		win = chat->win;
	else
		win = cur_window();

	title_q = irc_text_filter(in->tokens[3]);
	topic_q = irc_text_filter(in->args);

	screen_win_msg(win, 0, 1, 1, MSG_TYPE_CHAT_STATUS,
		"%s\t\t\t%s\t\t%s", title_q, in->tokens[4], topic_q);

	free(title_q);
	free(topic_q);
	return (0);
}

static int irc_handler_333(struct pork_acct *acct, struct irc_input *in) {
	struct chatroom *chat;
	struct imwindow *win;
	int time_set;
	char buf[64];
	char *chan;

	if (in->num_tokens < 6) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	if (str_to_int(in->tokens[5], &time_set) == -1) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	if (date_to_str((time_t) time_set, buf, sizeof(buf)) == -1) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	chat = chat_find(acct, in->tokens[3]);
	if (chat != NULL)
		win = chat->win;
	else
		win = cur_window();

	chan = irc_text_filter(in->tokens[3]);

	/* XXX */
	screen_win_msg(win, 0, 1, 1, MSG_TYPE_CHAT_STATUS,
		"Topic for %%c%s%%x set by %%c%s%%x on %s",
		chan, in->tokens[4], buf);

	free(chan);
	return (0);
}

static int irc_handler_005(struct pork_acct *acct, struct irc_input *in) {
	u_int32_t i;
	irc_session_t *irc = acct->data;

	irc_handler_print_tok(acct, in);

	for (i = 0 ; i < in->num_tokens ; i++) {
		if (!strcasecmp(in->tokens[i], "WALLCHOPS"))
			irc->wallchops = 1;
		else if (!strcasecmp(in->tokens[i], "CAPAB"))
			irc->capab = 1;
		else if (!strcasecmp(in->tokens[i], "KNOCK"))
			irc->knock = 1;
		else if (!strcasecmp(in->tokens[i], "INVEX"))
			irc->invex = 1;
		else if (!strcasecmp(in->tokens[i], "CALLERID"))
			irc->callerid = 1;
		else if (!strcasecmp(in->tokens[i], "ETRACE"))
			irc->etrace = 1;
		else if (!strcasecmp(in->tokens[i], "SAFELIST"))
			irc->safelist = 1;
		else if (!strcasecmp(in->tokens[i], "EXCEPTS"))
			irc->excepts = 1;
		else if (!strncasecmp(in->tokens[i], "PREFIX=", 7)) {
			char *p;
			char *args;

			p = in->tokens[i];
			if (*p++ != '(')
				break;

			args = strchr(p, ')');
			if (args == NULL)
				break;

			*args++ = '\0';

			if (strlen(p) != strlen(args))
				break;

			irc->prefix_types = xstrdup(p);
			irc->prefix_codes = xstrdup(args);
		} else if (!strncasecmp(in->tokens[i], "CHANTYPES=", 10))
			irc->chantypes = xstrdup(&in->tokens[i][10]);
		else if (!strncasecmp(in->tokens[i], "NICKLEN=", 8))
			str_to_uint(&in->tokens[i][8], &irc->nick_len);
		else if (!strncasecmp(in->tokens[i], "TOPICLEN=", 9))
			str_to_uint(&in->tokens[i][9], &irc->topic_len);
		else if (!strncasecmp(in->tokens[i], "KICKLEN=", 8))
			str_to_uint(&in->tokens[i][8], &irc->kick_len);
		else if (!strncasecmp(in->tokens[i], "CHANMODES=", 10))
			irc->chanmodes = xstrdup(&in->tokens[i][10]);
	}

	return (0);
}

static int irc_handler_notice(struct pork_acct *acct, struct irc_input *in) {
	char *p;
	char *host;
	int ret = 0;

	if (in->args == NULL) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	if (!strcasecmp(in->tokens[0], "NOTICE")) {
		in->args += 4;
		return (irc_handler_print_arg(acct, in));
	}

	if (in->num_tokens < 3) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	/* Server notice */
	p = strchr(in->tokens[0], '!');
	if (p == NULL)
		return (irc_handler_print_arg(acct, in));

	*p++ = '\0';
	host = p;

	/* CTCP REPLY */
	if (in->args[0] == 0x01) {
		p = strrchr(&in->args[1], 0x01);
		if (p != NULL) {
			char *msg;

			*p++ = '\0';

			in->cmd = &in->args[1];
			p = strchr(in->cmd, ' ');
			if (p != NULL)
				*p++ = '\0';
			in->args = p;

			if (host != NULL)
				host[-1] = '!';

			if (irc_callback_run(acct->data, in, "CTCP REPLY") == 1)
				return (0);
			else if (host != NULL)
				host[-1] = '\0';

			msg = irc_text_filter(in->args);
			screen_win_msg(cur_window(), 1, 0, 1, MSG_TYPE_NOTICE_RECV,
				"%%YCTCP REPLY %%G%s %%D[%%x%s %s%%D]%%x from %%C%s%%D(%%c%s%%D)%%x",
				acct->username, in->cmd, msg, in->tokens[0], host);

			free(msg);
			return (0);
		}
	}

	if (!strcasecmp(acct->username, in->tokens[2])) {
		ret = pork_recv_notice(acct, in->tokens[2],
				in->tokens[0], host, in->args);
	} else {
		struct chatroom *chat;
		char *p = in->tokens[2];

		while (*p == '@' || *p == '+' || *p == '%')
			p++;

		chat = chat_find(acct, p);
		if (chat == NULL) {
			debug("notice to unjoined chat: %s \"%s\"", p, in->orig);
			return (-1);
		}

		ret = chat_recv_notice(acct, chat, in->tokens[2],
				in->tokens[0], host, in->args);
	}

	return (ret);
}

static int irc_handler_user_mode(struct pork_acct *acct, struct irc_input *in) {
	char *p = in->args;
	char c;
	int mode;

	if (in->args[0] != '+' && in->args[0] != '-') {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	mode = in->args[0];

	while ((c = *++p) != '\0') {
		char *f;

		if (c == '+' || c == '-') {
			mode = c;
			continue;
		}

		f = strchr(acct->umode, c);

		if (mode == MODE_PLUS) {
			size_t len;

			if (f != NULL)
				continue;

			len = strlen(acct->umode);
			if (len < sizeof(acct->umode) - 1) {
				acct->umode[len] = c;
				acct->umode[len + 1] = '\0';
			}
		} else {
			size_t len;

			if (f == NULL)
				continue;

			f++;
			len = strlen(f);
			memmove(f - 1, f, len + 1);
		}
	}

	/* XXX */
	screen_win_msg(cur_window(), 1, 0, 1, MSG_TYPE_STATUS,
		"Mode %%c%s%%W:%%x %s", acct->username, in->args);
	return (0);
}

static int irc_handler_chan_mode(struct pork_acct *acct, struct irc_input *in) {
	struct chatroom *chat;
	char *p;
	char *mode_str;
	char c;
	u_int32_t i;
	u_int32_t op = MODE_PLUS;
	struct irc_chan_data *data;

	chat = chat_find(acct, in->tokens[2]);
	if (chat == NULL) {
		debug("chan mode for unjoined chan: %s", in->tokens[2]);
		return (-1);
	}

	data = chat->data;

	p = strchr(in->tokens[0], '!');
	if (p != NULL)
		*p = '\0';

	mode_str = str_from_tok(in->orig, 4);
	str_trim(mode_str);

	chat_got_mode(acct, chat, in->tokens[0], mode_str);

	/*
	** Mode arguments start at the 5th token.
	*/
	i = 4;

	p = in->tokens[3];
	while ((c = *p++) != '\0') {
		switch (c) {
			case '+':
			case '-':
				op = c;
				break;

			/*
			** Ban/ban exception/invite masks are ignored here.
			*/
			case 'b':
			case 'e':
			case 'I':
				i++;
				break;

			/*
			** op/halfop/voice and arguments aren't part of the mode string.
			*/
			case 'o':
			case 'v':
			case 'h': {
				struct chat_user *chat_user;

				chat_user = chat_find_user(acct, chat, in->tokens[i++]);
				if (chat_user == NULL)
					break;

				if (c == 'o') {
					if (op == MODE_PLUS)
						chat_user->status |= CHAT_STATUS_OP;
					else
						chat_user->status &= ~CHAT_STATUS_OP;
				} else if (c == 'v') {
					if (op == MODE_PLUS)
						chat_user->status |= CHAT_STATUS_VOICE;
					else
						chat_user->status &= ~CHAT_STATUS_VOICE;
				} else {
					if (op == MODE_PLUS)
						chat_user->status |= CHAT_STATUS_HALFOP;
					else
						chat_user->status &= ~CHAT_STATUS_HALFOP;
				}

				break;
			}

			default: {
				char *f;
				int mode_has_arg = irc_chanmode_has_arg(acct->data, c);

				if (mode_has_arg) {
					u_int32_t temp_hash;

					temp_hash = int_hash((int) c, data->mode_args.order);
					if (op == MODE_PLUS) {
						struct irc_chan_arg *chat_arg;

						/*
						** The channel is already +l and then another +l
						** with a new limit is sent.
						*/
						if (c == 'l') {
							hash_remove(&data->mode_args,
								INT_TO_POINTER((int) 'l'),temp_hash);
						}

						chat_arg = xcalloc(1, sizeof(*chat_arg));
						chat_arg->arg = (int) c;
						chat_arg->val = xstrdup(in->tokens[i++]);

						hash_add(&data->mode_args, chat_arg, temp_hash);
					} else {
						/*
						** The server doesn't send the argument for -l.
						** It does for everything else that gets here,
						** though, as far as I know..
						*/
						if (c != 'l')
							i++;

						hash_remove(&data->mode_args,
							INT_TO_POINTER((int) c), temp_hash);
					}
				}

				f = strchr(data->mode_str, c);
				if (op == MODE_PLUS) {
					if (f == NULL) {
						size_t len;

						len = strlen(data->mode_str);
						if (len < sizeof(data->mode_str) - 1) {
							data->mode_str[len] = c;
							data->mode_str[len + 1] = '\0';
						}
					}
				} else {
					if (f != NULL) {
						size_t len;

						f++;
						len = strlen(f);
						memmove(f - 1, f, len + 1);
					}
				}
			}
		}
	}

	irc_get_chanmode(acct->data, chat->data, chat->mode, sizeof(chat->mode));
	return (0);
}

static int irc_handler_mode(struct pork_acct *acct, struct irc_input *in) {
	if (in->num_tokens < 3) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	if (!strcasecmp(in->tokens[2], acct->username)) {
		if (in->args != NULL)
			return (irc_handler_user_mode(acct, in));
		else if (in->num_tokens > 3) {
			char *args;

			args = str_from_tok(in->orig, 4);
			if (args == NULL) {
				debug("invalid input from server: %s", in->orig);
				return (-1);
			}

			in->args = args;
			return (irc_handler_user_mode(acct, in));
		}

		return (-1);
	}

	if (in->num_tokens < 4) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	return (irc_handler_chan_mode(acct, in));
}

static int irc_handler_quit(struct pork_acct *acct, struct irc_input *in) {
	dlist_t *cur;
	char *p;

	if (in->num_tokens < 2) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	p = strchr(in->tokens[0], '!');
	if (p != NULL)
		*p = '\0';

	for (cur = acct->chat_list ; cur != NULL ; cur = cur->next) {
		struct chat_user *chat_user;

		chat_user = chat_find_user(acct, cur->data, in->tokens[0]);
		if (chat_user != NULL)
			chat_user_quit(acct, cur->data, chat_user, in->args);
	}

	return (0);
}

static int irc_handler_part(struct pork_acct *acct, struct irc_input *in) {
	char *p;
	struct chatroom *chat;
	int ret = 0;

	if (in->num_tokens < 3) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	chat = chat_find(acct, in->tokens[2]);
	if (chat == NULL)
		return (-1);

	p = strchr(in->tokens[0], '!');
	if (p == NULL)
		return (-1);
	*p++ = '\0';

	if (strcasecmp(acct->username, in->tokens[0]))
		ret = chat_user_left(acct, chat, in->tokens[0], 0);

	return (ret);
}

static int irc_handler_kick(struct pork_acct *acct, struct irc_input *in) {
	struct chatroom *chat;
	char *msg;
	char *p;
	int ret;

	if (in->num_tokens < 4) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	chat = chat_find(acct, in->tokens[2]);
	if (chat == NULL) {
		debug("kick from unjoined channel: %s \"%s\"", in->tokens[2], in->orig);
		return (-1);
	}

	p = strchr(in->tokens[0], '!');
	if (p == NULL) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}
	*p++ = '\0';

	if (in->args == NULL)
		msg = "No reason given";
	else
		msg = in->args;

	if (!strcasecmp(acct->username, in->tokens[3])) {
		struct irc_chan_data *data = chat->data;

		data->joined = 0;
		ret = chat_forced_leave(acct, chat->title, in->tokens[0], msg);
	} else
		ret = chat_user_kicked(acct, chat, in->tokens[3], in->tokens[0], msg);

	return (ret);
}

static int irc_handler_topic(struct pork_acct *acct, struct irc_input *in) {
	char *p;
	struct chatroom *chat;

	if (in->num_tokens < 3) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	chat = chat_find(acct, in->tokens[2]);
	if (chat == NULL) {
		debug("topic for unjoined channel: %s \"%s\"", in->tokens[2], in->orig);
		return (-1);
	}

	p = strchr(in->tokens[0], '!');
	if (p != NULL)
		*p++ = '\0';

	return (chat_got_topic(acct, chat, in->tokens[0], in->args));
}

static int irc_handler_kill(struct pork_acct *acct, struct irc_input *in) {
	char *reason;
	char *userhost;

	if (in->num_tokens < 3) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	if (in->args == NULL)
		reason = xstrdup("No reason given");
	else
		reason = irc_text_filter(in->args);

	userhost = strchr(in->tokens[0], '!');
	if (userhost != NULL) {
		*userhost++ = '\0';
		screen_win_msg(cur_window(), 1, 1, 1, MSG_TYPE_SIGNOFF,
			"You have been killed by %%c%s %%D(%%c%s%%D)%%x (%s)",
			in->tokens[0], userhost, reason);
	} else {
		screen_win_msg(cur_window(), 1, 1, 1, MSG_TYPE_SIGNOFF,
			"You have been killed by %%c%s%%x (%s)",
			in->tokens[0], reason);
	}

	free(reason);
	return (0);
}

static int irc_handler_invite(struct pork_acct *acct, struct irc_input *in) {
	char *p;

	if (in->num_tokens < 3 || in->args == NULL) {
		debug("invalid input from server: %s", in->orig);
		return (-1);
	}

	p = strchr(in->tokens[0], '!');
	if (p != NULL)
		*p++ = '\0';

	return (chat_got_invite(acct, in->args, in->tokens[0], p, NULL));
}

static int irc_handler_ping_reply(struct pork_acct *acct, struct irc_input *in)
{
	int32_t sec = 0;
	int32_t usec = 0;

	if (in->args != NULL && sscanf(in->args, "%d %d", &sec, &usec) > 0) {
		struct timeval tv;
		float timediff;

		gettimeofday(&tv, NULL);
		timediff = (tv.tv_sec - sec) + ((tv.tv_usec - usec) / 1000000.0);

		/* XXX */
		if (timediff >= 0) {
			char *host;

			host = strchr(in->tokens[0], '!');
			if (host == NULL)
				return (-1);
			*host++ = '\0';

			screen_win_msg(cur_window(), 1, 0, 1, MSG_TYPE_NOTICE_RECV,
				"%%YCTCP REPLY %%G%s %%D[%%xPING %f seconds%%D]%%x from %%C%s%%D(%%c%s%%D)%%x",
				acct->username, timediff, in->tokens[0], host);
			return (1);
		}
	}

	return (-1);
}

static int irc_handler_dcc_reply(struct pork_acct *acct, struct irc_input *in) {
	char *cmd;
	char *p;
	int ret;

	if (in->args == NULL)
		return (-1);

	p = in->args;
	cmd = strsep(&p, " ");

	if (cmd == NULL || p == NULL)
		return (-1);

	in->args = p;

	if (!strcasecmp(cmd, "REJECT")) {
		ret = irc_handler_dcc_reject(acct, in);
	} else {
		ret = -1;
	}

	return (ret);
}

int irc_callback_add_defaults(irc_session_t *session) {
	irc_callback_add(session, "004", irc_handler_print_tok);
	irc_callback_add(session, "252", irc_handler_print_num);
	irc_callback_add(session, "254", irc_handler_print_num);

	irc_callback_add(session, "005", irc_handler_005);
	irc_callback_add(session, "353", irc_handler_353);
	irc_callback_add(session, "352", irc_handler_352);
	irc_callback_add(session, "315", irc_handler_315);
	irc_callback_add(session, "001", irc_handler_001);

	irc_callback_add(session, "324", irc_handler_324);
	irc_callback_add(session, "221", irc_handler_221);
	irc_callback_add(session, "329", irc_handler_329);
	irc_callback_add(session, "321", irc_handler_321);
	irc_callback_add(session, "322", irc_handler_322);

	irc_callback_add(session, "301", irc_handler_301);
	irc_callback_add(session, "311", irc_handler_311);
	irc_callback_add(session, "319", irc_handler_319);
	irc_callback_add(session, "312", irc_handler_312);
	irc_callback_add(session, "317", irc_handler_317);
	irc_callback_add(session, "313", irc_handler_313);
	irc_callback_add(session, "332", irc_handler_332);
	irc_callback_add(session, "333", irc_handler_333);
	irc_callback_add(session, "314", irc_handler_314);
	irc_callback_add(session, "302", irc_handler_302);
	irc_callback_add(session, "367", irc_handler_367);
	irc_callback_add(session, "433", irc_handler_433);

	irc_callback_add(session, "263", irc_handler_err_msg);
	irc_callback_add(session, "401", irc_handler_err_msg);
	irc_callback_add(session, "402", irc_handler_err_msg);
	irc_callback_add(session, "403", irc_handler_err_msg);
	irc_callback_add(session, "404", irc_handler_err_msg);
	irc_callback_add(session, "405", irc_handler_err_msg);
	irc_callback_add(session, "406", irc_handler_err_msg);
	irc_callback_add(session, "407", irc_handler_err_msg);
	irc_callback_add(session, "408", irc_handler_err_msg);
	irc_callback_add(session, "413", irc_handler_err_msg);
	irc_callback_add(session, "414", irc_handler_err_msg);
	irc_callback_add(session, "415", irc_handler_err_msg);
	irc_callback_add(session, "421", irc_handler_err_msg);
	irc_callback_add(session, "423", irc_handler_err_msg);
	irc_callback_add(session, "432", irc_handler_err_msg);
	irc_callback_add(session, "437", irc_handler_err_msg);
	irc_callback_add(session, "442", irc_handler_err_msg);
	irc_callback_add(session, "444", irc_handler_err_msg);
	irc_callback_add(session, "461", irc_handler_err_msg);
	irc_callback_add(session, "467", irc_handler_err_msg);
	irc_callback_add(session, "471", irc_handler_err_msg);
	irc_callback_add(session, "472", irc_handler_err_msg);
	irc_callback_add(session, "473", irc_handler_err_msg);
	irc_callback_add(session, "474", irc_handler_err_msg);
	irc_callback_add(session, "475", irc_handler_err_msg);
	irc_callback_add(session, "476", irc_handler_err_msg);
	irc_callback_add(session, "477", irc_handler_err_msg);
	irc_callback_add(session, "482", irc_handler_err_msg);

	irc_callback_add(session, "JOIN", irc_handler_join);
	irc_callback_add(session, "PRIVMSG", irc_handler_privmsg);
	irc_callback_add(session, "NOTICE", irc_handler_notice);
	irc_callback_add(session, "MODE", irc_handler_mode);
	irc_callback_add(session, "PING", irc_handler_ping);
	irc_callback_add(session, "NICK", irc_handler_nick);
	irc_callback_add(session, "QUIT", irc_handler_quit);
	irc_callback_add(session, "PART", irc_handler_part);
	irc_callback_add(session, "KICK", irc_handler_kick);
	irc_callback_add(session, "TOPIC", irc_handler_topic);
	irc_callback_add(session, "INVITE", irc_handler_invite);
	irc_callback_add(session, "KILL", irc_handler_kill);

	irc_callback_add(session, "CTCP FINGER", irc_handler_ctcp_version);
	irc_callback_add(session, "CTCP CLIENTINFO", irc_handler_ctcp_version);
	irc_callback_add(session, "CTCP USERINFO", irc_handler_ctcp_version);
	irc_callback_add(session, "CTCP VERSION", irc_handler_ctcp_version);
	irc_callback_add(session, "CTCP TIME", irc_handler_ctcp_time);
	irc_callback_add(session, "CTCP PING", irc_handler_ctcp_echo);
	irc_callback_add(session, "CTCP ECHO", irc_handler_ctcp_echo);
	irc_callback_add(session, "CTCP ACTION", irc_handler_ctcp_action);
	irc_callback_add(session, "CTCP DCC", irc_handler_dcc);

	irc_callback_add(session, "CTCP REPLY DCC", irc_handler_dcc_reply);
	irc_callback_add(session, "CTCP REPLY PING", irc_handler_ping_reply);

	return (0);
}
