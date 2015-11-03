/*
** pork_slist.c - an ncurses scrolling list widget
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This widget is more akin to what GUI development toolkits call
** 'trees' than to a list, I think.
**
** This could be improved and made more useful for other purposes
** (it's fine for what this program needs right now) by supporting
** "hidden" nodes, so that nodes don't have to be deleted from the list
** in order to not be displayed.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <config.h>

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <sys/types.h>

#include <pork.h>
#include <pork_missing.h>
#include <pork_util.h>
#include <pork_list.h>
#include <pork_cstr.h>
#include <pork_misc.h>
#include <pork_slist.h>

static int slist_find_cb(void *l, void *r) {
	return (l != r);
}

static void slist_destroy_cleanup(void *param, void *data) {
	struct slist *slist = param;
	struct slist_cell *cell = data;

	dlist_destroy(cell->children, slist, slist_destroy_cleanup);
	free(cell->label);

	if (slist->cell_free_cb != NULL)
		slist->cell_free_cb(cell);

	free(cell);
}

static dlist_t *slist_node_prev(dlist_t *node) {
	struct slist_cell *cell;

	if (node->prev != NULL) {
		struct slist_cell *prev_cell = node->prev->data;
		dlist_t *cur = prev_cell->children;

		/*
		** The the node immediately before a node that follows
		** a node with children is the last of those children.
		*/

		while (cur != NULL) {
			dlist_t *next = cur->next;

			if (next == NULL)
				return (cur);

			cur = next;
		}

		return (node->prev);
	}

	cell = node->data;
	return (cell->parent);
}

dlist_t *slist_node_next(dlist_t *node) {
	struct slist_cell *cell = node->data;

	if (cell->children != NULL)
		return (cell->children);

	if (node->next != NULL)
		return (node->next);

	if (cell->parent != NULL)
		return (cell->parent->next);

	return (NULL);
}

static void slist_scroll_screen_up(struct slist *slist) {
	struct slist_cell *cell;

	scrollok(slist->win, TRUE);
	wscrl(slist->win, -1);
	scrollok(slist->win, FALSE);

	if (slist->has_border) {
		cell = slist->bot->data;
		if (cell->line != SLIST_LAST_ROW(slist)) {
			wmove(slist->win, SLIST_LAST_ROW(slist) + slist->has_border, 0);
			wclrtoeol(slist->win);
		}
	}

	cell = slist->top->data;

	wmove(slist->win, cell->line, 1);
	wclrtoeol(slist->win);
	mvwputstr(slist->win, cell->line, SLIST_FIRST_COL(slist), cell->label);
	slist->dirty = 1;
}

static void slist_scroll_screen_down(struct slist *slist) {
	struct slist_cell *cell;

	scrollok(slist->win, TRUE);
	wscrl(slist->win, 1);
	scrollok(slist->win, FALSE);

	cell = slist->bot->data;

	wmove(slist->win, cell->line, 1);
	wclrtoeol(slist->win);
	mvwputstr(slist->win, cell->line, SLIST_FIRST_COL(slist), cell->label);
	slist->dirty = 1;
}

int slist_init(	struct slist *slist,
				u_int32_t rows,
				u_int32_t cols,
				u_int32_t xpos,
				u_int32_t ypos)
{
	WINDOW *win;

	win = newwin(rows, cols, ypos, xpos);
	if (win == NULL)
		return (-1);
	set_default_win_opts(win);

	memset(slist, 0, sizeof(*slist));

	slist->win = win;
	slist->rows = rows;
	slist->cols = cols;
	slist->has_border = 1;

	return (0);
}

void slist_cell_free_cb(struct slist *slist, void (*cell_free_cb)(void *)) {
	slist->cell_free_cb = cell_free_cb;
}

void slist_destroy(struct slist *slist) {
	dlist_destroy(slist->cells, slist, slist_destroy_cleanup);
	delwin(slist->win);
	memset(slist, 0, sizeof(*slist));
}

int slist_renumber(struct slist *slist, dlist_t *start_cell, int cur_line) {
	dlist_t *cur = start_cell;

	if (cur == NULL)
		return (-1);

	while (1) {
		struct slist_cell *cell = cur->data;
		dlist_t *last;

		if (cur_line > SLIST_LAST_ROW(slist)) {
			slist->bot = slist_node_prev(cur);
			cur_line = -1;
		}

		cell->line = cur_line;
		if (cur_line >= 0)
			cur_line++;

		last = cur;
		cur = slist_node_next(cur);
		if (cur == NULL) {
			if (cur_line > 0)
				slist->bot = last;
			break;
		}
	}

	return (cur_line);
}

