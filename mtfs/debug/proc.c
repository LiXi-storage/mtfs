/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <debug.h>
#include <asm/atomic.h>
#include <linux/module.h>
#include <memory.h>
#include "tracefile.h"
#include "proc_internal.h"

atomic64_t mtfs_kmemory_used = {0};
EXPORT_SYMBOL(mtfs_kmemory_used);

atomic64_t mtfs_kmemory_used_max = {0};
EXPORT_SYMBOL(mtfs_kmemory_used_max);

int
mtfs_proc_call_handler(void *data, int write,
                       loff_t *ppos, void *buffer, size_t *lenp,
                       int (*handler)(void *data, int write,
                       loff_t pos, void *buffer, int len))
{
	int rc = handler(data, write, *ppos, buffer, *lenp);

	if (rc < 0) {
		return rc;
	}

	if (write) {
		*ppos += *lenp;
	} else {
		*lenp = rc;
		*ppos += rc;
	}

	return 0;
}
EXPORT_SYMBOL(mtfs_proc_call_handler);

static int __mtfs_proc_dump_kernel(void *data, int write,
                            loff_t pos, void *buffer, int nob)
{
	int ret = 0;

	if (!write) {
		goto out;
	}

	ret = mtfs_trace_dump_debug_buffer_usrstr(buffer, nob);
out:
	return ret;
}
MTFS_DECLARE_PROC_HANDLER(mtfs_proc_dump_kernel)

static int __mtfs_proc_dobitmasks(void *data, int write,
                                  loff_t pos, void *buffer, int nob)
{
	const int     tmpstrlen = 512;
	char         *tmpstr;
	int           rc;
	unsigned int *mask = data;
	int           is_subsys = (mask == &mtfs_subsystem_debug) ? 1 : 0;
	int           is_printk = (mask == &mtfs_printk) ? 1 : 0;

	rc = mtfs_trace_allocate_string_buffer(&tmpstr, tmpstrlen);
	if (rc < 0)
		return rc;
	if (!write) {
		mtfs_debug_mask2str(tmpstr, tmpstrlen, *mask, is_subsys);
		rc = strlen(tmpstr);
		if (pos >= rc) {
			rc = 0;
		} else {
			rc = mtfs_trace_copyout_string(buffer, nob,
			                               tmpstr + pos, "\n");
 		}
	} else {
		rc = mtfs_trace_copyin_string(tmpstr, tmpstrlen, buffer, nob);
		if (rc < 0)
			return rc;

		rc = mtfs_debug_str2mask(mask, tmpstr, is_subsys);
		/* Always print MBUG/MASSERT to console, so keep this mask */
		if (is_printk)
			*mask |= D_EMERG;
	}

	mtfs_trace_free_string_buffer(tmpstr, tmpstrlen);
	return rc;
}
MTFS_DECLARE_PROC_HANDLER(mtfs_proc_dobitmasks)
EXPORT_SYMBOL(mtfs_proc_dobitmasks);

static struct ctl_table mtfs_table[] = {
	/*
	 * NB No .strategy entries have been provided since sysctl(8) prefers
	 * to go via /proc for portability.
	 */
	{
		/* Memory that mtfs alloced */
		.ctl_name = MTFS_PROC_MEMUSED,
		.procname = "memused",
		.data     = (__u64 *)&mtfs_kmemory_used.counter,
		.maxlen   = sizeof(int),
		.mode     = 0444,
		.proc_handler = &proc_dointvec,
		.strategy = &sysctl_intvec,
	},
	{
		/* Historical record of memory that mtfs alloced */
		.ctl_name = MTFS_PROC_MEMUSED_MAX,
		.procname = "memused_max",
		.data     = (__u64 *)&mtfs_kmemory_used_max.counter,
		.maxlen   = sizeof(int),
		.mode     = 0444,
		.proc_handler = &proc_dointvec,
		.strategy = &sysctl_intvec,
	},
	{
		/* Dump kernel trace records */
		.ctl_name = MTFS_PROC_DUMP_KERNEL,
		.procname = "dump_kernel",
		.maxlen   = 256,
		.mode     = 0200,
		.proc_handler = &mtfs_proc_dump_kernel,
	},
        {
		/* Mask kernel trace */
		.ctl_name = MTFS_PROC_DEBUG_MASK,
		.procname = "debug",
		.data     = &mtfs_debug,
		.maxlen   = sizeof(int),
		.mode     = 0644,
		.proc_handler = &mtfs_proc_dobitmasks,
	},
	{0}
};

struct ctl_table mtfs_top_table[] = {
	{
		.ctl_name = CTL_MTFS,
		.procname = "mtfs",
		.mode     = 0555,
		.data     = NULL,
		.maxlen   = 0,
		.child    = mtfs_table,
	},
	{
		.ctl_name = 0
	}
};
EXPORT_SYMBOL(mtfs_top_table);

