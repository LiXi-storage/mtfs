/*
 * Copied from Lustre-2.2.0
 */

#ifndef __MTFS_TRACEFILE_H__
#define __MTFS_TRACEFILE_H__

#if defined(__linux__) && defined(__KERNEL__)
#include <linux/pagemap.h>
#include <mtfs_list.h>
#include <debug.h>

#ifndef get_cpu
# ifdef CONFIG_PREEMPT
#  define mtfs_get_cpu()  ({ preempt_disable(); smp_processor_id(); })
#  define mtfs_put_cpu()  preempt_enable()
# else
#  define mtfs_get_cpu()  smp_processor_id()
#  define mtfs_put_cpu()
# endif
#else
# define mtfs_get_cpu()   get_cpu()
# define mtfs_put_cpu()   put_cpu()
#endif /* get_cpu & put_cpu */

int mtfs_tracefile_init(int max_pages);
void mtfs_tracefile_exit(void);
int mtfs_trace_dump_debug_buffer_usrstr(void *usr_str, int usr_str_nob);
int mtfs_trace_allocate_string_buffer(char **str, int nob);
int mtfs_trace_copyout_string(char *usr_buffer, int usr_buffer_nob,
                              const char *knl_buffer, char *append);
int mtfs_trace_copyin_string(char *knl_buffer, int knl_buffer_nob,
                             const char *usr_buffer, int usr_buffer_nob);
void mtfs_trace_free_string_buffer(char *str, int nob);
#define TCD_MAX_PAGES (5 << (20 - PAGE_CACHE_SHIFT))
#define TCD_STOCK_PAGES (TCD_MAX_PAGES)
#define MTFS_TRACEFILE_SIZE (500 << 20)

/* Size of a buffer for sprinting console messages if we can't get a page
 * from system */
#define MTFS_TRACE_CONSOLE_BUFFER_SIZE   1024
union mtfs_trace_data_union {
	struct mtfs_trace_cpu_data {
		/*
		 * Even though this structure is meant to be per-CPU, locking
		 * is needed because in some places the data may be accessed
		 * from other CPUs. This lock is directly used in trace_get_tcd
		 * and trace_put_tcd, which are called in libmtfs_debug_vmsg2 and
		 * tcd_for_each_type_lock
		 */
		spinlock_t          tcd_lock;
		unsigned long           tcd_lock_flags;

		/*
		 * pages with trace records not yet processed by tracefiled.
		 */
		mtfs_list_t              tcd_pages;
		/* number of pages on ->tcd_pages */
		unsigned long           tcd_cur_pages;

		/*
		 * pages with trace records already processed by
		 * tracefiled. These pages are kept in memory, so that some
		 * portion of log can be written in the event of LBUG. This
		 * list is maintained in LRU order.
		 *
		 * Pages are moved to ->tcd_daemon_pages by tracefiled()
		 * (put_pages_on_daemon_list()). LRU pages from this list are
		 * discarded when list grows too large.
		 */
		mtfs_list_t              tcd_daemon_pages;
		/* number of pages on ->tcd_daemon_pages */
		unsigned long           tcd_cur_daemon_pages;

		/*
		 * Maximal number of pages allowed on ->tcd_pages and
		 * ->tcd_daemon_pages each.
		 * Always TCD_MAX_PAGES * tcd_pages_factor / 100 in current
		 * implementation.
		 */
		unsigned long           tcd_max_pages;

		/*
		 * preallocated pages to write trace records into. Pages from
		 * ->tcd_stock_pages are moved to ->tcd_pages by
		 * portals_debug_msg().
		 *
		 * This list is necessary, because on some platforms it's
		 * impossible to perform efficient atomic page allocation in a
		 * non-blockable context.
		 *
		 * Such platforms fill ->tcd_stock_pages "on occasion", when
		 * tracing code is entered in blockable context.
		 *
		 * trace_get_tage_try() tries to get a page from
		 * ->tcd_stock_pages first and resorts to atomic page
		 * allocation only if this queue is empty. ->tcd_stock_pages
		 * is replenished when tracing code is entered in blocking
		 * context (darwin-tracefile.c:trace_get_tcd()). We try to
		 * maintain TCD_STOCK_PAGES (40 by default) pages in this
		 * queue. Atomic allocation is only required if more than
		 * TCD_STOCK_PAGES pagesful are consumed by trace records all
		 * emitted in non-blocking contexts. Which is quite unlikely.
		 */
		mtfs_list_t              tcd_stock_pages;
		/* number of pages on ->tcd_stock_pages */
		unsigned long           tcd_cur_stock_pages;

