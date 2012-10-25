/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_ASYNC_INTERNAL_H__
#define __MTFS_ASYNC_INTERNAL_H__
#include <mtfs_async.h>

void masync_bucket_init(struct masync_bucket *bucket);
int masync_bucket_add(struct masync_bucket *bucket,
                      struct mtfs_interval_node_extent *extent);
int masync_bucket_cleanup(struct masync_bucket *bucket);
#endif /* __MTFS_SUBJECT_INTERNAL_H__ */
