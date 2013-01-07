/*
 * Copied from Lustre-2.2.0
 */

#include <linux/notifier.h>
#include <linux/module.h>
#include <linux/hardirq.h>
#include "debug.h"

static void mtfs_debug_dumpstack(struct task_struct *tsk)
{
#ifdef HAVE_DUMP_TRACE
	/* dump_stack() */
	/* show_trace() */
	if (tsk == NULL) {
                tsk = current;
	}
	printk("Pid: %d, comm: %.20s\n", tsk->pid, tsk->comm);
	/* show_trace_log_lvl() */
	printk("\nCall Trace:\n");
	dump_trace(tsk, NULL, NULL,
#ifdef HAVE_DUMP_TRACE_ADDRESS
	           0,
#endif /* HAVE_DUMP_TRACE_ADDRESS */
	           &print_trace_ops, NULL);
	printk("\n");
#elif defined(HAVE_SHOW_TASK)
	/* this is exported by kernel */
        extern void show_task(struct task_struct *);

        if (tsk == NULL) {
                tsk = current;
	}
        MWARN("showing stack for process %d\n", tsk->pid);
        show_task(tsk);
#else
        if ((tsk == NULL) || (tsk == current)) {
                dump_stack();
	} else {
                MWARN("can't show stack: kernel doesn't export show_task\n");
	}
#endif
}

void mbug(void)
{
	mtfs_catastrophe = 1;

        if (in_interrupt()) {
                panic("MBUG in interrupt.\n");
                /* not reached */
        }

	mtfs_debug_dumpstack(NULL);
	if (!mtfs_panic_on_mbug) {
		mtfs_debug_dumplog();
	}

	//mtfs_run_mbug_upcall(file, func, line);

	if (mtfs_panic_on_mbug) {
		panic("MBUG");
	}

	set_task_state(current, TASK_UNINTERRUPTIBLE);
	while(1) {
		schedule();
	}
}
EXPORT_SYMBOL(mbug);

static int panic_notifier(struct notifier_block *self, unsigned long unused1,
                         void *unused2)
{
        if (mtfs_panic_in_progress)
                return 0;

        mtfs_panic_in_progress = 1;
        mb();

#if 0
//#ifdef LNET_DUMP_ON_PANIC
        /* This is currently disabled because it spews far too much to the
         * console on the rare cases it is ever triggered. */

        if (in_interrupt()) {
                mtfs_trace_debug_print();
        } else {
                while (current->lock_depth >= 0)
                        unlock_kernel();

                mtfs_debug_dumplog_internal((void *)(long)mtfs_curproc_pid());
        }
#endif
        return 0;
}


static struct notifier_block mtfs_panic_notifier = {
        notifier_call :     panic_notifier,
        next :              NULL,
        priority :          10000
};

void mtfs_register_panic_notifier(void)
{
#ifdef HAVE_ATOMIC_PANIC_NOTIFIER
	atomic_notifier_chain_register(&panic_notifier_list, &mtfs_panic_notifier);
#else
	notifier_chain_register(&panic_notifier_list, &mtfs_panic_notifier);
#endif
}

void mtfs_unregister_panic_notifier(void)
{
#ifdef HAVE_ATOMIC_PANIC_NOTIFIER
	atomic_notifier_chain_unregister(&panic_notifier_list, &mtfs_panic_notifier);
#else
	notifier_chain_unregister(&panic_notifier_list, &mtfs_panic_notifier);
#endif
}

