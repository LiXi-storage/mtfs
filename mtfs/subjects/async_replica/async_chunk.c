/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <mtfs_async.h>
#include "async_chunk_internal.h"

struct masync_chunk *masync_chunk_init(struct masync_bucket *bucket,
                                       __u64 start,
                                       __u64 end)
{
	struct masync_chunk *chunk = NULL;
	MENTRY();

	MASSERT(start + MASYNC_CHUNK_SIZE - 1 == end);

	MTFS_SLAB_ALLOC_PTR(chunk, mtfs_async_chunk_cache);
	if (chunk == NULL) {
		MERROR("failed to create interval, not enough memory\n");
		goto out;
	}

	chunk->mac_bucket = bucket;
	chunk->mac_info = bucket->mab_info;
	MTFS_INIT_LIST_HEAD(&chunk->mac_linkage);
	mtfs_spin_lock_init(&chunk->mac_bucket_lock);
	chunk->mac_start = start;
	chunk->mac_end = end;
	chunk->mac_size = end - start + 1;
	atomic_set(&chunk->mac_reference, 0);
	init_MUTEX(&chunk->mac_lock);
out:
	MRETURN(chunk);
}

void masync_chunk_fini(struct masync_chunk *chunk)
{
	MENTRY();

	MASSERT(!chunk->mac_flags & MASYNC_CHUNK_FLAG_DRITY);
	MASSERT(mtfs_list_empty(&chunk->mac_linkage));
	MASSERT(atomic_read(&chunk->mac_reference) == 0);
	MTFS_SLAB_FREE_PTR(chunk, mtfs_async_chunk_cache);

	_MRETURN();
}

struct masync_chunk *_masync_chunk_find(struct masync_bucket *bucket,
                                        __u64 start,
                                        __u64 end)
{
	struct masync_chunk *chunk = NULL;
	struct masync_chunk *find = NULL;
	MENTRY();

	mtfs_list_for_each_entry(chunk,
	                         &bucket->mab_chunks,
	                         mac_linkage) {
		if (chunk->mac_start == start) {
			MASSERT(chunk->mac_end == end);
			find = chunk;
			break;
		}
	}
	MRETURN(find);
}

struct masync_chunk *masync_chunk_find(struct masync_bucket *bucket,
                                       __u64 start,
                                       __u64 end)
{
	struct masync_chunk *chunk = NULL;
	MENTRY();

	down(&bucket->mab_chunk_lock);
	chunk = _masync_chunk_find(bucket, start, end);
	up(&bucket->mab_chunk_lock);

	MRETURN(chunk);
}

int masync_chunk_mark_dirty(struct masync_chunk *chunk)
{
	int ret = 0;
	MENTRY();

	down(&chunk->mac_lock);
	if (!(chunk->mac_flags & MASYNC_CHUNK_FLAG_DRITY)) {
		/* TODO: write to CATALOG */
		chunk->mac_flags |= MASYNC_CHUNK_FLAG_DRITY;
	}
	up(&chunk->mac_lock);

	MRETURN(ret);
}

int masync_chunk_clean_dirty(struct masync_chunk *chunk)
{
	int ret = 0;
	MENTRY();

	down(&chunk->mac_lock);
	if (chunk->mac_flags & MASYNC_CHUNK_FLAG_DRITY) {
		/* TODO: clean handle of CATALOG */
		chunk->mac_flags &= ~MASYNC_CHUNK_FLAG_DRITY;
	}
	up(&chunk->mac_lock);

	MRETURN(ret);
}

void masync_chunk_cleanup(struct masync_bucket *bucket)
{
	struct masync_chunk *chunk = NULL;
	struct masync_chunk *n = NULL;

	down(&bucket->mab_chunk_lock);
	mtfs_list_for_each_entry_safe(chunk,
	                              n,
	                              &bucket->mab_chunks,
	                              mac_linkage) {
		/* This is slow and can be moved out of lock */
		masync_chunk_clean_dirty(chunk);
		mtfs_list_del_init(&chunk->mac_linkage);
		mtfs_spin_lock(&chunk->mac_bucket_lock);
		chunk->mac_bucket = NULL;
		mtfs_spin_unlock(&chunk->mac_bucket_lock);
	}
	up(&bucket->mab_chunk_lock);
}

void masync_chunk_get(struct masync_chunk *chunk)
{
	MENTRY();

	atomic_inc(&chunk->mac_reference);

	_MRETURN();
}

static int masync_chunk_try_unlink(struct masync_chunk *chunk)
{
	struct masync_bucket *bucket = NULL;
	int cleanup = 0;
	int retry = 0;

	mtfs_spin_lock(&chunk->mac_bucket_lock);
	bucket = chunk->mac_bucket;
	if (bucket == NULL) {
		MASSERT(mtfs_list_empty(&chunk->mac_linkage));
		masync_chunk_fini(chunk);
	} else if (!down_trylock(&bucket->mab_chunk_lock)) {
		if (atomic_read(&chunk->mac_reference) == 0) {
			cleanup = 1;
			chunk->mac_bucket = NULL;
			mtfs_list_del_init(&chunk->mac_linkage);
		}
		up(&bucket->mab_chunk_lock);
	} else {
		retry = 1;
	}
	mtfs_spin_unlock(&chunk->mac_bucket_lock);

	if (cleanup) {
		masync_chunk_clean_dirty(chunk);
		masync_chunk_fini(chunk);
	}
	return retry;
}

