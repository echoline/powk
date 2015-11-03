/*
** pork_perl.c - Perl scripting support
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** The init, execute_perl, and destroy functions were pretty much copied
** from the gaim perl.c source file.
** Copyrights there apply here.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <config.h>

#include <unistd.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdarg.h>
#include <sys/types.h>
#include <stdlib.h>
#include <sys/mman.h>

#undef PACKAGE
#undef instr
#define HAS_BOOL

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#include <pork.h>
#include <pork_missing.h>
#include <pork_util.h>
#include <pork_list.h>
#include <pork_screen_io.h>
#include <pork_perl.h>
#include <pork_perl_xs.h>

#ifdef OLD_PERL
extern void boot_DynaLoader _((CV * cv));
#define init_args void
#else
extern void boot_DynaLoader(pTHX_ CV* cv);
#define init_args pTHX
#endif

static void xs_init(init_args);
static PerlInterpreter *perl_inter;

void xs_init(init_args) {
	char *file = __FILE__;

	newXS("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);

	newXS("PORK::alias", PORK_alias, file);
	newXS("PORK::alias_get", PORK_alias_get, file);
	newXS("PORK::bind", PORK_bind, file);
	newXS("PORK::bind_get", PORK_bind_get, file);
	newXS("PORK::blist_bind", PORK_blist_bind, file);
	newXS("PORK::blist_bind_get", PORK_blist_bind_get, file);
	newXS("PORK::blist_unbind", PORK_blist_unbind, file);
	newXS("PORK::connect", PORK_connect, file);
	newXS("PORK::disconnect", PORK_disconnect, file);
	newXS("PORK::echo", PORK_echo, file);
	newXS("PORK::err_msg", PORK_err_msg, file);
	newXS("PORK::get_cur_user", PORK_get_cur_user, file);
	newXS("PORK::get_opt", PORK_get_opt, file);
	newXS("PORK::load", PORK_load, file);
	newXS("PORK::load_perl", PORK_load_perl, file);
	newXS("PORK::prompt_user", PORK_prompt_user, file);
	newXS("PORK::quit", PORK_quit, file);
	newXS("PORK::quote", PORK_quote, file);
	newXS("PORK::refresh", PORK_refresh, file);
	newXS("PORK::run_cmd", PORK_run_cmd, file);
	newXS("PORK::save", PORK_save, file);
	newXS("PORK::cur_user", PORK_cur_user, file);
	newXS("PORK::set_opt", PORK_set_opt, file);
	newXS("PORK::status_msg", PORK_status_msg, file);
	newXS("PORK::unbind", PORK_unbind, file);
	newXS("PORK::unalias", PORK_unalias, file);

	newXS("PORK::win_bind", PORK_win_bind, file);
	newXS("PORK::win_clear", PORK_win_clear, file);
	newXS("PORK::win_close", PORK_win_close, file);
	newXS("PORK::win_erase", PORK_win_erase, file);
	newXS("PORK::win_find_name", PORK_win_find_name, file);
	newXS("PORK::win_find_target", PORK_win_find_target, file);
	newXS("PORK::win_get_opt", PORK_win_get_opt, file);
	newXS("PORK::win_next", PORK_win_next, file);
	newXS("PORK::win_prev", PORK_win_prev, file);
	newXS("PORK::win_rename", PORK_win_rename, file);
	newXS("PORK::win_renumber", PORK_win_renumber, file);
	newXS("PORK::win_set_opt", PORK_win_set_opt, file);
	newXS("PORK::win_swap", PORK_win_swap, file);
	newXS("PORK::win_target", PORK_win_target, file);

	newXS("PORK::buddy_add", PORK_buddy_add, file);
	newXS("PORK::buddy_add_block", PORK_buddy_add_block, file);
	newXS("PORK::buddy_add_group", PORK_buddy_add_group, file);
	newXS("PORK::buddy_add_permit", PORK_buddy_add_permit, file);
	newXS("PORK::buddy_alias", PORK_buddy_alias, file);
	newXS("PORK::buddy_clear_block", PORK_buddy_clear_block, file);
	newXS("PORK::buddy_clear_permit", PORK_buddy_clear_permit, file);
	newXS("PORK::buddy_get_alias", PORK_buddy_get_alias, file);
	newXS("PORK::buddy_get_block", PORK_buddy_get_block, file);
	newXS("PORK::buddy_get_groups", PORK_buddy_get_groups, file);
	newXS("PORK::buddy_get_group_members", PORK_buddy_get_group_members, file);
	newXS("PORK::buddy_get_permit", PORK_buddy_get_permit, file);
	newXS("PORK::buddy_remove", PORK_buddy_remove, file);
	newXS("PORK::buddy_remove_group", PORK_buddy_add_group, file);
	newXS("PORK::buddy_remove_block", PORK_buddy_remove_block, file);
	newXS("PORK::buddy_remove_permit", PORK_buddy_remove_permit, file);

	newXS("PORK::get_acct_list", PORK_get_acct_list, file);
	newXS("PORK::get_buddy_away", PORK_get_buddy_away, file);
	newXS("PORK::get_buddy_profile", PORK_get_buddy_profile, file);
	newXS("PORK::get_profile", PORK_get_profile, file);
	newXS("PORK::privacy_mode", PORK_privacy_mode, file);
	newXS("PORK::report_idle", PORK_report_idle, file);
	newXS("PORK::search", PORK_search, file);
	newXS("PORK::send_msg", PORK_send_msg, file);
	newXS("PORK::send_msg_auto", PORK_send_msg_auto, file);
	newXS("PORK::set_away", PORK_set_away, file);
	newXS("PORK::set_idle", PORK_set_idle, file);
	newXS("PORK::set_profile", PORK_set_profile, file);
	newXS("PORK::send_profile", PORK_send_profile, file);
	newXS("PORK::warn", PORK_warn, file);

	newXS("PORK::scroll_by", PORK_scroll_by, file);
	newXS("PORK::scroll_down", PORK_scroll_down, file);
	newXS("PORK::scroll_end", PORK_scroll_end, file);
	newXS("PORK::scroll_page_down", PORK_scroll_page_down, file);
	newXS("PORK::scroll_page_up", PORK_scroll_page_up, file);
	newXS("PORK::scroll_start", PORK_scroll_start, file);
	newXS("PORK::scroll_up", PORK_scroll_up, file);

	newXS("PORK::timer_add", PORK_timer_add, file);
	newXS("PORK::timer_del", PORK_timer_del, file);
	newXS("PORK::timer_del_refnum", PORK_timer_del_refnum, file);
	newXS("PORK::timer_purge", PORK_timer_purge, file);

	newXS("PORK::blist_collapse", PORK_blist_collapse, file);
	newXS("PORK::blist_cursor", PORK_blist_cursor, file);
	newXS("PORK::blist_down", PORK_blist_down, file);
	newXS("PORK::blist_end", PORK_blist_end, file);
	newXS("PORK::blist_hide", PORK_blist_hide, file);
	newXS("PORK::blist_page_down", PORK_blist_page_down, file);
	newXS("PORK::blist_page_up", PORK_blist_page_up, file);
	newXS("PORK::blist_refresh", PORK_blist_refresh, file);
	newXS("PORK::blist_show", PORK_blist_show, file);
	newXS("PORK::blist_start", PORK_blist_start, file);
	newXS("PORK::blist_up", PORK_blist_up, file);
	newXS("PORK::blist_width", PORK_blist_width, file);

	newXS("PORK::event_add", PORK_event_add, file);
	newXS("PORK::event_del", PORK_event_del, file);
	newXS("PORK::event_del_type", PORK_event_del_type, file);
	newXS("PORK::event_del_refnum", PORK_event_del_refnum, file);
	newXS("PORK::event_purge", PORK_event_purge, file);

	newXS("PORK::chat_ban", PORK_chat_ban, file);
	newXS("PORK::chat_kick", PORK_chat_kick, file);
	newXS("PORK::chat_topic", PORK_chat_topic, file);
	newXS("PORK::chat_get_list", PORK_chat_get_list, file);
	newXS("PORK::chat_get_users", PORK_chat_get_users, file);
	newXS("PORK::chat_get_window", PORK_chat_get_window, file);
	newXS("PORK::chat_ignore", PORK_chat_ignore, file);
	newXS("PORK::chat_invite", PORK_chat_invite, file);
	newXS("PORK::chat_join", PORK_chat_join, file);
	newXS("PORK::chat_leave", PORK_chat_leave, file);
	newXS("PORK::chat_send", PORK_chat_send, file);
	newXS("PORK::chat_target", PORK_chat_target, file);
	newXS("PORK::chat_topic", PORK_chat_topic, file);
	newXS("PORK::chat_unignore", PORK_chat_unignore, file);
	newXS("PORK::input_send", PORK_input_send, file);
	newXS("PORK::input_get_data", PORK_input_get_data, file);
}

int perl_load_file(char *filename) {
	char *args[] = { filename, NULL };

	return (execute_perl("load_n_eval", args));
}

int execute_perl_va(char *function, const char *fmt, va_list ap) {
	u_int32_t i;
	int count;
	int ret_value = -1;

	dSP;
	ENTER;
	SAVETMPS;
	PUSHMARK(sp);

	for (i = 0 ; fmt[i] != '\0'; i++) {
		switch (fmt[i]) {
			case 'S':
			case 's': {
				char *str;

				str = va_arg(ap, char *);
				if (str != NULL) {
					size_t len;

					len = strlen(str);
					XPUSHs(sv_2mortal(newSVpv(str, len)));
				} else
					XPUSHs(sv_2mortal(newSVsv(&PL_sv_undef)));
				break;
			}

			case 'i': {
				int int_arg;

				int_arg = va_arg(ap, int);
				XPUSHs(sv_2mortal(newSViv(int_arg)));
				break;
			}
		}
	}

	PUTBACK;
	count = perl_call_pv(function, G_EVAL | G_SCALAR);
	SPAGAIN;

	if (SvTRUE(ERRSV)) {
		size_t notused;
		char *err_msg = SvPV(ERRSV, notused);

		screen_err_msg("Perl error: %s", err_msg);
		(void) POPs;
		ret_value = 0;
	} else if (count != 1) {
		screen_err_msg("Perl error: expected 1 value from %s, got %d",
			function, count);
		ret_value = 0;
	} else
		ret_value = POPi;

	PUTBACK;
	FREETMPS;
	LEAVE;

	return (ret_value);
}

int execute_perl(char *function, char **args) {
	int count;
	int ret_value = 1;

	dSP;
	ENTER;
	SAVETMPS;
	PUSHMARK(sp);

	count = perl_call_argv(function, G_EVAL | G_SCALAR, args);
	SPAGAIN;

	if (SvTRUE(ERRSV)) {
		size_t notused;
		char *err_msg = SvPV(ERRSV, notused);

		screen_err_msg("Perl error: %s", err_msg);
		(void) POPs;
	} else if (count != 1) {
		screen_err_msg("Perl error: expected 1 value from %s, got %d",
			function, count);
	} else {
		ret_value = POPi;
	}

	PUTBACK;
	FREETMPS;
	LEAVE;

	return (ret_value);
}

int perl_init(void) {
	static char *perl_args[] = { "", "-e", "0", "-w" };
	static char perl_definitions[] = {
		"sub load_file{"
			"my $f_name=shift;"
			"local $/=undef;"
			"open FH,$f_name or return \"__FAILED__\";"
			"$_=<FH>;"
			"close FH;"
			"return $_;"
		"}"
		"sub load_n_eval{"
			"my $f_name=shift;"
			"my $strin=load_file($f_name);"
			"return 2 if($strin eq \"__FAILED__\");"
			"eval $strin;"
			"if($@){"
				/*" #something went wrong\n"*/
				"PORK::err_msg(\"While loading file '$f_name': $@\");"
				"return 1;"
			"}"
			"return 0;"
		"}"
	};

	perl_inter = perl_alloc();
	if (perl_inter == NULL)
		return (-1);

	perl_construct(perl_inter);
	perl_parse(perl_inter, xs_init, 3, perl_args, NULL);

#ifdef HAVE_PERL_EVAL_PV
	eval_pv(perl_definitions, TRUE);
#else
	perl_eval_pv(perl_definitions, TRUE);
#endif

	return (0);
}

int perl_destroy(void) {
	if (perl_inter != NULL) {
		perl_destruct(perl_inter);
		perl_free(perl_inter);

		return (0);
	}

	return (-1);
}
