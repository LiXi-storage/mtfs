/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/sched.h>
#include <linux/module.h>
#include <mtfs_list.h>
#include <mtfs_inode.h>
#include <mtfs_dentry.h>
#include <memory.h>
#include <thread.h>
#include <bitmap.h>
#include "selfheal_internal.h"

struct mtfs_request *mtfs_request_alloc(void)
{
	struct mtfs_request *request = NULL;
	HENTRY();

	/* TODO: pool */
	MTFS_ALLOC_PTR(request);
	if (request == NULL) {
		HERROR("request allocation out of memory\n");
		goto out;
	}
out:
        HRETURN(request);
}

void mtfs_request_free(struct mtfs_request *request)
{
	HENTRY();
	MTFS_FREE_PTR(request);
	_HRETURN();
}

/* Return 1 if freed */
static int selfheal_req_finished(struct mtfs_request *request)
{
	int ret = 0;
	HENTRY();

	if (atomic_dec_and_test(&request->rq_refcount)) {
		mtfs_request_free(request);
		ret = 1;
	}

	HRETURN(ret);
}

struct mtfs_request *selfheal_request_pack(void)
{
	struct mtfs_request *request = NULL;
	int ret = 0;
	HENTRY();

	request = mtfs_request_alloc();
	if (request == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	request->rq_phase = RQ_PHASE_NEW;
	MTFS_INIT_LIST_HEAD(&request->rq_set_chain);
	atomic_set(&request->rq_refcount, 1);
out:
	if (ret) {
		request = ERR_PTR(ret);
	}
	HRETURN(request);
}
EXPORT_SYMBOL(selfheal_request_pack);

static int mtfs_req_interpret(struct mtfs_request *req)
{
	int ret = 0;
	HENTRY();

	HERROR("interpreting req: %p\n", req);
	HRETURN(ret);
}

static int mtfs_reqphase_move(struct mtfs_request *req, enum rq_phase new_phase)
{
	int ret = 0;
	HENTRY();

	if (req->rq_phase == new_phase) {
		goto out;
	}

	req->rq_phase = new_phase;
out:
	HRETURN(ret);
}

/*
 * If set is emptied return 1
 */
static int selfheal_check_set(struct selfheal_request_set *set)
{
	int ret = 0;
	mtfs_list_t *tmp = NULL;
	struct mtfs_request *req = NULL;
	HENTRY();

	if (atomic_read(&set->set_remaining) == 0) {
		ret = 1;
		goto out;
	}

	mtfs_list_for_each(tmp, &set->set_requests) {
		req = mtfs_list_entry(tmp,
		                      struct mtfs_request,
		                      rq_set_chain);

		mtfs_req_interpret(req);
		mtfs_reqphase_move(req, RQ_PHASE_COMPLETE);
                atomic_dec(&set->set_remaining);
	}
	ret = atomic_read(&set->set_remaining) == 0;
out:
	HRETURN(ret);
}

static int selfheal_check(struct selfheal_daemon_ctl *sc)
{
	int ret = 0;
	struct selfheal_request_set *set = sc->sc_set;
	mtfs_list_t *tmp = NULL;
	mtfs_list_t *pos = NULL;
	struct mtfs_request *req = NULL;
	HENTRY();

	ret = mtfs_test_bit(MTFS_DAEMON_STOP, &sc->sc_flags);
	if (ret) {
		goto out;
	}

	if (atomic_read(&set->set_new_count)) {
		spin_lock(&set->set_new_req_lock);
		{
			if (likely(!mtfs_list_empty(&set->set_new_requests))) {
				mtfs_list_splice_init(&set->set_new_requests,
				                      &set->set_requests);
				atomic_add(atomic_read(&set->set_new_count),
				               &set->set_remaining);
				atomic_set(&set->set_new_count, 0);
			}
			ret = 1;
		}
		spin_unlock(&set->set_new_req_lock);
	}

	if (atomic_read(&set->set_remaining)) {
		ret |= selfheal_check_set(set);
	}

	if (!mtfs_list_empty(&set->set_requests)) {
		/*
		 * prune the completed
		 * reqs after each iteration
		 */
		mtfs_list_for_each_safe(pos, tmp, &set->set_requests) {
			req = mtfs_list_entry(pos, struct mtfs_request,
			                      rq_set_chain);
			if (req->rq_phase != RQ_PHASE_COMPLETE)
				continue;

			mtfs_list_del_init(&req->rq_set_chain);
			req->rq_set = NULL;
			selfheal_req_finished(req);
                }
	}

	if (ret == 0) {
		/*
		 * If new requests have been added, make sure to wake up.
		 */
		ret = atomic_read(&set->set_new_count);
		if (ret == 0) {
			/*
			 * If we have nothing to do, check whether we can take some
			 * work from our partner threads.
			 */
		}
	}

out:
	HRETURN(ret);
}

static int selfheal_max_nthreads = 0;
module_param(selfheal_max_nthreads, int, 0644);
MODULE_PARM_DESC(selfheal_max_nthreads, "Max selfheal thread count to be started.");
EXPORT_SYMBOL(selfheal_max_nthreads);
static struct selfheal_daemon *selfheal_daemons;

static struct selfheal_daemon_ctl*
selfheal_select_pc(struct mtfs_request *req, mdl_policy_t policy, int index)
{
	int idx = 0;
	HENTRY();

	HASSERT(req != NULL);

	switch (policy) {
	case MDL_POLICY_SAME:
		idx = smp_processor_id() % selfheal_daemons->sd_nthreads;
		break;
	case MDL_POLICY_LOCAL:
		/* Fix me */
		index = -1;
	case MDL_POLICY_PREFERRED:
		if (index >= 0 && index < num_online_cpus()) {
			idx = index % selfheal_daemons->sd_nthreads;
		}
		/* Fall through to PDL_POLICY_ROUND for bad index. */
	case MDL_POLICY_ROUND:
	default:
		/* We do not care whether it is strict load balance. */
		idx = selfheal_daemons->sd_index + 1;
		if (idx == smp_processor_id()) {
			idx++;
		}
		idx %= selfheal_daemons->sd_nthreads;
		selfheal_daemons->sd_index = idx;
		break;
	}
	HRETURN(&selfheal_daemons->sd_threads[idx]);
}

int selfheal_set_add_new_req(struct selfheal_daemon_ctl *sc,
                           struct mtfs_request *req)
{
	struct selfheal_request_set *set = sc->sc_set;
	int ret = 0;
	int count = 0;
	HENTRY();

