/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_HEAL_INTERNAL_H__
#define __MTFS_HEAL_INTERNAL_H__

#include <mtfs_heal.h>
#include <mtfs_list.h>
#include <mtfs_oplist.h>

struct mtfs_dentry_list {
	mtfs_list_t list;
	struct dentry *dentry;
};

int heal_discard_dentry(struct inode *dir, struct dentry *dentry, struct mtfs_operation_list *list);
struct dentry *mtfs_dchild_remove(struct dentry *dparent, const char *name);
#endif /* __MTFS_HEAL_INTERNAL_H__ */
