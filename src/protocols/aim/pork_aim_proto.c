/*
** pork_aim_proto.c - AIM OSCAR interface to pork.
** Copyright (C) 2005 Ryan McCabe <ryan@numb.org>
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

#include <pork.h>
#include <pork_missing.h>
#include <pork_util.h>
#include <pork_list.h>
#include <pork_imsg.h>
#include <pork_io.h>
#include <pork_misc.h>
#include <pork_color.h>
#include <pork_html.h>
#include <pork_proto.h>
#include <pork_acct.h>
#include <pork_buddy.h>
#include <pork_chat.h>
#include <pork_transfer.h>
#include <pork_conf.h>
#include <pork_screen_io.h>

#include <libfaim/faimconfig.h>
#include <libfaim/aim.h>

#include <pork_aim.h>
#include <pork_aim_proto.h>

#include <pork_screen.h>
#include <pork_set.h>

double banpause = 2000000.0;

static int write_room(struct aim_chat *chat) {
	char filename[65536];
	char buf[MAXSNLEN+1024];
	char *pork_dir = opt_get_str(OPT_PORK_DIR);
	FILE *file;
	dlist_t *cur;

	if (pork_dir == NULL) {
		sprintf(filename, ".");
		opt_set(OPT_PORK_DIR, filename);
	}

	snprintf(filename, 65535, "%s/%s", pork_dir, chat->title);

	file = fopen(filename, "wb");
	if (file == NULL)
		return 0;

	cur = chat->downers;
	while (cur != NULL) {
		snprintf(buf, MAXSNLEN+3, "d:%s\n", (char*)cur->data);
		fputs(buf, file);
		screen_err_msg("%s", buf);
		cur = cur->next;
	}

	cur = chat->fullops;
	while (cur != NULL) {
		snprintf(buf, MAXSNLEN+3, "f:%s\n", (char*)cur->data);
		fputs(buf, file);
		screen_err_msg("%s", buf);
		cur = cur->next;
	}

	cur = chat->oparray;
	while (cur != NULL) {
		snprintf(buf, MAXSNLEN+3, "o:%s\n", (char*)cur->data);
		fputs(buf, file);
		screen_err_msg("%s", buf);
		cur = cur->next;
	}

	cur = chat->halfops;
	while (cur != NULL) {
		snprintf(buf, MAXSNLEN+3, "h:%s\n", (char*)cur->data);
		fputs(buf, file);
		screen_err_msg("%s", buf);
		cur = cur->next;
	}

	cur = chat->immlist;
	while (cur != NULL) {
		snprintf(buf, MAXSNLEN+3, "i:%s\n", (char*)cur->data);
		fputs(buf, file);
		screen_err_msg("%s", buf);
		cur = cur->next;
	}

	cur = chat->abarray;
	while (cur != NULL) {
		snprintf(buf, MAXSNLEN+3, "b:%s\n", (char*)cur->data);
		fputs(buf, file);
		screen_err_msg("%s", buf);
		cur = cur->next;
	}

	cur = chat->akarray;
	while (cur != NULL) {
		snprintf(buf, MAXSNLEN+3, "k:%s\n", (char*)cur->data);
		fputs(buf, file);
		screen_err_msg("%s", buf);
		cur = cur->next;
	}

	cur = chat->awarray;
	while (cur != NULL) {
		snprintf(buf, MAXSNLEN+3, "w:%s\n", (char*)cur->data);
		fputs(buf, file);
		screen_err_msg("%s", buf);
		cur = cur->next;
	}

	fclose(file);
	return 1;
}

static void append_room(struct aim_chat *chat, char *sn) {
	char filename[65536];
	char *pork_dir = opt_get_str(OPT_PORK_DIR);
	FILE *file;

	if (pork_dir == NULL) {
		sprintf(filename, ".");
		opt_set(OPT_PORK_DIR, filename);
	}

	snprintf(filename, 65535, "%s/%s", pork_dir, chat->title);

	file = fopen(filename, "ab");
	if (file == NULL) {
		screen_err_msg("Unable to append %s to list for room %s.", sn, chat->title);
		return;
	}
	fputs(sn, file);
	fclose(file);
	return;
}

void check_ab_queues() {
	dlist_t *cur;
	struct chatroom *chat;
	struct pork_acct *acct; 
	struct aim_chat *a_chat;
	struct aim_priv *priv;
	int refnum = 0;
	char msg[MAXSNLEN+1024];
	dlist_t *temp;

	while ((cur = pork_acct_find(refnum)) != NULL) {
		refnum++;
		acct = cur->data;
		if (acct->proto->protocol != PROTO_AIM)
			continue;
		priv = acct->data;
		for (cur = acct->chat_list; cur != NULL; cur = cur->next) {
			chat = cur->data;
			a_chat = chat->data;
			if (a_chat->abqueue != NULL) {
				if (check_if_imm(a_chat->abqueue->data,
						chat)) {
					free(a_chat->abqueue->data);
					dlist_remove_head(&a_chat->abqueue);
					return;
				}
				if (a_chat->chatsends > 1) {
					snprintf(msg, MAXSNLEN+1023, 
						"banning %s.",
						(char*)a_chat->abqueue->data);
					aim_chat_send_im(&priv->aim_session, 
						a_chat->conn, 0, msg, 
						strlen(msg), "us-ascii", "en");
				}
				if (!dlist_find(a_chat->banlist, a_chat->abqueue->data, (void*)strcmp))
					a_chat->banlist = dlist_add_tail(a_chat->banlist, xstrdup(a_chat->abqueue->data));
				aim_chat_ban(&priv->aim_session, a_chat->conn,
					a_chat->abqueue->data);
				free(a_chat->abqueue->data);
				dlist_remove_head(&a_chat->abqueue);
			}
			if (a_chat->akqueue != NULL) {
				if (check_if_imm(a_chat->akqueue->data,
						chat)) {
					free(a_chat->akqueue->data);
					dlist_remove_head(&a_chat->akqueue);
					return;
				}
				if (a_chat->chatsends > 1) {
					snprintf(msg, MAXSNLEN+1023, 
						"kicking %s.",
						(char*)a_chat->akqueue->data);
					aim_chat_send_im(&priv->aim_session, 
						a_chat->conn, 0, msg, 
						strlen(msg), "us-ascii", "en");
				}
				if ((temp = dlist_find(a_chat->banlist, a_chat->akqueue->data, (void*)strcmp)) != NULL) {
					free(temp->data);
					a_chat->banlist = dlist_remove(a_chat->banlist, temp);
				}
				aim_chat_ban(&priv->aim_session, a_chat->conn,
					a_chat->akqueue->data);
				aim_chat_unban(&priv->aim_session, a_chat->conn,
					a_chat->akqueue->data);
				free(a_chat->akqueue->data);
				dlist_remove_head(&a_chat->akqueue);
			}
		}
	}
}

int check_if_aq(char *sn, struct chatroom *ccon) {
	struct aim_chat *a_chat = ccon->data;
	char buf[MAXSNLEN+1024];

	normalize(buf, sn, strlen(sn) + 1);
	if (dlist_find(a_chat->abqueue, buf, (void*)strcmp))
		return 1;
	return 0;
}

int check_if_aw(char *word, struct chatroom *ccon) {
	struct aim_chat *a_chat = ccon->data;
	if (dlist_find(a_chat->awarray, word, (void*)strcmp))
		return 1;
	return 0;
}

int check_if_ak(char *sn, struct chatroom *ccon) {
	struct aim_chat *a_chat = ccon->data;
	char buf[MAXSNLEN+1];

	normalize(buf, sn, strlen(sn) + 1);
	if (dlist_find(a_chat->akarray, buf, (void*)strcmp))
		return 1;
	return 0;
}

int check_if_ab(char *sn, struct chatroom *ccon) {
	struct aim_chat *a_chat = ccon->data;
	char buf[MAXSNLEN+1];

	normalize(buf, sn, strlen(sn) + 1);
	if (dlist_find(a_chat->abarray, buf, (void*)strcmp))
		return 1;
	return 0;
}

int check_if_downer(char *sn, struct chatroom *ccon) {
	struct aim_chat *a_chat = ccon->data;
	char buf[MAXSNLEN+1];

	normalize(buf, sn, strlen(sn) + 1);
	if (dlist_find(a_chat->downers, buf, (void*)strcmp))
		return 1;
	return 0;
}

int check_if_fullop(char *sn, struct chatroom *ccon) {
	struct aim_chat *a_chat = ccon->data;
	char buf[MAXSNLEN+1];

	if (check_if_downer(sn, ccon))
		return 1;

	normalize(buf, sn, strlen(sn) + 1);
	if (dlist_find(a_chat->fullops, buf, (void*)strcmp))
		return 1;
	return 0;
}

int check_if_op(char *sn, struct chatroom *ccon) {
	struct aim_chat *a_chat = ccon->data;
	char buf[MAXSNLEN+1];

	if (check_if_fullop(sn, ccon))
		return 1;

	normalize(buf, sn, strlen(sn) + 1);
	if (dlist_find(a_chat->oparray, buf, (void*)strcmp))
		return 1;
	return 0;
}

int check_if_halfop(char *sn, struct chatroom *ccon) {
	struct aim_chat *a_chat = ccon->data;
	char buf[MAXSNLEN+1];

	if (check_if_op(sn, ccon))
		return 1;

	normalize(buf, sn, strlen(sn) + 1);
	if (dlist_find(a_chat->halfops, buf, (void*)strcmp))
		return 1;
	return 0;
}

int check_if_imm(char *sn, struct chatroom *ccon) {
	struct aim_chat *a_chat = ccon->data;
	char buf[MAXSNLEN+1];

	if (check_if_op(sn, ccon))
		return 1;

	normalize(buf, sn, strlen(sn) + 1);
	if (dlist_find(a_chat->immlist, buf, (void*)strcmp))
		return 1;
	return 0;
}

static int partial_nick_comp(void *l, void *r) {
	char *snpart = l;
	struct chat_user *chat_user = r; 
	char sn[MAXSNLEN+1];

	normalize(sn, chat_user->nname, strlen(chat_user->nname) + 1);
	if (strstr(sn, snpart))
		return 0;
	return 1;
}

static void partial_nick(char *sn, char *snout, struct chatroom *ccon) {
	dlist_t *people = ccon->user_list;
	dlist_t *person;

	normalize(snout, sn, strlen(sn) + 1);
	if ((person = dlist_find(people, snout, partial_nick_comp))) {
		struct chat_user *chat_user = person->data;
		strcpy(snout, chat_user->nname);
		return;
	}
	return;
}

static void check_akusers_in_room(struct chatroom *chat) {
	struct aim_chat *a_chat = chat->data;
	dlist_t *chat_users = chat->user_list;
	struct chat_user *chat_user;

	while (chat_users != NULL) {
		dlist_t *cur = a_chat->akarray;
		chat_user = chat_users->data;
		while (cur != NULL) {
			if (strstr(chat_user->nname, cur->data)) {
				a_chat->akqueue = dlist_add_tail(a_chat->akqueue, xstrdup(chat_user->nname));
				break;
			}
			cur = cur->next;
		}
		chat_users = chat_users->next;
	}
}

static void check_abusers_in_room(struct chatroom *chat) {
	struct aim_chat *a_chat = chat->data;
	dlist_t *chat_users = chat->user_list;
	struct chat_user *chat_user;

	while (chat_users != NULL) {
		dlist_t *cur = a_chat->abarray;
		chat_user = chat_users->data;
		while (cur != NULL) {
			if (strstr(chat_user->nname, cur->data)) {
				a_chat->abqueue = dlist_add_tail(a_chat->abqueue, xstrdup(chat_user->nname));
				break;
			}
			cur = cur->next;
		}
		chat_users = chat_users->next;
	}
}

void parse_ccom(char *cmd, struct chatroom *room, struct pork_acct *acct, int priv) {
	struct aim_chat *ccon = room->data;
	struct aim_priv *a_priv = acct->data;
	dlist_t *temp;
	char sn[MAXSNLEN+1];
	char msg[MAXSNLEN+1024];
	char *pmsg = msg;

	cmd[strcspn(cmd,"/")] = '\0';
	if(!strncasecmp(cmd, "op ", 3) && (priv >= FULLOPS)) {
		cmd += 3;
		if (!aim_snvalid(cmd))
			return;
		partial_nick(cmd, sn, room);
		if (check_if_op(sn, room))
			return;
		ccon->oparray = dlist_add_tail(ccon->oparray, xstrdup(sn));
		if (ccon->chatsends > 1) {
			sprintf(msg, "%s has been opped.", sn);
			aim_chat_send_im(&a_priv->aim_session, ccon->conn,
				0, pmsg, strlen(msg), "us-ascii", "en");
		}
	} else if(!strncasecmp(cmd, "deop ", 5) && (priv >= FULLOPS)) {
		cmd += 5;
		if (!aim_snvalid(cmd))
			return;
		partial_nick(cmd, sn, room);
		if (!check_if_op(sn, room))
			return;
		if (temp = dlist_find(ccon->oparray, sn, (void*)strcmp)) {
			free(temp->data);
			ccon->oparray = dlist_remove(ccon->oparray, temp);
			if (ccon->chatsends > 1) {
				sprintf(msg, "%s has been deopped.", sn);
				aim_chat_send_im(&a_priv->aim_session, ccon->conn,
					0, pmsg, strlen(msg), "us-ascii", "en");
			}
		}
	} else if(!strncasecmp(cmd, "defullop ", 7) && (priv >= DOWNERS)) {
		cmd += 9;
		if (!aim_snvalid(cmd))
			return;
		partial_nick(cmd, sn, room);
		if (temp = dlist_find(ccon->fullops, sn, (void*)strcmp)) {
			free(temp->data);
			ccon->fullops = dlist_remove(ccon->fullops, temp);
			if (ccon->chatsends > 1) {
				sprintf(msg, "%s has been defullopped.", sn);
				aim_chat_send_im(&a_priv->aim_session, ccon->conn,
					0, pmsg, strlen(msg), "us-ascii", "en");
			}
		}
	} else if(!strncasecmp(cmd, "fullop ", 7) && (priv >= FULLOPS)) {
		cmd += 7;
		if (!aim_snvalid(cmd))
			return;
		partial_nick(cmd, sn, room);
		if (check_if_fullop(sn, room))
			return;
		ccon->fullops = dlist_add_tail(ccon->fullops, xstrdup(sn));
		if (ccon->chatsends > 1) {
			sprintf(msg, "%s has been fullopped.", sn);
			aim_chat_send_im(&a_priv->aim_session, ccon->conn,
				0, pmsg, strlen(msg), "us-ascii", "en");
		}
	} else if(!strncasecmp(cmd, "halfop ", 7) && (priv > HALFOPS)) {
		cmd += 7;
		if (!aim_snvalid(cmd))
			return;
		partial_nick(cmd, sn, room);
		if (check_if_halfop(sn, room))
			return;
		ccon->halfops = dlist_add_tail(ccon->halfops, xstrdup(sn));
		if (ccon->chatsends > 1) {
			sprintf(msg, "%s has been halfopped.", sn);
			aim_chat_send_im(&a_priv->aim_session, ccon->conn,
				0, pmsg, strlen(msg), "us-ascii", "en");
		}
	} else if(!strncasecmp(cmd, "dehalfop ", 9) && (priv > HALFOPS)) {
		cmd += 9;
		if (!aim_snvalid(cmd))
			return;
		partial_nick(cmd, sn, room);
		if (!check_if_halfop(sn, room))
			return;
		if (temp = dlist_find(ccon->halfops, sn, (void*)strcmp)) {
			free(temp->data);
			ccon->halfops = dlist_remove(ccon->halfops, temp);
			if (ccon->chatsends > 1) {
				sprintf(msg, "%s has been dehalfopped.", sn);
				aim_chat_send_im(&a_priv->aim_session, ccon->conn,
					0, pmsg, strlen(msg), "us-ascii", "en");
			}
		}
	} else if(!strncasecmp(cmd, "kick ", 5)) {
		cmd += 5;
		if (!aim_snvalid(cmd))
			return;
		partial_nick(cmd, sn, room);
		if (check_if_imm(sn, room))
			return;
		if (ccon->chatsends > 1) {
			snprintf(msg, MAXSNLEN+127, "kicking %s.", sn);
			aim_chat_send_im(&a_priv->aim_session, ccon->conn,
				0, pmsg, strlen(msg), "us-ascii", "en");
		}
		if ((temp = dlist_find(ccon->banlist, sn, (void*)strcmp)) != NULL) {
			free(temp->data);
			ccon->banlist = dlist_remove(ccon->banlist, temp);
		}
		aim_chat_ban(&a_priv->aim_session, ccon->conn, sn);
		aim_chat_unban(&a_priv->aim_session, ccon->conn, sn);
	} else if(!strncasecmp(cmd, "ban ", 4) && (priv > HALFOPS)) {
		cmd += 4;
		if (!aim_snvalid(cmd))
			return;
		partial_nick(cmd, sn, room);
		if (check_if_imm(sn, room))
			return;
		if (ccon->chatsends > 1) {
			snprintf(msg, MAXSNLEN+127, "banning %s.", sn);
			aim_chat_send_im(&a_priv->aim_session, ccon->conn,
				0, pmsg, strlen(msg), "us-ascii", "en");
		}
		if (!dlist_find(ccon->banlist, sn, (void*)strcmp))
			ccon->banlist = dlist_add_tail(ccon->banlist, xstrdup(sn));
		aim_chat_ban(&a_priv->aim_session, ccon->conn, sn);
	} else if(!strncasecmp(cmd, "unban ", 6) && (priv > HALFOPS)) {
		cmd += 6;
		if (!aim_snvalid(cmd))
			return;
		if (ccon->chatsends > 1) {
			snprintf(msg, MAXSNLEN+127, "unbanning %s.", cmd);
			aim_chat_send_im(&a_priv->aim_session, ccon->conn,
				0, pmsg, strlen(msg), "us-ascii", "en");
		}
		if ((temp = dlist_find(ccon->banlist, cmd, (void*)strcmp)) != NULL) {
			free(temp->data);
			ccon->banlist = dlist_remove(ccon->banlist, temp);
		}
		aim_chat_unban(&a_priv->aim_session, ccon->conn, cmd);
	} else if(!strncasecmp(cmd, "pause ", 6) && (priv >= FULLOPS)) {
		cmd += 6;
		banpause = atoi(cmd);
		if (banpause < 0)
			banpause = 0;
		sprintf(msg, "autoban pause set to %g microseconds.", banpause);
		if (ccon->chatsends > 1)
			aim_chat_send_im(&a_priv->aim_session, ccon->conn,
				0, pmsg, strlen(msg), "us-ascii", "en");
	} else if(!strncasecmp(cmd, "help", 4) && ccon->chatsends) {
		char commands[MAXMSGLEN+1];
		strcpy(commands, "commands are: !status, !kick, !imm");
		if (priv > HALFOPS)
			strcat(commands,", !unimm, !ban, !unban, !ab, !unab, !aw, !unaw, !halfop, !dehalfop, !bj");
		if (priv > OPS)
			strcat(commands,", !ak, !unak, !op, !deop, !fullop");
		if (priv > FULLOPS)
			strcat(commands,", !defullop");
		strcat(commands, ".");
		aim_chat_send_im(&a_priv->aim_session, ccon->conn,
			0, commands, strlen(commands), "us-ascii", "en");
	} else if(!strncasecmp(cmd, "imm ", 4)) {
		cmd += 4;
		if (!aim_snvalid(cmd))
			return;
		partial_nick(cmd, sn, room);
		if (check_if_imm(sn, room))
			return;
		ccon->immlist = dlist_add_tail(ccon->immlist, xstrdup(sn));
		if (ccon->chatsends > 1) {
			sprintf(msg, "%s has been immed.", sn);
			aim_chat_send_im(&a_priv->aim_session, ccon->conn,
				0, pmsg, strlen(msg), "us-ascii", "en");
		}
	} else if(!strncasecmp(cmd, "unimm ", 6) && (priv > HALFOPS)) {
		cmd += 6;
		if (!aim_snvalid(cmd))
			return;
		partial_nick(cmd, sn, room);
		if (!check_if_imm(sn, room))
			return;
		if (temp = dlist_find(ccon->immlist, sn, (void*)strcmp)) {
			free(temp->data);
			ccon->immlist = dlist_remove(ccon->immlist, temp);
			if (ccon->chatsends > 1) {
				sprintf(msg, "%s has been unimmed.", sn);
				aim_chat_send_im(&a_priv->aim_session, ccon->conn,
					0, pmsg, strlen(msg), "us-ascii", "en");
			}
		}
	} else if(!strncasecmp(cmd, "ak ", 3) && (priv > OPS)) {
		cmd += 3;
		if (!aim_snvalid(cmd))
			return;
		if (check_if_ak(cmd, room))
			return;
		normalize(sn, cmd, strlen(cmd) + 1);
		ccon->akarray = dlist_add_tail(ccon->akarray, xstrdup(cmd));
		if (ccon->chatsends > 1) {
			sprintf(msg, "%s has been autokicked.", sn);
			aim_chat_send_im(&a_priv->aim_session, ccon->conn,
				0, pmsg, strlen(msg), "us-ascii", "en");
		}
		check_akusers_in_room(room);
	} else if(!strncasecmp(cmd, "ab ", 3) && (priv > HALFOPS)) {
		cmd += 3;
		if (!aim_snvalid(cmd))
			return;
		if (check_if_ab(cmd, room))
			return;
		normalize(sn, cmd, strlen(cmd) + 1);
		ccon->abarray = dlist_add_tail(ccon->abarray, xstrdup(cmd));
		if (ccon->chatsends > 1) {
			sprintf(msg, "%s has been autobanned.", sn);
			aim_chat_send_im(&a_priv->aim_session, ccon->conn,
				0, pmsg, strlen(msg), "us-ascii", "en");
		}
		check_abusers_in_room(room);
	} else if(!strncasecmp(cmd, "aw ", 3) && (priv > HALFOPS)) {
		cmd += 3;
		if (check_if_aw(cmd, room))
			return;
		ccon->awarray = dlist_add_tail(ccon->awarray, xstrdup(cmd));
		if (ccon->chatsends > 1) {
			sprintf(msg, "%s has been autoworded.", cmd);
			aim_chat_send_im(&a_priv->aim_session, ccon->conn,
				0, pmsg, strlen(msg), "us-ascii", "en");
		}
	} else if(!strncasecmp(cmd, "unak ", 5) && (priv > OPS)) {
		cmd += 5;
		if (!aim_snvalid(cmd))
			return;
		if (!check_if_ak(cmd, room))
			return;
		if (temp = dlist_find(ccon->akarray, cmd, (void*)strcmp)) {
			free(temp->data);
			ccon->akarray = dlist_remove(ccon->akarray, temp);
			if (0) { //ccon->chatsends > 1) {
				sprintf(msg, "%s has been unautokicked.", cmd);
				aim_chat_send_im(&a_priv->aim_session, ccon->conn,
					0, pmsg, strlen(msg), "us-ascii", "en");
			}
		}
	} else if(!strncasecmp(cmd, "unab ", 5) && (priv > HALFOPS)) {
		cmd += 5;
		if (!aim_snvalid(cmd))
			return;
		if (!check_if_ab(cmd, room))
			return;
		if (temp = dlist_find(ccon->abarray, cmd, (void*)strcmp)) {
			free(temp->data);
			ccon->abarray = dlist_remove(ccon->abarray, temp);
			if (0) { //ccon->chatsends > 1) {
				sprintf(msg, "%s has been unautobanned.", cmd);
				aim_chat_send_im(&a_priv->aim_session, ccon->conn,
					0, pmsg, strlen(msg), "us-ascii", "en");
			}
		}
	} else if(!strncasecmp(cmd, "unaw ", 5) && (priv > HALFOPS)) {
		cmd += 5;
		if (!check_if_aw(cmd, room))
			return;
		if (temp = dlist_find(ccon->awarray, cmd, (void*)strcmp)) {
			free(temp->data);
			ccon->awarray = dlist_remove(ccon->awarray, temp);
			if (ccon->chatsends > 1) {
				snprintf(msg, 1024, "%s has been unautoworded.", cmd);
				aim_chat_send_im(&a_priv->aim_session, ccon->conn,
					0, pmsg, strlen(msg), "us-ascii", "en");
			}
		}
	} else if(!strncasecmp(cmd, "status ", 7) && ccon->chatsends) {
		strcpy(msg, "");
		cmd += 7;
		if (!aim_snvalid(cmd))
			return;
		partial_nick(cmd, sn, room);
		if (check_if_fullop(sn, room)) {
			sprintf(msg, "%s is a full op.", sn);
		} else if (check_if_op(sn, room)) {
			sprintf(msg, "%s is an op.", sn);
		} else if (check_if_halfop(sn, room)) {
			sprintf(msg, "%s is a half op", sn);
			if (check_if_imm(sn, room))
				strcat(msg, " and immune");
			strcat(msg, ".");
		} else if (check_if_imm(sn, room)) {
			sprintf(msg, "%s is immune.", sn);
		}
		if (strlen(msg))
			aim_chat_send_im(&a_priv->aim_session, ccon->conn,
				0, pmsg, strlen(msg), "us-ascii", "en");
	} else if(!strncasecmp(cmd, "chatsends ", 10) && (priv >= FULLOPS)) {
		int chatsends;
		cmd += 10;
		chatsends = atoi(cmd);
		if ((chatsends >= 0) && (chatsends <= 2)) {
			ccon->chatsends = chatsends;
			if (ccon->chatsends > 1) {
				sprintf(msg, "chatsends now set to %d.", ccon->chatsends);
				aim_chat_send_im(&a_priv->aim_session, ccon->conn, 0, msg, strlen(msg), "us-ascii", "en");
			}
		}
	} else if(!strncasecmp(cmd, "save", 4) && (priv >= FULLOPS)) {
		if (!write_room(ccon)) {
			screen_err_msg("unable to save config for room %s.\n", ccon->title);
		}
	} else if(!strncasecmp(cmd, "bj", 2) && (priv > HALFOPS)) {
		ccon->banjoin = !ccon->banjoin;
		if (ccon->chatsends > 1) {
			sprintf(msg, "banjoin: %s.", ccon->banjoin?"on":"off");
			aim_chat_send_im(&a_priv->aim_session, ccon->conn, 0, msg, strlen(msg), "us-ascii", "en");
		}
	}
}

int aim_chat_rejoin(struct pork_acct *acct, struct chatroom *chat) { //EBC
	char *chatname;

	chatname = xstrdup(chat->title);

	cmd_chat_leave(chatname);
	cmd_chat_join(chatname);

	free(chatname);
}

static int aim_buddy_add(struct pork_acct *acct, struct buddy *buddy) {
	int ret;
	struct aim_priv *priv = acct->data;
	aim_session_t *session = &priv->aim_session;

	if (session->ssi.received_data == 0) {
		debug("buddy add failed");
		return (-1);
	}

	if (aim_ssi_itemlist_exists(session->ssi.local, buddy->nname)) {
		debug("buddy %s already exists for %s", buddy->nname, acct->username);
		return (-1);
	}

	ret = aim_ssi_addbuddy(session, buddy->nname,
			buddy->group->name, buddy->name, NULL, NULL, 0);

	return (ret);
}

static int aim_buddy_alias(struct pork_acct *acct, struct buddy *buddy) {
	struct aim_priv *priv = acct->data;
	int ret;

	if (priv->aim_session.ssi.received_data == 0) {
		debug("buddy alias failed");
		return (-1);
	}

	ret = aim_ssi_aliasbuddy(&priv->aim_session,
			buddy->group->name, buddy->nname, buddy->name);

	return (ret);
}

static int aim_add_block(struct pork_acct *acct, char *target) {
	struct aim_priv *priv = acct->data;

	if (priv->aim_session.ssi.received_data == 0)
		return (-1);

	return (aim_ssi_adddeny(&priv->aim_session, target));
}

static int aim_add_permit(struct pork_acct *acct, char *target) {
	struct aim_priv *priv = acct->data;

	if (priv->aim_session.ssi.received_data == 0) {
		debug("add permit failed");
		return (-1);
	}

	return (aim_ssi_addpermit(&priv->aim_session, target));
}

static int aim_buddy_remove(struct pork_acct *acct, struct buddy *buddy) {
	struct aim_priv *priv = acct->data;
	int ret;

	if (priv->aim_session.ssi.received_data == 0) {
		debug("buddy remove failed");
		return (-1);
	}

	ret = aim_ssi_delbuddy(&priv->aim_session, buddy->nname,
			buddy->group->name);

	return (ret);
}

static int aim_remove_permit(struct pork_acct *acct, char *target) {
	struct aim_priv *priv = acct->data;

	if (priv->aim_session.ssi.received_data == 0) {
		debug("remove permit failed");
		return (-1);
	}

	return (aim_ssi_delpermit(&priv->aim_session, target));
}

static int aim_unblock(struct pork_acct *acct, char *target) {
	struct aim_priv *priv = acct->data;

	if (priv->aim_session.ssi.received_data == 0) {
		debug("unblock failed");
		return (-1);
	}

	return (aim_ssi_deldeny(&priv->aim_session, target));
}

static int aim_update_buddy(struct pork_acct *acct __notused,
							struct buddy *buddy,
							void *data)
{
	aim_userinfo_t *userinfo = data;

	buddy->signon_time = userinfo->onlinesince;
	buddy->warn_level = (float) userinfo->warnlevel / 10;

	if (userinfo->present & AIM_USERINFO_PRESENT_FLAGS)
		buddy->type = userinfo->flags;

	if (userinfo->present & AIM_USERINFO_PRESENT_SESSIONLEN)
		buddy->idle_time = userinfo->idletime;

	buddy->status = STATUS_ACTIVE;

	if (buddy->idle_time > 0)
		buddy->status = STATUS_IDLE;

	if (userinfo->flags & AIM_FLAG_AWAY)
		buddy->status = STATUS_AWAY;

	return (0);
}

static struct chatroom *aim_find_chat_name_data(struct pork_acct *acct,
												char *name)
{
	dlist_t *ret;

	ret = aim_find_chat_name(acct, name);
	if (ret == NULL || ret->data == NULL)
		return (NULL);

	return (ret->data);
}

static int aim_leave_chatroom(struct pork_acct *acct, struct chatroom *chat) {
	struct aim_priv *priv = acct->data;
	struct aim_chat *a_chat;

	a_chat = chat->data;

	if (a_chat->oparray)
		dlist_destroy(a_chat->oparray, NULL, NULL);
	if (a_chat->fullops)
		dlist_destroy(a_chat->fullops, NULL, NULL);
	if (a_chat->halfops)
		dlist_destroy(a_chat->halfops, NULL, NULL);
	if (a_chat->immlist)
		dlist_destroy(a_chat->immlist, NULL, NULL);
	if (a_chat->abarray)
		dlist_destroy(a_chat->abarray, NULL, NULL);
	if (a_chat->akarray)
		dlist_destroy(a_chat->akarray, NULL, NULL);
	if (a_chat->awarray)
		dlist_destroy(a_chat->awarray, NULL, NULL);
	if (a_chat->abqueue)
		dlist_destroy(a_chat->abqueue, NULL, NULL);

	pork_io_del(a_chat->conn);
	aim_conn_kill(&priv->aim_session, &a_chat->conn);

	a_chat->conn = NULL;
	return (0);
}

static int aim_join_chatroom(struct pork_acct *acct, char *name, char *args) {
	int ret;
	struct aim_priv *priv = acct->data;
	aim_conn_t *chatnav_conn;
	struct chatroom_info info;
	struct chatroom *chat;

	chat = chat_find(acct, name);
	if (chat != NULL && chat->data != NULL)
		return (0);

	if (aim_chat_parse_name(name, &info) != 0) {
		debug("aim_chat_parse_name failed for %s", name);
		return (-1);
	}

	chatnav_conn = priv->chatnav_conn;
	if (chatnav_conn == NULL) {
		struct chatroom_info *chat_info = xcalloc(1, sizeof(*chat_info));

		chat_info->name = xstrdup(info.name);
		chat_info->exchange = (u_int16_t) info.exchange;

		priv->chat_create_list = dlist_add_head(priv->chat_create_list,
									chat_info);
		aim_reqservice(&priv->aim_session, priv->bos_conn,
			AIM_CONN_TYPE_CHATNAV);

		free(info.name);
		return (0);
	}

	ret = aim_chatnav_createroom(&priv->aim_session, chatnav_conn,
			info.name, info.exchange);

	free(info.name);
	return (ret);
}

static int aim_chat_send(	struct pork_acct *acct,
					struct chatroom *chat,
					char *target __notused,
					char *msg)
{
	char *msg_html;
	int msg_len;
	int ret;
	struct aim_priv *priv = acct->data;
	struct aim_chat *a_chat;
	char *cmd;

	if (msg == NULL) {
		debug("aim_chat_send with NULL msg");
		return (-1);
	}

	a_chat = chat->data;

	if (cmd = strchr(msg, '!')) {
		cmd++;
//screen_win_msg(cur_window(), 0, 1, 0, MSG_TYPE_STATUS, cmd); //EBC
		parse_ccom(cmd, chat, acct, DOWNERS);
	}

	msg_html = text_to_html(msg);
	msg_len = strlen(msg_html);

	if (msg_len > a_chat->max_msg_len) {
		debug("msg len too long");
		return (-1);
	}

	ret = aim_chat_send_im(&priv->aim_session, a_chat->conn,
			AIM_CHATFLAGS_NOREFLECT, msg_html, msg_len, "us-ascii", "en");

	return (ret);
}

static int aim_chat_kick(struct pork_acct *acct,
				struct chatroom *chat,
				char *user,
				char *msg)
{
	struct aim_priv *priv = acct->data;
	struct aim_chat *a_chat;
	int ret;
	dlist_t *temp;

	if (user == NULL) {
		debug("no sn to kick");
		return (-1);
	}

	a_chat = chat->data;

	if (msg != NULL)
		aim_chat_send_im(&priv->aim_session, a_chat->conn, AIM_CHATFLAGS_NOREFLECT, msg, strlen(msg), "us-ascii", "en");

	if ((temp = dlist_find(a_chat->banlist, user, (void*)strcmp)) != NULL) {
		free(temp->data);
		a_chat->banlist = dlist_remove(a_chat->banlist, temp);
	}
	aim_chat_ban(&priv->aim_session, a_chat->conn, user);
	aim_chat_unban(&priv->aim_session, a_chat->conn, user);
}

static int aim_chat_banban(struct pork_acct *acct,
				struct chatroom *chat,
				char *user)
{
	struct aim_priv *priv = acct->data;
	struct aim_chat *a_chat;
	int ret;

	if (user == NULL) {
		debug("no sn to ban");
		return (-1);
	}

	a_chat = chat->data;

	if (!dlist_find(a_chat->banlist, user, (void*)strcmp))
		a_chat->banlist = dlist_add_tail(a_chat->banlist, xstrdup(user));
	aim_chat_ban(&priv->aim_session, a_chat->conn, user);
}

static int aim_chat_action(struct pork_acct *acct,
					struct chatroom *chat,
					char *target __notused,
					char *msg)
{
	char buf[8192];
	int ret;

	if (!strncasecmp(msg, "<html>", 6))
		ret = snprintf(buf, sizeof(buf), "<HTML>/me %s", &msg[6]);
	else
		ret = snprintf(buf, sizeof(buf), "/me %s", msg);

	if (ret < 0 || (size_t) ret >= sizeof(buf))
		return (-1);

	return (aim_chat_send(acct, chat, chat->title_quoted, buf));
}

static int aim_chat_send_invite(struct pork_acct *acct,
								struct chatroom *chat,
								char *dest,
								char *msg)
{
	struct aim_priv *priv = acct->data;
	int ret;
	struct aim_chat *a_chat;

	a_chat = chat->data;

	if (msg == NULL)
		msg = "";

	ret = aim_im_sendch2_chatinvite(&priv->aim_session, dest, msg,
			a_chat->exchange, a_chat->fullname, 0);

	return (ret);
}

static int aim_search(struct pork_acct *acct, char *str) {
	struct aim_priv *priv = acct->data;

	return (aim_search_address(&priv->aim_session, priv->bos_conn, str));
}

static int aim_keepalive(struct pork_acct *acct) {
	struct aim_priv *priv = acct->data;

	return (aim_flap_nop(&priv->aim_session, priv->bos_conn));
}

static int aim_acct_update(struct pork_acct *acct) {
	struct aim_priv *priv;

	if (!acct->connected)
		return (-1);

	/*
	** We've got to keep track of AIM buddies' idle time
	** ourselves; the AIM server reports an idle time
	** for them only when there's a change (i.e. they
	** go idle, come back from idle or their client reports
	** a new idle time). This routine will run every minute
	** and increment the idle time of buddies where appropriate.
	*/

	priv = acct->data;
	if (time(NULL) >= priv->last_update + 60) {
		aim_keepalive(acct);
		buddy_update_idle(acct);
		aim_cleansnacs(&priv->aim_session, 59);
		time(&priv->last_update);
	}

	return (0);
}

