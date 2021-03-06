/*
** pork_help.h - /help command implementation.
** Copyright (C) 2003-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __PORK_HELP_H
#define __PORK_HELP_H

#define HELP_TABSTOP			4
#define HELP_SECTION_STYLE		"%W"
#define HELP_HEADER_STYLE		"%W"
#define HELP_HEADER_STYLE_END	"%x"

int pork_help_print(char *section, char *command);
int pork_help_get_cmds(char *section, char *buf, size_t len);

#endif
