/*
 * Copied from Lustre-2.x
 */
#include <linux/namei.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/random.h>
#include <thread.h>
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

static void mlog_swab_rec(struct mlog_rec_hdr *rec, struct mlog_rec_tail *tail)
{
	__swab32s(&rec->mrh_len);
	__swab32s(&rec->mrh_index);
	__swab32s(&rec->mrh_type);

	switch (rec->mrh_type) {
	case MLOG_HDR_MAGIC: {
		struct mlog_log_hdr *mlh = (struct mlog_log_hdr *)rec;

		__swab64s(&mlh->mlh_timestamp);
		__swab32s(&mlh->mlh_count);
		__swab32s(&mlh->mlh_bitmap_offset);
		__swab32s(&mlh->mlh_flags);
		__swab32s(&mlh->mlh_size);
		__swab32s(&mlh->mlh_cat_idx);
		if (tail != &mlh->mlh_tail) {
			__swab32s(&mlh->mlh_tail.mrt_index);
			__swab32s(&mlh->mlh_tail.mrt_len);
		}

		break;
	}
	case MLOG_LOGID_MAGIC: {
		struct mlog_logid_rec *mid = (struct mlog_logid_rec *)rec;

		__swab64s(&mid->mid_id.mgl_oid);
		__swab64s(&mid->mid_id.mgl_ogr);
		__swab32s(&mid->mid_id.mgl_ogen);
		break;
	}
	case MLOG_EXTENT_MAGIC: {
		struct mlog_extent_rec *mex = (struct mlog_extent_rec *)rec;

		__swab64s(&mex->mex_start);
		__swab64s(&mex->mex_end);
		break;
	}
	case MLOG_PAD_MAGIC:
	/* ignore old pad records of type 0 */
	default:
		MERROR("Unknown mlog rec type %#x swabbing rec %p\n",
		       rec->mrh_type, rec);
	}

	if (tail) {
		__swab32s(&tail->mrt_len);
		__swab32s(&tail->mrt_index);
	}
}

static void mlog_swab_hdr(struct mlog_log_hdr *h)
{
	mlog_swab_rec(&h->mlh_hdr, &h->mlh_tail);
}

static int mlog_vfs_read_header(struct mlog_handle *handle)
{
	struct mtfs_lowerfs *lowerfs = NULL;
	int ret = 0;
	MENTRY();

	MASSERT(sizeof(*handle->mgh_hdr) == MLOG_CHUNK_SIZE);

	lowerfs = handle->mgh_ctxt->moc_lowerfs;

	if (i_size_read(handle->mgh_file->f_dentry->d_inode) == 0) {
		MDEBUG("not reading header from 0-byte log\n");
		ret = MLOG_EEMPTY;
		goto out;
	}

	ret = mlog_vfs_read_blob(lowerfs, handle->mgh_file,
				 handle->mgh_hdr,
				 MLOG_CHUNK_SIZE, 0);
	if (ret) {
		MERROR("error reading log header from %.*s\n",
		       handle->mgh_file->f_dentry->d_name.len,
		       handle->mgh_file->f_dentry->d_name.name);
	} else {
		struct mlog_rec_hdr *mlh_hdr = &handle->mgh_hdr->mlh_hdr;

		if (MLOG_REC_HDR_NEEDS_SWABBING(mlh_hdr)) {
			mlog_swab_hdr(handle->mgh_hdr);
		}

		if (mlh_hdr->mrh_type != MLOG_HDR_MAGIC) {
			MERROR("bad log %.*s header magic: %#x (expected %#x)\n",
			       handle->mgh_file->f_dentry->d_name.len,
			       handle->mgh_file->f_dentry->d_name.name,
			       mlh_hdr->mrh_type, MLOG_HDR_MAGIC);
			ret = -EIO;
		} else if (mlh_hdr->mrh_len != MLOG_CHUNK_SIZE) {
			MERROR("incorrectly sized log %.*s header: %#x "
			       "(expected %#x)\n",
			       handle->mgh_file->f_dentry->d_name.len,
			       handle->mgh_file->f_dentry->d_name.name,
			       mlh_hdr->mrh_len, MLOG_CHUNK_SIZE);
			ret = -EIO;
		}
	}

	handle->mgh_last_idx = handle->mgh_hdr->mlh_tail.mrt_index;
	handle->mgh_file->f_pos = i_size_read(handle->mgh_file->f_dentry->d_inode);

out:
	MRETURN(ret);
}