int slist_resize(	struct slist *slist,
					u_int32_t rows,
					u_int32_t cols,
					u_int32_t screen_cols)
{
	struct slist_cell *cell;

	slist->rows = rows;
	slist->cols = cols;

	wresize(slist->win, slist->rows, slist->cols);
	mvwin(slist->win, 0, screen_cols - slist->cols);

	wmove(slist->win, 0, 0);
	wclrtobot(slist->win);

	slist_renumber(slist, slist->top, 0 + slist->has_border);
	slist_draw(slist);

	if (slist->cursor != NULL) {
		cell = slist->cursor->data;

		if (cell->line == -1)
			slist->cursor = slist->bot;
		else if (cell->line == -2)
			slist->cursor = slist->top;
	} else {
		/*
		** This should only happen if the list is
		** completely empty.
		*/
		slist->cursor = slist->top;
	}

	slist->dirty = 1;
	return (0);
}

int slist_cursor_up(struct slist *slist) {
	dlist_t *cursor;
	struct slist_cell *cell;

	if (slist->cursor == NULL)
		return (-1);

	cursor = slist_node_prev(slist->cursor);
	if (cursor == NULL)
		return (-1);

	slist->cursor = cursor;

	cell = cursor->data;
	if (cell->line < 0) {
		struct slist_cell *bot_cell = slist->bot->data;

		slist->top = slist_node_prev(slist->top);
		if (bot_cell->line == SLIST_LAST_ROW(slist))
			bot_cell->line = -1;

		slist_renumber(slist, slist->top, 0 + slist->has_border);
		slist_scroll_screen_up(slist);
	}

	slist->dirty = 1;
	return (0);
}

int slist_cursor_down(struct slist *slist) {
	dlist_t *cursor;
	struct slist_cell *cell;

	if (slist->cursor == NULL)
		return (-1);

	cursor = slist_node_next(slist->cursor);
	if (cursor == NULL)
		return (-1);

	cell = cursor->data;

	/*
	** The top line scrolls off the top.
	*/

	if (cell->line < 0) {
		struct slist_cell *top_cell = slist->top->data;

		top_cell->line = -2;
		slist->top = slist_node_next(slist->top);
		slist_renumber(slist, slist->top, 0 + slist->has_border);
		slist_scroll_screen_down(slist);
	}

	slist->cursor = cursor;
	slist->dirty = 1;
	return (0);
}

int slist_cursor_start(struct slist *slist) {
	int ret;
	int scrolled = 0;

	if (slist == NULL)
		return (-1);

	while ((ret = slist_cursor_up(slist)) == 0)
		scrolled++;

	return (scrolled);
}

int slist_cursor_end(struct slist *slist) {
	int ret;
	int scrolled = 0;

	if (slist == NULL)
		return (-1);

	while ((ret = slist_cursor_down(slist)) == 0)
		scrolled++;

	return (scrolled);
}

int slist_cursor_pgdown(struct slist *slist) {
	int ret;
	int lines = slist->rows - (slist->has_border * 2);
	int scrolled = 0;

	if (slist == NULL)
		return (-1);

	while (lines-- > 0 && (ret = slist_cursor_down(slist)) == 0)
		scrolled++;

	return (scrolled);
}

int slist_cursor_pgup(struct slist *slist) {
	int ret;
	int lines = slist->rows - (slist->has_border * 2);
	int scrolled = 0;

	if (slist == NULL)
		return (-1);

	while (lines-- > 0 && (ret = slist_cursor_up(slist)) == 0)
		scrolled++;

	return (scrolled);
}

dlist_t *slist_find(dlist_t *start, void *data) {
	dlist_t *cur = start;

	while (cur != NULL) {
		if (cur->data == data)
			return (cur);

		cur = slist_node_next(cur);
	}

	return (NULL);
}

inline struct slist_cell *slist_get_cursor(struct slist *slist) {
	if (slist->cursor == NULL)
		return (NULL);

	return (slist->cursor->data);
}

