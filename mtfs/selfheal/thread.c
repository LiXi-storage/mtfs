/*
 * Copied from Lustre-2.2
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/smp_lock.h>
#include <linux/fs_struct.h>

#ifdef HAVE_LINUX_OOM_H
#include <linux/oom.h>
#else
#include <linux/mm.h>
#endif

int oom_get_adj(struct task_struct *task, int scope)
{
	int oom_adj;
#ifdef HAVE_OOMADJ_IN_SIG
	unsigned long flags;

	spin_lock_irqsave(&task->sighand->siglock, flags);
	oom_adj = task->signal->oom_adj;
	task->signal->oom_adj = scope;
	spin_unlock_irqrestore(&task->sighand->siglock, flags);

#else /* !HAVE_OOMADJ_IN_SIG */
	oom_adj = task->oomkilladj;
	task->oomkilladj = scope;
#endif /* !HAVE_OOMADJ_IN_SIG */
	return oom_adj;
}

int mtfs_create_thread(int (*fn)(void *),
                      void *arg, unsigned long flags)
{
	void *orig_info = current->journal_info;
	int rc;
	int old_oom;

	old_oom = oom_get_adj(current, OOM_DISABLE);
	current->journal_info = NULL;
	rc = kernel_thread(fn, arg, flags);
	current->journal_info = orig_info;
	oom_get_adj(current, old_oom);

	return rc;
}
EXPORT_SYMBOL(mtfs_create_thread);

#define SIGNAL_MASK_LOCK(task, flags)                                  \
  spin_lock_irqsave(&task->sighand->siglock, flags)
#define SIGNAL_MASK_UNLOCK(task, flags)                                \
  spin_unlock_irqrestore(&task->sighand->siglock, flags)

sigset_t mtfs_block_sigs(sigset_t bits)
{
        unsigned long  flags;
        sigset_t        old;

        SIGNAL_MASK_LOCK(current, flags);
        old = current->blocked;
        current->blocked = bits;
        recalc_sigpending();
        SIGNAL_MASK_UNLOCK(current, flags);
        return old;
}
EXPORT_SYMBOL(mtfs_block_sigs);

sigset_t mtfs_block_sigsinv(unsigned long sigs)
{
        unsigned long flags;
        sigset_t old;

        SIGNAL_MASK_LOCK(current, flags);
        old = current->blocked;
        siginitsetinv(&current->blocked, sigs);
        recalc_sigpending();
        SIGNAL_MASK_UNLOCK(current, flags);

        return old;
}
EXPORT_SYMBOL(mtfs_block_sigsinv);

void mtfs_clear_sigpending(void)
{
        unsigned long flags;

        SIGNAL_MASK_LOCK(current, flags);
        clear_tsk_thread_flag(current, TIF_SIGPENDING);
        SIGNAL_MASK_UNLOCK(current, flags);
}
EXPORT_SYMBOL(mtfs_clear_sigpending);

void mtfs_daemonize(char *str)
{
	unsigned long flags;

	lock_kernel();
	daemonize(str);
	SIGNAL_MASK_LOCK(current, flags);
	sigfillset(&current->blocked);
	recalc_sigpending();
	SIGNAL_MASK_UNLOCK(current, flags);
	unlock_kernel();
}

int mtfs_daemonize_ctxt(char *str)
{
	mtfs_daemonize(str);
#ifndef HAVE_UNSHARE_FS_STRUCT
	{
	struct task_struct *tsk = current;
	struct fs_struct *fs = NULL;
	fs = copy_fs_struct(tsk->fs);
	if (fs == NULL)
		return -ENOMEM;
	exit_fs(tsk);
	tsk->fs = fs;
	}
#else
	unshare_fs_struct();
#endif
	return 0;
}
EXPORT_SYMBOL(mtfs_daemonize_ctxt);

#ifndef HAVE___ADD_WAIT_QUEUE_EXCLUSIVE

static inline void __add_wait_queue_exclusive(wait_queue_head_t *q,
                                              wait_queue_t *wait)
{
        wait->flags |= WQ_FLAG_EXCLUSIVE;
        __add_wait_queue(q, wait);
}

#endif /* HAVE___ADD_WAIT_QUEUE_EXCLUSIVE */

void
mtfs_waitq_add_exclusive_head(wait_queue_head_t *waitq, wait_queue_t *link)
{
        unsigned long flags;

        spin_lock_irqsave(&waitq->lock, flags);
        __add_wait_queue_exclusive(waitq, link);
        spin_unlock_irqrestore(&waitq->lock, flags);
}
