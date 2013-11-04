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
	struct masync_bucket       *mae_bucket;         /* Bucket belongs to, protected by mae_lock */
	struct msubject_async_info *mae_info;           /* Info that belongs to, unchangeable */
	mtfs_spinlock_t             mae_lock;           /* Protect mae_bucket */
	struct mtfs_interval        mae_interval;       /* Extent info, unchangeable */
	mtfs_list_t                 mae_lru_linkage;    /* Linkage to LRU list, protected by msai_lru_lock */
	mtfs_list_t                 mae_cancel_linkage; /* Linkage to cancel list, protected by msa_cancel_lock */
	atomic_t                    mae_reference;      /* Reference number */
};

#define masync_interval2extent(interval) container_of(interval, struct masync_extent, mae_interval)

struct masync_bucket {
	struct msubject_async_info *mab_info;    /* Info that belongs to, unchangeable */
	struct mtfs_interval_node  *mab_root;    /* Extent tree, protected by mab_lock */ 
	int                         mab_number;  /* Extent number in tree, protected by mab_lock */ 
	struct mtfs_file_info       mab_finfo;   /* File info for healing, protected by mab_lock */
	int                         mab_fvalid;  /* File info is valid or not, protected by mab_lock */
	struct semaphore            mab_lock;    /* Protect tree, mab_number, mab_finfo and mab_fvalid */
	mtfs_list_t                 mab_linkage; /* Linkage to info, protected by msai_bucket_lock */
};

#define MASYNC_BULK_SIZE (4096)

struct msubject_async_info {
	struct msubject_async *msai_subject;     /* Subject that belongs to, unchangeable */
	mtfs_list_t            msai_lru_extents; /* Extents LRU list, protected by msai_lru_lock */
	mtfs_spinlock_t        msai_lru_lock;    /* Protect msai_lru_extents and mae_lru_linkage */
	atomic_t               msai_lru_number;  /* Number of extents in lru */
	mtfs_list_t            msai_buckets;     /* Extents cancel list, protected by msai_bucket_lock */
	mtfs_spinlock_t        msai_bucket_lock; /* Protect msai_buckets */
	mtfs_list_t            msai_linkage;     /* Linkage to subject, protected by msai_bucket_lock */
	atomic_t               msai_reference;   /* Reference number */
	wait_queue_head_t      msai_waitq;       /* Queue to wait on when refered */
	struct proc_dir_entry *msai_proc_entry;  /* Proc entrys for async debug */
};

struct msubject_async {
	mtfs_list_t          msa_cancel_extents; /* Extents cancel list, protected by msa_cancel_lock */
	mtfs_spinlock_t      msa_cancel_lock;    /* Protect msa_cancel_extents and mae_cancel_linkage */
	struct mtfs_service *msa_service;        /* Service that heal the async files */ 
	mtfs_list_t          msa_infos;          /* Extents cancel list, protected by msai_bucket_lock */
	mtfs_spinlock_t      msa_info_lock;      /* Protect msa_infos */
	atomic_t             msa_info_number;    /* Number of infos */
};

extern struct mtfs_subject_operations masync_subject_ops;

extern const struct mtfs_io_operations masync_io_ops[];
#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_ASYNC_H__ */
