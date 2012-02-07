/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */
#include <hrfs_list.h>
#include <hrfs_inode.h>
#include <hrfs_dentry.h>
#include <memory.h>
#include "hrfs_internal.h"

struct hrfs_dentry_list {
	hrfs_list_t list;
	struct dentry *dentry;
};

/*
 * Return d_child needed to be dput()
 */
struct dentry *hrfs_dchild_create(struct dentry *dparent, const char *name, umode_t mode, dev_t rdev)
{
	int ret = 0;
	struct dentry *dchild = NULL;
	HENTRY();

	HASSERT(dparent);
	HASSERT(name);
	HASSERT(dparent->d_inode);
	mutex_lock(&dparent->d_inode->i_mutex);

	dchild = lookup_one_len(name, dparent, strlen(name));
	if (IS_ERR(dchild)) {
		HERROR("lookup [%s] under [%*s] failed, ret = %d\n",
		       name, dparent->d_name.len, dparent->d_name.name, ret);
		ret = PTR_ERR(dchild);
		goto out;
	}

	if (dchild->d_inode) {
		if ((dchild->d_inode->i_mode & S_IFMT) == (mode & S_IFMT)) {
			HDEBUG("[%*s/%s] already existed\n",
			       dparent->d_name.len, dparent->d_name.name, name);
			if ((dchild->d_inode->i_mode & S_IALLUGO) != (mode & S_IALLUGO)) {
				HDEBUG("permission mode of [%*s/%s] is 0%04o which should be 0%04o for security\n",
				       dparent->d_name.len, dparent->d_name.name, name, dchild->d_inode->i_mode & S_IALLUGO, S_IRWXU);
			}
			goto out;
		} else {
			HDEBUG("[%*s/%s] already existed, and is a %s, not a %s\n",
			       dparent->d_name.len, dparent->d_name.name, name, hrfs_mode2type(dchild->d_inode->i_mode), hrfs_mode2type(mode));
			ret = -EEXIST;
			goto out_put;
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
		HDEBUG("bad file type 0%o\n", mode & S_IFMT);
		ret = -EINVAL;
	}
	if (ret) {
		goto out_put;
	} else {
		HASSERT(dchild->d_inode);
	}
	goto out;
out_put:
	dput(dchild);
out:
	mutex_unlock(&dparent->d_inode->i_mutex);
	if (ret) {
		dchild = ERR_PTR(ret);
	}
	HRETURN(dchild);
}

struct dentry *hrfs_dentry_list_mkpath(struct dentry *d_parent, hrfs_list_t *dentry_list)
{
	struct hrfs_dentry_list *tmp_entry = NULL;
	struct dentry *d_child = NULL;
	struct dentry *d_parent_tmp = d_parent;
	const char *name = NULL;
	HENTRY();

	hrfs_list_for_each_entry(tmp_entry, dentry_list, list) {
		name = tmp_entry->dentry->d_name.name;

		d_child = hrfs_dchild_create(d_parent_tmp, name, S_IFDIR | S_IRWXU, 0);
		if (d_parent_tmp != d_parent) {
			dput(d_parent_tmp);
		}	
		if (IS_ERR(d_child)) {
			HERROR("create [%*s/%s] failed\n",
			       d_parent_tmp->d_name.len, d_parent_tmp->d_name.name, name);
			goto out;
		}
		d_parent_tmp = d_child;
	}
out:
	if (d_child == NULL) {
		d_child = ERR_PTR(-ENOENT);
		HERROR("unexpected: dentry_list is empty\n");
		HBUG();
	}
	HRETURN(d_child);
}

static inline void hrfs_dentry_list_cleanup(hrfs_list_t *dentry_list)
{
	struct hrfs_dentry_list *tmp_entry = NULL;
	struct hrfs_dentry_list *n = NULL;

	hrfs_list_for_each_entry_safe(tmp_entry, n, dentry_list, list) {
		dput(tmp_entry->dentry);
		HRFS_FREE_PTR(tmp_entry);
	}
}

/*
 * needed to dput $dentry after being called.
 */
struct dentry *hrfs_dchild_remove(struct dentry *dparent, const char *name)
{
	int ret = 0;
	struct dentry *dchild = NULL;
	HENTRY();

	HASSERT(dparent);
	HASSERT(name);
	dget(dparent);
	mutex_lock(&dparent->d_inode->i_mutex);

	dchild = lookup_one_len(name, dparent, strlen(name));
	if (IS_ERR(dchild)) {
		HERROR("lookup [%s] under [%*s] failed, ret = %d\n",
		       name, dparent->d_name.len, dparent->d_name.name, ret);
		ret = PTR_ERR(dchild);
		goto out;
	}

