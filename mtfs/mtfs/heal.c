/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <mtfs_list.h>
#include <mtfs_inode.h>
#include <mtfs_dentry.h>
#include <memory.h>
#include <mtfs_selfheal.h>
#include "inode_internal.h"
#include "heal_internal.h"
#include "support_internal.h"

/* If succeed, return dchild_new which needed to be dput */
struct dentry *mtfs_dchild_rename2new(struct dentry *dparent, struct dentry *dchild_old,
                                      const unsigned char *name_new, unsigned int len)
{
	struct dentry *dchild_new = NULL;
	int ret = 0;
	MENTRY();

	MASSERT(inode_is_locked(dparent->d_inode));
	dchild_new = lookup_one_len(name_new, dparent, len);
	if (IS_ERR(dchild_new)) {
		MERROR("lookup [%.*s/%s] failed, ret = %d\n",
		       dparent->d_name.len, dparent->d_name.name, name_new, ret);
		goto out;
	}

	if (dchild_new->d_inode != NULL) {
		dput(dchild_new);
		dchild_new = ERR_PTR(-EEXIST);
		goto out;
	}

	MDEBUG("renaming [%.*s/%.*s] to [%.*s/%.*s]\n",
	       dparent->d_name.len, dparent->d_name.name,
	       dchild_old->d_name.len, dchild_old->d_name.name,
	       dparent->d_name.len, dparent->d_name.name,
	       dchild_new->d_name.len, dchild_new->d_name.name);
	/*
	 * Rename in the same dir, and dir is locked,
	 * no need to lock_rename()
	 */
	ret = vfs_rename(dparent->d_inode, dchild_old,
	                 dparent->d_inode, dchild_new);
	if (ret) {
		dput(dchild_new);
		dchild_new = ERR_PTR(ret);
	}

out:
	MRETURN(dchild_new);
}

struct dentry *_mtfs_dchild_add_ino(struct dentry *dparent, struct dentry *dchild_old)
{
	char *name_new = NULL;
	struct dentry *dchild_new = NULL;
	MENTRY();

	MASSERT(dparent);
	MASSERT(dparent->d_inode);
	MASSERT(inode_is_locked(dparent->d_inode));
	MTFS_ALLOC(name_new, PATH_MAX);
	if (unlikely(name_new == NULL)) {
		dchild_new = ERR_PTR(-ENOMEM);
		goto out;
	}

	strncpy(name_new, dchild_old->d_name.name, dchild_old->d_name.len);
	sprintf(name_new, "%s:%lx", name_new, dchild_old->d_inode->i_ino);
	dchild_new = mtfs_dchild_rename2new(dparent, dchild_old, name_new, strlen(name_new));

	MTFS_FREE(name_new, PATH_MAX);
out:
	MRETURN(dchild_new);
}

/*
 * Return d_child needed to be dput() if succeed
 */
struct dentry *_mtfs_dchild_create(struct dentry *dparent, const unsigned char *name,
                                  unsigned int len, umode_t mode, dev_t rdev, int rename)
{
	int ret = 0;
	struct dentry *dchild = NULL;
	MENTRY();

	MASSERT(inode_is_locked(dparent->d_inode));

	dchild = lookup_one_len(name, dparent, len);
	if (IS_ERR(dchild)) {
		MERROR("lookup [%s] under [%.*s] failed, ret = %d\n",
		       name, dparent->d_name.len, dparent->d_name.name, ret);
		ret = PTR_ERR(dchild);
		goto out;
	}

	if (dchild->d_inode) {
		if ((dchild->d_inode->i_mode & S_IFMT) == (mode & S_IFMT)) {
			MDEBUG("[%.*s/%.*s] already existed\n",
			       dparent->d_name.len, dparent->d_name.name,
			       len, name);
			if ((dchild->d_inode->i_mode & S_IALLUGO) != (mode & S_IALLUGO)) {
				MDEBUG("permission mode of [%.*s/%.*s] is 0%04o, "
				       "which should be 0%04o for security\n",
				       dparent->d_name.len, dparent->d_name.name,
				       len, name,
				       dchild->d_inode->i_mode & S_IALLUGO, S_IRWXU);
			}
			goto out;
		} else {
			MDEBUG("[%.*s/%.*s] already existed, and is a %s, not a %s\n",
			       dparent->d_name.len, dparent->d_name.name, len, name,
			       mtfs_mode2type(dchild->d_inode->i_mode), mtfs_mode2type(mode));
			if (rename) {
				struct dentry *dchild_new = NULL;
				MDEBUG("Trying to rename [%.*s/%.*s]\n",
				       dparent->d_name.len, dparent->d_name.name,
				       len, name);
				dchild_new = _mtfs_dchild_add_ino(dparent, dchild);
				if (IS_ERR(dchild_new)) {
					ret = PTR_ERR(dchild_new);
					goto out_put;
				}
				dput(dchild_new);
				ret = -EAGAIN;
				goto out_put;
			} else {
				ret = -EEXIST;
				goto out_put;
			}
		}
	}

	switch (mode & S_IFMT) {
	case S_IFREG:
		ret = vfs_create(dparent->d_inode, dchild, mode, NULL);
		break;
	case S_IFDIR:
		ret = vfs_mkdir(dparent->d_inode, dchild, mode);
		break;
	case S_IFCHR:
	case S_IFBLK:
	case S_IFIFO:
	case S_IFSOCK:
		ret = vfs_mknod(dparent->d_inode, dchild, mode, rdev);
		break;
	default:
		MDEBUG("bad file type 0%o\n", mode & S_IFMT);
		ret = -EINVAL;
	}
	if (ret) {
		goto out_put;
	} else {
		MASSERT(dchild->d_inode);
	}
	goto out;
out_put:
	dput(dchild);
out:
	if (ret) {
		dchild = ERR_PTR(ret);
	}
	MRETURN(dchild);
}

