/*
** pork_buddy.c - Buddy list / permit / deny management.
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
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include <pork.h>
#include <pork_missing.h>
#include <pork_util.h>
#include <pork_list.h>
#include <pork_acct.h>
#include <pork_proto.h>
#include <pork_events.h>
#include <pork_imsg.h>
#include <pork_imwindow.h>
#include <pork_buddy.h>
#include <pork_buddy_list.h>
#include <pork_misc.h>
#include <pork_set.h>
#include <pork_screen.h>
#include <pork_screen_io.h>

static int buddy_hash_compare(void *l, void *r) {
	char *str = l;
	struct buddy *buddy = r;

	return (strcasecmp(str, buddy->nname));
}

static void buddy_hash_cleanup(void *param __notused, void *data) {
	struct buddy *buddy = data;

	free(buddy->nname);
	free(buddy->name);
	free(buddy->userhost);
	free(buddy);
}

static void group_cleanup(void *param __notused, void *data) {
	struct bgroup *group = data;

	/*
	** The memory allocated for the buddies in the member_list of
	** each group is freed when the buddy hash is destroyed, so
	** we don't have to do it here.
	*/

	dlist_destroy(group->member_list, NULL, NULL);
	free(group->name);
	free(group);
}

static void generic_cleanup(void *param __notused, void *data) {
	free(data);
}

static int buddy_rights_compare(void *l, void *r) {
	char *str1 = l;
	char *str2 = r;

	return (strcasecmp(str1, str2));
}

static int group_compare(void *l, void *r) {
	char *name = l;
	struct bgroup *gr = r;

	return (strcasecmp(name, gr->name));
}

static int buddy_hash_remove(struct pork_acct *acct, const char *screen_name) {
	struct buddy_pref *pref = acct->buddy_pref;
	char buddy[NUSER_LEN];
	u_int32_t hash;

	acct->proto->normalize(buddy, screen_name, sizeof(buddy));
	hash = string_hash(buddy, pref->buddy_hash.order);

	return (hash_remove(&pref->buddy_hash, buddy, hash));
}

int buddy_remove(struct pork_acct *acct, const char *screen_name, int send) {
	struct buddy *buddy;
	dlist_t *list_node;
	char nname[NUSER_LEN];

	if (acct->proto->buddy_remove == NULL)
		return (-1);

	acct->proto->normalize(nname, screen_name, sizeof(nname));
	buddy = buddy_find(acct, nname);
	if (buddy == NULL)
		return (-1);

	list_node = dlist_find(buddy->group->member_list,
					buddy->nname, buddy_hash_compare);

	if (list_node == NULL) {
		debug("buddy list node is NULL");
		return (-1);
	}

	if (buddy->status != STATUS_OFFLINE)
		buddy_offline(acct, buddy);

	if (send)
		acct->proto->buddy_remove(acct, buddy);

	buddy->group->member_list = dlist_remove(buddy->group->member_list,
									list_node);
	buddy->group->num_members--;
	acct->buddy_pref->num_buddies--;

	return (buddy_hash_remove(acct, buddy->name));
}

/*
** Add the buddy whose screen name is "screen_name" to the specified
** group.
*/

struct buddy *buddy_add(struct pork_acct *acct,
						const char *screen_name,
						struct bgroup *group,
						int send)
{
	struct buddy *buddy;
	char nname[NUSER_LEN];
	struct buddy_pref *pref = acct->buddy_pref;

	if (acct->proto->buddy_add == NULL)
		return (NULL);

	acct->proto->normalize(nname, screen_name, sizeof(nname));
	if (buddy_find(acct, nname) != NULL)
		return (NULL);

	buddy = xcalloc(1, sizeof(*buddy));
	buddy->name = xstrdup(screen_name);
	buddy->nname = xstrdup(nname);
	buddy->group = group;
	buddy->refnum = group->high_refnum++;
	pref->num_buddies++;

	group->num_members++;
	group->member_list = dlist_add_tail(group->member_list, buddy);

	hash_add(&pref->buddy_hash, buddy,
		string_hash(buddy->nname, pref->buddy_hash.order));

	if (send)
		acct->proto->buddy_add(acct, buddy);

	return (buddy);
}

