/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_SPINLOCK_H__
#define __MTFS_SPINLOCK_H__
#if defined (__linux__) && defined(__KERNEL__)
#include <linux/spinlock.h>

typedef spinlock_t mtfs_spinlock_t;

#define mtfs_spin_lock_init(lock)             spin_lock_init(lock)
#define mtfs_spin_lock(lock)                  spin_lock(lock)
#define mtfs_spin_unlock(lock)                spin_unlock(lock)
#define mtfs_spin_trylock(lock)               spin_trylock(lock)
#define mtfs_spin_is_locked(lock)             spin_is_locked(lock)
#define mtfs_spin_destroy(lock)               do {} while(0)

#else /* !((__linux__) && defined(__KERNEL__)) */
#include <pthread.h>

typedef pthread_spinlock_t mtfs_spinlock_t;

#define mtfs_spin_lock_init(lock)             pthread_spin_init(lock, 0)
#define mtfs_spin_lock(lock)                  pthread_spin_lock(lock)
#define mtfs_spin_unlock(lock)                pthread_spin_unlock(lock)
#define mtfs_spin_trylock(lock)               pthread_spin_trylock(lock)
#define mtfs_spin_is_locked(lock)             1
#define mtfs_spin_destroy(lock)               pthread_spin_destroy(lock);

#endif /* !((__linux__) && defined(__KERNEL__)) */

#endif /* __MTFS_SPINLOCK_H__ */
