/*
 * Copied from Lustre-2.2.0
 */
#include <linux/threads.h>
#include <linux/cache.h>
#include <linux/rwsem.h>
#include <linux/ctype.h>
#include <linux/module.h>
#include <linux/sched.h>

#include <compat.h>
#include <debug.h>
#include <memory.h>
#include "tracefile.h"
#include "linux_tracefile.h"

#ifndef HAVE_FILE_FSYNC_2ARGS
# define mtfs_do_fsync(fp, flag)    ((fp)->f_op->fsync(fp, (fp)->f_dentry, flag))
#else
# define mtfs_do_fsync(fp, flag)    ((fp)->f_op->fsync(fp, flag))
#endif
#define mtfs_filp_fsync(fp)                  mtfs_do_fsync(fp, 1)

union mtfs_trace_data_union
        (*mtfs_trace_data[TCD_MAX_TYPES])[NR_CPUS] __cacheline_aligned;

static struct tracefiled_ctl trace_tctl;
static int thread_running = 0;

atomic_t mtfs_tage_allocated = ATOMIC_INIT(0);

#define MTFS_DECL_MMSPACE                mm_segment_t __oldfs
#define MTFS_MMSPACE_OPEN \
        do { __oldfs = get_fs(); set_fs(get_ds());} while(0)
#define MTFS_MMSPACE_CLOSE               set_fs(__oldfs)

static inline struct mtfs_trace_page *
mtfs_tage_from_list(mtfs_list_t *list)
{
        return mtfs_list_entry(list, struct mtfs_trace_page, linkage);
}

static struct mtfs_trace_page *mtfs_tage_alloc(int gfp)
{
        struct page            *page;
        struct mtfs_trace_page *tage;

        /* My caller is trying to free memory */
        if (!in_interrupt() && (current->flags & PF_MEMALLOC))
                return NULL;

        /*
         * Don't spam console with allocation failures: they will be reported
         * by upper layer anyway.
         */
        gfp |= __GFP_NOWARN;
        page = alloc_page(gfp);
        if (page == NULL)
                return NULL;

	tage = kmalloc(sizeof(*tage), gfp);
	if (unlikely(tage == NULL)) {
		__free_page(page);
		return NULL;
	} 
	memset(tage, 0, sizeof(*tage));

        tage->page = page;
        atomic_inc(&mtfs_tage_allocated);
        return tage;
}


static void mtfs_tage_free(struct mtfs_trace_page *tage)
{
        __MASSERT(tage != NULL);
        __MASSERT(tage->page != NULL);

        __free_page(tage->page);
        kfree(tage);
        atomic_dec(&mtfs_tage_allocated);
}

static void mtfs_tage_to_tail(struct mtfs_trace_page *tage,
                             mtfs_list_t *queue)
{
        __MASSERT(tage != NULL);
        __MASSERT(queue != NULL);

        mtfs_list_move_tail(&tage->linkage, queue);
}

static void
mtfs_panic_collect_pages(struct mtfs_page_collection *pc)
{
        /* Do the collect_pages job on a single CPU: assumes that all other
         * CPUs have been stopped during a panic.  If this isn't true for some
         * arch, this will have to be implemented separately in each arch.  */
        int                        i;
        int                        j;
        struct mtfs_trace_cpu_data *tcd;

        MTFS_INIT_LIST_HEAD(&pc->pc_pages);

        mtfs_tcd_for_each(tcd, i, j) {
                mtfs_list_splice_init(&tcd->tcd_pages, &pc->pc_pages);
                tcd->tcd_cur_pages = 0;

                if (pc->pc_want_daemon_pages) {
                        mtfs_list_splice_init(&tcd->tcd_daemon_pages,
                                             &pc->pc_pages);
                        tcd->tcd_cur_daemon_pages = 0;
                }
        }
}

