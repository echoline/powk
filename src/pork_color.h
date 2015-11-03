/*
** pork_color.h - color functions.
** Copyright (C) 2002-2004 Amber Adams <amber@ojnk.net>
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __PORK_COLOR_H
#define __PORK_COLOR_H

void color_init(void);
int color_parse_code(const char *code, attr_t *attr);
int color_get_str(attr_t attr, char *buf, size_t len);
char *color_quote_codes(const char *str);

#endif