static int aim_acct_init(struct pork_acct *acct) {
	char buf[NUSER_LEN];
	struct aim_priv *priv;

	normalize(buf, acct->username, sizeof(buf));

	/*
	** You can't have the same screen name logged in more than
	** once with AIM.
	*/
	if (pork_acct_find_name(buf, PROTO_AIM) != NULL)
		return (-1);

	free(acct->username);
	acct->username = xstrdup(buf);

	priv = xcalloc(1, sizeof(*priv));
	acct->data = priv;

	if (aim_setup(acct) != 0) {
		free(priv);
		return (-1);
	}

	if (acct->profile == NULL)
		acct->profile = xstrdup(DEFAULT_AIM_PROFILE);

	return (0);
}

static int aim_acct_free(struct pork_acct *acct) {
	aim_kill_all_conn(acct);
	free(acct->data);
	return (0);
}

static int aim_connect(struct pork_acct *acct, char *args) {
	char *ptr;

	if (acct->passwd == NULL) {
		if (args != NULL && !blank_str(args)) {
			ptr = strchr(args, ' ');
			if (ptr != NULL) {
				*(ptr++) = '\0';
				if (!blank_str(ptr)) {
					acct->server = xstrdup(ptr);
				} else {
					acct->server = xstrdup(FAIM_LOGIN_SERVER);
				}
			}
			acct->passwd = xstrdup(args);
			memset(args, 0, strlen(args));
		} else {
			char buf[128];

			screen_prompt_user("Password: ", buf, sizeof(buf));
			if (buf[0] == '\0') {
				screen_err_msg("There was an error reading your password");
				return (-1);
			}

			acct->passwd = xstrdup(buf);
			memset(buf, 0, sizeof(buf));
		}
	}

	return (aim_login(acct, acct->server));
}

