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
	MTFS_INIT_LIST_HEAD(&chunk->mac_lru_linkage);
	MTFS_INIT_LIST_HEAD(&chunk->mac_linkage);
	chunk->mac_start = start;
	chunk->mac_end = end;
	chunk->mac_size = end - start + 1;
	/* Always refered bucket, unless about to be freed*/
	atomic_set(&chunk->mac_reference, 1);
	init_MUTEX(&bucket->mab_chunk_lock);

out:
	MRETURN(chunk);
}

void masync_chunk_fini(struct masync_chunk *chunk)
{
	MENTRY();

	/* TODO: clean CATALOG handle */
	MASSERT(chunk->mac_bucket == NULL);
	MASSERT(mtfs_list_empty(&chunk->mac_lru_linkage));
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
		mtfs_list_add_tail(&tmp_chunk->mac_linkage,
		                   &bucket->mab_chunks);
	}

	/*
	 * Add reference for the caller
	 * Should holding the lock in case others put it into free list
	 */
	masync_chunk_get(tmp_chunk);
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
                              struct masync_chunk **chunks,
                              __u64 *chunk_num,
                              __u64 start,
                              __u64 end)
{
	struct masync_chunk *chunk = NULL;
	__u64 chunk_start = 0;
	__u64 chunk_end = 0;
	__u64 num = 0;
	__u64 tmp_start = 0;
	__u64 tmp_end = 0;
	__u64 i = 0;
	int ret = 0;
	MENTRY();

	MASSERT(start <= end);

	chunk_start = start / MASYNC_CHUNK_SIZE;
	chunk_end = end / MASYNC_CHUNK_SIZE;
	num = chunk_end - chunk_start + 1;

	MTFS_ALLOC(chunks, sizeof(struct masync_chunk *) * num);
	if (chunks == NULL) {
		MERROR("failed to alloc chunks\n");
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < num; i++) {
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
			i--;
			goto out_clean;
		}
		chunks[i] = chunk;
	}

	*chunk_num = num;
	goto out;
out_clean:
	for (; i >= 0; i--) {
		chunk = chunks[i];
		masync_chunk_clean_dirty(chunk);
		masync_chunk_put(chunk);
	}
	MTFS_FREE(chunks, sizeof(struct masync_chunk *) * num);
out:
	MRETURN(ret);
}
