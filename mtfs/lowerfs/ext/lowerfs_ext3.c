/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <linux/smp_lock.h>
#include <linux/ext3_jbd.h>
#include <linux/quota.h>
#include <linux/quotaops.h>
#include <debug.h>
#include <mtfs_lowerfs.h>
#include "lowerfs_ext.h"

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

#define _mlowerfs_ext3_journal_abort_handle(handle, bh, ret)    \
do {                                                            \
    if (bh) {                                                   \
        BUFFER_TRACE(bh, "abort");                              \
    }                                                           \
    if (!handle->h_err)                                         \
        handle->h_err = ret;                                    \
    if (!is_handle_aborted(handle)) {                           \
        MERROR("EXT3-fs: %s: aborting transaction: ret = %d\n", \
               ret);                                            \
        journal_abort_handle(handle);                           \
    }                                                           \
} while (0)

static int _mlowerfs_ext3_journal_get_write_access(handle_t *handle,
				                   struct buffer_head *bh)
{
    int ret = journal_get_write_access(handle, bh);
    if (ret) {
    	_mlowerfs_ext3_journal_abort_handle(handle, bh, ret);
    }
    return ret;
}

static int _mlowerfs_ext3_journal_dirty_metadata(handle_t *handle,
                                              struct buffer_head *bh)
{
    int ret = journal_dirty_metadata(handle, bh);
    if (ret) {
    	_mlowerfs_ext3_journal_abort_handle(handle, bh, ret);
    }
    return ret;
}

static void *mlowerfs_ext3_start(struct inode *inode, int op)
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
 * Check ext3_writepage_trans_blocks() for detail
 *
 */
static int mlowerfs_ext3_credits_needed(struct inode *inode, __u64 len)
{
	struct super_block *sb = inode->i_sb;
	int _nblock = (int)(len >> sb->s_blocksize_bits); /* Any possible overflow? */
	int nblock = _nblock + 2; /* Two block or more */
	int nindirect = _nblock / EXT3_ADDR_PER_BLOCK(sb) + 2;
	int ndindirect = _nblock / (EXT3_ADDR_PER_BLOCK(sb) * EXT3_ADDR_PER_BLOCK(sb)) + 2;
	int nbitmaps = 1 + nblock + nindirect + ndindirect; /* tindirect */
	int ngdblocks = nbitmaps > EXT3_SB(sb)->s_gdb_count ? EXT3_SB(sb)->s_gdb_count : nbitmaps;
	int ngroups = nbitmaps > EXT3_SB(sb)->s_groups_count ? EXT3_SB(sb)->s_groups_count : nbitmaps;
	int needed = 2; /* inodes + superblock */
	int i = 0;

	if (mlowerfs_ext3_should_journal_data(inode)) {
		needed += nbitmaps;
	}

	needed += ngdblocks + ngroups;

#if defined(CONFIG_QUOTA)
	if (!IS_NOQUOTA(inode)) {
		for (i = 0; i < MAXQUOTAS; i++) {
			if (sb_has_quota_active(sb, i)) {
				needed += EXT3_SINGLEDATA_TRANS_BLOCKS;
			}
		}
	}
#endif
	return needed;
}

