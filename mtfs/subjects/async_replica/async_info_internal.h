/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_ASYNC_INFO_INTERNAL_H__
#define __MTFS_ASYNC_INFO_INTERNAL_H__
#include <thread.h>
#include <mtfs_async.h>
#include "async_internal.h"

static inline void masync_info_remove_from_list(struct msubject_async_info *info)
{
	struct msubject_async *subject = info->msai_subject;
	MENTRY();

	MASSERT(subject);
	mtfs_spin_lock(&subject->msa_info_lock);
	MASSERT(!mtfs_list_empty(&info->msai_linkage));
	mtfs_list_del_init(&info->msai_linkage);
	atomic_dec(&subject->msa_info_number);
	mtfs_spin_unlock(&subject->msa_info_lock);

	_MRETURN();
}

static inline void masync_info_add_to_list(struct msubject_async_info *info)
{
	struct msubject_async *subject = info->msai_subject;
	MENTRY();

	MASSERT(subject);

	mtfs_spin_lock(&subject->msa_info_lock);
	MASSERT(mtfs_list_empty(&info->msai_linkage));
	mtfs_list_add_tail(&info->msai_linkage, &subject->msa_infos);
	atomic_inc(&subject->msa_info_number);
	mtfs_spin_unlock(&subject->msa_info_lock);

	_MRETURN();
}

static inline void masync_info_touch_list_nonlock(struct msubject_async_info *info)
{
	struct msubject_async *subject = info->msai_subject;
	MENTRY();

	MASSERT(subject);

	MASSERT(!mtfs_list_empty(&info->msai_linkage));
	mtfs_list_move_tail(&info->msai_linkage, &subject->msa_infos);

	_MRETURN();
}

static inline void masync_info_get(struct msubject_async_info *info)
{
	atomic_inc(&info->msai_reference);
}

static inline void masync_info_put(struct msubject_async_info *info)
{
	if (atomic_dec_and_test(&info->msai_reference)) {
		wake_up(&info->msai_waitq);
	}
}

int masync_calculate_all(struct msubject_async_info *info);
struct msubject_async_info *masync_info_init(struct super_block *sb);
void masync_info_fini(struct msubject_async_info *info,
                      struct super_block *sb,
                      int force);
int masync_info_shrink(struct msubject_async_info *info,
                       int nr_to_scan);
#endif /* __MTFS_ASYNC_BUCKET_INTERNAL_H__ */
