/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <mtfs_inode.h>
#include <mtfs_subject.h>
#include <mtfs_device.h>
#include <mtfs_service.h>
#include "async_bucket_internal.h"
#include "async_info_internal.h"
#include "async_extent_internal.h"

static int _masync_shrink(int nr_to_scan, unsigned int gfp_mask);
int masync_super_init(struct super_block *sb)
{
	int ret = 0;
	struct msubject_async_info *info = NULL;
	MENTRY();

	info = masync_info_init(sb);
	if (info == NULL) {
		MERROR("failed to init info\n");
		goto out;
	}

	mtfs_s2subinfo(sb) = (void *)info;
out:
	MRETURN(ret);
}

int masync_super_fini(struct super_block *sb)
{
	int ret = 0;
	struct msubject_async_info *info = NULL;
	MENTRY();

	info = (struct msubject_async_info *)mtfs_s2subinfo(sb);
	masync_info_fini(info, sb, 0);

	MRETURN(ret);
}

int masync_inode_init(struct inode *inode)
{
	int ret = 0;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bnum = mtfs_i2bnum(inode);
	struct mtfs_lowerfs *lowerfs = NULL;
	MENTRY();

	masync_bucket_init((struct msubject_async_info *)mtfs_s2subinfo(inode->i_sb),
	                   mtfs_i2bucket(inode));

	for (bindex = 0; bindex < bnum; bindex++) {
		if (mtfs_i2branch(inode, bindex)) {
			lowerfs = mtfs_i2bops(inode, bindex);
			mlowerfs_bucket_init(mtfs_i2bbucket(inode, bindex),
			                     lowerfs->ml_bucket_type);
		}
	}
	MRETURN(ret);
}

int masync_inode_fini(struct inode *inode)
{
	int ret = 0;
	MENTRY();

	masync_bucket_cleanup(mtfs_i2bucket(inode));
	MRETURN(ret);
}

struct mtfs_subject_operations masync_subject_ops = {
	mso_super_init:                 masync_super_init,
	mso_super_fini:                 masync_super_fini,
	mso_inode_init:                 masync_inode_init,
	mso_inode_fini:                 masync_inode_fini,
};
EXPORT_SYMBOL(masync_subject_ops);

struct msubject_async the_async;

void masync_service_wakeup(void)
{
	MENTRY();

	wake_up(&the_async.msa_service->srv_waitq);
	_MRETURN();
}

int masync_service_busy(struct mtfs_service *service, struct mservice_thread *thread)
{
	int ret = 0;
	struct msubject_async *async = (struct msubject_async *)service->srv_data;
	MENTRY();

	mtfs_spin_lock(&async->msa_cancel_lock);
	ret = !mtfs_list_empty(&async->msa_cancel_extents);
	mtfs_spin_unlock(&async->msa_cancel_lock);
	MRETURN(ret);
}

#define MTFS_MAX_NR_ONCE 8

int masync_service_main(struct mtfs_service *service, struct mservice_thread *thread)
{
	struct msubject_async *async = (struct msubject_async *)service->srv_data;
	int ret = 0;
	char *buf = NULL;
	int buf_size = MASYNC_BULK_SIZE;
	struct masync_extent *async_extent = NULL;
	int retry = 0;
	int nr_to_scan = 0;
	MENTRY();

	MTFS_ALLOC(buf, buf_size);
	if (buf == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		MBUG();
		goto out;
	}

	while (1) {
		if (mservice_wait_event(service, thread)) {
			break;
		}

		mtfs_spin_lock(&async->msa_cancel_lock);
#if 0
		if (mtfs_list_empty(&async->msa_cancel_extents)) {
			mtfs_spin_unlock(&async->msa_cancel_lock);
			continue;
		}
#else
		if (mtfs_list_empty(&async->msa_cancel_extents)) {
			/* I am idle */
			mtfs_spin_unlock(&async->msa_cancel_lock);
			nr_to_scan = _masync_shrink(0, __GFP_FS);
			if (nr_to_scan != 0) {
				if (nr_to_scan > MTFS_MAX_NR_ONCE) {
					nr_to_scan /= 2;
				}
				nr_to_scan = _masync_shrink(nr_to_scan, __GFP_FS);
			}
			continue;
		}
#endif
		async_extent = mtfs_list_entry(async->msa_cancel_extents.next,
		                               struct masync_extent,
		                               mae_cancel_linkage);
		mtfs_list_del_init(&async_extent->mae_cancel_linkage);
		mtfs_spin_unlock(&async->msa_cancel_lock);

		/* If not in tree, masync_extent_flush() will skip it */
		retry = masync_extent_flush(async_extent, buf, buf_size);
		if (retry) {
			mtfs_spin_lock(&async->msa_cancel_lock);
			mtfs_list_add_tail(&async_extent->mae_cancel_linkage, &async->msa_cancel_extents);
			mtfs_spin_unlock(&async->msa_cancel_lock);
		} else {
			masync_extent_put(async_extent);
		}
	}

	MTFS_FREE(buf, buf_size);
out:
	MRETURN(ret);
}

