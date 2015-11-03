/*
** pork_opt.h - Command-line argument handler.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __PORK_OPT_H
#define __PORK_OPT_H

int get_options(int argc, char *const argv[]);
int opt_is_set(u_int32_t opt);

#define USE_ADDR	(1 << 1)
#define USE_PORT	(1 << 2)

#endif

