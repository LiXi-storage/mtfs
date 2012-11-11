/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <mtfs_lowerfs.h>
#include <debug.h>

#if !defined(__KERNEL__)
#include <string.h>
#endif

void _mlowerfs_bucket_dump(struct mlowerfs_bucket *bucket)
{
	int i = 0;
	struct mtfs_interval_node_extent last_extent = {0, 0};
	int inited = 0;
	int last_id = 0;
	int print_id = 0;
	MENTRY();

	for (i = 0; i < MLOWERFS_BUCKET_NUMBER; i++) {
		if (!bucket->mb_intervals[i].mi_used) {
			continue;
		}

		if (inited) {
			if (print_id) {
				MPRINT("%d[%llu, %llu], ",
				       last_id, last_extent.start,
				       last_extent.end);
			} else {
				MPRINT("[%llu, %llu], ",
				       last_extent.start,
				       last_extent.end);
			}
		}
		last_extent = bucket->mb_intervals[i].mi_extent;
		last_id = i;
		inited = 1;
	}
	if (print_id) {
		MPRINT("%d[%llu, %llu]",
		       last_id, last_extent.start,
		       last_extent.end);
	} else {
		MPRINT("[%llu, %llu]",
		       last_extent.start,
		       last_extent.end);
	}
	MPRINT("\n");

	_MRETURN();
}

int _mlowerfs_bucket_is_valid(struct mlowerfs_bucket *bucket)
{
	int i = 0;
	struct mtfs_interval_node_extent last_extent = {0, 0};
	int inited = 0;
	int ret = 0;
	MENTRY();

	for (i = 0; i < MLOWERFS_BUCKET_NUMBER; i++) {
		if (!bucket->mb_intervals[i].mi_used) {
			continue;
		}

		if (inited) {
			MASSERT(last_extent.start <= last_extent.end);
			MASSERT(last_extent.end <= bucket->mb_intervals[i].mi_extent.start);
			last_extent = bucket->mb_intervals[i].mi_extent;
			inited = 1;
		}
	}

	MRETURN(ret);
}

int _mlowerfs_bucket_check(struct mlowerfs_bucket *bucket, __u64 position)
{
	int i = 0;
	int ret = 0;
	MENTRY();

	for (i = 0; i < MLOWERFS_BUCKET_NUMBER; i++) {
		if (!bucket->mb_intervals[i].mi_used) {
			continue;
		}

		if (position > bucket->mb_intervals[i].mi_extent.end) {
			continue;
		} else if (position < bucket->mb_intervals[i].mi_extent.start) {
			break;
		} else {
			ret = 1;
			break;
		}
	}

	MRETURN(ret);
}

/*
 * Shift idle seat to [0]
 * return:     -1 if failed
 *             0  else
 */
int _mlowerfs_bucket_shift_head(struct mlowerfs_bucket *bucket)
{
	int i = 0;
	int ret = 0;
	MENTRY();

	for (i = 0; i < MLOWERFS_BUCKET_NUMBER; i++) {
		if (!bucket->mb_intervals[i].mi_used) {
			break;
		}
	}

	if (i == MLOWERFS_BUCKET_NUMBER) {
		ret = -1;
		goto out;
	}

	if (i != 0) {
		memmove(&bucket->mb_intervals[1],
		        &bucket->mb_intervals[0],
		        sizeof(struct mlowerfs_interval) * i);
	}
out:
	MRETURN(ret);
}

/*
 * Shift idle seat to [MLOWERFS_BUCKET_NUMBER - 1]
 * return:     -1 if failed
 *             0  else
 */
int _mlowerfs_bucket_shift_tail(struct mlowerfs_bucket *bucket)
{
	int i = 0;
	int ret = 0;
	MENTRY();

	for (i = 0; i < MLOWERFS_BUCKET_NUMBER; i++) {
		if (!bucket->mb_intervals[MLOWERFS_BUCKET_NUMBER - 1 - i].mi_used) {
			break;
		}
	}

	if (i == MLOWERFS_BUCKET_NUMBER) {
		ret = -1;
		goto out;
	}

	if (i != 0) {
		memmove(&bucket->mb_intervals[MLOWERFS_BUCKET_NUMBER - 1 - i],
		        &bucket->mb_intervals[MLOWERFS_BUCKET_NUMBER - i],
		        sizeof(struct mlowerfs_interval) * i);
	}
out:
	MRETURN(ret);
}

