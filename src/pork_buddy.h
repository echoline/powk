/*
** pork_buddy.h - Buddy list / permit / deny management.
** Copyright (C) 2002-2005 Ryan McCabe <ryan@numb.org>
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License, version 2,
** as published by the Free Software Foundation.
*/

#ifndef __PORK_BUDDY_H
#define __PORK_BUDDY_H

enum {
	STATUS_OFFLINE,
	STATUS_ACTIVE,
	STATUS_IDLE,
	STATUS_AWAY
};

struct pork_acct;

struct buddy_pref {
	dlist_t *group_list;
	dlist_t *permit_list;
	dlist_t *block_list;
	u_int32_t next_group_refnum;
	u_int16_t privacy_mode;
	u_int32_t num_buddies;
	hash_t buddy_hash;
};

struct bgroup {
	char *name;
	u_int32_t refnum;
	u_int32_t num_online;
	u_int32_t num_members;
	u_int32_t high_refnum;
	dlist_t *member_list;
	dlist_t *blist_line;
};

struct buddy {
	char *name;
	char *nname;
	char *userhost;
	struct bgroup *group;
	u_int32_t refnum;
	u_int32_t signon_time;
	u_int32_t idle_time;
	u_int32_t warn_level;
	u_int32_t status:2;
	u_int32_t notify:1;
	u_int32_t ignore:1;
	u_int32_t type:6;
	u_int32_t last_seen;
	dlist_t *blist_line;
};

int buddy_update(	struct pork_acct *acct,
					struct buddy *buddy,
					void *data);

void buddy_online(	struct pork_acct *acct,
					struct buddy *buddy,
					void *data);

void buddy_offline(struct pork_acct *acct, struct buddy *buddy);
void buddy_offline_all(struct pork_acct *acct);
char *buddy_name(struct pork_acct *acct, char *buddy);

int buddy_alias(struct pork_acct *acct,
				struct buddy *buddy,
				char *alias,
				int send);

struct bgroup *group_find(struct pork_acct *acct, char *group_name);
struct buddy *buddy_find(struct pork_acct *acct, const char *screen_name);

void buddy_update_idle(struct pork_acct *acct);

struct buddy *buddy_add(struct pork_acct *acct,
						const char *screen_name,
						struct bgroup *group,
						int send);

int buddy_init(struct pork_acct *acct);
void buddy_destroy(struct pork_acct *acct);

struct bgroup *group_add(struct pork_acct *acct, char *group_name);

int group_remove(struct pork_acct *acct, char *group_name, int send);
int buddy_remove(struct pork_acct *acct, const char *screen_name, int send);
int buddy_add_block(struct pork_acct *acct, char *screen_name, int send);
int buddy_remove_block(struct pork_acct *acct, char *screen_name, int send);
int buddy_add_permit(struct pork_acct *acct, char *screen_name, int send);
int buddy_remove_permit(struct pork_acct *acct, char *screen_name, int send);

int buddy_clear_block(struct pork_acct *acct);
int buddy_clear_permit(struct pork_acct *acct);

int buddy_came_online(struct pork_acct *acct, char *user, void *data);
int buddy_went_offline(struct pork_acct *acct, char *user);
int buddy_went_idle(struct pork_acct *acct, char *user, u_int32_t seconds);
int buddy_went_unidle(struct pork_acct *acct, char *user);
int buddy_went_away(struct pork_acct *acct, char *user);
int buddy_went_unaway(struct pork_acct *acct, char *user);

#endif