static void mtfs_collect_pages_on_all_cpus(struct mtfs_page_collection *pc)
{
        struct mtfs_trace_cpu_data *tcd;
        int i, cpu;

        spin_lock(&pc->pc_lock);
        mtfs_for_each_possible_cpu(cpu) {
                mtfs_tcd_for_each_type_lock(tcd, i, cpu) {
                        mtfs_list_splice_init(&tcd->tcd_pages, &pc->pc_pages);
                        tcd->tcd_cur_pages = 0;
                        if (pc->pc_want_daemon_pages) {
                                mtfs_list_splice_init(&tcd->tcd_daemon_pages,
                                                     &pc->pc_pages);
                                tcd->tcd_cur_daemon_pages = 0;
                        }
                }
        }
        spin_unlock(&pc->pc_lock);
}

static void mtfs_collect_pages(struct mtfs_page_collection *pc)
{
	MTFS_INIT_LIST_HEAD(&pc->pc_pages);

        if (mtfs_panic_in_progress)
		mtfs_panic_collect_pages(pc);
	else
		mtfs_collect_pages_on_all_cpus(pc);
}

static void mtfs_put_pages_back_on_all_cpus(struct mtfs_page_collection *pc)
{
        struct mtfs_trace_cpu_data *tcd;
        mtfs_list_t *cur_head;
        struct mtfs_trace_page *tage;
        struct mtfs_trace_page *tmp;
        int i, cpu;

        spin_lock(&pc->pc_lock);
        mtfs_for_each_possible_cpu(cpu) {
                mtfs_tcd_for_each_type_lock(tcd, i, cpu) {
                        cur_head = tcd->tcd_pages.next;

                        mtfs_list_for_each_entry_safe_typed(tage, tmp,
                                                           &pc->pc_pages,
                                                           struct mtfs_trace_page,
                                                           linkage) {

                                __MASSERT_TAGE_INVARIANT(tage);

                                if (tage->cpu != cpu || tage->type != i)
                                        continue;

                                mtfs_tage_to_tail(tage, cur_head);
                                tcd->tcd_cur_pages++;
                        }
                }
        }
        spin_unlock(&pc->pc_lock);
}


static void mtfs_put_pages_back(struct mtfs_page_collection *pc)
{
        if (!mtfs_panic_in_progress)
                mtfs_put_pages_back_on_all_cpus(pc);
}

static void put_pages_on_tcd_daemon_list(struct mtfs_page_collection *pc,
                                         struct mtfs_trace_cpu_data *tcd)
{
        struct mtfs_trace_page *tage;
        struct mtfs_trace_page *tmp;

        spin_lock(&pc->pc_lock);
        mtfs_list_for_each_entry_safe_typed(tage, tmp, &pc->pc_pages,
                                           struct mtfs_trace_page, linkage) {

                __MASSERT_TAGE_INVARIANT(tage);

                if (tage->cpu != tcd->tcd_cpu || tage->type != tcd->tcd_type)
                        continue;

                mtfs_tage_to_tail(tage, &tcd->tcd_daemon_pages);
                tcd->tcd_cur_daemon_pages++;

                if (tcd->tcd_cur_daemon_pages > tcd->tcd_max_pages) {
                        struct mtfs_trace_page *victim;

                        __MASSERT(!mtfs_list_empty(&tcd->tcd_daemon_pages));
                        victim = mtfs_tage_from_list(tcd->tcd_daemon_pages.next);

                        __MASSERT_TAGE_INVARIANT(victim);

                        mtfs_list_del(&victim->linkage);
                        mtfs_tage_free(victim);
                        tcd->tcd_cur_daemon_pages--;
                }
        }
        spin_unlock(&pc->pc_lock);
}

#if 0
static void put_pages_on_daemon_list(struct mtfs_page_collection *pc)
{
        struct mtfs_trace_cpu_data *tcd;
        int i, cpu;

        mtfs_for_each_possible_cpu(cpu) {
                mtfs_tcd_for_each_type_lock(tcd, i, cpu)
                        put_pages_on_tcd_daemon_list(pc, tcd);
        }
}
#endif

