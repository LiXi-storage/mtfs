/*
 * Copied from Lustre-2.x
 */
#include <linux/namei.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/random.h>
#include <mtfs_log.h>
#include <mtfs_file.h>

struct mlog_mini_rec {
        struct mlog_rec_hdr     mmr_hdr;
        struct mlog_rec_tail    mmr_tail;
} __attribute__((packed));

static int mlog_verify_handle(char *test, struct mlog_handle *mlh, int num_recs)
{
	int i;
	int last_idx = 0;
	int active_recs = 0;
	int ret = 0;

	for (i = 0; i < MLOG_BITMAP_BYTES * 8; i++) {
		if (ext2_test_bit(i, mlh->mgh_hdr->mlh_bitmap)) {
			last_idx = i;
			active_recs++;
		}
	}

	if (active_recs != num_recs) {
		MERROR("%s: expected %d active recs after write, found %d\n",
		       test, num_recs, active_recs);
		ret = -ERANGE;
		goto out;
	}

	if (mlh->mgh_hdr->mlh_count != num_recs) {
		MERROR("%s: handle->count is %d, expected %d after write\n",
		       test, mlh->mgh_hdr->mlh_count, num_recs);
		ret = -ERANGE;
		goto out;
	}

	if (mlh->mgh_last_idx < last_idx) {
		MERROR("%s: handle->last_idx is %d, expected %d after write\n",
		       test, mlh->mgh_last_idx, last_idx);
		ret = -ERANGE;
		goto out;
	}

out:
	MRETURN(ret);
}

static struct mlog_uuid mlog_test_uuid = { .uuid = "test_uuid" };

/* Test named-log create/open, close */
static int mlog_test_1(struct mlog_ctxt *ctxt,
		       const char *name)
{
	struct mlog_handle *mlh = NULL;
	int ret = 0;
	MENTRY();

	MPRINT("1a: create a log with name: %s\n", name);
	MASSERT(ctxt);

	ret = mlog_create(ctxt, &mlh, NULL, name);
	if (ret) {
		MERROR("1a: mlog_create with name %s failed: %d\n", name, ret);
		goto out;
	}

	mlog_init_handle(mlh, MLOG_F_IS_PLAIN, &mlog_test_uuid);

	ret = mlog_verify_handle("1", mlh, 1);
	if (ret) {
		MERROR("handle error, ret = %d\n", ret);
	}

	ret = mlog_close(mlh);
	if (ret) {
		MERROR("1b: close log %s failed: %d\n", name, ret);
	}
out:
	MRETURN(ret);
}

/* Test named-log reopen; returns opened log on success */
int mlog_test_2(struct mlog_ctxt *ctxt, char *name,
		struct mlog_handle **mlh)
{
	struct mlog_handle *loghandle = NULL;
	struct mlog_logid logid;
	int ret = 0;
	MENTRY();

	MPRINT("2a: re-open a log with name: %s\n", name);
	ret = mlog_create(ctxt, mlh, NULL, name);
	if (ret) {
		MERROR("2a: re-open log with name %s failed: %d\n", name, ret);
		goto out;
	}
	mlog_init_handle(*mlh, MLOG_F_IS_PLAIN, &mlog_test_uuid);

	if ((ret = mlog_verify_handle("2", *mlh, 1))) {
		MERROR("handle error, ret = %d\n", ret);
		goto out_close_mlh;
	}

	MPRINT("2b: create a log without specified NAME and LOGID\n");
	ret = mlog_create(ctxt, &loghandle, NULL, NULL);
	if (ret) {
		MERROR("2b: create log failed, ret = %d\n", ret);
		goto out_close_mlh;
	}
	mlog_init_handle(loghandle, MLOG_F_IS_PLAIN, &mlog_test_uuid);

	logid = loghandle->mgh_id;
	mlog_close(loghandle);

	MPRINT("2b: re-open the log by LOGID\n");
	ret = mlog_create(ctxt, &loghandle, &logid, NULL);
	if (ret) {
		MERROR("2b: re-open log by LOGID failed\n");
		goto out;
	}
	mlog_init_handle(loghandle, MLOG_F_IS_PLAIN, &mlog_test_uuid);

	MPRINT("2b: destroy this log\n");
	ret = mlog_destroy(loghandle);
	if (ret) {
		MERROR("2b: destroy log failed\n");
		goto out_free_handle;
	}
	mlog_free_handle(loghandle);
	goto out;
out_free_handle:
	mlog_free_handle(loghandle);
out_close_mlh:
	mlog_close(*mlh);
	*mlh = NULL;
out:
	MRETURN(ret);
}

