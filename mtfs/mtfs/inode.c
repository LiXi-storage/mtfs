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
#include "io_internal.h"
#include "lowerfs_internal.h"
#include "inode_internal.h"
#include "heal_internal.h"
#include "super_internal.h"
#include "mmap_internal.h"
#include "dentry_internal.h"
#include "async_internal.h"
#include "subject_internal.h"

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

static int mtfs_update_attr_times_choose(struct inode *inode)
{
	struct inode *hidden_inode = NULL;
	int ret = 0;
	MENTRY();

	MASSERT(inode);
	hidden_inode = mtfs_i_choose_branch(inode, MTFS_ATTR_VALID);
	if(IS_ERR(hidden_inode)) {
		ret = PTR_ERR(hidden_inode);
		goto out;
	}

	inode->i_atime = hidden_inode->i_atime;
	inode->i_mtime = hidden_inode->i_mtime;
	inode->i_ctime = hidden_inode->i_ctime;
out:
	MRETURN(ret);
}

static int mtfs_update_attr_atime_choose(struct inode *inode)
{
	struct inode *hidden_inode = NULL;
	int ret = 0;
	MENTRY();

	MASSERT(inode);
	hidden_inode = mtfs_i_choose_branch(inode, MTFS_ATTR_VALID);
	if(IS_ERR(hidden_inode)) {
		ret = PTR_ERR(hidden_inode);
		goto out;
	}

	inode->i_atime = hidden_inode->i_atime;
out:
	MRETURN(ret);
}

static int mtfs_update_inode_attrs_choose(struct inode *inode)
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

static int mtfs_update_inode_size_choose(struct inode *inode)
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

static int mtfs_update_attr_times_master(struct inode *inode)
{
	struct inode *hidden_inode = NULL;
	int ret = 0;
	MENTRY();

	MASSERT(inode);
	hidden_inode = mtfs_i2branch(inode, 0);
	if (hidden_inode == NULL) {
		hidden_inode = mtfs_i2branch(inode, 1);
	}
	MASSERT(hidden_inode);

	inode->i_atime = hidden_inode->i_atime;
	inode->i_mtime = hidden_inode->i_mtime;
	inode->i_ctime = hidden_inode->i_ctime;

	MRETURN(ret);
}

static int mtfs_update_attr_atime_master(struct inode *inode)
{
	struct inode *hidden_inode = NULL;
	int ret = 0;
	MENTRY();

	MASSERT(inode);
	hidden_inode = mtfs_i2branch(inode, 0);
	if (hidden_inode == NULL) {
		hidden_inode = mtfs_i2branch(inode, 1);
	}
	MASSERT(hidden_inode);

	inode->i_atime = hidden_inode->i_atime;

	MRETURN(ret);
}

static int mtfs_update_inode_attrs_master(struct inode *inode)
{
	struct inode* hidden_inode;
	int ret = 0;
	MENTRY();

	MASSERT(inode);
	hidden_inode = mtfs_i2branch(inode, 0);
	if (hidden_inode == NULL) {
		hidden_inode = mtfs_i2branch(inode, 1);
	}
	MASSERT(hidden_inode);

	fsstack_copy_attr_all(inode, hidden_inode, mtfs_get_nlinks);

	MRETURN(ret);
}

static int mtfs_update_inode_size_master(struct inode *inode)
{
	unsigned i_size = 0;
	unsigned i_blocks = 0;
	struct inode* hidden_inode;
	int ret = 0;
	MENTRY();

	MASSERT(inode);
	if (S_ISREG(inode->i_mode)) {
		hidden_inode = mtfs_i2branch(inode, 0);
		if (hidden_inode == NULL) {
			hidden_inode = mtfs_i2branch(inode, 1);
		}
		MASSERT(hidden_inode);

		i_size_write(inode, i_size_read(hidden_inode));
		inode->i_blocks = hidden_inode->i_blocks;
		MDEBUG("inode(%p) has i_size = %d, i_blocks = %d\n", inode, i_size, i_blocks);
	}

	MRETURN(ret);
}

int mtfs_update_attr_times(struct inode *inode)
{
	int ret = 0;
	struct mtfs_operations *ops = mtfs_i2ops(inode);
	struct mtfs_iupdate_operations *iupdate_ops = ops->iupdate_ops;
	MENTRY();

	if (iupdate_ops && iupdate_ops->miuo_times) {
		ret = iupdate_ops->miuo_times(inode);
	}

	MRETURN(ret);
}

int mtfs_update_attr_atime(struct inode *inode)
{
	int ret = 0;
	struct mtfs_operations *ops = mtfs_i2ops(inode);
	struct mtfs_iupdate_operations *iupdate_ops = ops->iupdate_ops;
	MENTRY();

	if (iupdate_ops && iupdate_ops->miuo_atime) {
		ret = iupdate_ops->miuo_atime(inode);
	}

	MRETURN(ret);
}

