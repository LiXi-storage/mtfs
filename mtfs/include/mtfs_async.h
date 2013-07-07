/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_ASYNC_H__
#define __MTFS_ASYNC_H__

#if defined(__linux__) && defined(__KERNEL__)
#include <linux/file.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <mtfs_proc.h>
#include <mtfs_io.h>
#include <mtfs_file.h>
#include <mtfs_interval_tree.h>

#ifdef HAVE_SHRINK_CONTROL
#define SHRINKER_ARGS(sc, nr_to_scan, gfp_mask)  \
                      struct shrinker *shrinker, \
                      struct shrink_control *sc
#define shrink_param(sc, var) ((sc)->var)
#else /* !HAVE_SHRINK_CONTROL */
#ifdef HAVE_SHRINKER_WANT_SHRINK_PTR
#define SHRINKER_ARGS(sc, nr_to_scan, gfp_mask)  \
                      struct shrinker *shrinker, \
                      int nr_to_scan, gfp_t gfp_mask
#else /* !HAVE_SHRINKER_WANT_SHRINK_PTR */
#define SHRINKER_ARGS(sc, nr_to_scan, gfp_mask)  \
                      int nr_to_scan, gfp_t gfp_mask
#endif /* !HAVE_SHRINKER_WANT_SHRINK_PTR */
#define shrink_param(sc, var) (var)
#endif /* !HAVE_SHRINK_CONTROL */

#ifdef HAVE_REGISTER_SHRINKER
typedef int (*mtfs_shrinker_t)(SHRINKER_ARGS(sc, nr_to_scan, gfp_mask));

static inline
struct shrinker *mtfs_set_shrinker(int seek, mtfs_shrinker_t func)
{
        struct shrinker *s;

        s = kmalloc(sizeof(*s), GFP_KERNEL);
        if (s == NULL)
                return (NULL);

        s->shrink = func;
        s->seeks = seek;

        register_shrinker(s);

        return s;
}

static inline
void mtfs_remove_shrinker(struct shrinker *shrinker)
{
        if (shrinker == NULL)
                return;

        unregister_shrinker(shrinker);
        kfree(shrinker);
}
#else /* !HAVE_REGISTER_SHRINKER */
typedef shrinker_t mtfs_shrinker_t;
#define mtfs_set_shrinker(s, f)  set_shrinker(s, f)
#define mtfs_remove_shrinker(s)  remove_shrinker(s)
#endif /* !HAVE_REGISTER_SHRINKER */

struct masync_extent {
	/* Bucket belongs to, protected by mae_lock */
	struct masync_bucket       *mae_bucket;
	/* Info that belongs to, unchangeable */
	struct msubject_async_info *mae_info;
	/* Protect mae_bucket */
	mtfs_spinlock_t             mae_lock;
	/* Extent info, protected by mab_lock */
	struct mtfs_interval        mae_interval;
	/* Linkage to LRU list, protected by msai_lru_lock */
	mtfs_list_t                 mae_lru_linkage;
	/* Linkage to cancel list, protected by msa_cancel_lock */
	mtfs_list_t                 mae_cancel_linkage;
	/* Reference number */
	atomic_t                    mae_reference;
	/* Chunks, protected by mab_lock */
	mtfs_list_t                 mae_chunks;
};

#define masync_interval2extent(interval) \
    container_of(interval, struct masync_extent, mae_interval)

#define MASYNC_CHUNK_FLAG_DRITY 0x00000001 /* Have diry extents */
#define MASYNC_CHUNK_SIZE      (MTFS_INTERVAL_EOF)

struct masync_chunk {
	/* Bucket belongs to, protected by mac_bucket_lock */
	struct masync_bucket       *mac_bucket;
	/* Protect mac_bucket */
	mtfs_spinlock_t             mac_bucket_lock;
	/* Info that belongs to, unchangeable */
	struct msubject_async_info *mac_info;
	/* Linkage to bucket, protected by mab_chunk_lock */
	mtfs_list_t                 mac_linkage;
	/* Start offset, unchangeable */
	__u64                       mac_start;
	/* End offset, unchangeable */
	__u64                       mac_end;
	/* Length, unchangeable */
	__u64                       mac_size;
	/* Reference number */
	atomic_t                    mac_reference;
	/* Protect mac_flag */
	struct semaphore            mac_lock;
	/* Flags */
	__u32                       mac_flags;
};

struct masync_chunk_linkage {
	/* Linkage to extent */
	mtfs_list_t                 macl_linkage;
	struct masync_chunk        *macl_chunk;
};

struct masync_bucket {
	/* Info that belongs to, unchangeable */
	struct msubject_async_info *mab_info;
	/* Extent tree, protected by mab_lock */
	struct mtfs_interval_node  *mab_root;
	/* Extent number in tree, protected by mab_lock */
	atomic_t                    mab_number;
	/* File info for healing, protected by mab_lock */
	struct mtfs_file_info       mab_finfo;
	/* File info is valid or not, protected by mab_lock */
	int                         mab_fvalid;
	/* Protect tree, mab_number, mab_finfo and mab_fvalid */
	struct semaphore            mab_lock;
	/* Linkage to info, protected by msai_bucket_lock */
	mtfs_list_t                 mab_linkage;
	/* chunk list, protected by mab_chunk_lock*/
	mtfs_list_t                 mab_chunks;
	/* Protect mab_chunks, mac_linkage */
	struct semaphore            mab_chunk_lock;
};

#define MASYNC_BULK_SIZE (4096)

struct msubject_async_info {
	/* Subject that belongs to, unchangeable */
	struct msubject_async *msai_subject;
	/* Extents LRU list, protected by msai_lru_lock */
	mtfs_list_t            msai_lru_extents;
	/* Protect msai_lru_extents and mae_lru_linkage */
	mtfs_spinlock_t        msai_lru_lock;
	/* Number of extents in lru */
	atomic_t               msai_lru_number;
	/* Extents cancel list, protected by msai_bucket_lock */
	mtfs_list_t            msai_buckets;
	/* Protect msai_buckets */
	mtfs_spinlock_t        msai_bucket_lock;
	/* Linkage to subject, protected by msai_bucket_lock */
	mtfs_list_t            msai_linkage;
	/* Reference number */   
	atomic_t               msai_reference;
	/* Queue to wait on when refered */
	wait_queue_head_t      msai_waitq;
	/* Proc entrys for async debug */
	struct proc_dir_entry *msai_proc_entry;
};

struct msubject_async {
	/* Extents cancel list, protected by msa_cancel_lock */
	mtfs_list_t          msa_cancel_extents;
	/* Protect msa_cancel_extents and mae_cancel_linkage */
	mtfs_spinlock_t      msa_cancel_lock;
	/* Service that heal the async files */
	struct mtfs_service *msa_service;
	/* Extents cancel list, protected by msai_bucket_lock */
	mtfs_list_t          msa_infos;
	/* Protect msa_infos */
	mtfs_spinlock_t      msa_info_lock;
	/* Number of infos */
	atomic_t             msa_info_number;
};

extern struct mtfs_subject_operations masync_subject_ops;

extern const struct mtfs_io_operations masync_io_ops[];
#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_ASYNC_H__ */
