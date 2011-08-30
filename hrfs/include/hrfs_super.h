/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_SUPER_H__
#define __HRFS_SUPER_H__

extern struct inode *hrfs_alloc_inode(struct super_block *sb);
extern void hrfs_destroy_inode(struct inode *inode);
extern void hrfs_put_super(struct super_block *sb);
extern int hrfs_statfs(struct dentry *dentry, struct kstatfs *buf);
extern void hrfs_clear_inode(struct inode *inode);
extern int hrfs_show_options(struct seq_file *m, struct vfsmount *mnt);

#endif /* __HRFS_SUPER_H__ */
