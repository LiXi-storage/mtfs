/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/ctype.h>
#include <linux/module.h>
#include <debug.h>
#include <memory.h>
#include <mtfs_common.h>
#include "junction_internal.h"

spinlock_t mtfs_junction_lock = SPIN_LOCK_UNLOCKED;
LIST_HEAD(mtfs_junctions);

static int _junction_support_secondary_type(struct mtfs_junction * junction, const char *type)
{
	const char **secondary_types = junction->mj_secondary_types;

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
		junction = list_entry(p, struct mtfs_junction, mj_list);
		if (strcmp(junction->mj_subject, subject) == 0 &&
		    strcmp(junction->mj_primary_type, primary_type) == 0) {
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

static int mjunction_operations_check(struct mtfs_operations *operations)
{
	int ret = 0;

	if (operations->symlink_iops == NULL ||
	    operations->dir_iops == NULL ||
	    operations->main_iops == NULL ||
	    operations->main_fops == NULL ||
	    operations->dir_fops == NULL ||
	    operations->sops == NULL ||
	    operations->dops == NULL ||
	    operations->aops == NULL ||
	    operations->vm_ops == NULL ||
	    //operations->ioctl == NULL ||
	    //operations->heal_ops == NULL ||
	    //operations->subject_ops == NULL ||
	    operations->iupdate_ops == NULL ||
	    operations->io_ops == NULL) {
	    	MERROR("invalid mtfs_operations\n");
	    	ret = -EINVAL;
	}

	return ret;
}

static int junction_check(struct mtfs_junction *junction)
{
	int ret = 0;

	if (junction->mj_owner == NULL ||
	    junction->mj_name == NULL ||
	    junction->mj_subject == NULL ||
	    junction->mj_primary_type == NULL ||
	    junction->mj_secondary_types == NULL ||
	    junction->mj_fs_ops == NULL ||
	    mjunction_operations_check(junction->mj_fs_ops)) {
	    	MERROR("invalid junction\n");
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

	found = _junction_search(junction->mj_subject,
	                         junction->mj_primary_type,
	                         junction->mj_secondary_types);
	if (found) {
		if (found != junction) {
			MERROR("try to register multiple operations for type %s\n",
			        junction->mj_name);
		} else {
			MERROR("operation of type %s has already been registered\n",
			        junction->mj_name);
		}
		ret = -EALREADY;
	} else {
		list_add(&junction->mj_list, &mtfs_junctions);
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

		found = list_entry(p, typeof(*found), mj_list);
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

struct mtfs_junction *_junction_get(const char *subject,
                                    const char *primary_type,
                                    const char **secondary_types)
{
	struct mtfs_junction *found = NULL;

	spin_lock(&mtfs_junction_lock);
	found = _junction_search(subject, primary_type, secondary_types);
	if (found == NULL) {
		MDEBUG("failed to found junction, subject = %s, type = %s\n",
		       subject, primary_type); /* TODO: secondary*/
	} else {
		if (try_module_get(found->mj_owner) == 0) {
			MDEBUG("failed to get junction module, subject = %s, type = %s\n",
			       subject, primary_type);
			found = ERR_PTR(-EOPNOTSUPP);
		}
	}
	spin_unlock(&mtfs_junction_lock);
	return found;
}

char *str_tolower(const char *str)
{
	char *tmp = NULL;
	int i = 0;

	MTFS_ALLOC(tmp, strlen(str) + 1);
	if (tmp == NULL) {
		MERROR("not enough memory\n");
		goto out;
	}

	for (i = 0; i < strlen(str); i++) {
		tmp[i] = tolower(str[i]);
	}
	tmp[strlen(str)] = '\0';

out:
	return tmp;
} 

struct mtfs_junction *junction_get(const char *subject,
                                   const char *primary_type,
                                   const char **secondary_types)
{
	struct mtfs_junction *found = NULL;
	char module_name[32];
	char *lower_subject = NULL;

	found = _junction_get(subject, primary_type, secondary_types);
	if (found == NULL) {
		lower_subject = str_tolower(subject);
		if (lower_subject == NULL) {
			found = ERR_PTR(-ENOMEM);
			goto out;
		}

		if (strcmp(primary_type, "ext2") == 0 ||
		    strcmp(primary_type, "ext3") == 0 ||
		    strcmp(primary_type, "ext4") == 0) {
			snprintf(module_name, sizeof(module_name) - 1,
			         "mtfs_junction_%s_ext", lower_subject);
		} else {
			snprintf(module_name, sizeof(module_name) - 1,
			         "mtfs_junction_%s_%s", lower_subject, primary_type);
		}

		if (request_module(module_name) != 0) {
			found = ERR_PTR(-ENOENT);
			MERROR("failed to load module %s\n", module_name);
		} else {
			found = _junction_get(subject, primary_type, secondary_types);
			if (found == NULL || IS_ERR(found)) {
				MERROR("failed to get junction after loading module %s, "
				       "subject = %s, primary_type = %s\n",
			               module_name, subject, primary_type);
				found = found == NULL ? ERR_PTR(-EOPNOTSUPP) : found;
			}
		}
		MTFS_FREE(lower_subject, strlen(lower_subject) + 1);
	} else if (IS_ERR(found)) {
		MERROR("failed to get lowerfs support, subject = %s, primary_type = %s\n",
		       primary_type, module_name);
	}

out:
	return found;
}

void junction_put(struct mtfs_junction *junction)
{
	module_put(junction->mj_owner);
}
