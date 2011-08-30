/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#include "hrfs_internal.h"
static struct ctl_table_header *hrfs_table_header = NULL;

atomic_t hrfs_kmemory_used = {0};
EXPORT_SYMBOL(hrfs_kmemory_used);

static struct ctl_table hrfs_table[] = {
	/*
	 * NB No .strategy entries have been provided since sysctl(8) prefers
	 * to go via /proc for portability.
	 */
	{
		.ctl_name = HRFS_PROC_DEBUG,
		.procname = "hrfs_debug",
		.data     = &hrfs_debug_level,
		.maxlen   = sizeof(int),
		.mode     = 0644,
		.proc_handler = &proc_dointvec,
	},
	{
		.ctl_name = HRFS_PROC_MEMUSED,
		.procname = "memused",
		.data     = (int *)&hrfs_kmemory_used.counter,
		.maxlen   = sizeof(int),
		.mode     = 0444,
		.proc_handler = &proc_dointvec,
		.strategy = &sysctl_intvec,
	},
	{0}
};
static struct ctl_table hrfs_top_table[] = {
	{
		.ctl_name = CTL_HRFS,
		.procname = "hrfs",
		.mode     = 0555,
		.data     = NULL,
		.maxlen   = 0,
		.child    = hrfs_table,
	},
	{
		.ctl_name = 0
	}
};

DECLARE_RWSEM(hrfs_proc_lock);

/* to begin from 2.6.23, Linux defines self file_operations (proc_reg_file_ops)
 * in procfs, the proc file_operation (hrfs_proc_generic_fops)
 * will be wrapped into the new defined proc_reg_file_ops, which instroduces 
 * user count in proc_dir_entrey(pde_users) to protect the proc entry from 
 * being deleted. then the protection lock (hrfs_proc_lock)
 * isn't necessary anymore for hrfs_proc_generic_fops(e.g. hrfs_proc_fops_read).
 */
#ifndef HAVE_PROCFS_USERS

#define HRFS_PROC_ENTRY()           do {  \
        down_read(&hrfs_proc_lock);      \
} while(0)
#define HRFS_PROC_EXIT()            do {  \
        up_read(&hrfs_proc_lock);        \
} while(0)

#else

#define HRFS_PROC_ENTRY()
#define HRFS_PROC_EXIT()
#endif

#ifdef HAVE_PROCFS_DELETED

#ifdef HAVE_PROCFS_USERS
#error proc_dir_entry->deleted is conflicted with proc_dir_entry->pde_users
#endif /* HAVE_PROCFS_USERS */

#define HRFS_PROC_CHECK_DELETED(dp) ((dp)->deleted)

#else /* !HAVE_PROCFS_DELETED */
#ifdef HAVE_PROCFS_USERS

#define HRFS_PROC_CHECK_DELETED(dp) ({            \
        int deleted = 0;                        \
        spin_lock(&(dp)->pde_unload_lock);      \
        if (dp->proc_fops == NULL)              \
                deleted = 1;                    \
        spin_unlock(&(dp)->pde_unload_lock);    \
        deleted;                                \
})

#else /* !HAVE_PROCFS_USERS */
#define HRFS_PROC_CHECK_DELETED(dp) (0)
#endif /* !HAVE_PROCFS_USERS */
#endif /* HAVE_PROCFS_DELETED */

