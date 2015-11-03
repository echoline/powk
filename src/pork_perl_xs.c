/*
** pork_perl_xs.c - Perl scripting support
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#include <config.h>

#include <unistd.h>
#include <ncurses.h>
#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/time.h>

#include <stdlib.h>
#include <sys/mman.h>

#undef PACKAGE
#undef instr

#ifndef HAS_BOOL
#	define HAS_BOOL
#endif

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

#include <pork.h>
#include <pork_missing.h>
#include <pork_util.h>
#include <pork_list.h>
#include <pork_events.h>
#include <pork_buddy.h>
#include <pork_set.h>
#include <pork_swindow.h>
#include <pork_imwindow.h>
#include <pork_proto.h>
#include <pork_acct.h>
#include <pork_input.h>
#include <pork_bind.h>
#include <pork_screen.h>
#include <pork_imsg.h>
#include <pork_screen_io.h>
#include <pork_status.h>
#include <pork_screen.h>
#include <pork_timer.h>
#include <pork_command.h>
#include <pork_command_defs.h>
#include <pork_slist.h>
#include <pork_buddy_list.h>
#include <pork_conf.h>
#include <pork_alias.h>
#include <pork_chat.h>
#include <pork_msg.h>
#include <pork_events.h>
#include <pork_perl.h>
#include <pork_perl_xs.h>

extern struct screen screen;

XS(PORK_run_cmd) {
	char *command_str;
	size_t notused;
	u_int32_t i = 0;
	dXSARGS;

	(void) cv;

	if (items < 1)
		XSRETURN_IV(-1);

	while (items-- > 0) {
		command_str = SvPV(ST(i), notused);
		i++;

		if (command_str == NULL)
			continue;

		run_command(command_str);
	}

	XSRETURN_IV(0);
}

XS(PORK_connect) {
	char *username = NULL;
	size_t notused;
	dXSARGS;

	(void) cv;

	if (items != 0)
		username = SvPV(ST(0), notused);

	cmd_connect(username);
	XSRETURN_IV(0);
}

XS(PORK_input_send) {
	char *args;
	size_t notused;
	dXSARGS;

	(void) cv;

	if (items == 0)
		XSRETURN_IV(-1);

	args = SvPV(ST(0), notused);

	if (args == NULL)
		XSRETURN_IV(-1);

	cmd_input_send(args);
	XSRETURN_IV(0);
}

XS(PORK_input_get_data) {
	dXSARGS;

	(void) cv;
	(void) items;

	XSRETURN_PV(input_get_buf_str(cur_window()->input));
}

XS(PORK_disconnect) {
	struct pork_acct *acct;
	char *reason = NULL;
	dXSARGS;

	(void) cv;

	if (items >= 1) {
		u_int32_t acct_refnum = SvIV(ST(0));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);

		if (items >= 2) {
			size_t notused;

			reason = SvPV(ST(1), notused);
		}
	} else
		acct = cur_window()->owner;

	if (!acct->can_connect)
		XSRETURN_IV(-1);

	XSRETURN_IV(pork_acct_del_refnum(acct->refnum, reason));
}

XS(PORK_cur_user) {
	dXSARGS;

	(void) cv;
	(void) items;
	XSRETURN_PV(cur_window()->owner->username);
}

XS(PORK_err_msg) {
	size_t notused;
	char *msg;
	dXSARGS;

	(void) cv;

	if (items != 1)
		XSRETURN_IV(-1);

	msg = SvPV(ST(0), notused);
	if (msg == NULL)
		XSRETURN_IV(-1);

	screen_err_msg("%s", msg);
	XSRETURN_IV(0);
}

XS(PORK_prompt_user) {
	dXSARGS;
	char buf[4096];
	int ret;
	char *prompt = NULL;

	(void) cv;

	if (items > 0) {
		size_t notused;

		prompt = SvPV(ST(0), notused);
	}

	screen_prompt_user(prompt, buf, sizeof(buf));
	ret = strlen(buf);

	if (ret <= 0)
		XSRETURN_UNDEF;

	XSRETURN_PV(buf);
}

XS(PORK_status_msg) {
	size_t notused;
	char *msg;
	dXSARGS;

	(void) cv;

	if (items != 1)
		XSRETURN_IV(-1);

	msg = SvPV(ST(0), notused);
	if (msg == NULL)
		XSRETURN_IV(-1);

	screen_win_msg(cur_window(), 1, 1, 1, MSG_TYPE_STATUS, "%s", msg);
	XSRETURN_IV(0);
}

XS(PORK_load) {
	int args = 0;
	dXSARGS;

	(void) cv;

	if (items < 1)
		XSRETURN_IV(-1);

	while (items > 0) {
		char *file;
		size_t notused;

		file = SvPV(ST(args), notused);
		if (file == NULL)
			XSRETURN_IV(-1);

		if (read_conf(file) != 0)
			XSRETURN_IV(-1);

		args++;
		items--;
	}

	XSRETURN_IV(0);
}

XS(PORK_load_perl) {
	int args = 0;
	dXSARGS;

	(void) cv;

	if (items < 1)
		XSRETURN_IV(-1);

	while (items > 0) {
		char *file;
		size_t notused;

		file = SvPV(ST(args), notused);
		if (file == NULL)
			XSRETURN_IV(-1);

		if (perl_load_file(file) != 0)
			XSRETURN_IV(-1);

		args++;
		items--;
	}

	XSRETURN_IV(0);
}

XS(PORK_echo) {
	size_t notused;
	char *msg;
	dXSARGS;

	(void) cv;

	if (items < 1)
		XSRETURN_IV(-1);

	msg = SvPV(ST(0), notused);
	screen_win_msg(cur_window(), 0, 0, 1, MSG_TYPE_CMD_OUTPUT, msg);
	XSRETURN_IV(0);
}

XS(PORK_quit) {
	int exit_val = 0;
	char *reason = NULL;
	dXSARGS;

	(void) cv;

	if (items >= 1)
		exit_val = SvIV(ST(0));

	if (items >= 2) {
		size_t notused;

		reason = SvPV(ST(1), notused);
	}

	pork_exit(exit_val, reason, NULL);
	XSRETURN_IV(0);
}

XS(PORK_quote) {
	struct pork_acct *acct;
	char *str;
	size_t notused;
	dXSARGS;

	(void) cv;

	str = SvPV(ST(0), notused);
	if (str == NULL)
		XSRETURN_IV(-1);

	if (items > 1) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	if (acct->proto->quote == NULL)
		XSRETURN_IV(-1);

	XSRETURN_IV(acct->proto->quote(acct, str));
}

XS(PORK_refresh) {
	dXSARGS;

	(void) cv;
	(void) items;

	screen_refresh();
	XSRETURN_IV(0);
}

XS(PORK_save) {
	dXSARGS;

	(void) cv;
	(void) items;

	XSRETURN_IV(save_global_config());
}

XS(PORK_bind) {
	char *key_str;
	int key;
	char *command;
	size_t notused;
	dXSARGS;

	(void) cv;

	if (items != 2)
		XSRETURN_IV(-1);

	key_str = SvPV(ST(0), notused);
	command = SvPV(ST(1), notused);

	if (key_str == NULL || command == NULL)
		XSRETURN_IV(-1);

	key = bind_get_keycode(key_str);
	if (key == -1)
		XSRETURN_IV(-1);

	bind_add(&screen.binds.main, key, command);
	XSRETURN_IV(0);
}

XS(PORK_unbind) {
	char *key_str;
	int key;
	size_t notused;
	dXSARGS;

	(void) cv;

	if (items != 1)
		XSRETURN_IV(-1);

	key_str = SvPV(ST(0), notused);
	if (key_str == NULL)
		XSRETURN_IV(-1);

	key = bind_get_keycode(key_str);
	if (key == -1)
		XSRETURN_IV(-1);

	key = bind_remove(&screen.binds.main, key);
	XSRETURN_IV(0);
}

XS(PORK_bind_get) {
	char *key_str;
	int key;
	size_t notused;
	struct binding *binding;
	dXSARGS;

	(void) cv;

	if (items != 1)
		XSRETURN_EMPTY;

	key_str = SvPV(ST(0), notused);
	if (key_str == NULL)
		XSRETURN_EMPTY;

	key = bind_get_keycode(key_str);
	if (key == -1)
		XSRETURN_EMPTY;

	binding = bind_find(&screen.binds.main, key);
	if (binding == NULL)
		XSRETURN_EMPTY;

	XSRETURN_PV(binding->binding);
}

XS(PORK_blist_bind) {
	char *key_str;
	int key;
	char *command;
	size_t notused;
	dXSARGS;

	(void) cv;

	if (items != 2)
		XSRETURN_IV(-1);

	key_str = SvPV(ST(0), notused);
	command = SvPV(ST(1), notused);

	if (key_str == NULL || command == NULL)
		XSRETURN_IV(-1);

	key = bind_get_keycode(key_str);
	if (key == -1)
		XSRETURN_IV(-1);

	bind_add(&screen.binds.blist, key, command);
	XSRETURN_IV(0);
}

XS(PORK_blist_unbind) {
	char *key_str;
	int key;
	size_t notused;
	dXSARGS;

	(void) cv;

	if (items != 1)
		XSRETURN_IV(-1);

	key_str = SvPV(ST(0), notused);
	if (key_str == NULL)
		XSRETURN_IV(-1);

	key = bind_get_keycode(key_str);
	if (key == -1)
		XSRETURN_IV(-1);

	key = bind_remove(&screen.binds.blist, key);
	XSRETURN_IV(0);
}

XS(PORK_blist_bind_get) {
	char *key_str;
	int key;
	size_t notused;
	struct binding *binding;
	dXSARGS;

	(void) cv;

	if (items != 1)
		XSRETURN_EMPTY;

	key_str = SvPV(ST(0), notused);
	if (key_str == NULL)
		XSRETURN_EMPTY;

	key = bind_get_keycode(key_str);
	if (key == -1)
		XSRETURN_EMPTY;

	binding = bind_find(&screen.binds.blist, key);
	if (binding == NULL)
		XSRETURN_EMPTY;

	XSRETURN_PV(binding->binding);
}

XS(PORK_alias) {
	char *alias;
	char *cmd;
	size_t notused;
	dXSARGS;

	(void) cv;

	if (items != 2)
		XSRETURN_IV(-1);

	alias = SvPV(ST(0), notused);
	cmd = SvPV(ST(1), notused);

	if (alias == NULL || cmd == NULL)
		XSRETURN_IV(-1);

	XSRETURN_IV(alias_add(&screen.alias_hash, alias, cmd));
}

XS(PORK_alias_get) {
	char *str;
	size_t notused;
	struct alias *alias;
	dXSARGS;

	(void) cv;

	if (items != 1)
		XSRETURN_EMPTY;

	str = SvPV(ST(0), notused);
	if (str == NULL)
		XSRETURN_EMPTY;

	alias = alias_find(&screen.alias_hash, str);
	if (alias == NULL)
		XSRETURN_EMPTY;

	SP -= items;
	XPUSHs(sv_2mortal(newSVpv(alias->orig, 0)));

	if (alias->args != NULL) {
		XPUSHs(sv_2mortal(newSVpv(alias->args, 0)));
		XSRETURN(2);
	}

	XSRETURN(1);
}

XS(PORK_unalias) {
	char *alias;
	size_t notused;
	dXSARGS;

	(void) cv;

	if (items != 1)
		XSRETURN_IV(-1);

	alias = SvPV(ST(0), notused);
	if (alias == NULL)
		XSRETURN_IV(-1);

	XSRETURN_IV(alias_remove(&screen.alias_hash, alias));
}

XS(PORK_set_opt) {
	char *var;
	char *val;
	size_t notused;
	int opt;
	dXSARGS;

	(void) cv;

	if (items != 2)
		XSRETURN_IV(-1);

	var = SvPV(ST(0), notused);
	val = SvPV(ST(1), notused);

	if (var == NULL || var == NULL)
		XSRETURN_IV(-1);

	opt = opt_find(var);
	if (opt == -1)
		XSRETURN_IV(-1);

	XSRETURN_IV(opt_set(opt, val));
}

XS(PORK_get_cur_user) {
	dXSARGS;

	(void) cv;
	(void) items;

	XSRETURN_PV(cur_window()->owner->username);
}

XS(PORK_get_opt) {
	char *val;
	size_t notused;
	char buf[2048];

	dXSARGS;

	(void) cv;

	if (items != 1)
		XSRETURN_EMPTY;

	val = SvPV(ST(0), notused);
	if (val == NULL)
		XSRETURN_EMPTY;

	if (opt_get_val(val, buf, sizeof(buf)) == -1)
		XSRETURN_EMPTY;

	XSRETURN_PV(buf);
}

XS(PORK_send_profile) {
	size_t notused;
	char *profile;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	profile = SvPV(ST(0), notused);

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	if (!acct->connected)
		XSRETURN_IV(-1);

	XSRETURN_IV(acct->proto->set_profile(acct, profile));
}

/* XXX - fix */

