/*
 * Copied from Lustre-2.2.0
 */
#include <linux/notifier.h>
#include "debug.h"

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