/* returns negative in on error; 0 if success && reccookie == 0; 1 otherwise */
/* appends if idx == -1, otherwise overwrites record idx. */
static int mlog_vfs_write_rec(struct mlog_handle *loghandle,
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
		/* write_blob adds header and tail to mrh_len. */ 
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

		/* Assumes constant mrh_len */
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
			reccookie->mgc_mgl = loghandle->mgh_id;
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
	/*The caller should make sure only 1 process access the mgh_last_idx,
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
		reccookie->mgc_mgl = loghandle->mgh_id;
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
EXPORT_SYMBOL(mlog_alloc_handle);

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
EXPORT_SYMBOL(mlog_free_handle);

/* Return name length*/
void mlog_id2name(struct mlog_logid *logid, char *name, int length)
{
	snprintf(name, length,
		 "log_0x%llx.0x%x",
		 logid->mgl_oid,
		 logid->mgl_ogen);
}

static struct file *mlog_vfs_open_id(struct mlog_ctxt *ctxt, struct mlog_logid *logid)
{
	struct dentry *dchild = NULL;
	struct file *file = NULL;
	int length = 64;
	char *name = NULL;
	MENTRY();

	MTFS_ALLOC(name, length);
	if (name == NULL) {
		MERROR("not enough memory\n");
		file = ERR_PTR(-ENOMEM);
		goto out;
	}

	mlog_id2name(logid, name, length);

	mutex_lock(&ctxt->moc_dlog->d_inode->i_mutex);
	dchild = lookup_one_len(name,
				ctxt->moc_dlog,
				strlen(name));
	mutex_unlock(&ctxt->moc_dlog->d_inode->i_mutex);
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
	MTFS_FREE(name, length);
out:
	MRETURN(file);
}

static struct file *mlog_vfs_create_open_name(struct mlog_ctxt *ctxt, const char *name)
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

int mlog_vfs_create_new(struct mlog_ctxt *ctxt, struct mlog_logid *logid)
{
	struct dentry *dchild = NULL;
	struct dentry *new_dchild = NULL;
	int ret = 0;
	int length = 64;
	char *name = NULL;
	unsigned int tmpname = random32();
	MENTRY();

	MTFS_ALLOC(name, length);
	if (name == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	sprintf(name, "%u.%u", tmpname, current->pid);

	dchild = mtfs_dchild_create(ctxt->moc_dlog,
				    name,
				    strlen(name),
				    S_IFREG | S_IRWXU,
				    0, NULL, 0);
	if (IS_ERR(dchild)) {
		MERROR("create [%.*s/%s] failed, ret = %d\n",
		       ctxt->moc_dlog->d_name.len,
		       ctxt->moc_dlog->d_name.name,
		       name, PTR_ERR(dchild));
		ret = PTR_ERR(dchild);
		goto out_free_name;
	}

	logid->mgl_oid = dchild->d_inode->i_ino;
	logid->mgl_ogen = dchild->d_inode->i_generation;
	mlog_id2name(logid, name, length);

	mutex_lock(&ctxt->moc_dlog->d_inode->i_mutex);
	new_dchild = lookup_one_len(name, ctxt->moc_dlog, strlen(name));
	if (IS_ERR(new_dchild)) {
		MERROR("getting neg dentry for obj rename: %d\n", ret);
		ret = PTR_ERR(new_dchild);
		goto out_unlock;
	}

	if (new_dchild->d_inode != NULL) {
		MERROR("impossible non-negative obj dentry %s\n",
		       name);
		mutex_unlock(&ctxt->moc_dlog->d_inode->i_mutex);
		ret = -EEXIST;
		goto out_put_new_dchild;
	}

	ret = vfs_rename(ctxt->moc_dlog->d_inode, dchild,
			 ctxt->moc_dlog->d_inode, new_dchild);
	if (ret) {
		MERROR("error renaming new object %s, ret = %d\n",
		       name);
	}
out_put_new_dchild:
	dput(new_dchild);
out_unlock:
	mutex_unlock(&ctxt->moc_dlog->d_inode->i_mutex);
	dput(dchild);
out_free_name:
	MTFS_FREE(name, length);
out:
	MRETURN(ret);
}

static struct file *mlog_vfs_create_open_new(struct mlog_ctxt *ctxt, struct mlog_logid *logid)
{
	struct dentry *dchild = NULL;
	struct file *file = NULL;
	int ret = 0;
	MENTRY();

	ret = mlog_vfs_create_new(ctxt, logid);
	if (ret) {
		MERROR("failed to create new log\n");
		file = ERR_PTR(ret);
		goto out;
	}

	file = mlog_vfs_open_id(ctxt, logid);
	if (IS_ERR(file)) {
		MERROR("failed to open file\n");
		goto out_put_dchild;
	}
	goto out;
out_put_dchild:
	dput(dchild);
out:
	MRETURN(file);
}

/* This is a callback from the mlog_* functions.
 * Assumes caller has already pushed us into the kernel context. */
int mlog_vfs_create(struct mlog_ctxt *ctxt, struct mlog_handle **res,
		    struct mlog_logid *logid, const char *name)
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
			MERROR("failed to open by logid\n");
			ret = PTR_ERR(handle->mgh_file);
			goto out_free_handle;
		}
		/* assign the value of mgh_id for handle directly */
		handle->mgh_id = *logid;
	} else if (name) {
		handle->mgh_file = mlog_vfs_create_open_name(ctxt, name);
		if (IS_ERR(handle->mgh_file)) {
			MERROR("failed to open by name\n");
			ret = PTR_ERR(handle->mgh_file);
			goto out_free_handle;
		}
		handle->mgh_id.mgl_ogr = 1;
		handle->mgh_id.mgl_oid =
			handle->mgh_file->f_dentry->d_inode->i_ino;
		handle->mgh_id.mgl_ogen =
			handle->mgh_file->f_dentry->d_inode->i_generation;
	} else {
		handle->mgh_file = mlog_vfs_create_open_new(ctxt, &handle->mgh_id);
		if (IS_ERR(handle->mgh_file)) {
			MERROR("failed to open new\n");
			ret = PTR_ERR(handle->mgh_file);
			goto out_free_handle;
		}
	}
	handle->mgh_ctxt = ctxt;
	goto out;
