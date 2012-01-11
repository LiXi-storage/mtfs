/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_SELFHEAL_INTERNAL_H__
#define __HRFS_SELFHEAL_INTERNAL_H__
#include <hrfs_flag.h>

struct dentry *hrfs_dchild_create(struct dentry *dparent, const char *name, umode_t mode, dev_t rdev);
struct dentry *hrfs_dchild_remove(struct dentry *dparent, const char *name);
struct dentry *hrfs_dchild_add_ino(struct dentry *dparent, const char *name);

int hrfs_backup_branch(struct dentry *dentry, hrfs_bindex_t bindex);
struct dentry *hrfs_cleanup_branch(struct dentry *dentry, hrfs_bindex_t bindex);
int hrfs_lookup_discard_dentry(struct dentry *dentry, struct hrfs_operation_list *list);
#endif /* __HRFS_SELFHEAL_INTERNAL_H__ */
