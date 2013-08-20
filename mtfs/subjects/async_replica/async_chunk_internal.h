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
void masync_chunk_get(struct masync_chunk *chunk);
void masync_chunk_put(struct masync_chunk *chunk);
void masync_chunk_cleanup(struct masync_bucket *bucket);
#endif /* __MTFS_ASYNC_CHUNK_INTERNAL_H__ */
