
/*
** pork_aim_proto.h - AIM OSCAR protocol interface to pork.
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

#ifndef __PORK_AIM_PROTO_H
#define __PORK_AIM_PROTO_H

int aim_proto_init(struct pork_proto *proto);
int aim_chat_free(struct pork_acct *acct, void *data);
int aim_connect_abort(struct pork_acct *acct);
int aim_report_idle(struct pork_acct *acct, int mode);
int aim_login(struct pork_acct *acct, char *s);

int aim_chat_print_users(	struct pork_acct *acct __notused,
							struct chatroom *chat);

void parse_ccom(char *cmd, struct chatroom *room, struct pork_acct *acct, int priv);
int check_if_aq(char *sn, struct chatroom *ccon);
int check_if_aw(char *sn, struct chatroom *ccon);
int check_if_ab(char *sn, struct chatroom *ccon);
int check_if_fullop(char *sn, struct chatroom *ccon);
int check_if_op(char *sn, struct chatroom *ccon);
int check_if_halfop(char *sn, struct chatroom *ccon);
int check_if_imm(char *sn, struct chatroom *ccon);

void check_ab_queues(void);
int aim_chat_rejoin(struct pork_acct *acct, struct chatroom *chat);

#endif
