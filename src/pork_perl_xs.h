/*
** pork_perl_xs.h - Perl scripting support
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __PORK_PERL_XS_H
#define __PORK_PERL_XS_H

XS(PORK_alias);
XS(PORK_alias_get);
XS(PORK_bind);
XS(PORK_bind_get);
XS(PORK_blist_bind);
XS(PORK_blist_bind_get);
XS(PORK_blist_unbind);
XS(PORK_connect);
XS(PORK_disconnect);
XS(PORK_echo);
XS(PORK_err_msg);
XS(PORK_get_cur_user);
XS(PORK_get_opt);
XS(PORK_load);
XS(PORK_load_perl);
XS(PORK_prompt_user);
XS(PORK_quit);
XS(PORK_quote);
XS(PORK_refresh);
XS(PORK_run_cmd);
XS(PORK_cur_user);
XS(PORK_save);
XS(PORK_set_opt);
XS(PORK_status_msg);
XS(PORK_unalias);
XS(PORK_unbind);

XS(PORK_win_find_target);
XS(PORK_win_find_name);
XS(PORK_win_bind);
XS(PORK_win_clear);
XS(PORK_win_close);
XS(PORK_win_erase);
XS(PORK_win_next);
XS(PORK_win_prev);
XS(PORK_win_rename);
XS(PORK_win_renumber);
XS(PORK_win_get_opt);
XS(PORK_win_set_opt);
XS(PORK_win_swap);
XS(PORK_win_target);

XS(PORK_get_acct_list);
XS(PORK_set_profile);
XS(PORK_send_profile);
XS(PORK_get_profile);
XS(PORK_send_msg);
XS(PORK_send_msg_auto);
XS(PORK_get_buddy_profile);
XS(PORK_get_buddy_away);
XS(PORK_privacy_mode);
XS(PORK_report_idle);
XS(PORK_search);
XS(PORK_set_away);
XS(PORK_set_idle);
XS(PORK_warn);

XS(PORK_buddy_add);
XS(PORK_buddy_add_block);
XS(PORK_buddy_add_group);
XS(PORK_buddy_add_permit);
XS(PORK_buddy_alias);
XS(PORK_buddy_clear_block);
XS(PORK_buddy_clear_permit);
XS(PORK_buddy_get_alias);
XS(PORK_buddy_get_block);
XS(PORK_buddy_get_groups);
XS(PORK_buddy_get_group_members);
XS(PORK_buddy_get_permit);
XS(PORK_buddy_remove);
XS(PORK_buddy_remove_block);
XS(PORK_buddy_remove_group);
XS(PORK_buddy_remove_permit);

XS(PORK_scroll_by);
XS(PORK_scroll_down);
XS(PORK_scroll_end);
XS(PORK_scroll_page_down);
XS(PORK_scroll_page_up);
XS(PORK_scroll_start);
XS(PORK_scroll_up);

XS(PORK_timer_add);
XS(PORK_timer_del);
XS(PORK_timer_del_refnum);
XS(PORK_timer_purge);

XS(PORK_blist_collapse);
XS(PORK_blist_cursor);
XS(PORK_blist_down);
XS(PORK_blist_end);
XS(PORK_blist_hide);
XS(PORK_blist_page_down);
XS(PORK_blist_page_up);
XS(PORK_blist_refresh);
XS(PORK_blist_select);
XS(PORK_blist_show);
XS(PORK_blist_start);
XS(PORK_blist_up);
XS(PORK_blist_width);

XS(PORK_event_add);
XS(PORK_event_del);
XS(PORK_event_del_refnum);
XS(PORK_event_del_type);
XS(PORK_event_purge);

XS(PORK_chat_ban);
XS(PORK_chat_get_list);
XS(PORK_chat_get_users);
XS(PORK_chat_get_window);
XS(PORK_chat_ignore);
XS(PORK_chat_invite);
XS(PORK_chat_join);
XS(PORK_chat_kick);
XS(PORK_chat_leave);
XS(PORK_chat_send);
XS(PORK_chat_target);
XS(PORK_chat_topic);
XS(PORK_chat_unignore);

XS(PORK_input_send);
XS(PORK_input_get_data);

#endif