/*
 * Shift idle seat for not overlaped extent
 * $head_index: -1 for add to head 
 *              MLOWERFS_BUCKET_NUMBER for adding to tail
 *              [0, MLOWERFS_BUCKET_NUMBER - 2] for middle
 */
int _mlowerfs_bucket_shift_idle(struct mlowerfs_bucket *bucket, int head_index)
{
	int selected_index = -1;
	int i = 0;
	MENTRY();

	if (head_index == -1) {
		/* head */
		if (_mlowerfs_bucket_shift_head(bucket) != 0) {
			goto out;
		}
		selected_index = 0;
	} else if (head_index == MLOWERFS_BUCKET_NUMBER) {
		/* Tail */
		if (_mlowerfs_bucket_shift_tail(bucket) != 0) {
			goto out;
		}
		selected_index = MLOWERFS_BUCKET_NUMBER - 1;
	} else {
		/* Middle */
		MASSERT(head_index >= 0);
		MASSERT(head_index < MLOWERFS_BUCKET_NUMBER - 1);

		for (i = head_index; i >= 0; i--) {
			if (!bucket->mb_intervals[i].mi_used) {
				break;
			}
		}
		if (i >= 0) {
			if (i < head_index) {
				memmove(&bucket->mb_intervals[i],
			 	       &bucket->mb_intervals[i + 1],
			 	       sizeof(struct mlowerfs_interval) * (head_index - i));
			}
			selected_index = head_index;
		} else {
			/* This is the hardest condition to cover for tests */
			for (i = head_index + 1; i < MLOWERFS_BUCKET_NUMBER; i++) {
				if (!bucket->mb_intervals[i].mi_used) {
					break;
				}
			}

			if (i == MLOWERFS_BUCKET_NUMBER) {
				goto out;
			}
			
			if (i - head_index - 1 != 0) {
				MASSERT(head_index + 2 < MLOWERFS_BUCKET_NUMBER);
				memmove(&bucket->mb_intervals[head_index + 1],
				        &bucket->mb_intervals[head_index + 2],
				        sizeof(struct mlowerfs_interval) * (i - head_index - 1));
			}
			selected_index = head_index + 1;
		}
	}

	MASSERT(selected_index >= 0);
	MASSERT(selected_index < MLOWERFS_BUCKET_NUMBER);
	bucket->mb_intervals[selected_index].mi_used = 0;

out:
	MRETURN(selected_index);
}

int _mlowerfs_bucket_add(struct mlowerfs_bucket *bucket,
                         struct mtfs_interval_node_extent *extent)
{
	int ret = 0;
	__u64 tmp_start = 0;
	__u64 tmp_end = 0;
	__u64 start = 0;
	__u64 end = 0;
	int start_inited = 0;
	int end_inited = 0;
	int start_index = MLOWERFS_BUCKET_NUMBER;
	int end_index = MLOWERFS_BUCKET_NUMBER;
	int selected_index = -1;
	int i = 0;	
	__u64 min_interval = MTFS_INTERVAL_EOF;
	__u64 tmp_interval = 0;
	__u64 head_index = 0;
	MENTRY();

