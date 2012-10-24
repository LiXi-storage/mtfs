/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/sched.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/fs.h>
#include <debug.h>
#include <mtfs_record.h>
#include "heal_internal.h"
#include "file_internal.h"

int mrecord_init(struct mrecord_handle *handle)
{
	int ret = 0;
	MENTRY();

	MASSERT(handle->mrh_ops);
	if (handle->mrh_ops->mro_init) {
		ret = handle->mrh_ops->mro_init(handle);
	}

	MRETURN(ret);
}
EXPORT_SYMBOL(mrecord_init);

int mrecord_fini(struct mrecord_handle *handle)
{
	int ret = 0;
	MENTRY();

	MASSERT(handle->mrh_ops);
	if (handle->mrh_ops->mro_fini) {
		ret = handle->mrh_ops->mro_fini(handle);
	}

	MRETURN(ret);
}
EXPORT_SYMBOL(mrecord_fini);

int mrecord_cleanup(struct mrecord_handle *handle)
{
	int ret = 0;
	MENTRY();

	MASSERT(handle->mrh_ops);
	if (handle->mrh_ops->mro_cleanup) {
		ret = handle->mrh_ops->mro_cleanup(handle);
	}

	MRETURN(ret);
}
EXPORT_SYMBOL(mrecord_cleanup);

int mrecord_add(struct mrecord_handle *handle, struct mrecord_head *head)
{
	int ret = 0;
	MENTRY();

	MASSERT(handle->mrh_ops);
	if (handle->mrh_ops->mro_add) {
		head->mrh_sequence = handle->mrh_prev_sequence + 1;
		ret = handle->mrh_ops->mro_add(handle, head);
		if (ret) {
			MERROR("failed to add record, ret = %d\n", ret);
			goto out;
		}
	}
	handle->mrh_prev_sequence++;
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mrecord_add);

static int mrecord_file_init(struct mrecord_handle *handle)
{
	int ret = 0;
	struct mrecord_file_info *mrh_file = &handle->u.mrh_file;
	MENTRY();

	MASSERT(mrh_file->mrfi_fname);
	MASSERT(mrh_file->mrfi_dparent);
	MASSERT(mrh_file->mrfi_mnt);
	mrh_file->mrfi_dchild = mtfs_dchild_create(mrh_file->mrfi_dparent,
	                                           mrh_file->mrfi_fname,
	                                           strlen(mrh_file->mrfi_fname),
	                                           S_IFREG | S_IRWXU, 0, 0);
	if (IS_ERR(mrh_file->mrfi_dchild)) {
		ret = PTR_ERR(mrh_file->mrfi_dchild);
		MERROR("failed to  create [%.*s/%s], ret = %d\n",
		       mrh_file->mrfi_dparent->d_name.len,
		       mrh_file->mrfi_dparent->d_name.name,
		       mrh_file->mrfi_fname, ret);
		goto out;
	}

	mrh_file->mrfi_filp = mtfs_dentry_open(dget(mrh_file->mrfi_dchild),
	                                       mntget(mrh_file->mrfi_mnt),
	                                       O_RDWR,
	                                       current_cred());
	if (IS_ERR(mrh_file->mrfi_filp)) {
		ret = PTR_ERR(mrh_file->mrfi_filp);
		MERROR("failed to open [%.*s/%s], ret = %d\n",
		       mrh_file->mrfi_dparent->d_name.len,
		       mrh_file->mrfi_dparent->d_name.name,
		       mrh_file->mrfi_fname, ret);
		goto out_dput;
	}

	init_rwsem(&mrh_file->mrfi_rwsem);
	goto out;
out_dput:
	dput(mrh_file->mrfi_dchild);
out:
	MRETURN(ret);
}

static int mrecord_file_add(struct mrecord_handle *handle, struct mrecord_head *head)
{
	int ret = 0;
	struct mrecord_file_info *mrh_file = &handle->u.mrh_file;
	ssize_t size = 0;
	MENTRY();

	MASSERT(mrh_file->mrfi_filp);
	down_write(&mrh_file->mrfi_rwsem);
	size = _do_read_write(WRITE, mrh_file->mrfi_filp,
	                      (void *)head, head->mrh_len,
	                      &mrh_file->mrfi_filp->f_pos);
	up_write(&mrh_file->mrfi_rwsem);
	MRETURN(ret);
}

static int mrecord_file_fini(struct mrecord_handle *handle)
{
	int ret = 0;
	struct mrecord_file_info *mrh_file = &handle->u.mrh_file;
	MENTRY();

	MASSERT(mrh_file->mrfi_filp);
	MASSERT(mrh_file->mrfi_dchild);
	fput(mrh_file->mrfi_filp);
	dput(mrh_file->mrfi_dchild);
	MRETURN(ret);
}

static int mrecord_file_cleanup(struct mrecord_handle *handle)
{
	int ret = 0;
	struct mrecord_file_info *mrh_file = &handle->u.mrh_file;
	struct iattr newattrs;
	struct inode *inode = NULL;
	MENTRY();

	MASSERT(mrh_file->mrfi_filp);
	MASSERT(mrh_file->mrfi_dchild);
	MASSERT(mrh_file->mrfi_dchild->d_inode);
	inode = mrh_file->mrfi_dchild->d_inode;
	MASSERT(S_ISREG(inode->i_mode));
	newattrs.ia_size = 0;
	newattrs.ia_valid = ATTR_SIZE;
	mutex_lock(&inode->i_mutex);
	ret = notify_change(mrh_file->mrfi_dchild, &newattrs);
	mutex_unlock(&inode->i_mutex);
	MRETURN(ret);
}

struct mrecord_operations mrecord_file_ops = {
	mro_init:    mrecord_file_init,
	mro_add:     mrecord_file_add,
	mro_cleanup: mrecord_file_cleanup,
	mro_fini:    mrecord_file_fini,
};
EXPORT_SYMBOL(mrecord_file_ops);

struct mrecord_operations mrecord_nop_ops = {
	mro_init:    NULL,
	mro_add:     NULL,
	mro_cleanup: NULL,
	mro_fini:    NULL,
};
EXPORT_SYMBOL(mrecord_nop_ops);

