/*
 * Copied from Lustre-2.2
 */

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <debug.h>
#include <memory.h>
#include <mtfs_list.h>
#include <thread.h>
#include "service_internal.h"

static inline int
mtfs_threads_increasable(struct mtfs_service *svc)
{
	return svc->srv_threads_running +
		svc->srv_threads_starting < svc->srv_threads_max;
}

int mservice_wait_event(struct mtfs_service *service, struct mservice_thread *thread)
{
	struct mtfs_wait_info mwi = MWI_TIMEOUT(mtfs_time_seconds(100),
                                                NULL, NULL);
	int ret = 0;
	MENTRY();

	cond_resched();

	MASSERT(service->srv_busy);
	mtfs_wait_event_exclusive_head(service->srv_waitq,
	                               mservice_thread_stopping(thread) ||
	                               (service->srv_busy && service->srv_busy(service, thread)),
	                               &mwi);

	if (mservice_thread_stopping(thread)) {
		ret = -EINTR;
	}

        MRETURN(ret);
}
EXPORT_SYMBOL(mservice_wait_event);

int mservice_main_loop(struct mtfs_service *service, struct mservice_thread *thread)
{
	int ret = 0;
	MENTRY();

	while (1) {
		if (mservice_wait_event(service, thread)) {
			break;
		}
		MERROR("thread[%d] looping\n", thread->t_pid);
	}

	MRETURN(ret);
}
EXPORT_SYMBOL(mservice_main_loop);

static int mservice_main(void *arg)
{
	int ret = 0;
	struct mservice_data *data = (struct mservice_data *)arg;
	struct mtfs_service  *service = data->svc;
	struct mservice_thread *thread = data->thread;
	MENTRY();

	thread->t_pid = current->pid;
	mtfs_daemonize_ctxt(data->name);
#if defined(HAVE_NODE_TO_CPUMASK) && defined(CONFIG_NUMA)
	if (service->srv_cpu_affinity) {
		int cpu, num_cpu;

		for (cpu = 0, num_cpu = 0; cpu < num_possible_cpus();
		     cpu++) {
			if (!cpu_online(cpu)) {
				continue;
			}

			if (num_cpu == thread->t_id % num_online_cpus()) {
				break;
			}
			num_cpu++;
		}
		mtfs_set_cpus_allowed(current,
		                      node_to_cpumask(cpu_to_node(cpu)));
	}
#endif /* defined(HAVE_NODE_TO_CPUMASK) && defined(CONFIG_NUMA) */

	spin_lock(&service->srv_lock);
	MASSERT(thread_is_starting(thread));
	thread_clear_flags(thread, SVC_STARTING);
        service->srv_threads_starting--;
	/* SVC_STOPPING may already be set here if someone else is trying
	 * to stop the service while this new thread has been dynamically
	 * forked. We still set SVC_RUNNING to let our creator know that
	 * we are now running, however we will exit as soon as possible */
        thread_add_flags(thread, SVC_RUNNING);
        service->srv_threads_running++;
	spin_unlock(&service->srv_lock);

	 /*
         * wake up our creator. Note: @data is invalid after this point,
         * because it's allocated on mservice_start_thread() stack.
         */
	wake_up(&thread->t_ctl_waitq);

	MASSERT(service->srv_main);
	service->srv_main(service, thread);

	spin_lock(&service->srv_lock);
	if (thread_test_and_clear_flags(thread, SVC_STARTING)) {
		service->srv_threads_starting--;
	}

	if (thread_test_and_clear_flags(thread, SVC_RUNNING)) {
		/* must know immediately */
		service->srv_threads_running--;
	}

	thread->t_id = ret;
	thread_add_flags(thread, SVC_STOPPED);

	wake_up(&thread->t_ctl_waitq);
	spin_unlock(&service->srv_lock);
     
	MRETURN(ret);
}

