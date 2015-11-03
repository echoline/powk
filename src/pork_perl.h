/*
** pork_perl.h - Perl scripting support
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __PORK_PERL_H
#define __PORK_PERL_H

int perl_init(void);
int perl_destroy(void);
int perl_load_file(char *filename);
int execute_perl(char *function, char **args);
int execute_perl_va(char *function, const char *fmt, va_list ap);

#endif
