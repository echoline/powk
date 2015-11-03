/*
** pork.h - pork main include file.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __PORK_H
#define __PORK_H

#ifdef HAVE___ATTRIBUTE__
	#define __used		__attribute__((used))
	#define __notused	__attribute__((unused))
	#define __format(x) __attribute__((format x ))
#else
	#define __used
	#define __notused
	#define __format(x)
#endif

#ifdef ENABLE_DEBUGGING
#	define debug(format, args...) do { fprintf(stderr, "[%s:%u:%s] DEBUG: " format "\n", __FILE__, __LINE__, __FUNCTION__, ##args); } while (0)
#else
#	define debug(format, args...) do { } while (0)
#endif

void pork_exit(int status, char *msg, char *fmt, ...) __format((printf, 3, 4));
void keyboard_input(int fd, u_int32_t condition, void *data);

#endif
