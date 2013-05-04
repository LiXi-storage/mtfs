/*
 * Copied from Lustre-2.x
 */
#include <linux/namei.h>
#include <linux/mount.h>
#include <mtfs_lowerfs.h>
#include <mtfs_log.h>
#include <mtfs_file.h>

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

/* Allocate a new log or catalog handle */
struct mlog_handle *mlog_alloc_handle(void)
{
	struct mlog_handle *loghandle;
	MENTRY();

	MTFS_ALLOC(loghandle, sizeof(*loghandle));
	if (loghandle == NULL) {
		goto out;
	}

	init_rwsem(&loghandle->mgh_lock);
out:
	MRETURN(loghandle);
}

void mlog_free_handle(struct mlog_handle *loghandle)
{
	if (!loghandle) {
		return;
	}

	if (!loghandle->mgh_hdr) {
		goto out;
	}

	if (loghandle->mgh_hdr->mlh_flags & MLOG_F_IS_PLAIN) {
		list_del_init(&loghandle->u.phd.phd_entry);
	}
	if (loghandle->mgh_hdr->mlh_flags & MLOG_F_IS_CAT) {
		MASSERT(list_empty(&loghandle->u.chd.chd_head));
	}
	MTFS_FREE(loghandle->mgh_hdr, MLOG_CHUNK_SIZE);

 out:
	MTFS_FREE(loghandle, sizeof(*loghandle));
}

/* Return name length*/
int mlog_id2name(struct mlog_logid *logid, char **name)
{
	char *tmp_name = NULL;
	int ret = 64;

	MTFS_ALLOC(tmp_name, ret);
	if (tmp_name == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	snprintf(tmp_name, sizeof(tmp_name), "log_0x%llx", logid->mgl_oid);
	*name = tmp_name;
out:
	return ret;
}

static struct file *mlog_vfs_open_id(struct mlog_ctxt *ctxt, struct mlog_logid *logid)
{
	struct dentry *dchild = NULL;
	struct file *file = NULL;
	int length = 0;
	char *name = NULL;
	MENTRY();

	length = mlog_id2name(logid, &name);
	if (length < 0) {
		MERROR("failed to get name from id\n");
		file = ERR_PTR(length);
		goto out;
	}

	dchild = lookup_one_len(name,
	                        ctxt->moc_dlog,
	                        strlen(name));
	if (IS_ERR(dchild)) {
		MERROR("lookup [%.*s/%s] failed, ret = %d\n",
		       ctxt->moc_dlog->d_name.len,
		       ctxt->moc_dlog->d_name.name,
		       name,
		       PTR_ERR(dchild));
		file = (struct file *)dchild;
		goto out_free_name;
	}

	if (dchild->d_inode == NULL) {
		MERROR("lookup [%.*s/%s] failed, no such file\n",
		       ctxt->moc_dlog->d_name.len,
		       ctxt->moc_dlog->d_name.name,
		       name);
		file = ERR_PTR(-ENOENT);
		goto out_put_dchild;
	}

	if (dchild->d_inode->i_nlink == 0) {
		if (dchild->d_inode->i_mode == 0 &&
		    dchild->d_inode->i_ctime.tv_sec == 0 ) {
			MERROR("found inode with zero nlink, mode and ctime, "
			       "this may indicate disk corruption, "
			       "inode = %lu, "
			       "link = %lu, "
			       "count = %d\n",
			       dchild->d_inode->i_ino,
			       (unsigned long)dchild->d_inode->i_nlink,
			       atomic_read(&dchild->d_inode->i_count));
                } else {
             	  	MERROR("found inode with zero nlink, "
             	  	       "inode = %lu, "
             	  	       "link = %lu, "
             	  	       "count = %d\n",
             	  	       dchild->d_inode->i_ino,
			       (unsigned long)dchild->d_inode->i_nlink,
			       atomic_read(&dchild->d_inode->i_count));
		}
		file = ERR_PTR(-ENOENT);
		goto out_put_dchild;
	}

	if (dchild->d_inode->i_generation != logid->mgl_ogen) {
		/* we didn't find the right inode.. */
		MDEBUG("found wrong generation, "
		       "inode = %lu,"
		       "link = %lu, "
		       "count = %d, "
		       "generation = %u/%u\n",
		        dchild->d_inode->i_ino,
		        (unsigned long)dchild->d_inode->i_nlink,
		        atomic_read(&dchild->d_inode->i_count),
		        dchild->d_inode->i_generation,
		        logid->mgl_ogen);
		file = ERR_PTR(-ENOENT);
		goto out_put_dchild;
	}

	file = mtfs_dentry_open(dchild,
	                        mntget(ctxt->moc_mnt),
	                        O_RDWR | O_CREAT | O_LARGEFILE,
	                        current_cred()); 
	if (IS_ERR(file)) {
		MERROR("failed to open file\n");
		goto out_put_dchild;
	}
	goto out_free_name;
out_put_dchild:
	dput(dchild);
out_free_name:
	MTFS_ALLOC(name, length);
out:
	MRETURN(file);
}

static struct file *mlog_vfs_open_name(struct mlog_ctxt *ctxt, const char *name)
{
	struct dentry *dchild = NULL;
	struct file *file = NULL;
	MENTRY();

	dchild = mtfs_dchild_create(ctxt->moc_dlog,
	                            name, strlen(name),
	                            S_IFREG | S_IRWXU, 0, NULL, 0);
	if (IS_ERR(dchild)) {
		MERROR("failed to create [%.*s/%s], ret = %d\n",
		       ctxt->moc_dlog->d_name.len,
		       ctxt->moc_dlog->d_name.name,
		       name, PTR_ERR(dchild));
		file = (struct file *)dchild;
		goto out;
	}

	file = mtfs_dentry_open(dchild,
	                        mntget(ctxt->moc_mnt),
	                        O_RDWR | O_CREAT | O_LARGEFILE,
	                        current_cred()); 
	if (IS_ERR(file)) {
		MERROR("failed to open file\n");
		goto out_put_dchild;
	}

	goto out_free_name;
out_put_dchild:
	dput(dchild);
out_free_name:
	
out:
	MRETURN(file);
}

/* This is a callback from the llog_* functions.
 * Assumes caller has already pushed us into the kernel context. */
int mlog_vfs_create(struct mlog_ctxt *ctxt, struct mlog_handle **res,
                    struct mlog_logid *logid, char *name)
{
	struct mlog_handle *handle = NULL;
	int ret = 0;
	MENTRY();

	handle = mlog_alloc_handle();
	if (handle == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	*res = handle;

	MASSERT(ctxt);
	if (logid != NULL) {
		handle->mgh_file = mlog_vfs_open_id(ctxt, logid);
		if (IS_ERR(handle->mgh_file)) {
			goto out_free_handle;
		}
		/* assign the value of lgh_id for handle directly */
		handle->mgh_id = *logid;
	} else if (name) {
		handle->mgh_file = mlog_vfs_open_name(ctxt, name);
		if (IS_ERR(handle->mgh_file)) {
			goto out_free_handle;
		}
                handle->mgh_id.mgl_ogr = 1;
                handle->mgh_id.mgl_oid =
                        handle->mgh_file->f_dentry->d_inode->i_ino;
                handle->mgh_id.mgl_ogen =
                        handle->mgh_file->f_dentry->d_inode->i_generation;
	} else {
		MBUG();
	}
	goto out;
out_free_handle:
	mlog_free_handle(handle);
out:
	MRETURN(ret);
}