DECLARE_RWSEM(mtfs_proc_lock);

/* to begin from 2.6.23, Linux defines self file_operations (proc_reg_file_ops)
 * in promtfs, the proc file_operation (mtfs_proc_generic_fops)
 * will be wrapped into the new defined proc_reg_file_ops, which instroduces 
 * user count in proc_dir_entrey(pde_users) to protect the proc entry from 
 * being deleted. then the protection lock (mtfs_proc_lock)
 * isn't necessary anymore for mtfs_proc_generic_fops(e.g. mtfs_proc_fops_read).
 */
#ifndef HAVE_PROMTFS_USERS

#define MTFS_PROC_ENTRY()           do {  \
        down_read(&mtfs_proc_lock);      \
} while(0)
#define MTFS_PROC_EXIT()            do {  \
        up_read(&mtfs_proc_lock);        \
} while(0)

#else

#define MTFS_PROC_ENTRY()
#define MTFS_PROC_EXIT()
#endif

#ifdef HAVE_PROMTFS_DELETED

#ifdef HAVE_PROMTFS_USERS
#error proc_dir_entry->deleted is conflicted with proc_dir_entry->pde_users
#endif /* HAVE_PROMTFS_USERS */

#define MTFS_PROC_CHECK_DELETED(dp) ((dp)->deleted)

#else /* !HAVE_PROMTFS_DELETED */
#ifdef HAVE_PROMTFS_USERS

#define MTFS_PROC_CHECK_DELETED(dp) ({            \
        int deleted = 0;                        \
        spin_lock(&(dp)->pde_unload_lock);      \
        if (dp->proc_fops == NULL)              \
                deleted = 1;                    \
        spin_unlock(&(dp)->pde_unload_lock);    \
        deleted;                                \
})

#else /* !HAVE_PROMTFS_USERS */
#define MTFS_PROC_CHECK_DELETED(dp) (0)
#endif /* !HAVE_PROMTFS_USERS */
#endif /* HAVE_PROMTFS_DELETED */

static ssize_t mtfs_proc_fops_read(struct file *f, char __user *buf, size_t size,
                                 loff_t *ppos)
{
	struct proc_dir_entry *dp = PDE(f->f_dentry->d_inode);
	char *page, *start = NULL;
	int rc = 0, eof = 1, count;

	if (*ppos >= PAGE_SIZE)
		return 0;

	page = (char *)__get_free_page(GFP_KERNEL);
	if (page == NULL)
		return -ENOMEM;

	MTFS_PROC_ENTRY();
	if (!MTFS_PROC_CHECK_DELETED(dp) && dp->read_proc)
		rc = dp->read_proc(page, &start, *ppos, PAGE_SIZE,
		&eof, dp->data);
	MTFS_PROC_EXIT();
	if (rc <= 0)
		goto out;

	/* For proc read, the read count must be less than PAGE_SIZE */
	MASSERT(eof == 1);

	if (start == NULL) {
		rc -= *ppos;
			if (rc < 0)
				rc = 0;
			if (rc == 0)
				goto out;
			start = page + *ppos;
	} else if (start < page) {
		start = page;
	}

	count = (rc < size) ? rc : size;
	if (copy_to_user(buf, start, count)) {
		rc = -EFAULT;
		goto out;
	}
	*ppos += count;

out:
	free_page((unsigned long)page);
	return rc;
}

static ssize_t mtfs_proc_fops_write(struct file *f, const char __user *buf,
                                  size_t size, loff_t *ppos)
{
	struct proc_dir_entry *dp = PDE(f->f_dentry->d_inode);
	int rc = -EIO;

	MTFS_PROC_ENTRY();
	if (!MTFS_PROC_CHECK_DELETED(dp) && dp->write_proc)
		rc = dp->write_proc(f, buf, size, dp->data);
	MTFS_PROC_EXIT();
	return rc;
}

static struct file_operations mtfs_proc_generic_fops = {
	.owner = THIS_MODULE,
	.read = mtfs_proc_fops_read,
	.write = mtfs_proc_fops_write,
};

struct proc_dir_entry *mtfs_proc_search(struct proc_dir_entry *head,
                                          const char *name)
{
	struct proc_dir_entry *temp;

	if (head == NULL) {
		return NULL;
	}

	down_read(&mtfs_proc_lock);
	temp = head->subdir;
	while (temp != NULL) {
		if (strcmp(temp->name, name) == 0) {
			up_read(&mtfs_proc_lock);
			return temp;
		}
		temp = temp->next;
	}
	up_read(&mtfs_proc_lock);
	return NULL;
}

