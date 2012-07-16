/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_SELFHEAL_INTERNAL_H__
#define __MTFS_SELFHEAL_INTERNAL_H__
#include <mtfs_selfheal.h>
#include <mtfs_list.h>

/* mtfs daemon load policy */
typedef enum {
	/* on the same CPU core as the caller */
	MDL_POLICY_SAME         = 1,
	/* within the same CPU partition, but not the same core as the caller */
	MDL_POLICY_LOCAL        = 2,
	/* round-robin on all CPU cores, but not the same core as the caller */
	MDL_POLICY_ROUND        = 3,
	/* the specified CPU core is preferred, but not enforced */
	MDL_POLICY_PREFERRED    = 4,
} mdl_policy_t;

enum rq_phase {
	RQ_PHASE_NEW            = 0xebc0de00,
	RQ_PHASE_RPC            = 0xebc0de01,
	RQ_PHASE_BULK           = 0xebc0de02,
	RQ_PHASE_INTERPRET      = 0xebc0de03,
	RQ_PHASE_COMPLETE       = 0xebc0de04,
	RQ_PHASE_UNREGISTERING  = 0xebc0de05,
	RQ_PHASE_UNDEFINED      = 0xebc0de06
};

struct selfheal_message {
	struct inode *dir;
};

struct mtfs_request {
	atomic_t                     rq_refcount;
	int                          rq_type;
	enum rq_phase                rq_phase;
	mtfs_list_t                  rq_set_chain;
	struct selfheal_request_set *rq_set;
	int                          rq_msg_len;
	void                        *rq_message;
};
	
#define MAX_SELFHEAL_NAME_LENGTH 16
struct selfheal_request_set {
	atomic_t          set_refcount;
	atomic_t          set_new_count;
	atomic_t          set_remaining;
	wait_queue_head_t set_waitq;
	mtfs_list_t       set_requests;
	spinlock_t        set_new_req_lock;
	mtfs_list_t       set_new_requests;
};

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