static ssize_t hrfs_proc_fops_read(struct file *f, char __user *buf, size_t size,
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

	HRFS_PROC_ENTRY();
	if (!HRFS_PROC_CHECK_DELETED(dp) && dp->read_proc)
		rc = dp->read_proc(page, &start, *ppos, PAGE_SIZE,
		&eof, dp->data);
	HRFS_PROC_EXIT();
	if (rc <= 0)
		goto out;

	/* for swgfs proc read, the read count must be less than PAGE_SIZE */
	HASSERT(eof == 1);

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

static ssize_t hrfs_proc_fops_write(struct file *f, const char __user *buf,
                                  size_t size, loff_t *ppos)
{
	struct proc_dir_entry *dp = PDE(f->f_dentry->d_inode);
	int rc = -EIO;

	HRFS_PROC_ENTRY();
	if (!HRFS_PROC_CHECK_DELETED(dp) && dp->write_proc)
		rc = dp->write_proc(f, buf, size, dp->data);
	HRFS_PROC_EXIT();
	return rc;
}

static struct file_operations hrfs_proc_generic_fops = {
	.owner = THIS_MODULE,
	.read = hrfs_proc_fops_read,
	.write = hrfs_proc_fops_write,
};

struct proc_dir_entry *hrfs_proc_search(struct proc_dir_entry *head,
                                          const char *name)
{
	struct proc_dir_entry *temp;

	if (head == NULL) {
		return NULL;
	}

	down_read(&hrfs_proc_lock);
	temp = head->subdir;
	while (temp != NULL) {
		if (strcmp(temp->name, name) == 0) {
			up_read(&hrfs_proc_lock);
			return temp;
		}
		temp = temp->next;
	}
	up_read(&hrfs_proc_lock);
	return NULL;
}

struct hrfs_proc_vars {
        const char   *name;
        read_proc_t *read_fptr;
        write_proc_t *write_fptr;
        void *data;
        struct file_operations *fops;
        /**
         * /proc file mode.
         */
        mode_t proc_mode;
};

int hrfs_proc_add_vars(struct proc_dir_entry *root, struct hrfs_proc_vars *list,
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
			HRFS_ALLOC(pathcopy, pathsize);
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
			proc = hrfs_proc_search(cur_root, cur);
			HDEBUG("cur_root=%s, cur=%s, next=%s, (%s)\n",
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
			HRFS_FREE(pathcopy, pathsize);

		if (cur_root == NULL || proc == NULL) {
			HERROR("No memory to create /proc entry %s",
			       list->name);
			return -ENOMEM;
		}

		if (list->fops)
			proc->proc_fops = list->fops;
		else
			proc->proc_fops = &hrfs_proc_generic_fops;
		proc->read_proc = list->read_fptr;
		proc->write_proc = list->write_fptr;
		proc->data = (list->data ? list->data : data);
		list++;
	}
	return 0;
}

void hrfs_proc_remove(struct proc_dir_entry **rooth)
{
	struct proc_dir_entry *root = *rooth;
	struct proc_dir_entry *temp = root;
	struct proc_dir_entry *rm_entry;
	struct proc_dir_entry *parent;

	if (!root)
		return;
	*rooth = NULL;

	parent = root->parent;
	HASSERT(parent != NULL);
	down_write(&hrfs_proc_lock); /* search vs remove race */

	while (1) {
		while (temp->subdir != NULL)
			temp = temp->subdir;

		rm_entry = temp;
		temp = temp->parent;

		/*
		 * Memory corruption once caused this to fail, and
		 * without this LASSERT we would loop here forever.
		 */
		HASSERT(strlen(rm_entry->name) == rm_entry->namelen);
#ifdef HAVE_PROCFS_USERS
		/*
		 * if procfs uses user count to synchronize deletion of
		 * proc entry, there is no protection for rm_entry->data,
		 * then lprocfs_fops_read and lprocfs_fops_write maybe
		 * call proc_dir_entry->read_proc (or write_proc) with
		 * proc_dir_entry->data == NULL, then cause kernel Oops.
		 * see bug19706 for detailed information
		 */

		/*
		 * procfs won't free rm_entry->data if it isn't a LINK,
		 * and SWGFS won't use rm_entry->data if it is a LINK
		 */
		if (S_ISLNK(rm_entry->mode))
			rm_entry->data = NULL;
#else
		/*
		 * Now, the rm_entry->deleted flags is protected
		 * by _lprocfs_lock.
		 */
		rm_entry->data = NULL;
#endif
		remove_proc_entry(rm_entry->name, temp);
		if (temp == parent)
			break;
	}
	up_write(&hrfs_proc_lock);
}

struct proc_dir_entry *hrfs_proc_register(const char *name,
                                            struct proc_dir_entry *parent,
                                            struct hrfs_proc_vars *list, void *data)
{
	struct proc_dir_entry *newchild;

	newchild = hrfs_proc_search(parent, name);
	if (newchild != NULL) {
		HERROR("Attempting to register %s more than once \n",
		       name);
		return ERR_PTR(-EALREADY);
	}

	newchild = proc_mkdir(name, parent);
	if (newchild != NULL && list != NULL) {
		int rc = hrfs_proc_add_vars(newchild, list, data);
		if (rc) {
			hrfs_proc_remove(&newchild);
			return ERR_PTR(rc);
		}
	}
	return newchild;
}

/* Root for /proc/fs/hrfs */
struct proc_dir_entry *hrfs_proc_root = NULL;
struct hrfs_proc_vars hrfs_proc_base[] = {
        { "devices", hrfs_proc_read_devices, NULL, NULL },
        { 0 }
};

int hrfs_insert_proc(void)
{
#ifdef CONFIG_SYSCTL
	if (hrfs_table_header == NULL) {
		hrfs_table_header = register_sysctl_table(hrfs_top_table, 0);
	}
#endif
	hrfs_proc_root = hrfs_proc_register("fs/hrfs", NULL,
                                        hrfs_proc_base, NULL);
	if (hrfs_proc_root == NULL) {
		HERROR("error registering /proc/fs/hrfs\n");
		return(-ENOMEM);
	}
	return 0;
}

void hrfs_remove_proc(void)
{
#ifdef CONFIG_SYSCTL
	if (hrfs_table_header != NULL) {
		unregister_sysctl_table(hrfs_table_header);
	}

	hrfs_table_header = NULL;
#endif
	if (hrfs_proc_root) {
		hrfs_proc_remove(&hrfs_proc_root);
	}
}
