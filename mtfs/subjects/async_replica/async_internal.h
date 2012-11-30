/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_ASYNC_INTERNAL_H__
#define __MTFS_ASYNC_INTERNAL_H__

static inline void masync_info_lock_init(struct msubject_async_info *info)
{
	init_rwsem(&info->msai_lock);
}

static inline void masync_info_read_lock(struct msubject_async_info *info)
{
	down_read(&info->msai_lock);
}

static inline void masync_info_read_unlock(struct msubject_async_info *info)
{
	up_read(&info->msai_lock);
}

static inline void masync_info_write_lock(struct msubject_async_info *info)
{
	down_write(&info->msai_lock);
}

static inline void masync_info_write_unlock(struct msubject_async_info *info)
{
	up_write(&info->msai_lock);
}
void masync_service_wakeup(void);

extern atomic_t masync_nr;
#endif /* __MTFS_ASYNC_INTERNAL_H__ */