XS(PORK_get_acct_list) {
	dlist_t *cur = screen.acct_list;
	int args = 0;
	dXSARGS;

	(void) cv;

	if (items != 0 || cur == NULL)
		XSRETURN_EMPTY;

	SP -= items;
	do {
		struct pork_acct *acct = cur->data;

		if (acct->can_connect) {
			XPUSHs(sv_2mortal(newSVpv(acct->username, 0)));
			args++;
		}
		cur = cur->next;
	} while (cur != NULL);

	XSRETURN(args);
}

XS(PORK_set_profile) {
	size_t notused;
	char *profile;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	profile = SvPV(ST(0), notused);

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	free(acct->profile);
	if (profile != NULL)
		acct->profile = xstrdup(profile);
	else
		acct->profile = NULL;

	if (!acct->connected)
		XSRETURN_IV(-1);

	XSRETURN_IV(pork_set_profile(acct, profile));
}

XS(PORK_get_profile) {
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 0)
		XSRETURN_EMPTY;

	if (items == 1) {
		u_int32_t acct_refnum = SvIV(ST(0));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_EMPTY;
	} else
		acct = cur_window()->owner;

	if (acct->profile == NULL)
		XSRETURN_EMPTY;

	XSRETURN_PV(acct->profile);
}

XS(PORK_send_msg) {
	size_t notused;
	char *dest;
	char *msg;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 2 && items != 3)
		XSRETURN_IV(-1);

	dest = SvPV(ST(0), notused);
	msg = SvPV(ST(1), notused);

	if (dest == NULL || msg == NULL)
		XSRETURN_IV(-1);

	if (items == 3) {
		u_int32_t acct_refnum = SvIV(ST(2));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	if (!acct->connected)
		XSRETURN_IV(-1);

	return (XSRETURN_IV(pork_msg_send(acct, dest, msg)));
}

