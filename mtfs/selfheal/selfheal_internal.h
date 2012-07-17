/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_SELFHEAL_INTERNAL_H__
#define __MTFS_SELFHEAL_INTERNAL_H__
#include <mtfs_selfheal.h>
#include <mtfs_list.h>
#include <mtfs_selfheal.h>

struct selfheal_daemon_ctl {
	unsigned long                sc_flags;
	spinlock_t                   sc_lock;
	int                          sc_index;
	struct completion            sc_starting;
	struct completion            sc_finishing;
	struct selfheal_request_set *sc_set;
	char                         sc_name[MAX_SELFHEAL_NAME_LENGTH];
};

struct selfheal_daemon {
	int                        sd_size;
	int                        sd_index;
	int                        sd_nthreads;
	struct selfheal_daemon_ctl sd_threads[0];
};

/* Bits for sc_flags */
enum mtfs_daemon_ctl_flags {
	/*
	 * Daemon thread start flag.
	 */
	MTFS_DAEMON_START       = 1 << 0,

	MTFS_DAEMON_STOP        = 1 << 1,
	/*
	 * Daemon thread force flag.
	 * This will cause aborting any inflight request handled
	 * by thread if MTFS_DAEMON_STOP is specified.
	 */
	MTFS_DAEMON_FORCE       = 1 << 2,
	/**
	 * The thread is bound to some CPU core.
	 */
	MTFS_DAEMON_BIND        = 1 << 3,
};

struct mtfs_request *selfheal_request_pack(void);
int selfheal_add_req(struct mtfs_request *req, mdl_policy_t policy, int idx);
#endif /* __MTFS_SELFHEAL_INTERNAL_H__ */
