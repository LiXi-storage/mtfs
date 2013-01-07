/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */
#ifndef __MTFS_THREAD_H__
#define __MTFS_THREAD_H__

#if defined (__linux__) && defined(__KERNEL__)

#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/kthread.h>
extern int mtfs_create_thread(int (*fn)(void *), void *arg, unsigned long flags);
extern int mtfs_daemonize_ctxt(char *str);

struct mtfs_wait_info {
        long   mwi_timeout;
        long   mwi_interval;
        int    mwi_allow_intr;
        int  (*mwi_on_timeout)(void *);
        void (*mwi_on_signal)(void *);
        void  *mwi_cb_data;
};

/* NB: MWI_TIMEOUT ignores signals completely */
#define MWI_TIMEOUT(time, cb, data)         \
((struct mtfs_wait_info) {                  \
    .mwi_timeout    = time,                 \
    .mwi_on_timeout = cb,                   \
    .mwi_cb_data    = data,                 \
    .mwi_interval   = 0,                    \
    .mwi_allow_intr = 0                     \
})

#define MWI_TIMEOUT_INTERVAL(time, interval, cb, data) \
((struct mtfs_wait_info) {                             \
    .mwi_timeout    = time,                            \
    .mwi_on_timeout = cb,                              \
    .mwi_cb_data    = data,                            \
    .mwi_interval   = interval,                        \
    .mwi_allow_intr = 0                                \
})

#define MWI_TIMEOUT_INTR(time, time_cb, sig_cb, data)  \
((struct mtfs_wait_info) {                             \
    .mwi_timeout    = time,                            \
    .mwi_on_timeout = time_cb,                         \
    .mwi_on_signal  = sig_cb,                          \
    .mwi_cb_data    = data,                            \
    .mwi_interval   = 0,                               \
    .mwi_allow_intr = 0                                \
})

#define MWI_TIMEOUT_INTR_ALL(time, time_cb, sig_cb, data) \
((struct mtfs_wait_info) {                                \
    .mwi_timeout    = time,                               \
    .mwi_on_timeout = time_cb,                            \
    .mwi_on_signal  = sig_cb,                             \
    .mwi_cb_data    = data,                               \
    .mwi_interval   = 0,                                  \
    .mwi_allow_intr = 1                                   \
})

#define MWI_INTR(cb, data)  MWI_TIMEOUT_INTR(0, NULL, cb, data)

#define MWI_ON_SIGNAL_NOOP ((void (*)(void *))(-1))

#define MTFS_FATAL_SIGS (sigmask(SIGKILL) | sigmask(SIGINT)  |               \
                         sigmask(SIGTERM) | sigmask(SIGQUIT) |               \
                         sigmask(SIGALRM))
/*
 * wait for @condition to become true, but no longer than timeout, specified
 * by @info.
 */
