/*
 * Copied from Lustre-2.2.0
 */

#ifndef __MTFS_LINUX_TRACEFILE_H__
#define __MTFS_LINUX_TRACEFILE_H__
#if defined(__linux__) && defined(__KERNEL__)
/**
 * three types of trace_data in linux
 */
typedef enum {
	MTFS_TCD_TYPE_PROC = 0,
	MTFS_TCD_TYPE_SOFTIRQ,
	MTFS_TCD_TYPE_IRQ,
	MTFS_TCD_TYPE_MAX
} mtfs_trace_buf_type_t;

extern char *mtfs_trace_console_buffers[NR_CPUS][MTFS_TCD_TYPE_MAX];

mtfs_trace_buf_type_t mtfs_trace_buf_idx_get(void);
#include "tracefile.h"


int  mtfs_tracefile_init_arch(void);
void mtfs_tracefile_fini_arch(void);

void
mtfs_set_ptldebug_header(struct mtfs_ptldebug_header *header,
                         struct mtfs_debug_msg_data *msgdata,
                         unsigned long stack);
void mtfs_print_to_console(struct mtfs_ptldebug_header *hdr, int mask,
                           const char *buf, int len, const char *file,
                           const char *fn);
int mtfs_trace_max_debug_mb(void);
int mtfs_trace_lock_tcd(struct mtfs_trace_cpu_data *tcd);
void mtfs_trace_unlock_tcd(struct mtfs_trace_cpu_data *tcd);
void mtfs_tracefile_read_lock(void);
void mtfs_tracefile_read_unlock(void);
void mtfs_tracefile_write_lock(void);
void mtfs_tracefile_write_unlock(void);

#endif /* defined (__linux__) && defined(__KERNEL__) */
#endif /* __MTFS_LINUX_TRACEFILE_H__ */
