/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_SERVICE_INTERNAL_H__
#define __MTFS_SERVICE_INTERNAL_H__
#include <mtfs_service.h>

enum {
	SVC_STOPPED  = 1 << 0,
	SVC_STOPPING = 1 << 1,
	SVC_STARTING = 1 << 2,
	SVC_RUNNING  = 1 << 3,
	SVC_EVENT    = 1 << 4,
	SVC_SIGNAL   = 1 << 5,
};

static inline int thread_is_stopped(struct mservice_thread *thread)
{
	return !!(thread->t_flags & SVC_STOPPED);
}

static inline int thread_is_stopping(struct mservice_thread *thread)
{
	return !!(thread->t_flags & SVC_STOPPING);
}

static inline int thread_is_starting(struct mservice_thread *thread)
{
	return !!(thread->t_flags & SVC_STARTING);
}

static inline int thread_is_running(struct mservice_thread *thread)
{
	return !!(thread->t_flags & SVC_RUNNING);
}

static inline int thread_is_event(struct mservice_thread *thread)
{
	return !!(thread->t_flags & SVC_EVENT);
}

static inline int thread_is_signal(struct mservice_thread *thread)
{
	return !!(thread->t_flags & SVC_SIGNAL);
}

static inline void thread_clear_flags(struct mservice_thread *thread, __u32 flags)
{
	thread->t_flags &= ~flags;
}

static inline void thread_set_flags(struct mservice_thread *thread, __u32 flags)
{
	 thread->t_flags = flags;
}

static inline void thread_add_flags(struct mservice_thread *thread, __u32 flags)
{
	thread->t_flags |= flags;
}

static inline int thread_test_and_clear_flags(struct mservice_thread *thread,
                                              __u32 flags)
{
	if (thread->t_flags & flags) {
		thread->t_flags &= ~flags;
		return 1;
	}
	return 0;
}

static inline int
mservice_thread_stopping(struct mservice_thread *thread)
{
        return thread_is_stopping(thread) ||
               thread->t_svc->srv_is_stopping;
}
#endif /* __MTFS_SERVICE_INTERNAL_H__ */
