/*
 * Copied from Lustre-2.x
 */
#include <linux/namei.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/random.h>
#include <mtfs_log.h>
#include <mtfs_file.h>

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
		MERROR("7: llog_create with name %s failed: %d\n", name, ret);
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