int mtfs_update_inode_attrs(struct inode *inode)
{
	int ret = 0;
	struct mtfs_operations *ops = mtfs_i2ops(inode);
	struct mtfs_iupdate_operations *iupdate_ops = ops->iupdate_ops;
	MENTRY();

	if (iupdate_ops && iupdate_ops->miuo_attrs) {
		ret = iupdate_ops->miuo_attrs(inode);
	}

	MRETURN(ret);
}

int mtfs_update_inode_size(struct inode *inode)
{
	int ret = 0;
	struct mtfs_operations *ops = mtfs_i2ops(inode);
	struct mtfs_iupdate_operations *iupdate_ops = ops->iupdate_ops;
	MENTRY();

	if (iupdate_ops && iupdate_ops->miuo_size) {
		ret = iupdate_ops->miuo_size(inode);
	}

	MRETURN(ret);
}

struct mtfs_iupdate_operations mtfs_iupdate_choose = {
	miuo_times:                mtfs_update_attr_times_choose,
	miuo_atime:                mtfs_update_attr_atime_choose,
	miuo_attrs:                mtfs_update_inode_attrs_choose,
	miuo_size:                 mtfs_update_inode_size_choose,
};
EXPORT_SYMBOL(mtfs_iupdate_choose);

struct mtfs_iupdate_operations mtfs_iupdate_master = {
	miuo_times:                mtfs_update_attr_times_master,
	miuo_atime:                mtfs_update_attr_atime_master,
	miuo_attrs:                mtfs_update_inode_attrs_master,
	miuo_size:                 mtfs_update_inode_size_master,
};
EXPORT_SYMBOL(mtfs_iupdate_master);

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
	msubject_inode_init(inode);
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

	list = mtfs_oplist_build(dir, &mtfs_oplist_flag);
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

	mtfs_oplist_gather(list);
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

int mtfs_create_branch(struct inode *dir,
                       struct dentry *dentry,
                       int mode,
                       struct nameidata *nd,
                       mtfs_bindex_t bindex)
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

int mtfs_create(struct inode *dir,
                struct dentry *dentry,
                int mode,
                struct nameidata *nd)
{
	int ret = 0;
	struct mtfs_io *io = NULL;
	struct mtfs_io_create *io_create = NULL;
	MENTRY();

	MDEBUG("create [%.*s]\n", dentry->d_name.len, dentry->d_name.name);
	MASSERT(inode_is_locked(dir));
	MASSERT(dentry->d_inode == NULL);

	MTFS_SLAB_ALLOC_PTR(io, mtfs_io_cache);
	if (io == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	io_create = &io->u.mi_create;

	io->mi_type = MIOT_CREATE;
	io->mi_bindex = 0;
	io->mi_oplist_inode = dir;
	io->mi_bnum = mtfs_d2bnum(dentry);
	io->mi_break = 0;
	io->mi_ops = &((*(mtfs_i2ops(dir)->io_ops))[io->mi_type]);

	io_create->dir = dir;
	io_create->dentry = dentry;
	io_create->mode = mode;
	io_create->nd = nd;

	ret = mtfs_io_loop(io);
	if (ret) {
		MERROR("failed to loop on io\n");
	} else {
		ret = io->mi_result.ret;
	}

	ret = mtfs_interpose(dentry, dir->i_sb, INTERPOSE_DEFAULT);
	if (ret) {
		MERROR("create failed when interposing, ret = %d\n", ret);
		goto out_free_io;
	}

	 /* Handle open intent */
	if (nd && (nd->flags & LOOKUP_OPEN) && dentry->d_inode) {
		struct file *filp;
		filp = lookup_instantiate_filp(nd, dentry, NULL);
		if (IS_ERR(filp)) {
			ret = PTR_ERR(filp);
			MERROR("open failed, ret = %d\n", ret);
			goto out_free_io;
		}
	}

	mtfs_update_attr_times(dir);
	mtfs_update_inode_size(dir);

out_free_io:
	MTFS_SLAB_FREE_PTR(io, mtfs_io_cache);
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_create);

int mtfs_link_branch(struct dentry *old_dentry,
                     struct inode *dir,
                     struct dentry *new_dentry,
                     mtfs_bindex_t bindex)
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

int mtfs_link(struct dentry *old_dentry,
              struct inode *dir,
              struct dentry *new_dentry)
{
	int ret = 0;
	struct mtfs_io *io = NULL;
	struct mtfs_io_link *io_link = NULL;
	mtfs_bindex_t bindex = 0;
	MENTRY();

	MDEBUG("link [%.*s] to [%.*s]\n",
	       old_dentry->d_name.len, old_dentry->d_name.name,
	       new_dentry->d_name.len, new_dentry->d_name.name);
	MASSERT(old_dentry->d_inode);
	MASSERT(inode_is_locked(old_dentry->d_inode));
	MASSERT(!S_ISDIR(old_dentry->d_inode->i_mode));
	MASSERT(inode_is_locked(dir));

