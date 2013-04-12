/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <linux/ext3_jbd.h>
#include <debug.h>
#include <mtfs_lowerfs.h>

#define _mlowerfs_ext3_abort(sb, fmt, ...)                               \
do {                                                                     \
    MERROR("EXT3-fs (%s): error "fmt"\n", sb->s_id, ##__VA_ARGS__);      \
    if (test_opt(sb, ERRORS_PANIC))                                      \
        panic("EXT3-fs: panic from previous error\n");                   \
    if (!(sb->s_flags & MS_RDONLY)) {                                    \
        MERROR("EXT3-fs (%s): error: remounting filesystem read-only\n", \
               sb->s_id);                                                \
        EXT3_SB(sb)->s_mount_state |= EXT3_ERROR_FS;                     \
        sb->s_flags |= MS_RDONLY;                                        \
        EXT3_SB(sb)->s_mount_opt |= EXT3_MOUNT_ABORT;                    \
        if (EXT3_SB(sb)->s_journal)                                      \
            journal_abort(EXT3_SB(sb)->s_journal, -EIO);                 \
    }                                                                    \
} while (0)

static handle_t *_mlowerfs_ext3_journal_start_sb(struct super_block *sb, int nblocks)
{
	journal_t *journal;

	if (sb->s_flags & MS_RDONLY)
		return ERR_PTR(-EROFS);

	/* Special case here: if the journal has aborted behind our
	 * backs (eg. EIO in the commit thread), then we still need to
	 * take the FS itself readonly cleanly. */
	journal = EXT3_SB(sb)->s_journal;
	if (is_journal_aborted(journal)) {
		_mlowerfs_ext3_abort(sb, "Detected aborted journal");
		return ERR_PTR(-EROFS);
	}

	return journal_start(journal, nblocks);
}

static inline handle_t *_mlowerfs_ext3_journal_start(struct inode *inode, int nblocks)
{
	return _mlowerfs_ext3_journal_start_sb(inode->i_sb, nblocks);
}

void *mlowerfs_ext3_start(struct inode *inode, int op)
{
	int nblocks = EXT3_SINGLEDATA_TRANS_BLOCKS;
	//journal_t *journal = NULL;
	void *handle = NULL;
	MENTRY();

        if (current->journal_info) {
                goto journal_start;
        }

	/* Increase block number according to operation type */

journal_start:
	MASSERT(nblocks > 0);
	handle = _mlowerfs_ext3_journal_start(inode, nblocks);
	if (!IS_ERR(handle)) {
		MASSERT(current->journal_info == handle);
	} else {
		MERROR("error starting handle for op %u (%u credits): rc %ld\n",
		       op, nblocks, PTR_ERR(handle));
	}

	MRETURN(handle);
}

static inline int _mlowerfs_ext3_journal_extend(handle_t *handle, int nblocks)
{
	return journal_extend(handle, nblocks);
}

static inline int _lowerfs_ext3_journal_restart(handle_t *handle, int nblocks)
{
	return journal_restart(handle, nblocks);
}

int mlowerfs_ext3_extend(struct inode *inode, unsigned int nblocks, void *h)
{
	handle_t *handle = h;
	int ret = 0;
	MENTRY();

	/* fsfilt_extend called with nblocks = 0 for testing in special cases */
	if (nblocks == 0) {
		handle->h_buffer_credits = 0;
		MWARN("setting credits of handle %p to zero by request\n", h);
	}

	if (handle->h_buffer_credits > nblocks) {
		ret = 0;
		goto out;
	}

	if (_mlowerfs_ext3_journal_extend(handle, nblocks) == 0) {
		ret = 0;
		goto out;
	}

	MASSERT(inode->i_sb->s_op->dirty_inode);
	inode->i_sb->s_op->dirty_inode(inode);
	ret = _lowerfs_ext3_journal_restart(handle, nblocks);

out:
	MRETURN(ret);
}

int _mlowerfs_ext3_journal_stop(handle_t *handle)
{
	struct super_block *sb;
	int err;
	int rc;

	sb = handle->h_transaction->t_journal->j_private;
	err = handle->h_err;
	rc = journal_stop(handle);

	if (!err)
		err = rc;
	return err;
}

int mlowerfs_ext3_commit(struct inode *inode, void *h, int force_sync)
{
	int ret = 0;
	handle_t *handle = h;
	MENTRY();

	MASSERT(current->journal_info == handle);
	if (force_sync){
		handle->h_sync = 1; /* recovery likes this */
	}

	ret = _mlowerfs_ext3_journal_stop(handle);
	if (ret) {
		MERROR("failed to stop journal, ret = %d\n", ret);
	}

        MRETURN(ret);
}

int mlowerfs_ext3_commit_async(struct inode *inode, void *h,
                               void **wait_handle)
{
	unsigned long tid = 0;
	transaction_t *transaction = NULL;
	handle_t *handle = h;
	journal_t *journal = NULL;
	int ret = 0;
	MENTRY();

	MASSERT(current->journal_info == handle);

	transaction = handle->h_transaction;
	journal = transaction->t_journal;
	tid = transaction->t_tid;
	/* we don't want to be blocked */
	handle->h_sync = 0;
	ret = _mlowerfs_ext3_journal_stop(handle);
	if (ret) {
		MERROR("error while stopping transaction: %d\n", ret);
		goto out;
	}
	log_start_commit(journal, tid);

	*wait_handle = (void *) tid;
out:
	MRETURN(ret);
}

int mlowerfs_ext3_commit_wait(struct inode *inode, void *h)
{
	journal_t *journal = EXT3_JOURNAL(inode);
	tid_t tid = (tid_t)(long)h;
	int ret = 0;
	MENTRY();

	if (unlikely(is_journal_aborted(journal))) {
		ret = -EIO;
		goto out;
	}

	log_wait_commit(EXT3_JOURNAL(inode), tid);

	if (unlikely(is_journal_aborted(journal))) {
		ret = -EIO;
		goto out;
        }
out:
        return ret;
}

struct mtfs_lowerfs lowerfs_ext3 = {
	ml_owner:           THIS_MODULE,
	ml_type:            "ext3",
	ml_magic:           EXT3_SUPER_MAGIC,
	ml_bucket_type:     &mlowerfs_bucket_xattr,
	ml_flag:            0,
	ml_start:           mlowerfs_ext3_start,
	ml_extend:          mlowerfs_ext3_extend,
	ml_commit:          mlowerfs_ext3_commit,
	ml_commit_async:    mlowerfs_ext3_commit_async,
	ml_commit_wait:     mlowerfs_ext3_commit_wait,
	ml_setflag:         mlowerfs_setflag_default,
	ml_getflag:         mlowerfs_getflag_default,
};