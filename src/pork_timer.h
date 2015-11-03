/*
** pork_timer.h - Timer implementation
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __PORK_TIMER_H
#define __PORK_TIMER_H

struct timer_entry {
	char *command;
	u_int32_t refnum;
	time_t interval;
	time_t last_run;
	u_int32_t times;
};

int timer_run(dlist_t **timer_list);
inline void timer_destroy(dlist_t **timer_list);
int timer_del_refnum(dlist_t **timer_list, u_int32_t refnum);
int timer_del(dlist_t **timer_list, char *command);
u_int32_t timer_add(dlist_t **timer_list,
					char *command,
					time_t interval,
					u_int32_t count);

#endif