/*
 * Return d_child needed to be dput() if succeed
 */
struct dentry *mtfs_dchild_create(struct dentry *dparent, const unsigned char *name,
                                  unsigned int len, umode_t mode, dev_t rdev, int repeat)
{
	struct dentry *dchild = NULL;
	MENTRY();

	MASSERT(dparent);
	MASSERT(name);
	MASSERT(dparent->d_inode);
	mutex_lock(&dparent->d_inode->i_mutex);
	dchild = _mtfs_dchild_create(dparent, name, len, mode, rdev, repeat);
	if (IS_ERR(dchild)) {
		if ((PTR_ERR(dchild) == -EAGAIN) && repeat) {
			dchild = _mtfs_dchild_create(dparent, name, len, mode, rdev, 0);
		}
	}
	mutex_unlock(&dparent->d_inode->i_mutex);
	MRETURN(dchild);
}

/*
 * Add the inode number to a dentry.
 * Return the dentry if successful.
 */
struct dentry *mtfs_dchild_add_ino(struct dentry *dparent, const unsigned char *name, unsigned int len)
{
	int ret = 0;
	struct dentry *dchild_old = NULL;
	struct dentry *dchild_new = NULL;
	MENTRY();

	MASSERT(dparent);
	MASSERT(dparent->d_inode);
	MASSERT(name);
	dget(dparent);
	mutex_lock(&dparent->d_inode->i_mutex);

	dchild_old = lookup_one_len(name, dparent, len);
	if (IS_ERR(dchild_old)) {
		MERROR("lookup [%.*s/%s] failed, ret = %d\n",
		       dparent->d_name.len, dparent->d_name.name, name, ret);
		ret = PTR_ERR(dchild_old);
		goto out_unlock;
	}

	if (dchild_old->d_inode == NULL) {
		dchild_new = dchild_old;
		goto out_unlock;
	}

	dchild_new = _mtfs_dchild_add_ino(dparent, dchild_old);

	dput(dchild_old);
out_unlock:
	mutex_unlock(&dparent->d_inode->i_mutex);
	dput(dparent);

	if (ret) {
		dchild_new = ERR_PTR(ret);
	}
	MRETURN(dchild_new);
}

struct dentry *mtfs_dentry_list_mkpath(struct dentry *d_parent, mtfs_list_t *dentry_list)
{
	struct mtfs_dentry_list *tmp_entry = NULL;
	struct dentry *d_child = NULL;
	struct dentry *d_parent_tmp = d_parent;
	MENTRY();

	mtfs_list_for_each_entry(tmp_entry, dentry_list, list) {
		d_child = mtfs_dchild_create(d_parent_tmp, tmp_entry->dentry->d_name.name,
		                             tmp_entry->dentry->d_name.len, S_IFDIR | S_IRWXU, 0, 1);
		if (d_parent_tmp != d_parent) {
			dput(d_parent_tmp);
		}	
		if (IS_ERR(d_child)) {
			MERROR("create [%.*s/%.*s] failed\n",
			       d_parent_tmp->d_name.len, d_parent_tmp->d_name.name,
			       tmp_entry->dentry->d_name.len, tmp_entry->dentry->d_name.name);
			goto out;
		}
		d_parent_tmp = d_child;
	}
out:
	if (d_child == NULL) {
		d_child = ERR_PTR(-ENOENT);
		MERROR("unexpected: dentry_list is empty\n");
		MBUG();
	}
	MRETURN(d_child);
}