static int aim_read_config(struct pork_acct *acct) {
	return (read_user_config(acct));
}

static int aim_write_config(struct pork_acct *acct) {
	return (save_user_config(acct));
}

static int aim_file_recv_data(struct file_transfer *xfer, char *buf, size_t len) {
	struct aim_oft_info *oft_info = xfer->data;

	oft_info->fh.recvcsum = aim_oft_checksum_chunk(buf, len,
								oft_info->fh.recvcsum);

	if (xfer->bytes_sent + xfer->start_offset >= xfer->file_len)
		transfer_recv_complete(xfer);

	return (0);
}

static int aim_file_recv_complete(struct file_transfer *xfer) {
	struct aim_oft_info *oft_info = xfer->data;

	oft_info->fh.nrecvd = xfer->bytes_sent;

	aim_oft_sendheader(oft_info->sess, AIM_CB_OFT_DONE, oft_info);

	aim_clearhandlers(oft_info->conn);
	aim_conn_kill(oft_info->sess, &oft_info->conn);
	aim_oft_destroyinfo(oft_info);

	pork_io_del(xfer);
	return (0);
}

static int aim_file_send_complete(struct file_transfer *xfer) {
	struct aim_oft_info *oft_info = xfer->data;

	aim_clearhandlers(oft_info->conn);
	aim_conn_kill(oft_info->sess, &oft_info->conn);
	aim_oft_destroyinfo(oft_info);

	pork_io_del(xfer);
	return (0);
}

