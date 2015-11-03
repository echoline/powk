/*
** pork_input.h - line editing and history
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __PORK_INPUT_H
#define __PORK_INPUT_H

#define INPUT_BUFFER_LEN	4096
#define MAX_HISTORY_LEN		150

#define META_MASK		(0xf << 28)
#define META_NUM(x)		(((x) & META_MASK) >> 28)
#define META_KEY(x, n)	((x) | ((n) << 28))

#define CTRL_KEY(x)		((x) & 0x1f)

struct input {
	u_int32_t width;
	u_int16_t cur;
	u_int16_t len;
	u_int16_t begin_completion;
	u_int16_t max_history_len;
	u_int16_t history_len;
	u_int16_t prompt_len;
	u_int16_t dirty:1;
	chtype *prompt;
	dlist_t *history;
	dlist_t *history_cur;
	dlist_t *history_end;
	char input_buf[INPUT_BUFFER_LEN];
};

char *input_partial(struct input *input);
void input_resize(struct input *input, u_int32_t width);
void input_destroy(struct input *input);
void input_init(struct input *input, u_int32_t width);

void input_delete(struct input *input);
void input_bkspace(struct input *input);
void input_insert(struct input *input, int c);
void input_insert_str(struct input *input, char *str);
void input_clear_line(struct input *input);
void input_clear_to_end(struct input *input);
void input_clear_to_start(struct input *input);
void input_home(struct input *input);
void input_end(struct input *input);
void input_prev_word(struct input *input);
void input_next_word(struct input *input);
void input_clear_prev_word(struct input *input);
void input_clear_next_word(struct input *input);
void input_move_left(struct input *input);
void input_move_right(struct input *input);
void input_history_prune(struct input *input);
void input_history_add(struct input *input);
void input_history_next(struct input *input);
void input_history_prev(struct input *input);
void input_history_clear(struct input *input);
int input_set_buf(struct input *input, char *str);
int input_set_prompt(struct input *input, char *prompt);
char *input_get_buf_str(struct input *input);
u_int32_t input_get_cursor_pos(struct input *input);

#endif
