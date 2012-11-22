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
		if (!mlowerfs_bucket2s_used(bucket, i)) {
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
		last_extent = mlowerfs_bucket2s_extent(bucket, i);
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
		if (!mlowerfs_bucket2s_used(bucket, i)) {
			continue;
		}

		if (inited) {
			MASSERT(mlowerfs_bucket2s_start(bucket, i) <= mlowerfs_bucket2s_end(bucket, i));
			MASSERT(last_extent.end < mlowerfs_bucket2s_start(bucket, i));
			last_extent = mlowerfs_bucket2s_extent(bucket, i);
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
		if (!mlowerfs_bucket2s_used(bucket, i)) {
			continue;
		}

		if (position > mlowerfs_bucket2s_end(bucket, i)) {
			continue;
		} else if (position < mlowerfs_bucket2s_start(bucket, i)) {
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
		if (!mlowerfs_bucket2s_used(bucket, i)) {
			break;
		}
	}

	if (i == MLOWERFS_BUCKET_NUMBER) {
		ret = -1;
		goto out;
	}

	if (i != 0) {
		memmove(&mlowerfs_bucket2slot(bucket, 1),
		        &mlowerfs_bucket2slot(bucket, 0),
		        sizeof(struct mlowerfs_slot) * i);
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
		if (!mlowerfs_bucket2s_used(bucket, MLOWERFS_BUCKET_NUMBER - 1 - i)) {
			break;
		}
	}

	if (i == MLOWERFS_BUCKET_NUMBER) {
		ret = -1;
		goto out;
	}

	if (i != 0) {
		memmove(&mlowerfs_bucket2slot(bucket, MLOWERFS_BUCKET_NUMBER - 1 - i),
		        &mlowerfs_bucket2slot(bucket, MLOWERFS_BUCKET_NUMBER - i),
		        sizeof(struct mlowerfs_slot) * i);
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
			if (!mlowerfs_bucket2s_used(bucket, i)) {
				break;
			}
		}
		if (i >= 0) {
			if (i < head_index) {
				memmove(&mlowerfs_bucket2slot(bucket, i),
			 	       &mlowerfs_bucket2slot(bucket, i + 1),
			 	       sizeof(struct mlowerfs_slot) * (head_index - i));
			}
			selected_index = head_index;
		} else {
			/* This is the hardest condition to cover for tests */
			for (i = head_index + 1; i < MLOWERFS_BUCKET_NUMBER; i++) {
				if (!mlowerfs_bucket2s_used(bucket, i)) {
					break;
				}
			}

			if (i == MLOWERFS_BUCKET_NUMBER) {
				goto out;
			}
			
			if (i - head_index - 1 != 0) {
				MASSERT(head_index + 2 < MLOWERFS_BUCKET_NUMBER);
				memmove(&mlowerfs_bucket2slot(bucket, head_index + 1),
				        &mlowerfs_bucket2slot(bucket, head_index + 2),
				        sizeof(struct mlowerfs_slot) * (i - head_index - 1));
			}
			selected_index = head_index + 1;
		}
	}

	MASSERT(selected_index >= 0);
	MASSERT(selected_index < MLOWERFS_BUCKET_NUMBER);
	mlowerfs_bucket2s_used(bucket, selected_index) = 0;

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
	struct mtfs_interval_node_extent last_extent = {0, 0};
	int last_inited = 0;
	MENTRY();

	MASSERT(extent->start <= extent->end);
	/* Collect information */
	for (i = 0; i < MLOWERFS_BUCKET_NUMBER; i++) {
		if (!mlowerfs_bucket2s_used(bucket, i)) {
			continue;
		}

		tmp_start = mlowerfs_bucket2s_start(bucket, i);
		tmp_end = mlowerfs_bucket2s_end(bucket, i);
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
		if (!mlowerfs_bucket2s_used(bucket, i)) {
			continue;
		}

		if (extent_overlapped(extent, &mlowerfs_bucket2s_extent(bucket, i))) {
		    	/* Overlaped */
			mlowerfs_bucket2s_used(bucket, i) = 0;
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
		min_interval = mlowerfs_bucket2s_start(bucket, 0) - extent->end;
		end =  mlowerfs_bucket2s_end(bucket, 0);
	}

	/* Get min interval */
	for (i = 0; i < MLOWERFS_BUCKET_NUMBER - 1; i++) {
		if (!mlowerfs_bucket2s_used(bucket, i)) {
			continue;
		}

		if (last_inited) {
			tmp_interval = mlowerfs_bucket2s_start(bucket, i) - last_extent.end;
			MASSERT(tmp_interval > 0);

			if (min_interval > tmp_interval) {
				min_interval = tmp_interval;
				head_index = i;
			}

			last_extent = mlowerfs_bucket2s_extent(bucket, i);
			last_inited = 1;
		}
	}

	/* New interval can be merged to a neighbour */
	if (start_index == -1) {
		if (head_index == -1) {
			/* Merge to head */
			 mlowerfs_bucket2s_start(bucket, 0) = extent->start;
			goto out;
		}
	} else if (start_index == MLOWERFS_BUCKET_NUMBER) {
		tmp_interval = extent->start -
		               mlowerfs_bucket2s_end(bucket, MLOWERFS_BUCKET_NUMBER - 1);
		MASSERT(tmp_interval > 0);
		if (min_interval > tmp_interval) {
			/* Merge to tail */
			 mlowerfs_bucket2s_end(bucket, MLOWERFS_BUCKET_NUMBER - 1) = extent->end;
		}
	} else {
		MASSERT(start_index >= 0);
		MASSERT(start_index < MLOWERFS_BUCKET_NUMBER - 1);
		tmp_interval = extent->start - mlowerfs_bucket2s_end(bucket, start_index);
		MASSERT(tmp_interval > 0);
		if (min_interval > tmp_interval) {
			min_interval = tmp_interval;
				tmp_interval = mlowerfs_bucket2s_start(bucket, start_index + 1) -
			                       extent->end;
			MASSERT(tmp_interval > 0);
			if (min_interval > tmp_interval) {
				/* Merge to $start_index + 1 */
				mlowerfs_bucket2s_start(bucket, start_index + 1) = extent->start;
			} else {
				/* Merge to $start_index */
				mlowerfs_bucket2s_end(bucket, start_index) = extent->end;
			}
			goto out;
		}
	}

	/* Merge old intervals and then insert new interval */
	MASSERT(head_index >= 0);
	MASSERT(head_index < MLOWERFS_BUCKET_NUMBER - 1);
	mlowerfs_bucket2s_end(bucket, head_index) = mlowerfs_bucket2s_end(bucket, head_index + 1);
	mlowerfs_bucket2s_used(bucket, head_index + 1) = 0;
	selected_index = _mlowerfs_bucket_shift_idle(bucket, start_index);
	MASSERT(selected_index != -1);

out_selected:
	MASSERT(start_inited);
	MASSERT(end_inited);
	MASSERT(!mlowerfs_bucket2s_used(bucket, selected_index));

	mlowerfs_bucket2s_start(bucket, selected_index) = start;
	mlowerfs_bucket2s_end(bucket, selected_index) = end;
	mlowerfs_bucket2s_used(bucket, selected_index) = 1;
out:
	MRETURN(ret);
}

#if defined (__linux__) && defined(__KERNEL__)

#include <linux/module.h>
#include "lowerfs_internal.h"

static spinlock_t mtfs_lowerfs_lock = SPIN_LOCK_UNLOCKED;
static LIST_HEAD(mtfs_lowerfs_types);

static struct mtfs_lowerfs *_mlowerfs_search(const char *type)
{
	struct mtfs_lowerfs *found = NULL;
	struct list_head *p = NULL;
	MENTRY();

	list_for_each(p, &mtfs_lowerfs_types) {
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
		list_add(&fs_ops->ml_linkage, &mtfs_lowerfs_types);
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

	list_for_each(p, &mtfs_lowerfs_types) {
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

ssize_t mlowerfs_getxattr(struct inode *inode,
                      const char *xattr_name,
                      void *value,
                      size_t size)
{
	struct dentry de = { .d_inode = inode };
	ssize_t ret = 0;
	int need_unlock = 0;
	MENTRY();

	MASSERT(inode);
	MASSERT(inode->i_op);
	MASSERT(inode->i_op->getxattr);

	if (!inode_is_locked(inode)) {
		mutex_lock(&inode->i_mutex);
		need_unlock = 1;
	}

	ret = inode->i_op->getxattr(&de, xattr_name,
                                    value, size);

	if (need_unlock) {
		mutex_unlock(&inode->i_mutex);
	}
	MRETURN(ret);
}

int mlowerfs_setxattr(struct inode *inode,
                      const char *xattr_name,
                      void *value,
                      size_t size)
{
	int ret = 0;
	struct dentry de = { .d_inode = inode };
	int need_unlock = 0;
	/*
	 * Should success all the time,
	 * so NO XATTR_CREATE or XATTR_REPLACE
	 */
	int flag = 0;
	MENTRY();

	MASSERT(inode);
	MASSERT(inode->i_op);
	MASSERT(inode->i_op->setxattr);

	if (!inode_is_locked(inode)) {
		mutex_lock(&inode->i_mutex);
		need_unlock = 1;
	}

	ret = inode->i_op->setxattr(&de, xattr_name,
                                    value, size, flag);

	if (need_unlock) {
		mutex_unlock(&inode->i_mutex);
	}
	MRETURN(ret);
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

	ret = mlowerfs_getflag_xattr(inode, mtfs_flag, MLOWERFS_XATTR_NAME_FLAG);

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

	ret = mlowerfs_setflag_xattr(inode, mtfs_flag, MLOWERFS_XATTR_NAME_FLAG);

	MRETURN(ret);
}
EXPORT_SYMBOL(mlowerfs_setflag_default);

static inline void mlowerfs_bucket_lock_init(struct mlowerfs_bucket *bucket)
{
	init_MUTEX(mlowerfs_bucket2lock(bucket));
}

static inline void mlowerfs_bucket_lock(struct mlowerfs_bucket *bucket)
{
	down(mlowerfs_bucket2lock(bucket));
}

static inline void mlowerfs_bucket_unlock(struct mlowerfs_bucket *bucket)
{
	up(mlowerfs_bucket2lock(bucket));
}

int mlowerfs_bucket_init(struct mlowerfs_bucket *bucket)
{
	int ret = 0;
	MENTRY();

	MASSERT(mlowerfs_bucket2inode(bucket));
	memset(mlowerfs_bucket2disk(bucket), 0,
	       sizeof(struct mlowerfs_disk_bucket));
	mlowerfs_bucket_lock_init(bucket);
	mlowerfs_bucket2type(bucket) = MLOWERFS_BUCKET_TYPE_DEFAULT;

	if (mlowerfs_bucket2type(bucket)->mbto_init) {
		ret = mlowerfs_bucket2type(bucket)->mbto_init(bucket);
	}

	MRETURN(ret);
}
EXPORT_SYMBOL(mlowerfs_bucket_init);

int mlowerfs_bucket_read_nonlock(struct mlowerfs_bucket *bucket)
{
	int ret = 0;
	MENTRY();

	MASSERT(bucket->mb_inited == 0);
	MASSERT(mlowerfs_bucket2type(bucket)->mbto_read);
	ret = mlowerfs_bucket2type(bucket)->mbto_read(bucket);
	if (ret) {
		MERROR("failed to read bucket, ret = %d\n", ret);
	} else {
		bucket->mb_inited = 1;
	}	

	MRETURN(ret);
}

int mlowerfs_bucket_flush_nonlock(struct mlowerfs_bucket *bucket)
{
	int ret = 0;
	MENTRY();

	MASSERT(mlowerfs_bucket2type(bucket)->mbto_flush);
	ret = mlowerfs_bucket2type(bucket)->mbto_flush(bucket);
	if (ret) {
		MERROR("failed to flush bucket, ret = %d\n", ret);
	} else {
		bucket->mb_dirty = 0;
	}

	MRETURN(ret);
}

int mlowerfs_bucket_add(struct mlowerfs_bucket *bucket,
                        struct mtfs_interval_node_extent *extent)
{
	int ret = 0;
	MENTRY();

	MASSERT(extent->start <= extent->end);

	mlowerfs_bucket_lock(bucket);

	if (!bucket->mb_inited) {
		MASSERT(!bucket->mb_dirty);
		ret = mlowerfs_bucket_read_nonlock(bucket);
		if (ret) {
			MERROR("failed to read bucket, ret = %d\n", ret);
			goto out_unlock;
		}
	}

	ret = _mlowerfs_bucket_add(bucket, extent);
	if (ret) {
		MERROR("failed to add bucket, ret = %d\n", ret);
		goto out_unlock;
	}

	bucket->mb_dirty = 1;

	ret = mlowerfs_bucket_flush_nonlock(bucket);
	if (ret) {
		MERROR("failed to flush bucket, ret = %d\n", ret);
		goto out_unlock;
	}

out_unlock:
	mlowerfs_bucket_unlock(bucket);

	MRETURN(ret);
}
EXPORT_SYMBOL(mlowerfs_bucket_add);

int mlowerfs_bucket_flush_xattr(struct mlowerfs_bucket *bucket)
{
	int ret = 0;
	struct inode *inode = mlowerfs_bucket2inode(bucket);
	MENTRY();

	MASSERT(inode);
	memset(mlowerfs_bucket2disk(bucket), 0,
	       sizeof(struct mlowerfs_disk_bucket));
	mlowerfs_bucket2generation(bucket) = 7777777;
	mlowerfs_bucket2reference(bucket) = 7777777;
	ret = mlowerfs_setxattr(inode, MLOWERFS_XATTR_NAME_BUCKET,
	                        mlowerfs_bucket2disk(bucket),
	                        sizeof(struct mlowerfs_disk_bucket));
	if (ret) {
		MERROR("failed to flush bucket, ret = %d\n", ret);
	}

	MRETURN(ret);
}

int mlowerfs_bucket_read_xattr(struct mlowerfs_bucket *bucket)
{
	int ret = 0;
	struct inode *inode = mlowerfs_bucket2inode(bucket);
	MENTRY();

	MASSERT(inode);
	memset(mlowerfs_bucket2disk(bucket), 0,
	       sizeof(struct mlowerfs_disk_bucket));
	ret = mlowerfs_getxattr(inode, MLOWERFS_XATTR_NAME_BUCKET,
	                        &bucket->mb_disk,
	                        sizeof(struct mlowerfs_disk_bucket));
	if (ret != sizeof(struct mlowerfs_disk_bucket)) {
		if (ret == -ENODATA) {
			ret = 0;
		} else {
			MERROR("failed to read bucket, expect %d, got %d\n",
			       sizeof(struct mlowerfs_disk_bucket), ret);
			if (ret > 0) {
				ret = -EINVAL;
			}
		}
	} else {
		ret = 0;
	}

	MRETURN(ret);
}

struct mlowerfs_bucket_type_object mlowerfs_bucket_xattr = {
	.mbto_type     = MLOWERFS_BUCKET_TYPE_XATTR,
	.mbto_init     = NULL,
	.mbto_read     = mlowerfs_bucket_read_xattr,
	.mbto_flush    = mlowerfs_bucket_flush_xattr,
};

int mlowerfs_getflag(struct mtfs_lowerfs *lowerfs,
                     struct inode *inode,
                     __u32 *mtfs_flag)
{
	int ret = 0;
	MENTRY();

	if (lowerfs->ml_getflag == NULL) {
		ret = -EOPNOTSUPP;
		MERROR("get flag not supported\n");
		goto out;
	}
	
	ret = lowerfs->ml_getflag(inode, mtfs_flag);
	if (ret) {
		MERROR("get flag failed, ret = %d\n", ret);
	}
out:
	MRETURN(ret);
}

int mlowerfs_setflag(struct mtfs_lowerfs *lowerfs,
                     struct inode *inode,
                     __u32 mtfs_flag)
{
	int ret = 0;
	MENTRY();

	if (lowerfs->ml_setflag == NULL) {
		ret = -EOPNOTSUPP;
		MERROR("set flag not supported\n");
		goto out;
	}

	MASSERT(mtfs_flag_is_valid(mtfs_flag));
	ret = lowerfs->ml_setflag(inode, mtfs_flag);
	if (ret) {
		MERROR("set flag failed, ret = %d\n", ret);
	}
out:
	MRETURN(ret);
}

/*
 * TODO: Change to a atomic operation.
 * Send a command, let sever make it atomic.
 */
int mlowerfs_invalidate(struct mtfs_lowerfs *lowerfs,
                        struct inode *inode,
                        __u32 valid_flags)
{
	int ret = 0;
	__u32 mtfs_flag = 0;
	MENTRY();

	MASSERT(inode);
	MASSERT(lowerfs);

	if (lowerfs->ml_getflag == NULL) {
		ret = -EOPNOTSUPP;
		MERROR("get flag not supported\n");
		goto out;
	}

	if (lowerfs->ml_setflag == NULL) {
		ret = -EOPNOTSUPP;
		MERROR("set flag not supported\n");
		goto out;
	}

	ret = lowerfs->ml_getflag(inode, &mtfs_flag);
	if (ret) {
		MERROR("get flag failed, ret = %d\n", ret);
		goto out;
	}

	if ((valid_flags & MTFS_ALL_VALID) == 0) {
		MERROR("nothing to set\n");
		goto out;
	}

	if (mtfs_flag & MTFS_FLAG_DATABAD) {
		MERROR("already set\n");
		goto out;
	}

	mtfs_flag |= MTFS_FLAG_DATABAD | MTFS_FLAG_SETED;
	MASSERT(mtfs_flag_is_valid(mtfs_flag));
	ret = lowerfs->ml_setflag(inode, mtfs_flag);
	if (ret) {
		MERROR("set flag failed, ret = %d\n", ret);
	}
out:
	MRETURN(ret);
}

#endif /* !defined (__linux__) && defined(__KERNEL__) */