int mtfs_proc_add_vars(struct proc_dir_entry *root, struct mtfs_proc_vars *list,
                       void *data)
{
	if (root == NULL || list == NULL)
		return -EINVAL;

	while (list->name != NULL) {
		struct proc_dir_entry *cur_root, *proc;
		char *pathcopy, *cur, *next, pathbuf[64];
		int pathsize = strlen(list->name) + 1;

		proc = NULL;
		cur_root = root;

		/* need copy of path for strsep */
		if (strlen(list->name) > sizeof(pathbuf) - 1) {
			MTFS_ALLOC(pathcopy, pathsize);
			if (pathcopy == NULL)
				return -ENOMEM;
		} else {
				pathcopy = pathbuf;
		}

		next = pathcopy;
		strcpy(pathcopy, list->name);

		while (cur_root != NULL && (cur = strsep(&next, "/"))) {
			if (*cur =='\0') /* skip double/trailing "/" */
				continue;
			proc = mtfs_proc_search(cur_root, cur);
			MDEBUG("cur_root=%s, cur=%s, next=%s, (%s)\n",
			       cur_root->name, cur, next,
			       (proc ? "exists" : "new"));
			if (next != NULL) {
				cur_root = (proc ? proc :
				           proc_mkdir(cur, cur_root));
			} else if (proc == NULL) {
				mode_t mode = 0;
				if (list->proc_mode != 0000) {
					mode = list->proc_mode;
				} else {
					if (list->read_fptr)
						mode = 0444;
						if (list->write_fptr)
							mode |= 0200;
				}
				proc = create_proc_entry(cur, mode, cur_root);
			}
		}

		if (pathcopy != pathbuf)
			MTFS_FREE(pathcopy, pathsize);

		if (cur_root == NULL || proc == NULL) {
			MERROR("No memory to create /proc entry %s",
			       list->name);
			return -ENOMEM;
		}

		if (list->fops)
			proc->proc_fops = list->fops;
		else
			proc->proc_fops = &mtfs_proc_generic_fops;
		proc->read_proc = list->read_fptr;
		proc->write_proc = list->write_fptr;
		proc->data = (list->data ? list->data : data);
		list++;
	}
	return 0;
}

void mtfs_proc_remove(struct proc_dir_entry **rooth)
{
	struct proc_dir_entry *root = *rooth;
	struct proc_dir_entry *temp = root;
	struct proc_dir_entry *rm_entry;
	struct proc_dir_entry *parent;

	if (!root)
		return;
	*rooth = NULL;

	parent = root->parent;
	MASSERT(parent != NULL);
	down_write(&mtfs_proc_lock); /* search vs remove race */

	while (1) {
		while (temp->subdir != NULL)
			temp = temp->subdir;

		rm_entry = temp;
		temp = temp->parent;

		/*
		 * Memory corruption once caused this to fail, and
		 * without this LASSERT we would loop here forever.
		 */
		MASSERT(strlen(rm_entry->name) == rm_entry->namelen);
#ifdef HAVE_PROMTFS_USERS
		/*
		 * if promtfs uses user count to synchronize deletion of
		 * proc entry, there is no protection for rm_entry->data,
		 * then lpromtfs_fops_read and lpromtfs_fops_write maybe
		 * call proc_dir_entry->read_proc (or write_proc) with
		 * proc_dir_entry->data == NULL, then cause kernel Oops.
		 * see bug19706 for detailed information
		 */

		/*
		 * promtfs won't free rm_entry->data if it isn't a LINK,
		 * and Lustre won't use rm_entry->data if it is a LINK
		 */
		if (S_ISLNK(rm_entry->mode))
			rm_entry->data = NULL;
#else
		/*
		 * Now, the rm_entry->deleted flags is protected
		 * by _lpromtfs_lock.
		 */
		rm_entry->data = NULL;
#endif
		remove_proc_entry(rm_entry->name, temp);
		if (temp == parent)
			break;
	}
	up_write(&mtfs_proc_lock);
}
EXPORT_SYMBOL(mtfs_proc_remove);

struct proc_dir_entry *mtfs_proc_register(const char *name,
                                            struct proc_dir_entry *parent,
                                            struct mtfs_proc_vars *list, void *data)
{
	struct proc_dir_entry *newchild;

	newchild = mtfs_proc_search(parent, name);
	if (newchild != NULL) {
		MERROR("Attempting to register %s more than once \n",
		       name);
		return ERR_PTR(-EALREADY);
	}

	newchild = proc_mkdir(name, parent);
	if (newchild != NULL && list != NULL) {
		int rc = mtfs_proc_add_vars(newchild, list, data);
		if (rc) {
			mtfs_proc_remove(&newchild);
			return ERR_PTR(rc);
		}
	}
	return newchild;
}
EXPORT_SYMBOL(mtfs_proc_register);