struct bgroup *group_find(struct pork_acct *acct, char *group_name) {
	dlist_t *node;
	struct buddy_pref *pref = acct->buddy_pref;

	node = dlist_find(pref->group_list, group_name, group_compare);
	if (node == NULL)
		return (NULL);

	return (node->data);
}

struct bgroup *group_add(struct pork_acct *acct, char *group_name) {
	struct bgroup *group;
	struct buddy_pref *pref = acct->buddy_pref;

	group = group_find(acct, group_name);
	if (group != NULL)
		return (group);

	group = xcalloc(1, sizeof(*group));
	group->name = xstrdup(group_name);
	group->refnum = pref->next_group_refnum++;
	pref->group_list = dlist_add_tail(pref->group_list, group);

	blist_add_group(acct->blist, group);

	return (group);
}

int group_remove(struct pork_acct *acct, char *group_name, int send) {
	struct bgroup *group;
	dlist_t *memb_list;
	dlist_t *node;
	struct buddy_pref *pref = acct->buddy_pref;

	node = dlist_find(pref->group_list, group_name, group_compare);
	if (node == NULL)
		return (-1);

	group = node->data;
	memb_list = group->member_list;

	while (memb_list != NULL) {
		dlist_t *next = memb_list->next;
		struct buddy *buddy = memb_list->data;

		buddy_remove(acct, buddy->name, send);
		memb_list = next;
	}

	pref->group_list = dlist_remove(pref->group_list, node);
	blist_del_group(acct->blist, group);

	return (0);
}

struct buddy *buddy_find(struct pork_acct *acct, const char *screen_name) {
	dlist_t *node;
	char buddy[NUSER_LEN];
	u_int32_t hash;
	struct buddy_pref *pref = acct->buddy_pref;

	if (pref == NULL)
		return (NULL);

	acct->proto->normalize(buddy, screen_name, sizeof(buddy));
	hash = string_hash(buddy, pref->buddy_hash.order);

	node = hash_find(&pref->buddy_hash, buddy, hash);
	if (node != NULL)
		return (node->data);

	return (NULL);
}

int buddy_alias(struct pork_acct *acct,
				struct buddy *buddy,
				char *alias,
				int send)
{
	if (!strcmp(buddy->name, alias))
		return (0);

	if (acct->proto->buddy_alias == NULL)
		return (-1);

	free(buddy->name);
	buddy->name = xstrdup(alias);

	blist_update_label(acct->blist, buddy->blist_line);

	if (send)
		return (acct->proto->buddy_alias(acct, buddy));

	return (0);
}

char *buddy_name(struct pork_acct *acct, char *name) {
	struct buddy *buddy;

	buddy = buddy_find(acct, name);
	if (buddy != NULL)
		return (buddy->name);

	return (name);
}

int buddy_clear_block(struct pork_acct *acct) {
	struct buddy_pref *pref = acct->buddy_pref;
	dlist_t *cur = pref->block_list;

	if (acct->proto->buddy_unblock == NULL)
		return (-1);

	while (cur != NULL) {
		dlist_t *next = cur->next;

		acct->proto->buddy_unblock(acct, cur->data);
		free(cur->data);
		free(cur);

		cur = next;
	}

	pref->block_list = NULL;
	return (0);
}

int buddy_clear_permit(struct pork_acct *acct) {
	struct buddy_pref *pref = acct->buddy_pref;
	dlist_t *cur = pref->permit_list;

	if (acct->proto->buddy_remove_permit == NULL)
		return (-1);

	while (cur != NULL) {
		dlist_t *next = cur->next;

		acct->proto->buddy_remove_permit(acct, cur->data);
		free(cur->data);
		free(cur);

		cur = next;
	}

	pref->permit_list = NULL;
	return (0);
}

