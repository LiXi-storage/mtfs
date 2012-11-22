/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_MEMORY_H__
#define __MTFS_MEMORY_H__
#include "debug.h"

#define POISON(ptr, c, s) memset(ptr, c, s)

#if defined (__linux__) && defined(__KERNEL__)

#ifdef MEMORY_DEBUG
#include <asm/atomic.h>

extern atomic64_t mtfs_kmemory_used;
extern atomic64_t mtfs_kmemory_used_max;

/*
 * mtfs_kmemory_used_max may be not accurate 
 * because of concurrent freeing and allocing.
 */
#define mtfs_kmem_inc(ptr, size)                                              \
do {                                                                          \
    __u64 used = atomic64_read(&mtfs_kmemory_used);                           \
    atomic64_add(size, &mtfs_kmemory_used);                                   \
    used += size;                                                             \
    if (used > atomic64_read(&mtfs_kmemory_used_max)) {                       \
        atomic64_set(&mtfs_kmemory_used_max, used);                           \
    }                                                                         \
} while (0)

#define mtfs_kmem_dec(ptr, size)                                              \
do {                                                                          \
    atomic64_sub(size, &mtfs_kmemory_used);                                   \
} while (0)
#else /* !MEMORY_DEBUG */
#define mtfs_kmem_inc(ptr, size) do {} while (0)
#define mtfs_kmem_dec(ptr, size) do {} while (0)
#endif /* !MEMORY_DEBUG */

#include <linux/slab.h>
#include <linux/hardirq.h>

#define MTFS_ALLOC_GFP(ptr, size, gfp_mask)                                   \
do {                                                                          \
    (ptr) = kmalloc(size, (gfp_mask));                                        \
    if (likely((ptr) != NULL)) {                                              \
        mtfs_kmem_inc((ptr), (size));                                         \
        memset((ptr), 0, size);                                               \
        MDEBUG_MEM("mtfs_kmalloced '" #ptr "': %d at %p.\n",                  \
                   (int)(size), (ptr));                                       \
    }                                                                         \
} while (0)

#define MTFS_ALLOC(ptr, size) MTFS_ALLOC_GFP(ptr, size, GFP_KERNEL)
#define MTFS_ALLOC_PTR(ptr)   MTFS_ALLOC(ptr, sizeof *(ptr))

#define MTFS_FREE(ptr, size)                                                  \
do {                                                                          \
    int _size = (size);                                                       \
    MASSERT(ptr);                                                             \
    MDEBUG_MEM("mtfs_kfreed '" #ptr "': %d at %p.\n",                         \
               _size, (ptr));                                                 \
    POISON((ptr), 0x5a, _size);                                               \
    kfree(ptr);                                                               \
    mtfs_kmem_dec((ptr), _size);                                              \
    (ptr) = NULL;                                                             \
} while (0)

#define MTFS_FREE_PTR(ptr) MTFS_FREE(ptr, sizeof *(ptr))

#define MTFS_SLAB_ALLOC_GFP(ptr, slab, size, gfp_mask)                        \
do {                                                                          \
    MASSERT(!in_interrupt());                                                 \
    (ptr) = kmem_cache_alloc(slab, (gfp_mask));                               \
    if (likely((ptr) != NULL)) {                                              \
        mtfs_kmem_inc((ptr), (size));                                         \
        memset((ptr), 0, size);                                               \
        MDEBUG_MEM("mtfs_slab-alloced '" #ptr "': %d at %p.\n",               \
                   (int)(size), (ptr));                                       \
    }                                                                         \
} while (0)

#define MTFS_SLAB_ALLOC(ptr, slab, size) MTFS_SLAB_ALLOC_GFP(ptr, slab, size, GFP_KERNEL)
#define MTFS_SLAB_ALLOC_PTR(ptr, slab) MTFS_SLAB_ALLOC(ptr, slab, sizeof *(ptr))

#define MTFS_SLAB_FREE(ptr, slab, size)                                       \
do {                                                                          \
    MASSERT(ptr);                                                             \
    MDEBUG_MEM("mtfs_slab-freed '" #ptr "': %d at %p.\n",                     \
               (int)(size), ptr);                                             \
    POISON(ptr, 0x5a, size);                                                  \
    kmem_cache_free(slab, ptr);                                               \
    mtfs_kmem_dec((ptr), size);                                               \
    (ptr) = NULL;                                                             \
} while (0)

#define MTFS_SLAB_FREE_PTR(ptr, slab) MTFS_SLAB_FREE(ptr, slab, sizeof *(ptr))

#define MTFS_STRDUP(dest, src)                                                \
do {                                                                          \
    MTFS_ALLOC(dest, strlen(src) + 1);                                        \
    if (dest) {                                                               \
        strcpy(dest, src);                                                    \
    }                                                                         \
} while (0)

#define MTFS_FREE_STR(str)                                                    \
do {                                                                          \
    MTFS_FREE(str, strlen(str) + 1);                                          \
} while (0)

extern struct kmem_cache *mtfs_file_info_cache;
extern struct kmem_cache *mtfs_dentry_info_cache;
extern struct kmem_cache *mtfs_inode_info_cache;
extern struct kmem_cache *mtfs_sb_info_cache;
extern struct kmem_cache *mtfs_device_cache;
extern struct kmem_cache *mtfs_oplist_cache;
extern struct kmem_cache *mtfs_lock_cache;
extern struct kmem_cache *mtfs_interval_cache;
extern struct kmem_cache *mtfs_io_cache;
extern struct kmem_cache *mtfs_config_cache;

#else /* !((__linux__) && defined(__KERNEL__)) */

#include <stdlib.h>
#include <string.h> /* For memset */
/* No need to detect memory_leak, since we have valgrind, yeah! */

#define MTFS_ALLOC(ptr, size)                                                 \
do {                                                                          \
    (ptr) = malloc(size);                                                     \
    if ((ptr) != NULL) {                                                      \
        memset(ptr, 0, size);                                                 \
    }                                                                         \
} while (0)

#define MTFS_ALLOC_PTR(ptr) MTFS_ALLOC(ptr, sizeof *(ptr))

#define MTFS_FREE(ptr, size)                                                  \
do {                                                                          \
    MASSERT(ptr);                                                             \
    POISON(ptr, 0x5a, size);                                                  \
    free(ptr);                                                                \
    (ptr) = NULL;                                                             \
} while (0)

#define MTFS_FREE_PTR(ptr) MTFS_FREE(ptr, sizeof *(ptr))

#define MTFS_STRDUP(buff, str)                                                \
do {                                                                          \
    buff = strdup(str);                                                       \
} while (0)

#define MTFS_FREE_STR(str)                                                    \
do {                                                                          \
    free(str);                                                                \
} while (0)

#define MTFS_SLAB_ALLOC(ptr, slab, size) MTFS_ALLOC(ptr, size)
#define MTFS_SLAB_ALLOC_PTR(ptr, slab)   MTFS_ALLOC(ptr, sizeof *(ptr))

#define MTFS_SLAB_FREE(ptr, slab, size)  MTFS_FREE(ptr, size)
#define MTFS_SLAB_FREE_PTR(ptr, slab)    MTFS_FREE(ptr, sizeof *(ptr))
#endif /* !((__linux__) && defined(__KERNEL__)) */
#endif /* __MTFS_MEMORY_H__ */