	MASSERT(extent->start <= extent->end);
	/* Collect information */
	for (i = 0; i < MLOWERFS_BUCKET_NUMBER; i++) {
		if (!bucket->mb_intervals[i].mi_used) {
			continue;
		}

		tmp_start = bucket->mb_intervals[i].mi_extent.start;
		tmp_end = bucket->mb_intervals[i].mi_extent.end;
		if (start_index == MLOWERFS_BUCKET_NUMBER) {
			if (extent->start < tmp_start) {
				start_inited = 1;
				start = extent->start;
				start_index = i - 1;
			} else if (extent->start >= tmp_start &&
			           extent->start <= tmp_end) {
				start_inited = 1;
				start = tmp_start;
				start_index = i;
			}
		}
		if (start_index != MLOWERFS_BUCKET_NUMBER &&
		    end_index == MLOWERFS_BUCKET_NUMBER) {
			if (extent->end < tmp_start) {
				end_inited = 1;
				end = extent->end;
				end_index = i - 1;
				break;
			} else if (extent->end >= tmp_start &&
			           extent->end <= tmp_end) {
				end_inited = 1;
				end = tmp_end;
				end_index = i;
				break;
			}
		}
	}

	/* Select overlaped intervals */
	for (i = (0 > start_index ? 0 : start_index);
	     i < (MLOWERFS_BUCKET_NUMBER < end_index + 1 ? MLOWERFS_BUCKET_NUMBER : end_index + 1);
	     i++) {
		if (!bucket->mb_intervals[i].mi_used) {
			continue;
		}

		if (extent_overlapped(extent, &bucket->mb_intervals[i].mi_extent)) {
		    	/* Overlaped */
			bucket->mb_intervals[i].mi_used = 0;
			if (selected_index == -1) {
				selected_index = i;
			}
		}
	}

	if (!end_inited) {
		end_inited = 1;
		end = extent->end;
	}

	if (selected_index != -1) {
		MASSERT(start_inited);
		goto out_selected;
	}

	MASSERT(start_index == end_index);
	if (!start_inited) {
		start_inited = 1;
		start = extent->start;
	}
      
	MASSERT(start_inited);
	MASSERT(end_inited);
	MASSERT(start == extent->start);
	MASSERT(end == extent->end);
	selected_index = _mlowerfs_bucket_shift_idle(bucket, start_index);
	if (selected_index != -1) {
		goto out_selected;
	}

	/* All intervals is used */
	if (start_index == -1) {
		head_index = -1;
		min_interval = bucket->mb_intervals[0].mi_extent.start - extent->end;
		end =  bucket->mb_intervals[0].mi_extent.end;
	}

	/* Get min interval */
	for (i = 0; i < MLOWERFS_BUCKET_NUMBER - 1; i++) {
		tmp_interval = bucket->mb_intervals[i + 1].mi_extent.start -
		               bucket->mb_intervals[i].mi_extent.end;
		if (min_interval > tmp_interval) {
			min_interval = tmp_interval;
			head_index = i;
		}
	}

	/* New interval can be merged to a neighbour */
	if (start_index == -1) {
		if (head_index == -1) {
			/* Merge to head */
			bucket->mb_intervals[0].mi_extent.start = 
				         extent->start;
			goto out;
		}
	} else if (start_index == MLOWERFS_BUCKET_NUMBER) {
		tmp_interval = extent->start -
		               bucket->mb_intervals[MLOWERFS_BUCKET_NUMBER - 1].mi_extent.end;
		if (min_interval > tmp_interval) {
			/* Merge to tail */
			bucket->mb_intervals[MLOWERFS_BUCKET_NUMBER - 1].mi_extent.end = 
				         extent->end;
		}
	} else {
		MASSERT(start_index >= 0);
		MASSERT(start_index < MLOWERFS_BUCKET_NUMBER - 1);
		tmp_interval = extent->start -
			       bucket->mb_intervals[start_index].mi_extent.end;
		if (min_interval > tmp_interval) {
			min_interval = tmp_interval;
				tmp_interval = bucket->mb_intervals[start_index + 1].mi_extent.start -
			                       extent->end;
			if (min_interval > tmp_interval) {
				/* Merge to $start_index + 1 */
				bucket->mb_intervals[start_index + 1].mi_extent.start = 
				         extent->start;
			} else {
				/* Merge to $start_index */
				bucket->mb_intervals[start_index].mi_extent.end = 
				         extent->end;
			}
			goto out;
		}
	}

