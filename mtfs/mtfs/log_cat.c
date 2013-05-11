/*
 * Copied from Lustre-2.x
 */
#include <linux/namei.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/random.h>
#include <mtfs_lowerfs.h>
#include <mtfs_log.h>
#include <mtfs_file.h>

/* Create a new log handle and add it to the open list.
 * This log handle will be closed when all of the records in it are removed.
 *
 * Assumes caller has already pushed us into the kernel context and is locking.
 */
static struct mlog_handle *mlog_cat_new_log(struct mlog_handle *cathandle)
{
	struct mlog_handle *loghandle;
	struct mlog_log_hdr *mlh;
	struct mlog_logid_rec rec = { { 0 }, };
	int ret, index, bitmap_size;
	MENTRY();

	mlh = cathandle->mgh_hdr;
	bitmap_size = MLOG_BITMAP_SIZE(mlh);

	index = (cathandle->mgh_last_idx + 1) % bitmap_size;

	/* maximum number of available slots in catlog is bitmap_size - 2 */
	if (mlh->mlh_cat_idx == index) {
		MERROR("no free catalog slots for log\n");
		loghandle = ERR_PTR(-ENOSPC);
		goto out;
	}

	ret = mlog_create(cathandle->mgh_ctxt, &loghandle, NULL, NULL);
	if (ret) {
		MERROR("failed to create log, ret = %d\n", ret);
		loghandle = ERR_PTR(ret);
		goto out;
	}

	ret = mlog_init_handle(loghandle,
			      MLOG_F_IS_PLAIN | MLOG_F_ZAP_WHEN_EMPTY,
			      &cathandle->mgh_hdr->mlh_tgtuuid);
	if (ret) {
		MERROR("failed to init handle, ret = %d\n", ret);
		goto out_destroy;
	}
	if (index == 0) {
		index = 1;
	}

	if (ext2_set_bit(index, mlh->mlh_bitmap)) {
		MERROR("index %u already set in log bitmap?\n",
		       index);
		MBUG(); /* should never happen */
	}

	cathandle->mgh_last_idx = index;
	mlh->mlh_count++;
	mlh->mlh_tail.mrt_index = index;

	MDEBUG("new recovery log %llx:%x for index %u of catalog %llx\n",
	       loghandle->mgh_id.mgl_oid, loghandle->mgh_id.mgl_ogen,
	       index, cathandle->mgh_id.mgl_oid);
	/* build the record for this log in the catalog */
	rec.mid_hdr.mrh_len = sizeof(rec);
	rec.mid_hdr.mrh_index = index;
	rec.mid_hdr.mrh_type = MLOG_LOGID_MAGIC;
	rec.mid_id = loghandle->mgh_id;
	rec.mid_tail.mrt_len = sizeof(rec);
	rec.mid_tail.mrt_index = index;

	/* update the catalog: header and record */
	ret = mlog_write_rec(cathandle, &rec.mid_hdr,
			    &loghandle->u.phd.phd_cookie, 1, NULL, index);
	if (ret < 0) {
		MERROR("failed to write record, ret = %d\n", ret);
		goto out_destroy;
	}

	loghandle->mgh_hdr->mlh_cat_idx = index;
	cathandle->u.chd.chd_current_log = loghandle;
	MASSERT(list_empty(&loghandle->u.phd.phd_entry));
	list_add_tail(&loghandle->u.phd.phd_entry, &cathandle->u.chd.chd_head);

out_destroy:
	if (ret < 0)
		mlog_destroy(loghandle);
out:
	MRETURN(loghandle);
}

/* Return the currently active log handle.  If the current log handle doesn't
 * have enough space left for the current record, start a new one.
 *
 * If reclen is 0, we only want to know what the currently active log is,
 * otherwise we get a lock on this log so nobody can steal our space.
 *
 * Assumes caller has already pushed us into the kernel context and is locking.
 *
 * NOTE: loghandle is write-locked upon successful return
 */
