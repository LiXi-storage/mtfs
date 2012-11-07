/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/backing-dev.h>
#include <linux/fs_stack.h>
#include <linux/module.h>
#include <linux/version.h>
#include <debug.h>
#include <memory.h>
#include <mtfs_oplist.h>
#include <mtfs_dentry.h>
#include <mtfs_inode.h>
#include "lowerfs_internal.h"
#include "inode_internal.h"
#include "heal_internal.h"
#include "super_internal.h"
#include "mmap_internal.h"
#include "dentry_internal.h"
#include "async_internal.h"

int mtfs_inode_dump(struct inode *inode)
{
	int ret = 0;

	if (inode && !IS_ERR(inode)) {
		MDEBUG("dentry: error = %ld\n", PTR_ERR(inode));
		ret = -1;
		goto out;
	}

	MDEBUG("inode: i_ino = %lu, fs_type = %s, i_count = %d, "
	       "i_nlink = %u, i_mode = %o, i_size = %llu, "
	       "i_blocks = %llu, i_ctime = %lld, nrpages = %lu, "
	       "i_state = 0x%lx, i_flags = 0x%x, i_version = %llu, "
	       "i_generation = %x\n",
	       inode->i_ino, inode->i_sb ? s_type(inode->i_sb) : "??", atomic_read(&inode->i_count),
	       inode->i_nlink, inode->i_mode, i_size_read(inode),
	       (unsigned long long)inode->i_blocks,
	       (long long)timespec_to_ns(&inode->i_ctime) & 0x0ffff,
	       inode->i_mapping ? inode->i_mapping->nrpages : 0,
	       inode->i_state, inode->i_flags, (u64)inode->i_version,
	       inode->i_generation);
out:
	return ret;
}

int mtfs_inode_init(struct inode *inode, struct dentry *dentry)
{
	int ret = 0;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bnum = mtfs_d2bnum(dentry);
	struct dentry *hidden_dentry = NULL;
	struct mtfs_operations *operations = NULL;
	MENTRY();

	MASSERT(bnum > 0 && bnum < MTFS_BRANCH_MAX);
	mtfs_i2bnum(inode) = bnum;

	for (bindex = 0; bindex < bnum; bindex++) {
		hidden_dentry = mtfs_d2branch(dentry, bindex);
		if (hidden_dentry && hidden_dentry->d_inode) {
			mtfs_i2branch(inode, bindex) = hidden_dentry->d_inode;
		} else {
			mtfs_i2branch(inode, bindex) = NULL;
		}
	}

	// inode->i_ino = ?;
	inode->i_version++;
	operations = mtfs_i2ops(inode);
	if (operations->main_iops) {
		inode->i_op = operations->main_iops;
	} else {
		MDEBUG("inode operations not supplied, use default\n");
		inode->i_op = &mtfs_main_iops;
	}
	
	if (operations->main_fops) {
		inode->i_fop = operations->main_fops;
	} else {
		MDEBUG("file operations not supplied, use default\n");
		inode->i_fop = &mtfs_main_fops;
	}

	if (operations->aops) {
		inode->i_mapping->a_ops = operations->aops;
	} else {
		MDEBUG("aops operations not supplied, use default\n");
		inode->i_mapping->a_ops = &mtfs_aops;
	}

	mlock_resource_init(mtfs_i2resource(inode));
	masync_bucket_init((struct msubject_async_info *)mtfs_s2subinfo(inode->i_sb),
	                   mtfs_i2bucket(inode));
	MRETURN(ret);
}

int mtfs_inode_test(struct inode *inode, void *data)
{
	struct dentry *dentry = (struct dentry *)data;
	struct dentry *hidden_dentry = NULL;
	struct inode *hidden_inode = NULL;
	int ret = 0;
	MENTRY();

	goto out;
	MASSERT(dentry);
	MASSERT(mtfs_d2info(dentry));
	
	hidden_dentry = mtfs_d2branch(dentry, 0);
	MASSERT(hidden_dentry);
	
	hidden_inode = hidden_dentry->d_inode;
	MASSERT(hidden_inode);
	
	MASSERT(mtfs_i2info(inode));
	MASSERT(mtfs_i2branch(inode, 0));
	
	if (mtfs_i2bnum(inode) != mtfs_d2bnum(dentry)) {
		goto out;
	}
	
	if (mtfs_i2branch(inode, 0) != hidden_inode) {
		goto out;
	}
	
	ret = 1;
out:
	MRETURN(ret);
}

int mtfs_inode_set(struct inode *inode, void *dentry)
{
	return mtfs_inode_init(inode, (struct dentry *)dentry);
}

int mtfs_inherit_raid_type(struct dentry *dentry, raid_type_t *raid_type)
{
	int ret = 0;
	struct inode *i_parent = NULL;
	struct inode *hidden_i_parent = NULL;
	struct mtfs_lowerfs *lowerfs = NULL;
	mtfs_bindex_t bindex = -1;
	MENTRY();

	MASSERT(dentry->d_parent);
	i_parent = dentry->d_parent->d_inode;
	MASSERT(i_parent);
	ret = mtfs_i_choose_bindex(i_parent, MTFS_ATTR_VALID, &bindex);
	if (ret) {
		MERROR("choose bindex failed, ret = %d\n", ret);
		goto out;
	}

	hidden_i_parent = mtfs_i2branch(i_parent, bindex);
	lowerfs = mtfs_i2bops(i_parent, bindex);
	ret = mlowerfs_get_raid_type(lowerfs, hidden_i_parent, raid_type);

	MDEBUG("parent raid_type = %d\n", *raid_type);
out:
	MRETURN(ret);
}

int mtfs_get_raid_type(struct dentry *dentry, raid_type_t *raid_type)
{
	int ret = 0;
	MENTRY();
	
	ret = mtfs_inherit_raid_type(dentry, raid_type);
	if (ret != 0 && raid_type_is_valid(*raid_type)) {
		goto out;
	} else if (ret){
		MERROR("fail to inherit raid_type, ret = %d\n", ret);
	}

	/* Parent raid_type not seted yet, Get type from name */

	/* Use default raid_type */
	*raid_type = 0; 
out:
	MRETURN(ret);
}

static struct backing_dev_info mtfs_backing_dev_info = {
        .ra_pages       = 0,    /* No readahead */
        .capabilities   = 0,    /* Does contribute to dirty memory */
};

int mtfs_interpose(struct dentry *dentry, struct super_block *sb, int flag)
{
	struct inode *hidden_inode = NULL;
	struct dentry *hidden_dentry = NULL;
	int ret = 0;
	struct inode *inode = NULL;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bnum = 0;
	struct mtfs_operations *operations = NULL;
	MENTRY();

	bnum = mtfs_d2bnum(dentry);
	for (bindex = 0; bindex < bnum; bindex++) {
		hidden_dentry = mtfs_d2branch(dentry, bindex);
		if (hidden_dentry == NULL || hidden_dentry->d_inode == NULL) {
			MDEBUG("branch[%d] of dentry [%.*s] is %s\n",
			       bindex, dentry->d_name.len, dentry->d_name.name,
			       hidden_dentry == NULL ? "NULL" : "negative");
			continue;
		}

		igrab(hidden_dentry->d_inode);
	}

	/* TODO: change to mtfs_d2branch(dentry, bindex)->d_inode */
	inode = iget5_locked(sb, iunique(sb, MTFS_ROOT_INO),
		     mtfs_inode_test, mtfs_inode_set, (void *)dentry);
	if (inode == NULL) {
		/* should be impossible? */
		MERROR("got a NULL inode for dentry [%.*s]",
		       dentry->d_name.len, dentry->d_name.name);
		ret = -EACCES;
		goto out;
	}

	if (inode->i_state & I_NEW) {
		unlock_new_inode(inode);
	} else {
		MERROR("got a old inode unexpcectly\n");
		ret = -ENOMEM;
		MBUG();
		goto out_iput;
	}

	hidden_inode = mtfs_i_choose_branch(inode, MTFS_ATTR_VALID);
	if (IS_ERR(hidden_inode)) {
		MERROR("choose branch failed, ret = %ld\n", PTR_ERR(hidden_inode));
		ret = PTR_ERR(hidden_inode);
		goto out_iput;
	}

	/* Use different set of inode ops for symlinks & directories*/
	operations = mtfs_i2ops(inode);
	if (S_ISLNK(hidden_inode->i_mode)) {
		if (operations->symlink_iops) {
			inode->i_op = operations->symlink_iops;
		} else {
			MDEBUG("inode operations for symlink not supplied, use default\n");
			inode->i_op = &mtfs_symlink_iops;
		}
	} else if (S_ISDIR(hidden_inode->i_mode)) {
		if (operations->dir_iops) {
			inode->i_op = operations->dir_iops;
		} else {
			MDEBUG("inode operations for dir not supplied, use default\n");
			inode->i_op = &mtfs_dir_iops;
		}

		/* Use different set of file ops for directories */
		if (operations->dir_fops) {
			inode->i_fop = operations->dir_fops;
		} else {
			MDEBUG("file operations for dir not supplied, use default\n");
			inode->i_fop = &mtfs_dir_fops;
		}
	}

	/* Properly initialize special inodes */
	if (special_file(hidden_inode->i_mode)) {
		init_special_inode(inode, hidden_inode->i_mode, hidden_inode->i_rdev);
		/* Initializing backing dev info. */
		inode->i_mapping->backing_dev_info = &mtfs_backing_dev_info;
	}

	/* only (our) lookup wants to do a d_add */
	switch(flag) {
		case INTERPOSE_SUPER:
		case INTERPOSE_DEFAULT:
			d_instantiate(dentry, inode);
			break;
		case INTERPOSE_LOOKUP:
			d_add(dentry, inode);
			break;
		default:
			MERROR("Invalid interpose flag passed!");
			MBUG();
			break;
	}

	MASSERT(mtfs_d2info(dentry));

	fsstack_copy_attr_all(inode, hidden_inode, mtfs_get_nlinks);
	fsstack_copy_inode_size(inode, hidden_inode);
	goto out;
out_iput:
	iput(inode);
	for (bindex = 0; bindex < bnum; bindex++) {
		hidden_dentry = mtfs_d2branch(dentry, bindex);
		if (hidden_dentry == NULL || hidden_dentry->d_inode) {
			continue;
		}
		iput(hidden_dentry->d_inode);
	}
out:
	MRETURN(ret);
}