out_free_handle:
	mlog_free_handle(handle);
out:
	MRETURN(ret);
}

static int mlog_vfs_close(struct mlog_handle *handle)
{
	int ret = 0;
	MENTRY();

	ret = filp_close(handle->mgh_file, 0);
	if (ret) {
		MERROR("error closing log, ret = %d\n", ret);
	}
	MRETURN(ret);
}

static int mlog_vfs_destroy(struct mlog_handle *handle)
{
	struct dentry *dentry = NULL;
	struct mlog_ctxt *ctxt = NULL;
	int ret = 0;
	struct vfsmount *mnt = NULL;
	MENTRY();

	dentry = handle->mgh_file->f_dentry;
	mnt = handle->mgh_file->f_vfsmnt;
	ctxt = handle->mgh_ctxt;
	MASSERT(dentry->d_parent == ctxt->moc_dlog);

	mntget(mnt);
	dget(dentry);
	ret = mlog_vfs_close(handle);
	if (ret == 0) {
		mutex_lock(&dentry->d_parent->d_inode->i_mutex);
		ret = vfs_unlink(dentry->d_parent->d_inode, dentry);
		mutex_unlock(&dentry->d_parent->d_inode->i_mutex);
	}
	dput(dentry);
	mntput(mnt);

	MRETURN(ret);
}

