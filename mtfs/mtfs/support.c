/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <debug.h>
#include "support_internal.h"

spinlock_t mtfs_lowerfs_lock = SPIN_LOCK_UNLOCKED;
LIST_HEAD(lowerfs_types);

static struct lowerfs_operations *_lowerfs_search_type(const char *type)
{
	struct lowerfs_operations *found = NULL;
	struct list_head *p = NULL;

	list_for_each(p, &lowerfs_types) {
		found = list_entry(p, struct lowerfs_operations, lowerfs_list);
		if (strcmp(found->lowerfs_type, type) == 0) {
			return found;
		}
	}
	return NULL;
}

static struct lowerfs_operations *lowerfs_search_type(const char *type)
{
	struct lowerfs_operations *found = NULL;
	
	spin_lock(&mtfs_lowerfs_lock);
	found = _lowerfs_search_type(type);
	spin_unlock(&mtfs_lowerfs_lock);
	return found;
}

static int _lowerfs_register_ops(struct lowerfs_operations *fs_ops)
{
	struct lowerfs_operations *found = NULL;
	int ret = 0;

	if ((found = _lowerfs_search_type(fs_ops->lowerfs_type))) {
		if (found != fs_ops) {
			MERROR("try to register multiple operations for type %s\n",
			        fs_ops->lowerfs_type);
		} else {
			MERROR("operation of type %s has already been registered\n",
			        fs_ops->lowerfs_type);
		}
		ret = -EALREADY;
	} else {
		//PORTAL_MODULE_USE;
		list_add(&fs_ops->lowerfs_list, &lowerfs_types);
	}

	return ret;
}

int lowerfs_register_ops(struct lowerfs_operations *fs_ops)
{
	int ret = 0;
	
	spin_lock(&mtfs_lowerfs_lock);
	ret = _lowerfs_register_ops(fs_ops);
	spin_unlock(&mtfs_lowerfs_lock);
	return ret;
}
EXPORT_SYMBOL(lowerfs_register_ops);

static void _lowerfs_unregister_ops(struct lowerfs_operations *fs_ops)
{
	struct list_head *p = NULL;

	list_for_each(p, &lowerfs_types) {
		struct lowerfs_operations *found;

		found = list_entry(p, typeof(*found), lowerfs_list);
		if (found == fs_ops) {
			list_del(p);
			break;
		}
	}
}

void lowerfs_unregister_ops(struct lowerfs_operations *fs_ops)
{
	spin_lock(&mtfs_lowerfs_lock);
	_lowerfs_unregister_ops(fs_ops);
	spin_unlock(&mtfs_lowerfs_lock);
}
EXPORT_SYMBOL(lowerfs_unregister_ops);

struct lowerfs_operations *lowerfs_get_ops(const char *type)
{
	struct lowerfs_operations *fs_ops = NULL;
	int ret = 0;
	MENTRY();

	fs_ops = lowerfs_search_type(type);
	if (fs_ops == NULL) {
		char name[32];

		snprintf(name, sizeof(name) - 1, "mtfs_%s", type);
		name[sizeof(name) - 1] = '\0';

		if (request_module(name) != 0) {
			ret = -ENODEV;
			MERROR("failed to load module '%s'\n", name);
			goto out;
		} else {
			MDEBUG("loaded module '%s'\n", name);
			fs_ops = lowerfs_search_type(type);
			if (fs_ops == NULL) {
				ret = -EOPNOTSUPP;
				goto out;
			}
		}
	}

	/*
	 * If support module removed after 
	 * lowerfs_search_type() just returned successfully,
	 * will this be OK? Since fs_ops will point to nobody.
	 */
	if (try_module_get(fs_ops->lowerfs_owner) == 0) {
		ret = -EOPNOTSUPP;
	}

out:
	if (ret) {
		fs_ops = ERR_PTR(ret);
	}
	MRETURN(fs_ops);
}

void lowerfs_put_ops(struct lowerfs_operations *fs_ops)
{
	module_put(fs_ops->lowerfs_owner);
}
