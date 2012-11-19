/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_SERVICE_H__
#define __MTFS_SERVICE_H__

struct mtfs_service;
struct mservice_thread;
typedef int (*mservice_main_t)(struct mtfs_service *service, struct mservice_thread *thread);
typedef int (*mservice_busy_t)(struct mtfs_service *service, struct mservice_thread *thread);

struct mtfs_service {
	mtfs_list_t       srv_list;
	char             *srv_name;
	char             *srv_thread_name;
	mtfs_list_t       srv_threads;
	int               srv_threads_running;
	int               srv_threads_starting;
	int               srv_threads_min;
	int               srv_threads_max;
	unsigned          srv_threads_next_id;
	unsigned          srv_is_stopping:1;
	unsigned          srv_cpu_affinity:1;
	spinlock_t        srv_lock;
	wait_queue_head_t srv_waitq;
	mservice_main_t   srv_main;
	void             *srv_data;
	mservice_busy_t   srv_busy;
};

/**
 * Definition of server service thread structure
 */
struct mservice_thread {
	mtfs_list_t          t_link;
	void                *t_data; //
	__u32                t_flags; //
	unsigned int         t_id;
	pid_t                t_pid;
	struct lc_watchdog  *t_watchdog; //
	struct mtfs_service *t_svc;
	wait_queue_head_t    t_ctl_waitq;
};

struct mservice_data {
        char *name;
        struct mtfs_service *svc;
        struct mservice_thread *thread;
};

extern void mservice_stop_threads(struct mtfs_service *svc);
extern int mservice_start_threads(struct mtfs_service *svc);
extern struct mtfs_service *mservice_init(char *name,
                                          char *thread_name,
                                           int threads_min,
                                           int threads_max,
                                           unsigned cpu_affinity,
                                           mservice_main_t main,
                                           mservice_busy_t busy,
                                           void *data);
extern int mservice_fini(struct mtfs_service *service);
extern int mservice_main_loop(struct mtfs_service *service, struct mservice_thread *thread);
extern int mservice_wait_event(struct mtfs_service *service, struct mservice_thread *thread);
#endif /* __MTFS_SERVICE_H__ */