int mtfs_tracefile_dump_all_pages(char *filename)
{
        struct mtfs_page_collection pc;
        struct file *filp;
        struct mtfs_trace_page *tage;
        struct mtfs_trace_page *tmp;
        int rc;

        MTFS_DECL_MMSPACE;

        mtfs_tracefile_write_lock();

        filp = filp_open(filename,
                         O_CREAT|O_EXCL|O_WRONLY|O_LARGEFILE, 0600);
	if (IS_ERR(filp)) {
		rc = PTR_ERR(filp);
		if (rc != -EEXIST) {
			printk(KERN_ERR
			       "MTFSError: can't open %s for dump,"
			       " ret = %d\n",
			       filename, rc);
			goto out;
		}
	}

	spin_lock_init(&pc.pc_lock);
	pc.pc_want_daemon_pages = 1;
	mtfs_collect_pages(&pc);
	if (mtfs_list_empty(&pc.pc_pages)) {
		rc = 0;
		goto close;
	}

	/* ok, for now, just write the pages.  in the future we'll be building
	 * iobufs with the pages and calling generic_direct_IO */
	MTFS_MMSPACE_OPEN;
	mtfs_list_for_each_entry_safe_typed(tage, tmp, &pc.pc_pages,
	                                   struct mtfs_trace_page, linkage) {

		__MASSERT_TAGE_INVARIANT(tage);

		rc = (filp)->f_op->write(filp, page_address(tage->page),
		                         tage->used, (&(filp->f_pos)));
		if (rc != (int)tage->used) {
			printk(KERN_WARNING "wanted to write %u but wrote "
			       "%d\n", tage->used, rc);
			mtfs_put_pages_back(&pc);
			__MASSERT(mtfs_list_empty(&pc.pc_pages));
			break;
		}
		mtfs_list_del(&tage->linkage);
		mtfs_tage_free(tage);
	}
	MTFS_MMSPACE_CLOSE;
	rc = mtfs_filp_fsync(filp);
	if (rc)
		printk(KERN_ERR "sync returns %d\n", rc);
 close:
	filp_close(filp, NULL);
 out:
	mtfs_tracefile_write_unlock();
	return rc;
}

int mtfs_trace_copyin_string(char *knl_buffer, int knl_buffer_nob,
                             const char *usr_buffer, int usr_buffer_nob)
{
	int    nob;

	if (usr_buffer_nob > knl_buffer_nob)
		return -EOVERFLOW;

	if (copy_from_user((void *)knl_buffer,
	    (void *)usr_buffer, usr_buffer_nob))
		return -EFAULT;

	nob = strnlen(knl_buffer, usr_buffer_nob);
	while (nob-- >= 0)                      /* strip trailing whitespace */
		if (!isspace(knl_buffer[nob]))
			break;

	if (nob < 0)                            /* empty string */
		return -EINVAL;

	if (nob == knl_buffer_nob)              /* no space to terminate */
		return -EOVERFLOW;

	knl_buffer[nob + 1] = 0;                /* terminate */
	return 0;
}

int mtfs_trace_copyout_string(char *usr_buffer, int usr_buffer_nob,
                              const char *knl_buffer, char *append)
{
        /* NB if 'append' != NULL, it's a single character to append to the
         * copied out string - usually "\n", for /proc entries and "" (i.e. a
         * terminating zero byte) for sysctl entries */
        int   nob = strlen(knl_buffer);

        if (nob > usr_buffer_nob)
                nob = usr_buffer_nob;

        if (copy_to_user(usr_buffer, knl_buffer, nob))
                return -EFAULT;

        if (append != NULL && nob < usr_buffer_nob) {
                if (copy_to_user(usr_buffer + nob, append, 1))
                        return -EFAULT;

                nob++;
        }

        return nob;
}

int mtfs_trace_allocate_string_buffer(char **str, int nob)
{
	if (nob > 2 * PAGE_CACHE_SIZE)            /* string must be "sensible" */
		return -EINVAL;

	*str = kmalloc(nob, __GFP_IO | __GFP_FS);
	if (unlikely(*str == NULL)) {
		return -ENOMEM;
	} 
	memset(*str, 0, nob);

        return 0;
}

void mtfs_trace_free_string_buffer(char *str, int nob)
{
	kfree(str);
}

