/*
** pork_perl_stub.c - Perl scripting support (stubs)
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <config.h>

#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>

#include <pork.h>
#include <pork_missing.h>
#include <pork_util.h>
#include <pork_perl.h>

/*
** Stubs for when Perl support isn't enabled.
*/

int perl_init(void) {
	return (0);
}

int perl_destroy(void) {
	return (0);
}

int execute_perl(char *function __notused, char **args __notused) {
	return (0);
}

int execute_perl_va(char *function __notused,
					const char *fmt __notused,
					va_list ap __notused)
{
	return (0);
}

int perl_load_file(char *filename __notused) {
	return (-1);
}
