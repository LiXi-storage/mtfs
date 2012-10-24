/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <debug.h>
#include "support_internal.h"

spinlock_t mtfs_lowerfs_lock = SPIN_LOCK_UNLOCKED;
LIST_HEAD(lowerfs_types);

static struct lowerfs_operations *_lowerfs_search_ops(const char *type)
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

static int _lowerfs_register_ops(struct lowerfs_operations *fs_ops)
{
	struct lowerfs_operations *found = NULL;
	int ret = 0;

	if ((found = _lowerfs_search_ops(fs_ops->lowerfs_type))) {
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

struct lowerfs_operations *_lowerfs_get_ops(const char *type)
{
	struct lowerfs_operations *found = NULL;

	spin_lock(&mtfs_lowerfs_lock);
	found = _lowerfs_search_ops(type);
	if (found == NULL) {
		MERROR("failed to found lowerfs support for type %s\n", type);
	} else {
		if (try_module_get(found->lowerfs_owner) == 0) {
			MERROR("failed to get module for lowerfs support for type %s\n", type);
			found = ERR_PTR(-EOPNOTSUPP);
		}
	}	
	spin_unlock(&mtfs_lowerfs_lock);
	return found;
}

struct lowerfs_operations *lowerfs_get_ops(const char *type)
{
	struct lowerfs_operations *found = NULL;
	char module_name[32];

	found = _lowerfs_get_ops(type);
	if (found == NULL) {
		if (strcmp(type, "ext2") == 0 ||
		    strcmp(type, "ext3") == 0 ||
		    strcmp(type, "ext4") == 0) {
			snprintf(module_name, sizeof(module_name) - 1, "mtfs_ext");
		} else {
			snprintf(module_name, sizeof(module_name) - 1, "mtfs_%s", type);
		}

		if (request_module(module_name) != 0) {
			found = ERR_PTR(-ENOENT);
			MERROR("failed to load module %s\n", module_name);
		} else {
			found = _lowerfs_get_ops(type);
			if (found == NULL || IS_ERR(found)) {
				MERROR("failed to get lowerfs support after loading module %s, "
				       "type = %s\n",
			               module_name, type);
				found = found == NULL ? ERR_PTR(-EOPNOTSUPP) : found;
			}
		}
	} else if (IS_ERR(found)) {
		MERROR("failed to get lowerfs support for type %s\n", type);
	}

	return found;
}


void lowerfs_put_ops(struct lowerfs_operations *fs_ops)
{
	module_put(fs_ops->lowerfs_owner);
}