static int aim_file_send(struct file_transfer *xfer) {
	struct aim_priv *priv = xfer->acct->data;
	struct aim_oft_info *oft_info;

	if (transfer_bind_listen_sock(xfer, priv->bos_conn->fd) == -1)
		return (-1);

	oft_info = aim_oft_createinfo(&priv->aim_session, NULL,
				xfer->peer_username, xfer->laddr_ip, xfer->lport,
				xfer->file_len, 0, xfer->fname_base);

	oft_info->fh.checksum = aim_oft_checksum_file(xfer->fname_local);
	oft_info->port = xfer->lport;

	aim_sendfile_listen(&priv->aim_session, oft_info, xfer->sock);

	if (oft_info->conn == NULL) {
		aim_oft_destroyinfo(oft_info);
		return (-1);
	}

	oft_info->conn->priv = xfer;
	xfer->data = oft_info;

	pork_io_add(xfer->sock, IO_COND_RW, oft_info->conn, oft_info->conn,
		aim_listen_conn_event);

	aim_im_sendch2_sendfile_ask(&priv->aim_session, oft_info);
	aim_conn_addhandler(&priv->aim_session, oft_info->conn, AIM_CB_FAM_OFT, AIM_CB_OFT_ESTABLISHED, aim_file_send_accepted, 0);

	transfer_request_send(xfer);
	return (0);
}