XS(PORK_send_msg_auto) {
	size_t notused;
	char *dest;
	char *msg;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 2 && items != 3)
		XSRETURN_IV(-1);

	dest = SvPV(ST(0), notused);
	msg = SvPV(ST(1), notused);

	if (dest == NULL || msg == NULL)
		XSRETURN_IV(-1);

	if (items == 3) {
		u_int32_t acct_refnum = SvIV(ST(2));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	if (!acct->connected)
		XSRETURN_IV(-1);

	return (XSRETURN_IV(pork_msg_autoreply(acct, dest, msg)));
}

XS(PORK_get_buddy_profile) {
	size_t notused;
	char *target;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	target = SvPV(ST(0), notused);
	if (target == NULL)
		XSRETURN_IV(-1);

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	if (!acct->connected)
		XSRETURN_IV(-1);

	XSRETURN_IV(acct->proto->get_profile(acct, target));
}

XS(PORK_get_buddy_away) {
	size_t notused;
	char *target;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	target = SvPV(ST(0), notused);
	if (target == NULL)
		XSRETURN_IV(-1);

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	if (!acct->connected)
		XSRETURN_IV(-1);

	XSRETURN_IV(acct->proto->get_away_msg(acct, target));
}

XS(PORK_privacy_mode) {
	int privacy_mode;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	privacy_mode = SvIV(ST(0));

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	if (!acct->connected)
		XSRETURN_IV(-1);

	XSRETURN_IV(acct->proto->set_privacy_mode(acct, privacy_mode));
}

XS(PORK_report_idle) {
	int report_idle;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	report_idle = SvIV(ST(0));

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	if (!acct->connected)
		XSRETURN_IV(-1);

	XSRETURN_IV(acct->proto->set_report_idle(acct, report_idle));
}

XS(PORK_search) {
	size_t notused;
	char *target;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	target = SvPV(ST(0), notused);

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;


	if (!acct->connected)
		XSRETURN_IV(-1);

	XSRETURN_IV(acct->proto->who(acct, target));
}

XS(PORK_set_away) {
	size_t notused;
	char *msg = NULL;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items > 2)
		XSRETURN_IV(-1);

	if (items >= 1 && SvTRUE(ST(0)))
		msg = SvPV(ST(0), notused);

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	if (!acct->connected)
		XSRETURN_IV(-1);

	XSRETURN_IV(pork_set_away(acct, msg));
}

XS(PORK_set_idle) {
	int seconds;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	seconds = SvIV(ST(0));

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	if (!acct->connected)
		XSRETURN_IV(-1);

	XSRETURN_IV(pork_set_idle_time(acct, seconds));
}

XS(PORK_warn) {
	size_t notused;
	char *target;
	int anon;
	struct pork_acct *acct;
	int ret;
	dXSARGS;

	(void) cv;

	if (items != 2 && items != 3)
		XSRETURN_IV(-1);

	target = SvPV(ST(0), notused);
	anon = SvIV(ST(1));

	if (target == NULL)
		XSRETURN_IV(-1);

	if (items == 3) {
		u_int32_t acct_refnum = SvIV(ST(2));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	if (!acct->connected)
		XSRETURN_IV(-1);

	if (anon != 0)
		ret = pork_send_warn(acct, target);
	else
		ret = pork_send_warn_anon(acct, target);

	XSRETURN_IV(ret);
}

/*
** Wrappers for non-aim /buddy commands.
*/

XS(PORK_buddy_add) {
	size_t notused;
	char *target;
	char *group;
	struct pork_acct *acct;
	struct bgroup *bgroup;
	dXSARGS;

	(void) cv;

	if (items != 2 && items != 3)
		XSRETURN_IV(-1);

	target = SvPV(ST(0), notused);
	group = SvPV(ST(1), notused);

	if (target == NULL || group == NULL)
		XSRETURN_IV(-1);

	if (items == 3) {
		u_int32_t acct_refnum = SvIV(ST(2));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	bgroup = group_find(acct, group);
	if (group == NULL)
		XSRETURN_IV(-1);

	XSRETURN_IV(buddy_add(acct, target, bgroup, 1) == NULL);
}

XS(PORK_buddy_add_block) {
	size_t notused;
	char *target;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	target = SvPV(ST(0), notused);
	if (target == NULL)
		XSRETURN_IV(-1);

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	XSRETURN_IV(buddy_add_block(acct, target, 1));
}

XS(PORK_buddy_clear_permit) {
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 0 && items != 1)
		XSRETURN_IV(-1);

	if (items == 1) {
		u_int32_t acct_refnum = SvIV(ST(0));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	buddy_clear_permit(acct);
	XSRETURN_IV(0);
}

XS(PORK_buddy_clear_block) {
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 0 && items != 1)
		XSRETURN_IV(-1);

	if (items == 1) {
		u_int32_t acct_refnum = SvIV(ST(0));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	buddy_clear_block(acct);
	XSRETURN_IV(0);
}

XS(PORK_buddy_add_permit) {
	size_t notused;
	char *target;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	target = SvPV(ST(0), notused);
	if (target == NULL)
		XSRETURN_IV(-1);

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	XSRETURN_IV(buddy_add_permit(acct, target, 1));
}

XS(PORK_buddy_alias) {
	size_t notused;
	char *target;
	char *alias;
	struct buddy *buddy;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 2 && items != 3)
		XSRETURN_IV(-1);

	target = SvPV(ST(0), notused);
	alias = SvPV(ST(1), notused);

	if (target == NULL || alias == NULL)
		XSRETURN_IV(-1);

	if (items == 3) {
		u_int32_t acct_refnum = SvIV(ST(2));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	buddy = buddy_find(acct, target);
	if (buddy == NULL)
		XSRETURN_IV(-1);

	buddy_alias(acct, buddy, alias, 0);
	XSRETURN_IV(0);
}

XS(PORK_buddy_get_alias) {
	size_t notused;
	char *target;
	struct buddy *buddy;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_EMPTY;

	target = SvPV(ST(0), notused);
	if (target == NULL)
		XSRETURN_EMPTY;

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_EMPTY;
	} else
		acct = cur_window()->owner;

	buddy = buddy_find(acct, target);
	if (buddy == NULL)
		XSRETURN_EMPTY;

	XSRETURN_PV(buddy->name);
}

XS(PORK_buddy_remove_block) {
	size_t notused;
	char *target;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	target = SvPV(ST(0), notused);
	if (target == NULL)
		XSRETURN_IV(-1);

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	XSRETURN_IV(buddy_remove_block(acct, target, 1));
}

XS(PORK_buddy_get_groups) {
	dlist_t *cur;
	int args = 0;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items == 1) {
		u_int32_t acct_refnum = SvIV(ST(0));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_EMPTY;
	} else
		acct = cur_window()->owner;

	if (acct->buddy_pref == NULL)
		XSRETURN_EMPTY;

	SP -= items;
	for (cur = acct->buddy_pref->group_list ; cur != NULL ; cur = cur->next) {
		struct bgroup *group = cur->data;

		XPUSHs(sv_2mortal(newSVpv(group->name, 0)));
		args++;
	}

	XSRETURN(args);
}

