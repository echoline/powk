/*
** pork_util.h - utility functions
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __PORK_UTIL_H
#define __PORK_UTIL_H

#define array_elem(x) (sizeof((x)) / sizeof((x)[0]))

#if SIZEOF_VOID_P == 8
	#define POINTER_TO_INT(p)	((int) (long) (p))
	#define POINTER_TO_UINT(p)	((unsigned int) (unsigned long) (p))
	#define INT_TO_POINTER(i)	((void *) (long) (i))
	#define UINT_TO_POINTER(u)	((void *) (unsigned long) (u))
#else
	#define POINTER_TO_INT(p)	((int) (p))
	#define POINTER_TO_UINT(p)	((unsigned int) (p))
	#define INT_TO_POINTER(i)	((void *) (i))
	#define UINT_TO_POINTER(u)	((void *) (u))
#endif

#ifndef min
#	define min(x,y) ((x) < (y) ? (x) : (y))
#endif

#ifndef max
#	define max(x,y) ((x) > (y) ? (x) : (y))
#endif

char *xstrdup(const char *str);
void *xmalloc(size_t len);
void *xcalloc(size_t nmemb, size_t len);
char *xstrndup(const char *str, size_t len);
void *xrealloc(void *ptr, size_t size);

void strtoupper(char *s);
void free_str_wipe(char *str);

int xstrncpy(char *dest, const char *src, size_t n);
int xstrncat(char *dest, const char *src, size_t n);
char *str_from_tok(char *str, u_int32_t tok_num);
void str_trim(char *str);
char *terminate_quote(char *buf);

int expand_path(char *path, char *dest, size_t len);

int blank_str(const char *str);

u_int32_t string_hash(const char *str, u_int32_t order);
u_int32_t int_hash(int num, u_int32_t order);

inline int str_to_uint(const char *str, u_int32_t *val);
inline int str_to_int(const char *str, int *val);

int file_get_size(FILE *fp, size_t *result);

#endif
