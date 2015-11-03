/*
** pork_format.h - parsing format strings.
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __PORK_FORMAT_H
#define __PORK_FORMAT_H

#define FORMAT_VARIABLE '$'

int fill_format_str(int type, char *buf, size_t len, ...);
void format_apply_justification(char *buf, chtype *ch, size_t len);

#endif