int mtfs_trace_dump_debug_buffer_usrstr(void *usr_str, int usr_str_nob)
{
        char         *str;
        int           rc;

        rc = mtfs_trace_allocate_string_buffer(&str, usr_str_nob + 1);
        if (rc != 0)
                return rc;

        rc = mtfs_trace_copyin_string(str, usr_str_nob + 1,
                                     usr_str, usr_str_nob);
        if (rc != 0)
                goto out;

#if !defined(__WINNT__)
        if (str[0] != '/') {
                rc = -EINVAL;
                goto out;
        }
#endif
        rc = mtfs_tracefile_dump_all_pages(str);
out:
        mtfs_trace_free_string_buffer(str, usr_str_nob + 1);
        return rc;
}

int mtfs_tracefile_init(int max_pages)
{
	struct mtfs_trace_cpu_data *tcd;
	int                         i;
        int                         j;
        int                         ret;
        int                         factor;

	ret = mtfs_tracefile_init_arch();
	if (ret != 0) {
		goto out;
	}

	mtfs_tcd_for_each(tcd, i, j) {
		/* tcd_pages_factor is initialized int tracefile_init_arch. */
		factor = tcd->tcd_pages_factor;
		MTFS_INIT_LIST_HEAD(&tcd->tcd_pages);
		MTFS_INIT_LIST_HEAD(&tcd->tcd_stock_pages);
		MTFS_INIT_LIST_HEAD(&tcd->tcd_daemon_pages);
		tcd->tcd_cur_pages = 0;
		tcd->tcd_cur_stock_pages = 0;
		tcd->tcd_cur_daemon_pages = 0;
		tcd->tcd_max_pages = (max_pages * factor) / 100;
		if (tcd->tcd_max_pages <= 0) {
			/* Debug system is not built up yet, so printk instead */
			printk(KERN_ERR "MTFS: got zero tcd_max_pages\n");
			panic("MTFS BUG");
		}
		tcd->tcd_shutting_down = 0;
        }

	
out:
        return ret;
}

static void trace_cleanup_on_all_cpus(void)
{
	struct mtfs_trace_cpu_data *tcd;
	struct mtfs_trace_page *tage;
	struct mtfs_trace_page *tmp;
	int i, cpu;

	mtfs_for_each_possible_cpu(cpu) {
		mtfs_tcd_for_each_type_lock(tcd, i, cpu) {
			tcd->tcd_shutting_down = 1;

			mtfs_list_for_each_entry_safe_typed(tage, tmp,
			                                    &tcd->tcd_pages,
			                                    struct mtfs_trace_page,
			                                    linkage) {
				__MASSERT_TAGE_INVARIANT(tage);

				mtfs_list_del(&tage->linkage);
				mtfs_tage_free(tage);
                        }

			tcd->tcd_cur_pages = 0;
                }
        }
}

static void mtfs_trace_cleanup(void)
{
	trace_cleanup_on_all_cpus();
	mtfs_tracefile_fini_arch();
}

void mtfs_tracefile_exit(void)
{
	//mtfs_trace_stop_thread();
	mtfs_trace_cleanup();
}

void
mtfs_trace_assertion_failed(const char *str,
                           struct mtfs_debug_msg_data *msgdata)
{
        struct mtfs_ptldebug_header hdr;

        mtfs_panic_in_progress = 1;
        mtfs_catastrophe = 1;
        mb();


        mtfs_set_ptldebug_header(&hdr, msgdata, MDEBUG_STACK());

        mtfs_print_to_console(&hdr, D_EMERG, str, strlen(str),
                             msgdata->msg_file, msgdata->msg_fn);

        panic("MTFS debug assertion failure\n");

        /* not reached */
}

struct mtfs_trace_cpu_data *
mtfs_trace_get_tcd(void)
{
	struct mtfs_trace_cpu_data *tcd =
                &(*mtfs_trace_data[mtfs_trace_buf_idx_get()])[mtfs_get_cpu()].tcd;

	mtfs_trace_lock_tcd(tcd);

	return tcd;
}

