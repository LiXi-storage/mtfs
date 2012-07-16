/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_SERVICE_INTERNAL_H__
#define __MTFS_SERVICE_INTERNAL_H__

struct mtfs_service {
	mtfs_list_t       srv_list;
	char             *srv_name;
	char             *srv_thread_name;
	mtfs_list_t       srv_threads;
	int               srv_threads_running;
	int               srv_threads_starting;
	int               srv_threads_max;
	unsigned          srv_threads_next_id;
	unsigned          srv_is_stopping:1;
	spinlock_t        srv_lock;
};

/**
 * Definition of server service thread structure
 */
struct mtfs_thread {
	mtfs_list_t          t_link;
	void                *t_data;
	__u32                t_flags;
	unsigned int         t_id;
	pid_t                t_pid;
	struct lc_watchdog  *t_watchdog;
	struct mtfs_service *t_svc;
	wait_queue_head_t    t_ctl_waitq; 
};

struct mtfs_svc_data {
        char *name;
        struct mtfs_service *svc;
        struct mtfs_thread *thread;
};

enum {
	SVC_STOPPED  = 1 << 0,
	SVC_STOPPING = 1 << 1,
	SVC_STARTING = 1 << 2,
	SVC_RUNNING  = 1 << 3,
	SVC_EVENT    = 1 << 4,
	SVC_SIGNAL   = 1 << 5,
};

static inline int thread_is_stopped(struct mtfs_thread *thread)
{
	return !!(thread->t_flags & SVC_STOPPED);
}

static inline int thread_is_stopping(struct mtfs_thread *thread)
{
	return !!(thread->t_flags & SVC_STOPPING);
}

static inline int thread_is_starting(struct mtfs_thread *thread)
{
	return !!(thread->t_flags & SVC_STARTING);
}

static inline int thread_is_running(struct mtfs_thread *thread)
{
	return !!(thread->t_flags & SVC_RUNNING);
}

static inline int thread_is_event(struct mtfs_thread *thread)
{
	return !!(thread->t_flags & SVC_EVENT);
}

static inline int thread_is_signal(struct mtfs_thread *thread)
{
	return !!(thread->t_flags & SVC_SIGNAL);
}

#endif /* __MTFS_SERVICE_INTERNAL_H__ */
