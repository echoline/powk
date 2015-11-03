/*
** pork_buddy_list.h - Buddy list screen widget.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __PORK_BUDDY_LIST_H
#define __PORK_BUDDY_LIST_H

#define DEFAULT_BLIST_WIDTH		28

struct pork_acct;
struct buddy;
struct bgroup;

#include <pork_slist.h>

struct blist {
	struct slist slist;
	size_t label_len;
};

int blist_init(struct pork_acct *acct);
void blist_update_label(struct blist *blist, dlist_t *node);
dlist_t *blist_add_group(struct blist *blist, struct bgroup *group);
dlist_t *blist_add(struct blist *blist, struct buddy *buddy);
void blist_del(struct blist *blist, struct buddy *buddy);
void blist_del_group(struct blist *blist, struct bgroup *group);
void blist_collapse_group(struct blist *blist, struct bgroup *group);

int blist_cursor_up(struct blist *blist);
int blist_cursor_down(struct blist *blist);
int blist_cursor_start(struct blist *blist);
int blist_cursor_end(struct blist *blist);
int blist_cursor_pgdown(struct blist *blist);
int blist_cursor_pgup(struct blist *blist);

inline struct slist_cell *blist_get_cursor(struct blist *blist);
inline int blist_resize(	struct blist *blist,
							u_int32_t rows,
							u_int32_t cols,
							u_int32_t screen_cols);
inline void blist_destroy(struct blist *blist);
inline void blist_draw(struct blist *blist);
inline void blist_draw_cursor(struct blist *blist, int status);
inline void blist_draw_line(struct blist *blist, struct slist_cell *cell);
inline int blist_refresh(struct blist *blist);

void blist_draw_border(struct blist *blist, int border_state);
void blist_changed_format(struct blist *blist);
void blist_changed_width(struct blist *blist);

#endif
