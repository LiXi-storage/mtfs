/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_THREAD_INTERNAL_H__
#define __MTFS_THREAD_INTERNAL_H__

struct l_wait_info {
        long lwi_timeout;
        long lwi_interval;
        int   lwi_allow_intr;
        int  (*lwi_on_timeout)(void *);
        void (*lwi_on_signal)(void *);
        void  *lwi_cb_data;
};

/* NB: LWI_TIMEOUT ignores signals completely */
#define LWI_TIMEOUT(time, cb, data)         \
((struct l_wait_info) {                     \
    .lwi_timeout    = time,                 \
    .lwi_on_timeout = cb,                   \
    .lwi_cb_data    = data,                 \
    .lwi_interval   = 0,                    \
    .lwi_allow_intr = 0                     \
})

#define LWI_TIMEOUT_INTERVAL(time, interval, cb, data) \
((struct l_wait_info) {                                \
    .lwi_timeout    = time,                            \
    .lwi_on_timeout = cb,                              \
    .lwi_cb_data    = data,                            \
    .lwi_interval   = interval,                        \
    .lwi_allow_intr = 0                                \
})

#define LWI_TIMEOUT_INTR(time, time_cb, sig_cb, data)  \
((struct l_wait_info) {                                \
    .lwi_timeout    = time,                            \
    .lwi_on_timeout = time_cb,                         \
    .lwi_on_signal  = sig_cb,                          \
    .lwi_cb_data    = data,                            \
    .lwi_interval   = 0,                               \
    .lwi_allow_intr = 0                                \
})

#define LWI_TIMEOUT_INTR_ALL(time, time_cb, sig_cb, data) \
((struct l_wait_info) {                                   \
    .lwi_timeout    = time,                               \
    .lwi_on_timeout = time_cb,                            \
    .lwi_on_signal  = sig_cb,                             \
    .lwi_cb_data    = data,                               \
    .lwi_interval   = 0,                                  \
    .lwi_allow_intr = 1                                   \
})

#define LWI_INTR(cb, data)  LWI_TIMEOUT_INTR(0, NULL, cb, data)

#define LWI_ON_SIGNAL_NOOP ((void (*)(void *))(-1))

#define MTFS_FATAL_SIGS (sigmask(SIGKILL) | sigmask(SIGINT)  |               \
                         sigmask(SIGTERM) | sigmask(SIGQUIT) |               \
                         sigmask(SIGALRM))
/*
 * wait for @condition to become true, but no longer than timeout, specified
 * by @info.
 */
#define __l_wait_event(wq, condition, info, ret, l_add_wait)                   \
do {                                                                           \
    wait_queue_t __wait;                                                       \
    long         __timeout = info->lwi_timeout;                                \
    sigset_t     __blocked;                                                    \
    int          __allow_intr = info->lwi_allow_intr;                          \
                                                                               \
        ret = 0;                                                               \
        if (condition)                                                         \
                break;                                                         \
                                                                               \
        init_waitqueue_entry(&__wait, current);                                \
        l_add_wait(&wq, &__wait);                                              \
                                                                               \
        /* Block all signals (just the non-fatal ones if no timeout). */       \
        if (info->lwi_on_signal != NULL && (__timeout == 0 || __allow_intr))   \
                __blocked = mtfs_block_sigsinv(MTFS_FATAL_SIGS);             \
        else                                                                   \
                __blocked = mtfs_block_sigsinv(0);                             \
                                                                               \
        for (;;) {                                                             \
                unsigned       __wstate;                                       \
                                                                               \
                __wstate = info->lwi_on_signal != NULL &&                      \
                           (__timeout == 0 || __allow_intr) ?                  \
                        TASK_INTERRUPTIBLE : TASK_UNINTERRUPTIBLE;             \
                                                                               \
                set_current_state(TASK_INTERRUPTIBLE);                         \
                                                                               \
                if (condition)                                                 \
                        break;                                                 \
                                                                               \
                if (__timeout == 0) {                                          \
                        schedule();                                            \
                } else {                                                       \
                        long interval = info->lwi_interval?                    \
                                             min_t(long,                       \
                                                 info->lwi_interval,__timeout):\
                                             __timeout;                        \
                        long remaining = schedule_timeout(interval);           \
                        __timeout = __timeout - (interval - remaining);        \
                        if (__timeout == 0) {                                  \
                                if (info->lwi_on_timeout == NULL ||            \
                                    info->lwi_on_timeout(info->lwi_cb_data)) { \
                                        ret = -ETIMEDOUT;                      \
                                        break;                                 \
                                }                                              \
                                /* Take signals after the timeout expires. */  \
                                if (info->lwi_on_signal != NULL)               \
                                    (void)mtfs_block_sigsinv(MTFS_FATAL_SIGS); \
                        }                                                      \
                }                                                              \
                                                                               \
                if (condition)                                                 \
                        break;                                                 \
                if (signal_pending(current)) {                                 \
                        if (info->lwi_on_signal != NULL &&                     \
                            (__timeout == 0 || __allow_intr)) {                \
                                if (info->lwi_on_signal != LWI_ON_SIGNAL_NOOP) \
                                        info->lwi_on_signal(info->lwi_cb_data);\
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

#define l_wait_event(wq, condition, info)                   \
({                                                          \
    int                 __ret;                              \
    struct l_wait_info *__info = (info);                    \
                                                            \
    __l_wait_event(wq, condition, __info,                   \
                   __ret, add_wait_queue);                   \
    __ret;                                                  \
})

#define l_wait_condition(wq, condition)                     \
({                                                          \
    struct l_wait_info lwi = { 0 };                         \
    l_wait_event(wq, condition, &lwi);                      \
})

sigset_t mtfs_block_sigs(sigset_t bits);
sigset_t mtfs_block_sigsinv(unsigned long sigs);
void mtfs_clear_sigpending(void);
void mtfs_daemonize(char *str);
#endif /* __MTFS_THREAD_INTERNAL_H__ */
