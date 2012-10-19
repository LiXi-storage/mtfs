/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_SUBJECT_INTERNAL_H__
#define __MTFS_SUBJECT_INTERNAL_H__
#include <linux/fs.h>

int mtfs_subject_init(struct super_block *sb);
int mtfs_subject_fini(struct super_block *sb);

#endif /* __MTFS_SUBJECT_INTERNAL_H__ */