static inline void mtfs_dentry_list_cleanup(mtfs_list_t *dentry_list)
{
	struct mtfs_dentry_list *tmp_entry = NULL;
	struct mtfs_dentry_list *n = NULL;

	mtfs_list_for_each_entry_safe(tmp_entry, n, dentry_list, list) {
		dput(tmp_entry->dentry);
		MTFS_FREE_PTR(tmp_entry);
	}
}

/*
 * needed to dput $dentry after being called.
 */
struct dentry *mtfs_dchild_remove(struct dentry *dparent, const char *name)
{
	int ret = 0;
	struct dentry *dchild = NULL;
	MENTRY();

	MASSERT(dparent);
	MASSERT(name);
	dget(dparent);
	mutex_lock(&dparent->d_inode->i_mutex);

	dchild = lookup_one_len(name, dparent, strlen(name));
	if (IS_ERR(dchild)) {
		MERROR("lookup [%s] under [%.*s] failed, ret = %d\n",
		       name, dparent->d_name.len, dparent->d_name.name, ret);
		ret = PTR_ERR(dchild);
		goto out;
	}

	if (dchild->d_inode == NULL) {
		dput(dchild);
		ret = -ENOENT;
		goto out;
	}

	MDEBUG("removing [%.*s/%.*s]\n",
	       dparent->d_name.len, dparent->d_name.name, dchild->d_name.len, dchild->d_name.name);
	switch (dchild->d_inode->i_mode & S_IFMT) {
	case S_IFDIR:
		ret = vfs_rmdir(dparent->d_inode, dchild);
		break;
	case S_IFREG:
	case S_IFCHR:
	case S_IFBLK:
	case S_IFIFO:
	case S_IFSOCK:
		ret = vfs_unlink(dparent->d_inode, dchild);
		MDEBUG("vfs_unlink, ret = %d\n", ret);
		break;
	default:
		MERROR("bad file type 0%o\n", dchild->d_inode->i_mode & S_IFMT);
		ret = -EINVAL;
	}

	if (ret) {
		dput(dchild);
	} else {
		/* This is not true, but why? */
		//MASSERT(dchild->d_inode == NULL);
	}

out:
	mutex_unlock(&dparent->d_inode->i_mutex);
	dput(dparent);
	if (ret) {
		dchild = ERR_PTR(ret);
	}
	MRETURN(dchild);
}

static inline int mutex_lock_if_needed(struct mutex *lock)
{
	if(!mutex_is_locked(lock)) {
		mutex_is_locked(lock);
	}
	return 0;
}

/*
 * p1 and p2 should be directories on the same fs.
 */
struct dentry *mtfs_lock_rename(struct dentry *p1, struct dentry *p2)
{
	struct dentry *p;

	if (p1 == p2) {
		mutex_lock_if_needed(&p1->d_inode->i_mutex);
		return NULL;
	}

	mutex_lock(&p1->d_inode->i_sb->s_vfs_rename_mutex);

	for (p = p1; p->d_parent != p; p = p->d_parent) {
		if (p->d_parent == p2) {
			mutex_lock(&p2->d_inode->i_mutex);
			mutex_lock(&p1->d_inode->i_mutex);
			return p;
		}
	}

	for (p = p2; p->d_parent != p; p = p->d_parent) {
		if (p->d_parent == p1) {
			mutex_lock(&p1->d_inode->i_mutex);
			mutex_lock(&p2->d_inode->i_mutex);
			return p;
		}
	}

	mutex_lock_if_needed(&p1->d_inode->i_mutex);
	mutex_lock_if_needed(&p2->d_inode->i_mutex);
	return NULL;
}

void mtfs_unlock_rename(struct dentry *p1, struct dentry *p2, int p1_locked, int p2_locked)
{
	if (!p1_locked) {
		mutex_unlock(&p1->d_inode->i_mutex);
	}
	if (p1 != p2) {
		if (!p2_locked) {
			mutex_unlock(&p2->d_inode->i_mutex);
		}
		mutex_unlock(&p1->d_inode->i_sb->s_vfs_rename_mutex);
	}
}

