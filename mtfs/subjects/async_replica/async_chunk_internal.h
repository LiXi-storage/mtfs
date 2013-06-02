/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_ASYNC_CHUNK_INTERNAL_H__
#define __MTFS_ASYNC_CHUNK_INTERNAL_H__

#include <mtfs_async.h>

void masync_bulk_get(struct masync_bulk *async_bulk);
void masync_bulk_put(struct masync_bulk *async_bulk);
struct masync_bulk *masync_bulk_init(struct masync_bucket *bucket,
                                     __u64 start,
                                     __u64 end);
void masync_bulk_fini(struct masync_bulk *async_bulk);

static inline void masync_chunk_add_to_lru(struct masync_chunk *chunk)
{
	struct msubject_async_info *info = NULL;
	MENTRY();

	info = chunk->mae_bucket->mab_info;

	mtfs_spin_lock(&info->msai_lru_chunk_lock);
	MASSERT(mtfs_list_empty(&chunk->mac_lru_linkage));
	mtfs_list_add_tail(&chunk->mac_lru_linkage, &info->msai_lru_chunks);
	atomic_inc(&info->msai_lru_chunk_number);
	mtfs_spin_unlock(&info->msai_lru_chunk_lock);

	masync_chunk_get(chunk);

	_MRETURN();
}

static inline void masync_chunk_touch_in_lru(struct masync_chunk *chunk)
{
	struct msubject_async_info *info = NULL;
	MENTRY();

	info = chunk->mae_bucket->mab_info;

	mtfs_spin_lock(&info->msai_lru_lock);
	MASSERT(!mtfs_list_empty(&chunk->mac_lru_linkage));
	mtfs_list_move_tail(&chunk->mac_lru_linkage, &info->msai_lru_chunks);
	mtfs_spin_unlock(&info->msai_lru_lock);

	_MRETURN();
}

/*
 * Reutrn 1 if succeeded, 0 if not in the LRU list.
 */
static inline int masync_chunk_remove_from_lru_nonlock(struct masync_chunk *chunk)
{
	struct msubject_async_info *info = NULL;
	int ret = 0;
	MENTRY();

	info = chunk->mae_info;

	if (!mtfs_list_empty(&chunk->mac_lru_linkage)) {
		mtfs_list_del_init(&chunk->mac_lru_linkage);
		atomic_dec(&info->msai_lru_chunk_number);
		ret = 1;
	}

	MRETURN(ret);
}

/*
 * Before calling this function, masync_chunk_get() first.
 */
static inline void masync_chunk_remove_from_lru(struct masync_chunk *chunk)
{
	struct msubject_async_info *info = NULL;
	int need_put = 0;
	MENTRY();

	info = chunk->mae_info;

	mtfs_spin_lock(&info->msai_lru_lock);
	need_put = masync_chunk_remove_from_lru_nonlock(chunk);
	mtfs_spin_unlock(&info->msai_lru_lock);

	if (need_put) {
		masync_chunk_put(chunk);
	}

	_MRETURN();
}


#endif /* __MTFS_ASYNC_CHUNK_INTERNAL_H__ */