static struct mlog_handle *mlog_cat_current_log(struct mlog_handle *cathandle,
						int create)
{
	struct mlog_handle *loghandle = NULL;
	MENTRY();

	down_read(&cathandle->mgh_lock);
	loghandle = cathandle->u.chd.chd_current_log;
	if (loghandle) {
		struct mlog_log_hdr *mlh = loghandle->mgh_hdr;
		down_write(&loghandle->mgh_lock);
		if (loghandle->mgh_last_idx < MLOG_BITMAP_SIZE(mlh) - 1) {
			up_read(&cathandle->mgh_lock);
			goto out;
		} else {
			up_write(&loghandle->mgh_lock);
		}
	}
	if (!create) {
		if (loghandle)
			down_write(&loghandle->mgh_lock);
		up_read(&cathandle->mgh_lock);
		goto out;
	}
	up_read(&cathandle->mgh_lock);

	/* time to create new log */

	/* first, we have to make sure the state hasn't changed */
	down_write(&cathandle->mgh_lock);
	loghandle = cathandle->u.chd.chd_current_log;
	if (loghandle) {
		struct mlog_log_hdr *mlh = loghandle->mgh_hdr;
		down_write(&loghandle->mgh_lock);
		if (loghandle->mgh_last_idx < MLOG_BITMAP_SIZE(mlh) - 1) {
			up_write(&cathandle->mgh_lock);
			goto out;
		} else {
			up_write(&loghandle->mgh_lock);
		}
	}

	MDEBUG("creating new log\n");
	loghandle = mlog_cat_new_log(cathandle);
	if (!IS_ERR(loghandle))
		down_write(&loghandle->mgh_lock);
	up_write(&cathandle->mgh_lock);
out:
	MRETURN(loghandle);
}

/* Open an existent log handle and add it to the open list.
 * This log handle will be closed when all of the records in it are removed.
 *
 * Assumes caller has already pushed us into the kernel context and is locking.
 * We return a lock on the handle to ensure nobody yanks it from us.
 */
int mlog_cat_id2handle(struct mlog_handle *cathandle, struct mlog_handle **res,
		       struct mlog_logid *logid)
{
	struct mlog_handle *loghandle;
	int ret = 0;
	MENTRY();

	if (cathandle == NULL) {
		ret = -EBADF;
		goto out;
	}

	list_for_each_entry(loghandle, &cathandle->u.chd.chd_head,
			    u.phd.phd_entry) {
		struct mlog_logid *cgl = &loghandle->mgh_id;
		if (cgl->mgl_oid == logid->mgl_oid) {
			if (cgl->mgl_ogen != logid->mgl_ogen) {
				MERROR("log %llx generation %x != %x\n",
				       logid->mgl_oid, cgl->mgl_ogen,
				       logid->mgl_ogen);
				continue;
			}
			loghandle->u.phd.phd_cat_handle = cathandle;
			goto out;
		}
	}

	ret = mlog_create(cathandle->mgh_ctxt, &loghandle, logid, NULL);
	if (ret) {
		MERROR("error opening log id %llx:%x: ret %d\n",
		       logid->mgl_oid, logid->mgl_ogen, ret);
	} else {
		ret = mlog_init_handle(loghandle, MLOG_F_IS_PLAIN, NULL);
		if (!ret) {
			list_add(&loghandle->u.phd.phd_entry,
				 &cathandle->u.chd.chd_head);
		}
	}
	if (!ret) {
		loghandle->u.phd.phd_cat_handle = cathandle;
		loghandle->u.phd.phd_cookie.mgc_mgl = cathandle->mgh_id;
		loghandle->u.phd.phd_cookie.mgc_index = 
			loghandle->mgh_hdr->mlh_cat_idx;
	}

out:
	*res = loghandle;
	MRETURN(ret);
}

