/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_INTERNAL_H__
#define __HRFS_INTERNAL_H__

#include <linux/version.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/string.h>
#include <linux/stat.h>
#include <linux/errno.h>
#include <linux/buffer_head.h>
#include <linux/pagemap.h>
#include <linux/namei.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/page-flags.h>
#include <linux/writeback.h>
#include <linux/page-flags.h>
#include <linux/statfs.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/poll.h>
#include <linux/list.h>
#include <linux/init.h>
#include <linux/security.h>
#include <linux/swap.h>
#include <asm/system.h>
#include <asm/segment.h>
#include <asm/mman.h>

#include <hrfs_common.h>
#include <hrfs_inode.h>
#include <hrfs_dentry.h>
#include <hrfs_super.h>
#include <memory.h>
#include <libcfs/libcfs.h>
#include <libcfs/kp30.h>
#include <debug.h>
#include <raid.h>

extern struct rw_semaphore global_rwsem;
static inline void global_read_lock(void)
{
	down_read(&global_rwsem);
}

static inline void global_read_unlock(void)
{
	up_read(&global_rwsem);
}

static inline void global_write_lock(void)
{
	down_write(&global_rwsem);
}

static inline void global_write_unlock(void)
{
	up_write(&global_rwsem);
}

static inline void global_downgrade_lock(void)
{
	downgrade_write(&global_rwsem);
}

static inline int global_read_trylock(void)
{
	return down_read_trylock(&global_rwsem);
}

static inline int global_write_trylock(void)
{
	return down_write_trylock(&global_rwsem);
}

#include "selfheal_internal.h"
#include "flag_internal.h"
#include "super_internal.h"
#include "inode_internal.h"
#include "dentry_internal.h"
#include "file_internal.h"
#include "mmap_internal.h"
#include "support_internal.h"
#include "proc_internal.h"
#include "main_internal.h"
#include "hide_internal.h"
#include "device_internal.h"
#include "ioctl_internal.h"
#include <hrfs_stack.h>
#endif	/* __HRFS_INTERNAL_H__ */