void masync_chunk_put(struct masync_chunk *chunk)
{
	MENTRY();

	if (atomic_dec_and_test(&chunk->mac_reference)) {
		while (1) {
			if (masync_chunk_try_unlink(chunk) == 0) {
				break;
			}
			udelay(5);
		}
	}

	_MRETURN();
}

/*
 * If failed to alloc, chunk will be set to NULL
 */
int masync_chunk_add(struct masync_bucket *bucket,
                     struct masync_chunk **chunk,
                     __u64 start,
                     __u64 end)
{
	struct masync_chunk *tmp_chunk = NULL;
	int ret = 0;
	MENTRY();

	MASSERT(start + MASYNC_CHUNK_SIZE - 1 == end);

	down(&bucket->mab_chunk_lock);
	tmp_chunk = _masync_chunk_find(bucket,
	                               start,
	                               end);
	if (tmp_chunk == NULL) {
		tmp_chunk = masync_chunk_init(bucket, start, end);
		if (tmp_chunk == NULL) {
			MERROR("failed to init chunk\n");
			up(&bucket->mab_chunk_lock);
			*chunk = NULL;
			ret = -ENOMEM;
			goto out;
		}
		masync_chunk_get(tmp_chunk);
		mtfs_list_add_tail(&tmp_chunk->mac_linkage,
		                   &bucket->mab_chunks);
	}
	up(&bucket->mab_chunk_lock);

	*chunk = tmp_chunk;
	ret = masync_chunk_mark_dirty(tmp_chunk);
	if (ret) {
		MERROR("failed to mark dirty, ret = %d\n", ret);
	}
out:
	MRETURN(ret);
}

int masync_chunk_add_interval(struct masync_bucket *bucket,
                              mtfs_list_t *chunks,
                              __u64 start,
                              __u64 end)
{
	struct masync_chunk *chunk = NULL;
	struct masync_chunk_linkage *linkage = NULL;
	struct masync_chunk_linkage *head = NULL;
	__u64 chunk_start = 0;
	__u64 chunk_end = 0;
	__u64 num = 0;
	__u64 tmp_start = 0;
	__u64 tmp_end = 0;
	__u64 i = 0;
	int ret = 0;
	MENTRY();

	MASSERT(mtfs_list_empty(chunks));
	MASSERT(start <= end);

	chunk_start = start / MASYNC_CHUNK_SIZE;
	chunk_end = end / MASYNC_CHUNK_SIZE;
	num = chunk_end - chunk_start + 1;

	for (i = 0; i < num; i++) {
		MTFS_ALLOC_PTR(linkage);
		if (linkage == NULL) {
			goto out_clean;
		}

	     	tmp_start = (chunk_start + i) * MASYNC_CHUNK_SIZE;
		tmp_end = tmp_start + MASYNC_CHUNK_SIZE - 1;
		ret = masync_chunk_add(bucket,
		                       &chunk,
		                       tmp_start,
		                       tmp_end);
		if (ret) {
			if (chunk) {
				masync_chunk_put(chunk);
			}
			MERROR("failed to add chunk [%llu, %llu], ret = %d\n",
			       tmp_start, tmp_end, ret);
			MTFS_FREE_PTR(linkage);
			goto out_clean;
		}

		linkage->macl_chunk = chunk;
		mtfs_list_add_tail(&linkage->macl_linkage,
		                   chunks);
	}

	goto out;
out_clean:
	mtfs_list_for_each_entry_safe(linkage, head, chunks, macl_linkage) {
		mtfs_list_del(&linkage->macl_linkage);
		chunk = linkage->macl_chunk;
		masync_chunk_put(chunk);
		MTFS_FREE_PTR(linkage);
	}
out:
	MRETURN(ret);
}

/* Put the the chunks of old extent to new extent */
void masync_chunk_merge_extent(struct masync_extent *old,
                               struct masync_extent *new)
{
	struct masync_chunk_linkage *old_linkage = NULL;
	struct masync_chunk_linkage *n = NULL;
	struct masync_chunk_linkage *new_linkage = NULL;
	int found = 0;
	MENTRY();

	mtfs_list_for_each_entry_safe(old_linkage,
		                      n,
		                      &old->mae_chunks,
		                      macl_linkage) {
		found = 0;
		mtfs_list_for_each_entry(new_linkage,
		                         &new->mae_chunks,
		                         macl_linkage) {
			if (old_linkage->macl_chunk ==
			    new_linkage->macl_chunk) {
				found = 1;
				break;
			}
		}
		if (!found) {
			mtfs_list_del_init(&old_linkage->macl_linkage);
			mtfs_list_add_tail(&old_linkage->macl_linkage,
			                   &new->mae_chunks);
		}
	}
}

/* Put the the chunks of old extent to new extent */
void masync_chunk_truncate_extent(struct masync_extent *old,
                                  struct masync_extent *new)
{
	struct masync_chunk_linkage *old_linkage = NULL;
	struct masync_chunk_linkage *new_linkage = NULL;
	int found = 0;
	MENTRY();

	mtfs_list_for_each_entry(old_linkage,
		                 &old->mae_chunks,
		                 macl_linkage) {
		found = 0;
		mtfs_list_for_each_entry(new_linkage,
		                         &new->mae_chunks,
		                         macl_linkage) {
			if (old_linkage->macl_chunk ==
			    new_linkage->macl_chunk) {
				found = 1;
				break;
			}
		}
		if (!found) {
			mtfs_list_del_init(&old_linkage->macl_linkage);
			mtfs_list_add_tail(&old_linkage->macl_linkage,
			                   &new->mae_chunks);
		}
	}
}