#define MTFS_MAX_THREAD_NAME_LENGTH 32
int mservice_start_thread(struct mtfs_service *svc)
{
	struct mtfs_wait_info mwi = { 0 };
	struct mservice_thread *thread = NULL;
	int ret = 0;
	char name[MTFS_MAX_THREAD_NAME_LENGTH];
	struct mservice_data d;
	MENTRY();

	if (unlikely(svc->srv_is_stopping)) {
		MERROR("this service is stopping\n");
		ret = -ESRCH;
		goto out;
	}

	if (!mtfs_threads_increasable(svc)) {
		MERROR("this service can not increase threads\n");
		ret = -EMFILE;
		goto out;
	}

	MTFS_ALLOC_PTR(thread);
	if (thread == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	init_waitqueue_head(&thread->t_ctl_waitq);

	spin_lock(&svc->srv_lock);
	{
		if (!mtfs_threads_increasable(svc)) {
			spin_unlock(&svc->srv_lock);
			MERROR("this service can not increase threads\n");
			ret = -EMFILE;
			goto out_free_thread;
		}
		svc->srv_threads_starting++;
		thread->t_id = svc->srv_threads_next_id++;
		thread_add_flags(thread, SVC_STARTING);
		thread->t_svc = svc;

		mtfs_list_add(&thread->t_link, &svc->srv_threads);
	}
	spin_unlock(&svc->srv_lock);

	sprintf(name, "%s_%02d", svc->srv_thread_name, thread->t_id);
	d.svc = svc;
	d.name = name;
	d.thread = thread;

	MDEBUG("starting thread '%s'\n", name);
	ret = mtfs_create_thread(mservice_main, &d, 0);
	if (ret < 0) {
		MERROR("cannot start thread '%s', ret = %d\n", name, ret);
		goto out_delete_thread;
	}

	mtfs_wait_event(thread->t_ctl_waitq,
	                thread_is_running(thread) || thread_is_stopped(thread),
	                &mwi);

	ret = thread_is_stopped(thread) ? thread->t_id : 0;
	goto out;

out_delete_thread:
	spin_lock(&svc->srv_lock);
	mtfs_list_del(&thread->t_link);
	--svc->srv_threads_starting;
	spin_unlock(&svc->srv_lock);
out_free_thread:
	MTFS_FREE_PTR(thread);
out:
	MRETURN(ret);
}

int mservice_start_threads(struct mtfs_service *svc)
{
	int i  =0;
	int ret = 0;
	MENTRY();

	MASSERT(svc->srv_threads_min >= 1);
	for (i = 0; i < svc->srv_threads_min; i++) {
		ret = mservice_start_thread(svc);
		/* We have enough threads, don't start more. */
		if (ret == -EMFILE) {
			ret = 0;
			break;
		}

		if (ret) {
			MERROR("cannot start %s thread #%d: ret = %d\n",
			       svc->srv_thread_name, i, ret);
			mservice_stop_threads(svc);
			break;
		}
	}
        MRETURN(ret);
}
EXPORT_SYMBOL(mservice_start_threads);

static void mservice_stop_thread(struct mtfs_service *svc,
                                  struct mservice_thread *thread)
{
	struct mtfs_wait_info mwi = { 0 };
	MENTRY();

	MDEBUG("Stopping thread [ %p : %u ]\n",
	       thread, thread->t_pid);

	spin_lock(&svc->srv_lock);
	/* let the thread know that we would like it to stop asap */
	thread_add_flags(thread, SVC_STOPPING);
	spin_unlock(&svc->srv_lock);

	wake_up_all(&svc->srv_waitq);
	mtfs_wait_event(thread->t_ctl_waitq,
	                thread_is_stopped(thread), &mwi);

	spin_lock(&svc->srv_lock);
	list_del(&thread->t_link);
	spin_unlock(&svc->srv_lock);

	MTFS_FREE_PTR(thread);

	_MRETURN();
}

void mservice_stop_threads(struct mtfs_service *service)
{
        struct mservice_thread *thread;
        MENTRY();

        spin_lock(&service->srv_lock);
        while (!list_empty(&service->srv_threads)) {
                thread = list_entry(service->srv_threads.next,
                                    struct mservice_thread, t_link);

                spin_unlock(&service->srv_lock);
                mservice_stop_thread(service, thread);
                spin_lock(&service->srv_lock);
        }
        spin_unlock(&service->srv_lock);

        _MRETURN();
}
EXPORT_SYMBOL(mservice_stop_threads);

struct mtfs_service *mservice_init(char *name,
                                   char *thread_name,
                                   int threads_min,
                                   int threads_max,
                                   unsigned cpu_affinity,
                                   mservice_main_t main,
                                   mservice_busy_t busy,
                                   void *data)
{
	int ret = 0;
	struct mtfs_service *service = NULL;
	MENTRY();

	MTFS_ALLOC_PTR(service);
	if (service == NULL) {
		MERROR("not enough memory\n");
		goto out;
	}

	service->srv_name = name;
	service->srv_thread_name = thread_name;
	INIT_LIST_HEAD(&service->srv_threads);
	service->srv_threads_running = 0;
	service->srv_threads_starting = 0;
	service->srv_threads_min = threads_min;
	service->srv_threads_max = threads_max;
	service->srv_threads_next_id = 0;
	service->srv_is_stopping = 0;
	service->srv_cpu_affinity = cpu_affinity;
	spin_lock_init(&service->srv_lock);
	init_waitqueue_head(&service->srv_waitq);
	service->srv_main = main;
	service->srv_busy = busy;
	service->srv_data = data;

	ret = mservice_start_threads(service);
	if (ret) {
		MERROR("failed to start theads, ret = %d\n", ret);
		goto out_free_service;
	}
	goto out;
out_free_service:
	MTFS_FREE_PTR(service);
	service = NULL;
out:	
	MRETURN(service);
}
EXPORT_SYMBOL(mservice_init);

int mservice_fini(struct mtfs_service *service)
{
	int ret = 0;
	MENTRY();

	service->srv_is_stopping = 1;
	mservice_stop_threads(service);
	MASSERT(list_empty(&service->srv_threads));

	MTFS_FREE_PTR(service);

	MRETURN(ret);
}
EXPORT_SYMBOL(mservice_fini);
