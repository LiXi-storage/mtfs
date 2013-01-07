/*
 * Copied from Lustre-2.2.0
 */

#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/sysctl.h>
#include <thread.h>
#include <debug.h>
#include <compat.h>
#include <memory.h>
#include <mtfs_proc.h>
#include "linux_debug.h"
#include "linux_tracefile.h"
#include "tracefile.h"

static char debug_file_name[1024];

unsigned int mtfs_subsystem_debug = ~0;
module_param(mtfs_subsystem_debug, int, 0644);
MODULE_PARM_DESC(mtfs_debug, "MTFS kernel debug subsystem mask");
EXPORT_SYMBOL(mtfs_subsystem_debug);

unsigned int mtfs_debug = D_CANTMASK;
module_param(mtfs_debug, int, 0644);
MODULE_PARM_DESC(mtfs_debug, "MTFS kernel debug mask");
EXPORT_SYMBOL(mtfs_debug);

unsigned int mtfs_debug_mb = 0;
module_param(mtfs_debug_mb, int, 0644);
MODULE_PARM_DESC(mtfs_debug, "Total debug buffer size");
EXPORT_SYMBOL(mtfs_debug_mb);

unsigned int mtfs_printk = D_CANTMASK;
module_param(mtfs_printk, uint, 0644);
MODULE_PARM_DESC(mtfs_printk, "MTFS kernel debug console mask");
EXPORT_SYMBOL(mtfs_printk);

unsigned int mtfs_console_ratelimit = 1;
module_param(mtfs_console_ratelimit, uint, 0644);
MODULE_PARM_DESC(mtfs_console_ratelimit, "MTFS kernel debug console ratelimit (0 to disable)");
EXPORT_SYMBOL(mtfs_console_ratelimit);

unsigned int mtfs_console_max_delay;
module_param(mtfs_console_max_delay, uint, 0644);
MODULE_PARM_DESC(mtfs_console_max_delay,
                 "MTFS kernel debug console max delay (jiffies)");
EXPORT_SYMBOL(mtfs_console_max_delay);

unsigned int mtfs_console_min_delay;
module_param(mtfs_console_min_delay, uint, 0644);
MODULE_PARM_DESC(mtfs_console_min_delay,
                 "MTFS kernel debug console max delay (jiffies)");
EXPORT_SYMBOL(mtfs_console_min_delay);

unsigned int mtfs_console_backoff = MDEBUG_DEFAULT_BACKOFF;
module_param(mtfs_console_backoff, uint, 0644);
MODULE_PARM_DESC(mtfs_console_backoff,
                 "MTFS kernel debug console backoff factor");
EXPORT_SYMBOL(mtfs_console_backoff);

#define MTFS_DEBUG_FILE_PATH_DEFAULT "/tmp/mtfs-log"
char mtfs_debug_file_path_arr[PATH_MAX] = MTFS_DEBUG_FILE_PATH_DEFAULT;

char *mtfs_debug_file_path;
module_param(mtfs_debug_file_path, charp, 0644);
MODULE_PARM_DESC(mtfs_debug_file_path, "Path for dumping debug logs, "
                                       "set 'NONE' to prevent log dumping");
EXPORT_SYMBOL(mtfs_debug_file_path);

unsigned int mtfs_debug_binary = 1;
EXPORT_SYMBOL(mtfs_debug_binary);

unsigned int mtfs_stack = 3 * THREAD_SIZE / 4;
EXPORT_SYMBOL(mtfs_stack);

unsigned int mtfs_catastrophe;
EXPORT_SYMBOL(mtfs_catastrophe);

unsigned int mtfs_panic_on_mbug = 0;
module_param(mtfs_panic_on_mbug, uint, 0644);
MODULE_PARM_DESC(mtfs_debug_file_path, "MTFS panic on MBUG");

int mtfs_panic_in_progress;

wait_queue_head_t debug_ctlwq;

