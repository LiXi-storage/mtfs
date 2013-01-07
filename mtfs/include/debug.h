/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_DEBUG_H__
#define __MTFS_DEBUG_H__

#if defined (__linux__) && defined(__KERNEL__)
#include <linux/types.h>
#include <linux/limits.h>
#include <linux/sched.h> /* THREAD_SIZE */

extern int mtfs_debug_init(unsigned long bufsize);
extern int mtfs_debug_cleanup(void);

#if !defined(__x86_64__)
#ifdef  __ia64__
#define MDEBUG_STACK() (THREAD_SIZE -                                   \
                          ((unsigned long)__builtin_dwarf_cfa() &       \
                           (THREAD_SIZE - 1)))
#else /* !__ia64__ */
#define MDEBUG_STACK() (THREAD_SIZE -                                   \
                          ((unsigned long)__builtin_frame_address(0) &  \
                           (THREAD_SIZE - 1)))
#endif /* !__ia64__ */

#define __CHECK_STACK(msgdata, mask, cdls)                              \
do {                                                                    \
        if (unlikely(MDEBUG_STACK() > mtfs_stack)) {                  \
                mtfs_stack = MDEBUG_STACK();                          \
                (msgdata)->msg_mask = D_WARNING;                        \
                (msgdata)->msg_cdls = NULL;                             \
                mtfs_debug_msg(msgdata,                               \
                                 "maximum lustre stack %lu\n",          \
                                 MDEBUG_STACK());                       \
                (msgdata)->msg_mask = mask;                             \
                (msgdata)->msg_cdls = cdls;                             \
                dump_stack();                                           \
              /*panic("LBUG");*/                                        \
        }                                                               \
} while (0)
#define MTFS_CHECK_STACK(msgdata, mask, cdls)  __CHECK_STACK(msgdata, mask, cdls)
#else /* __x86_64__ */
#define MTFS_CHECK_STACK(msgdata, mask, cdls) do {} while(0)
#define MDEBUG_STACK() (0L)
#endif /* __x86_64__ */

/*
 *  Debugging
 */
extern unsigned int mtfs_subsystem_debug;
extern unsigned int mtfs_stack;
extern unsigned int mtfs_debug;
extern unsigned int mtfs_printk;
extern unsigned int mtfs_console_ratelimit;
extern unsigned int mtfs_watchdog_ratelimit;
extern unsigned int mtfs_console_max_delay;
extern unsigned int mtfs_console_min_delay;
extern unsigned int mtfs_console_backoff;
extern unsigned int mtfs_debug_binary;
extern char mtfs_debug_file_path_arr[PATH_MAX];
extern int mtfs_panic_in_progress;

typedef unsigned long mtfs_time_t;      /* jiffies */

typedef struct {
        mtfs_time_t     cdls_next;
        unsigned int    cdls_delay;
        int             cdls_count;
} mtfs_debug_limit_state_t;

struct mtfs_debug_msg_data {
        const char               *msg_file;
        const char               *msg_fn;
        int                      msg_subsys;
        int                      msg_line;
        int                      msg_mask;
        mtfs_debug_limit_state_t  *msg_cdls;
};

extern int mtfs_debug_msg(struct mtfs_debug_msg_data *msgdata,
                   const char *format, ...);
extern int mtfs_debug_mask2str(char *str, int size, int mask, int is_subsys);
extern int
mtfs_debug_str2mask(int *mask, const char *str, int is_subsys);

#define S_MTFS        0x00000002
#define S_UNDEFINED   0x00000001

#define D_TRACE       0x00000001 /* ENTRY/EXIT markers */
#define D_INFO        0x00000002
#define D_ERROR       0x00000004
#define D_EMERG       0x00000008
#define D_WARNING     0x00000010
#define D_CONSOLE     0x00000020
#define D_MALLOC      0x00000040
#define D_DEBUG       0x00000080
#define D_SPECIAL     0x00000100

#define D_CANTMASK   (D_ERROR | D_EMERG | D_WARNING | D_CONSOLE)

#define MDEBUG_DEFAULT_MAX_DELAY (600 * HZ)         /* jiffies */
#define MDEBUG_DEFAULT_MIN_DELAY ((1 * HZ + 1) / 2) /* jiffies */
#define MDEBUG_DEFAULT_BACKOFF   2

#define DEBUG_SUBSYSTEM S_MTFS

#define MTFS_DEBUG_MSG_DATA_DECL(dataname, mask, cdls)      \
        static struct mtfs_debug_msg_data dataname = {      \
               .msg_subsys = DEBUG_SUBSYSTEM,               \
               .msg_file   = __FILE__,                      \
               .msg_fn     = __FUNCTION__,                  \
               .msg_line   = __LINE__,                      \
               .msg_cdls   = (cdls)         };              \
        dataname.msg_mask   = (mask);

/**
 * Filters out logging messages based on mask and subsystem.
 */
static inline int mtfs_debug_show(unsigned int mask, unsigned int subsystem)
{
	return mask & D_CANTMASK ||
	((mtfs_debug & mask) && (mtfs_subsystem_debug & subsystem));
}