struct dentry *mtfs_lookup_branch(struct dentry *dentry, mtfs_bindex_t bindex)
{
	struct dentry *hidden_dentry = NULL;
	struct dentry *hidden_dir_dentry = mtfs_d2branch(dentry->d_parent, bindex);
	int ret = 0;
	MENTRY();

	ret = mtfs_device_branch_errno(mtfs_d2dev(dentry), bindex, BOPS_MASK_WRITE);
	if (ret) {
		MDEBUG("branch[%d] is abandoned\n", bindex);
		hidden_dentry = ERR_PTR(ret); 
		goto out; 
	}

	if (hidden_dir_dentry && hidden_dir_dentry->d_inode) {
		mutex_lock(&hidden_dir_dentry->d_inode->i_mutex);
		hidden_dentry = lookup_one_len(dentry->d_name.name, hidden_dir_dentry, dentry->d_name.len);
		mutex_unlock(&hidden_dir_dentry->d_inode->i_mutex);
		if (IS_ERR(hidden_dentry)) {
			MDEBUG("lookup branch[%d] of dentry [%.*s], ret = %ld\n", 
			       bindex, dentry->d_name.len, dentry->d_name.name,
			       PTR_ERR(hidden_dentry));
		} else {
			mtfs_d2branch(dentry, bindex) = hidden_dentry;
		}
	} else {
		MDEBUG("branch[%d] of dir_dentry [%.*s] is %s\n",
		       bindex, dentry->d_parent->d_name.len, dentry->d_parent->d_name.name,
		       hidden_dir_dentry == NULL ? "NULL" : "negative");
		hidden_dentry = ERR_PTR(-ENOENT);
	}

out:
	MRETURN(hidden_dentry);
}

int mtfs_lookup_backend(struct inode *dir, struct dentry *dentry, int interpose_flag)
{
	int ret = 0;
	struct dentry *hidden_dentry = NULL;
	struct inode *hidden_dir = NULL;
	mtfs_bindex_t i = 0;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bnum = 0;
	int i_valid = 0;
	struct mtfs_operations *operations = NULL;
	struct mtfs_operation_list *list = NULL;
	mtfs_operation_result_t result = {0};
	MENTRY();

	MASSERT(inode_is_locked(dir));

	list = mtfs_oplist_build(dir);
	if (unlikely(list == NULL)) {
		MERROR("failed to build operation list\n");
		ret = -ENOMEM;
		goto out;
	}

	if (list->latest_bnum == 0) {
		MERROR("directory [%.*s] has no valid branch, please check it\n",
		       dentry->d_parent->d_name.len, dentry->d_parent->d_name.name);
		if (!mtfs_dev2noabort(mtfs_i2dev(dir))) {
			ret = -EIO;
			goto out_free_oplist;
		}
	}

	if (mtfs_d_is_alloced(dentry)) {
		mtfs_d_free(dentry);
	}

	ret = mtfs_d_alloc(dentry, mtfs_i2bnum(dir));
	if (ret) {
		MERROR("failed to alloc info for dentry [%.*s]\n",
		       dentry->d_name.len, dentry->d_name.name);
		goto out_free_oplist;
	}

	operations = mtfs_d2ops(dentry);
	if (operations->dops) {
		dentry->d_op = operations->dops;
	} else {
		MDEBUG("dentry operations not supplied, use default\n");
		dentry->d_op = &mtfs_dops;
	}

	bnum = mtfs_d2bnum(dentry);
	for (i = 0; i < bnum; i++) {
		bindex = list->op_binfo[i].bindex;
		hidden_dentry = mtfs_lookup_branch(dentry, bindex);
		result.ptr = (void *)hidden_dentry;
		if ((!IS_ERR(hidden_dentry)) && hidden_dentry->d_inode) {
			mtfs_oplist_setbranch(list, i, 1, result);
		} else {
			mtfs_oplist_setbranch(list, i, 0, result);
		}
	}

	mtfs_oplist_check(list);
	if (list->success_latest_bnum == 0) {
		if (list->latest_bnum > 0 && list->success_nonlatest_bnum > 0) {
			/* TODO: remove branch */
			MERROR("nonlatest branches exist when lastest branches absent, "
			       "removing those branches\n");
			ret = heal_discard_dentry(dir, dentry, list);
			if (ret) {
				MERROR("failed to remove branches, ret = %d\n", ret);
			}
		}
		i_valid = 0;
	} else {
		if (list->fault_latest_bnum > 0) {
			/* TODO: setflag */
		}
		i_valid = 1;
	}

	if (i_valid) {
		ret = mtfs_interpose(dentry, dir->i_sb, interpose_flag);
		if (ret) {
			MDEBUG("failed to interpose dentry [%.*s], ret = %d\n",
		           dentry->d_name.len, dentry->d_name.name, ret);
			goto out_free;
		}

		hidden_dir = mtfs_i_choose_branch(dir, MTFS_ATTR_VALID);
		if (IS_ERR(hidden_dir)) {
			ret = PTR_ERR(hidden_dir);
			MERROR("choose branch failed, ret = %d\n", ret);
			goto out_free;
		}
		fsstack_copy_attr_atime(dir, hidden_dir);
	} else {
		d_add(dentry, NULL);
	}

	goto out_free_oplist;
out_free:
	/* should dput all the underlying dentries on error condition */
	for (bindex = 0; bindex < bnum; bindex++) {
		hidden_dentry = mtfs_d2branch(dentry, bindex);
		if (hidden_dentry) {
			dput(hidden_dentry);
		}
	}
//out_d_drop:
	mtfs_d_free(dentry);
	d_drop(dentry);
out_free_oplist:
	mtfs_oplist_free(list);
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_lookup_backend);

struct dentry *mtfs_lookup(struct inode *dir,
                           struct dentry *dentry, struct nameidata *nd)
{
	struct dentry *ret = NULL;
	int rc = 0;
	MENTRY();

	MDEBUG("lookup [%.*s]\n", dentry->d_name.len, dentry->d_name.name);
	MASSERT(inode_is_locked(dir));
	MASSERT(!IS_ROOT(dentry));

	/*
	 * Lustre has follow comment, we should do this too:
	 * As do_lookup is called before follow_mount, root dentry may be left
	 * not valid, revalidate it here.
	 */
	if (dir->i_sb->s_root && (dir->i_sb->s_root->d_inode == dir) && 
		nd && (nd->flags & (LOOKUP_CREATE | LOOKUP_CREATE))) {
		rc = mtfs_d_revalidate(dir->i_sb->s_root, NULL);
		if (rc == 0) {
			/* Is invalid */
			ret = ERR_PTR(-ENOMEM); /* which errno? */
			goto out;
		} else if (rc < 0) {
			ret = ERR_PTR(rc);
			goto out;
		}
	}

	if (nd && !(nd->flags & (LOOKUP_CONTINUE|LOOKUP_PARENT))) {
		/*
		 * Do we came here from failed revalidate just to propagate its error?
		 */
		if (nd->flags & LOOKUP_OPEN) {
			if (IS_ERR(nd->intent.open.file)) {
				ret = (struct dentry *)nd->intent.open.file;
				goto out;
			}
		}
	}

	rc = mtfs_lookup_backend(dir, dentry, INTERPOSE_LOOKUP);
	MDEBUG("mtfs_lookup_backend ret = %d\n", rc);

	if (nd && rc == 0 && mtfs_d_is_positive(dentry)) {
		MDEBUG("mtfs_d_is_positive\n");
		if (!(nd->flags & (LOOKUP_CONTINUE|LOOKUP_PARENT))) {
			if(nd->flags & LOOKUP_OPEN) {
				if (nd->flags & LOOKUP_CREATE) {
					MDEBUG("do not handle LOOKUP_CREATE\n");
				}
				MASSERT(dentry->d_inode);
				if (S_ISREG(dentry->d_inode->i_mode) ||
				    S_ISDIR(dentry->d_inode->i_mode)) {
					MDEBUG("open for an intented lookup\n");
					(void)lookup_instantiate_filp(nd, dentry, NULL);  
				} else {
					/* BUG236: do not open for symbol link
					 * backend fs already release hidden_file? 
					 */
				}
			}
		}
	}
	ret = ERR_PTR(rc);
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_lookup);

struct dentry *mtfs_lookup_nonnd(struct inode *dir, struct dentry *dentry, struct nameidata *nd)
{
	struct dentry *tmp_dentry = NULL;
	int ret = 0;
	MENTRY();

	MASSERT(inode_is_locked(dir));
	MASSERT(!IS_ROOT(dentry));

	ret = mtfs_lookup_backend(dir, dentry, INTERPOSE_LOOKUP);
	tmp_dentry = ERR_PTR(ret);

	MRETURN(tmp_dentry);
}
EXPORT_SYMBOL(mtfs_lookup_nonnd);

static int mtfs_create_branch(struct dentry *dentry, int mode,
                              mtfs_bindex_t bindex, struct nameidata *nd)
{
	int ret = 0;
	struct dentry *hidden_dentry = mtfs_d2branch(dentry, bindex);
	struct dentry *hidden_dir_dentry = NULL;
#if (LINUX_VERSION_CODE == KERNEL_VERSION(2,6,32))
	struct dentry *dentry_save = NULL;
	struct vfsmount *vfsmount_save = 0;
	unsigned int flags_save = 0;
#endif  /* (LINUX_VERSION_CODE == KERNEL_VERSION(2,6,32)) */
	MENTRY();

	ret = mtfs_device_branch_errno(mtfs_d2dev(dentry), bindex, BOPS_MASK_WRITE);
	if (ret) {
		MDEBUG("branch[%d] is abandoned\n", bindex);
		goto out; 
	}

	if (hidden_dentry && hidden_dentry->d_parent) {
		hidden_dir_dentry = lock_parent(hidden_dentry);
#if (LINUX_VERSION_CODE == KERNEL_VERSION(2,6,32))
		/* God damn the nfs_create of this version */
		if (strcmp(hidden_dir_dentry->d_sb->s_type->name, "nfs") == 0) {
			dentry_save = nd->path.dentry;
			vfsmount_save = nd->path.mnt;
			flags_save = nd->flags;

			nd->path.dentry = hidden_dentry;
			nd->path.mnt = mtfs_s2mntbranch(dentry->d_sb, bindex);
			nd->flags &= ~LOOKUP_OPEN;
			ret = vfs_create(hidden_dir_dentry->d_inode,
			                 hidden_dentry, mode, nd);
			nd->path.dentry = dentry_save;
			nd->path.mnt = vfsmount_save;
			nd->flags = flags_save;
		} else {
			ret = vfs_create(hidden_dir_dentry->d_inode,
			                 hidden_dentry, mode, NULL);
		}
#else /* !(LINUX_VERSION_CODE == KERNEL_VERSION(2,6,32)) */
		ret = vfs_create(hidden_dir_dentry->d_inode,
		                 hidden_dentry, mode, NULL);
#endif /* !(LINUX_VERSION_CODE == KERNEL_VERSION(2,6,32)) */
		unlock_dir(hidden_dir_dentry);
		if (!ret && hidden_dentry->d_inode == NULL) {
			MBUG();
			MDEBUG("branch[%d] of dentry [%.*s] is negative\n", bindex,
			       hidden_dentry->d_name.len, hidden_dentry->d_name.name);
			ret = -EIO; /* which errno? */
		}
	} else {
		if (hidden_dentry == NULL) {
			MDEBUG("branch[%d] of dentry [%.*s/%.*s] is NULL\n", bindex,
			       dentry->d_parent->d_name.len,
			       dentry->d_parent->d_name.name,
			       dentry->d_name.len, dentry->d_name.name);
		} else {
			MDEBUG("parent's branch[%d] of dentry [%.*s/%.*s] is NULL\n", bindex,
			       dentry->d_parent->d_name.len,
			       dentry->d_parent->d_name.name,
			       dentry->d_name.len, dentry->d_name.name);
		}
		ret = -ENOENT;
	}

out:
	MRETURN(ret);
}

