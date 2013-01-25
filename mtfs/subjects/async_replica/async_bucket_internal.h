/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_ASYNC_BUCKET_INTERNAL_H__
#define __MTFS_ASYNC_BUCKET_INTERNAL_H__
#include "async_internal.h"

static inline void masync_bucket_remove_from_list(struct masync_bucket *bucket)
{
	struct msubject_async_info *info = NULL;
	MENTRY();

	info = bucket->mab_info;
	MASSERT(info);

	mtfs_spin_lock(&info->msai_bucket_lock);
	MASSERT(!mtfs_list_empty(&bucket->mab_linkage));
	mtfs_list_del_init(&bucket->mab_linkage);
	mtfs_spin_unlock(&info->msai_bucket_lock);

	_MRETURN();
}

static inline void masync_bucket_add_to_list(struct masync_bucket *bucket)
{
	struct msubject_async_info *info = NULL;
	MENTRY();

	info = bucket->mab_info;
	MASSERT(info);

	mtfs_spin_lock(&info->msai_bucket_lock);
	MASSERT(mtfs_list_empty(&bucket->mab_linkage));
	mtfs_list_add_tail(&bucket->mab_linkage, &info->msai_buckets);
	mtfs_spin_unlock(&info->msai_bucket_lock);

	_MRETURN();
}

int masync_bucket_add(struct file *file,
                      struct mtfs_interval_node_extent *interval);
int masync_bucket_cancel(struct masync_bucket *bucket,
                         char *buf, int buf_len,
                         int nr_to_cacel);
void masync_bucket_init(struct msubject_async_info *info,
                        struct masync_bucket *bucket);
int masync_bucket_cleanup(struct masync_bucket *bucket);
int masync_sync_file(struct masync_bucket *bucket,
                     struct mtfs_interval_node_extent *extent,
                     char *buf,
                     int buf_size);
#endif /* __MTFS_ASYNC_BUCKET_INTERNAL_H__ */
