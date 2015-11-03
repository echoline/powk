/*
** pork_events.h - event handler functions.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __PORK_EVENTS_H
#define __PORK_EVENTS_H

enum {
	EVENT_BUDDY_AWAY,
	EVENT_BUDDY_BACK,
	EVENT_BUDDY_IDLE,
	EVENT_BUDDY_SIGNOFF,
	EVENT_BUDDY_SIGNON,
	EVENT_BUDDY_UNIDLE,
	EVENT_QUIT,
	EVENT_RECV_ACTION,
	EVENT_RECV_AWAYMSG,
	EVENT_RECV_CHAT_ACTION,
	EVENT_RECV_CHAT_INVITE,
	EVENT_RECV_CHAT_JOIN,
	EVENT_RECV_CHAT_KICK,
	EVENT_RECV_CHAT_LEAVE,
	EVENT_RECV_CHAT_MSG,
	EVENT_RECV_CHAT_MODE,
	EVENT_RECV_CHAT_NOTICE,
	EVENT_RECV_CHAT_QUIT,
	EVENT_RECV_CHAT_TOPIC,
	EVENT_RECV_IM,
	EVENT_RECV_NOTICE,
	EVENT_RECV_PROFILE,
	EVENT_RECV_RAW,
	EVENT_RECV_SEARCH_RESULT,
	EVENT_RECV_WARN,
	EVENT_SEND_ACTION,
	EVENT_SEND_AWAY,
	EVENT_SEND_CHAT_ACTION,
	EVENT_SEND_CHAT_INVITE,
	EVENT_SEND_CHAT_JOIN,
	EVENT_SEND_CHAT_KICK,
	EVENT_SEND_CHAT_LEAVE,
	EVENT_SEND_CHAT_MSG,
	EVENT_SEND_CHAT_NOTICE,
	EVENT_SEND_CHAT_TOPIC,
	EVENT_SEND_IDLE,
	EVENT_SEND_IM,
	EVENT_SEND_LINE,
	EVENT_SEND_NOTICE,
	EVENT_SEND_PROFILE,
	EVENT_SEND_WARN,
	EVENT_SIGNOFF,
	EVENT_SIGNON,
	EVENT_UNLOAD,
	MAX_EVENT_TYPE
};

struct event_info {
	char *name;
	char *fmt;
	int inside;
};

struct event {
	dlist_t *e[MAX_EVENT_TYPE];
};

struct event_entry {
	char *command;
	u_int32_t refnum;
	u_int32_t quiet:1;
	u_int32_t has_run:1;
};

int event_add(	struct event *events,
				const char *event_type,
				const char *args,
				u_int32_t *refnum);

void event_list(struct event *events, const char *event_type);
int event_generate(struct event *events, u_int32_t event_num, ...);
int event_del_refnum(struct event *events, u_int32_t refnum);
int event_del_type(struct event *events, const char *type, const char *func);
void event_init(struct event *events);
void event_destroy(struct event *events);
void event_purge(struct event *events);

#endif