int mtfs_create(struct inode *dir, struct dentry *dentry, int mode, struct nameidata *nd)
{
	int ret = 0;
	struct inode *hidden_dir = NULL;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t i = 0;
	struct mtfs_operation_list *list = NULL;
	mtfs_operation_result_t result = {0};
	MENTRY();

	MDEBUG("create [%.*s]\n", dentry->d_name.len, dentry->d_name.name);
	MASSERT(inode_is_locked(dir));
	MASSERT(dentry->d_inode == NULL);

	list = mtfs_oplist_build(dir);
	if (unlikely(list == NULL)) {
		MERROR("failed to build operation list\n");
		ret = -ENOMEM;
		goto out;
	}

	if (list->latest_bnum == 0) {
		MERROR("dir [%.*s] has no valid branch, please check it\n",
		       dentry->d_parent->d_name.len, dentry->d_parent->d_name.name);
		if (!mtfs_dev2noabort(mtfs_i2dev(dir))) {
			ret = -EIO;
			goto out_free_oplist;
		}
	}

	for (i = 0; i < mtfs_i2bnum(dir); i++) {
		bindex = list->op_binfo[i].bindex;
		ret = mtfs_create_branch(dentry, mode, bindex, nd);
		result.ret = ret;
		mtfs_oplist_setbranch(list, i, (ret == 0 ? 1 : 0), result);
		if (i == list->latest_bnum - 1) {
			mtfs_oplist_check(list);
			if (list->success_latest_bnum <= 0) {
				MDEBUG("operation failed for all latest branches\n");
				if (!mtfs_dev2noabort(mtfs_i2dev(dir))) {
					ret = -EIO;
					goto out_free_oplist;
				}
			}
		}
	}

	mtfs_oplist_check(list);
	if (list->success_bnum <= 0) {
		result = mtfs_oplist_result(list);
		ret = result.ret;
		goto out_free_oplist;
	}

	ret = mtfs_oplist_update(dir, list);
	if (ret) {
		MERROR("failed to update inode\n");
		MBUG();
	}

	ret = mtfs_interpose(dentry, dir->i_sb, INTERPOSE_DEFAULT);
	if (ret) {
		MERROR("create failed when interposing, ret = %d\n", ret);
		goto out_free_oplist;
	}

	 /* Handle open intent */
	if (nd && (nd->flags & LOOKUP_OPEN) && dentry->d_inode) {
		struct file *filp;
		filp = lookup_instantiate_filp(nd, dentry, NULL);
		if (IS_ERR(filp)) {
			ret = PTR_ERR(filp);
			MERROR("open failed, ret = %d\n", ret);
			goto out_free_oplist;
		}
	}

	/* Update parent dir */
	hidden_dir = mtfs_i_choose_branch(dir, MTFS_ATTR_VALID);
	if (IS_ERR(hidden_dir)) {
		ret = PTR_ERR(hidden_dir);
		MERROR("choose branch failed, ret = %d\n", ret);
		goto out_free_oplist;
	}

	fsstack_copy_attr_times(dir, hidden_dir);
	fsstack_copy_inode_size(dir, hidden_dir);

	goto out_free_oplist;
out_free_oplist:
	mtfs_oplist_free(list);
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_create);

int mtfs_link_branch(struct dentry *old_dentry, struct dentry *new_dentry, mtfs_bindex_t bindex)
{
	struct dentry *hidden_new_dentry = mtfs_d2branch(new_dentry, bindex);
	struct dentry *hidden_old_dentry = mtfs_d2branch(old_dentry, bindex);
	struct dentry *hidden_new_dir_dentry = NULL;
	int ret = 0;
	MENTRY();

	ret = mtfs_device_branch_errno(mtfs_d2dev(old_dentry), bindex, BOPS_MASK_WRITE);
	if (ret) {
		MDEBUG("branch[%d] is abandoned\n", bindex); 
		goto out; 
	}

	if (hidden_old_dentry && hidden_new_dentry) {
		dget(hidden_old_dentry);

		hidden_new_dir_dentry = lock_parent(hidden_new_dentry);
		ret = vfs_link(hidden_old_dentry, hidden_new_dir_dentry->d_inode, hidden_new_dentry);
		unlock_dir(hidden_new_dir_dentry);

		dput(hidden_old_dentry);
	} else {
		if (hidden_old_dentry == NULL) {
			MDEBUG("branch[%d] of dentry [%.*s/%.*s] is NULL\n", bindex,
			       old_dentry->d_parent->d_name.len, old_dentry->d_parent->d_name.name,
			       old_dentry->d_name.len, old_dentry->d_name.name);
		}
	
		if (hidden_new_dentry == NULL){
			MDEBUG("branch[%d] of dentry [%.*s/%.*s] is NULL\n", bindex,
			       new_dentry->d_parent->d_name.len, new_dentry->d_parent->d_name.name,
			       new_dentry->d_name.len, new_dentry->d_name.name);
		}
		ret = -ENOENT;
	}

out:
	MRETURN(ret);
}

int mtfs_link(struct dentry *old_dentry, struct inode *dir, struct dentry *new_dentry)
{
	int ret = 0;
	struct inode *hidden_dir = NULL;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t i = 0;
	struct mtfs_operation_list *list = NULL;
	mtfs_operation_result_t result = {0};
	MENTRY();

	MDEBUG("link [%.*s] to [%.*s]\n",
	       old_dentry->d_name.len, old_dentry->d_name.name,
	       new_dentry->d_name.len, new_dentry->d_name.name);
	MASSERT(old_dentry->d_inode);
	MASSERT(inode_is_locked(old_dentry->d_inode));
	MASSERT(!S_ISDIR(old_dentry->d_inode->i_mode));
	MASSERT(inode_is_locked(dir));

	list = mtfs_oplist_build(dir);
	if (unlikely(list == NULL)) {
		MERROR("failed to build operation list\n");
		ret = -ENOMEM;
		goto out;
	}

	if (list->latest_bnum == 0) {
		MERROR("directory [%.*s] has no valid branch, please check it\n",
		       new_dentry->d_parent->d_name.len, new_dentry->d_parent->d_name.name);
		if (!mtfs_dev2noabort(mtfs_i2dev(dir))) {
			ret = -EIO;
			goto out_free_oplist;
		}
	}

	for (i = 0; i < mtfs_i2bnum(dir); i++) {
		bindex = list->op_binfo[i].bindex;
		ret = mtfs_link_branch(old_dentry, new_dentry, bindex);
		result.ret = ret;
		mtfs_oplist_setbranch(list, i, (ret == 0 ? 1 : 0), result);
		if (i == list->latest_bnum - 1) {
			mtfs_oplist_check(list);
			if (list->success_latest_bnum <= 0) {
				MDEBUG("operation failed for all latest %d branches\n", list->latest_bnum);
				if (!mtfs_dev2noabort(mtfs_i2dev(dir))) {
					result = mtfs_oplist_result(list);
					ret = result.ret;
					goto out_free_oplist;
				}
			}
		}
	}

	mtfs_oplist_check(list);
	if (list->success_bnum <= 0) {
		result = mtfs_oplist_result(list);
		ret = result.ret;
		goto out_free_oplist;
	}

	ret = mtfs_oplist_update(dir, list);
	if (ret) {
		MERROR("failed to update inode\n");
		MBUG();
	}

	for (i = 0; i < mtfs_i2bnum(dir); i++) {
		bindex = list->op_binfo[i].bindex;
		dput(mtfs_d2branch(new_dentry, bindex));
	}

	ret = mtfs_lookup_backend(dir, new_dentry, INTERPOSE_DEFAULT);
	if (ret) {
		MERROR("link failed when lookup, ret = %d\n", ret);
		goto out_free_oplist;
	}

	/* Update parent dir */
	hidden_dir = mtfs_i_choose_branch(dir, MTFS_ATTR_VALID);
	if (IS_ERR(hidden_dir)) {
		ret = PTR_ERR(hidden_dir);
		MERROR("choose branch failed, ret = %d\n", ret);
		goto out_free_oplist;
	}

	fsstack_copy_attr_times(dir, hidden_dir);
	fsstack_copy_inode_size(dir, hidden_dir);

	old_dentry->d_inode->i_nlink = mtfs_get_nlinks(old_dentry->d_inode);

	goto out_free_oplist;
out_free_oplist:
	mtfs_oplist_free(list);
out:
	if (!new_dentry->d_inode) {
		d_drop(new_dentry);
	}

	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_link);

static inline int __vfs_unlink(struct inode *dir, struct dentry *dentry)
{
	int ret =0;

	mutex_lock(&dentry->d_inode->i_mutex);
	ret = dir->i_op->unlink(dir, dentry);
	mutex_unlock(&dentry->d_inode->i_mutex);
	
	return ret;
}