int buddy_init(struct pork_acct *acct) {
	struct buddy_pref *pref;

	if (acct->buddy_pref != NULL)
		return (-1);

	pref = xcalloc(1, sizeof(*pref));
	acct->buddy_pref = pref;

	hash_init(&pref->buddy_hash, 5, buddy_hash_compare, buddy_hash_cleanup);
	return (0);
}

void buddy_destroy(struct pork_acct *acct) {
	struct buddy_pref *pref = acct->buddy_pref;

	if (pref != NULL) {
		hash_destroy(&pref->buddy_hash);

		dlist_destroy(pref->group_list, NULL, group_cleanup);
		dlist_destroy(pref->permit_list, NULL, generic_cleanup);
		dlist_destroy(pref->block_list, NULL, generic_cleanup);

		pref->group_list = NULL;
		pref->permit_list = NULL;
		pref->block_list = NULL;
		acct->buddy_pref = NULL;

		free(pref);
	}
}

int buddy_update(	struct pork_acct *acct,
					struct buddy *buddy,
					void *data)
{
	int ret;

	if (acct->proto->buddy_update == NULL)
		return (-1);

	ret = acct->proto->buddy_update(acct, buddy, data);
	blist_update_label(acct->blist, buddy->blist_line);

	return (ret);
}

void buddy_online(	struct pork_acct *acct,
					struct buddy *buddy,
					void *data)
{
	buddy->last_seen = time(NULL);
	buddy->group->num_online++;
	buddy_update(acct, buddy, data);
	blist_add(acct->blist, buddy);
}

void buddy_offline(struct pork_acct *acct, struct buddy *buddy) {
	buddy->last_seen = time(NULL);
	buddy->status = STATUS_OFFLINE;
	buddy->signon_time = 0;
	buddy->idle_time = 0;
	buddy->warn_level = 0;
	buddy->group->num_online--;

	blist_del(acct->blist, buddy);
}

void buddy_offline_all(struct pork_acct *acct) {
	struct buddy_pref *pref = acct->buddy_pref;
	dlist_t *gcur;

	gcur = pref->group_list;
	while (gcur != NULL) {
		struct bgroup *gr = gcur->data;
		dlist_t *bcur = gr->member_list;

		while (bcur != NULL) {
			struct buddy *buddy = bcur->data;

			if (buddy->status != STATUS_OFFLINE)
				buddy_offline(acct, bcur->data);

			bcur = bcur->next;
		}

		gcur = gcur->next;
	}
}

int buddy_add_block(struct pork_acct *acct, char *screen_name, int send) {
	dlist_t *node;
	char nname[NUSER_LEN];
	struct buddy_pref *pref = acct->buddy_pref;

	if (acct->proto->buddy_block == NULL)
		return (-1);

	acct->proto->normalize(nname, screen_name, sizeof(nname));

	node = dlist_find(pref->block_list, nname, buddy_rights_compare);
	if (node != NULL)
		return (-1);

	pref->block_list = dlist_add_tail(pref->block_list, xstrdup(nname));

	if (send)
		return (acct->proto->buddy_block(acct, nname));

	return (0);
}

int buddy_remove_block(struct pork_acct *acct, char *screen_name, int send) {
	dlist_t *node;
	char nname[NUSER_LEN];
	struct buddy_pref *pref = acct->buddy_pref;
	int ret = 0;

	if (acct->proto->buddy_unblock == NULL)
		return (-1);

	acct->proto->normalize(nname, screen_name, sizeof(nname));

	node = dlist_find(pref->block_list, nname, buddy_rights_compare);
	if (node == NULL)
		return (-1);

	if (send)
		ret = acct->proto->buddy_unblock(acct, nname);

	free(node->data);
	pref->block_list = dlist_remove(pref->block_list, node);
	return (ret);
}

