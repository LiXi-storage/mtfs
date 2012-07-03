/*
 * Copied from Lustre-2.2
 */

#include <linux/module.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

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