/* Unable to put this to struct msubject_async in some Linux version */
static struct shrinker *masync_shrinker;

static int _masync_shrink(int nr_to_scan, unsigned int gfp_mask)
{
	int ret = 0;
	int i = 0;
	struct msubject_async *subject = &the_async;
	struct msubject_async_info *info = NULL;
	int total = 0;
	int cancel = 0;
	int mine = 0;
	int total_again = 0;
	MENTRY();

	if (!(gfp_mask & __GFP_FS)) {
		ret = -1;
		goto out;
	}

	/* Not accurate because of race, but that's ok */
	for (i = atomic_read(&the_async.msa_info_number);
	     i > 0; i--) {
		mtfs_spin_lock(&subject->msa_info_lock);
		if (mtfs_list_empty(&subject->msa_infos)) {
			ret = 0;
			mtfs_spin_unlock(&subject->msa_info_lock);
			goto out;
		}
		info = mtfs_list_entry(subject->msa_infos.next,
		                       struct msubject_async_info,
		                       msai_linkage);
		masync_info_get(info);
		masync_info_touch_list_nonlock(info);
		mtfs_spin_unlock(&subject->msa_info_lock);

		total += atomic_read(&info->msai_lru_number);
		masync_info_put(info);
	}

	if (nr_to_scan == 0 || total == 0) {
		ret = total;
		goto out;
	}

	/* Not accurate because of race, but that's ok */
	for (i = atomic_read(&the_async.msa_info_number);
	     i > 0; i--) {
		mtfs_spin_lock(&subject->msa_info_lock);
		if (mtfs_list_empty(&subject->msa_infos)) {
			ret = 0;
			mtfs_spin_unlock(&subject->msa_info_lock);
			goto out;
		}
		info = mtfs_list_entry(subject->msa_infos.next,
		                       struct msubject_async_info,
		                       msai_linkage);
		masync_info_get(info);
		masync_info_touch_list_nonlock(info);
		mtfs_spin_unlock(&subject->msa_info_lock);

		mine = atomic_read(&info->msai_lru_number);
		cancel = 1 + nr_to_scan * mine / total;
		masync_info_shrink(info, cancel);
		total_again += atomic_read(&info->msai_lru_number);
		masync_info_put(info);
	}
out:
	MRETURN(ret);
}

int masync_shrink(SHRINKER_ARGS(sc, nr_to_scan, gfp_mask))
{
	return _masync_shrink(shrink_param(sc, nr_to_scan),
                              shrink_param(sc, gfp_mask));
}

#define MSLEFHEAL_SERVICE_NAME "mtfs_selfheal"
static int __init masync_init(void)
{
	int ret = 0;
	MENTRY();

	MTFS_INIT_LIST_HEAD(&the_async.msa_cancel_extents);
	mtfs_spin_lock_init(&the_async.msa_cancel_lock);
	MTFS_INIT_LIST_HEAD(&the_async.msa_infos);
	mtfs_spin_lock_init(&the_async.msa_info_lock);
	atomic_set(&the_async.msa_info_number, 0);
	the_async.msa_service = mservice_init(MSLEFHEAL_SERVICE_NAME,
	                                      MSLEFHEAL_SERVICE_NAME,
	                                      1, 1, 1,
	                                      0, masync_service_main,
	                                      masync_service_busy,
	                                      &the_async);

	if (the_async.msa_service == NULL) {
		ret = -ENOMEM;
		MERROR("failed to init service\n");
		goto out;
	}

	masync_shrinker = mtfs_set_shrinker(DEFAULT_SEEKS, masync_shrink);
out:
	MRETURN(ret);
}

static void __exit masync_exit(void)
{
	MENTRY();

	MASSERT(mtfs_list_empty(&the_async.msa_cancel_extents));
	MASSERT(mtfs_list_empty(&the_async.msa_infos));
	MASSERT(atomic_read(&the_async.msa_info_number) == 0);

	mservice_fini(the_async.msa_service);
	mtfs_remove_shrinker(masync_shrinker);
	_MRETURN();
}

MODULE_AUTHOR("MulTi File System Development Workgroup");
MODULE_DESCRIPTION("async replica subject for mtfs");
MODULE_LICENSE("GPL");

module_init(masync_init)
module_exit(masync_exit)