dlist_t *slist_add(struct slist *slist, struct slist_cell *cell) {
	int next_line;
	dlist_t *cur;
	dlist_t **head;
	dlist_t **start_renumber;

	if (cell == NULL)
		return (NULL);

	if (cell->parent == NULL) {
		head = &slist->cells;
		next_line = SLIST_FIRST_ROW(slist);
		start_renumber = head;
	} else {
		struct slist_cell *parent_cell = cell->parent->data;

		if (parent_cell->type != TYPE_LIST_CELL)
			return (NULL);

		if (parent_cell->collapsed)
			return (NULL);

		head = &parent_cell->children;
		next_line = parent_cell->line;
		if (parent_cell->line >= 0)
			next_line++;
		start_renumber = head;
	}

	cur = *head;
	if (cur == NULL) {
		dlist_t *old_head = *head;

		cell->line = next_line;
		*head = dlist_add_head(*head, cell);

		if (slist->top == old_head) {
			slist->top = *head;
			start_renumber = head;
		}
	} else {
		struct slist_cell *last_cell = cur->data;
		dlist_t *old_head = *head;

		if (last_cell->refnum > cell->refnum) {
			cell->line = last_cell->line;

			*head = dlist_add_head(*head, cell);

			if (slist->top == old_head) {
				slist->top = *head;
				start_renumber = head;
			}
		} else {
			dlist_t *temp;
			dlist_t *child;

			do {
				struct slist_cell *cur_cell;
				dlist_t *next = cur->next;

				if (next == NULL)
					break;

				cur_cell = next->data;
				if (cur_cell->refnum > cell->refnum)
					break;

				cur = next;
			} while (1);

			*head = dlist_add_after(*head, cur, cell);
			temp = cur->next;
			start_renumber = &temp;

			last_cell = cur->data;

			/*
			** We're adding this new node, call it B, after an old
			** node, call it A. If A holds a list cell, then B's line
			** number will be one more than that of the last child of B.
			*/

			child = last_cell->children;
			if (child != NULL) {
				dlist_t *last = NULL;

				while (child != temp) {
					last = child;
					child = slist_node_next(child);
				}

				if (last != NULL)
					last_cell = last->data;
			}

			cell->line = last_cell->line;
			if (cell->line >= 0)
				cell->line++;
		}
	}

	if (cell->line > SLIST_LAST_ROW(slist))
		cell->line = -1;

	cur = *start_renumber;

	if (slist->top == NULL)
		slist->top = slist->cells;

	if (cell->line >= 0) {
		if (slist->cursor == NULL)
			slist->cursor = cur;

		slist_renumber(slist, *start_renumber, cell->line);
	}

	if (slist->cursor == NULL)
		slist->cursor = slist->top;

	/*
	** If the node to be added is off the screen at the top, scroll
	** everything up.
	*/

	if (cell->line == -2) {
		slist->top = slist_node_prev(slist->top);
		slist_renumber(slist, slist->top, 0 + slist->has_border);
		slist_scroll_screen_up(slist);
	}

	if (slist->cursor != NULL) {
		struct slist_cell *cur_cell = slist->cursor->data;

		if (cur_cell->line == -1)
			slist->cursor = slist->bot;
		else if (cur_cell->line == -2)
			slist->cursor = slist->top;
	}

	return (cur);
}

static void slist_del_cell_cb(void *param, void *data) {
	struct slist *slist = param;
	struct slist_cell *cell = data;

	free(cell->label);
	if (slist->cell_free_cb != NULL)
		slist->cell_free_cb(cell);
	free(cell);
}

void slist_del_children(struct slist *slist, dlist_t *node) {
	dlist_t *cur;
	struct slist_cell *cell;

	if (node == NULL)
		return;

	cell = node->data;
	cur = cell->children;
	while (cur != NULL) {
		struct slist_cell *cur_cell = cur->data;

		if (cur_cell->children != NULL)
			slist_del_children(slist, cur);

		if (slist->cursor == cur)
			slist->cursor = node;

		if (slist->top == cur)
			slist->top = node;

		if (slist->bot == cur)
			slist->bot = node;

		cur = cur->next;
	}

	dlist_destroy(cell->children, slist, slist_del_cell_cb);
	cell->collapsed = 1;
	cell->children = NULL;
}

int slist_collapse_list_cell(struct slist *slist, dlist_t *node) {
	struct slist_cell *cell;

	if (node == NULL || node->data == NULL)
		return (-1);
	cell = node->data;

	if (cell->type != TYPE_LIST_CELL || cell->collapsed)
		return (-1);

	slist_del_children(slist, node);
	slist_renumber(slist, slist->top, 0 + slist->has_border);
	return (0);
}

struct slist_cell *slist_del(struct slist *slist, struct slist_cell *cell) {
	dlist_t *cur;
	dlist_t **head;

	if (cell == NULL)
		return (NULL);