static int mtfs_unlink_branch(struct dentry *dentry, mtfs_bindex_t bindex)
{
	struct dentry *hidden_dentry = mtfs_d2branch(dentry, bindex);
	struct dentry *hidden_dir_dentry = NULL;
	int ret = 0;
	struct mtfs_lowerfs *lowerfs = mtfs_d2bops(dentry, bindex);
	MENTRY();

	ret = mtfs_device_branch_errno(mtfs_d2dev(dentry), bindex, BOPS_MASK_WRITE);
	if (ret) {
		MDEBUG("branch[%d] is abandoned\n", bindex); 
		goto out; 
	}
	
	if (hidden_dentry && hidden_dentry->d_parent && hidden_dentry->d_inode) {
		hidden_dentry = dget(hidden_dentry);
		hidden_dir_dentry = lock_parent(hidden_dentry);
		MASSERT(hidden_dir_dentry);
		MASSERT(hidden_dir_dentry->d_inode); 
		if (lowerfs->ml_flag & MLOWERFS_FLAG_UNLINK_NO_DDLETE) {
			/* BUG263: do not use vfs_unlink() for its d_delete() */
			ret = __vfs_unlink(hidden_dir_dentry->d_inode, hidden_dentry);
		} else {
			/* For local disk fs, we need vfs_unlink call d_delete */
			ret = vfs_unlink(hidden_dir_dentry->d_inode, hidden_dentry);
		}

		unlock_dir(hidden_dir_dentry);
		dput(hidden_dentry);
	} else {
		if (hidden_dentry == NULL) {
			MDEBUG("branch[%d] of dentry [%.*s/%.*s] is NULL\n", bindex,
			       dentry->d_parent->d_name.len, dentry->d_parent->d_name.name,
			       dentry->d_name.len, dentry->d_name.name);
		} else if (hidden_dentry->d_inode == NULL){
			MDEBUG("branch[%d] of dentry [%.*s/%.*s] is negative\n", bindex,
			       dentry->d_parent->d_name.len, dentry->d_parent->d_name.name,
			       dentry->d_name.len, dentry->d_name.name);
		} else {
			MDEBUG("parent's branch[%d] of dentry [%.*s/%.*s] is NULL\n", bindex,
			       dentry->d_parent->d_name.len, dentry->d_parent->d_name.name,
			       dentry->d_name.len, dentry->d_name.name);
		}
		ret = -ENOENT;
	}

out:
	MRETURN(ret);
}

int mtfs_unlink(struct inode *dir, struct dentry *dentry)
{
	int ret = 0;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t i = 0;
	struct inode *hidden_dir = NULL;
	struct inode *inode = dentry->d_inode;
	struct mtfs_operation_list *list = NULL;
	mtfs_operation_result_t result = {0};	
	MENTRY();

	MDEBUG("unlink [%.*s]\n", dentry->d_name.len, dentry->d_name.name);
	MASSERT(inode_is_locked(dir));
	MASSERT(inode);
	MASSERT(inode_is_locked(inode));

	dget(dentry);

	list = mtfs_oplist_build(dir);
	if (unlikely(list == NULL)) {
		MERROR("failed to build operation list\n");
		ret = -ENOMEM;
		goto out;
	}

	if (list->latest_bnum == 0) {
		MERROR("dir [%.*s] has no valid branch, please check it\n",
		       dentry->d_parent->d_name.len, dentry->d_parent->d_name.name);
		if (!mtfs_dev2noabort(mtfs_i2dev(dir))) {
			ret = -EIO;
			goto out_free_oplist;
		}
	}

	for (i = 0; i < mtfs_i2bnum(dir); i++) {
		bindex = list->op_binfo[i].bindex;
		ret = mtfs_unlink_branch(dentry, bindex);
		result.ret = ret;
		mtfs_oplist_setbranch(list, i, (ret == 0 ? 1 : 0), result);
		if (i == list->latest_bnum - 1) {
			mtfs_oplist_check(list);
			if (list->success_latest_bnum <= 0) {
				MDEBUG("operation failed for all latest %d branches\n", list->latest_bnum);
				if (!mtfs_dev2noabort(mtfs_i2dev(dir))) {
					result = mtfs_oplist_result(list);
					ret = result.ret;
					goto out_free_oplist;
				}
			}
		}
	}

	mtfs_oplist_check(list);
	if (list->success_bnum <= 0) {
		result = mtfs_oplist_result(list);
		ret = result.ret;
		goto out_free_oplist;
	}

	ret = mtfs_oplist_update(dir, list);
	if (ret) {
		MERROR("failed to update inode\n");
		MBUG();
	}

	hidden_dir = mtfs_i_choose_branch(dir, MTFS_ATTR_VALID);
	if (IS_ERR(hidden_dir)) {
		ret = PTR_ERR(hidden_dir);
		MERROR("choose branch failed, ret = %d\n", ret);
		goto out_free_oplist;
	}
	fsstack_copy_attr_times(dir, hidden_dir);
	dentry->d_inode->i_nlink = mtfs_get_nlinks(dentry->d_inode);

	/* call d_drop so the system "forgets" about us */
	d_drop(dentry);
out_free_oplist:
	mtfs_oplist_free(list);
out:
	dput(dentry);
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_unlink);

static inline int __vfs_rmdir(struct inode *dir, struct dentry *dentry)
{
	int ret = 0;

	MASSERT(dentry);
	MASSERT(dentry->d_inode);
	mutex_lock(&dentry->d_inode->i_mutex);
	dentry_unhash(dentry);
	ret = dir->i_op->rmdir(dir, dentry);
	mutex_unlock(&dentry->d_inode->i_mutex);
	/* dput() for dentry_unhash */
	dput(dentry);

	return ret;
}

static int mtfs_rmdir_branch(struct dentry *dentry, mtfs_bindex_t bindex)
{
	struct dentry *hidden_dentry = mtfs_d2branch(dentry, bindex);
	struct dentry *hidden_dir_dentry = NULL;
	int ret = 0;
	struct mtfs_lowerfs *lowerfs = mtfs_d2bops(dentry, bindex);
	MENTRY();

	ret = mtfs_device_branch_errno(mtfs_d2dev(dentry), bindex, BOPS_MASK_WRITE);
	if (ret) {
		MDEBUG("branch[%d] is abandoned\n", bindex); 
		goto out; 
	}

	if (hidden_dentry && hidden_dentry->d_parent && hidden_dentry->d_inode) {
		dget(hidden_dentry);
		hidden_dir_dentry = lock_parent(hidden_dentry);
		if (lowerfs->ml_flag & MLOWERFS_FLAG_RMDIR_NO_DDLETE) {
			/* BUG264: do not use vfs_rmdir() for its d_delete() */
			ret = __vfs_rmdir(hidden_dir_dentry->d_inode, hidden_dentry);
		} else {
			ret = vfs_rmdir(hidden_dir_dentry->d_inode, hidden_dentry);
		}
		unlock_dir(hidden_dir_dentry);
		dput(hidden_dentry);
		if (!ret && hidden_dentry->d_inode == NULL) {
			/* TODO: Take tmpfs as lowerfs,
			 * pjd_fstest:bug:0 will come here. Why?
			 */
			//MBUG();
			MDEBUG("branch[%d] of dentry [%.*s] is negative\n", bindex,
			       hidden_dentry->d_name.len, hidden_dentry->d_name.name);
			ret = -EIO; /* which errno? */
		}
	} else {
		if (hidden_dentry == NULL) {
			MDEBUG("branch[%d] of dentry [%.*s/%.*s] is NULL\n", bindex,
			       dentry->d_parent->d_name.len, dentry->d_parent->d_name.name,
			       dentry->d_name.len, dentry->d_name.name);
		} else if (hidden_dentry->d_inode == NULL){
			MDEBUG("branch[%d] of dentry [%.*s/%.*s] is negative\n", bindex,
			       dentry->d_parent->d_name.len, dentry->d_parent->d_name.name,
			       dentry->d_name.len, dentry->d_name.name);
		} else {
			MDEBUG("parent's branch[%d] of dentry [%.*s/%.*s] is NULL\n", bindex,
			       dentry->d_parent->d_name.len, dentry->d_parent->d_name.name,
			       dentry->d_name.len, dentry->d_name.name);
		}
		ret = -ENOENT;
	}

out:
	MRETURN(ret);
}

int mtfs_rmdir(struct inode *dir, struct dentry *dentry)
{	
	mtfs_bindex_t bindex = 0; 
	int ret = 0;
	struct inode *hidden_dir = NULL;
	mtfs_bindex_t i = 0;
	struct mtfs_operation_list *list = NULL;
	mtfs_operation_result_t result = {0};
	MENTRY();

	MDEBUG("rmdir [%.*s]\n", dentry->d_name.len, dentry->d_name.name);
	MASSERT(inode_is_locked(dir));
	MASSERT(dentry->d_inode);
	MASSERT(inode_is_locked(dentry->d_inode));

	dget(dentry);

	list = mtfs_oplist_build(dir);
	if (unlikely(list == NULL)) {
		MERROR("failed to build operation list\n");
		ret = -ENOMEM;
		goto out;
	}

	if (list->latest_bnum == 0) {
		MERROR("dir [%.*s] has no valid branch, please check it\n",
		       dentry->d_parent->d_name.len, dentry->d_parent->d_name.name);
		if (!mtfs_dev2noabort(mtfs_i2dev(dir))) {
			ret = -EIO;
			goto out_free_oplist;
		}
	}

	for (i = 0; i < mtfs_i2bnum(dir); i++) {
		bindex = list->op_binfo[i].bindex;
		ret = mtfs_rmdir_branch(dentry, bindex);
		result.ret = ret;
		mtfs_oplist_setbranch(list, i, (ret == 0 ? 1 : 0), result);
		if (i == list->latest_bnum - 1) {
			mtfs_oplist_check(list);
			if (list->success_latest_bnum <= 0) {
				MDEBUG("operation failed for all latest %d branches\n", list->latest_bnum);
				if (!mtfs_dev2noabort(mtfs_i2dev(dir))) {
					result = mtfs_oplist_result(list);
					ret = result.ret;
					goto out_free_oplist;
				}
			}
		}
	}

	mtfs_oplist_check(list);
	if (list->success_bnum <= 0) {
		result = mtfs_oplist_result(list);
		ret = result.ret;
		goto out_free_oplist;
	}

	ret = mtfs_oplist_update(dir, list);
	if (ret) {
		MERROR("failed to update inode\n");
		MBUG();
	}

	hidden_dir = mtfs_i_choose_branch(dir, MTFS_ATTR_VALID);
	if (IS_ERR(hidden_dir)) {
		ret = PTR_ERR(hidden_dir);
		MERROR("choose branch failed, ret = %d\n", ret);
		goto out_free_oplist;
	}

	fsstack_copy_attr_times(dir, hidden_dir);
	fsstack_copy_inode_size(dir, hidden_dir);

	dentry->d_inode->i_nlink = mtfs_get_nlinks(dentry->d_inode);

	d_drop(dentry);

	goto out_free_oplist;
out_free_oplist:
	mtfs_oplist_free(list);
out:
	dput(dentry);
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_rmdir);

