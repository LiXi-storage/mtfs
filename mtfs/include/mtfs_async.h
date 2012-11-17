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
# define SHRINKER_ARGS(sc, nr_to_scan, gfp_mask)  \
                       struct shrinker *shrinker, \
                       struct shrink_control *sc
# define shrink_param(sc, var) ((sc)->var)
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

struct masync_bucket {
	struct msubject_async_info *mab_info;    /* Info that belongs to */
	mtfs_list_t                 mab_linkage; /* Linkage to to info */
	atomic_t                    mab_nr;      /* Extent objects in tree */ 
	struct mtfs_interval_node  *mab_root;    /* Extent tree */
	struct semaphore            mab_lock;    /* Protect tree */
	struct mtfs_file_info       mab_finfo;   /* Fget for healing  */
	int                         mab_fvalid;  /* For debug  */
};

struct msubject_async_info {
	//struct shrinker          *msai_shrinker;      /* Can not for compat reasons */
	mtfs_list_t                 msai_dirty_buckets; /* Dirty buckets LRU list */
	struct rw_semaphore         msai_lock;          /* Protect msai_dirty_buckets */
	struct proc_dir_entry      *msai_proc_entry;    /* Proc entrys for async debug */
};

extern struct mtfs_subject_operations masync_subject_ops;

extern const struct mtfs_io_operations masync_io_ops[];
#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_ASYNC_H__ */
