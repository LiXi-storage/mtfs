/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#include "junction_internal.h"
#include <debug.h>
#include <hrfs_common.h>

spinlock_t hrfs_junction_lock = SPIN_LOCK_UNLOCKED;
LIST_HEAD(hrfs_junctions);

static int _junction_support_secondary_type(struct hrfs_junction * junction, const char *type)
{
	const char **secondary_types = junction->secondary_types;

	if (secondary_types) {
		while(*secondary_types != NULL) {
			if (strcmp(*secondary_types, type) == 0) {
				return 1;
			}
			secondary_types ++;
		}
	}
	return 0;
}

static struct hrfs_junction *_junction_search(const char *primary_type, const char **secondary_types)
{
	struct hrfs_junction *junction = NULL;
	int support = 0;
	struct list_head *p = NULL;
	const char **tmp_type = secondary_types;

	HASSERT(primary_type);
	list_for_each(p, &hrfs_junctions) {
		junction = list_entry(p, struct hrfs_junction, junction_list);
		if (strcmp(junction->primary_type, primary_type) == 0) {
			support = 1;
			if (tmp_type) {
				while(*tmp_type != NULL) {
					if (!_junction_support_secondary_type(junction, *tmp_type)) {
						support = 0;
						break;
					}
					tmp_type ++;
				}
			}
			if (support) {
				return junction;
			}
		}
	}
	return NULL;
}

static struct hrfs_junction *junction_search(const char *primary_type, const char **secondary_types)
{
	struct hrfs_junction *found = NULL;
	
	spin_lock(&hrfs_junction_lock);
	found = _junction_search(primary_type, secondary_types);
	spin_unlock(&hrfs_junction_lock);
	return found;
}

static int _junction_register(struct hrfs_junction *junction)
{
	struct hrfs_junction *found = NULL;
	int ret = 0;

	if ((found = _junction_search(junction->primary_type, junction->secondary_types))) {
		if (found != junction) {
			HERROR("try to register multiple operations for type %s\n",
			        junction->junction_name);
		} else {
			HERROR("operation of type %s has already been registered\n",
			        junction->junction_name);
		}
		ret = -EALREADY;
	} else {
		//PORTAL_MODULE_USE;
		list_add(&junction->junction_list, &hrfs_junctions);
	}

	return ret;
}

int junction_register(struct hrfs_junction *junction)
{
	int ret = 0;
	
	spin_lock(&hrfs_junction_lock);
	ret = _junction_register(junction);
	spin_unlock(&hrfs_junction_lock);
	return ret;
}
EXPORT_SYMBOL(junction_register);

static void _junction_unregister(struct hrfs_junction *junction)
{
	struct list_head *p = NULL;

	list_for_each(p, &hrfs_junctions) {
		struct hrfs_junction *found;

		found = list_entry(p, typeof(*found), junction_list);
		if (found == junction) {
			list_del(p);
			break;
		}
	}
}

void junction_unregister(struct hrfs_junction *junction)
{
	spin_lock(&hrfs_junction_lock);
	_junction_unregister(junction);
	spin_unlock(&hrfs_junction_lock);
}
EXPORT_SYMBOL(junction_unregister);

struct hrfs_junction *junction_get(const char *primary_type, const char **secondary_types)
{
	struct hrfs_junction *junction = NULL;
	int ret = 0;
	HENTRY();

	junction = junction_search(primary_type, secondary_types);
	if (junction == NULL) {
		ret = -EOPNOTSUPP;
		HERROR("failed to find proper junction for type '%s'\n", primary_type); /* TODO: secondary*/
		goto out;
	}

	/*
	 * If support module removed after 
	 * junction_search() just returned successfully,
	 * will this be OK? Since junction will point to nobody.
	 */
	if (try_module_get(junction->junction_owner) == 0) {
		ret = -EOPNOTSUPP;
	}

out:
	if (ret) {
		junction = ERR_PTR(ret);
	}
	HRETURN(junction);
}

void junction_put(struct hrfs_junction *junction)
{
	module_put(junction->junction_owner);
}