static int mtfs_symlink_branch(struct dentry *dentry, mtfs_bindex_t bindex, const char *symname)
{
	struct dentry *hidden_dentry = mtfs_d2branch(dentry, bindex);
	struct dentry *hidden_dir_dentry = NULL;
	int ret = 0;
#ifdef HAVE_VFS_SYMLINK_4ARGS
	umode_t	mode = S_IALLUGO;
#endif /* HAVE_VFS_SYMLINK_4ARGS */
	MENTRY();

	ret = mtfs_device_branch_errno(mtfs_d2dev(dentry), bindex, BOPS_MASK_WRITE);
	if (ret) {
		MDEBUG("branch[%d] is abandoned\n", bindex); 
		goto out; 
	}

	if (hidden_dentry && hidden_dentry->d_parent) {
		hidden_dir_dentry = lock_parent(hidden_dentry);
#ifdef HAVE_VFS_SYMLINK_4ARGS
		ret = vfs_symlink(hidden_dir_dentry->d_inode, hidden_dentry, symname, mode);
#else /* ! HAVE_VFS_SYMLINK_4ARGS */
		ret = vfs_symlink(hidden_dir_dentry->d_inode, hidden_dentry, symname);
#endif /* ! HAVE_VFS_SYMLINK_4ARGS */
		unlock_dir(hidden_dir_dentry);
		if (!ret && hidden_dentry->d_inode == NULL) {
			MBUG();
			MDEBUG("branch[%d] of dentry [%.*s] is negative\n", bindex,
			       hidden_dentry->d_name.len, hidden_dentry->d_name.name);
			ret = -EIO; /* which errno? */
		}
	} else {
		if (hidden_dentry == NULL) {
			MDEBUG("branch[%d] of dentry [%.*s/%.*s] is NULL\n", bindex,
			       dentry->d_parent->d_name.len, dentry->d_parent->d_name.name,
			       dentry->d_name.len, dentry->d_name.name);
		} else if (hidden_dentry->d_parent == NULL){
			MDEBUG("parent's branch[%d] of dentry [%.*s/%.*s] is NULL\n", bindex,
			       dentry->d_parent->d_name.len, dentry->d_parent->d_name.name,
			       dentry->d_name.len, dentry->d_name.name);
		}
		ret = -ENOENT;
	}

out:
	MRETURN(ret);
}

int mtfs_symlink(struct inode *dir, struct dentry *dentry, const char *symname)
{
	int ret = 0;
	mtfs_bindex_t bindex = 0; 
	mtfs_bindex_t i = 0;
	struct mtfs_operation_list *list = NULL;
	mtfs_operation_result_t result = {0};
	struct inode *hidden_dir = NULL;
	MENTRY();

	MDEBUG("symlink [%.*s] to [%s]\n", dentry->d_name.len, dentry->d_name.name, symname);
	MASSERT(inode_is_locked(dir));
	MASSERT(dentry->d_inode == NULL);

	list = mtfs_oplist_build(dir);
	if (unlikely(list == NULL)) {
		MERROR("failed to build operation list\n");
		ret = -ENOMEM;
		goto out;
	}

	if (list->latest_bnum == 0) {
		MERROR("dir [%.*s] has no valid branch, please check it\n",
		       dentry->d_parent->d_name.len, dentry->d_parent->d_name.name);
		if (!mtfs_dev2noabort(mtfs_i2dev(dir))) {
			ret = -EIO;
			goto out_free_oplist;
		}
	}

	for (i = 0; i < mtfs_i2bnum(dir); i++) {
		bindex = list->op_binfo[i].bindex;
		ret = mtfs_symlink_branch(dentry, bindex, symname);
		result.ret = ret;
		mtfs_oplist_setbranch(list, i, (ret == 0 ? 1 : 0), result);
		if (i == list->latest_bnum - 1) {
			mtfs_oplist_check(list);
			if (list->success_latest_bnum <= 0) {
				MDEBUG("operation failed for all latest %d branches\n", list->latest_bnum);
				if (!mtfs_dev2noabort(mtfs_i2dev(dir))) {
					result = mtfs_oplist_result(list);
					ret = result.ret;
					goto out_free_oplist;
				}
			}
		}
	}

	mtfs_oplist_check(list);
	if (list->success_bnum <= 0) {
		result = mtfs_oplist_result(list);
		ret = result.ret;
		goto out_free_oplist;
	}

	ret = mtfs_oplist_update(dir, list);
	if (ret) {
		MERROR("failed to update inode\n");
		MBUG();
	}

	ret = mtfs_interpose(dentry, dir->i_sb, INTERPOSE_DEFAULT);
	if (ret) {
		goto out_free_oplist;
	}

	hidden_dir = mtfs_i_choose_branch(dir, MTFS_ATTR_VALID);
	if (IS_ERR(hidden_dir)) {
		ret = PTR_ERR(hidden_dir);
		MERROR("choose branch failed, ret = %d\n", ret);
		goto out_free_oplist;
	}
	fsstack_copy_attr_times(dir, hidden_dir);
	fsstack_copy_inode_size(dir, hidden_dir);

	goto out_free_oplist;
out_free_oplist:
	mtfs_oplist_free(list);
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_symlink);

static int mtfs_mkdir_branch(struct dentry *dentry, int mode, mtfs_bindex_t bindex)
{
	int ret = 0;
	struct dentry *hidden_dentry = mtfs_d2branch(dentry, bindex);
	struct dentry *hidden_dir_dentry = NULL;
	MENTRY();

	ret = mtfs_device_branch_errno(mtfs_d2dev(dentry), bindex, BOPS_MASK_WRITE);
	if (ret) {
		MDEBUG("branch[%d] is abandoned\n", bindex); 
		goto out; 
	}

	if (hidden_dentry && hidden_dentry->d_parent) {
		hidden_dir_dentry = lock_parent(hidden_dentry);
		ret = vfs_mkdir(hidden_dir_dentry->d_inode, hidden_dentry, mode);
		unlock_dir(hidden_dir_dentry);
		if (ret) {
			MDEBUG("failed to mkdir branch[%d] of dentry [%.*s/%.*s], ret = %d\n", 
			       bindex, dentry->d_parent->d_name.len, dentry->d_parent->d_name.name,
			       dentry->d_name.len, dentry->d_name.name, ret);
		}
		if (!ret && hidden_dentry->d_inode == NULL) {
			MDEBUG("branch[%d] of dentry [%.*s] is negative\n", bindex,
			       hidden_dentry->d_name.len, hidden_dentry->d_name.name);
			MBUG();
		}
	} else {
		if (hidden_dentry == NULL) {
			MDEBUG("branch[%d] of dentry [%.*s/%.*s] is NULL\n", bindex,
			       dentry->d_parent->d_name.len, dentry->d_parent->d_name.name,
			       dentry->d_name.len, dentry->d_name.name);
		} else {
			MDEBUG("parent's branch[%d] of dentry [%.*s/%.*s] is NULL\n", bindex,
			       dentry->d_parent->d_name.len, dentry->d_parent->d_name.name,
			       dentry->d_name.len, dentry->d_name.name);
		}
		ret = -ENOENT;
	}

out:
	MRETURN(ret);
}

int mtfs_mkdir(struct inode *dir, struct dentry *dentry, int mode)
{
	int ret = 0;
	struct inode *hidden_dir = NULL;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t i = 0;
	struct mtfs_operation_list *list = NULL;
	mtfs_operation_result_t result = {0};
	MENTRY();

	MDEBUG("mkdir [%.*s]\n", dentry->d_name.len, dentry->d_name.name);
	MASSERT(inode_is_locked(dir));
	MASSERT(dentry->d_inode == NULL);

	list = mtfs_oplist_build(dir);
	if (unlikely(list == NULL)) {
		MERROR("failed to build operation list\n");
		ret = -ENOMEM;
		goto out;
	}

	if (list->latest_bnum == 0) {
		MERROR("dir [%.*s] has no valid branch, please check it\n",
		       dentry->d_parent->d_name.len, dentry->d_parent->d_name.name);
		if (!mtfs_dev2noabort(mtfs_i2dev(dir))) {
			ret = -EIO;
			goto out_free_oplist;
		}
	}

	MASSERT(mtfs_i2bnum(dir) == mtfs_d2bnum(dentry) );
	for (i = 0; i < mtfs_i2bnum(dir); i++) {
		bindex = list->op_binfo[i].bindex;
		ret = mtfs_mkdir_branch(dentry, mode, bindex);
		result.ret = ret;
		mtfs_oplist_setbranch(list, i, (ret == 0 ? 1 : 0), result);
		if (i == list->latest_bnum - 1) {
			mtfs_oplist_check(list);
			if (list->success_latest_bnum <= 0) {
				MDEBUG("operation failed for all latest %d branches\n", list->latest_bnum);
				if (!mtfs_dev2noabort(mtfs_i2dev(dir))) {
					result = mtfs_oplist_result(list);
					ret = result.ret;
					goto out_free_oplist;
				}
			}
		}
	}

	mtfs_oplist_check(list);
	if (list->success_bnum <= 0) {
		result = mtfs_oplist_result(list);
		ret = result.ret;
		goto out_free_oplist;
	}

	ret = mtfs_oplist_update(dir, list);
	if (ret) {
		MERROR("failed to update inode\n");
		MBUG();
	}
	ret = 0;

	ret = mtfs_interpose(dentry, dir->i_sb, INTERPOSE_DEFAULT);
	if (ret) {
		MERROR("mkdir failed when interposing, ret = %d\n", ret);
		goto out_free_oplist;
	}

	hidden_dir = mtfs_i_choose_branch(dir, MTFS_ATTR_VALID);
	if (IS_ERR(hidden_dir)) {
		ret = PTR_ERR(hidden_dir);
		MERROR("choose branch failed, ret = %d\n", ret);
		goto out_free_oplist;
	}

	fsstack_copy_attr_times(dir, hidden_dir);
	fsstack_copy_inode_size(dir, hidden_dir);

	goto out_free_oplist;
out_free_oplist:
	mtfs_oplist_free(list);
out:
	if (!dentry->d_inode) {
		d_drop(dentry);
	}
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_mkdir);

