/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_HEAL_INTERNAL_H__
#define __MTFS_HEAL_INTERNAL_H__

#include <mtfs_list.h>
#include <mtfs_oplist.h>

struct mtfs_dentry_list {
	mtfs_list_t list;
	struct dentry *dentry;
};

int mtfs_lookup_discard_dentry(struct inode *dir, struct dentry *dentry, struct mtfs_operation_list *list);
struct dentry *mtfs_dchild_create(struct dentry *dparent, const unsigned char *name,
                                  unsigned int len, umode_t mode, dev_t rdev, int repeat);
struct dentry *mtfs_dchild_remove(struct dentry *dparent, const char *name);

#endif /* __MTFS_HEAL_INTERNAL_H__ */