#define __mtfs_wait_event(wq, condition, info, ret, mtfs_add_wait)             \
do {                                                                           \
    wait_queue_t __wait;                                                       \
    long         __timeout = info->mwi_timeout;                                \
    sigset_t     __blocked;                                                    \
    int          __allow_intr = info->mwi_allow_intr;                          \
                                                                               \
        ret = 0;                                                               \
        if (condition)                                                         \
                break;                                                         \
                                                                               \
        init_waitqueue_entry(&__wait, current);                                \
        mtfs_add_wait(&wq, &__wait);                                           \
                                                                               \
        /* Block all signals (just the non-fatal ones if no timeout). */       \
        if (info->mwi_on_signal != NULL && (__timeout == 0 || __allow_intr))   \
                __blocked = mtfs_block_sigsinv(MTFS_FATAL_SIGS);               \
        else                                                                   \
                __blocked = mtfs_block_sigsinv(0);                             \
                                                                               \
        for (;;) {                                                             \
                unsigned       __wstate;                                       \
                                                                               \
                __wstate = info->mwi_on_signal != NULL &&                      \
                           (__timeout == 0 || __allow_intr) ?                  \
                           TASK_INTERRUPTIBLE : TASK_UNINTERRUPTIBLE;          \
                                                                               \
                set_current_state(TASK_INTERRUPTIBLE);                         \
                                                                               \
                if (condition)                                                 \
                        break;                                                 \
                                                                               \
                if (__timeout == 0) {                                          \
                        schedule();                                            \
                } else {                                                       \
                        long interval = info->mwi_interval?                    \
                                             min_t(long,                       \
                                                 info->mwi_interval,__timeout):\
                                             __timeout;                        \
                        long remaining = schedule_timeout(interval);           \
                        __timeout = __timeout - (interval - remaining);        \
                        if (__timeout == 0) {                                  \
                                if (info->mwi_on_timeout == NULL ||            \
                                    info->mwi_on_timeout(info->mwi_cb_data)) { \
                                        ret = -ETIMEDOUT;                      \
                                        break;                                 \
                                }                                              \
                                /* Take signals after the timeout expires. */  \
                                if (info->mwi_on_signal != NULL)               \
                                    (void)mtfs_block_sigsinv(MTFS_FATAL_SIGS); \
                        }                                                      \
                }                                                              \
                                                                               \
                if (condition)                                                 \
                        break;                                                 \
                if (signal_pending(current)) {                                 \
                        if (info->mwi_on_signal != NULL &&                     \
                            (__timeout == 0 || __allow_intr)) {                \
                                if (info->mwi_on_signal != MWI_ON_SIGNAL_NOOP) \
                                        info->mwi_on_signal(info->mwi_cb_data);\
                                ret = -EINTR;                                  \
                                break;                                         \
                        }                                                      \
                        /* We have to do this here because some signals */     \
                        /* are not blockable - ie from strace(1).       */     \
                        /* In these cases we want to schedule_timeout() */     \
                        /* again, because we don't want that to return  */     \
                        /* -EINTR when the RPC actually succeeded.      */     \
                        /* the RECALC_SIGPENDING below will deliver the */     \
                        /* signal properly.                             */     \
                        mtfs_clear_sigpending();                               \
                }                                                              \
        }                                                                      \
                                                                               \
        mtfs_block_sigs(__blocked);                                            \
                                                                               \
        set_current_state(TASK_RUNNING);                                       \
        remove_wait_queue(&wq, &__wait);                                       \
} while (0)

#define mtfs_wait_event(wq, condition, info)                    \
({                                                              \
    int                 __ret;                                  \
    struct mtfs_wait_info *__info = (info);                     \
                                                                \
    __mtfs_wait_event(wq, condition, __info,                    \
                      __ret, add_wait_queue);                   \
    __ret;                                                      \
})

#define mtfs_wait_event_exclusive_head(wq, condition, info)     \
({                                                              \
    int                 __ret;                                  \
    struct mtfs_wait_info *__info = (info);                     \
                                                                \
    __mtfs_wait_event(wq, condition, __info,                    \
                      __ret, mtfs_waitq_add_exclusive_head);    \
    __ret;                                                      \
})

#define mtfs_wait_condition(wq, condition)                      \
({                                                              \
    struct mtfs_wait_info mwi = { 0 };                          \
    mtfs_wait_event(wq, condition, &mwi);                       \
})

sigset_t mtfs_block_sigs(sigset_t bits);
sigset_t mtfs_block_sigsinv(unsigned long sigs);
void mtfs_clear_sigpending(void);
void mtfs_daemonize(char *str);
void
mtfs_waitq_add_exclusive_head(wait_queue_head_t *waitq, wait_queue_t *link);
static inline void mtfs_pause(signed long timeout)
{
        set_current_state(TASK_UNINTERRUPTIBLE);
        schedule_timeout(timeout);
}

#ifdef HAVE_SET_CPUS_ALLOWED
#define mtfs_set_cpus_allowed(task, mask)  set_cpus_allowed(task, mask)
#else
#define mtfs_set_cpus_allowed(task, mask)  set_cpus_allowed_ptr(task, &(mask))
#endif

#if !defined(HAVE_NODE_TO_CPUMASK) && defined(HAVE_CPUMASK_OF_NODE)
#define node_to_cpumask(i)         (*(cpumask_of_node(i)))
#define HAVE_NODE_TO_CPUMASK
#endif

static inline long mtfs_time_seconds(int seconds)
{
        return ((long)seconds) * HZ;
}

#define mtfs_kthread_run(fn, data, fmt, arg...) kthread_run(fn, data, fmt, ##arg)

#else /* defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* ! defined (__linux__) && defined(__KERNEL__) */

#endif /*__MTFS_THREAD_H__ */
