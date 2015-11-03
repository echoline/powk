/*
** pork_conf.c - pork's configuration parser.
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
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <pork.h>
#include <pork_missing.h>
#include <pork_util.h>
#include <pork_list.h>
#include <pork_buddy.h>
#include <pork_imwindow.h>
#include <pork_acct.h>
#include <pork_cstr.h>
#include <pork_misc.h>
#include <pork_bind.h>
#include <pork_alias.h>
#include <pork_screen_io.h>
#include <pork_screen.h>
#include <pork_command.h>
#include <pork_conf.h>

static int pork_mkdir(const char *path) {
	struct stat st;

	if (stat(path, &st) != 0) {
		if (mkdir(path, 0700) != 0) {
			screen_err_msg("Error: mkdir %s: %s", path, strerror(errno));
			return (-1);
		}
	} else {
		if (!S_ISDIR(st.st_mode)) {
			screen_err_msg("Error: %s is not a directory", path);
			return (-1);
		}
	}

	return (0);
}

int read_conf(const char *path) {
	FILE *fp;
	char buf[4096];
	u_int32_t line = 0;

	fp = fopen(path, "r");
	if (fp == NULL) {
		if (errno != -ENOENT)
			debug("fopen: %s: %s", path, strerror(errno));
		return (-1);
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		char *p;

		++line;

		p = strchr(buf, '\n');
		if (p == NULL) {
			debug("line %u too long", line);
			fclose(fp);
			return (-1);
		}

		*p = '\0';

		while ((p = strchr(p, '\t')) != NULL)
			*p++ = ' ';

		p = buf;
		if (*p == '#')
			continue;

		if (*p == opt_get_char(OPT_CMDCHARS))
			p++;

		if (!blank_str(p))
			run_command(p);
	}

	fclose(fp);
	return (0);
}

static int read_acct_conf(struct pork_acct *acct, const char *filename) {
	FILE *fp;
	char buf[2048];
	int line = 0;

	fp = fopen(filename, "r");
	if (fp == NULL) {
		if (errno != ENOENT) {
			debug("fopen: %s: %s", filename, strerror(errno));
			return (-1);
		}

		return (0);
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		char *p;

		++line;

		p = strchr(buf, '\n');
		if (p == NULL) {
			debug("line %u too long", line);
			fclose(fp);
			return (-1);
		}

		*p = '\0';

		while ((p = strchr(p, '\t')) != NULL)
			*p++ = ' ';

		p = buf;
		if (*p == '#')
			continue;

		p = strchr(buf, ':');
		if (p == NULL)
			continue;
		*p++ = '\0';

		while (*p == ' ')
			p++;

		if (!strcasecmp(buf, "username")) {
			free(acct->username);
			acct->username = xstrdup(p);
		} else if (!strcasecmp(buf, "password")) {
			if (acct->passwd != NULL) {
				memset(acct->passwd, 0, strlen(acct->passwd));
				free(acct->passwd);
			}

			acct->passwd = xstrdup(p);
		} else if (!strcasecmp(buf, "profile")) {
			free(acct->profile);
			acct->profile = xstrdup(p);
		} else {
			screen_err_msg("Error: account config line %d: bad setting: %s",
				line, buf);
		}
	}

	fclose(fp);
	return (0);
}

static int read_buddy_list(struct pork_acct *acct, const char *filename) {
	FILE *fp;
	char buf[1024];
	u_int32_t line = 0;
	struct bgroup *cur_group = NULL;
	struct buddy *cur_buddy = NULL;
	struct buddy_pref *pref = acct->buddy_pref;

	fp = fopen(filename, "r");
	if (fp == NULL) {
		if (errno != ENOENT) {
			screen_err_msg("Can't open buddy list: %s: %s",
				filename, strerror(errno));
			return (-1);
		}

		return (0);
	}

	acct->report_idle = opt_get_bool(OPT_REPORT_IDLE);
	pref->privacy_mode = 1;

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		char *p;
		char type;

		++line;
		p = strchr(buf, '\n');
		if (p == NULL) {
			screen_err_msg("Invalid buddy list data at line %u", line);
			fclose(fp);
			return (-1);
		}

		*p = '\0';
		p = buf;

		type = *p++;

		while (*p == ' ' || *p == '\t')
			p++;

		if (type == 'g') {
			cur_buddy = NULL;
			cur_group = group_add(acct, p);
		} else if (type == 'b') {
			char *alias;
			struct buddy *buddy;

			if (cur_group == NULL) {
				screen_err_msg("Invalid buddy list data at line %u", line);
				fclose(fp);
				return (-1);
			}

			alias = strchr(p, ':');
			if (alias != NULL)
				*alias++ = '\0';

			buddy = buddy_add(acct, p, cur_group, 0);
			if (alias != NULL)
				buddy_alias(acct, buddy, alias, 0);

			cur_buddy = buddy;
		} else if (type == 'l') {
			u_int32_t last_seen;

			if (str_to_uint(p, &last_seen) != 0 || cur_buddy == NULL) {
				screen_err_msg("Invalid buddy list data at line %u", line);
				fclose(fp);
				return (-1);
			}

			cur_buddy->last_seen = last_seen;
		} else if (type == 'p') {
			buddy_add_permit(acct, p, 0);

			cur_buddy = NULL;
			cur_group = NULL;
		} else if (type == 'd') {
			buddy_add_block(acct, p, 0);

			cur_buddy = NULL;
			cur_group = NULL;
		} else if (type == 'm') {
			u_int32_t privacy_mode;

			if (str_to_uint(p, &privacy_mode) != 0 || privacy_mode > 5) {
				screen_err_msg(
					"%s: Invalid value for permit/deny mode at line %u",
					p, line);
				fclose(fp);
				return (-1);
			}

			pref->privacy_mode = privacy_mode;

			cur_buddy = NULL;
			cur_group = NULL;
		} else if (type == 'i') {
			u_int32_t report_idle_val;

			if (str_to_uint(p, &report_idle_val) != 0 || report_idle_val > 1) {
				screen_err_msg(
					"Invalid value for report idle mode at line %u", line);
				fclose(fp);
				return (-1);
			}

			acct->report_idle = report_idle_val;

			cur_buddy = NULL;
			cur_group = NULL;
		} else {
			screen_err_msg("Invalid buddy list data at line %u", line);
			fclose(fp);
			return (-1);
		}
	}

	fclose(fp);
	return (0);
}

int read_user_config(struct pork_acct *acct) {
	char nname[NUSER_LEN];
	char buf[PATH_MAX];
	char *pork_dir = opt_get_str(OPT_PORK_DIR);

	if (acct == NULL || pork_dir == NULL)
		return (-1);

	normalize(nname, acct->username, sizeof(nname));

	snprintf(buf, sizeof(buf), "%s/%s", pork_dir, nname);
	if (pork_mkdir(buf) != 0)
		return (-1);

	snprintf(buf, sizeof(buf), "%s/%s/logs", pork_dir, nname);
	if (pork_mkdir(buf) != 0)
		return (-1);

	snprintf(buf, sizeof(buf), "%s/%s/buddy_list", pork_dir, nname);
	if (read_buddy_list(acct, buf) != 0)
		screen_err_msg("There was an error reading your buddy list");

	snprintf(buf, sizeof(buf), "%s/%s/porkrc", pork_dir, nname);
	if (read_conf(buf) != 0 && errno != ENOENT)
		screen_err_msg("There was an error reading your porkrc file");

	snprintf(buf, sizeof(buf), "%s/%s/account", pork_dir, nname);
	if (read_acct_conf(acct, buf) != 0)
		screen_err_msg("Error: Can't read account config file, %s", buf);

	return (0);
}

static int save_buddy_list(struct pork_acct *acct, const char *filename) {
	char *fn;
	size_t len;
	FILE *fp;
	dlist_t *gcur;
	struct buddy_pref *pref = acct->buddy_pref;

	len = strlen(filename) + sizeof("-TEMP");
	fn = xmalloc(len);
	snprintf(fn, len, "%s-TEMP", filename);

	fp = fopen(fn, "w");
	if (fp == NULL) {
		screen_err_msg("Can't open buddy list file for writing: %s",
			strerror(errno));
		free(fn);
		return (-1);
	}

	fprintf(fp, "m %u\n", pref->privacy_mode);
	fprintf(fp, "i %u\n", acct->report_idle);

	for (gcur = pref->group_list ; gcur != NULL ; gcur = gcur->next) {
		struct bgroup *gr = gcur->data;
		dlist_t *bcur;

		fprintf(fp, "g %s\n", gr->name);

		for (bcur = gr->member_list ; bcur != NULL ; bcur = bcur->next) {
			struct buddy *buddy = bcur->data;

			fprintf(fp, "b %s:%s\n", buddy->nname, buddy->name);

			if (buddy->last_seen != 0)
				fprintf(fp, "l %u\n", buddy->last_seen);
		}
	}

	for (gcur = pref->permit_list ; gcur != NULL ; gcur = gcur->next)
		fprintf(fp, "p %s\n", (char *) gcur->data);

	for (gcur = pref->block_list ; gcur != NULL ; gcur = gcur->next)
		fprintf(fp, "d %s\n", (char *) gcur->data);

	fchmod(fileno(fp), 0600);
	fclose(fp);

	if (rename(fn, filename) != 0) {
		debug("rename: %s<=>%s: %s", fn, filename, strerror(errno));
		unlink(fn);
		free(fn);
		return (-1);
	}

	free(fn);
	return (0);
}

static int save_acct_conf(struct pork_acct *acct, char *filename) {
	char *fn;
	size_t len;
	FILE *fp;

	len = strlen(filename) + sizeof("-TEMP");
	fn = xmalloc(len);
	snprintf(fn, len, "%s-TEMP", filename);

	fp = fopen(fn, "w");
	if (fp == NULL) {
		debug("fopen: %s: %s", fn, strerror(errno));
		free(fn);
		return (-1);
	}

	if (acct->username != NULL)
		fprintf(fp, "username: %s\n", acct->username);
	if (acct->profile != NULL)
		fprintf(fp, "profile: %s\n", acct->profile);
	if (opt_get_bool(OPT_SAVE_PASSWD) && acct->passwd != NULL)
		fprintf(fp, "password: %s\n", acct->passwd);

	fchmod(fileno(fp), 0600);
	fclose(fp);

	if (rename(fn, filename) != 0) {
		debug("rename: %s<=>%s: %s", fn, filename, strerror(errno));
		unlink(fn);
		free(fn);
		return (-1);
	}

	free(fn);
	return (0);
}

int save_user_config(struct pork_acct *acct) {
	char nname[NUSER_LEN];
	char buf[PATH_MAX];
	char *pork_dir = opt_get_str(OPT_PORK_DIR);

	if (acct == NULL || pork_dir == NULL) {
		debug("acct=%p port_dir=%p", acct, pork_dir);
		return (-1);
	}

	normalize(nname, acct->username, sizeof(nname));

	snprintf(buf, sizeof(buf), "%s/%s/buddy_list", pork_dir, nname);
	if (save_buddy_list(acct, buf) != 0)
		screen_err_msg("There was an error writing your buddy list.");

	snprintf(buf, sizeof(buf), "%s/%s/account", pork_dir, nname);
	if (save_acct_conf(acct, buf) != 0)
		screen_err_msg("Error: Can't write account config file, %s.", buf);

	return (0);
}

int read_global_config(void) {
	struct passwd *pw;
	char *pork_dir;
	char buf[PATH_MAX];

	if (read_conf(SYSTEM_PORKRC) != 0)
		screen_err_msg("Error reading the system-wide porkrc file");

	pw = getpwuid(getuid());
	if (pw == NULL) {
		debug("getpwuid: %s", strerror(errno));
		return (-1);
	}

	pork_dir = opt_get_str(OPT_PORK_DIR);
	if (pork_dir == NULL) {
		snprintf(buf, sizeof(buf), "%s/.pork", pw->pw_dir);
		opt_set(OPT_PORK_DIR, buf);

		pork_dir = opt_get_str(OPT_PORK_DIR);
	} else
		xstrncpy(buf, pork_dir, sizeof(buf));

	if (pork_mkdir(buf) != 0)
		return (-1);

	pork_dir = opt_get_str(OPT_PORK_DIR);

	snprintf(buf, sizeof(buf), "%s/porkrc", pork_dir);
	if (read_conf(buf) != 0 && errno != ENOENT)
		return (-1);

	return (0);
}

static void write_alias_line(void *data, void *filep) {
	struct alias *alias = data;
	FILE *fp = filep;

	fprintf(fp, "alias %s %s%s\n",
		alias->alias, alias->orig, (alias->args ? alias->args : ""));
}

static void write_bind_line(void *data, void *filep) {
	struct binding *binding = data;
	FILE *fp = filep;
	char key_name[32];

	bind_get_keyname(binding->key, key_name, sizeof(key_name));
	fprintf(fp, "bind %s %s\n", key_name, binding->binding);
}

static void write_bind_blist_line(void *data, void *filep) {
	struct binding *binding = data;
	FILE *fp = filep;
	char key_name[32];

	bind_get_keyname(binding->key, key_name, sizeof(key_name));
	fprintf(fp, "bind -buddy %s %s\n", key_name, binding->binding);
}

int save_global_config(void) {
	char porkrc[PATH_MAX];
	char *fn;
	size_t len;
	FILE *fp;
	char *pork_dir = opt_get_str(OPT_PORK_DIR);

	if (pork_dir == NULL)
		return (-1);

	snprintf(porkrc, sizeof(porkrc), "%s/porkrc", pork_dir);

	len = strlen(porkrc) + sizeof("-TEMP");
	fn = xmalloc(len);
	snprintf(fn, len, "%s-TEMP", porkrc);

	fp = fopen(fn, "w");
	if (fp == NULL) {
		debug("fopen: %s: %s", fn, strerror(errno));
		return (-1);
	}

	opt_write(fp);
	fprintf(fp, "\n");
	hash_iterate(&screen.alias_hash, write_alias_line, fp);
	fprintf(fp, "\n");
	hash_iterate(&screen.binds.main.hash, write_bind_line, fp);
	fprintf(fp, "\n");
	hash_iterate(&screen.binds.blist.hash, write_bind_blist_line, fp);

	fclose(fp);

	if (rename(fn, porkrc) != 0) {
		debug("rename: %s<=>%s: %s", fn, porkrc, strerror(errno));
		unlink(fn);
		free(fn);
		return (-1);
	}

	free(fn);
	return (0);
}