	/* Merge old intervals and then insert new interval */
	MASSERT(head_index >= 0);
	MASSERT(head_index < MLOWERFS_BUCKET_NUMBER - 1);
	bucket->mb_intervals[head_index].mi_extent.end =
	        bucket->mb_intervals[head_index + 1].mi_extent.end;
	bucket->mb_intervals[head_index + 1].mi_used = 0;
	selected_index = _mlowerfs_bucket_shift_idle(bucket, start_index);
	MASSERT(selected_index != -1);

out_selected:
	MASSERT(start_inited);
	MASSERT(end_inited);
	MASSERT(!bucket->mb_intervals[selected_index].mi_used);
	bucket->mb_intervals[selected_index].mi_extent.start = start;
	bucket->mb_intervals[selected_index].mi_extent.end = end;
	bucket->mb_intervals[selected_index].mi_used = 1;
out:
	MRETURN(ret);
}

#if defined (__linux__) && defined(__KERNEL__)

#include <linux/module.h>
#include "lowerfs_internal.h"

spinlock_t mtfs_lowerfs_lock = SPIN_LOCK_UNLOCKED;
LIST_HEAD(ml_types);

static struct mtfs_lowerfs *_mlowerfs_search(const char *type)
{
	struct mtfs_lowerfs *found = NULL;
	struct list_head *p = NULL;
	MENTRY();

	list_for_each(p, &ml_types) {
		found = list_entry(p, struct mtfs_lowerfs, ml_linkage);
		if (strcmp(found->ml_type, type) == 0) {
			MRETURN(found);
		}
	}

	MRETURN(NULL);
}

static int mlowerfs_check(struct mtfs_lowerfs *lowerfs)
{
	int ret = 0;
	MENTRY();

	if (lowerfs->ml_owner == NULL ||
	    lowerfs->ml_type == NULL ||
	    lowerfs->ml_magic == 0 ||
	    !mlowerfs_flag_is_valid(lowerfs->ml_flag) ||
	    lowerfs->ml_setflag == NULL ||
	    lowerfs->ml_getflag == NULL) {
	    	ret = -EINVAL;
	}

	MRETURN(ret);
}

static int _mlowerfs_register(struct mtfs_lowerfs *fs_ops)
{
	struct mtfs_lowerfs *found = NULL;
	int ret = 0;
	MENTRY();

	ret = mlowerfs_check(fs_ops);
	if (ret){
		MERROR("lowerfs is not valid\n");
		goto out;
	}

	if ((found = _mlowerfs_search(fs_ops->ml_type))) {
		if (found != fs_ops) {
			MERROR("try to register multiple operations for type %s\n",
			        fs_ops->ml_type);
		} else {
			MERROR("operation of type %s has already been registered\n",
			        fs_ops->ml_type);
		}
		ret = -EALREADY;
	} else {
		//PORTAL_MODULE_USE;
		list_add(&fs_ops->ml_linkage, &ml_types);
	}

out:
	MRETURN(ret);
}

int mlowerfs_register(struct mtfs_lowerfs *fs_ops)
{
	int ret = 0;
	MENTRY();

	spin_lock(&mtfs_lowerfs_lock);
	ret = _mlowerfs_register(fs_ops);
	spin_unlock(&mtfs_lowerfs_lock);

	MRETURN(ret);
}
EXPORT_SYMBOL(mlowerfs_register);

static void _mlowerfs_unregister(struct mtfs_lowerfs *fs_ops)
{
	struct list_head *p = NULL;
	MENTRY();

	list_for_each(p, &ml_types) {
		struct mtfs_lowerfs *found;

		found = list_entry(p, typeof(*found), ml_linkage);
		if (found == fs_ops) {
			list_del(p);
			break;
		}
	}

	_MRETURN();
}

void mlowerfs_unregister(struct mtfs_lowerfs *fs_ops)
{
	MENTRY();

	spin_lock(&mtfs_lowerfs_lock);
	_mlowerfs_unregister(fs_ops);
	spin_unlock(&mtfs_lowerfs_lock);

	_MRETURN();
}
EXPORT_SYMBOL(mlowerfs_unregister);