/* Test record writing, single and in bulk */
static int mlog_test_3(struct mlog_ctxt *ctxt, struct mlog_handle *mlh)
{
	struct mlog_logid_rec mid;
	int ret = 0;
	int i = 0;
	int num_recs = 1; /* 1 for the header */
	MENTRY();

	mid.mid_hdr.mrh_len = mid.mid_tail.mrt_len = sizeof(mid);
	mid.mid_hdr.mrh_type = MLOG_LOGID_MAGIC;

	MPRINT("3a: write one logid record\n");
	ret = mlog_write_rec(mlh,  &mid.mid_hdr, NULL, 0, NULL, -1);
	num_recs++;
	if (ret) {
		MERROR("3a: write one log record failed: %d\n", ret);
		goto out;
	}

	if ((ret = mlog_verify_handle("3a", mlh, num_recs))) {
		MERROR("handle error, ret = %d\n", ret);
		goto out;
	}

	MPRINT("3b: write 10 cfg log records with 8 bytes bufs\n");
	for (i = 0; i < 10; i++) {
		struct mlog_rec_hdr hdr;
		char buf[8];
		hdr.mrh_len = 8;
		hdr.mrh_type = MLOG_LOGID_MAGIC;
		memset(buf, 0, sizeof buf);
		ret = mlog_write_rec(mlh, &hdr, NULL, 0, buf, -1);
		if (ret) {
			MERROR("3b: write 10 records failed at #%d: %d\n",
			       i + 1, ret);
			goto out;
		}
		num_recs++;
		if ((ret = mlog_verify_handle("3c", mlh, num_recs))) {
			MERROR("handle error, ret = %d\n", ret);
			goto out;
		}
	}

	if ((ret = mlog_verify_handle("3b", mlh, num_recs))) {
		goto out;
	}

	MPRINT("3c: write 1000 more log records\n");
	for (i = 0; i < 1000; i++) {
		ret = mlog_write_rec(mlh, &mid.mid_hdr, NULL, 0, NULL, -1);
		if (ret) {
			MERROR("3c: write 1000 records failed at #%d: %d\n",
			       i + 1, ret);
			goto out;
		}
		num_recs++;
		if ((ret = mlog_verify_handle("3b", mlh, num_recs))) {
			MERROR("handle error, ret = %d\n", ret);
			goto out;
		}
	}

	if ((ret = mlog_verify_handle("3c", mlh, num_recs))){
		MERROR("handle error, ret = %d\n", ret);
		goto out;
	}
	
	MPRINT("3d: write log more than BITMAP_SIZE, return -ENOSPC\n");
	for (i = 0; i < MLOG_BITMAP_SIZE(mlh->mgh_hdr) + 1; i++) {
		struct mlog_rec_hdr hdr;
		char buf_even[24];
		char buf_odd[32];

		memset(buf_odd, 0, sizeof buf_odd);
		memset(buf_even, 0, sizeof buf_even);
		if ((i % 2) == 0) {
			hdr.mrh_len = 24;
			hdr.mrh_type = MLOG_LOGID_MAGIC;
			ret = mlog_write_rec(mlh, &hdr, NULL, 0, buf_even, -1);
		} else {
			hdr.mrh_len = 32;
			hdr.mrh_type = MLOG_LOGID_MAGIC;
			ret = mlog_write_rec(mlh, &hdr, NULL, 0, buf_odd, -1);
		}
		if (ret) {
			if (ret == -ENOSPC) {
				break;
			} else {
				MERROR("3c: write recs failed at #%d: %d\n",
				       i + 1, ret);
				goto out;
			}
		}
		num_recs++;
	}
	if (ret != -ENOSPC) {
		MPRINT("3d: write record more than BITMAP size!\n");
		ret = -EINVAL;
	}
	if ((ret = mlog_verify_handle("3d", mlh, num_recs))) {
		MERROR("handle error, ret = %d\n", ret);
		goto out;
	}
out:
	MRETURN(ret);
}