	if (cell->parent == NULL)
		head = &slist->cells;
	else {
		struct slist_cell *parent_cell = cell->parent->data;

		if (parent_cell->type != TYPE_LIST_CELL)
			return (NULL);

		if (parent_cell->collapsed)
			return (NULL);

		head = &parent_cell->children;
	}

	cur = dlist_find(*head, cell, slist_find_cb);
	if (cur == NULL)
		return (NULL);

	if (cell->children != NULL)
		slist_del_children(slist, cur);

	if (slist->top == cur) {
		slist->top = slist_node_next(cur);
		if (slist->top == NULL)
			slist->top = slist_node_prev(cur);
	}

	if (slist->cursor == cur) {
		slist->cursor = slist_node_next(cur);
		if (slist->cursor == NULL)
			slist->cursor = slist_node_prev(cur);
	}

	/*
	** If the node to be deleted is off the screen at the top, scroll
	** everything down.
	*/

	if (cell->line == -2) {
		dlist_t *new_top;

		new_top = slist_node_next(slist->top);
		if (new_top != NULL) {
			dlist_t *old_top = slist->top;
			struct slist_cell *top_cell = slist->top->data;

			top_cell->line = -2;

			slist->top = new_top;

			if (slist->cursor == old_top)
				slist->cursor = slist->top;

			slist_scroll_screen_down(slist);
		}
	}

	*head = dlist_remove(*head, cur);
	slist_del_cell_cb(slist, cell);

	if (slist->top != NULL)
		slist_renumber(slist, slist->top, 0 + slist->has_border);

	if (slist->cursor != NULL) {
		cell = slist->cursor->data;
		if (cell->line == -1)
			slist->cursor = slist->bot;
		else if (cell->line == -2)
			slist->cursor = slist->top;
	} else
		slist->cursor = slist->top;

	return (0);
}

void slist_draw_line(struct slist *slist, struct slist_cell *cell) {
	int ret;

	if (cell->line < 0)
		return;

	wmove(slist->win, cell->line, SLIST_FIRST_COL(slist));
	ret = mvwputnstr(slist->win, cell->line, slist->has_border, cell->label,
			SLIST_LAST_COL(slist) - SLIST_FIRST_COL(slist) + 1);

	/*
	** Pad it out with spaces.
	*/

	while (++ret <= SLIST_LAST_COL(slist))
		waddch(slist->win, ' ');

	slist->dirty = 1;
}

void slist_draw_cursor(struct slist *slist, attr_t curs_attr) {
	int i;
	struct slist_cell *cell;

	if (slist == NULL || slist->cursor == NULL || slist->cursor->data == NULL)
		return;

	cell = slist->cursor->data;
	if (cell->line < 0)
		return;

	wmove(slist->win, cell->line, SLIST_FIRST_COL(slist));

	if (curs_attr) {
		/*
		** I don't call wattron and then wputstr here because
		** ncurses' waddch fuction seems to ignore the attributes
		** that you just set with wattron if a chtype already has
		** color attributes set.
		*/

		for (i = 0 ; i <= SLIST_LAST_COL(slist) && cell->label[i] != 0 ; i++)
			waddch(slist->win, chtype_get(cell->label[i]) | curs_attr);
	} else {
		/*
		** If no attributes are specified, just use the ones in
		** the chtype already.
		*/

		for (i = 0 ; i <= SLIST_LAST_COL(slist) && cell->label[i] != 0 ; i++)
			waddch(slist->win, cell->label[i]);
	}

	/*
	** Pad it out with spaces.
	*/

	while (++i <= SLIST_LAST_COL(slist))
		waddch(slist->win, ' ' | curs_attr);

	slist->dirty = 1;
}

void slist_draw(struct slist *slist) {
	dlist_t *cur = slist->top;

	while (cur != NULL) {
		struct slist_cell *cell = cur->data;

		if (cell->line < 0)
			return;

		slist_draw_line(slist, cell);
		cur = slist_node_next(cur);
	}

	slist->dirty = 1;
}

void slist_clear_bot(struct slist *slist) {
	struct slist_cell *cell;

	if (slist->bot == NULL)
		return;

	cell = slist->bot->data;
	if (cell == NULL)
		return;

	if (cell->line != SLIST_LAST_ROW(slist)) {
		wmove(slist->win, cell->line + 1, 0);
		wclrtobot(slist->win);
		slist->dirty = 1;
	}
}

int slist_refresh(struct slist *slist) {
	if (!slist->dirty)
		return (0);

	wnoutrefresh(slist->win);
	slist->dirty = 0;
	return (1);
}
