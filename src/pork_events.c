/*
** pork_events.c - event handler functions.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <config.h>

#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>
#include <string.h>
#include <stdarg.h>

#include <pork.h>
#include <pork_missing.h>
#include <pork_util.h>
#include <pork_list.h>
#include <pork_set.h>
#include <pork_screen_io.h>
#include <pork_perl.h>
#include <pork_command.h>
#include <pork_events.h>
#include <pork_imsg.h>
#include <pork_screen.h>

static u_int32_t next_refnum;

static struct event_info events_info[] = {
	{ "BUDDY_AWAY",				"Si",		0	},
	{ "BUDDY_BACK",				"Si",		0	},
	{ "BUDDY_IDLE",				"Sii",		0	},
	{ "BUDDY_SIGNOFF",			"Si",		0	},
	{ "BUDDY_SIGNON",			"Si",		0	},
	{ "BUDDY_UNIDLE",			"Si",		0	},
	{ "QUIT",					"S",		0	},
	{ "RECV_ACTION",			"SSSsi",	0	},
	{ "RECV_AWAYMSG",			"Siiiisi",	0	},
	{ "RECV_CHAT_ACTION",		"SSSSsi",	0	},
	{ "RECV_CHAT_INVITE",		"SSSsi",	0	},
	{ "RECV_CHAT_JOIN",			"SSSi",		0	},
	{ "RECV_CHAT_KICK",			"SSSsi",	0	},
	{ "RECV_CHAT_LEAVE",		"SSi",		0	},
	{ "RECV_CHAT_MSG",			"SSSSsi",	0	},
	{ "RECV_CHAT_MODE",			"SSsi",		0	},
	{ "RECV_CHAT_NOTICE",		"SSSSsi",	0	},
	{ "RECV_CHAT_QUIT",			"SSsi",		0	},
	{ "RECV_CHAT_TOPIC",		"SSsi",		0	},
	{ "RECV_IM",				"SSSisi",	0	},
	{ "RECV_NOTICE",			"SSSsi",	0	},
	{ "RECV_PROFILE",			"Siiiisi",	0	},
	{ "RECV_RAW",				"SSi",		0	},
	{ "RECV_SEARCH_RESULT",		"SSi",		0	},
	{ "RECV_WARN",				"Sii",		0	},
	{ "SEND_ACTION",			"Ssi",		0	},
	{ "SEND_AWAY",				"si",		0	},
	{ "SEND_CHAT_ACTION",		"Ssi",		0	},
	{ "SEND_CHAT_INVITE",		"SSsi",		0	},
	{ "SEND_CHAT_JOIN",			"Si",		0	},
	{ "SEND_CHAT_KICK",			"SSsi",		0	},
	{ "SEND_CHAT_LEAVE",		"Si",		0	},
	{ "SEND_CHAT_MSG",			"Ssi",		0	},
	{ "SEND_CHAT_NOTICE",		"Ssi",		0	},
	{ "SEND_CHAT_TOPIC",		"Ssi",		0	},
	{ "SEND_IDLE",				"ii",		0	},
	{ "SEND_IM",				"Ssi",		0	},
	{ "SEND_NOTICE",			"Ssi",		0	},
	{ "SEND_LINE",				"si",		0	},
	{ "SEND_PROFILE",			"si",		0	},
	{ "SEND_WARN",				"Sii",		0	},
	{ "SIGNOFF",				"i",		0	},
	{ "SIGNON",					"i",		0	},
	{ "UNLOAD",					"",			0	},
};

static int event_compare(const void *l, const void *r) {
	const char *key = (char *) l;
	struct event_info *info = (struct event_info *) r;

	return (strcasecmp(key, info->name));
}

static int event_find_refnum_cb(void *l, void *r) {
	u_int32_t refnum = POINTER_TO_UINT(l);
	struct event_entry *event = (struct event_entry *) r;

	return (refnum - event->refnum);
}

static int event_find(const char *name) {
	struct event_info *info;
	u_int32_t offset;

	info = bsearch(name, events_info, array_elem(events_info),
			sizeof(struct event_info), event_compare);

	if (info == NULL)
		return (-1);

	offset = (long) info - (long) &events_info[0];
	return (offset / sizeof(struct event_info));
}

void event_init(struct event *events) {
	memset(events, 0, sizeof(*events));
}

static void event_destroy_cb(void *param __notused, void *data) {
	struct event_entry *event = (struct event_entry *) data;

	free(event->command);
}

inline void event_purge(struct event *events) {
	event_destroy(events);
	event_init(events);
}

void event_destroy(struct event *events) {
	u_int32_t i;

	for (i = 0 ; i < MAX_EVENT_TYPE ; i++) {
		dlist_destroy(events->e[i], NULL, event_destroy_cb);
		events->e[i] = NULL;
	}
}

static void event_print_type(struct event *events, u_int32_t type) {
	dlist_t *cur = events->e[type];

	if (cur == NULL) {
		screen_win_msg(cur_window(), 0, 1, 1, MSG_TYPE_CMD_OUTPUT,
			"No %s events", events_info[type].name);
		return;
	}

	do {
		struct event_entry *event = cur->data;

		screen_win_msg(cur_window(), 0, 1, 1, MSG_TYPE_CMD_OUTPUT,
			"[refnum %u] EVENT %s: %s",
			event->refnum,
			events_info[type].name,
 			event->command);

		cur = cur->next;
	} while (cur != NULL);
}

void event_list(struct event *events, const char *event_type) {
	u_int32_t i;

	if (event_type != NULL) {
		int event;

		event = event_find(event_type);
		if (event == -1) {
			screen_err_msg("No such event: %s", event_type);
			return;
		}

		event_print_type(events, event);
		return;
	}

	for (i = 0 ; i < MAX_EVENT_TYPE ; i++)
		event_print_type(events, i);
}

int event_add(	struct event *events,
				const char *event_type,
				const char *args,
				u_int32_t *event_refnum)
{
	int event;
	int quiet = 0;
	struct event_entry *new_event;

	if (*event_type == '^') {
		quiet = 1;
		event_type++;
	}

	event = event_find(event_type);
	if (event == -1)
		return (-1);

	new_event = xcalloc(1, sizeof(*new_event));
	new_event->command = xstrdup(args);
	new_event->quiet = quiet;
	new_event->refnum = next_refnum++;

	events->e[event] = dlist_add_tail(events->e[event], new_event);

	*event_refnum = new_event->refnum;
	return (0);
}

int event_del_refnum(struct event *events, u_int32_t refnum) {
	u_int32_t i;

	for (i = 0 ; i < MAX_EVENT_TYPE ; i++) {
		dlist_t *cur;

		cur = dlist_find(events->e[i], UINT_TO_POINTER(refnum),
				event_find_refnum_cb);

		if (cur != NULL) {
			struct event_entry *event = cur->data;

			events->e[i] = dlist_remove(events->e[i], cur);
			free(event->command);
			free(event);

			return (0);
		}
	}

	return (-1);
}

int event_del_type(struct event *events, const char *type, const char *func) {
	int event = event_find(type);
	dlist_t *cur;

	if (event == -1)
		return (-1);

	if (func == NULL) {
		dlist_destroy(events->e[event], NULL, event_destroy_cb);
		events->e[event] = NULL;

		return (0);
	}

	cur = events->e[event];
	while (cur != NULL) {
		struct event_entry *entry = cur->data;

		if (!strcasecmp(entry->command, func)) {
			events->e[event] = dlist_remove(events->e[event], cur);
			free(entry->command);
			free(entry);

			return (0);
		}
	}

	return (-1);
}

static int event_should_generate(u_int32_t event_num) {
	int recursive_events = opt_get_bool(OPT_RECURSIVE_EVENTS);

	if (events_info[event_num].inside > 0 && !recursive_events)
		return (0);

	return (1);
}

int event_generate(struct event *events, u_int32_t event_num, ...) {
	dlist_t *cur;

	/*
	** Event handlers can delete and add other event
	** handlers, so after one handler runs, we have no idea
	** what the list of event handlers for the event type that was
	** just handled looks like. We need some way to figure out
	** which handlers in the list have and have not already run.
	** This could get out of control if the list of handlers for
	** some event type got big, but that should never happen.
	*/
	cur = events->e[event_num];
	while (cur != NULL) {
		struct event_entry *event = cur->data;

		event->has_run = 0;
		cur = cur->next;
	}

	cur = events->e[event_num];
	while (cur != NULL) {
		int ret = 0;
		va_list data;
		struct event_entry *event = cur->data;
		int old_quiet = 0;
		int quiet = event->quiet;

		if (event->has_run) {
			cur = cur->next;
			continue;
		}

		if (quiet)
			old_quiet = screen_set_quiet(1);

		va_start(data, event_num);

		event->has_run = 1;
		if (event_should_generate(event_num)) {
			events_info[event_num].inside++;

			ret = execute_perl_va(event->command,
					events_info[event_num].fmt, data);

			events_info[event_num].inside--;
		}

		va_end(data);

		if (quiet)
			screen_set_quiet(old_quiet);

		if (ret != 0)
			return (ret);

		/*
		** The event handler that just ran could have removed
		** any other event handlers for this type. It could
		** have added new handlers to the head of the list. We start over.
		*/
		cur = events->e[event_num];
	}

	return (0);
}