/* Test catalogue additions */
static int mlog_test_4(struct mlog_ctxt *ctxt, char *name, struct mlog_logid *cat_logid)
{
	struct mlog_handle *cath;
	int ret, i, buflen;
	struct mlog_mini_rec mmr;
	struct mlog_cookie cookie;
	int num_recs = 0;
	char *buf;
	struct mlog_rec_hdr rec;
	MENTRY();

	mmr.mmr_hdr.mrh_len = mmr.mmr_tail.mrt_len = MLOG_MIN_REC_SIZE;
	mmr.mmr_hdr.mrh_type = 0xf00f00;

	sprintf(name, "%x", random32());
	MPRINT("4a: create a catalog log with name: %s\n", name);
	ret = mlog_create(ctxt, &cath, NULL, name);
	if (ret) {
		MERROR("1a: mlog_create with name %s failed: %d\n", name, ret);
		goto out;
	}

	mlog_init_handle(cath, MLOG_F_IS_CAT, &mlog_test_uuid);
	num_recs++;
	*cat_logid = cath->mgh_id;

	MPRINT("4b: write 1 record into the catalog\n");
	ret = mlog_cat_add_rec(cath, &mmr.mmr_hdr, &cookie, NULL);
	if (ret != 1) {
		MERROR("4b: write 1 catalog record failed at: %d\n", ret);
		goto out_close;
	}
	num_recs++;
	if ((ret = mlog_verify_handle("4b", cath, 2))) {
		goto out_close;
	}

	if ((ret = mlog_verify_handle("4b", cath->u.chd.chd_current_log, num_recs))) {
		goto out_close;
	}

	MPRINT("4c: cancel 1 log record\n");
	ret = mlog_cat_cancel_records(cath, 1, &cookie);
	if (ret) {
		MERROR("4c: cancel 1 catalog based record failed: %d\n", ret);
		goto out_close;
	}
	num_recs--;

	if ((ret = mlog_verify_handle("4c", cath->u.chd.chd_current_log, num_recs))) {
		goto out_close;
	}

	MPRINT("4d: write 40,000 more log records\n");
	for (i = 0; i < 40000; i++) {
		ret = mlog_cat_add_rec(cath, &mmr.mmr_hdr, NULL, NULL);
		if (ret) {
			MERROR("4d: write 40000 records failed at #%d: %d\n",
			       i + 1, ret);
			goto out_close;
		}
		num_recs++;
	}

	MPRINT("4e: add 5 large records, one record per block\n");
	buflen = MLOG_CHUNK_SIZE - sizeof(struct mlog_rec_hdr)
			- sizeof(struct mlog_rec_tail);
	MTFS_ALLOC(buf, buflen);
	if (buf == NULL) {
		ret = -ENOMEM;
		goto out_close;
	}

	for (i = 0; i < 5; i++) {
		rec.mrh_len = buflen;
		rec.mrh_type = MLOG_LOGID_MAGIC;
		ret = mlog_cat_add_rec(cath, &rec, NULL, buf);
		if (ret) {
			MERROR("4e: write 5 records failed at #%d: %d\n",
			       i + 1, ret);
			goto out_free;
		}
		num_recs++;
	}

out_free:
	MTFS_FREE(buf, buflen);
out_close:
	mlog_cat_put(cath);
	if (ret) {
		MERROR("1b: close log %s failed: %d\n", name, ret);
	}
 out:
	MRETURN(ret);
}

static int cat_print_cb(struct mlog_handle *mlh, struct mlog_rec_hdr *rec,
                        void *data)
{
	struct mlog_logid_rec *mir = (struct mlog_logid_rec *)rec;
	MENTRY();

	if (rec->mrh_type != MLOG_LOGID_MAGIC) {
		MERROR("invalid record in catalog\n");
		MRETURN(-EINVAL);
	}

	MPRINT("seeing record at index %d - %llx:%x in log %llx\n",
	       rec->mrh_index, mir->mid_id.mgl_oid,
	       mir->mid_id.mgl_ogen, mlh->mgh_id.mgl_oid);
        MRETURN(0);
}

static int plain_print_cb(struct mlog_handle *mlh, struct mlog_rec_hdr *rec,
                          void *data)
{
	MENTRY();

	if (!(mlh->mgh_hdr->mlh_flags & MLOG_F_IS_PLAIN)) {
		MERROR("log is not plain\n");
		MRETURN(-EINVAL);
	}

	MPRINT("seeing record at index %d in log %llx\n",
	       rec->mrh_index, mlh->mgh_id.mgl_oid);
	MRETURN(0);
}

/* Test log and catalogue processing */
static int mlog_test_5(struct mlog_ctxt *ctxt,
                       char *name,
                       struct mlog_logid *cat_logid)
{
	int ret = 0;
	struct mlog_handle *mlh = NULL;
	MENTRY();

	MPRINT("5a: re-open catalog by nam\n");
	ret = mlog_create(ctxt, &mlh, NULL, name);
        if (ret) {
		MERROR("5a: mlog_create with name failed: %d\n", ret);
		goto out;
	}

	if (!mlog_logid_equals(&mlh->mgh_id, cat_logid)) {
		ret = -EEXIST;
		MERROR("5a: logid different\n");
		goto out_close;
	}

	mlog_init_handle(mlh, MLOG_F_IS_CAT, &mlog_test_uuid);

        MPRINT("5b: print the catalog entries.. we expect 2\n");
        ret = mlog_process(mlh, cat_print_cb, "test 5", NULL);
        if (ret) {
                MERROR("5b: process with cat_print_cb failed: %d\n", ret);
        }

