/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_LOCK_H__
#define __HRFS_LOCK_H__
#include <libcfs/linux-lock.h>
#include "mtfs_internal.h"
static inline void d_info_rwlock_init(dentry_t *dentry)
{
	HASSERT(dentry);
	HASSERT(mtfs_d2info(dentry));
	return rwlock_init(&mtfs_d2info(dentry)->rwlock);
}

static inline void d_info_read_lock(dentry_t *dentry)
{
	HASSERT(dentry);
	HASSERT(mtfs_d2info(dentry));
	return read_lock(&mtfs_d2info(dentry)->rwlock);
}

static inline void d_info_read_unlock(dentry_t *dentry)
{
	HASSERT(dentry);
	HASSERT(mtfs_d2info(dentry));
	return read_unlock(&mtfs_d2info(dentry)->rwlock);
}

static inline void d_info_write_lock(dentry_t *dentry)
{
	HASSERT(dentry);
	HASSERT(mtfs_d2info(dentry));
	return write_lock_bh(&mtfs_d2info(dentry)->rwlock);
}

static inline void d_info_write_unlock(dentry_t *dentry)
{
	HASSERT(dentry);
	HASSERT(mtfs_d2info(dentry));
	return write_unlock_bh(&mtfs_d2info(dentry)->rwlock);
}


#endif /* __HRFS_LOCK_H__ */