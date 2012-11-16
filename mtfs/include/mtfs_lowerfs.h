/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_LOWERFS_H__
#define __MTFS_LOWERFS_H__

#include "mtfs_interval_tree.h"
#if defined (__linux__) && defined(__KERNEL__)

#include <linux/list.h>
#include <linux/fs.h>
#include <linux/mutex.h>

#define MLOWERFS_XATTR_NAME_FLAG    "trusted.mtfs.flag"
#define MLOWERFS_XATTR_NAME_BUCKET  "trusted.mtfs.bucket"

#define MLOWERFS_FLAG_RMDIR_NO_DDLETE  0x00000001 /* When rmdir lowerfs, avoid d_delete in vfs_rmdir */
#define MLOWERFS_FLAG_UNLINK_NO_DDLETE 0x00000002 /* When unlink lowerfs, avoid d_delete in vfs_rmdir */
#define MLOWERFS_FLAG_ALL (MLOWERFS_FLAG_RMDIR_NO_DDLETE | MLOWERFS_FLAG_UNLINK_NO_DDLETE)

static inline int mlowerfs_flag_is_valid(unsigned long flag)
{
	if (flag & (~MLOWERFS_FLAG_ALL)) {
		return 0;
	}
	return 1;
}

struct mtfs_lowerfs {
	struct list_head ml_linkage;
	struct module   *ml_owner;
	const char      *ml_type;
	unsigned long    ml_magic; /* The same with sb->s_magic */
	unsigned long    ml_flag;

	/* Operations that should be provided */
	int (* ml_setflag)(struct inode *inode, __u32 flag);
	int (* ml_getflag)(struct inode *inode, __u32 *mtfs_flag);
};

/* lowerfs bucket types */
typedef enum {
	MLOWERFS_BUCKET_TYPE_MIN    = 0,
	MLOWERFS_BUCKET_TYPE_XATTR  = 1,
	MLOWERFS_BUCKET_TYPE_MAX
} mlowerfs_bucket_type_t;

struct mlowerfs_bucket;

struct mlowerfs_bucket_type_object {
	mlowerfs_bucket_type_t mbto_type;
	int  (* mbto_init)(struct mlowerfs_bucket *bucket);  /* Init bucket */
	int  (* mbto_read)(struct mlowerfs_bucket *bucket);  /* Read bucket from lowerfs */
	int  (* mbto_flush)(struct mlowerfs_bucket *bucket); /* Flush bucket to lowerfs */
};

extern struct mlowerfs_bucket_type_object mlowerfs_bucket_xattr;

#define MLOWERFS_BUCKET_TYPE_DEFAULT (&mlowerfs_bucket_xattr)


extern int mlowerfs_register(struct mtfs_lowerfs *fs_ops);
extern void mlowerfs_unregister(struct mtfs_lowerfs *fs_ops);
extern int mlowerfs_getflag_xattr(struct inode *inode, __u32 *mtfs_flag, const char *xattr_name);
extern int mlowerfs_setflag_xattr(struct inode *inode, __u32 mtfs_flag, const char *xattr_name);
extern int mlowerfs_getflag_default(struct inode *inode, __u32 *mtfs_flag);
extern int mlowerfs_setflag_default(struct inode *inode, __u32 mtfs_flag);
#endif /* defined (__linux__) && defined(__KERNEL__) */

#define MLOWERFS_BUCKET_NUMBER (64)

struct mlowerfs_slot {
	int                              mi_used;
	struct mtfs_interval_node_extent mi_extent;
};

struct mlowerfs_disk_bucket {
	__u64                               mdb_generation; /* Generation of bucket */
	__u64                               mdb_reference;
	struct mlowerfs_slot                mdb_slots[MLOWERFS_BUCKET_NUMBER];
};

struct mlowerfs_bucket {
#if defined (__linux__) && defined(__KERNEL__)
	struct mlowerfs_bucket_type_object *mb_type;
	struct semaphore                    mb_lock; /* Lock */
#endif /* defined (__linux__) && defined(__KERNEL__) */
	struct mlowerfs_disk_bucket         mb_disk;
	int                                 mb_inited; /* $mb_disk is inited or not */
	int                                 mb_dirty; /* $mb_disk is flushed or not */
};

#define mlowerfs_bucket2type(bucket)             (bucket->mb_type)
#define mlowerfs_bucket2lock(bucket)             (&bucket->mb_lock)
#define mlowerfs_bucket2disk(bucket)             (&bucket->mb_disk)

#define mlowerfs_bucket2generation(bucket)       (mlowerfs_bucket2disk(bucket)->mdb_generation)
#define mlowerfs_bucket2reference(bucket)        (mlowerfs_bucket2disk(bucket)->mdb_reference)
#define mlowerfs_bucket2slots(bucket)            (mlowerfs_bucket2disk(bucket)->mdb_slots)

#define mlowerfs_bucket2slot(bucket, bindex)     (mlowerfs_bucket2slots(bucket)[bindex])
#define mlowerfs_bucket2s_used(bucket, bindex)   (mlowerfs_bucket2slot(bucket, bindex).mi_used)
#define mlowerfs_bucket2s_extent(bucket, bindex) (mlowerfs_bucket2slot(bucket, bindex).mi_extent)
#define mlowerfs_bucket2s_start(bucket, bindex)  (mlowerfs_bucket2s_extent(bucket, bindex).start)
#define mlowerfs_bucket2s_end(bucket, bindex)    (mlowerfs_bucket2s_extent(bucket, bindex).end)

#if !defined (__KERNEL__)
int _mlowerfs_bucket_add(struct mlowerfs_bucket *bucket,
                         struct mtfs_interval_node_extent *extent);
void _mlowerfs_bucket_dump(struct mlowerfs_bucket *bucket);
int _mlowerfs_bucket_is_valid(struct mlowerfs_bucket *bucket);
int _mlowerfs_bucket_check(struct mlowerfs_bucket *bucket, __u64 position);
#endif /* !defined (__KERNEL__) */

#endif /* __MTFS_LOWERFS_H__ */
