#include <mtfs_async.h>

struct masync_chunk *masync_chunk_init(struct masync_bucket *bucket,
                                       __u64 start,
                                       __u64 end)
{
	struct masync_chunk *async_chunk = NULL;
	MENTRY();

	MTFS_SLAB_ALLOC_PTR(async_chunk, mtfs_async_chunk_cache);
	if (async_chunk == NULL) {
		MERROR("failed to create interval, not enough memory\n");
		goto out;
	}

	async_chunk->mac_bucket = bucket;
	async_chunk->mac_info = bucket->mab_info;
	MTFS_INIT_LIST_HEAD(&async_chunk->mac_lru_linkage);
	MTFS_INIT_LIST_HEAD(&async_chunk->mac_linkage);
	async_chunk->mac_start = start;
	async_chunk->mac_end = end;
	atomic_set(&async_chunk->mac_reference, 0);

out:
	MRETURN(async_chunk);
}

void masync_chunk_fini(struct masync_chunk *async_chunk)
{
	MENTRY();

	MASSERT(async_chunk->mac_bucket == NULL);
	MASSERT(mtfs_list_empty(&async_chunk->mac_lru_linkage));
	MASSERT(mtfs_list_empty(&async_chunk->mac_linkage));
	MASSERT(atomic_read(&async_chunk->mac_reference) == 0);
	MTFS_SLAB_FREE_PTR(async_chunk, mtfs_async_chunk_cache);

	_MRETURN();
}

void masync_chunk_get(struct masync_chunk *async_chunk)
{
	MENTRY();

	atomic_inc(&async_chunk->mac_reference);

	_MRETURN();
}

void masync_chunk_put(struct masync_chunk *async_chunk)
{
	MENTRY();

	if (atomic_dec_and_test(&async_chunk->mac_reference)) {
		masync_chunk_fini(async_chunk);
	}

	_MRETURN();
}