		unsigned short          tcd_shutting_down;
		unsigned short          tcd_cpu;
		unsigned short          tcd_type;
		/* The factors to share debug memory. */
		unsigned short          tcd_pages_factor;
	} tcd;
	char __pad[L1_CACHE_ALIGN(sizeof(struct mtfs_trace_cpu_data))];
};

#define TCD_MAX_TYPES      8
extern union mtfs_trace_data_union (*mtfs_trace_data[TCD_MAX_TYPES])[NR_CPUS];

#ifndef num_possible_cpus
#define mtfs_num_possible_cpus() NR_CPUS
#else
#define mtfs_num_possible_cpus() num_possible_cpus()
#endif
#define mtfs_tcd_for_each(tcd, i, j)                                       \
    for (i = 0; mtfs_trace_data[i] != NULL; i++)                           \
        for (j = 0, ((tcd) = &(*mtfs_trace_data[i])[j].tcd);               \
             j < mtfs_num_possible_cpus();                                 \
             j++, (tcd) = &(*mtfs_trace_data[i])[j].tcd)

#define mtfs_tcd_for_each_type_lock(tcd, i, cpu)                           \
    for (i = 0; mtfs_trace_data[i] &&                                      \
         ((tcd) = &(*mtfs_trace_data[i])[cpu].tcd) &&                      \
         mtfs_trace_lock_tcd(tcd); mtfs_trace_unlock_tcd(tcd), i++)

struct mtfs_page_collection {
	mtfs_list_t              pc_pages;
	/*
	 * spin-lock protecting ->pc_pages. It is taken by smp_call_function()
	 * call-back functions. XXX nikita: Which is horrible: all processors
	 * receive NMI at the same time only to be serialized by this
	 * lock. Probably ->pc_pages should be replaced with an array of
	 * NR_CPUS elements accessed locklessly.
	 */
	spinlock_t          pc_lock;
	/*
	 * if this flag is set, collect_pages() will spill both
	 * ->tcd_daemon_pages and ->tcd_pages to the ->pc_pages. Otherwise,
	 * only ->tcd_pages are spilled.
	 */
	int                     pc_want_daemon_pages;
};

struct tracefiled_ctl {
	struct completion       tctl_start;
	struct completion       tctl_stop;
	wait_queue_head_t       tctl_waitq;
	pid_t                   tctl_pid;
	atomic_t                tctl_shutdown;
};

struct mtfs_trace_page {
	/*
	 * page itself
	 */
	struct page         *page;
	/*
	 * linkage into one of the lists in trace_data_union or
	 * page_collection
	 */
	mtfs_list_t          linkage;
	/*
	 * number of bytes used within this page
	 */
	unsigned int         used;
	/*
	 * cpu that owns this page
	 */
	unsigned short       cpu;
	/*
	 * type(context) of this page
	 */
	unsigned short       type;
};

/* ASSERTION that is safe to use within the debug system */
#define __MASSERT(cond)                                                    \
do {                                                                       \
        if (unlikely(!(cond))) {                                           \
                MTFS_DEBUG_MSG_DATA_DECL(msgdata, D_EMERG, NULL);          \
                mtfs_trace_assertion_failed("ASSERTION("#cond") failed",   \
                                           &msgdata);                      \
        }                                                                  \
} while (0)

#define __MASSERT_TAGE_INVARIANT(tage)                                  \
do {                                                                    \
        __MASSERT(tage != NULL);                                        \
        __MASSERT(tage->page != NULL);                                  \
        __MASSERT(tage->used <= PAGE_CACHE_SIZE);                         \
        __MASSERT(page_count(tage->page) > 0);                      \
} while (0)

void
mtfs_trace_assertion_failed(const char *str,
                            struct mtfs_debug_msg_data *msgdata);
#endif /* defined (__linux__) && defined(__KERNEL__) */
#endif /* __MTFS_TRACEFILE_H__ */