/* We can skip reading at least as many log blocks as the number of
* minimum sized log records we are skipping.  If it turns out
* that we are not far enough along the log (because the
* actual records are larger than minimum size) we just skip
* some more records. */

static void mlog_skip_over(__u64 *off, int curr, int goal)
{
	if (goal <= curr)
		return;
	*off = (*off + (goal-curr-1) * MLOG_MIN_REC_SIZE) &
		~(MLOG_CHUNK_SIZE - 1);
}

/* sets:
 *  - cur_offset to the furthest point read in the log file
 *  - cur_idx to the log index preceeding cur_offset
 * returns -EIO/-EINVAL on error
 */
static int mlog_vfs_next_block(struct mlog_handle *loghandle, int *cur_idx,
                                int next_idx, __u64 *cur_offset, void *buf,
                                int len)
{
        int ret = 0;
	MENTRY();

	if (len == 0 || len & (MLOG_CHUNK_SIZE - 1)) {
		ret = -EINVAL;
		goto out;
	}

	MDEBUG("looking for log index %u (cur idx %u off %llx)\n",
	       next_idx, *cur_idx, *cur_offset);

	while (*cur_offset < i_size_read(loghandle->mgh_file->f_dentry->d_inode)) {
		struct mlog_rec_hdr *rec;
		struct mlog_rec_tail *tail;
		loff_t ppos;

		mlog_skip_over(cur_offset, *cur_idx, next_idx);

		ppos = *cur_offset;
		ret = mlowerfs_read_record(loghandle->mgh_ctxt->moc_lowerfs,
		                           loghandle->mgh_file, buf, len,
		                           &ppos);
		if (ret) {
			MERROR("Cant read mlog block at log id %llx/%u offset %llx\n",
			       loghandle->mgh_id.mgl_oid,
			       loghandle->mgh_id.mgl_ogen,
			       *cur_offset);
			goto out;
		}

		/* put number of bytes read into rc to make code simpler */
		ret = ppos - *cur_offset;
		*cur_offset = ppos;
		
		if (ret < len) {
			/* signal the end of the valid buffer to mlog_process */
			memset(buf + ret, 0, len - ret);
		}

		if (ret == 0) {
			/* end of file, nothing to do */
			goto out;
		}

		if (ret < sizeof(*tail)) {
			MERROR("Invalid mlog block at log id %llx/%u offset %llx\n",
			       loghandle->mgh_id.mgl_oid,
			       loghandle->mgh_id.mgl_ogen, *cur_offset);
			ret = -EINVAL;
			goto out;
		}

		rec = buf;
		tail = (struct mlog_rec_tail *)((char *)buf + ret -
		                                sizeof(struct mlog_rec_tail));

		if (MLOG_REC_HDR_NEEDS_SWABBING(rec)) {
			mlog_swab_rec(rec, tail);
		}

		*cur_idx = tail->mrt_index;

		/* this shouldn't happen */
		if (tail->mrt_index == 0) {
			MERROR("Invalid mlog tail at log id %llx/%u offset %llx\n",
			       loghandle->mgh_id.mgl_oid,
			       loghandle->mgh_id.mgl_ogen, *cur_offset);
			ret = -EINVAL;
			goto out;
		}
		if (tail->mrt_index < next_idx)
			continue;

		/* sanity check that the start of the new buffer is no farther
		 * than the record that we wanted.  This shouldn't happen. */
		if (rec->mrh_index > next_idx) {
			MERROR("missed desired record? %u > %u\n",
			       rec->mrh_index, next_idx);
			ret = -ENOENT;
			goto out;
		}
		ret = 0;
		goto out;
	}
	ret = -EIO;
out:
	MRETURN(ret);
}