XS(PORK_buddy_get_block) {
	dlist_t *cur;
	int args = 0;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items == 1) {
		u_int32_t acct_refnum = SvIV(ST(0));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_EMPTY;
	} else
		acct = cur_window()->owner;

	if (acct->buddy_pref == NULL)
		XSRETURN_EMPTY;

	SP -= items;
	for (cur = acct->buddy_pref->block_list ; cur != NULL ; cur = cur->next) {
		XPUSHs(sv_2mortal(newSVpv(cur->data, 0)));
		args++;
	}

	XSRETURN(args);
}

XS(PORK_buddy_get_permit) {
	dlist_t *cur;
	int args = 0;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items == 1) {
		u_int32_t acct_refnum = SvIV(ST(0));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_EMPTY;
	} else
		acct = cur_window()->owner;

	if (acct->buddy_pref == NULL)
		XSRETURN_EMPTY;

	SP -= items;
	for (cur = acct->buddy_pref->permit_list ; cur != NULL ; cur = cur->next) {
		XPUSHs(sv_2mortal(newSVpv(cur->data, 0)));
		args++;
	}

	XSRETURN(args);
}

XS(PORK_buddy_get_group_members) {
	dlist_t *cur;
	char *group;
	struct bgroup *bgroup;
	size_t notused;
	int args = 0;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_EMPTY;

	group = SvPV(ST(0), notused);
	if (group == NULL)
		XSRETURN_EMPTY;

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_EMPTY;
	} else
		acct = cur_window()->owner;

	bgroup = group_find(acct, group);
	if (bgroup == NULL)
		XSRETURN_EMPTY;

	SP -= items;
	for (cur = bgroup->member_list ; cur != NULL ; cur = cur->next) {
		struct buddy *buddy = cur->data;

		XPUSHs(sv_2mortal(newSVpv(buddy->nname, 0)));
		args++;
	}

	XSRETURN(args);
}

XS(PORK_buddy_remove) {
	size_t notused;
	char *target;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	target = SvPV(ST(0), notused);
	if (target == NULL)
		XSRETURN_IV(-1);

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	XSRETURN_IV(buddy_remove(acct, target, 1));
}

XS(PORK_buddy_remove_group) {
	size_t notused;
	char *target;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	target = SvPV(ST(0), notused);
	if (target == NULL)
		XSRETURN_IV(-1);

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	XSRETURN_IV(group_remove(acct, target, 1));
}

XS(PORK_buddy_add_group) {
	size_t notused;
	char *target;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	target = SvPV(ST(0), notused);
	if (target == NULL)
		XSRETURN_IV(-1);

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	if (group_add(acct, target) == NULL)
		XSRETURN_IV(-1);

	XSRETURN_IV(0);
}

XS(PORK_buddy_remove_permit) {
	size_t notused;
	char *target;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	target = SvPV(ST(0), notused);
	if (target == NULL)
		XSRETURN_IV(-1);

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	XSRETURN_IV(buddy_remove_permit(acct, target, 1));
}

/*
** Scroll commands
*/

XS(PORK_scroll_by) {
	int lines;
	struct imwindow *imwindow;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	lines = SvIV(ST(0));

	if (items == 2) {
		u_int32_t refnum = SvIV(ST(1));

		imwindow = imwindow_find_refnum(refnum);
		if (imwindow == NULL)
			XSRETURN_IV(-1);
	} else
		imwindow = cur_window();

	imwindow_scroll_by(imwindow, lines);
	XSRETURN_IV(0);
}

XS(PORK_scroll_down) {
	struct imwindow *imwindow;
	dXSARGS;

	(void) cv;

	if (items != 0 && items != 1)
		XSRETURN_IV(-1);

	if (items == 1) {
		u_int32_t refnum = SvIV(ST(0));

		imwindow = imwindow_find_refnum(refnum);
		if (imwindow == NULL)
			XSRETURN_IV(-1);
	} else
		imwindow = cur_window();

	imwindow_scroll_down(imwindow);
	XSRETURN_IV(0);
}

XS(PORK_scroll_end) {
	struct imwindow *imwindow;
	dXSARGS;

	(void) cv;

	if (items != 0 && items != 1)
		XSRETURN_IV(-1);

	if (items == 1) {
		u_int32_t refnum = SvIV(ST(0));

		imwindow = imwindow_find_refnum(refnum);
		if (imwindow == NULL)
			XSRETURN_IV(-1);
	} else
		imwindow = cur_window();

	imwindow_scroll_end(imwindow);
	XSRETURN_IV(0);
}