int mtfs_backup_branch(struct dentry *dentry, mtfs_bindex_t bindex)
{
	struct dentry *hidden_d_old = mtfs_d2branch(dentry, bindex);
	struct dentry *hidden_d_tmp = NULL;
	struct dentry *hidden_d_recover = NULL;
	struct dentry *hidden_d_root = NULL;
	struct dentry *hidden_d_new = NULL;
	struct mtfs_dentry_list *tmp_entry = NULL;
	MTFS_LIST_HEAD(dentry_list);
	int ret = 0;
	struct dentry *hidden_d_parent_new = NULL;
	struct dentry *hidden_d_parent_old = NULL;
	MENTRY();

	MASSERT(hidden_d_old);

	dget(hidden_d_old);
	MASSERT(hidden_d_old->d_inode);
	hidden_d_recover = mtfs_d_recover_branch(dentry, bindex);
	if (hidden_d_recover == NULL) {
		MERROR("failed to get d_recover for branch[%d]\n", bindex);
		ret = -ENOENT;
		goto out;
	}

	hidden_d_root = mtfs_d_root_branch(dentry, bindex);
	if (hidden_d_root == NULL) {
		MERROR("failed to get d_root for branch[%d]\n", bindex);
		ret = -ENOENT;
		goto out;
	}

	/* TODO: handle long path */
	for(hidden_d_tmp = dget_parent(hidden_d_old); ; hidden_d_tmp = dget_parent(hidden_d_tmp)) {
		if (hidden_d_tmp == hidden_d_root) {
			dput(hidden_d_tmp);
			break;
		}

		if (hidden_d_tmp == hidden_d_recover) {
			MERROR("[%.*s] of branch[%d] is already under recover directory, skipping\n",
			       dentry->d_name.len, dentry->d_name.name, bindex);
			dput(hidden_d_tmp);
			goto out_free_list;
		}

		if (hidden_d_tmp == hidden_d_tmp->d_parent) {
			MERROR("back to the d_root [%.*s] of branch[%d]\n",
			       hidden_d_tmp->d_name.len, hidden_d_tmp->d_name.name, bindex);
			dput(hidden_d_tmp);
			ret = -EINVAL;
			MBUG();
			goto out_free_list;
		}

		MTFS_ALLOC_PTR(tmp_entry);
		if (tmp_entry == NULL) {
			ret = -ENOMEM;
			dput(hidden_d_tmp);
			MBUG();
			goto out_free_list;
		}
		tmp_entry->dentry = hidden_d_tmp;
		mtfs_list_add(&tmp_entry->list, &dentry_list);
	}

	if (mtfs_list_empty(&dentry_list)) {
		hidden_d_parent_new = dget(hidden_d_recover);
	} else {
		hidden_d_parent_new = mtfs_dentry_list_mkpath(hidden_d_recover, &dentry_list);
		if (IS_ERR(hidden_d_parent_new)) {
			ret = PTR_ERR(hidden_d_parent_new);
			MBUG();
			goto out_free_list;
		}
	}

	/* Rename the conflicting dentry */
	hidden_d_new = mtfs_dchild_add_ino(hidden_d_parent_new,
	                                   hidden_d_old->d_name.name, hidden_d_old->d_name.len);
	if (IS_ERR(hidden_d_new)) {
		MERROR("failed to add info to [%.*s] of branch[%d]\n",
		       hidden_d_old->d_name.len, hidden_d_old->d_name.name, bindex);
		ret = PTR_ERR(hidden_d_new);
		goto out_dput;		
	}
	dput(hidden_d_new);

	/* Lookup the target dentry */
	mutex_lock(&hidden_d_parent_new->d_inode->i_mutex);
	hidden_d_new = lookup_one_len(hidden_d_old->d_name.name,
	                              hidden_d_parent_new, hidden_d_old->d_name.len);
	mutex_unlock(&hidden_d_parent_new->d_inode->i_mutex);
	if (IS_ERR(hidden_d_new)) {
		MERROR("lookup [%.*s/%.*s] failed, ret = %d\n",
		       hidden_d_parent_new->d_name.len, hidden_d_parent_new->d_name.name,
		       hidden_d_old->d_name.len, hidden_d_old->d_name.name, ret);
		ret = PTR_ERR(hidden_d_new);
		goto out_dput;
	}
	if (hidden_d_new->d_inode != NULL) {
		MERROR("[%.*s/%.*s] exists aready\n",
		       hidden_d_parent_new->d_name.len, hidden_d_parent_new->d_name.name,
		       hidden_d_old->d_name.len, hidden_d_old->d_name.name);
		ret = -EEXIST;
		goto out_dput_new;
	}


	hidden_d_parent_old = dget(hidden_d_old->d_parent);
	lock_rename(hidden_d_parent_old, hidden_d_parent_new);
	ret = vfs_rename(hidden_d_parent_old->d_inode, hidden_d_old,
	                 hidden_d_parent_new->d_inode, hidden_d_new);
	unlock_rename(hidden_d_parent_old, hidden_d_parent_new);
	if (ret) {
		MERROR("failed to rename [%.*s] to [%.*s] of branch[%d], ret = %d\n",
		       hidden_d_old->d_name.len, hidden_d_old->d_name.name,
		       hidden_d_new->d_name.len, hidden_d_new->d_name.name,
		       bindex, ret);
	}
	dput(hidden_d_parent_old);
out_dput_new:
	dput(hidden_d_new);
out_dput:
	dput(hidden_d_parent_new);
out_free_list:
	mtfs_dentry_list_cleanup(&dentry_list);
out:
	dput(hidden_d_old);
	MRETURN(ret);
}