static int aim_file_send_data(	struct file_transfer *xfer,
						char *buf __notused,
						size_t len __notused)
{
	if (xfer->status == TRANSFER_STATUS_COMPLETE) {
		pork_io_del(xfer);
		return (transfer_send_complete(xfer));
	}

	return (0);
}

static int aim_file_abort(struct file_transfer *xfer) {
	struct aim_oft_info *oft_info = xfer->data;

	pork_io_del(xfer);

	if (oft_info != NULL) {
		pork_io_del(oft_info->conn);
		aim_im_sendch2_sendfile_cancel(oft_info->sess, oft_info);
		aim_clearhandlers(oft_info->conn);

		if (!(xfer->protocol_flags & AIM_XFER_IN_HANDLER))
			aim_conn_kill(oft_info->sess, &oft_info->conn);
		else
			aim_conn_close(oft_info->conn);

		aim_oft_destroyinfo(oft_info);
	}

	return (0);
}

static int aim_file_accept(struct file_transfer *xfer) {
	struct aim_oft_info *oft_info;
	struct aim_priv *priv = xfer->acct->data;
	char buf[512];
	int ret;
	int sock;

	oft_info = xfer->data;

	ret = snprintf(buf, sizeof(buf), "%s:%d", oft_info->verifiedip,
			oft_info->port);

	if (ret < 0 || (size_t) ret >= sizeof(buf))
		return (-1);

	oft_info->conn = aim_newconn(&priv->aim_session,
						AIM_CONN_TYPE_RENDEZVOUS, NULL);

	if (oft_info->conn == NULL) {
		screen_err_msg("Error connecting to %s@%s while receiving %s",
			xfer->peer_username, buf, xfer->fname_local);
		return (-1);
	}

	oft_info->conn->subtype = AIM_CONN_SUBTYPE_OFT_SENDFILE;
	oft_info->conn->priv = xfer;

	ret = aim_sock_connect(buf, &xfer->acct->laddr, &sock);
	if (ret == 0) {
		aim_connected(sock, 0, oft_info->conn);
	} else if (ret == -EINPROGRESS) {
		oft_info->conn->status |= AIM_CONN_STATUS_INPROGRESS;
		pork_io_add(sock, IO_COND_WRITE, oft_info->conn, oft_info->conn,
			aim_connected);
	} else {
		aim_conn_kill(&priv->aim_session, &oft_info->conn);
		screen_err_msg("Error connecting to %s@%s while receiving %s",
			xfer->peer_username, buf, xfer->fname_local);
		return (-1);
	}

	aim_conn_addhandler(&priv->aim_session, oft_info->conn,
		AIM_CB_FAM_OFT, AIM_CB_OFT_PROMPT, aim_file_recv_accept, 0);
	aim_conn_addhandler(&priv->aim_session, oft_info->conn,
		AIM_CB_FAM_SPECIAL, AIM_CB_SPECIAL_CONNDEAD, aim_file_transfer_dead, 0);

	transfer_recv_accepted(xfer);
	return (0);
}