int mtfs_debug_init(unsigned long bufsize)
{
        int    rc = 0;
        unsigned int max = mtfs_debug_mb;

	init_waitqueue_head(&debug_ctlwq);

	if (mtfs_console_max_delay <= 0 || /* not set by user or */
	    mtfs_console_min_delay <= 0 || /* set to invalid values */
	    mtfs_console_min_delay >= mtfs_console_max_delay) {
		mtfs_console_max_delay = MDEBUG_DEFAULT_MAX_DELAY;
		mtfs_console_min_delay = MDEBUG_DEFAULT_MIN_DELAY;
	}

	if (mtfs_debug_file_path != NULL) {
		memset(mtfs_debug_file_path_arr, 0, PATH_MAX);
		strncpy(mtfs_debug_file_path_arr, 
		        mtfs_debug_file_path, PATH_MAX-1);
        }

	/*
	 * If mtfs_debug_mb is set to an invalid value or uninitialized
	 * then just make the total buffers smp_num_cpus * TCD_MAX_PAGES
	 */
	if (max > mtfs_trace_max_debug_mb() || max < mtfs_num_possible_cpus()) {
		max = TCD_MAX_PAGES;
        } else {
		max = (max / mtfs_num_possible_cpus());
		max = (max << (20 - PAGE_CACHE_SHIFT));
	}
	rc = mtfs_tracefile_init(max);

	if (rc == 0) {
		mtfs_register_panic_notifier();
	}

        return rc;
}
EXPORT_SYMBOL(mtfs_debug_init);

int mtfs_debug_cleanup(void)
{
        mtfs_unregister_panic_notifier();
        mtfs_tracefile_exit();
        return 0;
}
EXPORT_SYMBOL(mtfs_debug_cleanup);

const char *
mtfs_debug_subsys2str(int subsys)
{
        switch (1 << subsys) {
        default:
                return NULL;
        case S_UNDEFINED:
                return "undefined";
        case S_MTFS:
                return "mtfs";
        }
}

const char *
mtfs_debug_dbg2str(int debug)
{
	switch (1 << debug) {
	default:
		return NULL;
	case D_TRACE:
		return "trace";
        case D_MALLOC:
		return "malloc";
        case D_INFO:
		return "info";
        case D_WARNING:
		return "warning";
        case D_ERROR:
		return "error";
        case D_EMERG:
		return "emerg";
        case D_CONSOLE:
                return "console";
        case D_DEBUG:
                return "debug";
        case D_SPECIAL:
                return "special";
        }
}

int
mtfs_debug_mask2str(char *str, int size, int mask, int is_subsys)
{
	const char *(*fn)(int bit) = is_subsys ? mtfs_debug_subsys2str :
	                                         mtfs_debug_dbg2str;
	int           len = 0;
	const char   *token;
	int           i;

	if (mask == 0) {                        /* "0" */
		if (size > 0)
			str[0] = '0';
		len = 1;
        } else {                                /* space-separated tokens */
		for (i = 0; i < 32; i++) {
			if ((mask & (1 << i)) == 0)
				continue;

			token = fn(i);
			if (token == NULL)              /* unused bit */
				continue;

			if (len > 0) {                  /* separator? */
				if (len < size)
					str[len] = ' ';
				len++;
			}

			while (*token != 0) {
				if (len < size)
					str[len] = *token;
				token++;
				len++;
			}
		}
	}

	/* terminate 'str' */
	if (len < size)
		str[len] = 0;
	else
		str[size - 1] = 0;

	return len;
}

int mtfs_str2mask(const char *str, const char *(*bit2str)(int bit),
                  int *oldmask, int minmask, int allmask)
{
        const char *debugstr;
        char op = 0;
        int newmask = minmask, i, len, found = 0;

        /* <str> must be a list of tokens separated by whitespace
         * and optionally an operator ('+' or '-').  If an operator
         * appears first in <str>, '*oldmask' is used as the starting point
         * (relative), otherwise minmask is used (absolute).  An operator
         * applies to all following tokens up to the next operator. */
        while (*str != 0) {
                while (isspace(*str))
                        str++;
                if (*str == 0)
                        break;
                if (*str == '+' || *str == '-') {
                        op = *str++;
                        if (!found)
                                /* only if first token is relative */
                                newmask = *oldmask;
                        while (isspace(*str))
                                str++;
                        if (*str == 0)          /* trailing op */
                                return -EINVAL;
                }

                /* find token length */
                for (len = 0; str[len] != 0 && !isspace(str[len]) &&
                      str[len] != '+' && str[len] != '-'; len++);

                /* match token */
                found = 0;
                for (i = 0; i < 32; i++) {
                        debugstr = bit2str(i);
                        if (debugstr != NULL &&
                            strlen(debugstr) == len &&
                            strncasecmp(str, debugstr, len) == 0) {
                                if (op == '-')
                                        newmask &= ~(1 << i);
                                else
                                        newmask |= (1 << i);
                                found = 1;
                                break;
                        }
                }
                if (!found && len == 3 &&
                    (strncasecmp(str, "ALL", len) == 0)) {
                        if (op == '-')
                                newmask = minmask;
                        else
                                newmask = allmask;
                        found = 1;
                }
                if (!found) {
                        MWARN("unknown mask '%.*s'.\n"
                              "mask usage: [+|-]<all|type> ...\n", len, str);
                        return -EINVAL;
                }
                str += len;
        }

        *oldmask = newmask;
        return 0;
}