XS(PORK_scroll_page_down) {
	struct imwindow *imwindow;
	dXSARGS;

	(void) cv;

	if (items != 0 && items != 1)
		XSRETURN_IV(-1);

	if (items == 1) {
		u_int32_t refnum = SvIV(ST(0));

		imwindow = imwindow_find_refnum(refnum);
		if (imwindow == NULL)
			XSRETURN_IV(-1);
	} else
		imwindow = cur_window();

	imwindow_scroll_page_down(imwindow);
	XSRETURN_IV(0);
}

XS(PORK_scroll_page_up) {
	struct imwindow *imwindow;
	dXSARGS;

	(void) cv;

	if (items != 0 && items != 1)
		XSRETURN_IV(-1);

	if (items == 1) {
		u_int32_t refnum = SvIV(ST(0));

		imwindow = imwindow_find_refnum(refnum);
		if (imwindow == NULL)
			XSRETURN_IV(-1);
	} else
		imwindow = cur_window();

	imwindow_scroll_page_up(imwindow);
	XSRETURN_IV(0);
}

XS(PORK_scroll_start) {
	struct imwindow *imwindow;
	dXSARGS;

	(void) cv;

	if (items != 0 && items != 1)
		XSRETURN_IV(-1);

	if (items == 1) {
		u_int32_t refnum = SvIV(ST(0));

		imwindow = imwindow_find_refnum(refnum);
		if (imwindow == NULL)
			XSRETURN_IV(-1);
	} else
		imwindow = cur_window();

	imwindow_scroll_start(imwindow);
	XSRETURN_IV(0);
}

XS(PORK_scroll_up) {
	struct imwindow *imwindow;
	dXSARGS;

	(void) cv;

	if (items != 0 && items != 1)
		XSRETURN_IV(-1);

	if (items == 1) {
		u_int32_t refnum = SvIV(ST(0));

		imwindow = imwindow_find_refnum(refnum);
		if (imwindow == NULL)
			XSRETURN_IV(-1);
	} else
		imwindow = cur_window();

	imwindow_scroll_up(imwindow);
	XSRETURN_IV(0);
}

/*
** window commands
*/

XS(PORK_win_find_name) {
	char *str;
	size_t notused;
	struct pork_acct *acct;
	struct imwindow *imwindow;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	str = SvPV(ST(0), notused);
	if (str == NULL)
		XSRETURN_IV(-1);

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	imwindow = imwindow_find_name(acct, str);
	if (imwindow == NULL)
		XSRETURN_IV(-1);

	XSRETURN_IV(imwindow->refnum);
}

XS(PORK_win_target) {
	u_int32_t win_refnum;
	struct imwindow *imwindow;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items == 0)
		XSRETURN_PV(cur_window()->target);

	win_refnum = SvIV(ST(0));

	if (items > 1) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_UNDEF;
	} else
		acct = cur_window()->owner;

	imwindow = imwindow_find_refnum(win_refnum);
	if (imwindow == NULL)
		XSRETURN_UNDEF;

	XSRETURN_PV(imwindow->target);
}

XS(PORK_win_find_target) {
	char *str;
	size_t notused;
	struct imwindow *imwindow;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	str = SvPV(ST(0), notused);
	if (str == NULL)
		XSRETURN_IV(-1);

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	imwindow = imwindow_find(acct, str);
	if (imwindow == NULL)
		XSRETURN_IV(-1);

	XSRETURN_IV(imwindow->refnum);
}

XS(PORK_win_bind) {
	u_int32_t acct_refnum;
	struct imwindow *imwindow;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	acct_refnum = SvIV(ST(0));

	if (items == 2) {
		u_int32_t refnum = SvIV(ST(1));

		imwindow = imwindow_find_refnum(refnum);
		if (imwindow == NULL)
			XSRETURN_IV(-1);
	} else
		imwindow = cur_window();

	XSRETURN_IV(imwindow_bind_acct(imwindow, acct_refnum));
}

XS(PORK_win_clear) {
	struct imwindow *imwindow;
	dXSARGS;

	(void) cv;

	if (items != 0 && items != 1)
		XSRETURN_IV(-1);

	if (items == 1) {
		u_int32_t refnum = SvIV(ST(0));

		imwindow = imwindow_find_refnum(refnum);
		if (imwindow == NULL)
			XSRETURN_IV(-1);
	} else
		imwindow = cur_window();

	imwindow_clear(imwindow);
	XSRETURN_IV(0);
}

XS(PORK_win_close) {
	struct imwindow *imwindow;
	dXSARGS;

	(void) cv;

	if (items != 0 && items != 1)
		XSRETURN_IV(-1);

	if (items == 1) {
		u_int32_t refnum = SvIV(ST(0));

		imwindow = imwindow_find_refnum(refnum);
		if (imwindow == NULL)
			XSRETURN_IV(-1);
	} else
		imwindow = cur_window();

	XSRETURN_IV(screen_close_window(imwindow));
}

XS(PORK_win_erase) {
	struct imwindow *imwindow;
	dXSARGS;

	(void) cv;

	if (items != 0 && items != 1)
		XSRETURN_IV(-1);

	if (items == 1) {
		u_int32_t refnum = SvIV(ST(0));

		imwindow = imwindow_find_refnum(refnum);
		if (imwindow == NULL)
			XSRETURN_IV(-1);
	} else
		imwindow = cur_window();

	imwindow_erase(imwindow);
	XSRETURN_IV(0);
}

XS(PORK_win_next) {
	dXSARGS;

	(void) cv;

	if (items != 0)
		XSRETURN_IV(-1);

	screen_cycle_fwd();
	XSRETURN_IV(0);
}

XS(PORK_win_prev) {
	dXSARGS;

	(void) cv;

	if (items != 0)
		XSRETURN_IV(-1);

	screen_cycle_bak();
	XSRETURN_IV(0);
}

XS(PORK_win_rename) {
	dXSARGS;
	struct imwindow *imwindow;
	char *new_name;
	size_t notused;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	new_name = SvPV(ST(0), notused);
	if (new_name == NULL)
		XSRETURN_IV(-1);

	if (items == 2) {
		u_int32_t refnum = SvIV(ST(1));

		imwindow = imwindow_find_refnum(refnum);
		if (imwindow == NULL)
			XSRETURN_IV(-1);
	} else
		imwindow = cur_window();

	imwindow_rename(imwindow, new_name);
	XSRETURN_IV(0);
}

XS(PORK_win_renumber) {
	dXSARGS;
	struct imwindow *imwindow;
	u_int32_t new_refnum;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	new_refnum = SvIV(ST(0));

	if (items == 2) {
		u_int32_t refnum = SvIV(ST(1));

		imwindow = imwindow_find_refnum(refnum);
		if (imwindow == NULL)
			XSRETURN_IV(-1);
	} else
		imwindow = cur_window();

	XSRETURN_IV(screen_renumber(imwindow, new_refnum));
}

