/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_SELFHEAL_H__
#define __MTFS_SELFHEAL_H__

#if defined(__linux__) && defined(__KERNEL__)
#include <mtfs_list.h>
#include <asm/atomic.h>
#include <linux/wait.h>

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
//	RQ_PHASE_RPC            = 0xebc0de01,
//	RQ_PHASE_BULK           = 0xebc0de02,
//	RQ_PHASE_INTERPRET      = 0xebc0de03,
	RQ_PHASE_COMPLETE       = 0xebc0de04,
//	RQ_PHASE_UNREGISTERING  = 0xebc0de05,
	RQ_PHASE_UNDEFINED      = 0xebc0de06
};

typedef enum mselfheal_message_type {
	MSMT_MIN = 0,
	MSMT_FILE = 0,
	MSMT_MAX,
} mselfheal_message_type_t;

struct mselfheal_message {
	mselfheal_message_type_t msm_type;
};

struct mtfs_request {
	atomic_t                     rq_refcount;
	int                          rq_type;
	enum rq_phase                rq_phase;
	mtfs_list_t                  rq_set_chain;
	struct selfheal_request_set *rq_set;
	struct mselfheal_message    *rq_message;
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

extern struct mtfs_request *selfheal_request_pack(void);
extern int selfheal_add_req(struct mtfs_request *req, mdl_policy_t policy, int idx);

#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_SELFHEAL_H__ */
