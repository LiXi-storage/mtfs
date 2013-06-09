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
	MTFS_INIT_LIST_HEAD(&chunk->mac_idle_linkage);
	MTFS_INIT_LIST_HEAD(&chunk->mac_linkage);
	mtfs_spin_lock_init(&chunk->mac_bucket_lock);
	chunk->mac_start = start;
	chunk->mac_end = end;
	chunk->mac_size = end - start + 1;
	/* Always refered bucket, unless about to be freed*/
	atomic_set(&chunk->mac_reference, 1);
	init_MUTEX(&chunk->mac_lock);

out:
	MRETURN(chunk);
}

void masync_chunk_fini(struct masync_chunk *chunk)
{
	MENTRY();

	MASSERT(!chunk->mac_flags & MASYNC_CHUNK_FLAG_DRITY);
	MASSERT(chunk->mac_bucket == NULL);
	MASSERT(mtfs_list_empty(&chunk->mac_idle_linkage));
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

void masync_chunk_get(struct masync_chunk *chunk)
{
	struct msubject_async_info *info = NULL;
	int reference = 0;
	MENTRY();

	info = chunk->mac_info;
	reference = atomic_inc_return(&chunk->mac_reference);
	if (reference == 1) {
		mtfs_spin_lock(&info->msai_idle_chunk_lock);
		/* Might be removed by others */
		if (!mtfs_list_empty(&chunk->mac_idle_linkage)) {
			mtfs_list_del_init(&chunk->mac_idle_linkage);
			atomic_dec(&info->msai_idle_chunk_number);
		}
		mtfs_spin_unlock(&info->msai_idle_chunk_lock);
	}

	_MRETURN();
}

void masync_chunk_put(struct masync_chunk *chunk)
{
	struct msubject_async_info *info = NULL;
	int reference = 0;
	int is_free = 0;
	MENTRY();

	info = chunk->mac_info;
	mtfs_spin_lock(&chunk->mac_bucket_lock);
	is_free = (chunk->mac_bucket == NULL);
	mtfs_spin_unlock(&chunk->mac_bucket_lock);

	reference = atomic_dec_return(&chunk->mac_reference);
	if (reference == 1) {
		/* Not used by bucket any more */
		MASSERT(is_free);
		masync_chunk_clean_dirty(chunk);
		masync_chunk_fini(chunk);
	} else if (reference == 2 && (!is_free)) {
		mtfs_spin_lock(&info->msai_idle_chunk_lock);
		/* Might be inserted by others */
		if (mtfs_list_empty(&chunk->mac_idle_linkage)) {
			mtfs_list_add_tail(&chunk->mac_idle_linkage,
			                   &info->msai_idle_chunks);
			atomic_inc(&info->msai_idle_chunk_number);
		}
		mtfs_spin_unlock(&info->msai_idle_chunk_lock);
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
                              mtfs_list_t *chunks,
                              __u64 *chunk_num,
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

	*chunk_num = num;
	goto out;
out_clean:
	mtfs_list_for_each_entry_safe(linkage, head, chunks, macl_linkage) {
		chunk = linkage->macl_chunk;
		masync_chunk_put(chunk);
	}
out:
	MRETURN(ret);
}