int
mtfs_common_mask2str(char *str, int size, int mask,
                     const char *(*mask2str)(int bit))
{
	int           len = 0;
	const char   *token;
	int           i;

	if (mask == 0) {                        /* "0" */
		if (size > 0)
			str[0] = '0';
		len = 1;
        } else {                                /* space-separated tokens */
		for (i = 0; i < 32; i++) {
			if ((mask & (1 << i)) == 0)
				continue;

			token = mask2str(i);
			if (token == NULL)              /* unused bit */
				continue;

			if (len > 0) {                  /* separator? */
				if (len < size)
					str[len] = ' ';
				len++;
			}

			while (*token != 0) {
				if (len < size)
					str[len] = *token;
				token++;
				len++;
			}
		}
	}

	/* terminate 'str' */
	if (len < size)
		str[len] = 0;
	else
		str[size - 1] = 0;

	return len;
}
EXPORT_SYMBOL(mtfs_common_mask2str);

int
mtfs_common_str2mask(int *mask, const char *str,
                     const char *(*mask2str)(int bit), int minmask)
{
        int         m = 0;
        int         matched;
        int         n;
        int         t;

        /* Allow a number for backwards compatibility */

        for (n = strlen(str); n > 0; n--)
                if (!isspace(str[n-1]))
                        break;
        matched = n;

        if ((t = sscanf(str, "%i%n", &m, &matched)) >= 1 &&
            matched == n) {
                /* don't print warning for lctl set_param debug=0 or -1 */
                if (m != 0 && m != -1)
                        MWARN("You are trying to use a numerical value for the "
                              "mask - this will be deprecated in a future "
                              "release.\n");
                *mask = m;
                return 0;
        }

        return mtfs_str2mask(str, mask2str, mask, minmask,
                            0xffffffff);
}
EXPORT_SYMBOL(mtfs_common_str2mask);

int mtfs_common_proc_dobitmasks(void *data, int write,
                                loff_t pos, void *buffer,
                                int nob, int minmask,
                                char *tmpstr, int tmpstrlen,
                                const char *(*mask2str)(int bit))
{
	int           ret;
	unsigned int *mask = data;

	if (!write) {
		mtfs_common_mask2str(tmpstr, tmpstrlen, *mask, mask2str);
		ret = strlen(tmpstr);
		if (pos >= ret) {
			ret = 0;
		} else {
			ret = mtfs_trace_copyout_string(buffer, nob,
			                               tmpstr + pos, "\n");
		}
	} else {
		ret = mtfs_trace_copyin_string(tmpstr, tmpstrlen, buffer, nob);
		if (ret < 0) {
			goto out;
		}

		ret = mtfs_common_str2mask(mask, tmpstr, mask2str, minmask);
	}

out:
	return ret;
}
EXPORT_SYMBOL(mtfs_common_proc_dobitmasks);

int
mtfs_debug_str2mask(int *mask, const char *str, int is_subsys)
{
        const char *(*fn)(int bit) = is_subsys ? mtfs_debug_subsys2str :
                                                 mtfs_debug_dbg2str;
        int         m = 0;
        int         matched;
        int         n;
        int         t;

        /* Allow a number for backwards compatibility */

        for (n = strlen(str); n > 0; n--)
                if (!isspace(str[n-1]))
                        break;
        matched = n;

        if ((t = sscanf(str, "%i%n", &m, &matched)) >= 1 &&
            matched == n) {
                /* don't print warning for lctl set_param debug=0 or -1 */
                if (m != 0 && m != -1)
                        MWARN("You are trying to use a numerical value for the "
                              "mask - this will be deprecated in a future "
                              "release.\n");
                *mask = m;
                return 0;
        }

        return mtfs_str2mask(str, fn, mask, is_subsys ? 0 : D_CANTMASK,
                            0xffffffff);
}


