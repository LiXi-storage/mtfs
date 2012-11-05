/*
 * Copied from Lustre-2.2.0
 */
#include <linux/threads.h>
#include <linux/cache.h>
#include <linux/rwsem.h>
#include <linux/mm.h>
#include <linux/hardirq.h>
#include <linux/sched.h>
#include <mtfs_common.h>
#include "tracefile.h"
#include "linux_tracefile.h"

/* percents to share the total debug memory for each type */
static unsigned int pages_factor[MTFS_TCD_TYPE_MAX] = {
	80,  /* 80% pages for MTFS_TCD_TYPE_PROC */
	10,  /* 10% pages for MTFS_TCD_TYPE_SOFTIRQ */
	10   /* 10% pages for MTFS_TCD_TYPE_IRQ */
};

char *mtfs_trace_console_buffers[NR_CPUS][MTFS_TCD_TYPE_MAX];

struct rw_semaphore mtfs_tracefile_sem;

void mtfs_tracefile_fini_arch(void)
{
	int i;
	int j;

	for (i = 0; i < num_possible_cpus(); i++) {
		for (j = 0; j < MTFS_TCD_TYPE_MAX; j++) {
			if (mtfs_trace_console_buffers[i][j] != NULL) {
				kfree(mtfs_trace_console_buffers[i][j]);
				mtfs_trace_console_buffers[i][j] = NULL;
			}
		}
	}

	for (i = 0; mtfs_trace_data[i] != NULL; i++) {
		kfree(mtfs_trace_data[i]);
		mtfs_trace_data[i] = NULL;
	}

	//mtfs_fini_rwsem(&mtfs_tracefile_sem);
}

int mtfs_tracefile_init_arch(void)
{
	int    i = 0;
	int    j = 0;
	struct mtfs_trace_cpu_data *tcd = NULL;
	int    ret = 0;

	init_rwsem(&mtfs_tracefile_sem);

	/* initialize trace_data */
	memset(mtfs_trace_data, 0, sizeof(mtfs_trace_data));
	for (i = 0; i < MTFS_TCD_TYPE_MAX; i++) {
		mtfs_trace_data[i] =
		      kmalloc(sizeof(union mtfs_trace_data_union) * NR_CPUS,
		      GFP_KERNEL);
		if (mtfs_trace_data[i] == NULL) {
			printk(KERN_ERR "lnet: Not enough memory\n");
			ret = -ENOMEM;
			goto out;
		}
	}

	/* arch related info initialized */
	mtfs_tcd_for_each(tcd, i, j) {
		spin_lock_init(&tcd->tcd_lock);
		tcd->tcd_pages_factor = pages_factor[i];
		tcd->tcd_type = i;
		tcd->tcd_cpu = j;
	}

	for (i = 0; i < num_possible_cpus(); i++) {
		for (j = 0; j < MTFS_TCD_TYPE_MAX; j++) {
			mtfs_trace_console_buffers[i][j] =
			kmalloc(MTFS_TRACE_CONSOLE_BUFFER_SIZE,
			        GFP_KERNEL);

			if (mtfs_trace_console_buffers[i][j] == NULL) {
				printk(KERN_ERR "MTFS: Not enough memory\n");
				ret = -ENOMEM;
				goto out_fini;
			}
		}
	}
	goto out;
	
out_fini:
	mtfs_tracefile_fini_arch();
out:
	return ret;
}

void mtfs_tracefile_read_lock(void)
{
	down_read(&mtfs_tracefile_sem);
}

void mtfs_tracefile_read_unlock(void)
{
	up_read(&mtfs_tracefile_sem);
}

void mtfs_tracefile_write_lock(void)
{
	down_write(&mtfs_tracefile_sem);
}

void mtfs_tracefile_write_unlock(void)
{
	up_write(&mtfs_tracefile_sem);
}

mtfs_trace_buf_type_t mtfs_trace_buf_idx_get(void)
{
	if (in_irq())
		return MTFS_TCD_TYPE_IRQ;
	else if (in_softirq())
		return MTFS_TCD_TYPE_SOFTIRQ;
	else
		return MTFS_TCD_TYPE_PROC;
}

int mtfs_trace_lock_tcd(struct mtfs_trace_cpu_data *tcd)
{
	__MASSERT(tcd->tcd_type < MTFS_TCD_TYPE_MAX);
	if (tcd->tcd_type == MTFS_TCD_TYPE_IRQ) {
		spin_lock_irqsave(&tcd->tcd_lock, tcd->tcd_lock_flags);
	} else if (tcd->tcd_type == MTFS_TCD_TYPE_SOFTIRQ) {
		spin_lock_bh(&tcd->tcd_lock);
	} else {
		spin_lock(&tcd->tcd_lock);
	}
	return 1;
}

void mtfs_trace_unlock_tcd(struct mtfs_trace_cpu_data *tcd)
{
	__MASSERT(tcd->tcd_type < MTFS_TCD_TYPE_MAX);
	if (tcd->tcd_type == MTFS_TCD_TYPE_IRQ) {
		spin_unlock_irqrestore(&tcd->tcd_lock, tcd->tcd_lock_flags);
	} else if (tcd->tcd_type == MTFS_TCD_TYPE_SOFTIRQ) {
		spin_unlock_bh(&tcd->tcd_lock);
	} else {
		spin_unlock(&tcd->tcd_lock);
	}
}

void
mtfs_set_ptldebug_header(struct mtfs_ptldebug_header *header,
                         struct mtfs_debug_msg_data *msgdata,
                         unsigned long stack)
{
	struct timeval tv;

	do_gettimeofday(&tv);

	header->ph_subsys = msgdata->msg_subsys;
	header->ph_mask = msgdata->msg_mask;
	header->ph_cpu_id = smp_processor_id();
	header->ph_type = mtfs_trace_buf_idx_get();
	header->ph_sec = (__u32)tv.tv_sec;
	header->ph_usec = tv.tv_usec;
	header->ph_stack = stack;
	header->ph_pid = current->pid;
	header->ph_line_num = msgdata->msg_line;
	header->ph_extern_pid = 0;
	return;
}

void mtfs_print_to_console(struct mtfs_ptldebug_header *hdr, int mask,
                           const char *buf, int len, const char *file,
                           const char *fn)
{
	char *prefix = "MTFS", *ptype = NULL;

	if ((mask & D_EMERG) != 0) {
		prefix = "MTFS Error";
		ptype = KERN_EMERG;
	} else if ((mask & D_ERROR) != 0) {
		prefix = "MTFS Error";
		ptype = KERN_ERR;
	} else if ((mask & D_WARNING) != 0) {
		prefix = "MTFS Warning";
		ptype = KERN_WARNING;
	} else if ((mask & (D_CONSOLE | mtfs_printk)) != 0) {
		prefix = "MTFS";
		ptype = KERN_INFO;
	}

	if ((mask & D_CONSOLE) != 0) {
		printk("%s%s:%.*s", ptype, prefix, len, buf);
	} else {
		printk("%s%s:%d:%d:(%s:%d:%s()) %.*s", ptype, prefix,
                       hdr->ph_pid, hdr->ph_extern_pid, file, hdr->ph_line_num,
                       fn, len, buf);
	}
	return;
}

int mtfs_trace_max_debug_mb(void)
{
	int  total_mb = (num_physpages >> (20 - PAGE_SHIFT));

	return MAX(512, (total_mb * 80)/100);
}