	ret = mlog_cat_destroy(mlh);
	if (ret) {
		MERROR("failed to destroy handle, ret = %d\n", ret);
	} else {
		mlog_free_handle(mlh); 
	}
	goto out;
out_close:
	ret = mlog_cat_put(mlh);
	if (ret) {
		MERROR("failed to close hangle, ret = %d", ret);
	}
out:
	MRETURN(ret);
}

/* Test client api; open log by name and process */
static int mlog_test_6(struct mlog_ctxt *ctxt, char *name)
{
	struct mlog_handle *mlh = NULL;
	int ret = 0;

	MPRINT("6a: re-open log %s using client API\n", name);
        ret = mlog_create(ctxt, &mlh, NULL, name);
	if (ret) {
		MERROR("6: mlog_create failed %d\n", ret);
		goto out;
        }

	ret = mlog_init_handle(mlh, MLOG_F_IS_PLAIN, NULL);
	if (ret) {
		MERROR("6: mlog_init_handle failed %d\n", ret);
		goto out_close;
	}

	ret = mlog_process(mlh, plain_print_cb, NULL, NULL);
        if (ret) {
                MERROR("6: mlog_process failed %d\n", ret);
                goto out_close;
        }

	ret = mlog_reverse_process(mlh, plain_print_cb, NULL, NULL);
	if (ret) {
		MERROR("6: mlog_reverse_process failed %d\n", ret);
		goto out_close;
	}

out_close:
	ret = mlog_close(mlh);
out:
	if (ret) {
		MERROR("6: mlog_close failed: ret = %d\n", ret);
	}

	MRETURN(ret);
}

static int mlog_test_7(struct mlog_ctxt *ctxt)
{
	struct mlog_handle *mlh = NULL;
	struct mlog_logid_rec mid;
	char name[10];
	int ret = 0;
	MENTRY();

	sprintf(name, "%x", random32());
	MPRINT("7: create a log with name: %s\n", name);
	MASSERT(ctxt);

	ret = mlog_create(ctxt, &mlh, NULL, name);
	if (ret) {
		MERROR("7: mlog_create with name %s failed: %d\n", name, ret);
		goto out;
	}
	mlog_init_handle(mlh, MLOG_F_IS_PLAIN, &mlog_test_uuid);

	mid.mid_hdr.mrh_len = mid.mid_tail.mrt_len = sizeof(mid);
	mid.mid_hdr.mrh_type = MLOG_LOGID_MAGIC;
	ret = mlog_write_rec(mlh,  &mid.mid_hdr, NULL, 0, NULL, -1);
	if (ret) {
		MERROR("7: write one log record failed: %d\n", ret);
		goto out;
	}

	ret = mlog_destroy(mlh);
	if (ret) {
		MERROR("7: mlog_destroy failed: %d\n", ret);
	} else {
		mlog_free_handle(mlh); 
	}
out:
	MRETURN(ret);
}

int mlog_run_tests(struct mlog_ctxt *ctxt)
{
	int ret = 0;
	char *name = "log_test";
	struct mlog_handle *mlh = NULL;
	char cat_name[64];
	struct mlog_logid cat_logid;
	MENTRY();

	MERROR("running mlog tests\n");
	ret = mlog_test_1(ctxt, name);
	if (ret) {
		MERROR("test 1 failed\n");
		goto out;
	}

	ret = mlog_test_2(ctxt, name, &mlh);
	if (ret) {
		MERROR("test 2 failed\n");
		goto out;
	}

	ret = mlog_test_3(ctxt, mlh);
	if (ret) {
		MERROR("test 3 failed\n");
		goto out_destroy;
	}

	ret = mlog_test_4(ctxt, cat_name, &cat_logid);
	if (ret) {
		MERROR("test 4 failed\n");
		goto out_destroy;
	}

	ret = mlog_test_5(ctxt, cat_name, &cat_logid);
	if (ret) {
		MERROR("test 5 failed\n");
		goto out_destroy;
	}

	ret = mlog_test_6(ctxt, name);
	if (ret) {
		MERROR("test 5 failed\n");
		goto out_destroy;
	}

	ret = mlog_test_7(ctxt);
	if (ret) {
		MERROR("test 7 failed\n");
		goto out_destroy;
	}

	MERROR("mlog tests finished\n");
out_destroy:
	ret = mlog_destroy(mlh);
	if (ret) {
		MERROR("failed to destroy handle, ret = %d\n", ret);
	} else {
		mlog_free_handle(mlh); 
	}
out:
	MRETURN(ret);
}
EXPORT_SYMBOL(mlog_run_tests);