static char *aim_filter_text_out(char *msg) {
	if (!strncasecmp(msg, "<HTML", 5))
		return (strip_html(msg));

	return (color_quote_codes(msg));
}

static int aim_chat_get_name(	const char *str,
						char *buf,
						size_t len,
						char *arg_buf,
						size_t arg_len)
{
	struct chatroom_info info;
	int ret;

	if (aim_chat_parse_name(str, &info) != 0) {
		debug("aim_chat_parse_name failed for %s", str);
		return (-1);
	}

	ret = snprintf(buf, len, "%s/%d", info.name, info.exchange);
	free(info.name);

	if (ret < 0 || (size_t) ret >= len)
		return (-1);

	return (0);
}

static int aim_set_privacy_mode(struct pork_acct *acct, int mode) {
	struct aim_priv *priv = acct->data;

	if (mode >= 0 && mode <= 5) {
		acct->buddy_pref->privacy_mode = mode;
		aim_ssi_setpermdeny(&priv->aim_session,
			acct->buddy_pref->privacy_mode, 0xffffffff);
	}

	return (acct->buddy_pref->privacy_mode);
}

static int aim_send_msg_auto(struct pork_acct *acct, char *dest, char *msg) {
	struct aim_priv *priv = acct->data;
	char *msg_html = text_to_html(msg);

	return (aim_im_sendch1(&priv->aim_session, dest, 1, msg_html));
}

