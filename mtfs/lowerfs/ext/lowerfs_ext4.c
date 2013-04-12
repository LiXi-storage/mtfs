/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <../fs/ext4/ext4_jbd2.h>
#include <debug.h>
#include <mtfs_lowerfs.h>

#define _mlowerfs_ext4_abort(sb, fmt, ...)                               \
do {                                                                     \
    MERROR("EXT4-fs (%s): error "fmt"\n", sb->s_id, ##__VA_ARGS__);      \
    if (test_opt(sb, ERRORS_PANIC))                                      \
        panic("EXT4-fs: panic from previous error\n");                   \
    if (!(sb->s_flags & MS_RDONLY)) {                                    \
        MERROR("EXT4-fs (%s): error: remounting filesystem read-only\n", \
               sb->s_id);                                                \
        EXT4_SB(sb)->s_mount_state |= EXT4_ERROR_FS;                     \
        sb->s_flags |= MS_RDONLY;                                        \
        EXT4_SB(sb)->s_mount_opt |= EXT4_MF_FS_ABORTED;                  \
        if (EXT4_SB(sb)->s_journal)                                      \
            jbd2_journal_abort(EXT4_SB(sb)->s_journal, -EIO);            \
    }                                                                    \
} while (0)

/* Just increment the non-pointer handle value */
static handle_t *lowerfs_ext4_get_nojournal(void)
{
	handle_t *handle = current->journal_info;
	unsigned long ref_cnt = (unsigned long)handle;

	BUG_ON(ref_cnt >= EXT4_NOJOURNAL_MAX_REF_COUNT);

	ref_cnt++;
	handle = (handle_t *)ref_cnt;

	current->journal_info = handle;
	return handle;
}

static handle_t *mlowerfs_ext4_journal_start_sb(struct super_block *sb, int nblocks)
{
	journal_t *journal;

	if (sb->s_flags & MS_RDONLY)
		return ERR_PTR(-EROFS);

	/* Special case here: if the journal has aborted behind our
	 * backs (eg. EIO in the commit thread), then we still need to
	 * take the FS itself readonly cleanly. */
	journal = EXT4_SB(sb)->s_journal;
	if (journal) {
		if (is_journal_aborted(journal)) {
			_mlowerfs_ext4_abort(sb, "Detected aborted journal");
			return ERR_PTR(-EROFS);
		}
		return jbd2_journal_start(journal, nblocks);
	}
	return lowerfs_ext4_get_nojournal();
}

handle_t *mlowerfs_ext4_journal_start(struct inode *inode, int nblocks)
{
	return mlowerfs_ext4_journal_start_sb(inode->i_sb, nblocks);
}

static int _mlowerfs_ext4_handle_valid(handle_t *handle)
{
	if ((unsigned long)handle < EXT4_NOJOURNAL_MAX_REF_COUNT)
		return 0;
	return 1;
}

/* Decrement the non-pointer handle value */
static void _mlowerfs_ext4_put_nojournal(handle_t *handle)
{
	unsigned long ref_cnt = (unsigned long)handle;

	BUG_ON(ref_cnt == 0);

	ref_cnt--;
	handle = (handle_t *)ref_cnt;

	current->journal_info = handle;
}


int lowerfs_ext4_journal_stop(const char *where, handle_t *handle)
{
	struct super_block *sb;
	int err;
	int rc;

	if (!_mlowerfs_ext4_handle_valid(handle)) {
		_mlowerfs_ext4_put_nojournal(handle);
		return 0;
	}
	sb = handle->h_transaction->t_journal->j_private;
	err = handle->h_err;
	rc = jbd2_journal_stop(handle);

	if (!err)
		err = rc;
	return err;
}


struct mtfs_lowerfs lowerfs_ext4 = {
	ml_owner:           THIS_MODULE,
	ml_type:            "ext4",
	ml_magic:           EXT4_SUPER_MAGIC,
	ml_bucket_type:     &mlowerfs_bucket_xattr,
	ml_flag:            0,
	ml_setflag:         mlowerfs_setflag_default,
	ml_getflag:         mlowerfs_getflag_default,
};