	MTFS_SLAB_ALLOC_PTR(io, mtfs_io_cache);
	if (io == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	io_link = &io->u.mi_link;

	io->mi_type = MIOT_LINK;
	io->mi_bindex = 0;
	io->mi_oplist_inode = dir;
	io->mi_bnum = mtfs_i2bnum(dir);
	io->mi_break = 0;
	io->mi_ops = &((*(mtfs_i2ops(dir)->io_ops))[io->mi_type]);

	io_link->old_dentry = old_dentry;
	io_link->dir = dir;
	io_link->new_dentry = new_dentry;

	ret = mtfs_io_loop(io);
	if (ret) {
		MERROR("failed to loop on io\n");
	} else {
		ret = io->mi_result.ret;
	}

	if (ret) {
		goto out_free_io;
	}

	for (bindex = 0; bindex < mtfs_i2bnum(dir); bindex++) {
		dput(mtfs_d2branch(new_dentry, bindex));
	}

	ret = mtfs_lookup_backend(dir, new_dentry, INTERPOSE_DEFAULT);
	if (ret) {
		MERROR("link failed when lookup, ret = %d\n", ret);
		goto out_free_io;
	}

	mtfs_update_attr_times(dir);
	mtfs_update_inode_size(dir);
	old_dentry->d_inode->i_nlink = mtfs_get_nlinks(old_dentry->d_inode);
out_free_io:
	MTFS_SLAB_FREE_PTR(io, mtfs_io_cache);
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

int mtfs_unlink_branch(struct inode *dir,
                       struct dentry *dentry,
                       mtfs_bindex_t bindex)
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
	struct mtfs_io *io = NULL;
	struct mtfs_io_unlink *io_unlink = NULL;
	MENTRY();

	MDEBUG("unlink [%.*s]\n",
	       dentry->d_name.len, dentry->d_name.name);
	MASSERT(inode_is_locked(dir));
	MASSERT(dentry->d_inode);
	MASSERT(inode_is_locked(dentry->d_inode));

	dget(dentry);

	MTFS_SLAB_ALLOC_PTR(io, mtfs_io_cache);
	if (io == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	io_unlink = &io->u.mi_unlink;

	io->mi_type = MIOT_UNLINK;
	io->mi_bindex = 0;
	io->mi_oplist_inode = dir;
	io->mi_bnum = mtfs_i2bnum(dir);
	io->mi_break = 0;
	io->mi_ops = &((*(mtfs_i2ops(dir)->io_ops))[io->mi_type]);

	io_unlink->dir = dir;
	io_unlink->dentry = dentry;

	ret = mtfs_io_loop(io);
	if (ret) {
		MERROR("failed to loop on io\n");
	} else {
		ret = io->mi_result.ret;
	}

	if (ret) {
		goto out_free_io;
	}

	mtfs_update_attr_times(dir);
	dentry->d_inode->i_nlink = mtfs_get_nlinks(dentry->d_inode);
	/* Call d_drop so the system "forgets" about us */
	d_drop(dentry);

out_free_io:
	MTFS_SLAB_FREE_PTR(io, mtfs_io_cache);
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

int mtfs_rmdir_branch(struct inode *dir,
                      struct dentry *dentry,
                      mtfs_bindex_t bindex)
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
	int ret = 0;
	struct mtfs_io *io = NULL;
	struct mtfs_io_rmdir *io_rmdir = NULL;
	MENTRY();

	MDEBUG("rmdir [%.*s]\n",
	       dentry->d_name.len, dentry->d_name.name);
	MASSERT(inode_is_locked(dir));
	MASSERT(dentry->d_inode);
	MASSERT(inode_is_locked(dentry->d_inode));

	dget(dentry);

	MTFS_SLAB_ALLOC_PTR(io, mtfs_io_cache);
	if (io == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	io_rmdir = &io->u.mi_rmdir;

	io->mi_type = MIOT_RMDIR;
	io->mi_bindex = 0;
	io->mi_oplist_inode = dir;
	io->mi_bnum = mtfs_i2bnum(dir);
	io->mi_break = 0;
	io->mi_ops = &((*(mtfs_i2ops(dir)->io_ops))[io->mi_type]);

	io_rmdir->dir = dir;
	io_rmdir->dentry = dentry;

	ret = mtfs_io_loop(io);
	if (ret) {
		MERROR("failed to loop on io\n");
	} else {
		ret = io->mi_result.ret;
	}

	if (ret) {
		goto out_free_io;
	}

	mtfs_update_attr_times(dir);
	dentry->d_inode->i_nlink = mtfs_get_nlinks(dentry->d_inode);
	/* Call d_drop so the system "forgets" about us */
	d_drop(dentry);

out_free_io:
	MTFS_SLAB_FREE_PTR(io, mtfs_io_cache);
out:
	dput(dentry);
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_rmdir);

int mtfs_symlink_branch(struct inode *dir,
                        struct dentry *dentry,
                        const char *symname,
                        mtfs_bindex_t bindex)
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
	struct mtfs_io *io = NULL;
	struct mtfs_io_symlink *io_symlink = NULL;
	MENTRY();

	MDEBUG("symlink [%.*s] to [%s]\n",
	       dentry->d_name.len, dentry->d_name.name, symname);
	MASSERT(inode_is_locked(dir));
	MASSERT(dentry->d_inode == NULL);

	MTFS_SLAB_ALLOC_PTR(io, mtfs_io_cache);
	if (io == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	io_symlink = &io->u.mi_symlink;

	io->mi_type = MIOT_SYMLINK;
	io->mi_bindex = 0;
	io->mi_oplist_inode = dir;
	io->mi_bnum = mtfs_i2bnum(dir);
	io->mi_break = 0;
	io->mi_ops = &((*(mtfs_i2ops(dir)->io_ops))[io->mi_type]);

	io_symlink->dir = dir;
	io_symlink->dentry = dentry;
	io_symlink->symname = symname;

	ret = mtfs_io_loop(io);
	if (ret) {
		MERROR("failed to loop on io\n");
	} else {
		ret = io->mi_result.ret;
	}

	if (ret) {
		goto out_free_io;
	}
	
	ret = mtfs_interpose(dentry, dir->i_sb, INTERPOSE_DEFAULT);
	if (ret) {
		goto out_free_io;
	}

	mtfs_update_attr_times(dir);
	mtfs_update_inode_size(dir);
out_free_io:
	MTFS_SLAB_FREE_PTR(io, mtfs_io_cache);
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_symlink);

int mtfs_mkdir_branch(struct inode *dir,
                      struct dentry *dentry,
                      int mode,
                      mtfs_bindex_t bindex)
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
	struct mtfs_io *io = NULL;
	struct mtfs_io_mkdir *io_mkdir = NULL;
	MENTRY();

	MDEBUG("mkdir [%.*s]\n",
	       dentry->d_name.len, dentry->d_name.name);
	MASSERT(inode_is_locked(dir));
	MASSERT(dentry->d_inode == NULL);

	MTFS_SLAB_ALLOC_PTR(io, mtfs_io_cache);
	if (io == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	io_mkdir = &io->u.mi_mkdir;

	io->mi_type = MIOT_MKDIR;
	io->mi_bindex = 0;
	io->mi_oplist_inode = dir;
	io->mi_bnum = mtfs_i2bnum(dir);
	io->mi_break = 0;
	io->mi_ops = &((*(mtfs_i2ops(dir)->io_ops))[io->mi_type]);

	io_mkdir->dir = dir;
	io_mkdir->dentry = dentry;
	io_mkdir->mode = mode;

	ret = mtfs_io_loop(io);
	if (ret) {
		MERROR("failed to loop on io\n");
	} else {
		ret = io->mi_result.ret;
	}

	if (ret) {
		goto out_free_io;
	}
	
	ret = mtfs_interpose(dentry, dir->i_sb, INTERPOSE_DEFAULT);
	if (ret) {
		goto out_free_io;
	}

	mtfs_update_attr_times(dir);
	mtfs_update_inode_size(dir);
out_free_io:
	MTFS_SLAB_FREE_PTR(io, mtfs_io_cache);
out:
	if (!dentry->d_inode) {
		d_drop(dentry);
	}
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_mkdir);

int mtfs_mknod_branch(struct inode *dir,
                      struct dentry *dentry,
                      int mode,
                      dev_t dev,
                      mtfs_bindex_t bindex)
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

int mtfs_mknod(struct inode *dir,
               struct dentry *dentry,
               int mode,
               dev_t dev)
{
	int ret = 0;
	struct mtfs_io *io = NULL;
	struct mtfs_io_mknod *io_mknod = NULL;
	MENTRY();

	MDEBUG("mknode [%.*s]\n",
	       dentry->d_name.len, dentry->d_name.name);
	MASSERT(inode_is_locked(dir));
	MASSERT(dentry->d_inode == NULL);

	MTFS_SLAB_ALLOC_PTR(io, mtfs_io_cache);
	if (io == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	io_mknod = &io->u.mi_mknod;

	io->mi_type = MIOT_MKNOD;
	io->mi_bindex = 0;
	io->mi_oplist_inode = dir;
	io->mi_bnum = mtfs_i2bnum(dir);
	io->mi_break = 0;
	io->mi_ops = &((*(mtfs_i2ops(dir)->io_ops))[io->mi_type]);

	io_mknod->dir = dir;
	io_mknod->dentry = dentry;
	io_mknod->mode = mode;
	io_mknod->dev = dev;

	ret = mtfs_io_loop(io);
	if (ret) {
		MERROR("failed to loop on io\n");
	} else {
		ret = io->mi_result.ret;
	}

	if (ret) {
		goto out_free_io;
	}
	
	ret = mtfs_interpose(dentry, dir->i_sb, INTERPOSE_DEFAULT);
	if (ret) {
		goto out_free_io;
	}

	mtfs_update_attr_times(dir);
	mtfs_update_inode_size(dir);
out_free_io:
	MTFS_SLAB_FREE_PTR(io, mtfs_io_cache);
out:
	if (!dentry->d_inode) {
		d_drop(dentry);
	}
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_mknod);

int mtfs_rename_branch(struct inode *old_dir,
                       struct dentry *old_dentry,
                       struct inode *new_dir,
                       struct dentry *new_dentry,
                       mtfs_bindex_t bindex)
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

int mtfs_rename(struct inode *old_dir,
                struct dentry *old_dentry,
                struct inode *new_dir,
                struct dentry *new_dentry)
{
	int ret = 0;
	struct mtfs_io *io = NULL;
	struct mtfs_io_rename *io_rename = NULL;
	MENTRY();

	MDEBUG("rename [%.*s] to [%.*s]\n",
	       old_dentry->d_name.len, old_dentry->d_name.name,
	       new_dentry->d_name.len, new_dentry->d_name.name);
	MASSERT(inode_is_locked(old_dir));
	MASSERT(inode_is_locked(new_dir));
	MASSERT(old_dentry->d_inode);
	if (new_dentry->d_inode) {
		MASSERT(inode_is_locked(new_dentry->d_inode));
	}

	/* TODO: build according to new_dir */
	MTFS_SLAB_ALLOC_PTR(io, mtfs_io_cache);
	if (io == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	io_rename = &io->u.mi_rename;

	io->mi_type = MIOT_RENAME;
	io->mi_bindex = 0;
	/* TODO: build according to new_dir */
	io->mi_oplist_inode = old_dir;
	io->mi_bnum = mtfs_i2bnum(old_dir);
	io->mi_break = 0;
	io->mi_ops = &((*(mtfs_i2ops(old_dir)->io_ops))[io->mi_type]);

	io_rename->old_dir = old_dir;
	io_rename->old_dentry = old_dentry;
	io_rename->new_dir = new_dir;
	io_rename->new_dentry = new_dentry;

	ret = mtfs_io_loop(io);
	if (ret) {
		MERROR("failed to loop on io\n");
	} else {
		ret = io->mi_result.ret;
	}

	if (ret) {
		goto out_free_io;
	}

	mtfs_update_inode_attrs(old_dir);
	if (new_dir != old_dir) {
		mtfs_update_inode_attrs(new_dir);
	}

	/* This flag is seted: FS_RENAME_DOES_D_MOVE */
	d_move(old_dentry, new_dentry);
out_free_io:
	MTFS_SLAB_FREE_PTR(io, mtfs_io_cache);
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_rename);

int mtfs_readlink_branch(struct dentry *dentry,
                         char __user *buf,
                         int bufsiz,
                         mtfs_bindex_t bindex)
{
	int ret = 0;
	struct dentry *hidden_dentry = mtfs_d2branch(dentry, bindex);
	MENTRY();

	ret = mtfs_device_branch_errno(mtfs_d2dev(dentry), bindex, BOPS_MASK_READ);
	if (ret) {
		MDEBUG("branch[%d] is abandoned\n", bindex);
		goto out; 
	}

	if (hidden_dentry && hidden_dentry->d_inode) {
		MASSERT(hidden_dentry->d_inode->i_op);
		MASSERT(hidden_dentry->d_inode->i_op->readlink);
		ret = hidden_dentry->d_inode->i_op->readlink(hidden_dentry, buf, bufsiz);
	} else {
		MDEBUG("branch[%d] of dentry [%.*s] is %s\n",
		       bindex, dentry->d_name.len, dentry->d_name.name,
		       hidden_dentry == NULL ? "NULL" : "negative");
		ret = -ENOENT;
	}
out:
	MRETURN(ret);
}

int mtfs_readlink(struct dentry *dentry, char __user *buf, int bufsiz)
{
	int ret = 0;
	struct mtfs_io *io = NULL;
	struct mtfs_io_readlink *io_readlink = NULL;
	MENTRY();

	MDEBUG("readlink [%.*s]\n",
	       dentry->d_name.len, dentry->d_name.name);
	MASSERT(dentry->d_inode);

	MTFS_SLAB_ALLOC_PTR(io, mtfs_io_cache);
	if (io == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	io_readlink = &io->u.mi_readlink;

	io->mi_type = MIOT_READLINK;
	io->mi_bindex = 0;
	io->mi_oplist_dentry = dentry;
	io->mi_bnum = mtfs_d2bnum(dentry);
	io->mi_break = 0;
	io->mi_ops = &((*(mtfs_d2ops(dentry)->io_ops))[io->mi_type]);

	io_readlink->dentry = dentry;
	io_readlink->buf = buf;
	io_readlink->bufsiz = bufsiz;

	ret = mtfs_io_loop(io);
	if (ret) {
		MERROR("failed to loop on io\n");
	} else {
		ret = io->mi_result.ret;
	}

	if (ret < 0) {
		goto out_free_io;
	}

	mtfs_update_attr_atime(dentry->d_inode);

out_free_io:
	MTFS_SLAB_FREE_PTR(io, mtfs_io_cache);
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
int mtfs_permission_branch(struct inode *inode,
                           int mask,
                           mtfs_bindex_t bindex)
#else /* !HAVE_INODE_PERMISION_2ARGS */
int mtfs_permission_branch(struct inode *inode,
                           int mask,
                           struct nameidata *nd,
                           mtfs_bindex_t bindex)
#endif /* !HAVE_INODE_PERMISION_2ARGS */
{
	int ret = 0;
	struct inode *hidden_inode = mtfs_i2branch(inode, bindex);
	MENTRY();

#if 0 /* Abandon tests will fail */
	ret = mtfs_device_branch_errno(mtfs_i2dev(inode), bindex, BOPS_MASK_READ);
	if (ret) {
		MDEBUG("branch[%d] is abandoned\n", bindex);
		MRETURN(ret); 
	}
#endif

	if (hidden_inode) {
#ifdef HAVE_INODE_PERMISION
		ret = inode_permission(hidden_inode, mask);
#else /* !HAVE_INODE_PERMISION */
		ret = permission(hidden_inode, mask, nd);
#endif /* !HAVE_INODE_PERMISION */
	} else {
		MDEBUG("branch[%d] of inode is NULL\n");
		ret = -ENOENT;
	}

	MRETURN(ret);
}

#ifdef HAVE_INODE_PERMISION_2ARGS
int mtfs_permission(struct inode *inode, int mask)
#else /* !HAVE_INODE_PERMISION_2ARGS */
int mtfs_permission(struct inode *inode, int mask, struct nameidata *nd)
#endif /* !HAVE_INODE_PERMISION_2ARGS */
{
	int ret = 0;
	struct mtfs_io *io = NULL;
	struct mtfs_io_permission *io_permission = NULL;
	MENTRY();

	MDEBUG("permission\n");

	MTFS_SLAB_ALLOC_PTR(io, mtfs_io_cache);
	if (io == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	io_permission = &io->u.mi_permission;

	io->mi_type = MIOT_PERMISSION;
	io->mi_bindex = 0;
	io->mi_oplist_inode = inode;
	io->mi_bnum = mtfs_i2bnum(inode);
	io->mi_break = 0;
	io->mi_ops = &((*(mtfs_i2ops(inode)->io_ops))[io->mi_type]);

	io_permission->inode = inode;
	io_permission->mask = mask;
	io_permission->nd = nd;

	ret = mtfs_io_loop(io);
	if (ret) {
		MERROR("failed to loop on io\n");
	} else {
		ret = io->mi_result.ret;
	}

	MTFS_SLAB_FREE_PTR(io, mtfs_io_cache);
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_permission);

int mtfs_setattr_branch(struct dentry *dentry,
                        struct iattr *ia,
                        mtfs_bindex_t bindex)
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
	struct mtfs_io *io = NULL;
	struct mtfs_io_setattr *io_setattr = NULL;
	MENTRY();

	MDEBUG("setattr [%.*s]\n", dentry->d_name.len, dentry->d_name.name);
	MASSERT(dentry->d_inode);
	MASSERT(inode_is_locked(dentry->d_inode));

	MTFS_SLAB_ALLOC_PTR(io, mtfs_io_cache);
	if (io == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	io_setattr = &io->u.mi_setattr;

	io->mi_type = MIOT_SETATTR;
	io->mi_bindex = 0;
	io->mi_oplist_dentry = dentry;
	io->mi_bnum = mtfs_d2bnum(dentry);
	io->mi_break = 0;
	io->mi_ops = &((*(mtfs_d2ops(dentry)->io_ops))[io->mi_type]);

	io_setattr->dentry = dentry;
	io_setattr->ia = ia;

	ret = mtfs_io_loop(io);
	if (ret) {
		MERROR("failed to loop on io\n");
	} else {
		ret = io->mi_result.ret;
	}

	mtfs_update_inode_attrs(dentry->d_inode);
	if (ia->ia_valid & ATTR_SIZE) {
		mtfs_update_inode_size(dentry->d_inode);
	}

	MTFS_SLAB_FREE_PTR(io, mtfs_io_cache);
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_setattr);

int mtfs_getattr_branch(struct vfsmount *mnt,
                        struct dentry *dentry,
                        struct kstat *stat,
                        mtfs_bindex_t bindex)
{
	int ret = 0;
	struct dentry *hidden_dentry = mtfs_d2branch(dentry, bindex);
	struct vfsmount *hidden_mnt = NULL;
	MENTRY();

	ret = mtfs_device_branch_errno(mtfs_d2dev(dentry), bindex, BOPS_MASK_READ);
	if (ret) {
		MDEBUG("branch[%d] is abandoned\n", bindex);
		goto out; 
	}

	if (hidden_dentry && hidden_dentry->d_inode) {
		hidden_dentry = dget(hidden_dentry);
		hidden_mnt = mtfs_s2mntbranch(dentry->d_sb, bindex);
		MASSERT(hidden_mnt);
		ret = vfs_getattr(hidden_mnt, hidden_dentry, stat);
		if (!ret) {
			fsstack_copy_attr_all(dentry->d_inode, hidden_dentry->d_inode, mtfs_get_nlinks);
		}
		dput(hidden_dentry);
	} else {
		MDEBUG("branch[%d] of dentry [%.*s] is %s\n",
		       bindex, dentry->d_name.len, dentry->d_name.name,
		       hidden_dentry == NULL ? "NULL" : "negative");
		ret = -ENOENT;
	}
out:
	MRETURN(ret);
}

int mtfs_getattr(struct vfsmount *mnt,
                 struct dentry *dentry,
                 struct kstat *stat)
{
	int ret = 0;
	struct mtfs_io *io = NULL;
	struct mtfs_io_getattr *io_getattr = NULL;
	MENTRY();

	MDEBUG("getattr [%.*s]\n",
	       dentry->d_name.len, dentry->d_name.name);
	MASSERT(dentry->d_inode);

	MTFS_SLAB_ALLOC_PTR(io, mtfs_io_cache);
	if (io == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	io_getattr = &io->u.mi_getattr;

	io->mi_type = MIOT_GETATTR;
	io->mi_bindex = 0;
	io->mi_oplist_dentry = dentry;
	io->mi_bnum = mtfs_d2bnum(dentry);
	io->mi_break = 0;
	io->mi_ops = &((*(mtfs_d2ops(dentry)->io_ops))[io->mi_type]);

	io_getattr->mnt = mnt;
	io_getattr->dentry = dentry;
	io_getattr->stat = stat;

	ret = mtfs_io_loop(io);
	if (ret) {
		MERROR("failed to loop on io\n");
	} else {
		ret = io->mi_result.ret;
	}

	MTFS_SLAB_FREE_PTR(io, mtfs_io_cache);
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_getattr);

ssize_t mtfs_getxattr_branch(struct dentry *dentry,
                             const char *name,
                             void *value,
                             size_t size,
                             mtfs_bindex_t bindex)
{
	ssize_t ret = 0;
	struct dentry *hidden_dentry = mtfs_d2branch(dentry, bindex);
	MENTRY();

	ret = mtfs_device_branch_errno(mtfs_d2dev(dentry), bindex, BOPS_MASK_READ);
	if (ret) {
		MDEBUG("branch[%d] is abandoned\n", bindex);
		goto out; 
	}

	if (hidden_dentry && hidden_dentry->d_inode) {
		MASSERT(hidden_dentry->d_inode->i_op);
		MASSERT(hidden_dentry->d_inode->i_op->getxattr);
		ret = hidden_dentry->d_inode->i_op->getxattr(hidden_dentry, name, value, size);
	} else {
		MDEBUG("branch[%d] of dentry [%.*s] is %s\n",
		       bindex, dentry->d_name.len, dentry->d_name.name,
		       hidden_dentry == NULL ? "NULL" : "negative");
		ret = -ENOENT;
	}
out:
	MRETURN(ret);
}

ssize_t mtfs_getxattr(struct dentry *dentry,
                      const char *name,
                      void *value,
                      size_t size) 
{
	ssize_t ret = 0;
	struct mtfs_io *io = NULL;
	struct mtfs_io_getxattr *io_getxattr = NULL;
	MENTRY();

	MDEBUG("getxattr %s of [%.*s]\n", name,
	       dentry->d_name.len, dentry->d_name.name);
	MASSERT(dentry->d_inode);

	MTFS_SLAB_ALLOC_PTR(io, mtfs_io_cache);
	if (io == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	io_getxattr = &io->u.mi_getxattr;

	io->mi_type = MIOT_GETXATTR;
	io->mi_bindex = 0;
	io->mi_oplist_dentry = dentry;
	io->mi_bnum = mtfs_d2bnum(dentry);
	io->mi_break = 0;
	io->mi_ops = &((*(mtfs_d2ops(dentry)->io_ops))[io->mi_type]);

	io_getxattr->dentry = dentry;
	io_getxattr->name = name;
	io_getxattr->value = value;
	io_getxattr->size = size;

	ret = mtfs_io_loop(io);
	if (ret) {
		MERROR("failed to loop on io\n");
	} else {
		ret = io->mi_result.ssize;
	}

	MTFS_SLAB_FREE_PTR(io, mtfs_io_cache);
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_getxattr);

int mtfs_setxattr_branch(struct dentry *dentry,
                         const char *name,
                         const void *value,
                         size_t size,
                         int flags,
                         mtfs_bindex_t bindex)
{
	int ret = 0;
	struct dentry *hidden_dentry = mtfs_d2branch(dentry, bindex);
	MENTRY();

	ret = mtfs_device_branch_errno(mtfs_d2dev(dentry), bindex,
	                               BOPS_MASK_WRITE);
	if (ret) {
		MDEBUG("branch[%d] is abandoned\n", bindex);
		goto out; 
	}

	if (hidden_dentry && hidden_dentry->d_inode) {
		MASSERT(hidden_dentry->d_inode->i_op);
		MASSERT(hidden_dentry->d_inode->i_op->setxattr);
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

int mtfs_setxattr(struct dentry *dentry,
                  const char *name,
                  const void *value,
                  size_t size,
                  int flags)
{
	int ret = 0;
	struct mtfs_io *io = NULL;
	struct mtfs_io_setxattr *io_setxattr = NULL;
	MENTRY();

	MDEBUG("setxattr %s of [%.*s]\n", name,
	       dentry->d_name.len, dentry->d_name.name);
	MASSERT(dentry->d_inode);
	MASSERT(inode_is_locked(dentry->d_inode));

	MTFS_SLAB_ALLOC_PTR(io, mtfs_io_cache);
	if (io == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	io_setxattr = &io->u.mi_setxattr;

	io->mi_type = MIOT_SETXATTR;
	io->mi_bindex = 0;
	io->mi_oplist_dentry = dentry;
	io->mi_bnum = mtfs_d2bnum(dentry);
	io->mi_break = 0;
	io->mi_ops = &((*(mtfs_d2ops(dentry)->io_ops))[io->mi_type]);

	io_setxattr->dentry = dentry;
	io_setxattr->name = name;
	io_setxattr->value = value;
	io_setxattr->size = size;
	io_setxattr->flags = flags;

	ret = mtfs_io_loop(io);
	if (ret) {
		MERROR("failed to loop on io\n");
	} else {
		ret = io->mi_result.ret;
	}

	MTFS_SLAB_FREE_PTR(io, mtfs_io_cache);
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_setxattr);

int mtfs_removexattr_branch(struct dentry *dentry,
                            const char *name,
                            mtfs_bindex_t bindex)
{
	int ret = 0;
	struct dentry *hidden_dentry = mtfs_d2branch(dentry, bindex);
	MENTRY();

	ret = mtfs_device_branch_errno(mtfs_d2dev(dentry), bindex,
	                               BOPS_MASK_WRITE);
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
	struct mtfs_io *io = NULL;
	struct mtfs_io_removexattr *io_removexattr = NULL;
	MENTRY();

	MDEBUG("removexattr %s of [%.*s]\n", name,
	       dentry->d_name.len, dentry->d_name.name);
	MASSERT(dentry->d_inode);
	MASSERT(inode_is_locked(dentry->d_inode));

	MTFS_SLAB_ALLOC_PTR(io, mtfs_io_cache);
	if (io == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	io_removexattr = &io->u.mi_removexattr;

	io->mi_type = MIOT_REMOVEXATTR;
	io->mi_bindex = 0;
	io->mi_oplist_dentry = dentry;
	io->mi_bnum = mtfs_d2bnum(dentry);
	io->mi_break = 0;
	io->mi_ops = &((*(mtfs_d2ops(dentry)->io_ops))[io->mi_type]);

	io_removexattr->dentry = dentry;
	io_removexattr->name = name;

	ret = mtfs_io_loop(io);
	if (ret) {
		MERROR("failed to loop on io\n");
	} else {
		ret = io->mi_result.ret;
	}

	MTFS_SLAB_FREE_PTR(io, mtfs_io_cache);
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_removexattr);

ssize_t mtfs_listxattr_branch(struct dentry *dentry,
                              char *list,
                              size_t size,
                              mtfs_bindex_t bindex)
{
	ssize_t ret = 0;
	struct dentry *hidden_dentry = mtfs_d2branch(dentry, bindex);
	MENTRY();

	ret = mtfs_device_branch_errno(mtfs_d2dev(dentry), bindex, BOPS_MASK_READ);
	if (ret) {
		MDEBUG("branch[%d] is abandoned\n", bindex);
		goto out; 
	}

	if (hidden_dentry && hidden_dentry->d_inode) {
		MASSERT(hidden_dentry->d_inode->i_op);
		MASSERT(hidden_dentry->d_inode->i_op->listxattr);
		ret = hidden_dentry->d_inode->i_op->listxattr(hidden_dentry, list, size);
	} else {
		MDEBUG("branch[%d] of dentry [%.*s] is %s\n",
		       bindex, dentry->d_name.len, dentry->d_name.name,
		       hidden_dentry == NULL ? "NULL" : "negative");
		ret = -ENOENT;
	}
out:
	MRETURN(ret);
}

ssize_t mtfs_listxattr(struct dentry *dentry,
                       char *list,
                       size_t size)
{
	ssize_t ret = 0;
	struct mtfs_io *io = NULL;
	struct mtfs_io_listxattr *io_listxattr = NULL;
	MENTRY();

	MDEBUG("listxattr [%.*s]\n",
	       dentry->d_name.len, dentry->d_name.name);
	MASSERT(dentry->d_inode);

	MTFS_SLAB_ALLOC_PTR(io, mtfs_io_cache);
	if (io == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	io_listxattr = &io->u.mi_listxattr;

	io->mi_type = MIOT_LISTXATTR;
	io->mi_bindex = 0;
	io->mi_oplist_dentry = dentry;
	io->mi_bnum = mtfs_d2bnum(dentry);
	io->mi_break = 0;
	io->mi_ops = &((*(mtfs_d2ops(dentry)->io_ops))[io->mi_type]);

	io_listxattr->dentry = dentry;
	io_listxattr->list = list;
	io_listxattr->size = size;

	ret = mtfs_io_loop(io);
	if (ret) {
		MERROR("failed to loop on io\n");
	} else {
		ret = io->mi_result.ssize;
	}

	MTFS_SLAB_FREE_PTR(io, mtfs_io_cache);
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mtfs_listxattr);

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