int mlog_cat_put(struct mlog_handle *cathandle)
{
	struct mlog_handle *loghandle, *n;
	int ret;
	MENTRY();

	list_for_each_entry_safe(loghandle, n, &cathandle->u.chd.chd_head,
				 u.phd.phd_entry) {
		int err = mlog_close(loghandle);
		if (err)
			MERROR("error closing loghandle\n");
	}
	ret = mlog_close(cathandle);
	MRETURN(ret);
}
EXPORT_SYMBOL(mlog_cat_put);

/* Add a single record to the recovery log(s) using a catalog
 * Returns as mlog_write_record
 *
 * Assumes caller has already pushed us into the kernel context.
 */
int mlog_cat_add_rec(struct mlog_handle *cathandle, struct mlog_rec_hdr *rec,
		     struct mlog_cookie *reccookie, void *buf)
{
	struct mlog_handle *loghandle = NULL;
	int ret = 0;
	MENTRY();

	MASSERT(rec->mrh_len <= MLOG_CHUNK_SIZE);
	loghandle = mlog_cat_current_log(cathandle, 1);
	if (IS_ERR(loghandle)) {
		ret = PTR_ERR(loghandle);
		goto out;
	}

	/* loghandle is already locked by mlog_cat_current_log() for us */
	ret = mlog_write_rec(loghandle, rec, reccookie, 1, buf, -1);
	up_write(&loghandle->mgh_lock);
	if (ret == -ENOSPC) {
		/* to create a new plain log */
		loghandle = mlog_cat_current_log(cathandle, 1);
		if (IS_ERR(loghandle)) {
			ret = PTR_ERR(loghandle);
			goto out;
		}
		ret = mlog_write_rec(loghandle, rec, reccookie, 1, buf, -1);
		up_write(&loghandle->mgh_lock);
	}
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mlog_cat_add_rec);

static int mlog_cat_set_first_idx(struct mlog_handle *cathandle, int index)
{
	struct mlog_log_hdr *mlh = cathandle->mgh_hdr;
	int i, bitmap_size, idx;
	MENTRY();

	bitmap_size = MLOG_BITMAP_SIZE(mlh);
	if (mlh->mlh_cat_idx == (index - 1)) {
		idx = mlh->mlh_cat_idx + 1;
		mlh->mlh_cat_idx = idx;
		if (idx == cathandle->mgh_last_idx)
			goto out;
		for (i = (index + 1) % bitmap_size;
		     i != cathandle->mgh_last_idx;
		     i = (i + 1) % bitmap_size) {
			if (!ext2_test_bit(i, mlh->mlh_bitmap)) {
				idx = mlh->mlh_cat_idx + 1;
				mlh->mlh_cat_idx = idx;
			} else if (i == 0) {
				mlh->mlh_cat_idx = 0;
			} else {
				break;
			}
		}
out:
		MDEBUG("set catlog %llx first idx %u\n",
		       cathandle->mgh_id.mgl_oid, mlh->mlh_cat_idx);
	}

	MRETURN(0);
}

/* For each cookie in the cookie array, we clear the log in-use bit and either:
 * - the log is empty, so mark it free in the catalog header and delete it
 * - the log is not empty, just write out the log header
 *
 * The cookies may be in different log files, so we need to get new logs
 * each time.
 *
 * Assumes caller has already pushed us into the kernel context.
 */
int mlog_cat_cancel_records(struct mlog_handle *cathandle, int count,
			    struct mlog_cookie *cookies)
{
	int i, index, ret = 0;
	MENTRY();