	HASSERT(req->rq_set == NULL);
	HASSERT(mtfs_test_bit(MTFS_DAEMON_STOP, &sc->sc_flags) == 0);
	spin_lock(&set->set_new_req_lock);
	{
		req->rq_set = set;
		mtfs_list_add_tail(&req->rq_set_chain, &set->set_new_requests);
		count = atomic_inc_return(&set->set_new_count);
	}
	spin_unlock(&set->set_new_req_lock);

	/* Only need to call wakeup once for the first entry. */
	if (count == 1) {
		wake_up(&set->set_waitq);
		/* TODO: partners */
	}
	HRETURN(ret);
}

int selfheal_add_req(struct mtfs_request *req, mdl_policy_t policy, int idx)
{
	struct selfheal_daemon_ctl *sc = NULL;
	int ret = 0;
	HENTRY();

	HERROR("adding request: %p\n", req);
	sc = selfheal_select_pc(req, policy, idx);
	selfheal_set_add_new_req(sc, req);
	HRETURN(ret);
}
EXPORT_SYMBOL(selfheal_add_req);

#ifdef HAVE_SET_CPUS_ALLOWED
#define mtfs_set_cpus_allowed(task, mask)  set_cpus_allowed(task, mask)
#else
#define mtfs_set_cpus_allowed(task, mask)  set_cpus_allowed_ptr(task, &(mask))
#endif

#ifndef HAVE_NODE_TO_CPUMASK
#define node_to_cpumask(i)         (*(cpumask_of_node(i)))
#endif


#define MTFS_SELFHEAL_TIMEOUT 10
static int selfheal_daemon_main(void *arg)
{
	int ret = 0;
	struct selfheal_daemon_ctl *sc = arg;
	struct selfheal_request_set *set = sc->sc_set;
	int exit = 0;
	HENTRY();

	mtfs_daemonize_ctxt(sc->sc_name);
#if defined(CONFIG_SMP)
	if (mtfs_test_bit(MTFS_DAEMON_BIND, &sc->sc_flags)) {
		int index = sc->sc_index;

		HASSERT(index  >= 0 && index < num_possible_cpus());
		while (!cpu_online(index)) {
			if (++index >= num_possible_cpus()) {
				index = 0;
			}
		}
		mtfs_set_cpus_allowed(current,
                                      node_to_cpumask(cpu_to_node(index)));
	}
#endif

	complete(&sc->sc_starting);
	do {
		struct mtfs_wait_info mwi;
		int timeout;

		timeout = MTFS_SELFHEAL_TIMEOUT;
		mwi = MWI_TIMEOUT_INTR_ALL(timeout * HZ, NULL, NULL, NULL);

		mtfs_wait_event(set->set_waitq, selfheal_check(sc), &mwi);
		if (mtfs_test_bit(MTFS_DAEMON_STOP, &sc->sc_flags)) {
			if (mtfs_test_bit(MTFS_DAEMON_FORCE, &sc->sc_flags)) {
				//abort;
			}
			exit++;
		}
		HDEBUG("selfheal daemon loop again\n");
	} while (!exit);

	complete(&sc->sc_finishing);
	mtfs_clear_bit(MTFS_DAEMON_START, &sc->sc_flags);
	mtfs_clear_bit(MTFS_DAEMON_STOP, &sc->sc_flags);
	mtfs_clear_bit(MTFS_DAEMON_FORCE, &sc->sc_flags);
	mtfs_clear_bit(MTFS_DAEMON_BIND, &sc->sc_flags);
	HRETURN(ret);
}

struct selfheal_request_set *selfheal_prep_set(void)
{
	struct selfheal_request_set *set = NULL;
	HENTRY();

