/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_SELFHEAL_INTERNAL_H__
#define __HRFS_SELFHEAL_INTERNAL_H__
#include <hrfs_oplist.h>

struct dentry *hrfs_dchild_rename2new(struct dentry *dparent, struct dentry *dchild_old,
                                      const unsigned char *name_new, unsigned int len);
struct dentry *_hrfs_dchild_add_ino(struct dentry *dparent, struct dentry *dchild_old);
struct dentry *hrfs_dchild_create(struct dentry *dparent, const unsigned char *name,
                                  unsigned int len, umode_t mode, dev_t rdev, int repeat);
struct dentry *hrfs_dchild_remove(struct dentry *dparent, const char *name);
struct dentry *hrfs_dchild_add_ino(struct dentry *dparent, const unsigned char *name_new, unsigned int len);

int hrfs_backup_branch(struct dentry *dentry, hrfs_bindex_t bindex);
struct dentry *hrfs_cleanup_branch(struct inode* dir, struct dentry *dentry, hrfs_bindex_t bindex);
int hrfs_lookup_discard_dentry(struct inode* dir, struct dentry *dentry, struct hrfs_operation_list *list);
#endif /* __HRFS_SELFHEAL_INTERNAL_H__ */