int buddy_add_permit(struct pork_acct *acct, char *screen_name, int send) {
	dlist_t *node;
	char nname[NUSER_LEN];
	struct buddy_pref *pref = acct->buddy_pref;
	int ret = 0;

	if (acct->proto->buddy_permit == NULL)
		return (-1);

	acct->proto->normalize(nname, screen_name, sizeof(nname));

	node = dlist_find(pref->permit_list, nname, buddy_rights_compare);
	if (node != NULL)
		return (-1);

	if (send)
		ret = acct->proto->buddy_permit(acct, nname);

	pref->permit_list = dlist_add_tail(pref->permit_list, xstrdup(nname));
	return (ret);
}

int buddy_remove_permit(struct pork_acct *acct, char *screen_name, int send) {
	dlist_t *node;
	char nname[NUSER_LEN];
	struct buddy_pref *pref = acct->buddy_pref;
	int ret = 0;

	if (acct->proto->buddy_remove_permit == NULL)
		return (-1);

	acct->proto->normalize(nname, screen_name, sizeof(nname));

	node = dlist_find(pref->permit_list, nname, buddy_rights_compare);
	if (node == NULL)
		return (-1);

	if (send)
		ret = acct->proto->buddy_remove_permit(acct, nname);

	free(node->data);
	pref->permit_list = dlist_remove(pref->permit_list, node);
	return (ret);
}

void buddy_update_idle(struct pork_acct *acct) {
	struct buddy_pref *pref = acct->buddy_pref;
	dlist_t *gcur;

	gcur = pref->group_list;
	while (gcur != NULL) {
		struct bgroup *gr = gcur->data;
		dlist_t *bcur = gr->member_list;

		while (bcur != NULL) {
			struct buddy *buddy = bcur->data;

			if (buddy->idle_time > 0) {
				buddy->idle_time++;
				blist_update_label(acct->blist, buddy->blist_line);
			}

			bcur = bcur->next;
		}

		gcur = gcur->next;
	}
}

int buddy_went_offline(struct pork_acct *acct, char *user) {
	struct buddy *buddy;
	char *name = user;
	int notify = 0;

	buddy = buddy_find(acct, user);
	if (buddy != NULL) {
		/*
		** This happens with AIM sometimes. I don't know why.
		*/
		if (buddy->status == STATUS_OFFLINE)
			return (0);

		if (buddy->notify)
			notify = 1;

		name = buddy->name;
		buddy_offline(acct, buddy);
	}

	if (event_generate(acct->events, EVENT_BUDDY_SIGNOFF, user, acct->refnum))
		return (1);

	if (opt_get_bool(OPT_SHOW_BUDDY_SIGNOFF)) {
		struct imwindow *win;

		win = imwindow_find(acct, user);
		if (win != NULL) {
			win->typing = 0;
			screen_win_msg(win, 1, 1, 0,
				MSG_TYPE_SIGNOFF, "%s has signed off", name);
		} else if (notify) {
			screen_win_msg(screen.status_win, 1, 1, 0,
				MSG_TYPE_SIGNOFF, "%s has signed off", name);
		}
	}

	return (0);
}

int buddy_came_online(struct pork_acct *acct, char *user, void *data) {
	struct buddy *buddy;
	char *name = user;
	int notify = 0;

	buddy = buddy_find(acct, user);
	if (buddy != NULL) {
		if (buddy->notify)
			notify = 1;

		name = buddy->name;
		buddy_online(acct, buddy, data);
	}

	if (event_generate(acct->events, EVENT_BUDDY_SIGNON, user, acct->refnum))
		return (1);

	if (opt_get_bool(OPT_SHOW_BUDDY_SIGNOFF)) {
		struct imwindow *win;

		win = imwindow_find(acct, user);
		if (win != NULL) {
			screen_win_msg(win, 1, 1, 0,
				MSG_TYPE_SIGNON, "%s has signed on", name);
		} else if (notify) {
			screen_win_msg(screen.status_win, 1, 1, 0,
				MSG_TYPE_SIGNON, "%s has signed on", name);
		}
	}

	return (0);
}

