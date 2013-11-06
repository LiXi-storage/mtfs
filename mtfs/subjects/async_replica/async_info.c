/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <thread.h>
#include <mtfs_super.h>
#include <mtfs_inode.h>
#include <mtfs_device.h>
#include "async_extent_internal.h"
#include "async_info_internal.h"
#include "async_internal.h"

static int masync_proc_read_dirty(char *page, char **start, off_t off, int count,
                                  int *eof, void *data)
{
	int ret = 0;
	struct msubject_async_info *async_info = NULL;
	struct masync_bucket *bucket = NULL;
	int extent_number = 0;
	MENTRY();

	*eof = 1;

	async_info = (struct msubject_async_info *)data;

	mtfs_spin_lock(&async_info->msai_bucket_lock);
	mtfs_list_for_each_entry(bucket, &async_info->msai_buckets, mab_linkage) {
		if (ret >= count - 1) {
			MERROR("not enough memory for proc read\n");
			break;
		}
		extent_number = atomic_read(&bucket->mab_number);
		ret += snprintf(page + ret, count - ret, "Inode: %p, NR: %d\n",
		                mtfs_bucket2inode(bucket), extent_number);
	}
	mtfs_spin_unlock(&async_info->msai_bucket_lock);

	MRETURN(ret);
}

static struct mtfs_proc_vars masync_proc_vars[] = {
	{ "dirty", masync_proc_read_dirty, NULL, NULL },
	{ 0 }
};

#define MASYNC_PROC_NAME "async_replica"
static int masync_info_proc_init(struct msubject_async_info *async_info,
                                 struct super_block *sb)
{
	int ret = 0;
	struct mtfs_device *device = mtfs_s2dev(sb);
	MENTRY();

	async_info->msai_proc_entry = mtfs_proc_register(MASYNC_PROC_NAME,
	                                                 mtfs_dev2proc(device),
	                                                 masync_proc_vars,
	                                                 async_info);
	if (unlikely(async_info->msai_proc_entry == NULL)) {
		MERROR("failed to register proc for async replica\n");
		ret = -ENOMEM;
	}

	MRETURN(ret);
}

static int masync_info_proc_fini(struct msubject_async_info *info,
                          struct super_block *sb)
{
	int ret = 0;
	MENTRY();

	mtfs_proc_remove(&info->msai_proc_entry);

	MRETURN(ret);
}

struct msubject_async_info *masync_info_init(struct super_block *sb)
{
	struct msubject_async_info *info = NULL;
	int ret = 0;
	MENTRY();

	MTFS_ALLOC_PTR(info);
	if (info == NULL) {
		MERROR("not enough memory\n");
		goto out;
	}

	info->msai_subject = &the_async;
	MTFS_INIT_LIST_HEAD(&info->msai_lru_extents);
	mtfs_spin_lock_init(&info->msai_lru_lock);
	atomic_set(&info->msai_lru_number, 0);
	MTFS_INIT_LIST_HEAD(&info->msai_buckets);
	mtfs_spin_lock_init(&info->msai_bucket_lock);
	MTFS_INIT_LIST_HEAD(&info->msai_linkage);
	atomic_set(&info->msai_reference, 0);
	init_waitqueue_head(&info->msai_waitq);
	masync_info_add_to_list(info);

	ret = masync_info_proc_init(info, sb);
	if (ret) {
		goto out_free_info;
	}
	goto out;

out_free_info:
	MTFS_FREE_PTR(info);
out:
	MRETURN(info);
}

#define MASYNC_UNMOUNT_TIMEOUT 16
void masync_info_fini(struct msubject_async_info *info,
                      struct super_block *sb,
                      int force)
{
	int ret = 0;
	MENTRY();

	/* TODO: masync_info_cleanup */
	/*
	 * First make sure nobody can find and refer to this info again.
	 */
	masync_info_remove_from_list(info);
	
	if (atomic_read(&info->msai_reference) > 0) {
		struct mtfs_wait_info mwi = MWI_INTR(MWI_ON_SIGNAL_NOOP, NULL);

		if (force) {
			 mwi = MWI_TIMEOUT(MASYNC_UNMOUNT_TIMEOUT * HZ, NULL, NULL);
		}

force_wait:
		ret = mtfs_wait_event(info->msai_waitq,
                                      atomic_read(&info->msai_reference) == 0, &mwi);
		if (force && ret == -ETIMEDOUT) {
			MERROR("Forced cleanup waiting\n");
			goto force_wait;
		}
		
	}

	/* TODO: masync_info_cleanup */
	MASSERT(info);
	MASSERT(mtfs_list_empty(&info->msai_lru_extents));
	MASSERT(mtfs_list_empty(&info->msai_buckets));
	MASSERT(atomic_read(&info->msai_lru_number) == 0);
	MASSERT(atomic_read(&info->msai_reference) == 0);
	masync_info_proc_fini(info, sb);

	MTFS_FREE_PTR(info);

	_MRETURN();
}

/*
 * Prepare a cancel list from LRU.
 * Reomove it from LRU list, but do not release its reference.
 */
int masync_cancel_list_prepare(struct msubject_async_info *info,
                               mtfs_list_t *cancels,
                               int extent_number)
{
	int number = 0;
	struct masync_extent *async_extent = NULL;
	int ret = 0;
	MENTRY();

	while (1) {
		if (number >= extent_number) {
			break;
		}

		ret = 0;
		mtfs_spin_lock(&info->msai_lru_lock);
		if (!mtfs_list_empty(&info->msai_lru_extents)) {
			async_extent = mtfs_list_entry(info->msai_lru_extents.next,
			                               struct masync_extent,
			                               mae_lru_linkage);
			ret = masync_extent_remove_from_lru_nonlock(async_extent);
			MASSERT(ret == 1);
			number++;
		}
		mtfs_spin_unlock(&info->msai_lru_lock);

		if (ret == 0) {
			MERROR("trying to prepare %d, only got %d\n", extent_number, number);
			break;
		}

		mtfs_list_add(&async_extent->mae_cancel_linkage, cancels);
	}

	MRETURN(number);
}

/*
 * Add a list to cancel list.
 */
void masync_cancel_list_merge(struct msubject_async *subject_async,
                             mtfs_list_t *cancels,
                             int extent_number)
{
	MENTRY();
	mtfs_spin_lock(&subject_async->msa_cancel_lock);
	mtfs_list_splice_tail(cancels, &subject_async->msa_cancel_extents);
	mtfs_spin_unlock(&subject_async->msa_cancel_lock);

	_MRETURN();
}

int masync_info_shrink(struct msubject_async_info *info,
                       int nr_to_scan)
{
	int count = 0;
	int ret = 0;
	MTFS_LIST_HEAD(cancel_list);
	MENTRY();

	count = masync_cancel_list_prepare(info, &cancel_list, nr_to_scan);
	masync_cancel_list_merge(info->msai_subject, &cancel_list, count);

	masync_service_wakeup();
	MRETURN(ret);
}


