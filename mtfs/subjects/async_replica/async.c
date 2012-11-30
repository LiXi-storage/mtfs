/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <mtfs_inode.h>
#include <mtfs_subject.h>
#include <mtfs_device.h>
#include <mtfs_service.h>
#include "async_bucket_internal.h"
static struct msubject_async_info *masync_info; /* TODO: remove */

int masync_calculate_all(struct msubject_async_info *info)
{
	int ret = 0;
	MENTRY();

	ret = atomic_read(&info->msai_nr);
	MASSERT(ret >= 0);

	MRETURN(ret);
}

/* Return nr that canceled */
int masync_cancel(struct msubject_async_info *info,
                  char *buf, int buf_len,
                  int nr_to_cacel)
{
	int ret = 0;
	struct masync_bucket *bucket = NULL;
	struct masync_bucket *n = NULL;
	int canceled = 0;
	int degraded = 0;
	MENTRY();

	MASSERT(nr_to_cacel > 0);
	while (ret < nr_to_cacel) {
		degraded = 0;

		masync_info_write_lock(info);
		mtfs_list_for_each_entry_safe(bucket, n, &info->msai_dirty_buckets, mab_linkage) {
			if (masync_bucket_trylock(bucket) != 0) {
				/*
				 * Do not wait, degrade instead to free it next time.
				 * Not precisely fair, but that is OK.
				 */
				masync_bucket_degrade_in_lru_nonlock(bucket);
				degraded++;
				MDEBUG("a bucket is too busy to be freed\n");
				continue;
			}

			canceled = masync_bucket_cancel(bucket, buf, buf_len, nr_to_cacel - ret);
			masync_bucket_unlock(bucket);
			ret += canceled;
			if (ret >= nr_to_cacel) {
				break;
			}
		}
		masync_info_write_unlock(info);

		if (ret < nr_to_cacel && degraded == 0) {
			MERROR("only canceled %d/%d, but none is available\n",
			       ret, nr_to_cacel);
			break;
		}
	}

	MRETURN(ret);
}