int buddy_went_idle(struct pork_acct *acct, char *user, u_int32_t seconds) {
	if (event_generate(acct->events, EVENT_BUDDY_IDLE, user,
		seconds, acct->refnum))
	{
		return (1);
	}

	if (opt_get_bool(OPT_SHOW_BUDDY_IDLE)) {
		struct imwindow *win;
		char *name = user;
		int notify = 0;
		struct buddy *buddy;

		buddy = buddy_find(acct, user);
		if (buddy != NULL) {
			name = buddy->name;
			if (buddy->notify)
				notify = 1;
		}

		win = imwindow_find(acct, user);
		if (win != NULL)
			screen_win_msg(win, 1, 1, 0, MSG_TYPE_IDLE, "%s is now idle", name);
		else if (notify) {
			screen_win_msg(screen.status_win, 1, 1, 0,
				MSG_TYPE_IDLE, "%s is now idle", name);
		}
	}

	return (0);
}

int buddy_went_unidle(struct pork_acct *acct, char *user) {
	if (event_generate(acct->events, EVENT_BUDDY_UNIDLE, user, acct->refnum))
		return (1);

	if (opt_get_bool(OPT_SHOW_BUDDY_IDLE)) {
		struct imwindow *win;
		char *name = user;
		int notify = 0;
		struct buddy *buddy;

		buddy = buddy_find(acct, user);
		if (buddy != NULL) {
			name = buddy->name;
			if (buddy->notify)
				notify = 1;
		}

		win = imwindow_find(acct, user);
		if (win != NULL) {
			screen_win_msg(win, 1, 1, 0,
				MSG_TYPE_UNIDLE, "%s is no longer idle", name);
		} else if (notify) {
			screen_win_msg(screen.status_win, 1, 1, 0,
				MSG_TYPE_UNIDLE, "%s is no longer idle", name);
		}
	}

	return (0);
}

int buddy_went_away(struct pork_acct *acct, char *user) {
	if (event_generate(acct->events, EVENT_BUDDY_AWAY, user, acct->refnum))
		return (1);

	if (opt_get_bool(OPT_SHOW_BUDDY_AWAY)) {
		struct imwindow *win;
		char *name = user;
		int notify = 0;
		struct buddy *buddy;

		buddy = buddy_find(acct, user);
		if (buddy != NULL) {
			name = buddy->name;
			if (buddy->notify)
				notify = 1;
		}

		win = imwindow_find(acct, user);
		if (win != NULL) {
			screen_win_msg(win, 1, 1, 0,
				MSG_TYPE_AWAY, "%s is now away", name);
		} else if (notify) {
			screen_win_msg(screen.status_win, 1, 1, 0,
				MSG_TYPE_AWAY, "%s is no longer away", name);
		}
	}

	return (0);
}

int buddy_went_unaway(struct pork_acct *acct, char *user) {
	if (event_generate(acct->events, EVENT_BUDDY_BACK, user, acct->refnum))
		return (1);

	if (opt_get_bool(OPT_SHOW_BUDDY_AWAY)) {
		struct imwindow *win;
		char *name = user;
		int notify = 0;
		struct buddy *buddy;

		buddy = buddy_find(acct, user);
		if (buddy != NULL) {
			name = buddy->name;
			if (buddy->notify)
				notify = 1;
		}

		win = imwindow_find(acct, user);
		if (win != NULL) {
			screen_win_msg(win, 1, 1, 0,
				MSG_TYPE_BACK, "%s is no longer away", name);
		} else if (notify) {
			screen_win_msg(screen.status_win, 1, 1, 0,
				MSG_TYPE_BACK, "%s is no longer away", name);
		}
	}

	return (0);
}