/* return a page that has 'len' bytes left at the end */
static struct mtfs_trace_page *
mtfs_trace_get_tage_try(struct mtfs_trace_cpu_data *tcd, unsigned long len)
{
        struct mtfs_trace_page *tage;

        if (tcd->tcd_cur_pages > 0) {
                __MASSERT(!mtfs_list_empty(&tcd->tcd_pages));
                tage = mtfs_tage_from_list(tcd->tcd_pages.prev);
                if (tage->used + len <= PAGE_CACHE_SIZE)
                        return tage;
        }

        if (tcd->tcd_cur_pages < tcd->tcd_max_pages) {
                if (tcd->tcd_cur_stock_pages > 0) {
                        tage = mtfs_tage_from_list(tcd->tcd_stock_pages.prev);
                        -- tcd->tcd_cur_stock_pages;
                        mtfs_list_del_init(&tage->linkage);
                } else {
                        tage = mtfs_tage_alloc(GFP_ATOMIC);
                        if (tage == NULL) {
                                if (printk_ratelimit())
                                        printk(KERN_WARNING
                                               "cannot allocate a tage (%ld)\n",
                                       tcd->tcd_cur_pages);
                                return NULL;
                        }
                }

                tage->used = 0;
                tage->cpu = smp_processor_id();
                tage->type = tcd->tcd_type;
                mtfs_list_add_tail(&tage->linkage, &tcd->tcd_pages);
                tcd->tcd_cur_pages++;

                if (tcd->tcd_cur_pages > 8 && thread_running) {
                        struct tracefiled_ctl *tctl = &trace_tctl;
                        /*
                         * wake up tracefiled to process some pages.
                         */
			wake_up(&tctl->tctl_waitq);
                }
                return tage;
        }
        return NULL;
}

static void mtfs_tcd_shrink(struct mtfs_trace_cpu_data *tcd)
{
        int pgcount = tcd->tcd_cur_pages / 10;
        struct mtfs_page_collection pc;
        struct mtfs_trace_page *tage;
        struct mtfs_trace_page *tmp;

        /*
         * XXX nikita: do NOT call portals_debug_msg() (CDEBUG/ENTRY/EXIT)
         * from here: this will lead to infinite recursion.
         */

        if (printk_ratelimit())
                printk(KERN_WARNING "debug daemon buffer overflowed; "
                       "discarding 10%% of pages (%d of %ld)\n",
                       pgcount + 1, tcd->tcd_cur_pages);

        MTFS_INIT_LIST_HEAD(&pc.pc_pages);
        spin_lock_init(&pc.pc_lock);

        mtfs_list_for_each_entry_safe_typed(tage, tmp, &tcd->tcd_pages,
                                           struct mtfs_trace_page, linkage) {
                if (pgcount-- == 0)
                        break;

                mtfs_list_move_tail(&tage->linkage, &pc.pc_pages);
                tcd->tcd_cur_pages--;
        }
        put_pages_on_tcd_daemon_list(&pc, tcd);
}


/* return a page that has 'len' bytes left at the end */
static struct mtfs_trace_page *mtfs_trace_get_tage(struct mtfs_trace_cpu_data *tcd,
                                                 unsigned long len)
{
        struct mtfs_trace_page *tage;

        /*
         * XXX nikita: do NOT call portals_debug_msg() (CDEBUG/ENTRY/EXIT)
         * from here: this will lead to infinite recursion.
         */

        if (len > PAGE_CACHE_SIZE) {
                printk(KERN_ERR
                       "cowardly refusing to write %lu bytes in a page\n", len);
                return NULL;
        }

        tage = mtfs_trace_get_tage_try(tcd, len);
        if (tage != NULL)
                return tage;
        if (thread_running)
                mtfs_tcd_shrink(tcd);
        if (tcd->tcd_cur_pages > 0) {
                tage = mtfs_tage_from_list(tcd->tcd_pages.next);
                tage->used = 0;
                mtfs_tage_to_tail(tage, &tcd->tcd_pages);
        }
        return tage;
}


void
mtfs_trace_put_tcd (struct mtfs_trace_cpu_data *tcd)
{
	mtfs_trace_unlock_tcd(tcd);

	mtfs_put_cpu();
}

