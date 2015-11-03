/*
** pork_misc.h - miscellaneous functions.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __PORK_MISC_H
#define __PORK_MISC_H

#define NUSER_LEN	128

int initialize_environment(void);
void set_default_win_opts(WINDOW *win);

void resize_terminal(int rows, int cols);
int normalize(char *dest, const char *src, size_t len);
int time_to_str(u_int32_t timespec, char *buf, size_t len);
int time_to_str_full(u_int32_t timespec, char *buf, size_t len);
int date_to_str(time_t timespec, char *buf, size_t len);
int wgetinput(WINDOW *win);

#endif