static int mtfs_mknod_branch(struct dentry *dentry, int mode, dev_t dev, mtfs_bindex_t bindex)
{
	int ret = 0;
	struct dentry *hidden_dentry = mtfs_d2branch(dentry, bindex);
	struct dentry *hidden_dir_dentry = NULL;
	MENTRY();

	ret = mtfs_device_branch_errno(mtfs_d2dev(dentry), bindex, BOPS_MASK_WRITE);
	if (ret) {
		MDEBUG("branch[%d] is abandoned\n", bindex); 
		goto out; 
	}

	if (hidden_dentry && hidden_dentry->d_parent) {
		hidden_dir_dentry = lock_parent(hidden_dentry);
		ret = vfs_mknod(hidden_dir_dentry->d_inode, hidden_dentry, mode, dev);
		unlock_dir(hidden_dir_dentry);
		if (ret) {
			MDEBUG("failed to mknod branch[%d] of dentry [%.*s/%.*s], ret = %d\n", 
			       bindex, dentry->d_parent->d_name.len, dentry->d_parent->d_name.name,
			       dentry->d_name.len, dentry->d_name.name, ret);
		}
		if (!ret && hidden_dentry->d_inode == NULL) {
			MBUG();
			MDEBUG("branch[%d] of dentry [%.*s] is negative\n", bindex,
			       hidden_dentry->d_name.len, hidden_dentry->d_name.name);
			ret = -EIO; /* which errno? */
		}
	} else {
		if (hidden_dentry == NULL) {
			MDEBUG("branch[%d] of dentry [%.*s/%.*s] is NULL\n", bindex,
			       dentry->d_parent->d_name.len, dentry->d_parent->d_name.name,
			       dentry->d_name.len, dentry->d_name.name);
		} else {
			MDEBUG("parent's branch[%d] of dentry [%.*s/%.*s] is NULL\n", bindex,
			       dentry->d_parent->d_name.len, dentry->d_parent->d_name.name,
			       dentry->d_name.len, dentry->d_name.name);
		}
		ret = -ENOENT;
	}

out:
	MRETURN(ret);
}

int mtfs_mknod(struct inode *dir, struct dentry *dentry, int mode, dev_t dev)
{
	int ret = 0;
	struct inode *hidden_dir = NULL;
	mtfs_bindex_t bindex= 0;
	mtfs_bindex_t i= 0;
	struct mtfs_operation_list *list = NULL;
	mtfs_operation_result_t result = {0};
	MENTRY();

	MDEBUG("mkdir [%.*s]\n", dentry->d_name.len, dentry->d_name.name);
	MASSERT(inode_is_locked(dir));
	MASSERT(dentry->d_inode == NULL);

	list = mtfs_oplist_build(dir);
	if (unlikely(list == NULL)) {
		MERROR("failed to build operation list\n");
		ret = -ENOMEM;
		goto out;
	}

	if (list->latest_bnum == 0) {
		MERROR("dir [%.*s] has no valid branch, please check it\n",
		       dentry->d_parent->d_name.len, dentry->d_parent->d_name.name);
		if (!mtfs_dev2noabort(mtfs_i2dev(dir))) {
			ret = -EIO;
			goto out_free_oplist;
		}
	}

	MASSERT(mtfs_i2bnum(dir) == mtfs_d2bnum(dentry) );
	for (i = 0; i < mtfs_i2bnum(dir); i++) {
		bindex = list->op_binfo[i].bindex;
		ret = mtfs_mknod_branch(dentry, mode, dev, bindex);
		result.ret = ret;
		mtfs_oplist_setbranch(list, i, (ret == 0 ? 1 : 0), result);
		if (i == list->latest_bnum - 1) {
			mtfs_oplist_check(list);
			if (list->success_latest_bnum <= 0) {
				MDEBUG("operation failed for all latest branches\n");
				if (!mtfs_dev2noabort(mtfs_i2dev(dir))) {
					result = mtfs_oplist_result(list);
					ret = result.ret;
					goto out_free_oplist;
				}
			}
		}
	}

	mtfs_oplist_check(list);
	if (list->success_bnum <= 0) {
		result = mtfs_oplist_result(list);
		ret = result.ret;
		goto out_free_oplist;
	}

	ret = mtfs_oplist_update(dir, list);
	if (ret) {
		MERROR("failed to update inode\n");
		MBUG();
	}
	ret = 0;

	ret = mtfs_interpose(dentry, dir->i_sb, INTERPOSE_DEFAULT);
	if (ret) {
		MERROR("mkdir failed when interposing, ret = %d\n", ret);
		goto out_free_oplist;
	}

	hidden_dir = mtfs_i_choose_branch(dir, MTFS_ATTR_VALID);
	if (IS_ERR(hidden_dir)) {
		ret = PTR_ERR(hidden_dir);
		MERROR("choose branch failed, ret = %d\n", ret);
		goto out_free_oplist;
	}

	fsstack_copy_attr_times(dir, hidden_dir);
	fsstack_copy_inode_size(dir, hidden_dir);

	goto out_free_oplist;
out_free_oplist:
	mtfs_oplist_free(list);
out:
	if (!dentry->d_inode) {
		d_drop(dentry);
	}
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_mknod);

static int mtfs_rename_branch(struct dentry *old_dentry, struct dentry *new_dentry, mtfs_bindex_t bindex)
{
	struct dentry *hidden_old_dentry = mtfs_d2branch(old_dentry, bindex);
	struct dentry *hidden_new_dentry = mtfs_d2branch(new_dentry, bindex);
	struct dentry *hidden_old_dir_dentry = NULL;
	struct dentry *hidden_new_dir_dentry = NULL;
	int ret = 0;
	MENTRY();

	ret = mtfs_device_branch_errno(mtfs_d2dev(old_dentry), bindex, BOPS_MASK_WRITE);
	if (ret) {
		MDEBUG("branch[%d] is abandoned\n", bindex); 
		goto out; 
	}

	if (hidden_old_dentry && hidden_new_dentry && hidden_old_dentry->d_inode) {
		dget(hidden_old_dentry);
		dget(hidden_new_dentry);

		hidden_old_dir_dentry = dget_parent(hidden_old_dentry);
		hidden_new_dir_dentry = dget_parent(hidden_new_dentry);
	
		lock_rename(hidden_new_dir_dentry, hidden_old_dir_dentry);
		ret = vfs_rename(hidden_old_dir_dentry->d_inode, hidden_old_dentry,
						hidden_new_dir_dentry->d_inode, hidden_new_dentry);
		unlock_rename(hidden_new_dir_dentry, hidden_old_dir_dentry);

		dput(hidden_new_dir_dentry);
		dput(hidden_old_dir_dentry);

		dput(hidden_new_dentry);
		dput(hidden_old_dentry);
	} else {
		if (hidden_old_dentry == NULL) {
			MASSERT(old_dentry);
			MDEBUG("branch[%d] of dentry [%.*s] is NULL\n", bindex,
			       old_dentry->d_name.len, old_dentry->d_name.name);
		} else if (hidden_old_dentry->d_inode == NULL) {
			MASSERT(old_dentry);
			MDEBUG("branch[%d] of dentry [%.*s] is negative\n", bindex,
			       old_dentry->d_name.len, old_dentry->d_name.name);
		}

		if (hidden_new_dentry == NULL) {
			MASSERT(new_dentry);
			MDEBUG("branch[%d] of dentry [%.*s] is NULL\n", bindex,
			       new_dentry->d_name.len, new_dentry->d_name.name);
		}

		ret = -ENOENT; /* which errno? */
	}

out:
	MRETURN(ret);
}

int mtfs_rename(struct inode *old_dir, struct dentry *old_dentry, struct inode *new_dir, struct dentry *new_dentry)
{
	int ret = 0;
	mtfs_bindex_t bindex = 0;
	struct inode *hidden_new_dir = NULL;
	struct inode *hidden_old_dir = NULL;
	struct inode *old_inode = old_dentry->d_inode;
	mtfs_bindex_t i = 0;
	struct mtfs_operation_list *list = NULL;
	mtfs_operation_result_t result = {0};
	MENTRY();

	MDEBUG("rename [%.*s] to [%.*s]\n",
	       old_dentry->d_name.len, old_dentry->d_name.name,
	       new_dentry->d_name.len, new_dentry->d_name.name);
	MASSERT(inode_is_locked(old_dir));
	MASSERT(inode_is_locked(new_dir));
	MASSERT(old_inode);
	if (new_dentry->d_inode) {
		MASSERT(inode_is_locked(new_dentry->d_inode));
	}

	MDEBUG("old_dentry = %p, old_name = \"%s\", new_dentry = %p, new_name = \"%s\"\n", 
	       old_dentry, old_dentry->d_name.name, new_dentry, new_dentry->d_name.name);

	/* TODO: build according to new_dir */
	list = mtfs_oplist_build(old_dir);
	if (unlikely(list == NULL)) {
		MERROR("failed to build operation list\n");
		ret = -ENOMEM;
		goto out;
	}

	if (list->latest_bnum == 0) {
		MERROR("dir [%.*s] has no valid branch, please check it\n",
		       old_dentry->d_parent->d_name.len, old_dentry->d_parent->d_name.name);
		if (!mtfs_dev2noabort(mtfs_i2dev(old_dir))) {
			ret = -EIO;
			goto out_free_oplist;
		}
	}

	for (i = 0; i < mtfs_i2bnum(old_dir); i++) {
		bindex = list->op_binfo[i].bindex;
		ret = mtfs_rename_branch(old_dentry, new_dentry, bindex);
		result.ret = ret;
		mtfs_oplist_setbranch(list, i, (ret == 0 ? 1 : 0), result);
		if (i == list->latest_bnum - 1) {
			mtfs_oplist_check(list);
			if (list->success_latest_bnum <= 0) {
				MDEBUG("rename failed for all latest %d branches\n", list->latest_bnum);
				if (!mtfs_dev2noabort(mtfs_i2dev(old_dir))) {
					result = mtfs_oplist_result(list);
					ret = result.ret;
					goto out_free_oplist;
				}
			}
		}
	}

	mtfs_oplist_check(list);
	if (list->success_bnum <= 0) {
		result = mtfs_oplist_result(list);
		ret = result.ret;
		goto out_free_oplist;
	}

	ret = mtfs_oplist_update(old_dir, list);
	if (ret) {
		MERROR("failed to update old inode\n");
		MBUG();
	}

	if (new_dir != old_dir) {
		ret = mtfs_oplist_update(new_dir, list);
		if (ret) {
			MERROR("failed to update new inode\n");
			MBUG();
		}
	}
	ret = 0;

	/* Update parent dir */
	hidden_new_dir = mtfs_i_choose_branch(new_dir, MTFS_ATTR_VALID);
	if (IS_ERR(hidden_new_dir)) {
		ret = PTR_ERR(hidden_new_dir);
		MERROR("choose branch failed, ret = %d\n", ret);
		//goto out_free_oplist;
	}
	fsstack_copy_attr_all(new_dir, hidden_new_dir, mtfs_get_nlinks);

	if (new_dir != old_dir) {
		hidden_old_dir = mtfs_i_choose_branch(old_dir, MTFS_ATTR_VALID);
		if (IS_ERR(hidden_old_dir)) {
			ret = PTR_ERR(hidden_old_dir);
			MERROR("choose branch failed, ret = %d\n", ret);
			//goto out_free_oplist;
		} else {
			fsstack_copy_attr_all(new_dir, hidden_old_dir, mtfs_get_nlinks);
		}
	}

	/* This flag is seted: FS_RENAME_DOES_D_MOVE */
	d_move(old_dentry, new_dentry);
	goto out_free_oplist;
out_free_oplist:
	mtfs_oplist_free(list);
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_rename);

