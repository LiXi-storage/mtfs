/*
 * Copied from Lustre-2.x
 */
#include <mtfs_lowerfs.h>
#include <mtfs_log.h>

static int mlog_vfs_pad(struct mtfs_lowerfs *lowerfs,
                    struct file *file,
                    int len,
                    int index)
{
	struct mlog_rec_hdr rec = { 0 };
	struct mlog_rec_tail tail;
	int ret = 0;
	MENTRY();

	MASSERT(len >= MLOG_MIN_REC_SIZE && (len & 0x7) == 0);

	tail.mrt_len = rec.mrh_len = len;
	tail.mrt_index = rec.mrh_index = index;
	rec.mrh_type = MLOG_PAD_MAGIC;

	ret = mlowerfs_write_record(lowerfs, file, &rec, sizeof(rec), &file->f_pos, 0);
	if (ret) {
                MERROR("error writing padding record, ret = %d\n", ret);
		goto out;
	}

	file->f_pos += len - sizeof(rec) - sizeof(tail);
	ret = mlowerfs_write_record(lowerfs, file, &tail, sizeof(tail),&file->f_pos, 0);
	if (ret) {
		MERROR("error writing padding record, ret = %d\n", ret);
		goto out;
	}

 out:
        MRETURN(ret);
}

static int mlog_vfs_read_blob(struct mtfs_lowerfs *lowerfs,
                          struct file *file,
                          void *buf,
                          int size,
                          loff_t off)
{
	loff_t offset = off;
	int ret = 0;
	MENTRY();

	ret = mlowerfs_read_record(lowerfs, file, buf, size, &offset);
	if (ret) {
		MERROR("error reading log record, ret =  %d\n", ret);
		goto out;
	}
out:
	MRETURN(0);
}

static int mlog_vfs_write_blob(struct mtfs_lowerfs *lowerfs,
                           struct file *file,
                           struct mlog_rec_hdr *rec,
                           void *buf,
                           loff_t off)
{
	int ret = 0;
	struct mlog_rec_tail end;
	loff_t saved_off = file->f_pos;
	int buflen = rec->mrh_len;
	MENTRY();

	file->f_pos = off;

	if (buflen == 0) {
		MWARN("0-length record\n");
	}

	if (!buf) {
		ret = mlowerfs_write_record(lowerfs, file, rec, buflen,&file->f_pos,0);
		if (ret) {
			MERROR("error writing log record, ret = %d\n", ret);
                        goto out;
                }
                goto out;
        }

        /* the buf case */
        rec->mrh_len = sizeof(*rec) + buflen + sizeof(end);
        ret = mlowerfs_write_record(lowerfs, file, rec, sizeof(*rec), &file->f_pos, 0);
        if (ret) {
                MERROR("error writing log hdr, ret = %d\n", ret);
                goto out;
        }

        ret = mlowerfs_write_record(lowerfs, file, buf, buflen, &file->f_pos, 0);
        if (ret) {
                MERROR("error writing log buffer, ret = %d\n", ret);
                goto out;
        }

        end.mrt_len = rec->mrh_len;
        end.mrt_index = rec->mrh_index;
	ret = mlowerfs_write_record(lowerfs, file, &end, sizeof(end), &file->f_pos, 0);
	if (ret) {
		MERROR("error writing log tail, ret = %d\n", ret);
		goto out;
	}

 out:
	if (saved_off > file->f_pos)
		file->f_pos = saved_off;
	MASSERT(ret <= 0);
	MRETURN(ret);
}