static void *mlowerfs_ext3_brw_start(struct inode *inode, __u64 len)
{
	journal_t *journal = NULL;
	int needed = 0;
	handle_t *handle = NULL;
	MENTRY();

	journal = EXT3_SB(inode->i_sb)->s_journal;
	needed = mlowerfs_ext3_credits_needed(inode, len);

	/* The number of blocks we could _possibly_ dirty can very large.
	 * We reduce our request if it is absurd (and we couldn't get that
	 * many credits for a single handle anyways).
	 *
	 * At some point we have to limit the size of I/Os sent at one time,
	 * increase the size of the journal, or we have to calculate the
	 * actual journal requirements more carefully by checking all of
	 * the blocks instead of being maximally pessimistic.  It remains to
	 * be seen if this is a real problem or not.
	 */
	if (needed > journal->j_max_transaction_buffers) {
		MERROR("want too many journal credits (%d) using %d instead\n",
		       needed, journal->j_max_transaction_buffers);
		needed = journal->j_max_transaction_buffers;
        }

	MASSERT(needed > 0);
	handle = _mlowerfs_ext3_journal_start(inode, needed);
	if (IS_ERR(handle)) {
		MERROR("can't get handle for %d credits: rc = %ld\n", needed,
		       PTR_ERR(handle));
        } else {
		MASSERT(handle->h_buffer_credits >= needed);
		MASSERT(current->journal_info == handle);
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

static int mlowerfs_ext3_extend(struct inode *inode, unsigned int nblocks, void *h)
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

static int _mlowerfs_ext3_journal_stop(handle_t *handle)
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

static int mlowerfs_ext3_commit(struct inode *inode, void *h, int force_sync)
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

static int mlowerfs_ext3_commit_async(struct inode *inode, void *h,
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

static int mlowerfs_ext3_commit_wait(struct inode *inode, void *h)
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

static int mlowerfs_ext3_read_record(struct file * file, void *buf,
                                     int size, loff_t *offs)
{
	struct inode *inode = file->f_dentry->d_inode;
	unsigned long block;
	struct buffer_head *bh;
	int err, blocksize, csize, boffs;

	/* prevent reading after eof */
	lock_kernel();
	if (i_size_read(inode) < *offs + size) {
		size = i_size_read(inode) - *offs;
		unlock_kernel();
		if (size < 0) {
			MDEBUG("size %llu is too short for read @%llu\n",
			       i_size_read(inode), *offs);
			return -EBADR;
		} else if (size == 0) {
			return 0;
		}
	} else {
		unlock_kernel();
	}

	blocksize = 1 << inode->i_blkbits;

	while (size > 0) {
		block = *offs >> inode->i_blkbits;
		boffs = *offs & (blocksize - 1);
		csize = min(blocksize - boffs, size);
		bh = _mlowerfs_ext3_bread(NULL, inode, block, 0, &err);
		if (!bh) {
			MERROR("can't read block: %d\n", err);
			return err;
		}

		memcpy(buf, bh->b_data + boffs, csize);
		brelse(bh);

		*offs += csize;
		buf += csize;
		size -= csize;
	}
	return 0;
}

int mlowerfs_ext3_write_record(struct file *file, void *buf, int bufsize,
                               loff_t *offs, int force_sync)
{
	struct buffer_head *bh = NULL;
	unsigned long block;
	struct inode *inode = file->f_dentry->d_inode;
	loff_t old_size = i_size_read(inode), offset = *offs;
	loff_t new_size = i_size_read(inode);
	handle_t *handle;
	int err = 0, block_count = 0, blocksize, size, boffs;

	/* Determine how many transaction credits are needed */
	blocksize = 1 << inode->i_blkbits;
	block_count = (*offs & (blocksize - 1)) + bufsize;
	block_count = (block_count + blocksize - 1) >> inode->i_blkbits;

	handle = _mlowerfs_ext3_journal_start(inode,
	                                      block_count * EXT3_DATA_TRANS_BLOCKS(inode->i_sb) + 2);
	if (IS_ERR(handle)) {
		MERROR("can't start transaction for %d blocks (%d bytes)\n",
		       block_count * EXT3_DATA_TRANS_BLOCKS(inode->i_sb) + 2, bufsize);
		return PTR_ERR(handle);
	}

	while (bufsize > 0) {
		if (bh != NULL)
			brelse(bh);

		block = offset >> inode->i_blkbits;
		boffs = offset & (blocksize - 1);
		size = min(blocksize - boffs, bufsize);
		bh = _mlowerfs_ext3_bread(handle, inode, block, 1, &err);
		if (!bh) {
			MERROR("can't read/create block: %d\n", err);
			goto out;
		}

		err = _mlowerfs_ext3_journal_get_write_access(handle, bh);
		if (err) {
			MERROR("journal_get_write_access() returned error %d\n",
			       err);
			goto out;
		}
		MASSERT(bh->b_data + boffs + size <= bh->b_data + bh->b_size);
		memcpy(bh->b_data + boffs, buf, size);
		err = _mlowerfs_ext3_journal_dirty_metadata(handle, bh);
		if (err) {
			MERROR("journal_dirty_metadata() returned error %d\n",
			       err);
			goto out;
		}
		if (offset + size > new_size)
			new_size = offset + size;
		offset += size;
		bufsize -= size;
		buf += size;
	}

	if (force_sync)
		handle->h_sync = 1; /* recovery likes this */
out:
	if (bh)
		brelse(bh);

	/* correct in-core and on-disk sizes */
	if (new_size > i_size_read(inode)) {
		lock_kernel();
		if (new_size > i_size_read(inode))
			i_size_write(inode, new_size);
		if (i_size_read(inode) > EXT3_I(inode)->i_disksize)
			EXT3_I(inode)->i_disksize = i_size_read(inode);
		if (i_size_read(inode) > old_size)
			mark_inode_dirty(inode);
		unlock_kernel();
	}

	_mlowerfs_ext3_journal_stop(handle);

	if (err == 0)
		*offs = offset;
	return err;
}

struct mtfs_lowerfs lowerfs_ext3 = {
	ml_owner:           THIS_MODULE,
	ml_type:            "ext3",
	ml_magic:           EXT3_SUPER_MAGIC,
	ml_bucket_type:     &mlowerfs_bucket_xattr,
	ml_flag:            0,
	ml_start:           mlowerfs_ext3_start,
	ml_brw_start:       mlowerfs_ext3_brw_start,
	ml_extend:          mlowerfs_ext3_extend,
	ml_commit:          mlowerfs_ext3_commit,
	ml_commit_async:    mlowerfs_ext3_commit_async,
	ml_commit_wait:     mlowerfs_ext3_commit_wait,
	ml_write_record:    mlowerfs_ext3_write_record,
	ml_read_record:     mlowerfs_ext3_read_record,
	ml_setflag:         mlowerfs_setflag_default,
	ml_getflag:         mlowerfs_getflag_default,
};