int mtfs_readlink(struct dentry *dentry, char __user *buf, int bufsiz)
{
	int ret = 0;
	struct dentry *hidden_dentry = NULL;
	MENTRY();

	MDEBUG("readlink [%.*s]\n", dentry->d_name.len, dentry->d_name.name);
	hidden_dentry = mtfs_d_choose_branch(dentry, MTFS_ATTR_VALID);
	if (IS_ERR(hidden_dentry)) {
		ret = PTR_ERR(hidden_dentry);
		goto out;
	}

	MASSERT(hidden_dentry->d_inode);
	MASSERT(hidden_dentry->d_inode->i_op);
	MASSERT(hidden_dentry->d_inode->i_op->readlink);

	ret = hidden_dentry->d_inode->i_op->readlink(hidden_dentry, buf, bufsiz);

	if (ret >= 0) {
		fsstack_copy_attr_atime(dentry->d_inode, hidden_dentry->d_inode);
	}

out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_readlink);

#define MTFS_MAX_LINKNAME PAGE_SIZE
void *mtfs_follow_link(struct dentry *dentry, struct nameidata *nd)
{
	char *buf = NULL;
	int ret = 0;
	int len = MTFS_MAX_LINKNAME;
	mm_segment_t old_fs;
	MENTRY();

	MDEBUG("follow_link [%.*s]\n", dentry->d_name.len, dentry->d_name.name);
	MTFS_ALLOC(buf, len);
	if (unlikely(buf == NULL)) {
		ret = -ENOMEM;
		goto out;
	}

	old_fs = get_fs();
	set_fs(get_ds());
	ret = dentry->d_inode->i_op->readlink(dentry, (char __user *)buf, len);
	set_fs(old_fs);
	if (ret < 0) {
		goto out_free;
	} else {
		buf[ret] = '\0';
	}
	ret = 0;
	nd_set_link(nd, buf);
	goto out;
out_free:
	MTFS_FREE(buf, len);
out:
	MRETURN(ERR_PTR(ret));
}
EXPORT_SYMBOL(mtfs_follow_link);

void mtfs_put_link(struct dentry *dentry, struct nameidata *nd, void *ptr)
{
	int len = MTFS_MAX_LINKNAME;
	char *buf = nd_get_link(nd);
	MENTRY();

	MDEBUG("put_link [%.*s]\n", dentry->d_name.len, dentry->d_name.name);
	MASSERT(buf);
	/* Free the char* */
	MTFS_FREE(buf, len);
	_MRETURN();
}
EXPORT_SYMBOL(mtfs_put_link);

#ifdef HAVE_INODE_PERMISION_2ARGS
int mtfs_permission(struct inode *inode, int mask)
#else /* !HAVE_INODE_PERMISION_2ARGS */
int mtfs_permission(struct inode *inode, int mask, struct nameidata *nd)
#endif /* !HAVE_INODE_PERMISION_2ARGS */
{
	int ret = 0;
	struct inode *hidden_inode = NULL;
	MENTRY();

	MDEBUG("permission [%lu]\n", inode->i_ino);
	hidden_inode = mtfs_i_choose_branch(inode, MTFS_BRANCH_VALID);
	if (IS_ERR(hidden_inode)) {
		ret = PTR_ERR(hidden_inode);
		MERROR("choose branch failed, ret = %d\n", ret);
		goto out;
	}

#ifdef HAVE_INODE_PERMISION
	ret = inode_permission(hidden_inode, mask);
#else /* !HAVE_INODE_PERMISION */
	ret = permission(hidden_inode, mask, nd);
#endif /* !HAVE_INODE_PERMISION */

out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_permission);

static int mtfs_setattr_branch(struct dentry *dentry, struct iattr *ia, mtfs_bindex_t bindex)
{
	int ret = 0;
	struct iattr temp_ia = (*ia);
	struct dentry *hidden_dentry = mtfs_d2branch(dentry, bindex);
	MENTRY();

	ret = mtfs_device_branch_errno(mtfs_d2dev(dentry), bindex, BOPS_MASK_WRITE);
	if (ret) {
		MDEBUG("branch[%d] is abandoned\n", bindex); 
		goto out; 
	}

	if (hidden_dentry && hidden_dentry->d_inode) {
		MASSERT(hidden_dentry->d_inode->i_op);
		mutex_lock(&hidden_dentry->d_inode->i_mutex);
		ret = notify_change(hidden_dentry, &temp_ia);
		mutex_unlock(&hidden_dentry->d_inode->i_mutex);
	} else {
		MDEBUG("branch[%d] of dentry [%.*s] is %s\n",
		       bindex, dentry->d_name.len, dentry->d_name.name,
		       hidden_dentry == NULL ? "NULL" : "negative");
		ret = -ENOENT;
	}

out:
	MRETURN(ret);
}

int mtfs_setattr(struct dentry *dentry, struct iattr *ia)
{
	int ret = 0;
	struct inode *inode = dentry->d_inode;
	struct inode *hidden_inode = NULL;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t i = 0;
	struct mtfs_operation_list *list = NULL;
	mtfs_operation_result_t result = {0};
	MENTRY();

	MDEBUG("setattr [%.*s]\n", dentry->d_name.len, dentry->d_name.name);
	MASSERT(inode);
	MASSERT(inode_is_locked(inode));	

	list = mtfs_oplist_build(inode);
	if (unlikely(list == NULL)) {
		MERROR("failed to build operation list\n");
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < mtfs_d2bnum(dentry); i++) {
		bindex = list->op_binfo[i].bindex;
		ret = mtfs_setattr_branch(dentry, ia, bindex);
		result.ret = ret;
		mtfs_oplist_setbranch(list, i, (ret == 0 ? 1 : 0), result);
		if (i == list->latest_bnum - 1) {
			mtfs_oplist_check(list);
			if (list->success_latest_bnum <= 0) {
				MDEBUG("operation failed for all branches\n");
				//goto out_free_oplist;
				break;
			} else {
				if (list->fault_latest_bnum > 0) {
					/* TODO: setflag */
				}
			}
		}
	}
	result = mtfs_oplist_result(list);
	ret = result.ret;

	hidden_inode = mtfs_i_choose_branch(inode, MTFS_BRANCH_VALID);
	if (IS_ERR(hidden_inode)) {
		ret = PTR_ERR(hidden_inode);
		MERROR("choose branch failed, ret = %d\n", ret);
		goto out_free_oplist;
	}
	fsstack_copy_attr_all(inode, hidden_inode, mtfs_get_nlinks);

	if (ia->ia_valid & ATTR_SIZE) {
		mtfs_update_inode_size(inode);
	}

out_free_oplist:
	mtfs_oplist_free(list);
out:
	MRETURN(ret);	
}
EXPORT_SYMBOL(mtfs_setattr);

int mtfs_getattr(struct vfsmount *mnt, struct dentry *dentry, struct kstat *stat)
{
	int ret = 0;
	struct inode *inode = dentry->d_inode;
	mtfs_bindex_t bindex = -1;
	struct dentry *hidden_dentry = NULL;
	struct vfsmount *hidden_mnt = NULL;
	MENTRY();

	MDEBUG("getattr [%.*s]\n", dentry->d_name.len, dentry->d_name.name);

	ret = mtfs_i_choose_bindex(inode, MTFS_BRANCH_VALID, &bindex);
	if (ret) {
		MERROR("choose bindex failed, ret = %d\n", ret);
		goto out;
	}

	MASSERT(bindex >=0 && bindex < mtfs_i2bnum(inode));
	hidden_dentry = mtfs_d2branch(dentry, bindex);
	MASSERT(hidden_dentry);
	MASSERT(hidden_dentry->d_inode);

	hidden_dentry = dget(hidden_dentry);

	hidden_mnt = mtfs_s2mntbranch(dentry->d_sb, bindex);
	MASSERT(hidden_mnt);
	ret = vfs_getattr(hidden_mnt, hidden_dentry, stat);

	dput(hidden_dentry);
	fsstack_copy_attr_all(inode, hidden_dentry->d_inode, mtfs_get_nlinks);

out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_getattr);

ssize_t mtfs_getxattr(struct dentry *dentry, const char *name, void *value, size_t size) 
{
	struct dentry *hidden_dentry = NULL;
	ssize_t ret = -EOPNOTSUPP; 
	MENTRY();

	MDEBUG("getxattr [%.*s]\n", dentry->d_name.len, dentry->d_name.name);
	MASSERT(dentry->d_inode);
	hidden_dentry = mtfs_d_choose_branch(dentry, MTFS_ATTR_VALID);
	if (IS_ERR(hidden_dentry)) {
		ret = PTR_ERR(hidden_dentry);
		goto out;
	}

	MASSERT(hidden_dentry->d_inode);
	MASSERT(hidden_dentry->d_inode->i_op);
	MASSERT(hidden_dentry->d_inode->i_op->getxattr);
	ret = hidden_dentry->d_inode->i_op->getxattr(hidden_dentry, name, value, size);

out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_getxattr);