	MTFS_ALLOC_PTR(set);
	if (set == NULL) {
		goto out;
	}

	atomic_set(&set->set_refcount, 1);
	MTFS_INIT_LIST_HEAD(&set->set_requests);
	init_waitqueue_head(&set->set_waitq);
	atomic_set(&set->set_new_count, 0);
	atomic_set(&set->set_remaining, 0);
	spin_lock_init(&set->set_new_req_lock);
	MTFS_INIT_LIST_HEAD(&set->set_new_requests);

out:
	HRETURN(set);
}

void selfheal_put_get(struct selfheal_request_set *set)
{
	atomic_inc(&set->set_refcount);	
}

void selfheal_put_set(struct selfheal_request_set *set)
{
	if (atomic_dec_and_test(&set->set_refcount)) {
		MTFS_FREE_PTR(set);
	}	
}

void selfheal_set_destroy(struct selfheal_request_set *set)
{
	selfheal_put_set(set);
}

int selfheal_daemon_start(int index, const char *name, int no_bind, struct selfheal_daemon_ctl *sc)
{
	int ret = 0;
	struct selfheal_request_set *set = NULL;
	HENTRY();

	if (mtfs_test_and_set_bit(MTFS_DAEMON_START, &sc->sc_flags)) {
		HERROR("thread [%s] is already started\n",
                       name);
		goto out;
        }

	sc->sc_index = index;
	init_completion(&sc->sc_starting);
	init_completion(&sc->sc_finishing);
	spin_lock_init(&sc->sc_lock);
	strncpy(sc->sc_name, name, sizeof(sc->sc_name) - 1);

	sc->sc_set = selfheal_prep_set();
	if (sc->sc_set == NULL) {
		ret = -ENOMEM;
		goto out;
	}	
	
	if (!no_bind) {
		mtfs_set_bit(MTFS_DAEMON_BIND, &sc->sc_flags);
	}

	ret = mtfs_create_thread(selfheal_daemon_main, sc, 0);
	if (ret < 0) {
		HERROR("failed to create thread [%s]", name);
		goto out_destroy_set;
	}
	ret = 0;

	wait_for_completion(&sc->sc_starting);

	goto out;
out_destroy_set:
	set = sc->sc_set;
	spin_lock(&sc->sc_lock);
	sc->sc_set = NULL;
	spin_unlock(&sc->sc_lock);
	selfheal_set_destroy(set);

	mtfs_clear_bit(MTFS_DAEMON_BIND, &sc->sc_flags);
	mtfs_clear_bit(MTFS_DAEMON_START, &sc->sc_flags);
out:
	HRETURN(ret);
}

void selfheal_daemon_stop(struct selfheal_daemon_ctl *sc, int force)
{
	struct selfheal_request_set *set = NULL;
	HENTRY();

	HDEBUG("Stoping\n");
	if (!mtfs_test_bit(MTFS_DAEMON_START, &sc->sc_flags)) {
                HERROR("Thread for pc %p was not started\n", sc);
		HBUG();
                goto out;
        }

        mtfs_set_bit(MTFS_DAEMON_STOP, &sc->sc_flags);
	if (force) {
		mtfs_set_bit(MTFS_DAEMON_FORCE, &sc->sc_flags);
	}

	wake_up(&sc->sc_set->set_waitq);
	wait_for_completion(&sc->sc_finishing);

	set = sc->sc_set;
	spin_lock(&sc->sc_lock);
	sc->sc_set = NULL;
	spin_unlock(&sc->sc_lock);
	selfheal_set_destroy(set);

out:
	_HRETURN();
}

void selfheal_daemon_fini(void)
{
	int i = 0;
	HENTRY();

	if (selfheal_daemons == NULL) {
		HERROR("selfheal daemons is not inited yet\n");
		HBUG();
		goto out;
	}

	for (i = 0; i < selfheal_daemons->sd_nthreads; i++) {
		selfheal_daemon_stop(&selfheal_daemons->sd_threads[i], 0);
	}
	MTFS_FREE(selfheal_daemons, selfheal_daemons->sd_size);
	selfheal_daemons = NULL;

out:
	_HRETURN();
}

int selfheal_daemon_init(void)
{
	int nthreads = num_online_cpus();
	char name[MAX_SELFHEAL_NAME_LENGTH];
	int size = 0;
	int ret = 0;
	int i = 0;
	HENTRY();

	if (selfheal_max_nthreads > 0 && selfheal_max_nthreads < nthreads) {
                nthreads = selfheal_max_nthreads;
	}
#ifndef LIXI_20120706
	nthreads = 1;
#endif

	size = offsetof(struct selfheal_daemon, sd_threads[nthreads]);
	MTFS_ALLOC(selfheal_daemons, size);
	if (selfheal_daemons == NULL) {
		HERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	for (i = 0; i < nthreads; i++) {
		snprintf(name, MAX_SELFHEAL_NAME_LENGTH - 1, "selfheal_%d", i);
		ret = selfheal_daemon_start(i, name, 0,
		                            &selfheal_daemons->sd_threads[i]);
		if (ret) {
			HERROR("failed to start selfheal daemon[%d]\n", i);
			i--;
			goto out_stop;
		}
	}

	selfheal_daemons->sd_size = size;
	selfheal_daemons->sd_index = 0;
	selfheal_daemons->sd_nthreads = nthreads;
	goto out;
out_stop:
	for (; i >= 0; i--) {
		selfheal_daemon_stop(&selfheal_daemons->sd_threads[i], 0);
	}
	MTFS_FREE(selfheal_daemons, size);
	selfheal_daemons = NULL;
out:
	HRETURN(ret);
}

static int __init mtfs_selfheal_init(void)
{
	int ret = 0;
	ret = selfheal_daemon_init();
	return ret;
}

static void __exit mtfs_selfheal_exit(void)
{
	selfheal_daemon_fini();
}

MODULE_AUTHOR("MulTi File System Development Workgroup");
MODULE_DESCRIPTION("mtfs_selfheal");
MODULE_LICENSE("GPL");

module_init(mtfs_selfheal_init)
module_exit(mtfs_selfheal_exit)
