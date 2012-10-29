/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_ASYNC_INTERNAL_H__
#define __MTFS_ASYNC_INTERNAL_H__
#include <mtfs_async.h>

void masync_bucket_init(struct msubject_async_info *mab_info, struct masync_bucket *bucket);
int masync_bucket_add(struct masync_bucket *bucket,
                      struct mtfs_interval_node_extent *extent);
int masync_bucket_cleanup(struct masync_bucket *bucket);
int masync_subject_init(struct super_block *sb);
int masync_subject_fini(struct super_block *sb);
#endif /* __MTFS_SUBJECT_INTERNAL_H__ */
