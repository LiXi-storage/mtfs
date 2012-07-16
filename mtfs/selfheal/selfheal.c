/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <mtfs_list.h>
#include <mtfs_inode.h>
#include <mtfs_dentry.h>
#include <memory.h>
#include <thread.h>
#include <bitmap.h>
#include "selfheal_internal.h"
#include "thread_internal.h"

struct mtfs_dentry_list {
	mtfs_list_t list;
	struct dentry *dentry;
};

/*
 * Return d_child needed to be dput() if succeed
 */
struct dentry *_mtfs_dchild_create(struct dentry *dparent, const unsigned char *name,
                                  unsigned int len, umode_t mode, dev_t rdev, int rename)
{
	int ret = 0;
	struct dentry *dchild = NULL;
	HENTRY();

	HASSERT(inode_is_locked(dparent->d_inode));

	dchild = lookup_one_len(name, dparent, len);
	if (IS_ERR(dchild)) {
		HERROR("lookup [%s] under [%*s] failed, ret = %d\n",
		       name, dparent->d_name.len, dparent->d_name.name, ret);
		ret = PTR_ERR(dchild);
		goto out;
	}

	if (dchild->d_inode) {
		if ((dchild->d_inode->i_mode & S_IFMT) == (mode & S_IFMT)) {
			HDEBUG("[%*s/%*s] already existed\n",
			       dparent->d_name.len, dparent->d_name.name, len, name);
			if ((dchild->d_inode->i_mode & S_IALLUGO) != (mode & S_IALLUGO)) {
				HDEBUG("permission mode of [%*s/%*s] is 0%04o, "
				       "which should be 0%04o for security\n",
				       dparent->d_name.len, dparent->d_name.name, len, name,
				       dchild->d_inode->i_mode & S_IALLUGO, S_IRWXU);
			}
			goto out;
		} else {
			HDEBUG("[%*s/%*s] already existed, and is a %s, not a %s\n",
			       dparent->d_name.len, dparent->d_name.name, len, name,
			       mtfs_mode2type(dchild->d_inode->i_mode), mtfs_mode2type(mode));
			if (rename) {
				struct dentry *dchild_new = NULL;
				HDEBUG("Trying to rename [%*s/%*s]\n",
				       dparent->d_name.len, dparent->d_name.name, len, name);
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
	if (ret) {
		dchild = ERR_PTR(ret);
	}
	HRETURN(dchild);
}

/*
 * Return d_child needed to be dput() if succeed
 */
struct dentry *mtfs_dchild_create(struct dentry *dparent, const unsigned char *name,
                                  unsigned int len, umode_t mode, dev_t rdev, int repeat)
{
	struct dentry *dchild = NULL;
	HENTRY();

	HASSERT(dparent);
	HASSERT(name);
	HASSERT(dparent->d_inode);
	mutex_lock(&dparent->d_inode->i_mutex);
	dchild = _mtfs_dchild_create(dparent, name, len, mode, rdev, repeat);
	if (IS_ERR(dchild)) {
		if ((PTR_ERR(dchild) == -EAGAIN) && repeat) {
			dchild = _mtfs_dchild_create(dparent, name, len, mode, rdev, 0);
		}
	}
	mutex_unlock(&dparent->d_inode->i_mutex);
	HRETURN(dchild);
}
EXPORT_SYMBOL(mtfs_dchild_create);

struct dentry *mtfs_dentry_list_mkpath(struct dentry *d_parent, mtfs_list_t *dentry_list)
{
	struct mtfs_dentry_list *tmp_entry = NULL;
	struct dentry *d_child = NULL;
	struct dentry *d_parent_tmp = d_parent;
	HENTRY();

	mtfs_list_for_each_entry(tmp_entry, dentry_list, list) {
		d_child = mtfs_dchild_create(d_parent_tmp, tmp_entry->dentry->d_name.name,
		                             tmp_entry->dentry->d_name.len, S_IFDIR | S_IRWXU, 0, 1);
		if (d_parent_tmp != d_parent) {
			dput(d_parent_tmp);
		}	
		if (IS_ERR(d_child)) {
			HERROR("create [%*s/%*s] failed\n",
			       d_parent_tmp->d_name.len, d_parent_tmp->d_name.name,
			       tmp_entry->dentry->d_name.len, tmp_entry->dentry->d_name.name);
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
		dput(dchild);
		ret = -ENOENT;
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
EXPORT_SYMBOL(mtfs_dchild_remove);

/* If succeed, return dchild_new which needed to be dput */
struct dentry *mtfs_dchild_rename2new(struct dentry *dparent, struct dentry *dchild_old,
                                      const unsigned char *name_new, unsigned int len)
{
	struct dentry *dchild_new = NULL;
	int ret = 0;
	HENTRY();

	HASSERT(inode_is_locked(dparent->d_inode));
	dchild_new = lookup_one_len(name_new, dparent, len);
	if (IS_ERR(dchild_new)) {
		HERROR("lookup [%*s/%s] failed, ret = %d\n",
		       dparent->d_name.len, dparent->d_name.name, name_new, ret);
		goto out;
	}

	if (dchild_new->d_inode != NULL) {
		dput(dchild_new);
		dchild_new = ERR_PTR(-EEXIST);
		goto out;
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
		dchild_new = ERR_PTR(ret);
	}

out:
	HRETURN(dchild_new);
}
EXPORT_SYMBOL(mtfs_dchild_rename2new);

struct dentry *_mtfs_dchild_add_ino(struct dentry *dparent, struct dentry *dchild_old)
{
	char *name_new = NULL;
	struct dentry *dchild_new = NULL;
	HENTRY();

	HASSERT(dparent);
	HASSERT(dparent->d_inode);
	HASSERT(inode_is_locked(dparent->d_inode));
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
	HRETURN(dchild_new);
}
EXPORT_SYMBOL(_mtfs_dchild_add_ino);

struct dentry *mtfs_dchild_add_ino(struct dentry *dparent, const unsigned char *name, unsigned int len)
{
	int ret = 0;
	struct dentry *dchild_old = NULL;
	struct dentry *dchild_new = NULL;
	HENTRY();

	HASSERT(dparent);
	HASSERT(dparent->d_inode);
	HASSERT(name);
	dget(dparent);
	mutex_lock(&dparent->d_inode->i_mutex);

	dchild_old = lookup_one_len(name, dparent, len);
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

	dchild_new = _mtfs_dchild_add_ino(dparent, dchild_old);

	dput(dchild_old);
out_unlock:
	mutex_unlock(&dparent->d_inode->i_mutex);
	dput(dparent);

	if (ret) {
		dchild_new = ERR_PTR(ret);
	}
	HRETURN(dchild_new);
}
EXPORT_SYMBOL(mtfs_dchild_add_ino);

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
	struct mtfs_dentry_list *n = NULL;
	int ret = 0;
	struct dentry *hidden_d_parent_new = NULL;
	struct dentry *hidden_d_parent_old = NULL;
	HENTRY();

	HASSERT(hidden_d_old);

	dget(hidden_d_old);
	hidden_d_recover = mtfs_d_recover_branch(dentry, bindex);
	if (hidden_d_recover == NULL) {
		HERROR("failed to get d_recover for branch[%d]\n", bindex);
		ret = -ENOENT;
		goto out;
	}

	hidden_d_root = mtfs_d_root_branch(dentry, bindex);
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

		MTFS_ALLOC_PTR(tmp_entry);
		if (tmp_entry == NULL) {
			ret = -ENOMEM;
			dput(hidden_d_tmp);
			HBUG();
			goto out_free_list;
		}
		tmp_entry->dentry = hidden_d_tmp;
		mtfs_list_add(&tmp_entry->list, &dentry_list);
	}

	if (mtfs_list_empty(&dentry_list)) {
		hidden_d_parent_new = dget(hidden_d_root);
	} else {
		hidden_d_parent_new = mtfs_dentry_list_mkpath(hidden_d_recover, &dentry_list);
		if (IS_ERR(hidden_d_parent_new)) {
			HBUG();
			goto out_free_list;
		}
	}

	hidden_d_new = mtfs_dchild_add_ino(hidden_d_parent_new,
	                                   hidden_d_old->d_name.name, hidden_d_old->d_name.len);
	if (IS_ERR(hidden_d_new)) {
		goto out_dput;		
	}

	hidden_d_parent_old = dget(hidden_d_old->d_parent);
	lock_rename(hidden_d_parent_old, hidden_d_parent_new);
	ret = vfs_rename(hidden_d_parent_old->d_inode, hidden_d_old,
	                 hidden_d_parent_new->d_inode, hidden_d_new);
	unlock_rename(hidden_d_parent_old, hidden_d_parent_new);
	dput(hidden_d_parent_old);
	dput(hidden_d_new);

out_dput:
	dput(hidden_d_parent_new);
out_free_list:
	mtfs_list_for_each_entry_safe(tmp_entry, n, &dentry_list, list) {
		HDEBUG("branch[%d]: dentry [%*s]\n", bindex,
		       tmp_entry->dentry->d_name.len, tmp_entry->dentry->d_name.name);
	}
	mtfs_dentry_list_cleanup(&dentry_list);
out:
	dput(hidden_d_old);
	HRETURN(ret);
}

struct dentry *mtfs_cleanup_branch(struct inode *dir, struct dentry *dentry, mtfs_bindex_t bindex)
{
	int ret = 0;
	struct dentry *hidden_dentry = mtfs_d2branch(dentry, bindex);
	struct dentry *hidden_dir_dentry = mtfs_d2branch(dentry->d_parent, bindex);
	HENTRY();

	HASSERT(inode_is_locked(dir));

	if (hidden_dentry != NULL) {
		ret = mtfs_backup_branch(dentry, bindex);
		if (ret) {
			HERROR("failed to backup branch[%d] of dentry [%*s], ret = %d\n",
			       bindex, dentry->d_name.len, dentry->d_name.name, ret);
			hidden_dentry = ERR_PTR(ret);
			goto out;
		}

		dput(hidden_dentry);
	}

	mtfs_d2branch(dentry, bindex) = NULL;

	mutex_lock(&hidden_dir_dentry->d_inode->i_mutex);
	hidden_dentry = lookup_one_len(dentry->d_name.name, hidden_dir_dentry, dentry->d_name.len);
	mutex_unlock(&hidden_dir_dentry->d_inode->i_mutex);
	if (IS_ERR(hidden_dentry)) {
		goto out;
	} else {
		mtfs_d2branch(dentry, bindex) = hidden_dentry;
	}
out:
	HRETURN(hidden_dentry);
}

int mtfs_lookup_discard_dentry(struct inode *dir, struct dentry *dentry, struct mtfs_operation_list *list)
{
	int ret = 0;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t i = 0;
	struct dentry *hidden_dentry = NULL;
	HENTRY();

#ifndef LIXI_20120715
	{
		struct mtfs_request *request = NULL;
		HERROR("in mtfs_lookup_discard_dentry\n");
		request = selfheal_request_pack();
		if (IS_ERR(request)) {
			HERROR("failed to alloc request\n");
			goto out;
		}
		selfheal_add_req(request, MDL_POLICY_ROUND, -1);
	}
#endif

	HASSERT(inode_is_locked(dir));
	for (i = list->latest_bnum; i < list->bnum; i++) {
		if (list->op_binfo[i].is_suceessful) {
			bindex = list->op_binfo[i].bindex;
			hidden_dentry = mtfs_cleanup_branch(dir, dentry, bindex);
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
EXPORT_SYMBOL(mtfs_lookup_discard_dentry);

struct mtfs_request *mtfs_request_alloc(void)
{
	struct mtfs_request *request = NULL;
	HENTRY();

	/* TODO: pool */
	MTFS_ALLOC_PTR(request);
	if (request == NULL) {
		HERROR("request allocation out of memory\n");
		goto out;
	}
out:
        HRETURN(request);
}

void mtfs_request_free(struct mtfs_request *request)
{
	HENTRY();
	MTFS_FREE_PTR(request);
	_HRETURN();
}

/* Return 1 if freed */
static int selfheal_req_finished(struct mtfs_request *request)
{
	int ret = 0;
	HENTRY();

	if (atomic_dec_and_test(&request->rq_refcount)) {
		mtfs_request_free(request);
		ret = 1;
	}

	HRETURN(ret);
}

struct mtfs_request *selfheal_request_pack(void)
{
	struct mtfs_request *request = NULL;
	int ret = 0;
	HENTRY();

	request = mtfs_request_alloc();
	if (request == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	request->rq_phase = RQ_PHASE_NEW;
	MTFS_INIT_LIST_HEAD(&request->rq_set_chain);
	atomic_set(&request->rq_refcount, 1);
out:
	if (ret) {
		request = ERR_PTR(ret);
	}
	HRETURN(request);
}	

static int mtfs_req_interpret(struct mtfs_request *req)
{
	int ret = 0;
	HENTRY();

	HERROR("interpreting req: %p\n", req);
	HRETURN(ret);
}

static int mtfs_reqphase_move(struct mtfs_request *req, enum rq_phase new_phase)
{
	int ret = 0;
	HENTRY();

	if (req->rq_phase == new_phase) {
		goto out;
	}

	req->rq_phase = new_phase;
out:
	HRETURN(ret);
}

/*
 * If set is emptied return 1
 */
static int selfheal_check_set(struct selfheal_request_set *set)
{
	int ret = 0;
	mtfs_list_t *tmp = NULL;
	struct mtfs_request *req = NULL;
	HENTRY();

	if (atomic_read(&set->set_remaining) == 0) {
		ret = 1;
		goto out;
	}

	mtfs_list_for_each(tmp, &set->set_requests) {
		req = mtfs_list_entry(tmp,
		                      struct mtfs_request,
		                      rq_set_chain);

		mtfs_req_interpret(req);
		mtfs_reqphase_move(req, RQ_PHASE_COMPLETE);
                atomic_dec(&set->set_remaining);
	}
	ret = atomic_read(&set->set_remaining) == 0;
out:
	HRETURN(ret);
}


static int selfheal_check(struct selfheal_daemon_ctl *sc)
{
	int ret = 0;
	struct selfheal_request_set *set = sc->sc_set;
	mtfs_list_t *tmp = NULL;
	mtfs_list_t *pos = NULL;
	struct mtfs_request *req = NULL;
	HENTRY();

	ret = mtfs_test_bit(MTFS_DAEMON_STOP, &sc->sc_flags);
	if (ret) {
		goto out;
	}

	if (atomic_read(&set->set_new_count)) {
		spin_lock(&set->set_new_req_lock);
		{
			if (likely(!mtfs_list_empty(&set->set_new_requests))) {
				mtfs_list_splice_init(&set->set_new_requests,
				                      &set->set_requests);
				atomic_add(atomic_read(&set->set_new_count),
				               &set->set_remaining);
				atomic_set(&set->set_new_count, 0);
			}
			ret = 1;
		}
		spin_unlock(&set->set_new_req_lock);
	}

	if (atomic_read(&set->set_remaining)) {
		ret |= selfheal_check_set(set);
	}

	if (!mtfs_list_empty(&set->set_requests)) {
		/*
		 * prune the completed
		 * reqs after each iteration
		 */
		mtfs_list_for_each_safe(pos, tmp, &set->set_requests) {
			req = mtfs_list_entry(pos, struct mtfs_request,
			                      rq_set_chain);
			if (req->rq_phase != RQ_PHASE_COMPLETE)
				continue;

			mtfs_list_del_init(&req->rq_set_chain);
			req->rq_set = NULL;
			selfheal_req_finished(req);
                }
	}

	if (ret == 0) {
		/*
		 * If new requests have been added, make sure to wake up.
		 */
		ret = atomic_read(&set->set_new_count);
		if (ret == 0) {
			/*
			 * If we have nothing to do, check whether we can take some
			 * work from our partner threads.
			 */
		}
	}

out:
	HRETURN(ret);
}

static int selfheal_max_nthreads = 0;
module_param(selfheal_max_nthreads, int, 0644);
MODULE_PARM_DESC(selfheal_max_nthreads, "Max selfheal thread count to be started.");
EXPORT_SYMBOL(selfheal_max_nthreads);
static struct selfheal_daemon *selfheal_daemons;

static struct selfheal_daemon_ctl*
selfheal_select_pc(struct mtfs_request *req, mdl_policy_t policy, int index)
{
	int idx = 0;
	HENTRY();

	HASSERT(req != NULL);

	switch (policy) {
	case MDL_POLICY_SAME:
		idx = smp_processor_id() % selfheal_daemons->sd_nthreads;
		break;
	case MDL_POLICY_LOCAL:
		/* Fix me */
		index = -1;
	case MDL_POLICY_PREFERRED:
		if (index >= 0 && index < num_online_cpus()) {
			idx = index % selfheal_daemons->sd_nthreads;
		}
		/* Fall through to PDL_POLICY_ROUND for bad index. */
	case MDL_POLICY_ROUND:
	default:
		/* We do not care whether it is strict load balance. */
		idx = selfheal_daemons->sd_index + 1;
		if (idx == smp_processor_id()) {
			idx++;
		}
		idx %= selfheal_daemons->sd_nthreads;
		selfheal_daemons->sd_index = idx;
		break;
	}
	HRETURN(&selfheal_daemons->sd_threads[idx]);
}

int selfheal_set_add_new_req(struct selfheal_daemon_ctl *sc,
                           struct mtfs_request *req)
{
	struct selfheal_request_set *set = sc->sc_set;
	int ret = 0;
	int count = 0;
	HENTRY();

	HASSERT(req->rq_set == NULL);
	HASSERT(mtfs_test_bit(MTFS_DAEMON_STOP, &sc->sc_flags) == 0);
	spin_lock(&set->set_new_req_lock);
	{
		req->rq_set = set;
		mtfs_list_add_tail(&req->rq_set_chain, &set->set_new_requests);
		count = atomic_inc_return(&set->set_new_count);
	}
	spin_unlock(&set->set_new_req_lock);

	/* Only need to call wakeup once for the first entry. */
	if (count == 1) {
		wake_up(&set->set_waitq);
		/* TODO: partners */
	}
	HRETURN(ret);
}

int selfheal_add_req(struct mtfs_request *req, mdl_policy_t policy, int idx)
{
	struct selfheal_daemon_ctl *sc = NULL;
	int ret = 0;
	HENTRY();

	HERROR("adding request: %p\n", req);
	sc = selfheal_select_pc(req, policy, idx);
	selfheal_set_add_new_req(sc, req);
	HRETURN(ret);
}

#define MTFS_SELFHEAL_TIMEOUT 10
static int selfheal_daemon_main(void *arg)
{
	int ret = 0;
	struct selfheal_daemon_ctl *sc = arg;
	struct selfheal_request_set *set = sc->sc_set;
	int exit = 0;
	HENTRY();

	mtfs_daemonize_ctxt(sc->sc_name);
#if defined(CONFIG_SMP)
	if (mtfs_test_bit(MTFS_DAEMON_BIND, &sc->sc_flags)) {
		int index = sc->sc_index;

		HASSERT(index  >= 0 && index < num_possible_cpus());
		while (!cpu_online(index)) {
			if (++index >= num_possible_cpus()) {
				index = 0;
			}
		}
		set_cpus_allowed(current,
                                 node_to_cpumask(cpu_to_node(index)));
	}
#endif

	complete(&sc->sc_starting);
	do {
		struct l_wait_info lwi;
		int timeout;

		timeout = MTFS_SELFHEAL_TIMEOUT;
		lwi = LWI_TIMEOUT_INTR_ALL(timeout * HZ, NULL, NULL, NULL);

		l_wait_event(set->set_waitq, selfheal_check(sc), &lwi);
		if (mtfs_test_bit(MTFS_DAEMON_STOP, &sc->sc_flags)) {
			if (mtfs_test_bit(MTFS_DAEMON_FORCE, &sc->sc_flags)) {
				//abort;
			}
			exit++;
		}
		HDEBUG("selfheal daemon loop again\n");
	} while (!exit);

	complete(&sc->sc_finishing);
	mtfs_clear_bit(MTFS_DAEMON_START, &sc->sc_flags);
	mtfs_clear_bit(MTFS_DAEMON_STOP, &sc->sc_flags);
	mtfs_clear_bit(MTFS_DAEMON_FORCE, &sc->sc_flags);
	mtfs_clear_bit(MTFS_DAEMON_BIND, &sc->sc_flags);
	HRETURN(ret);
}

struct selfheal_request_set *selfheal_prep_set(void)
{
	struct selfheal_request_set *set = NULL;
	HENTRY();

	MTFS_ALLOC_PTR(set);
	if (set == NULL) {
		goto out;
	}

	atomic_set(&set->set_refcount, 1);
	MTFS_INIT_LIST_HEAD(&set->set_requests);
	init_waitqueue_head(&set->set_waitq);
	atomic_set(&set->set_new_count, 0);
	atomic_set(&set->set_remaining, 0);
	spin_lock_init(&set->set_new_req_lock);
	MTFS_INIT_LIST_HEAD(&set->set_new_requests);

out:
	HRETURN(set);
}

void selfheal_put_get(struct selfheal_request_set *set)
{
	atomic_inc(&set->set_refcount);	
}

void selfheal_put_set(struct selfheal_request_set *set)
{
	if (atomic_dec_and_test(&set->set_refcount)) {
		MTFS_FREE_PTR(set);
	}	
}

void selfheal_set_destroy(struct selfheal_request_set *set)
{
	selfheal_put_set(set);
}

int selfheal_daemon_start(int index, const char *name, int no_bind, struct selfheal_daemon_ctl *sc)
{
	int ret = 0;
	struct selfheal_request_set *set = NULL;
	HENTRY();

	if (mtfs_test_and_set_bit(MTFS_DAEMON_START, &sc->sc_flags)) {
		HERROR("thread [%s] is already started\n",
                       name);
		goto out;
        }

	sc->sc_index = index;
	init_completion(&sc->sc_starting);
	init_completion(&sc->sc_finishing);
	spin_lock_init(&sc->sc_lock);
	strncpy(sc->sc_name, name, sizeof(sc->sc_name) - 1);

	sc->sc_set = selfheal_prep_set();
	if (sc->sc_set == NULL) {
		ret = -ENOMEM;
		goto out;
	}	
	
	if (!no_bind) {
		mtfs_set_bit(MTFS_DAEMON_BIND, &sc->sc_flags);
	}

	ret = mtfs_create_thread(selfheal_daemon_main, sc, 0);
	if (ret < 0) {
		HERROR("failed to create thread [%s]", name);
		goto out_destroy_set;
	}
	ret = 0;

	wait_for_completion(&sc->sc_starting);

	goto out;
out_destroy_set:
	set = sc->sc_set;
	spin_lock(&sc->sc_lock);
	sc->sc_set = NULL;
	spin_unlock(&sc->sc_lock);
	selfheal_set_destroy(set);

	mtfs_clear_bit(MTFS_DAEMON_BIND, &sc->sc_flags);
	mtfs_clear_bit(MTFS_DAEMON_START, &sc->sc_flags);
out:
	HRETURN(ret);
}

void selfheal_daemon_stop(struct selfheal_daemon_ctl *sc, int force)
{
	struct selfheal_request_set *set = NULL;
	HENTRY();

	HERROR("Stoping\n");
	if (!mtfs_test_bit(MTFS_DAEMON_START, &sc->sc_flags)) {
                HERROR("Thread for pc %p was not started\n", sc);
		HBUG();
                goto out;
        }

        mtfs_set_bit(MTFS_DAEMON_STOP, &sc->sc_flags);
	if (force) {
		mtfs_set_bit(MTFS_DAEMON_FORCE, &sc->sc_flags);
	}

	wake_up(&sc->sc_set->set_waitq);
	wait_for_completion(&sc->sc_finishing);

	set = sc->sc_set;
	spin_lock(&sc->sc_lock);
	sc->sc_set = NULL;
	spin_unlock(&sc->sc_lock);
	selfheal_set_destroy(set);

out:
	_HRETURN();
}

void selfheal_daemon_fini(void)
{
	int i = 0;
	HENTRY();

	if (selfheal_daemons == NULL) {
		HERROR("selfheal daemons is not inited yet\n");
		HBUG();
		goto out;
	}

	for (i = 0; i < selfheal_daemons->sd_nthreads; i++) {
		selfheal_daemon_stop(&selfheal_daemons->sd_threads[i], 0);
	}
	MTFS_FREE(selfheal_daemons, selfheal_daemons->sd_size);
	selfheal_daemons = NULL;

out:
	_HRETURN();
}

int selfheal_daemon_init(void)
{
	int nthreads = num_online_cpus();
	char name[MAX_SELFHEAL_NAME_LENGTH];
	int size = 0;
	int ret = 0;
	int i = 0;
	HENTRY();

	if (selfheal_max_nthreads > 0 && selfheal_max_nthreads < nthreads) {
                nthreads = selfheal_max_nthreads;
	}
#ifndef LIXI_20120706
	nthreads = 1;
#endif

	size = offsetof(struct selfheal_daemon, sd_threads[nthreads]);
	MTFS_ALLOC(selfheal_daemons, size);
	if (selfheal_daemons == NULL) {
		HERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < nthreads; i++) {
		snprintf(name, MAX_SELFHEAL_NAME_LENGTH - 1, "selfheal_%d", i);
		ret = selfheal_daemon_start(i, name, 0,
		                            &selfheal_daemons->sd_threads[i]);
		if (ret) {
			HERROR("failed to start selfheal daemon[%d]\n", i);
			i--;
			goto out_stop;
		}
	}

	selfheal_daemons->sd_size = size;
	selfheal_daemons->sd_index = 0;
	selfheal_daemons->sd_nthreads = nthreads;
	goto out;
out_stop:
	for (; i >= 0; i--) {
		selfheal_daemon_stop(&selfheal_daemons->sd_threads[i], 0);
	}
	MTFS_FREE(selfheal_daemons, size);
	selfheal_daemons = NULL;
out:
	HRETURN(ret);
}

static int __init mtfs_selfheal_init(void)
{
	int ret = 0;
	ret = selfheal_daemon_init();
	return ret;
}

static void __exit mtfs_selfheal_exit(void)
{
	selfheal_daemon_fini();
}

MODULE_AUTHOR("MulTi File System Development Workgroup");
MODULE_DESCRIPTION("mtfs_selfheal");
MODULE_LICENSE("GPL");

module_init(mtfs_selfheal_init)
module_exit(mtfs_selfheal_exit)

