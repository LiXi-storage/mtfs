/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include "junction_internal.h"
#include <debug.h>
#include <mtfs_common.h>
#include <linux/module.h>

spinlock_t mtfs_junction_lock = SPIN_LOCK_UNLOCKED;
LIST_HEAD(mtfs_junctions);

static int _junction_support_secondary_type(struct mtfs_junction * junction, const char *type)
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

static struct mtfs_junction *_junction_search(const char *subject,
                                              const char *primary_type,
                                              const char **secondary_types)
{
	struct mtfs_junction *junction = NULL;
	int support = 0;
	struct list_head *p = NULL;
	const char **tmp_type = secondary_types;

	MASSERT(primary_type);
	list_for_each(p, &mtfs_junctions) {
		junction = list_entry(p, struct mtfs_junction, junction_list);
		if (strcmp(junction->mj_subject, subject) == 0 &&
		    strcmp(junction->primary_type, primary_type) == 0) {
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

static struct mtfs_junction *junction_search(const char *subject,
                                              const char *primary_type,
                                              const char **secondary_types)
{
	struct mtfs_junction *found = NULL;
	
	spin_lock(&mtfs_junction_lock);
	found = _junction_search(subject, primary_type, secondary_types);
	spin_unlock(&mtfs_junction_lock);
	return found;
}

static int junction_check(struct mtfs_junction *junction)
{
	int ret = 0;

	if (junction->junction_owner == NULL ||
	    junction->junction_name == NULL ||
	    junction->mj_subject == NULL ||
	    junction->primary_type == NULL ||
	    junction->secondary_types == NULL ||
	    junction->fs_ops == NULL) {
	    	ret = -EINVAL;
	}

	return ret;
}

static int _junction_register(struct mtfs_junction *junction)
{
	struct mtfs_junction *found = NULL;
	int ret = 0;
	MENTRY();

	ret = junction_check(junction);
	if (ret){
		MERROR("junction is not valid\n");
		goto out;
	}

	if ((found = _junction_search(junction->mj_subject, junction->primary_type, junction->secondary_types))) {
		if (found != junction) {
			MERROR("try to register multiple operations for type %s\n",
			        junction->junction_name);
		} else {
			MERROR("operation of type %s has already been registered\n",
			        junction->junction_name);
		}
		ret = -EALREADY;
	} else {
		list_add(&junction->junction_list, &mtfs_junctions);
	}

out:
	MRETURN(ret);
}

int junction_register(struct mtfs_junction *junction)
{
	int ret = 0;
	
	spin_lock(&mtfs_junction_lock);
	ret = _junction_register(junction);
	spin_unlock(&mtfs_junction_lock);
	return ret;
}
EXPORT_SYMBOL(junction_register);

static void _junction_unregister(struct mtfs_junction *junction)
{
	struct list_head *p = NULL;

	list_for_each(p, &mtfs_junctions) {
		struct mtfs_junction *found;

		found = list_entry(p, typeof(*found), junction_list);
		if (found == junction) {
			list_del(p);
			break;
		}
	}
}

void junction_unregister(struct mtfs_junction *junction)
{
	spin_lock(&mtfs_junction_lock);
	_junction_unregister(junction);
	spin_unlock(&mtfs_junction_lock);
}
EXPORT_SYMBOL(junction_unregister);

struct mtfs_junction *junction_get(const char *subject,
                                   const char *primary_type,
                                   const char **secondary_types)
{
	struct mtfs_junction *junction = NULL;
	int ret = 0;
	MENTRY();

	junction = junction_search(subject, primary_type, secondary_types);
	if (junction == NULL) {
		ret = -EOPNOTSUPP;
		MERROR("failed to find proper junction for type '%s'\n", primary_type); /* TODO: secondary*/
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
	MRETURN(junction);
}

void junction_put(struct mtfs_junction *junction)
{
	module_put(junction->junction_owner);
}