static int masync_proc_read_dirty(char *page, char **start, off_t off, int count,
                                  int *eof, void *data)
{
	int ret = 0;
	struct msubject_async_info *async_info = NULL;
	struct masync_bucket *bucket = NULL;
	MENTRY();

	*eof = 1;

	async_info = (struct msubject_async_info *)data;

	masync_info_read_lock(async_info);
	mtfs_list_for_each_entry(bucket, &async_info->msai_dirty_buckets, mab_linkage) {
		if (ret >= count - 1) {
			MERROR("not enough memory for proc read\n");
			break;
		}
		ret += snprintf(page + ret, count - ret, "Inode: %p, NR: %d\n",
		                mtfs_bucket2inode(bucket), atomic_read(&bucket->mab_nr));
	}
	masync_info_read_unlock(async_info);

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

#define MSLEFHEAL_SERVICE_NAME "mtfs_selfheal"
int masync_super_init(struct super_block *sb)
{
	int ret = 0;
	struct msubject_async_info *info = NULL;
	MENTRY();

	MTFS_ALLOC_PTR(info);
	if (info == NULL) {
		ret = -ENOMEM;
		MERROR("not enough memory\n");
		goto out;
	}

	MTFS_INIT_LIST_HEAD(&info->msai_dirty_buckets);
	masync_info_lock_init(info);
	atomic_set(&info->msai_nr, 0);

	ret = masync_info_proc_init(info, sb);
	if (ret) {
		goto out_free_info;
	}

	mtfs_s2subinfo(sb) = (void *)info;

	/* TODO: remove */
	masync_info = info;
	goto out;
out_free_info:
	MTFS_FREE_PTR(info);
out:
	MRETURN(ret);
}

int masync_super_fini(struct super_block *sb)
{
	int ret = 0;
	struct msubject_async_info *info = NULL;
	MENTRY();

	info = (struct msubject_async_info *)mtfs_s2subinfo(sb);
	MASSERT(mtfs_list_empty(&info->msai_dirty_buckets));

	masync_info_proc_fini(info, sb);

	MTFS_FREE_PTR(info);

	MRETURN(ret);
}

int masync_inode_init(struct inode *inode)
{
	int ret = 0;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bnum = mtfs_i2bnum(inode);
	MENTRY();

	masync_bucket_init((struct msubject_async_info *)mtfs_s2subinfo(inode->i_sb),
	                   mtfs_i2bucket(inode));

	for (bindex = 0; bindex < bnum; bindex++) {
		if (mtfs_i2branch(inode, bindex)) {
			mlowerfs_bucket_init(mtfs_i2bbucket(inode, bindex));
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

static mtfs_spinlock_t masync_shrinking_lock; /* Protect masync_shrinking */
static int masync_shrinking;
static struct shrinker *masync_shrinker;
atomic_t masync_nr;
struct mtfs_service *masync_service;           /* Service that heal the async files */ 

void masync_service_wakeup(void)
{
	MENTRY();

	wake_up(&masync_service->srv_waitq);
	_MRETURN();
}

static int _masync_shrink(int nr_to_scan, unsigned int gfp_mask)
{
	int ret = 0;
	MENTRY();

	if (nr_to_scan == 0) {
		goto out_calc;
	} else if (!(gfp_mask & __GFP_FS)) {
		ret = -1;
		goto out;
	}

	mtfs_spin_lock(&masync_shrinking_lock);
	masync_shrinking += nr_to_scan;
	mtfs_spin_unlock(&masync_shrinking_lock);
	masync_service_wakeup();
out_calc:
	ret = (atomic_read(&masync_nr) / 100) * sysctl_vfs_cache_pressure;
out:
	MRETURN(ret);
}

static int masync_shrink(SHRINKER_ARGS(sc, nr_to_scan, gfp_mask))
{
	return _masync_shrink(shrink_param(sc, nr_to_scan),
                              shrink_param(sc, gfp_mask));
}

static int masync_service_busy(struct mtfs_service *service, struct mservice_thread *thread)
{
	int ret = 0;
	struct msubject_async_info *info = (struct msubject_async_info *)service->srv_data;
	MENTRY();

	ret = masync_calculate_all(info);
	MRETURN(ret);
}

static int masync_service_main(struct mtfs_service *service, struct mservice_thread *thread)
{
	struct msubject_async_info **info = (struct msubject_async_info **)service->srv_data;
	int ret = 0;
	int flushing = 0;
	int flushed = 0;
	char *buf = NULL;
	int buf_size = MASYNC_BULK_SIZE;
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

		MASSERT(*info);
		mtfs_spin_lock(&masync_shrinking_lock);
		flushing = masync_shrinking;
		masync_shrinking = 0;
		mtfs_spin_unlock(&masync_shrinking_lock);

		if (flushing == 0) {
			/* TODO: try to flush some */
			continue;
		}

		flushed = masync_cancel(*info, buf, buf_size, flushing);
		if (flushed != flushing) {
			MERROR("try to flush %d, only flushed %d\n",
			       flushing, flushed);
		}
	}

	MTFS_FREE(buf, buf_size);
out:
	MRETURN(ret);
}

static int __init masync_init(void)
{
	int ret = 0;
	MENTRY();

	atomic_set(&masync_nr, 0);
	mtfs_spin_lock_init(&masync_shrinking_lock);

	masync_service = mservice_init(MSLEFHEAL_SERVICE_NAME,
	                               MSLEFHEAL_SERVICE_NAME,
	                               1, 1,
	                               0, masync_service_main,
	                               masync_service_busy,
	                               &masync_info);
	if (masync_service == NULL) {
		ret = -ENOMEM;
		MERROR("failed to init service\n");
		goto out;
	}

	ret = mservice_start_threads(masync_service);
        if (ret) {
        	MERROR("failed to start theads\n");
        	goto out_free_service;
        }

	masync_shrinker = mtfs_set_shrinker(DEFAULT_SEEKS, masync_shrink);
	goto out;
out_free_service:
	mservice_fini(masync_service);
out:
	MRETURN(ret);
}

static void __exit masync_exit(void)
{
	MENTRY();

	MASSERT(atomic_read(&masync_nr) == 0);

	mservice_fini(masync_service);
	mtfs_remove_shrinker(masync_shrinker);
	_MRETURN();
}

MODULE_AUTHOR("MulTi File System Development Workgroup");
MODULE_DESCRIPTION("async replica subject for mtfs");
MODULE_LICENSE("GPL");

module_init(masync_init)
module_exit(masync_exit)