	if (dchild->d_inode == NULL) {
		goto out;
	}

	HDEBUG("removing [%*s/%*s]\n",
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
		HDEBUG("vfs_unlink, ret = %d\n", ret);
		break;
	default:
		HERROR("bad file type 0%o\n", dchild->d_inode->i_mode & S_IFMT);
		ret = -EINVAL;
	}

	if (ret) {
		dput(dchild);
	} else {
		/* This is not true, but why? */
		//HASSERT(dchild->d_inode == NULL);
	}
	
out:
	mutex_unlock(&dparent->d_inode->i_mutex);
	dput(dparent);
	if (ret) {
		dchild = ERR_PTR(ret);
	}
	HRETURN(dchild);
}

struct dentry *hrfs_dchild_add_ino(struct dentry *dparent, const char *name)
{
	int ret = 0;
	struct dentry *dchild_old = NULL;
	struct dentry *dchild_new = NULL;
	char *name_new = NULL;
	HENTRY();

	HRFS_ALLOC(name_new, PATH_MAX);
	if (unlikely(name_new == NULL)) {
		ret = -ENOMEM;
		goto out;
	}

	HASSERT(dparent);
	HASSERT(name);
	dget(dparent);
	mutex_lock(&dparent->d_inode->i_mutex);

	dchild_old = lookup_one_len(name, dparent, strlen(name));
	if (IS_ERR(dchild_old)) {
		HERROR("lookup [%*s/%s] failed, ret = %d\n",
		       dparent->d_name.len, dparent->d_name.name, name, ret);
		ret = PTR_ERR(dchild_old);
		goto out_unlock;
	}

	if (dchild_old->d_inode == NULL) {
		dchild_new = dchild_old;
		goto out_unlock;
	}

	strcpy(name_new, name);
repeat:
	sprintf(name_new, "%s:%lx", name_new, dchild_old->d_inode->i_ino);

	dchild_new = lookup_one_len(name_new, dparent, strlen(name_new));
	if (IS_ERR(dchild_new)) {
		HERROR("lookup [%*s/%s] failed, ret = %d\n",
		       dparent->d_name.len, dparent->d_name.name, name_new, ret);
		ret = PTR_ERR(dchild_new);
		goto out_dput_old;
	}

	if (dchild_new->d_inode != NULL) {
		dput(dchild_new);
		if (strlen(name_new) + 16 >= PATH_MAX) {
			ret = -EEXIST;
			goto out_dput_old;
		}
		goto repeat;
	}

	HDEBUG("renaming [%*s/%*s] to [%*s/%*s]\n",
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
	}

out_dput_old:
	dput(dchild_old);
out_unlock:
	mutex_unlock(&dparent->d_inode->i_mutex);
	dput(dparent);
	HRFS_FREE(name_new, PATH_MAX);
out:
	if (ret) {
		dchild_new = ERR_PTR(ret);
	}
	HRETURN(dchild_new);
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
struct dentry *hrfs_lock_rename(struct dentry *p1, struct dentry *p2)
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

void hrfs_unlock_rename(struct dentry *p1, struct dentry *p2, int p1_locked, int p2_locked)
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

int hrfs_backup_branch(struct dentry *dentry, hrfs_bindex_t bindex)
{
	struct dentry *hidden_d_old = hrfs_d2branch(dentry, bindex);
	struct dentry *hidden_d_tmp = NULL;
	struct dentry *hidden_d_recover = NULL;
	struct dentry *hidden_d_root = NULL;
	struct dentry *hidden_d_new = NULL;
	struct hrfs_dentry_list *tmp_entry = NULL;
	HRFS_LIST_HEAD(dentry_list);
	struct hrfs_dentry_list *n = NULL;
	int ret = 0;
	struct dentry *hidden_d_parent_new = NULL;
	struct dentry *hidden_d_parent_old = NULL;
	int old_locked = 0;
	int new_locked = 0;
	HENTRY();

	HASSERT(hidden_d_old);

	dget(hidden_d_old);
	hidden_d_recover = hrfs_d_recover_branch(dentry, bindex);
	if (hidden_d_recover == NULL) {
		HERROR("failed to get d_recover for branch[%d]\n", bindex);
		ret = -ENOENT;
		goto out;
	}

	hidden_d_root = hrfs_d_root_branch(dentry, bindex);
	if (hidden_d_root == NULL) {
		HERROR("failed to get d_root for branch[%d]\n", bindex);
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
			HERROR("[%*s] is already under recover directory, skipping\n",
			       dentry->d_name.len, dentry->d_name.name);
			dput(hidden_d_tmp);
			goto out_free_list;
		}

		if (hidden_d_tmp == hidden_d_tmp->d_parent) {
			HERROR("back to the d_root [%*s] of branch[%d]\n",
			       hidden_d_tmp->d_name.len, hidden_d_tmp->d_name.name, bindex);
			dput(hidden_d_tmp);
			HBUG();
			goto out_free_list;
		}

		HRFS_ALLOC_PTR(tmp_entry);
		if (tmp_entry == NULL) {
			ret = -ENOMEM;
			dput(hidden_d_tmp);
			HBUG();
			goto out_free_list;
		}
		tmp_entry->dentry = hidden_d_tmp;
		hrfs_list_add(&tmp_entry->list, &dentry_list);
	}

