/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_ASYNC_CHUNK_INTERNAL_H__
#define __MTFS_ASYNC_CHUNK_INTERNAL_H__

#include <mtfs_async.h>

struct masync_chunk *masync_chunk_init(struct masync_bucket *bucket,
                                      __u64 start,
                                      __u64 end);
void masync_chunk_fini(struct masync_chunk *chunk);


static inline void masync_chunk_get(struct masync_chunk *chunk)
{
	MENTRY();

	atomic_inc(&chunk->mac_reference);

	_MRETURN();
}

static inline void masync_chunk_put(struct masync_chunk *chunk)
{
	MENTRY();

	if (atomic_dec_and_test(&chunk->mac_reference)) {
		/* Not in bucket any more */
		masync_chunk_fini(chunk);
	}

	_MRETURN();
}

static inline void masync_chunk_add_to_lru(struct masync_chunk *chunk)
{
	struct msubject_async_info *info = NULL;
	MENTRY();

	info = chunk->mac_info;

	mtfs_spin_lock(&info->msai_lru_chunk_lock);
	MASSERT(mtfs_list_empty(&chunk->mac_lru_linkage));
	mtfs_list_add_tail(&chunk->mac_lru_linkage, &info->msai_lru_chunks);
	atomic_inc(&info->msai_lru_chunk_number);
	mtfs_spin_unlock(&info->msai_lru_chunk_lock);

	_MRETURN();
}

static inline void masync_chunk_touch_in_lru(struct masync_chunk *chunk)
{
	struct msubject_async_info *info = NULL;
	MENTRY();

	info = chunk->mac_info;

	mtfs_spin_lock(&info->msai_lru_lock);
	MASSERT(!mtfs_list_empty(&chunk->mac_lru_linkage));
	mtfs_list_move_tail(&chunk->mac_lru_linkage, &info->msai_lru_chunks);
	mtfs_spin_unlock(&info->msai_lru_lock);

	_MRETURN();
}

/*
 * Before calling this function, masync_chunk_get() first.
 */
static inline void masync_chunk_remove_from_lru(struct masync_chunk *chunk)
{
	struct msubject_async_info *info = NULL;
	MENTRY();

	info = chunk->mac_info;

	mtfs_spin_lock(&info->msai_lru_lock);
	MASSERT(!mtfs_list_empty(&chunk->mac_lru_linkage));
	mtfs_list_del_init(&chunk->mac_lru_linkage);
	atomic_dec(&info->msai_lru_chunk_number);
	mtfs_spin_unlock(&info->msai_lru_lock);

	masync_chunk_put(chunk);

	_MRETURN();
}

#endif /* __MTFS_ASYNC_CHUNK_INTERNAL_H__ */