static struct mtfs_lowerfs *_mlowerfs_get(const char *type)
{
	struct mtfs_lowerfs *found = NULL;
	MENTRY();

	spin_lock(&mtfs_lowerfs_lock);
	found = _mlowerfs_search(type);
	if (found == NULL) {
		MERROR("failed to found lowerfs support for type %s\n", type);
	} else {
		if (try_module_get(found->ml_owner) == 0) {
			MERROR("failed to get module for lowerfs support for type %s\n", type);
			found = ERR_PTR(-EOPNOTSUPP);
		}
	}	
	spin_unlock(&mtfs_lowerfs_lock);

	MRETURN(found);
}

struct mtfs_lowerfs *mlowerfs_get(const char *type)
{
	struct mtfs_lowerfs *found = NULL;
	char module_name[32];
	MENTRY();

	found = _mlowerfs_get(type);
	if (found == NULL) {
		if (strcmp(type, "ext2") == 0 ||
		    strcmp(type, "ext3") == 0 ||
		    strcmp(type, "ext4") == 0) {
			snprintf(module_name, sizeof(module_name) - 1, "mtfs_ext");
		} else {
			snprintf(module_name, sizeof(module_name) - 1, "mtfs_%s", type);
		}

		if (request_module(module_name) != 0) {
			found = ERR_PTR(-ENOENT);
			MERROR("failed to load module %s\n", module_name);
		} else {
			found = _mlowerfs_get(type);
			if (found == NULL || IS_ERR(found)) {
				MERROR("failed to get lowerfs support after loading module %s, "
				       "type = %s\n",
			               module_name, type);
				found = found == NULL ? ERR_PTR(-EOPNOTSUPP) : found;
			}
		}
	} else if (IS_ERR(found)) {
		MERROR("failed to get lowerfs support for type %s\n", type);
	}

	MRETURN(found);
}

void mlowerfs_put(struct mtfs_lowerfs *fs_ops)
{
	MENTRY();

	module_put(fs_ops->ml_owner);

	_MRETURN();
}

int mlowerfs_getflag_xattr(struct inode *inode, __u32 *mtfs_flag, const char *xattr_name)
{
	struct dentry de = { .d_inode = inode };
	int ret = 0;
	int need_unlock = 0;
	__u32 disk_flag = 0;
	MENTRY();

	MASSERT(inode);
	MASSERT(inode->i_op);
	MASSERT(inode->i_op->getxattr);

	if (!inode_is_locked(inode)) {
		mutex_lock(&inode->i_mutex);
		need_unlock = 1;
	}

	ret = inode->i_op->getxattr(&de, xattr_name,
                              &disk_flag, sizeof(disk_flag));
	if (ret == -ENODATA) {
		MDEBUG("not set\n");
		*mtfs_flag = 0;
		goto out_succeeded;
	} else {
		if (unlikely(ret != sizeof(disk_flag))) {
			if (ret >= 0) {
				MERROR("getxattr ret = %d, expect %ld\n", ret, sizeof(disk_flag));
				ret = -EINVAL;
			} else {
				MERROR("getxattr ret = %d\n", ret);
			}
			goto out;
		}
	}

	if (unlikely(!mtfs_disk_flag_is_valid(disk_flag))) {
		MERROR("disk_flag 0x%x is not valid\n", disk_flag);
		ret = -EINVAL;
		goto out;
	}
	*mtfs_flag = disk_flag & MTFS_FLAG_DISK_MASK;
	MDEBUG("ret = %d, mtfs_flag = 0x%x\n", ret, *mtfs_flag);
out_succeeded:
	ret = 0;
out:
	if (need_unlock) {
		mutex_unlock(&inode->i_mutex);
	}
	MRETURN(ret);
}
EXPORT_SYMBOL(mlowerfs_getflag_xattr);

int mlowerfs_getflag_default(struct inode *inode, __u32 *mtfs_flag)
{
	int ret = 0;
	MENTRY();

	ret = mlowerfs_getflag_xattr(inode, mtfs_flag, XATTR_NAME_MTFS_FLAG);

	MRETURN(ret);
}
EXPORT_SYMBOL(mlowerfs_getflag_default);

