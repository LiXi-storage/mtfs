/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_SUBJECT_INTERNAL_H__
#define __MTFS_SUBJECT_INTERNAL_H__
#include <linux/fs.h>

int msubject_super_init(struct super_block *sb);
int msubject_super_fini(struct super_block *sb);
int msubject_inode_init(struct inode *inode);
int msubject_inode_fini(struct inode *inode);

#endif /* __MTFS_SUBJECT_INTERNAL_H__ */