static int aim_send_msg(struct pork_acct *acct, char *dest, char *msg) {
	struct aim_priv *priv = acct->data;
	char *msg_html = text_to_html(msg);

	return (aim_im_sendch1(&priv->aim_session, dest, 0, msg_html));
}

static int aim_action(struct pork_acct *acct, char *dest, char *msg) {
	char buf[8192];
	int ret;

	if (!strncasecmp(msg, "<html>", 6))
		ret = snprintf(buf, sizeof(buf), "<HTML>/me %s", &msg[6]);
	else
		ret = snprintf(buf, sizeof(buf), "/me %s", msg);

	if (ret < 0 || (size_t) ret >= sizeof(buf)) {
		debug("snprintf failed");
		return (-1);
	}

	return (aim_send_msg(acct, dest, buf));
}







static int aim_warn(struct pork_acct *acct, char *target) {
	struct aim_priv *priv = acct->data;

	return (aim_im_warn(&priv->aim_session, priv->bos_conn, target, 0));
}

static int aim_warn_anon(struct pork_acct *acct, char *target) {
	struct aim_priv *priv = acct->data;
	int ret;

	ret = aim_im_warn(&priv->aim_session, priv->bos_conn,
			target, AIM_WARN_ANON);

	return (ret);
}

static int aim_set_back(struct pork_acct *acct) {
	struct aim_priv *priv = acct->data;
	int ret;

	ret = aim_locate_setprofile(&priv->aim_session, NULL, NULL, 0, NULL, "", 0);
	return (ret);
}

static int aim_set_away(struct pork_acct *acct, char *away_msg) {
	size_t len;
	int ret;
	char *msg_html;
	struct aim_priv *priv = acct->data;

	msg_html = text_to_html(away_msg);

	len = strlen(msg_html);
	if (len > priv->rights.max_away_len) {
		screen_err_msg("%s's away message is too long. The maximum length is %u characters",
			acct->username, priv->rights.max_away_len);
		return (-1);
	}

	ret = aim_locate_setprofile(&priv->aim_session,
			NULL, NULL, 0, "us-ascii", msg_html, len);

	return (ret);
}