int mlowerfs_setflag_xattr(struct inode *inode, __u32 mtfs_flag, const char *xattr_name)
{
	int ret = 0;
	struct dentry de = { .d_inode = inode };
	int flag = 0; /* Should success all the time, so NO XATTR_CREATE or XATTR_REPLACE */
	__u32 disk_flag = 0;
	int need_unlock = 0;
	MENTRY();

	MASSERT(inode);
	if (!inode_is_locked(inode)) {
		mutex_lock(&inode->i_mutex);
		need_unlock = 1;
	}

	if(unlikely(!mtfs_flag_is_valid(mtfs_flag))) {
		MERROR("0x%x is not a valid mtfs_flag\n", mtfs_flag);
		ret = -EPERM;
		goto out;
	}

	disk_flag = mtfs_flag | MTFS_FLAG_DISK_SYMBOL;
	if (unlikely(!inode->i_op || !inode->i_op->setxattr)) {
		ret = -EPERM;
		goto out;
	}

	ret = inode->i_op->setxattr(&de, xattr_name,
                             &disk_flag, sizeof(disk_flag), flag);
	if (ret != 0) {
		MERROR("setxattr failed, rc = %d\n", ret);
		goto out;
	}

out:
	if (need_unlock) {
		mutex_unlock(&inode->i_mutex);
	}
	MRETURN(ret);
}
EXPORT_SYMBOL(mlowerfs_setflag_xattr);

int mlowerfs_setflag_default(struct inode *inode, __u32 mtfs_flag)
{
	int ret = 0;
	MENTRY();

	ret = mlowerfs_setflag_xattr(inode, mtfs_flag, XATTR_NAME_MTFS_FLAG);

	MRETURN(ret);
}
EXPORT_SYMBOL(mlowerfs_setflag_default);

static inline void mlowerfs_bucket_lock_init(struct mlowerfs_bucket *bucket)
{
	init_MUTEX(&bucket->mb_lock);
}

static inline void mlowerfs_bucket_lock(struct mlowerfs_bucket *bucket)
{
	down(&bucket->mb_lock);
}

static inline void mlowerfs_bucket_unlock(struct mlowerfs_bucket *bucket)
{
	up(&bucket->mb_lock);
}

int mlowerfs_bucket_init(struct mlowerfs_bucket *bucket)
{
	int ret = 0;
	MENTRY();

	mlowerfs_bucket_lock_init(bucket);
	bucket->mb_type = MLOWERFS_BUCKET_TYPE_DEFAULT;

	MRETURN(ret);
}

int mlowerfs_bucket_read(struct mlowerfs_bucket *bucket)
{
	int ret = 0;
	int i = 0;
	struct inode *inode = NULL;
	MENTRY();

	mlowerfs_bucket_lock(bucket);
	inode = mlowerfs_bucket2inode(bucket);

	if (bucket->mb_type->mbto_read) {
		bucket->mb_type->mbto_read(bucket);
	}

	for (i = 0; i < MLOWERFS_BUCKET_NUMBER; i++) {
		bucket->mb_intervals[i].mi_used = 0;
	}
	mlowerfs_bucket_unlock(bucket);

	MRETURN(ret);
}

int mlowerfs_bucket_add(struct mlowerfs_bucket *bucket,
                         struct mtfs_interval_node_extent *extent)
{
	int ret = 0;
	MENTRY();

	MASSERT(extent->start <= extent->end);
	mlowerfs_bucket_lock(bucket);
	_mlowerfs_bucket_add(bucket, extent);
	mlowerfs_bucket_unlock(bucket);

	MRETURN(ret);
}

struct mlowerfs_bucket_type_object mlowerfs_bucket_xattr = {
	.mbto_type     = MLOWERFS_BUCKET_TYPE_XATTR,
	.mbto_read     = NULL,
	.mbto_flush    = NULL,
};

#endif /* !defined (__linux__) && defined(__KERNEL__) */