#define __MDEBUG(cdls, mask, format, ...)                               \
do {                                                                    \
        MTFS_DEBUG_MSG_DATA_DECL(msgdata, mask, cdls);                  \
                                                                        \
        MTFS_CHECK_STACK(&msgdata, mask, cdls);                         \
                                                                        \
        if (mtfs_debug_show(mask, DEBUG_SUBSYSTEM))                     \
                mtfs_debug_msg(&msgdata, format, ## __VA_ARGS__);       \
} while (0)

#define MDEBUG_NOLIMIT(mask, format, ...) __MDEBUG(NULL, mask, format, ## __VA_ARGS__)

#define MDEBUG_LIMIT(mask, format, ...)                \
do {                                                   \
        static mtfs_debug_limit_state_t cdls;          \
                                                       \
        __MDEBUG(&cdls, mask, format, ## __VA_ARGS__); \
} while (0)

/* Has there been an BUG? */
extern unsigned int mtfs_catastrophe;

#define MDEBUG_MEM(format, args...) MDEBUG_LIMIT(D_MALLOC, format, ##args)
#define MTRACE(format, args...)     MDEBUG_LIMIT(D_TRACE, format, ##args)
#define MWARN(format, args...)      MDEBUG_LIMIT(D_WARNING, format, ##args)
#define MERROR(format, args...)     MDEBUG_LIMIT(D_ERROR, format, ##args)
#define MDEBUG(format, args...)     MDEBUG_LIMIT(D_INFO, format, ##args)
#define MERROR(format, args...)     MDEBUG_LIMIT(D_ERROR, format, ##args)
#define MPRINT(format, args...)     MDEBUG_LIMIT(D_INFO, format, ##args)

#include <linux/err.h>
#include <linux/string.h>
#include <linux/kernel.h>

#define MBUG()                                       \
do {                                                 \
    MERROR("a bug found!\n");                        \
    panic("MTFS BUG");                               \
} while(0)

#define MASSERT(cond) MASSERTF(cond, "\n")

#define MASSERTF(cond, format, args...)              \
do {                                                 \
    if (!(cond)) {                                   \
    	MERROR("ASSERTION( %s ) failed: "            \
               format, #cond, ##args);               \
        MBUG();                                      \
    }                                                \
} while(0)

#else /* !(defined(__linux__) && defined(__KERNEL__)) */
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <asm/types.h>

#define MBUG()                                       \
do {                                                 \
    MERROR("a bug found!\n");                        \
    assert(0);                                       \
} while(0)

#define MASSERT(cond) assert(cond)
#if 0
#define MTRACE(format, args...) fprintf(stderr, "DEBUG: %s(%d) %s(): " format, __FILE__,  __LINE__, __FUNCTION__, ##args)
#else
#define MTRACE(format, args...)
#endif
#define MDEBUG(format, args...) fprintf(stderr, "DEBUG: %s(%d) %s(): " format, __FILE__,  __LINE__, __FUNCTION__, ##args)
#define MERROR(format, args...) fprintf(stderr, "ERROR: %s(%d) %s(): " format, __FILE__,  __LINE__, __FUNCTION__, ##args)
#define MWARN(format, args...)  fprintf(stderr, "WARN: %s(%d) %s(): " format, __FILE__,  __LINE__, __FUNCTION__, ##args)
#define MPRINT(format, args...) fprintf(stdout, format, ##args)
#define MFLUSH() fflush(stdout)

#define IS_ERR(a) ((unsigned long)(a) > (unsigned long)-1000L)
#define PTR_ERR(a) ((long)(a))
#define ERR_PTR(a) ((void*)((long)(a)))

#endif /* !(defined (__linux__) && defined(__KERNEL__)) */

#if defined(__GNUC__)
#define MENTRY()                                                        \
do {                                                                    \
        MTRACE("entered\n");                                            \
} while (0)

#define _MRETURN()                                                      \
do {                                                                    \
        MTRACE("leaving\n");                                            \
        return;                                                         \
} while (0)

#define MRETURN(ret)                                                    \
do {                                                                    \
        typeof(ret) RETURN__ret = (ret);                                \
        MTRACE("leaving (ret = %lu:%ld:0x%lx)\n",                       \
               (long)RETURN__ret, (long)RETURN__ret, (long)RETURN__ret);\
        return RETURN__ret;                                             \
} while (0)

#else
# error "Unkown compiler"
#endif /* __GNUC__ */

#define PH_FLAG_FIRST_RECORD 1

/**
 * Format for debug message headers
 */
struct mtfs_ptldebug_header {
	__u32 ph_len;
	__u32 ph_flags;
	__u32 ph_subsys;
	__u32 ph_mask;
	__u16 ph_cpu_id;
	__u16 ph_type;
	__u32 ph_sec;
	__u64 ph_usec;
	__u32 ph_stack;
	__u32 ph_pid;
	__u32 ph_extern_pid;
	__u32 ph_line_num;
} __attribute__((packed));

#endif /* __MTFS_DEBUG_H__ */
