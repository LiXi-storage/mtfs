/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <mtfs_subject.h>
#include "lowerfs_internal.h"

int msubject_super_init(struct super_block *sb)
{
	int ret = 0;
	struct mtfs_operations *operations = NULL;
	MENTRY();

	operations = mtfs_s2ops(sb);
	if (operations->subject_ops &&
	    operations->subject_ops->mso_super_init) {
		ret = operations->subject_ops->mso_super_init(sb);
	}

	MRETURN(ret);
}

int msubject_super_fini(struct super_block *sb)
{
	int ret = 0;
	struct mtfs_operations *operations = NULL;
	MENTRY();

	operations = mtfs_s2ops(sb);
	if (operations->subject_ops &&
	    operations->subject_ops->mso_super_fini) {
		ret = operations->subject_ops->mso_super_fini(sb);
	}

	MRETURN(ret);
}

int msubject_inode_init(struct inode *inode)
{
	int ret = 0;
	struct mtfs_operations *operations = NULL;
	MENTRY();

	operations = mtfs_i2ops(inode);
	if (operations->subject_ops &&
	    operations->subject_ops->mso_inode_init) {
		ret = operations->subject_ops->mso_inode_init(inode);
	}

	MRETURN(ret);
}

int msubject_inode_fini(struct inode *inode)
{
	int ret = 0;
	struct mtfs_operations *operations = NULL;
	MENTRY();

	operations = mtfs_i2ops(inode);
	if (operations->subject_ops &&
	    operations->subject_ops->mso_inode_fini) {
		ret = operations->subject_ops->mso_inode_fini(inode);
	}

	MRETURN(ret);
}

static spinlock_t mtfs_subject_lock = SPIN_LOCK_UNLOCKED;
static LIST_HEAD(mtfs_subjects);

static struct mtfs_subject *_msubject_search(const char *type)
{
	struct mtfs_subject *found = NULL;
	struct list_head *p = NULL;
	MENTRY();

	list_for_each(p, &mtfs_subjects) {
		found = list_entry(p, struct mtfs_subject, ms_linkage);
		if (strcmp(found->ms_name, type) == 0) {
			MRETURN(found);
		}
	}

	MRETURN(NULL);
}


static int msubject_check(struct mtfs_subject *subject)
{
	int ret = 0;
	MENTRY();

	if (subject->ms_owner == NULL ||
	    subject->ms_name == NULL) {
	    	ret = -EINVAL;
	}

	MRETURN(ret);
}

static int _msubject_register(struct mtfs_subject *subject)
{
	struct mtfs_subject *found = NULL;
	int ret = 0;
	MENTRY();

	ret = msubject_check(subject);
	if (ret){
		MERROR("subject is not valid\n");
		goto out;
	}

	if ((found = _msubject_search(subject->ms_name))) {
		if (found != subject) {
			MERROR("try to register multiple operations for type %s\n",
			        subject->ms_name);
		} else {
			MERROR("operation of type %s has already been registered\n",
			        subject->ms_name);
		}
		ret = -EALREADY;
	} else {
		list_add(&subject->ms_linkage, &mtfs_subjects);
	}

out:
	MRETURN(ret);
}

int msubject_register(struct mtfs_subject *subject)
{
	int ret = 0;
	MENTRY();

	spin_lock(&mtfs_subject_lock);
	ret = _msubject_register(subject);
	spin_unlock(&mtfs_subject_lock);

	MRETURN(ret);
}
EXPORT_SYMBOL(msubject_register);

static void _msubject_unregister(struct mtfs_subject *subject)
{
	struct list_head *p = NULL;
	MENTRY();

	list_for_each(p, &mtfs_subjects) {
		struct mtfs_subject *found;

		found = list_entry(p, typeof(*found), ms_linkage);
		if (found == subject) {
			list_del(p);
			break;
		}
	}

	_MRETURN();
}

void msubject_unregister(struct mtfs_subject *subject)
{
	MENTRY();

	spin_lock(&mtfs_subject_lock);
	_msubject_unregister(subject);
	spin_unlock(&mtfs_subject_lock);

	_MRETURN();
}
EXPORT_SYMBOL(msubject_unregister);

static struct mtfs_subject *_msubject_get(const char *name)
{
	struct mtfs_subject *found = NULL;
	MENTRY();

	spin_lock(&mtfs_subject_lock);
	found = _msubject_search(name);
	if (found == NULL) {
		MERROR("failed to found subject support for type %s\n", name);
	} else {
		if (try_module_get(found->ms_owner) == 0) {
			MERROR("failed to get module for subject support for type %s\n", name);
			found = ERR_PTR(-EOPNOTSUPP);
		}
	}	
	spin_unlock(&mtfs_subject_lock);

	MRETURN(found);
}

struct mtfs_subject *msubject_get(const char *name)
{
	struct mtfs_subject *found = NULL;
	char module_name[32];
	MENTRY();

	found = _msubject_get(name);
	if (found == NULL) {
		snprintf(module_name, sizeof(module_name) - 1, "mtfs_subject_%s", name);

		if (request_module(module_name) != 0) {
			found = ERR_PTR(-ENOENT);
			MERROR("failed to load module %s\n", module_name);
		} else {
			found = _msubject_get(name);
			if (found == NULL || IS_ERR(found)) {
				MERROR("failed to get subject support after loading module %s, "
				       "type = %s\n",
			               module_name, name);
				found = found == NULL ? ERR_PTR(-EOPNOTSUPP) : found;
			}
		}
	} else if (IS_ERR(found)) {
		MERROR("failed to get subject support for type %s\n", name);
	}

	MRETURN(found);
}

void msubject_put(struct mtfs_subject *subject)
{
	MENTRY();

	module_put(subject->ms_owner);

	_MRETURN();
}