int mtfs_setxattr_branch(struct dentry *dentry, const char *name, const void *value, size_t size, int flags, mtfs_bindex_t bindex)
{
	int ret = 0;
	struct dentry *hidden_dentry = mtfs_d2branch(dentry, bindex);
	MENTRY();

	ret = mtfs_device_branch_errno(mtfs_d2dev(dentry), bindex, BOPS_MASK_WRITE);
	if (ret) {
		MDEBUG("branch[%d] is abandoned\n", bindex);
		hidden_dentry = ERR_PTR(ret); 
		goto out; 
	}

	if (hidden_dentry && hidden_dentry->d_inode) {
		MASSERT(hidden_dentry->d_inode->i_op);
		mutex_lock(&hidden_dentry->d_inode->i_mutex);
		ret = hidden_dentry->d_inode->i_op->setxattr(hidden_dentry, name, value, size, flags);
		mutex_unlock(&hidden_dentry->d_inode->i_mutex);
	} else {
		MDEBUG("branch[%d] of dentry [%.*s] is %s\n",
		       bindex, dentry->d_name.len, dentry->d_name.name,
		       hidden_dentry == NULL ? "NULL" : "negative");
		ret = -ENOENT;
	}

out:
	MRETURN(ret);
}

int mtfs_setxattr(struct dentry *dentry, const char *name, const void *value, size_t size, int flags)
{
	int ret = 0;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t i = 0;
	struct mtfs_operation_list *list = NULL;
	mtfs_operation_result_t result = {0};
	MENTRY();

	MDEBUG("setxattr [%.*s]\n", dentry->d_name.len, dentry->d_name.name);
	MASSERT(dentry->d_inode);

	list = mtfs_oplist_build(dentry->d_inode);
	if (unlikely(list == NULL)) {
		MERROR("failed to build operation list\n");
		ret = -ENOMEM;
		goto out;
	}

	if (list->latest_bnum == 0) {
		MERROR("dentry [%.*s] has no valid branch, please check it\n",
		       dentry->d_name.len, dentry->d_name.name);
		if (!mtfs_dev2noabort(mtfs_d2dev(dentry))) {
			ret = -EIO;
			goto out_free_oplist;
		}
	}

	for (i = 0; i < mtfs_d2bnum(dentry); i++) {
		bindex = list->op_binfo[i].bindex;
		ret = mtfs_setxattr_branch(dentry, name, value, size, flags, bindex);
		result.ret = ret;
		mtfs_oplist_setbranch(list, i, (ret == 0 ? 1 : 0), result);
		if (i == list->latest_bnum - 1) {
			mtfs_oplist_check(list);
			if (list->success_latest_bnum <= 0) {
				MDEBUG("operation failed for all latest %d branches\n", list->latest_bnum);
				if (!mtfs_dev2noabort(mtfs_d2dev(dentry))) {
					result = mtfs_oplist_result(list);
					ret = result.ret;
					goto out_free_oplist;
				}
			}
		}
	}

	mtfs_oplist_check(list);
	if (list->success_bnum <= 0) {
		result = mtfs_oplist_result(list);
		ret = result.ret;
		goto out_free_oplist;
	}

	ret = mtfs_oplist_update(dentry->d_inode, list);
	if (ret) {
		MERROR("failed to update inode\n");
		MBUG();
	}

	goto out_free_oplist;
out_free_oplist:
	mtfs_oplist_free(list);
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_setxattr);

int mtfs_removexattr_branch(struct dentry *dentry, const char *name, mtfs_bindex_t bindex)
{
	int ret = 0;
	struct dentry *hidden_dentry = mtfs_d2branch(dentry, bindex);
	MENTRY();

	ret = mtfs_device_branch_errno(mtfs_d2dev(dentry), bindex, BOPS_MASK_WRITE);
	if (ret) {
		MDEBUG("branch[%d] is abandoned\n", bindex);
		goto out; 
	}

	if (hidden_dentry && hidden_dentry->d_inode) {
		MASSERT(hidden_dentry->d_inode->i_op);
		MASSERT(hidden_dentry->d_inode->i_op->removexattr);
		mutex_lock(&hidden_dentry->d_inode->i_mutex);
		ret = hidden_dentry->d_inode->i_op->removexattr(hidden_dentry, name);
		mutex_unlock(&hidden_dentry->d_inode->i_mutex);
	} else {
		MDEBUG("branch[%d] of dentry [%.*s] is %s\n",
		       bindex, dentry->d_name.len, dentry->d_name.name,
		       hidden_dentry == NULL ? "NULL" : "negative");
		ret = -ENOENT;
	}

out:
	MRETURN(ret);
}

int mtfs_removexattr(struct dentry *dentry, const char *name)
{
	int ret = 0;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t i = 0;
	struct mtfs_operation_list *list = NULL;
	mtfs_operation_result_t result = {0};
	MENTRY();

	MDEBUG("removexattr [%.*s]\n", dentry->d_name.len, dentry->d_name.name);
	MASSERT(dentry->d_inode);

	list = mtfs_oplist_build(dentry->d_inode);
	if (unlikely(list == NULL)) {
		MERROR("failed to build operation list\n");
		ret = -ENOMEM;
		goto out;
	}

	if (list->latest_bnum == 0) {
		MERROR("dir [%.*s] has no valid branch, please check it\n",
		       dentry->d_parent->d_name.len, dentry->d_parent->d_name.name);
		if (!mtfs_dev2noabort(mtfs_d2dev(dentry))) {
			ret = -EIO;
			goto out_free_oplist;
		}
	}

	for (i = 0; i < mtfs_d2bnum(dentry); i++) {
		bindex = list->op_binfo[i].bindex;
		ret = mtfs_removexattr_branch(dentry, name, bindex);
		result.ret = ret;
		mtfs_oplist_setbranch(list, i, (ret == 0 ? 1 : 0), result);
		if (i == list->latest_bnum - 1) {
			mtfs_oplist_check(list);
			if (list->success_latest_bnum <= 0) {
				MDEBUG("operation failed for all latest %d branches\n", list->latest_bnum);
				if (!mtfs_dev2noabort(mtfs_d2dev(dentry))) {
					result = mtfs_oplist_result(list);
					ret = result.ret;
					goto out_free_oplist;
				}
			}
		}
	}

	mtfs_oplist_check(list);
	if (list->success_bnum <= 0) {
		result = mtfs_oplist_result(list);
		ret = result.ret;
		goto out_free_oplist;
	}

	ret = mtfs_oplist_update(dentry->d_inode, list);
	if (ret) {
		MERROR("failed to update inode\n");
		MBUG();
	}

out_free_oplist:
	mtfs_oplist_free(list);
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_removexattr);

ssize_t mtfs_listxattr(struct dentry *dentry, char *list, size_t size)
{
	struct dentry *hidden_dentry = NULL;
	ssize_t ret = -EOPNOTSUPP; 
	MENTRY();

	MDEBUG("listxattr [%.*s]\n", dentry->d_name.len, dentry->d_name.name);
	hidden_dentry = mtfs_d_choose_branch(dentry, MTFS_ATTR_VALID);
	if (IS_ERR(hidden_dentry)) {
		ret = PTR_ERR(hidden_dentry);
		goto out;
	}

	MASSERT(hidden_dentry->d_inode);
	MASSERT(hidden_dentry->d_inode->i_op);
	if (hidden_dentry->d_inode->i_op->listxattr) {
		ret = hidden_dentry->d_inode->i_op->listxattr(hidden_dentry, list, size);
	}

out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_listxattr);

int mtfs_update_inode_size(struct inode *inode)
{
	unsigned i_size = 0;
	unsigned i_blocks = 0;
	struct inode* hidden_inode;
	int ret = 0;
	MENTRY();

	MASSERT(inode);
	if (S_ISREG(inode->i_mode)) {
		hidden_inode = mtfs_i_choose_branch(inode, MTFS_DATA_VALID);
		if(IS_ERR(hidden_inode)) {
			ret = PTR_ERR(hidden_inode);
			goto out;
		}

		i_size_write(inode, i_size_read(hidden_inode));
		inode->i_blocks = hidden_inode->i_blocks;
		MDEBUG("inode(%p) has i_size = %d, i_blocks = %d\n", inode, i_size, i_blocks);
	}
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_update_inode_size);

int mtfs_update_inode_attr(struct inode *inode)
{
	struct inode* hidden_inode;
	int ret = 0;
	MENTRY();

	MASSERT(inode);
	hidden_inode = mtfs_i_choose_branch(inode, MTFS_ATTR_VALID);
	if(IS_ERR(hidden_inode)) {
		ret = PTR_ERR(hidden_inode);
		goto out;
	}

	fsstack_copy_attr_all(inode, hidden_inode, mtfs_get_nlinks);
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_update_inode_attr);

struct inode_operations mtfs_symlink_iops =
{
	readlink:       mtfs_readlink,
	follow_link:    mtfs_follow_link,
	put_link:       mtfs_put_link,
	permission:     mtfs_permission,
	setattr:        mtfs_setattr,
	getattr:        mtfs_getattr,
};

struct inode_operations mtfs_dir_iops =
{
	create:	        mtfs_create,
	lookup:	        mtfs_lookup,
	link:           mtfs_link,
	unlink:	        mtfs_unlink,
	symlink:        mtfs_symlink,
	mkdir:	        mtfs_mkdir,
	rmdir:	        mtfs_rmdir,
	mknod:	        mtfs_mknod,
	rename:	        mtfs_rename,
	permission:     mtfs_permission,
	setattr:        mtfs_setattr,
	getattr:        mtfs_getattr,
	setxattr:       mtfs_setxattr,
	getxattr:       mtfs_getxattr,
	removexattr:    mtfs_removexattr,
	listxattr:      mtfs_listxattr,
};

struct inode_operations mtfs_main_iops =
{
	permission:     mtfs_permission,
	setattr:        mtfs_setattr,
	getattr:        mtfs_getattr,
	setxattr:       mtfs_setxattr,
	getxattr:       mtfs_getxattr,
	removexattr:    mtfs_removexattr,
	listxattr:      mtfs_listxattr,
};