static int mlog_vfs_prev_block(struct mlog_handle *loghandle,
                               int prev_idx, void *buf, int len)
{
	__u64 cur_offset;
	int rc;
	MENTRY();

	if (len == 0 || len & (MLOG_CHUNK_SIZE - 1))
		MRETURN(-EINVAL);

	MDEBUG("looking for log index %u\n", prev_idx);

	cur_offset = MLOG_CHUNK_SIZE;
	mlog_skip_over(&cur_offset, 0, prev_idx);

	while (cur_offset < i_size_read(loghandle->mgh_file->f_dentry->d_inode)) {
		struct mlog_rec_hdr *rec;
		struct mlog_rec_tail *tail;
		loff_t ppos;

		ppos = cur_offset;

		rc = mlowerfs_read_record(loghandle->mgh_ctxt->moc_lowerfs,
	                                  loghandle->mgh_file, buf, len,
		                          &ppos);
		if (rc) {
			MERROR("Cant read mlog block at log id %llx/%u offset %llx\n",
			       loghandle->mgh_id.mgl_oid,
			       loghandle->mgh_id.mgl_ogen,
			       cur_offset);
			MRETURN(rc);
		}

		/* put number of bytes read into rc to make code simpler */
		rc = ppos - cur_offset;
		cur_offset = ppos;

		if (rc == 0) /* end of file, nothing to do */
			MRETURN(0);

		if (rc < sizeof(*tail)) {
			MERROR("Invalid mlog block at log id %llx/%u offset %llx\n",
			       loghandle->mgh_id.mgl_oid,
			       loghandle->mgh_id.mgl_ogen, cur_offset);
			MRETURN(-EINVAL);
		}

		tail = buf + rc - sizeof(struct mlog_rec_tail);

		/* this shouldn't happen */
		if (tail->mrt_index == 0) {
			MERROR("Invalid mlog tail at log id %llx/%u offset%llx\n",
			       loghandle->mgh_id.mgl_oid,
			       loghandle->mgh_id.mgl_ogen, cur_offset);
			MRETURN(-EINVAL);
		}
		if (le32_to_cpu(tail->mrt_index) < prev_idx)
			continue;

		/* sanity check that the start of the new buffer is no farther
		 * than the record that we wanted.  This shouldn't happen. */
		rec = buf;
		if (le32_to_cpu(rec->mrh_index) > prev_idx) {
			MERROR("missed desired record? %u > %u\n",
			       le32_to_cpu(rec->mrh_index), prev_idx);
			MRETURN(-ENOENT);
		}
		MRETURN(0);
	}
	MRETURN(-EIO);
}

/* returns negative on error; 0 if success; 1 if success & log destroyed */
int mlog_cancel_rec(struct mlog_handle *loghandle, int index)
{
	struct mlog_log_hdr *mlh = loghandle->mgh_hdr;
	int ret = 0;
	MENTRY();

	MDEBUG("Canceling %d in log %llx\n",
	       index, loghandle->mgh_id.mgl_oid);

	if (index == 0) {
		MERROR("Can't cancel index 0 which is header\n");
		ret = -EINVAL;
		goto out;
	}

	if (!ext2_clear_bit(index, mlh->mlh_bitmap)) {
		MDEBUG("Catalog index %u already clear?\n", index);
		ret = -ENOENT;
		goto out;
	}

	mlh->mlh_count--;

	if ((mlh->mlh_flags & MLOG_F_ZAP_WHEN_EMPTY) &&
	    (mlh->mlh_count == 1) &&
	    (loghandle->mgh_last_idx == (MLOG_BITMAP_BYTES * 8) - 1)) {
		ret = mlog_destroy(loghandle);
		if (ret) {
			MERROR("Failure destroying log after last cancel: %d\n",
			       ret);
			ext2_set_bit(index, mlh->mlh_bitmap);
			mlh->mlh_count++;
		} else {
			ret = 1;
		}
		goto out;
	}

	ret = mlog_write_rec(loghandle, &mlh->mlh_hdr, NULL, 0, NULL, 0);
	if (ret) {
		MERROR("Failure re-writing header %d\n", ret);
		ext2_set_bit(index, mlh->mlh_bitmap);
		mlh->mlh_count++;
	}
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mlog_cancel_rec);

static inline int mlog_uuid_equals(struct mlog_uuid *u1, struct mlog_uuid *u2)
{
	return strcmp((char *)u1->uuid, (char *)u2->uuid) == 0;
}