/**
 * Dump MTFS log to ::debug_file_path by calling tracefile_dump_all_pages()
 */
static void mtfs_debug_dumplog_internal(void *arg)
{
	void *journal_info = NULL;

	journal_info = current->journal_info;
	current->journal_info = NULL;

	if (strncmp(mtfs_debug_file_path_arr, "NONE", 4) != 0) {
		snprintf(debug_file_name, sizeof(debug_file_name) - 1,
		         "%s.%ld.%ld", mtfs_debug_file_path_arr,
		         get_seconds(), (long)arg);
		printk(KERN_ALERT "MTFS Error: dumping log to %s\n",
		       debug_file_name);
		mtfs_tracefile_dump_all_pages(debug_file_name);
		//mtfs_run_debug_log_upcall(debug_file_name);
        }
	current->journal_info = journal_info;
}

static int mtfs_debug_dumplog_thread(void *arg)
{
        mtfs_debug_dumplog_internal(arg);
        wake_up(&debug_ctlwq);
        return 0;
}

void mtfs_debug_dumplog(void)
{
        wait_queue_t wait;
        struct task_struct *dumper = NULL;
        MENTRY();

        /* we're being careful to ensure that the kernel thread is
         * able to set our state to running as it exits before we
         * get to schedule() */
	init_waitqueue_entry(&wait, current);
	set_current_state(TASK_INTERRUPTIBLE);
        add_wait_queue(&debug_ctlwq, &wait);

        dumper = mtfs_kthread_run(mtfs_debug_dumplog_thread,
                                 (void*)(long)current->pid,
                                 "mtfs_debug_dumper");
        if (IS_ERR(dumper)) {
                printk(KERN_ERR "MTFS Error: cannot start log dump thread:"
                       " %ld\n", PTR_ERR(dumper));
        } else {
                schedule();
	}

        /* be sure to teardown if cfs_create_thread() failed */
        remove_wait_queue(&debug_ctlwq, &wait);
        set_current_state(TASK_RUNNING);
}
EXPORT_SYMBOL(mtfs_debug_dumplog);

static struct ctl_table_header *mtfs_table_header = NULL;

static int __init mtfs_debug_module_init(void)
{
	int ret = 0;

	atomic_set(&mtfs_kmemory_used, 0);
	atomic_set(&mtfs_kmemory_used_max, 0);

	ret = mtfs_debug_init(5 * 1024 * 1024);
	if (ret) {
		printk(KERN_ERR "MTFS Error: failed to init debug system, ret = %d\n", ret);
		goto out;
	}

#ifdef CONFIG_SYSCTL
	if (mtfs_table_header == NULL) {
#ifdef HAVE_REGISTER_SYSCTL_2ARGS
		mtfs_table_header = register_sysctl_table(mtfs_top_table, 0);
#else /* !HAVE_REGISTER_SYSCTL_2ARGS */
		mtfs_table_header = register_sysctl_table(mtfs_top_table);
#endif /* !HAVE_REGISTER_SYSCTL_2ARGS */
	}
#endif /* CONFIG_SYSCTL */

out:
	return ret;
}

static void __exit mtfs_debug_module_exit(void)
{
#ifdef CONFIG_SYSCTL
	if (mtfs_table_header != NULL) {
		unregister_sysctl_table(mtfs_table_header);
	}

	mtfs_table_header = NULL;
#endif
	mtfs_debug_cleanup();
	MASSERT(atomic_read(&mtfs_kmemory_used) == 0);
}

MODULE_AUTHOR("MulTi File System Development Workgroup");
MODULE_DESCRIPTION("Debug support module for MulTi File System");
MODULE_LICENSE("GPL");

module_init(mtfs_debug_module_init)
module_exit(mtfs_debug_module_exit)