XS(PORK_win_get_opt) {
	struct imwindow *imwindow;
	char *val;
	size_t notused;
	char buf[2048];

	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_EMPTY;

	val = SvPV(ST(0), notused);
	if (val == NULL)
		XSRETURN_EMPTY;

	if (items == 2) {
		u_int32_t refnum = SvIV(ST(1));

		imwindow = imwindow_find_refnum(refnum);
		if (imwindow == NULL)
			XSRETURN_EMPTY;
	} else
		imwindow = cur_window();

	if (wopt_get_val(imwindow, val, buf, sizeof(buf)) == -1)
		XSRETURN_EMPTY;

	XSRETURN_PV(buf);
}

XS(PORK_win_set_opt) {
	char *var;
	char *val;
	size_t notused;
	int opt;
	struct imwindow *imwindow;
	dXSARGS;

	(void) cv;

	if (items != 2 && items != 3)
		XSRETURN_IV(-1);

	var = SvPV(ST(0), notused);
	val = SvPV(ST(1), notused);

	if (var == NULL || var == NULL)
		XSRETURN_IV(-1);

	opt = wopt_find(var);
	if (opt == -1)
		XSRETURN_IV(-1);

	if (items == 3) {
		u_int32_t refnum = SvIV(ST(2));

		imwindow = imwindow_find_refnum(refnum);
		if (imwindow == NULL)
			XSRETURN_IV(-1);
	} else
		imwindow = cur_window();

	XSRETURN_IV(wopt_set(imwindow, opt, val));
}

XS(PORK_win_swap) {
	u_int32_t refnum;
	dXSARGS;

	(void) cv;

	if (items != 1)
		XSRETURN_IV(-1);

	refnum = SvIV(ST(0));
	XSRETURN_IV(screen_goto_window(refnum));
}

/*
** Timer commands.
*/

XS(PORK_timer_add) {
	char *command;
	u_int32_t interval;
	u_int32_t times;
	size_t notused;
	dXSARGS;

	(void) cv;

	if (items != 3)
		XSRETURN_UNDEF;

	interval = SvIV(ST(0));
	times = SvIV(ST(1));
	command = SvPV(ST(2), notused);

	if (command == NULL)
		XSRETURN_UNDEF;

	XSRETURN_IV(timer_add(&screen.timer_list, command, interval, times));
}

XS(PORK_timer_del) {
	char *command;
	size_t notused;
	dXSARGS;

	(void) cv;

	if (items != 1)
		XSRETURN_IV(-1);

	command = SvPV(ST(0), notused);
	if (command == NULL)
		XSRETURN_IV(-1);

	XSRETURN_IV(timer_del(&screen.timer_list, command));
}

XS(PORK_timer_del_refnum) {
	u_int32_t refnum;
	dXSARGS;

	(void) cv;

	if (items != 1)
		XSRETURN_IV(-1);

	refnum = SvIV(ST(0));
	XSRETURN_IV(timer_del_refnum(&screen.timer_list, refnum));
}

XS(PORK_timer_purge) {
	dXSARGS;

	(void) items;
	(void) cv;

	timer_destroy(&screen.timer_list);
	XSRETURN_IV(0);
}

/*
** /blist commands.
*/

XS(PORK_blist_collapse) {
	struct pork_acct *acct;
	struct blist *blist;
	size_t notused;
	char *group;
	struct bgroup *bgroup;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	group = SvPV(ST(0), notused);
	if (group == NULL)
		XSRETURN_IV(-1);

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	bgroup = group_find(acct, group);
	if (bgroup == NULL)
		XSRETURN_IV(-1);

	blist = acct->blist;
	if (blist == NULL)
		XSRETURN_IV(-1);

	blist_collapse_group(blist, bgroup);
	XSRETURN_IV(0);
}

XS(PORK_blist_cursor) {
	struct pork_acct *acct;
	struct blist *blist;
	struct slist_cell *cell;
	dXSARGS;

	(void) cv;

	if (items != 0 && items != 1)
		XSRETURN_EMPTY;

	if (items == 1) {
		u_int32_t acct_refnum = SvIV(ST(0));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_EMPTY;
	} else
		acct = cur_window()->owner;

	blist = acct->blist;
	if (blist == NULL)
		XSRETURN_EMPTY;

	cell = blist_get_cursor(blist);
	if (cell == NULL)
		XSRETURN_EMPTY;

	SP -= items;

	if (cell->type == TYPE_FLAT_CELL) {
		struct buddy *buddy = cell->data;

		XPUSHs(sv_2mortal(newSVpv(buddy->nname, 0)));
	} else {
		struct bgroup *group = cell->data;

		XPUSHs(sv_2mortal(newSVpv(group->name, 0)));
	}

	XPUSHs(sv_2mortal(newSViv(cell->type)));
	XSRETURN(2);
}

