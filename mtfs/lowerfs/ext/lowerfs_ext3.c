/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <linux/ext3_jbd.h>
#include <linux/quota.h>
#include <linux/quotaops.h>
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

static inline int mlowerfs_ext3_should_journal_data(struct inode *inode)
{
	if (!S_ISREG(inode->i_mode))
		return 1;
	if (test_opt(inode->i_sb, DATA_FLAGS) == EXT3_MOUNT_JOURNAL_DATA)
		return 1;
	if (EXT3_I(inode)->i_flags & EXT3_JOURNAL_DATA_FL)
		return 1;
	return 0;
}

/*
 * Calculate the number of buffer credits needed to write multiple pages in
 * a single ext3 transaction. 
 *
 * With N blocks, we have at most:
 * N for leaf
 * 
 * min(N*P, blocksize/4 + 1) dindirect blocks
 * niocount tindirect
 *
 */
int mlowerfs_ext3_credits_needed(struct inode *inode, __u64 offset, __u64 len)
{
	struct super_block *sb = inode->i_sb;
	int _nblock = len >> sb->s_blocksize_bits;
	int nblock = _nblock > 2 ? _nblock : 2;
	int _nindirect = len / (EXT3_ADDR_PER_BLOCK(sb) << sb->s_blocksize_bits);
	int nindirect = _nindirect > 2 ? _nindirect : 2;
	int _ndindirect = (len >> sb->s_blocksize_bits) / (EXT3_ADDR_PER_BLOCK(sb) * EXT3_ADDR_PER_BLOCK(sb));
	int ndindirect = _ndindirect > 2 ? _ndindirect : 2;
	int nbitmaps = 1 + nblock + nindirect + ndindirect; /* tindirect */
	int ngdblocks = nbitmaps > EXT3_SB(sb)->s_gdb_count ? EXT3_SB(sb)->s_gdb_count : nbitmaps;
	int ngroups = nbitmaps > EXT3_SB(sb)->s_groups_count ? EXT3_SB(sb)->s_groups_count : nbitmaps;
	int needed = 2; /* inodes + superblock */
	int i = 0;

	if (mlowerfs_ext3_should_journal_data(inode)) {
		needed += nbitmaps;
	}

	needed += ngdblocks + ngroups;

	/* last_rcvd update, this used to be handled in vfs_dq_init() */
	needed += EXT3_DATA_TRANS_BLOCKS(sb);

#if defined(CONFIG_QUOTA)
	for (i = 0; i < MAXQUOTAS; i++) {
		if (sb_has_quota_active(sb, i)) {
			needed += EXT3_SINGLEDATA_TRANS_BLOCKS;
		}
	}
#endif
	return needed;
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