	down_write(&cathandle->mgh_lock);
	for (i = 0; i < count; i++, cookies++) {
		struct mlog_handle *loghandle;
		struct mlog_logid *mgl = &cookies->mgc_mgl;

		ret = mlog_cat_id2handle(cathandle, &loghandle, mgl);
		if (ret) {
			MERROR("Cannot find log %llx\n", mgl->mgl_oid);
			break;
		}

		down_write(&loghandle->mgh_lock);
		ret = mlog_cancel_rec(loghandle, cookies->mgc_index);
		up_write(&loghandle->mgh_lock);

		if (ret == 1) {	  /* log has been destroyed */
			index = loghandle->u.phd.phd_cookie.mgc_index;
			if (cathandle->u.chd.chd_current_log == loghandle)
				cathandle->u.chd.chd_current_log = NULL;
			mlog_free_handle(loghandle);

			MASSERT(index);
			mlog_cat_set_first_idx(cathandle, index);
			ret = mlog_cancel_rec(cathandle, index);
			if (ret == 0)
				MDEBUG("cancel plain log at index %u"
				       " of catalog %llx\n",
				       index, cathandle->mgh_id.mgl_oid);
		}
	}
	up_write(&cathandle->mgh_lock);

	MRETURN(ret);
}
EXPORT_SYMBOL(mlog_cat_cancel_records);

int mlog_cat_destroy(struct mlog_handle *cathandle)
{
	struct mlog_handle *loghandle, *n;
	int ret;
	MENTRY();

	list_for_each_entry_safe(loghandle, n, &cathandle->u.chd.chd_head,
	                         u.phd.phd_entry) {
		ret = mlog_destroy(loghandle);
		if (ret) {
			MERROR("error closing loghandle\n");
		} else {
			mlog_free_handle(loghandle); 
		}
	}
	ret = mlog_destroy(cathandle);
	MRETURN(ret);
}
EXPORT_SYMBOL(mlog_cat_destroy);

int mlog_cat_process_cb(struct mlog_handle *cat_mlh, struct mlog_rec_hdr *rec,
                        void *data)
{
	struct mlog_process_data *d = data;
	struct mlog_logid_rec *mir = (struct mlog_logid_rec *)rec;
	struct mlog_handle *mlh;
	int ret = 0;
	MENTRY();

	if (rec->mrh_type != MLOG_LOGID_MAGIC) {
		MERROR("invalid record in catalog\n");
		ret = -EINVAL;
		goto out;
	}
	MDEBUG("processing log %llx:%x at index %u of catalog %llx\n",
	       mir->mid_id.mgl_oid, mir->mid_id.mgl_ogen,
	       rec->mrh_index, cat_mlh->mgh_id.mgl_oid);

	ret = mlog_cat_id2handle(cat_mlh, &mlh, &mir->mid_id);
	if (ret) {
		MERROR("Cannot find handle for log %llx\n",
		       mir->mid_id.mgl_oid);
		goto out;
	}

	ret = mlog_process(mlh, d->mpd_cb, d->mpd_data, NULL);
out:
	MRETURN(ret);
}

int mlog_cat_process(struct mlog_handle *cat_mlh, mlog_cb_t cb, void *data)
{
	struct mlog_process_data d;
	struct mlog_process_cat_data cd;
	struct mlog_log_hdr *mlh = cat_mlh->mgh_hdr;
	int ret = 0;
	MENTRY();

	MASSERT(mlh->mlh_flags & MLOG_F_IS_CAT);
	d.mpd_data = data;
	d.mpd_cb = cb;

	if (mlh->mlh_cat_idx > cat_mlh->mgh_last_idx) {
		MPRINT("catlog %llx crosses index zero\n",
		       cat_mlh->mgh_id.mgl_oid);

		cd.mpcd_first_idx = mlh->mlh_cat_idx;
		cd.mpcd_last_idx = 0;
		ret = mlog_process(cat_mlh, mlog_cat_process_cb, &d, &cd);
		if (ret != 0) {
			goto out;
		}

		cd.mpcd_first_idx = 0;
		cd.mpcd_last_idx = cat_mlh->mgh_last_idx;
		ret = mlog_process(cat_mlh, mlog_cat_process_cb, &d, &cd);
	} else {
		ret = mlog_process(cat_mlh, mlog_cat_process_cb, &d, NULL);
	}

out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mlog_cat_process);