/* returns negative in on error; 0 if success && reccookie == 0; 1 otherwise */
/* appends if idx == -1, otherwise overwrites record idx. */
int mlog_vfs_write_rec(struct mlog_handle *loghandle,
                   struct mlog_rec_hdr *rec,
                   struct mlog_cookie *reccookie,
                   int cookiecount,
                   void *buf, int idx)
{
	struct mlog_log_hdr *mlh;
	int reclen = rec->mrh_len;
	int index = 0;
	int ret = 0;
	struct mlog_rec_tail *mrt;
	struct file *file;
	size_t left;
	struct mtfs_lowerfs *lowerfs = NULL;
	MENTRY();

	mlh = loghandle->mgh_hdr;
	file = loghandle->mgh_file;
	lowerfs = loghandle->mgh_ctxt->moc_lowerfs;

	/* record length should not bigger than MLOG_CHUNK_SIZE */
	if (buf){
		ret = (reclen > MLOG_CHUNK_SIZE - sizeof(struct mlog_rec_hdr) -
		      sizeof(struct mlog_rec_tail)) ? -E2BIG : 0;
	} else {
		ret = (reclen > MLOG_CHUNK_SIZE) ? -E2BIG : 0;
	}

	if (ret) {
		goto out;
	}
	if (buf) {
		/* write_blob adds header and tail to lrh_len. */ 
		reclen = sizeof(*rec) + rec->mrh_len + 
		         sizeof(struct mlog_rec_tail);
	}

	if (idx != -1) {
		loff_t saved_offset;

		/* no header: only allowed to insert record 1 */
		if (idx != 1 && !i_size_read(file->f_dentry->d_inode)) {
			MERROR("idx != -1 in empty log\n");
			MBUG();
		}

		if (idx && mlh->mlh_size && mlh->mlh_size != rec->mrh_len) {
			ret = -EINVAL;
			goto out;
		}

		if (!ext2_test_bit(idx, mlh->mlh_bitmap)) {
			MERROR("Modify unset record %u\n", idx);
		}

		if (idx != rec->mrh_index) {
			MERROR("Index mismatch %d %u\n", idx, rec->mrh_index);
		}

                ret = mlog_vfs_write_blob(lowerfs, file, &mlh->mlh_hdr, NULL, 0);
                /* we are done if we only write the header or on error */
                if (ret || idx == 0) {
			goto out;
                }

		/* Assumes constant lrh_len */
		saved_offset = sizeof(*mlh) + (idx - 1) * reclen;

		if (buf) {
			struct mlog_rec_hdr check;

			/* We assume that caller has set mgh_cur_* */
			saved_offset = loghandle->mgh_cur_offset;

                        MDEBUG("modify record %I64x: idx:%d/%u/%d, len:%u "
                               "offset %llu\n",
                               loghandle->mgh_id.mgl_oid, idx, rec->mrh_index,
                               loghandle->mgh_cur_idx, rec->mrh_len,
                               (long long)(saved_offset - sizeof(*mlh)));
			if (rec->mrh_index != loghandle->mgh_cur_idx) {
				MERROR("modify idx mismatch %u/%d\n",
				       idx, loghandle->mgh_cur_idx);
				ret = -EFAULT;
				goto out;
                        }
#if 1  /* FIXME remove this safety check at some point */
                        /* Verify that the record we're modifying is the 
                           right one. */
			ret = mlog_vfs_read_blob(lowerfs, file, &check,
			                    sizeof(check), saved_offset);
			if (check.mrh_index != idx || check.mrh_len != reclen) {
				MERROR("bad modify idx %u/%u size %u/%u (%d)\n",
                                       idx, check.mrh_index, reclen, 
                                       check.mrh_len, ret);
                                ret = -EFAULT;
                                goto out;
                        }
#endif
		}

		ret = mlog_vfs_write_blob(lowerfs, file, rec, buf, saved_offset);
		if (ret == 0 && reccookie) {
			reccookie->mgc_lgl = loghandle->mgh_id;
			reccookie->mgc_index = idx;
			ret = 1;
		}
		goto out;
	}

        /* Make sure that records don't cross a chunk boundary, so we can
         * process them page-at-a-time if needed.  If it will cross a chunk
         * boundary, write in a fake (but referenced) entry to pad the chunk.
         *
         * We know that mlog_current_log() will return a loghandle that is
         * big enough to hold reclen, so all we care about is padding here.
         */
	left = MLOG_CHUNK_SIZE - (file->f_pos & (MLOG_CHUNK_SIZE - 1));
	/* NOTE: padding is a record, but no bit is set */
	if (left != 0 && left != reclen &&
	    left < (reclen + MLOG_MIN_REC_SIZE)) {
		index = loghandle->mgh_last_idx + 1;
		ret = mlog_vfs_pad(lowerfs, file, left, index);
		if (ret) {
			goto out;
		}
		loghandle->mgh_last_idx++; /*for pad rec*/
        }

        /* if it's the last idx in log file, then return -ENOSPC */
	if (loghandle->mgh_last_idx >= MLOG_BITMAP_SIZE(mlh) - 1) {
		ret = -ENOSPC;
		goto out;
	}
	index = ++loghandle->mgh_last_idx;
	rec->mrh_index = index;
        if (buf == NULL) {
		mrt = (struct mlog_rec_tail *)
		       ((char *)rec + rec->mrh_len - sizeof(*mrt));
		mrt->mrt_len = rec->mrh_len;
		mrt->mrt_index = rec->mrh_index;
        }
	/*The caller should make sure only 1 process access the lgh_last_idx,
	 *Otherwise it might hit the assert.*/
	MASSERT(index < MLOG_BITMAP_SIZE(mlh));
	if (ext2_set_bit(index, mlh->mlh_bitmap)) {
                MERROR("argh, index %u already set in log bitmap?\n", index);
                MBUG(); /* should never happen */
        }
        mlh->mlh_count++;
        mlh->mlh_tail.mrt_index = index;

        ret = mlog_vfs_write_blob(lowerfs, file, &mlh->mlh_hdr, NULL, 0);
        if (ret) {
                goto out;
        }

	ret = mlog_vfs_write_blob(lowerfs, file, rec, buf, file->f_pos);
        if (ret) {
                goto out;
        }

        MDEBUG("added record %I64x: idx: %u, %u bytes\n",
               loghandle->mgh_id.mgl_oid, index, rec->mrh_len);
        if (ret == 0 && reccookie) {
                reccookie->mgc_lgl = loghandle->mgh_id;
                reccookie->mgc_index = index;
                ret = 1;
        }
        if (ret == 0 && rec->mrh_type == MLOG_GEN_REC) {
                ret = 1;
        }

out:
	MRETURN(ret);
}