static int aim_set_idle(struct pork_acct *acct, u_int32_t idle_secs) {
	struct aim_priv *priv = acct->data;
	int ret;

	ret = aim_srv_setidle(&priv->aim_session, idle_secs);
	if (ret >= 0) {
		if (idle_secs > 0)
			acct->marked_idle = 1;
		else
			acct->marked_idle = 0;
	}

	return (ret);
}

static int aim_set_profile(struct pork_acct *acct, char *profile) {
	int ret;
	size_t len;
	char *profile_html;
	struct aim_priv *priv = acct->data;

	if (profile == NULL) {
		ret = aim_locate_setprofile(&priv->aim_session,
				NULL, "", 0, NULL, NULL, 0);

		return (ret);
	}

	profile_html = text_to_html(profile);

	len = strlen(profile_html);
	if (len > priv->rights.max_profile_len) {
		screen_err_msg("%s's profile is too long. The maximum length is %u characters",
			acct->username, priv->rights.max_profile_len);
		return (-1);
	}

	ret = aim_locate_setprofile(&priv->aim_session, "us-ascii",
			profile_html, len, NULL, NULL, 0);

	return (ret);
}

static int aim_get_away_msg(struct pork_acct *acct, char *buddy) {
	struct aim_priv *priv = acct->data;

	return (aim_locate_getinfoshort(&priv->aim_session, buddy, 0x00000002));
}

static int aim_whois(struct pork_acct *acct, char *buddy) {
	struct aim_priv *priv = acct->data;

	return (aim_locate_getinfoshort(&priv->aim_session, buddy, 0x00000003));
}

int aim_chat_print_users(	struct pork_acct *acct __notused,
							struct chatroom *chat)
{
	int ret;
	dlist_t *cur;
	char buf[2048];
	size_t i = 0;
	size_t len = sizeof(buf);
	struct chat_user *chat_user;
	struct aim_chat *a_chat;
	struct imwindow *win = chat->win;

	a_chat = chat->data;
	cur = chat->user_list;
	if (cur == NULL) {
		screen_win_msg(win, 0, 0, 1, MSG_TYPE_CHAT_STATUS,
			"%%D--%%m--%%M--%%x No %%Cu%%csers%%x in %%C%s%%x (%%W%s%%x)",
			chat->title_quoted, chat->title_full_quoted);

		return (0);
	}

	screen_win_msg(win, 0, 0, 1, MSG_TYPE_CHAT_STATUS,
		"%%D--%%m--%%M--%%x %u %%Cu%%csers%%x in %%C%s%%x (%%W%s%%x)",
		chat->num_users, chat->title_quoted, chat->title_full_quoted);

	while (cur != NULL) {
		chat_user = cur->data;

		if (chat_user->ignore)
			ret = snprintf(&buf[i], len, "[%%R%s%%x] ", chat_user->name);
		else
			ret = snprintf(&buf[i], len, "[%%B%s%%x] ", chat_user->name);

		if (ret < 0 || (size_t) ret >= len) {
			screen_err_msg("The results were too long to display");
			return (0);
		}

		len -= ret;
		i += ret;

		cur = cur->next;
	}

	if (i > 0 && buf[i - 1] == ' ')
		buf[--i] = '\0';

	screen_win_msg(win, 0, 0, 1, MSG_TYPE_CHAT_STATUS, "%s", buf);
	return (0);
}

int aim_chat_free(struct pork_acct *acct, void *data) {
	struct aim_chat *a_chat = data;

	if (a_chat->conn != NULL) {
		struct aim_priv *priv = acct->data;

		pork_io_del(a_chat->conn);
		aim_conn_kill(&priv->aim_session, &a_chat->conn);
	}

	free(a_chat->fullname);
	free(a_chat->fullname_quoted);
	free(a_chat->title);
	free(a_chat);

	return (0);
}

int aim_report_idle(struct pork_acct *acct, int mode) {
	struct aim_priv *priv = acct->data;

	u_int32_t report_idle = aim_ssi_getpresence(priv->aim_session.ssi.local);
	int ret;

	if (mode != 0)
		report_idle |= 0x0000400;
	else
		report_idle &= ~0x0000400;

	acct->report_idle = ((report_idle & 0x0000400) != 0);
	ret = aim_ssi_setpresence(&priv->aim_session, report_idle);
	return (ret);
}

int aim_connect_abort(struct pork_acct *acct) {
	aim_kill_all_conn(acct);
	aim_setup(acct);

	return (0);
}

int aim_proto_init(struct pork_proto *proto) {
	proto->buddy_add = aim_buddy_add;
	proto->buddy_alias = aim_buddy_alias;
	proto->buddy_block = aim_add_block;
	proto->buddy_permit = aim_add_permit;
	proto->buddy_remove = aim_buddy_remove;
	proto->buddy_remove_permit = aim_remove_permit;
	proto->buddy_unblock = aim_unblock;
	proto->buddy_update = aim_update_buddy;

	proto->chat_free = aim_chat_free;
	proto->chat_find = aim_find_chat_name_data;
	proto->chat_action = aim_chat_action;
	proto->chat_invite = aim_chat_send_invite;
	proto->chat_join = aim_join_chatroom;
	proto->chat_leave = aim_leave_chatroom;
	proto->chat_name = aim_chat_get_name;
	proto->chat_send = aim_chat_send;
	proto->chat_users = aim_chat_print_users;
	proto->chat_who = aim_chat_print_users;
	proto->chat_kick = aim_chat_kick;
	proto->chat_ban = aim_chat_banban;

	proto->who = aim_search;
	proto->connect = aim_connect;
	proto->connect_abort = aim_connect_abort;
	proto->reconnect = aim_connect;
	proto->free = aim_acct_free;
	proto->get_away_msg = aim_get_away_msg;
	proto->get_profile = aim_whois;
	proto->init = aim_acct_init;
	proto->keepalive = aim_keepalive;
	proto->filter_text = strip_html;
	proto->filter_text_out = aim_filter_text_out;
	proto->normalize = normalize;
	proto->user_compare = aim_sncmp;
	proto->read_config = aim_read_config;
	proto->send_msg = aim_send_msg;
	proto->send_msg_auto = aim_send_msg_auto;
	proto->set_away = aim_set_away;
	proto->set_back = aim_set_back;
	proto->set_idle_time = aim_set_idle;
	proto->set_privacy_mode = aim_set_privacy_mode;
	proto->set_profile = aim_set_profile;
	proto->set_report_idle = aim_report_idle;
	proto->update = aim_acct_update;
	proto->warn = aim_warn;
	proto->send_action = aim_action;
	proto->warn_anon = aim_warn_anon;
	proto->write_config = aim_write_config;

	proto->file_send = aim_file_send;
	proto->file_send_data = aim_file_send_data;
	proto->file_accept = aim_file_accept;
	proto->file_abort = aim_file_abort;
	proto->file_recv_data = aim_file_recv_data;
	proto->file_recv_complete = aim_file_recv_complete;
	proto->file_send_complete = aim_file_send_complete;

	return (0);
}