	if (hrfs_list_empty(&dentry_list)) {
		hidden_d_parent_new = dget(hidden_d_root);
	} else {
		hidden_d_parent_new = hrfs_dentry_list_mkpath(hidden_d_recover, &dentry_list);
		if (IS_ERR(hidden_d_parent_new)) {
			HBUG();
			goto out_free_list;
		}
	}

#ifndef LIXI_20120111
	hidden_d_new = hrfs_dchild_add_ino(hidden_d_parent_new, hidden_d_old->d_name.name);
#else
	hidden_d_new = hrfs_dchild_remove(hidden_d_parent_new, hidden_d_old->d_name.name);
#endif
	if (IS_ERR(hidden_d_new)) {
		goto out_dput;		
	}

	hidden_d_parent_old = hidden_d_old->d_parent;
	old_locked = inode_is_locked(hidden_d_parent_old->d_inode);
	new_locked = inode_is_locked(hidden_d_parent_new->d_inode);
	hrfs_lock_rename(hidden_d_parent_old, hidden_d_parent_new);
	ret = vfs_rename(hidden_d_parent_old->d_inode, hidden_d_old,
	                 hidden_d_parent_new->d_inode, hidden_d_new);
	hrfs_unlock_rename(hidden_d_parent_old, hidden_d_parent_new, old_locked, new_locked);
	dput(hidden_d_new);
out_dput:
	dput(hidden_d_parent_new);
out_free_list:
	hrfs_list_for_each_entry_safe(tmp_entry, n, &dentry_list, list) {
		HDEBUG("branch[%d]: dentry [%*s]\n", bindex,
		       tmp_entry->dentry->d_name.len, tmp_entry->dentry->d_name.name);
	}
	hrfs_dentry_list_cleanup(&dentry_list);
out:
	dput(hidden_d_old);
	HRETURN(ret);
}

struct dentry *hrfs_cleanup_branch(struct dentry *dentry, hrfs_bindex_t bindex)
{
	int ret = 0;
	struct dentry *hidden_dentry = hrfs_d2branch(dentry, bindex);
	HENTRY();

	if (hidden_dentry != NULL) {
		ret = hrfs_backup_branch(dentry, bindex);
		if (ret) {
			HERROR("failed to backup branch[%d] of dentry [%*s], ret = %d\n",
			       bindex, dentry->d_name.len, dentry->d_name.name, ret);
			hidden_dentry = ERR_PTR(ret);
			goto out;
		}
	
		dput(hidden_dentry);
	}

	hrfs_d2branch(dentry, bindex) = NULL;
	hidden_dentry = hrfs_lookup_branch(dentry, bindex);
	if (IS_ERR(hidden_dentry)) {
		goto out;
	} else {
		hrfs_d2branch(dentry, bindex) = hidden_dentry;
	}
out:
	HRETURN(hidden_dentry);
}

int hrfs_lookup_discard_dentry(struct dentry *dentry, struct hrfs_operation_list *list)
{
	int ret = 0;
	hrfs_bindex_t bindex = 0;
	hrfs_bindex_t i = 0;
	struct dentry *hidden_dentry = NULL;
	HENTRY();

	for (i = list->latest_bnum; i < list->bnum; i++) {
		bindex = list->op_binfo[i].bindex;
		if (list->op_binfo[i].is_suceessful) {
			hidden_dentry = hrfs_cleanup_branch(dentry, bindex);
			if (IS_ERR(hidden_dentry)) {
				ret = PTR_ERR(hidden_dentry);
				HERROR("failed to cleanup branch[%d] of dentry [%*s], ret = %d\n",
				       bindex, dentry->d_name.len, dentry->d_name.name, ret);
				goto out;
			}
		}
	}
out:
	HRETURN(ret);
}
