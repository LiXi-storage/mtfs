/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_SELFHEAL_H__
#define __MTFS_SELFHEAL_H__

#if defined(__linux__) && defined(__KERNEL__)
#include <mtfs_oplist.h>

extern struct dentry *mtfs_dchild_rename2new(struct dentry *dparent, struct dentry *dchild_old,
                                      const unsigned char *name_new, unsigned int len);
extern struct dentry *_mtfs_dchild_add_ino(struct dentry *dparent, struct dentry *dchild_old);
extern struct dentry *mtfs_dchild_create(struct dentry *dparent, const unsigned char *name,
                                  unsigned int len, umode_t mode, dev_t rdev, int repeat);
extern struct dentry *mtfs_dchild_remove(struct dentry *dparent, const char *name);
extern struct dentry *mtfs_dchild_add_ino(struct dentry *dparent, const unsigned char *name_new, unsigned int len);

extern int mtfs_backup_branch(struct dentry *dentry, mtfs_bindex_t bindex);
extern struct dentry *mtfs_cleanup_branch(struct inode* dir, struct dentry *dentry, mtfs_bindex_t bindex);
extern int mtfs_lookup_discard_dentry(struct inode* dir, struct dentry *dentry, struct mtfs_operation_list *list);

#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_SELFHEAL_H__ */