static inline char *
mtfs_trace_get_console_buffer(void)
{
        unsigned int i = mtfs_get_cpu();
        unsigned int j = mtfs_trace_buf_idx_get();

        return mtfs_trace_console_buffers[i][j];
}

static inline void
mtfs_trace_put_console_buffer(char *buffer)
{
        mtfs_put_cpu();
}

#define __current_nesting_level() (0)

int mtfs_debug_vmsg2(struct mtfs_debug_msg_data *msgdata,
                       const char *format1, va_list args,
                       const char *format2, ...)
{
        struct mtfs_trace_cpu_data *tcd = NULL;
        struct mtfs_ptldebug_header header = {0};
        struct mtfs_trace_page     *tage;
        /* string_buf is used only if tcd != NULL, and is always set then */
        char                      *string_buf = NULL;
        char                      *debug_buf;
        int                        known_size;
        int                        needed = 85; /* average message length */
        int                        max_nob;
        va_list                    ap;
        int                        depth;
        int                        i;
        int                        remain;
        int                        mask = msgdata->msg_mask;
        char                      *file = (char *)msgdata->msg_file;
        mtfs_debug_limit_state_t   *cdls = msgdata->msg_cdls;

	file = (char *)msgdata->msg_file;
	if (strchr(file, '/')) {
		file = strrchr(file, '/') + 1;
	}

	tcd = mtfs_trace_get_tcd();

        /* mtfs_trace_get_tcd() grabs a lock, which disables preemption and
         * pins us to a particular CPU.  This avoids an smp_processor_id()
         * warning on Linux when debugging is enabled. */
        mtfs_set_ptldebug_header(&header, msgdata, MDEBUG_STACK());

        if (tcd == NULL)                /* arch may not log in IRQ context */
                goto console;

        if (tcd->tcd_cur_pages == 0)
                header.ph_flags |= PH_FLAG_FIRST_RECORD;

        if (tcd->tcd_shutting_down) {
                mtfs_trace_put_tcd(tcd);
                tcd = NULL;
                goto console;
        }

        depth = __current_nesting_level();
        known_size = strlen(file) + 1 + depth;
        if (msgdata->msg_fn)
                known_size += strlen(msgdata->msg_fn) + 1;

        if (mtfs_debug_binary)
                known_size += sizeof(header);

        /*/
         * '2' used because vsnprintf return real size required for output
         * _without_ terminating NULL.
         * if needed is to small for this format.
         */
        for (i = 0; i < 2; i++) {
                tage = mtfs_trace_get_tage(tcd, needed + known_size + 1);
                if (tage == NULL) {
                        if (needed + known_size > PAGE_CACHE_SIZE)
                                mask |= D_ERROR;

                        mtfs_trace_put_tcd(tcd);
                        tcd = NULL;
                        goto console;
                }

                string_buf = (char *)page_address(tage->page) +
                                     tage->used + known_size;

                max_nob = PAGE_CACHE_SIZE - tage->used - known_size;
                if (max_nob <= 0) {
                        printk(KERN_EMERG "negative max_nob: %d\n",
                               max_nob);
                        mask |= D_ERROR;
                        mtfs_trace_put_tcd(tcd);
                        tcd = NULL;
                        goto console;
                }

                needed = 0;
                if (format1) {
                        va_copy(ap, args);
                        needed = vsnprintf(string_buf, max_nob, format1, ap);
                        va_end(ap);
                }

                if (format2) {
                        remain = max_nob - needed;
                        if (remain < 0)
                                remain = 0;

                        va_start(ap, format2);
                        needed += vsnprintf(string_buf + needed, remain,
                                            format2, ap);
                        va_end(ap);
                }

                if (needed < max_nob) /* well. printing ok.. */
                        break;
        }

        if (*(string_buf+needed-1) != '\n')
                printk(KERN_INFO "format at %s:%d:%s doesn't end in "
                       "newline\n", file, msgdata->msg_line, msgdata->msg_fn);

        header.ph_len = known_size + needed;
        debug_buf = (char *)page_address(tage->page) + tage->used;

        if (mtfs_debug_binary) {
                memcpy(debug_buf, &header, sizeof(header));
                tage->used += sizeof(header);
                debug_buf += sizeof(header);
        }

        /* indent message according to the nesting level */
        while (depth-- > 0) {
                *(debug_buf++) = '.';
                ++ tage->used;
        }

        strcpy(debug_buf, file);
        tage->used += strlen(file) + 1;
        debug_buf += strlen(file) + 1;

        if (msgdata->msg_fn) {
                strcpy(debug_buf, msgdata->msg_fn);
                tage->used += strlen(msgdata->msg_fn) + 1;
                debug_buf += strlen(msgdata->msg_fn) + 1;
        }

        __MASSERT(debug_buf == string_buf);

        tage->used += needed;
        __MASSERT (tage->used <= PAGE_CACHE_SIZE);

console:
        if ((mask & mtfs_printk) == 0) {
                /* no console output requested */
                if (tcd != NULL)
                        mtfs_trace_put_tcd(tcd);
                return 1;
        }

        if (cdls != NULL) {
                if (mtfs_console_ratelimit &&
                    cdls->cdls_next != 0 &&     /* not first time ever */
                    !time_before(cdls->cdls_next, jiffies)) {
                        /* skipping a console message */
                        cdls->cdls_count++;
                        if (tcd != NULL)
                                mtfs_trace_put_tcd(tcd);
                        return 1;
                }

                if (time_before(cdls->cdls_next +
                	        mtfs_console_max_delay +
                	        ((time_t)10 * HZ),
                	        jiffies)) {
                        /* last timeout was a long time ago */
                        cdls->cdls_delay /= mtfs_console_backoff * 4;
                } else {
                        cdls->cdls_delay *= mtfs_console_backoff;

                        if (cdls->cdls_delay < mtfs_console_min_delay)
                                cdls->cdls_delay = mtfs_console_min_delay;
                        else if (cdls->cdls_delay > mtfs_console_max_delay)
                                cdls->cdls_delay = mtfs_console_max_delay;
                }

                /* ensure cdls_next is never zero after it's been seen */
                cdls->cdls_next = (jiffies + cdls->cdls_delay) | 1;
        }

        if (tcd != NULL) {
                mtfs_print_to_console(&header, mask, string_buf, needed, file,
                                      msgdata->msg_fn);
                mtfs_trace_put_tcd(tcd);
        } else {
                string_buf = mtfs_trace_get_console_buffer();

                needed = 0;
                if (format1 != NULL) {
                        va_copy(ap, args);
                        needed = vsnprintf(string_buf,
                                           MTFS_TRACE_CONSOLE_BUFFER_SIZE,
                                           format1, ap);
                        va_end(ap);
                }
                if (format2 != NULL) {
                        remain = MTFS_TRACE_CONSOLE_BUFFER_SIZE - needed;
                        if (remain > 0) {
                                va_start(ap, format2);
                                needed += vsnprintf(string_buf+needed, remain,
                                                    format2, ap);
                                va_end(ap);
                        }
                }
                mtfs_print_to_console(&header, mask,
                                     string_buf, needed, file, msgdata->msg_fn);

                mtfs_trace_put_console_buffer(string_buf);
        }

        if (cdls != NULL && cdls->cdls_count != 0) {
                string_buf = mtfs_trace_get_console_buffer();

                needed = snprintf(string_buf, MTFS_TRACE_CONSOLE_BUFFER_SIZE,
                                  "Skipped %d previous similar message%s\n",
                                  cdls->cdls_count,
                                  (cdls->cdls_count > 1) ? "s" : "");

                mtfs_print_to_console(&header, mask,
                                     string_buf, needed, file, msgdata->msg_fn);

                mtfs_trace_put_console_buffer(string_buf);
                cdls->cdls_count = 0;
        }

        return 0;

}

int mtfs_debug_msg(struct mtfs_debug_msg_data *msgdata,
                   const char *format, ...)
{
	va_list args;
	int     ret = 0;

        va_start(args, format);
	ret = mtfs_debug_vmsg2(msgdata, format, args, NULL);
	va_end(args);

	return ret;
}
EXPORT_SYMBOL(mtfs_debug_msg);