struct dentry *mtfs_cleanup_branch(struct inode *dir, struct dentry *dentry, mtfs_bindex_t bindex)
{
	int ret = 0;
	struct dentry *hidden_dentry = mtfs_d2branch(dentry, bindex);
	MENTRY();

	MASSERT(inode_is_locked(dir));

	if (hidden_dentry != NULL) {
		ret = mtfs_backup_branch(dentry, bindex);
		if (ret) {
			MERROR("failed to backup branch[%d] of dentry [%.*s], ret = %d\n",
			       bindex, dentry->d_name.len, dentry->d_name.name, ret);
			hidden_dentry = ERR_PTR(ret);
			goto out;
		}

		dput(hidden_dentry);
	}

	mtfs_d2branch(dentry, bindex) = NULL;
	hidden_dentry = mtfs_lookup_branch(dentry, bindex);
	if (IS_ERR(hidden_dentry)) {
		MERROR("failed to lookup branch[%d] of dentry [%.*s], ret = %d\n",
		       bindex, dentry->d_name.len, dentry->d_name.name,
		       PTR_ERR(hidden_dentry));
		goto out;
	} else {
		mtfs_d2branch(dentry, bindex) = hidden_dentry;
	}
out:
	MRETURN(hidden_dentry);
}

int heal_discard_dentry_sync(struct inode *dir, struct dentry *dentry, struct mtfs_operation_list *list)
{
	int            ret = 0;
	mtfs_bindex_t  bindex = 0;
	mtfs_bindex_t  i = 0;
	struct dentry *hidden_dentry = NULL;
	MENTRY();

	MASSERT(inode_is_locked(dir));
	for (i = list->latest_bnum; i < list->bnum; i++) {
		if (list->op_binfo[i].is_suceessful) {
			bindex = list->op_binfo[i].bindex;
			hidden_dentry = mtfs_cleanup_branch(dir, dentry, bindex);
			if (IS_ERR(hidden_dentry)) {
				ret = PTR_ERR(hidden_dentry);
				MERROR("failed to cleanup branch[%d] of dentry [%.*s], ret = %d\n",
				       bindex, dentry->d_name.len, dentry->d_name.name, ret);
				goto out;
			}
			
		}
	}
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(heal_discard_dentry_sync);

int heal_discard_dentry_async(struct inode *dir, struct dentry *dentry, struct mtfs_operation_list *list)
{
	int ret = 0;
	struct mtfs_request *request = NULL;
	MENTRY();

	request = selfheal_request_pack();
	if (IS_ERR(request)) {
		MERROR("failed to alloc request\n");
		goto out;
	}
	selfheal_add_req(request, MDL_POLICY_ROUND, -1);

out:
	MRETURN(ret);
}
EXPORT_SYMBOL(heal_discard_dentry_async);

int heal_discard_dentry(struct inode *dir, struct dentry *dentry, struct mtfs_operation_list *list)
{
	int ret = 0;
	struct mtfs_operations *operations = NULL;
	MENTRY();

	MASSERT(inode_is_locked(dir));
	operations = mtfs_i2ops(dir);
	if (operations->heal_ops && operations->heal_ops->ho_discard_dentry) {
		ret = operations->heal_ops->ho_discard_dentry(dir, dentry, list);
	} else {
		MDEBUG("discard_dentry operation not supplied, use default\n");
		ret = heal_discard_dentry_sync(dir, dentry, list);
	}

	MRETURN(ret);
}