int mlog_init_handle(struct mlog_handle *handle, int flags,
		     struct mlog_uuid *uuid)
{
	int ret = 0;
	struct mlog_log_hdr *mlh = NULL;
	MENTRY();

	MASSERT(handle->mgh_hdr == NULL);

	MTFS_ALLOC(mlh, sizeof(*mlh));
	if (mlh == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	handle->mgh_hdr = mlh;
	/* first assign flags to use mlog_client_ops */
	mlh->mlh_flags = flags;
	ret = mlog_read_header(handle);
	if (ret == 0) {
		flags = mlh->mlh_flags;
		if (uuid && !mlog_uuid_equals(uuid, &mlh->mlh_tgtuuid)) {
			MERROR("uuid mismatch: %s/%s\n", (char *)uuid->uuid,
			       (char *)mlh->mlh_tgtuuid.uuid);
			ret = -EEXIST;
		}
		goto out;
	} else if (ret != MLOG_EEMPTY || !flags) {
		/* set a pesudo flag for initialization */
		flags = MLOG_F_IS_CAT;
		goto out;
	}
	ret = 0;

	handle->mgh_last_idx = 0; /* header is record with index 0 */
	mlh->mlh_count = 1;	 /* for the header record */
	mlh->mlh_hdr.mrh_type = MLOG_HDR_MAGIC;
	mlh->mlh_hdr.mrh_len = mlh->mlh_tail.mrt_len = MLOG_CHUNK_SIZE;
	mlh->mlh_hdr.mrh_index = mlh->mlh_tail.mrt_index = 0;
	mlh->mlh_timestamp = get_seconds();
	if (uuid)
		memcpy(&mlh->mlh_tgtuuid, uuid, sizeof(mlh->mlh_tgtuuid));
	mlh->mlh_bitmap_offset = offsetof(typeof(*mlh), mlh_bitmap);
	ext2_set_bit(0, mlh->mlh_bitmap);

out:
	if (flags & MLOG_F_IS_CAT) {
		MTFS_INIT_LIST_HEAD(&handle->u.chd.chd_head);
		mlh->mlh_size = sizeof(struct mlog_logid_rec);
	} else if (flags & MLOG_F_IS_PLAIN) {
		MTFS_INIT_LIST_HEAD(&handle->u.phd.phd_entry);
	} else {
		MERROR("Unknown flags: %#x (Expected %#x or %#x\n",
		       flags, MLOG_F_IS_CAT, MLOG_F_IS_PLAIN);
		MBUG();
	}

	if (ret) {
		MTFS_FREE(mlh, sizeof(*mlh));
		handle->mgh_hdr = NULL;
	}

	MRETURN(ret);
}

static int mlog_process_thread(void *arg)
{
	struct mlog_process_info     *mpi = (struct mlog_process_info *)arg;
	struct mlog_handle           *loghandle = mpi->mpi_loghandle;
	struct mlog_log_hdr          *mlh = loghandle->mgh_hdr;
	struct mlog_process_cat_data *cd  = mpi->mpi_catdata;
	char	                     *buf;
	__u64			      cur_offset = MLOG_CHUNK_SIZE;
	__u64			      last_offset;
	int			      ret = 0, index = 1, last_index;
	int			      saved_index = 0, last_called_index = 0;

	MASSERT(mlh);

	MTFS_ALLOC(buf, MLOG_CHUNK_SIZE);
	if (!buf) {
		mpi->mpi_ret = -ENOMEM;
#ifdef __KERNEL__
		complete(&mpi->mpi_completion);
#endif
		return 0;
	}

	mtfs_daemonize_ctxt("mlog_process_thread");

	if (cd != NULL) {
		last_called_index = cd->mpcd_first_idx;
		index = cd->mpcd_first_idx + 1;
	}
	if (cd != NULL && cd->mpcd_last_idx)
		last_index = cd->mpcd_last_idx;
	else
		last_index = MLOG_BITMAP_BYTES * 8 - 1;

	while (ret == 0) {
		struct mlog_rec_hdr *rec;

		/* skip records not set in bitmap */
		while (index <= last_index &&
		       !ext2_test_bit(index, mlh->mlh_bitmap))
			++index;

		MASSERT(index <= last_index + 1);
		if (index == last_index + 1)
			break;

		MDEBUG("index: %d last_index %d\n",
		       index, last_index);

		/* get the buf with our target record; avoid old garbage */
		last_offset = cur_offset;
		ret = mlog_next_block(loghandle, &saved_index, index,
				     &cur_offset, buf, MLOG_CHUNK_SIZE);
		if (ret) {
			goto out;
		}

		/* NB: when rec->mrh_len is accessed it is already swabbed
		 * since it is used at the "end" of the loop and the rec
		 * swabbing is done at the beginning of the loop. */
		for (rec = (struct mlog_rec_hdr *)buf;
		     (char *)rec < buf + MLOG_CHUNK_SIZE;
		     rec = (struct mlog_rec_hdr *)((char *)rec + rec->mrh_len)){

			MDEBUG("processing rec 0x%p type %#x\n",
			       rec, rec->mrh_type);

			if (MLOG_REC_HDR_NEEDS_SWABBING(rec))
				mlog_swab_rec(rec, NULL);

			MDEBUG("after swabbing, type=%#x idx=%d\n",
			       rec->mrh_type, rec->mrh_index);

			if (rec->mrh_index == 0) {
				/* no more records */
				goto out;
			}

			if (rec->mrh_len == 0 || rec->mrh_len > MLOG_CHUNK_SIZE){
				MERROR("invalid length %d in mlog record for "
				       "index %d/%d\n", rec->mrh_len,
				       rec->mrh_index, index);
				ret = -EINVAL;
				goto out;
			}

			if (rec->mrh_index < index) {
				MDEBUG("skipping mrh_index %d\n",
				       rec->mrh_index);
				continue;
			}

			MDEBUG("mrh_index: %d mrh_len: %d (%d remains)\n",
			       rec->mrh_index, rec->mrh_len,
			       (int)(buf + MLOG_CHUNK_SIZE - (char *)rec));

			loghandle->mgh_cur_idx    = rec->mrh_index;
			loghandle->mgh_cur_offset = (char *)rec - (char *)buf +
						    last_offset;

			/* if set, process the callback on this record */
			if (ext2_test_bit(index, mlh->mlh_bitmap)) {
				ret = mpi->mpi_cb(loghandle, rec,
						 mpi->mpi_cbdata);
				last_called_index = index;
				if (ret == MLOG_PROC_BREAK) {
					MDEBUG("recovery from log: %llx:%x stopped\n",
					       loghandle->mgh_id.mgl_oid,
					       loghandle->mgh_id.mgl_ogen);
					goto out;
				} else if (ret == MLOG_DEL_RECORD) {
					mlog_cancel_rec(loghandle,
							rec->mrh_index);
					ret = 0;
				}
				if (ret) {
					goto out;
				}
			} else {
				MDEBUG("Skipped index %d\n", index);
			}

			/* next record, still in buffer? */
			++index;
			if (index > last_index) {
				goto out;
			}
		}
	}

 out:
	if (cd != NULL)
		cd->mpcd_last_idx = last_called_index;
	if (buf)
		MTFS_FREE(buf, MLOG_CHUNK_SIZE);
	mpi->mpi_ret = ret;
#ifdef __KERNEL__
	complete(&mpi->mpi_completion);
#endif
	MRETURN(ret);
}

int mlog_process(struct mlog_handle *loghandle, mlog_cb_t cb,
                 void *data, void *catdata)
{
	struct mlog_process_info *mpi;
	int                       ret;
	MENTRY();

	MTFS_ALLOC_PTR(mpi);
	if (mpi == NULL) {
		MERROR("cannot alloc pointer\n");
		ret = -ENOMEM;
		goto out;
	}
	mpi->mpi_loghandle = loghandle;
	mpi->mpi_cb        = cb;
	mpi->mpi_cbdata    = data;
	mpi->mpi_catdata   = catdata;

#ifdef __KERNEL__
	init_completion(&mpi->mpi_completion);
	ret = mtfs_create_thread(mlog_process_thread, mpi, CLONE_VM | CLONE_FILES);
	if (ret < 0) {
		MERROR("cannot start thread: %d\n", ret);
		MTFS_FREE_PTR(mpi);
		goto out;
	}
	wait_for_completion(&mpi->mpi_completion);
#else
	mlog_process_thread(mpi);
#endif
	ret = mpi->mpi_ret;
	MTFS_FREE_PTR(mpi);
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mlog_process);

int mlog_reverse_process(struct mlog_handle *loghandle, mlog_cb_t cb,
                         void *data, void *catdata)
{
        struct mlog_log_hdr *mlh = loghandle->mgh_hdr;
	struct mlog_process_cat_data *cd = catdata;
	void *buf = NULL;
	int ret = 0, first_index = 1, index, idx;
	MENTRY();

	MTFS_ALLOC(buf, MLOG_CHUNK_SIZE);
	if (buf == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	if (cd != NULL)
		first_index = cd->mpcd_first_idx + 1;
	if (cd != NULL && cd->mpcd_last_idx)
		index = cd->mpcd_last_idx;
	else
		index = MLOG_BITMAP_BYTES * 8 - 1;

	while (ret == 0) {
		struct mlog_rec_hdr *rec;
		struct mlog_rec_tail *tail;

		/* skip records not set in bitmap */
		while (index >= first_index &&
		       !ext2_test_bit(index, mlh->mlh_bitmap))
			--index;

		MASSERT(index >= first_index - 1);
		if (index == first_index - 1)
			break;

		/* get the buf with our target record; avoid old garbage */
		memset(buf, 0, MLOG_CHUNK_SIZE);
		ret = mlog_prev_block(loghandle, index, buf, MLOG_CHUNK_SIZE);
		if (ret) {
			goto out_free;
		}

		rec = buf;
		idx = le32_to_cpu(rec->mrh_index);
		if (idx < index)
			MDEBUG("index %u : idx %u\n", index, idx);
		while (idx < index) {
			rec = ((void *)rec + le32_to_cpu(rec->mrh_len));
			idx ++;
		}
		tail = (void *)rec + le32_to_cpu(rec->mrh_len) - sizeof(*tail);

		/* process records in buffer, starting where we found one */
		while ((void *)tail > buf) {
			rec = (void *)tail - le32_to_cpu(tail->mrt_len) +
				sizeof(*tail);

			if (rec->mrh_index == 0) {
				/* no more records */
				ret = 0;
				goto out_free;
			}

			/* if set, process the callback on this record */
			if (ext2_test_bit(index, mlh->mlh_bitmap)) {
				ret = cb(loghandle, rec, data);
				if (ret == MLOG_PROC_BREAK) {
					MPRINT("recovery from log: %llx:%x"
					      " stopped\n",
					      loghandle->mgh_id.mgl_oid,
					      loghandle->mgh_id.mgl_ogen);
					goto out_free;
				}
				if (ret) {
					goto out_free;
				}
			}

			/* previous record, still in buffer? */
			--index;
			if (index < first_index) {
				ret = 0;
				goto out_free;
			}
			tail = (void *)rec - sizeof(*tail);
		}
	}

out_free:
	MTFS_FREE(buf, MLOG_CHUNK_SIZE);
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mlog_reverse_process);

struct mlog_operations mlog_vfs_operations = {
	mop_write_rec:      mlog_vfs_write_rec,
	mop_destroy:        mlog_vfs_destroy,
	mop_next_block:     mlog_vfs_next_block,
	mop_prev_block:     mlog_vfs_prev_block,
	mop_create:         mlog_vfs_create,
	mop_close:          mlog_vfs_close,
	mop_read_header:    mlog_vfs_read_header,
	mop_setup:          NULL,
	mop_sync:           NULL,
	mop_cleanup:        NULL,
	mop_add:            NULL,
	mop_cancel:         NULL,
};
EXPORT_SYMBOL(mlog_vfs_operations);