XS(PORK_blist_down) {
	struct pork_acct *acct;
	struct blist *blist;
	dXSARGS;

	(void) cv;

	if (items != 0 && items != 1)
		XSRETURN_IV(-1);

	if (items == 1) {
		u_int32_t acct_refnum = SvIV(ST(0));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	blist = acct->blist;
	if (blist == NULL)
		XSRETURN_IV(-1);

	XSRETURN_IV(blist_cursor_down(blist));
}

XS(PORK_blist_end) {
	struct pork_acct *acct;
	struct blist *blist;
	dXSARGS;

	(void) cv;

	if (items != 0 && items != 1)
		XSRETURN_IV(-1);

	if (items == 1) {
		u_int32_t acct_refnum = SvIV(ST(0));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	blist = acct->blist;
	if (blist == NULL)
		XSRETURN_IV(-1);

	XSRETURN_IV(blist_cursor_end(blist));
}

XS(PORK_blist_hide) {
	struct imwindow *imwindow;
	dXSARGS;

	(void) cv;

	if (items != 0 && items != 1)
		XSRETURN_IV(-1);

	if (items == 1) {
		u_int32_t refnum = SvIV(ST(0));

		imwindow = imwindow_find_refnum(refnum);
		if (imwindow == NULL)
			XSRETURN_IV(-1);
	} else
		imwindow = cur_window();

	imwindow_blist_hide(imwindow);
	XSRETURN_IV(0);
}

XS(PORK_blist_page_down) {
	struct pork_acct *acct;
	struct blist *blist;
	dXSARGS;

	(void) cv;

	if (items != 0 && items != 1)
		XSRETURN_IV(-1);

	if (items == 1) {
		u_int32_t acct_refnum = SvIV(ST(0));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	blist = acct->blist;
	if (blist == NULL)
		XSRETURN_IV(-1);

	XSRETURN_IV(blist_cursor_pgdown(blist));
}

XS(PORK_blist_page_up) {
	struct pork_acct *acct;
	struct blist *blist;
	dXSARGS;

	(void) cv;

	if (items != 0 && items != 1)
		XSRETURN_IV(-1);

	if (items == 1) {
		u_int32_t acct_refnum = SvIV(ST(0));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	blist = acct->blist;
	if (blist == NULL)
		XSRETURN_IV(-1);

	XSRETURN_IV(blist_cursor_pgup(blist));
}

XS(PORK_blist_refresh) {
	struct pork_acct *acct;
	struct blist *blist;
	dXSARGS;

	(void) cv;

	if (items != 0 && items != 1)
		XSRETURN_IV(-1);

	if (items == 1) {
		u_int32_t acct_refnum = SvIV(ST(0));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	blist = acct->blist;
	if (blist == NULL)
		XSRETURN_IV(-1);

	XSRETURN_IV(blist_refresh(blist));
}

XS(PORK_blist_show) {
	struct imwindow *imwindow;
	dXSARGS;

	(void) cv;

	if (items != 0 && items != 1)
		XSRETURN_IV(-1);

	if (items == 1) {
		u_int32_t refnum = SvIV(ST(0));

		imwindow = imwindow_find_refnum(refnum);
		if (imwindow == NULL)
			XSRETURN_IV(-1);
	} else
		imwindow = cur_window();

	imwindow_blist_show(imwindow);
	XSRETURN_IV(0);
}

XS(PORK_blist_start) {
	struct pork_acct *acct;
	struct blist *blist;
	dXSARGS;

	(void) cv;

	if (items != 0 && items != 1)
		XSRETURN_IV(-1);

	if (items == 1) {
		u_int32_t acct_refnum = SvIV(ST(0));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	blist = acct->blist;
	if (blist == NULL)
		XSRETURN_IV(-1);

	XSRETURN_IV(blist_cursor_start(blist));
}

XS(PORK_blist_up) {
	struct pork_acct *acct;
	struct blist *blist;
	dXSARGS;

	(void) cv;

	if (items != 0 && items != 1)
		XSRETURN_IV(-1);

	if (items == 1) {
		u_int32_t acct_refnum = SvIV(ST(0));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	blist = acct->blist;
	if (blist == NULL)
		XSRETURN_IV(-1);

	XSRETURN_IV(blist_cursor_up(blist));
}

XS(PORK_blist_width) {
	struct pork_acct *acct;
	struct blist *blist;
	int new_width;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	new_width = SvIV(ST(0));

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	blist = acct->blist;
	if (blist == NULL)
		XSRETURN_IV(-1);

	XSRETURN_IV(screen_blist_width(blist, new_width));
}

/*
** Event commands.
*/

XS(PORK_event_add) {
	struct pork_acct *acct;
	char *type;
	char *handler;
	size_t notused;
	dXSARGS;
	u_int32_t refnum;

	(void) cv;

	if (items != 2 && items != 3)
		XSRETURN_UNDEF;

	type = SvPV(ST(0), notused);
	handler = SvPV(ST(1), notused);

	if (type == NULL || handler == NULL)
		XSRETURN_UNDEF;

	if (items == 3) {
		u_int32_t acct_refnum = SvIV(ST(2));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_UNDEF;
	} else
		acct = cur_window()->owner;

	if (event_add(acct->events, type, handler, &refnum) != 0)
		XSRETURN_UNDEF;

	XSRETURN_IV(refnum);
}

XS(PORK_event_del) {
	struct pork_acct *acct;
	size_t notused;
	char *type;
	char *handler;
	dXSARGS;

	(void) cv;

	if (items != 2 && items != 3)
		XSRETURN_IV(-1);

	type = SvPV(ST(0), notused);
	handler = SvPV(ST(1), notused);

	if (type == NULL || handler == NULL)
		XSRETURN_IV(-1);

	if (items == 3) {
		u_int32_t acct_refnum = SvIV(ST(2));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	XSRETURN_IV(event_del_type(acct->events, type, handler));
}

XS(PORK_event_del_type) {
	struct pork_acct *acct;
	size_t notused;
	char *type;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	type = SvPV(ST(0), notused);

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	XSRETURN_IV(event_del_type(acct->events, type, NULL));
}

XS(PORK_event_del_refnum) {
	struct pork_acct *acct;
	u_int32_t refnum;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	refnum = SvIV(ST(0));

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	XSRETURN_IV(event_del_refnum(acct->events, refnum));
}

XS(PORK_event_purge) {
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items == 1) {
		u_int32_t acct_refnum = SvIV(ST(0));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	event_purge(acct->events);
	XSRETURN_IV(0);
}

/*
** /chat commands.
*/

XS(PORK_chat_ban) {
	char *chat_name;
	char *ban_user;
	size_t notused;
	struct pork_acct *acct;
	struct chatroom *chat;
	dXSARGS;

	(void) cv;

	if (items != 2 && items != 3)
		XSRETURN_IV(-1);

	chat_name = SvPV(ST(0), notused);
	ban_user = SvPV(ST(1), notused);

	if (chat_name == NULL || ban_user == NULL)
		XSRETURN_IV(-1);

	if (items == 3) {
		u_int32_t acct_refnum = SvIV(ST(2));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	chat = chat_find(acct, chat_name);
	if (chat == NULL)
		XSRETURN_IV(-1);

	XSRETURN_IV(chat_ban(acct, chat, ban_user));
}

XS(PORK_chat_kick) {
	char *chat_name;
	char *user;
	char *reason = NULL;
	size_t notused;
	struct pork_acct *acct;
	struct chatroom *chat;
	dXSARGS;

	(void) cv;

	if (items < 2)
		XSRETURN_IV(-1);

	chat_name = SvPV(ST(0), notused);
	user = SvPV(ST(1), notused);

	if (items > 2)
		reason = SvPV(ST(2), notused);

	if (chat_name == NULL || user == NULL)
		XSRETURN_IV(-1);

	if (reason == NULL)
		reason = "";

	if (items == 4) {
		u_int32_t acct_refnum = SvIV(ST(3));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	chat = chat_find(acct, chat_name);
	if (chat == NULL)
		XSRETURN_IV(-1);

	XSRETURN_IV(chat_kick(acct, chat, user, reason));
}

XS(PORK_chat_topic) {
	char *chat_name;
	char *topic;
	size_t notused;
	struct pork_acct *acct;
	struct chatroom *chat;
	dXSARGS;

	(void) cv;

	if (items != 2 && items != 3)
		XSRETURN_IV(-1);

	chat_name = SvPV(ST(0), notused);
	topic = SvPV(ST(1), notused);

	if (chat_name == NULL)
		XSRETURN_IV(-1);

	if (items == 3) {
		u_int32_t acct_refnum = SvIV(ST(2));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	chat = chat_find(acct, chat_name);
	if (chat == NULL)
		XSRETURN_IV(-1);

	if (topic == NULL || *topic == '\0') {
		if (chat->topic != NULL)
			XSRETURN_PV(chat->topic);
		else
			XSRETURN_UNDEF;
	}

	XSRETURN_IV(chat_set_topic(acct, chat, topic));
}

XS(PORK_chat_get_list) {
	dXSARGS;
	dlist_t *cur;
	struct pork_acct *acct;
	u_int32_t i = 0;

	(void) cv;

	if (items == 1) {
		u_int32_t acct_refnum = SvIV(ST(0));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	SP -= items;
	for (cur = acct->chat_list ; cur != NULL ; cur = cur->next) {
		struct chatroom *chat = cur->data;

		XPUSHs(sv_2mortal(newSVpv(chat->title, 0)));
		i++;
	}

	XSRETURN(i);
}

XS(PORK_chat_get_window) {
	char *chat_name;
	size_t notused;
	struct pork_acct *acct;
	struct imwindow *win;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	chat_name = SvPV(ST(0), notused);
	if (chat_name == NULL)
		XSRETURN_IV(-1);

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	win = imwindow_find_chat_target(acct, chat_name);
	if (win == NULL)
		XSRETURN_IV(-1);

	XSRETURN_IV(win->refnum);
}

XS(PORK_chat_get_users) {
	char *chat_name;
	size_t notused;
	dlist_t *node;
	struct pork_acct *acct;
	struct chatroom *chat;
	u_int32_t flags = 0;
	u_int32_t args = 0;
	u_int32_t invert = 0;
	dXSARGS;

	(void) cv;

	if (items < 3)
		XSRETURN_EMPTY;

	chat_name = SvPV(ST(0), notused);
	if (chat_name == NULL)
		XSRETURN_EMPTY;

	flags = SvIV(ST(1));
	invert = SvIV(ST(2));

	if (items == 4) {
		u_int32_t acct_refnum = SvIV(ST(3));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_EMPTY;
	} else
		acct = cur_window()->owner;

	chat = chat_find(acct, chat_name);
	if (chat == NULL)
		XSRETURN_EMPTY;

	SP -= items;
	for (node = chat->user_list ; node != NULL ; node = node->next) {
		struct chat_user *user = node->data;

		if (!flags || (!!(user->status & flags)) ^ invert) {
			XPUSHs(sv_2mortal(newSVpv(user->nname, 0)));
			args++;
		}
	}

	XSRETURN(args);
}

XS(PORK_chat_ignore) {
	char *name;
	char *dest;
	size_t notused;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 2 && items != 3)
		XSRETURN_IV(-1);

	name = SvPV(ST(0), notused);
	dest = SvPV(ST(1), notused);

	if (name == NULL || dest == NULL)
		XSRETURN_IV(-1);

	if (items == 3) {
		u_int32_t acct_refnum = SvIV(ST(2));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	XSRETURN_IV(chat_ignore(acct, name, dest));
}

XS(PORK_chat_invite) {
	char *name;
	char *dest;
	char *msg;
	size_t notused;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 3 && items != 4)
		XSRETURN_IV(-1);

	name = SvPV(ST(0), notused);
	dest = SvPV(ST(1), notused);
	msg = SvPV(ST(2), notused);

	if (name == NULL || dest == NULL)
		XSRETURN_IV(-1);

	if (items == 4) {
		u_int32_t acct_refnum = SvIV(ST(3));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	if (!acct->connected)
		XSRETURN_IV(-1);

	XSRETURN_IV(chat_invite(acct, name, dest, msg));
}

XS(PORK_chat_join) {
	char *chat_name;
	size_t notused;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	chat_name = SvPV(ST(0), notused);
	if (chat_name == NULL)
		XSRETURN_IV(-1);

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	XSRETURN_IV(chat_join(acct, chat_name));
}

XS(PORK_chat_leave) {
	char *chat_name;
	size_t notused;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 1 && items != 2)
		XSRETURN_IV(-1);

	chat_name = SvPV(ST(0), notused);
	if (chat_name == NULL)
		XSRETURN_IV(-1);

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	XSRETURN_IV(chat_leave(acct, chat_name, 0));
}

XS(PORK_chat_send) {
	struct pork_acct *acct;
	char *msg;
	char *chat_name;
	size_t notused;
	struct chatroom *chat;
	dXSARGS;

	(void) cv;

	if (items != 2 && items != 3)
		XSRETURN_IV(-1);

	chat_name = SvPV(ST(0), notused);
	msg = SvPV(ST(1), notused);

	if (chat_name == NULL || msg == NULL)
		XSRETURN_IV(-1);

	if (items == 3) {
		u_int32_t acct_refnum = SvIV(ST(2));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	chat = chat_find(acct, chat_name);
	if (chat == NULL)
		XSRETURN_IV(-1);

	XSRETURN_IV(chat_send_msg(acct, chat, chat_name, msg));
}

XS(PORK_chat_unignore) {
	char *name;
	char *dest;
	size_t notused;
	struct pork_acct *acct;
	dXSARGS;

	(void) cv;

	if (items != 2 && items != 3)
		XSRETURN_IV(-1);

	name = SvPV(ST(0), notused);
	dest = SvPV(ST(1), notused);

	if (name == NULL || dest == NULL)
		XSRETURN_IV(-1);

	if (items == 3) {
		u_int32_t acct_refnum = SvIV(ST(2));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_IV(-1);
	} else
		acct = cur_window()->owner;

	XSRETURN_IV(chat_unignore(acct, name, dest));
}

XS(PORK_chat_target) {
	char *name = NULL;
	size_t notused;
	struct pork_acct *acct;
	struct chatroom *chat;
	dXSARGS;

	(void) cv;

	if (items > 0) {
		name = SvPV(ST(0), notused);
		if (name == NULL)
			XSRETURN_UNDEF;
	} else {
		if (cur_window()->type != WIN_TYPE_CHAT)
			XSRETURN_UNDEF;

		chat = cur_window()->data;
		XSRETURN_PV(chat->title);
	}

	if (items == 2) {
		u_int32_t acct_refnum = SvIV(ST(1));

		acct = pork_acct_get_data(acct_refnum);
		if (acct == NULL)
			XSRETURN_UNDEF;
	} else
		acct = cur_window()->owner;

	chat = chat_find(acct, name);
	if (chat == NULL)
		XSRETURN_UNDEF;

	XSRETURN_PV(chat->title);
}
