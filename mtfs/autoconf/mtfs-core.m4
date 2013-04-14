#* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
#* vim:expandtab:shiftwidth=8:tabstop=8:
#
# LC_CONFIG_SRCDIR
#
# Wrapper for AC_CONFIG_SUBDIR
#
AC_DEFUN([LC_CONFIG_SRCDIR],
[AC_CONFIG_SRCDIR([mtfs/mtfs/file.c])
libcfs_is_module=yes
])

#
# LC_TARGET_SUPPORTED
#
# is the target os supported?
#
AC_DEFUN([LC_TARGET_SUPPORTED],
[case $target_os in
        linux*)
$1
                ;;
        *)
$2
                ;;
esac
])

#
# LC_PATH_DEFAULTS
#
# mtfs specific paths
#
AC_DEFUN([LC_PATH_DEFAULTS],
[MTFS="$PWD/mtfs"
AC_SUBST(MTFS)

# mount.mtfs
rootsbindir='/sbin'
AC_SUBST(rootsbindir)

])

#
# MTFS_TAG is temporary, until backend fs check is in place
#
AC_DEFUN([LC_CONFIG_MTFS_TAG],
[AC_MSG_CHECKING([whether to enable MTFS_TAG])
AC_ARG_ENABLE([mtfs_tag],
        AC_HELP_STRING([--disable-mtfs-tag],
                        [disable MTFS_TAG]),
        [],[enable_mtfs_tag='yes'])
AC_MSG_RESULT([$enable_mtfs_tag])
if test x$enable_mtfs_tag = xyes; then
        AC_DEFINE(MTFS_TAG, 1, [enable MTFS_TAG])
fi
])

#
# whether to enable memory debug
#
AC_DEFUN([LC_CONFIG_MEMORY_DEBUG],
[AC_MSG_CHECKING([whether to enable MTFS_TAG])
AC_ARG_ENABLE([memory_debug],
        AC_HELP_STRING([--disable-memory-debug],
                        [disable MEMORY_DEBUG]),
        [],[enable_memory_debug='yes'])
AC_MSG_RESULT([$enable_memory_debug])
if test x$enable_memory_debug = xyes; then
        AC_DEFINE(MEMORY_DEBUG, 1, [enable MEMORY_DEBUG])
fi
])

# 2.6.18-53 does not have fs_stack.h yet
AC_DEFUN([LC_FS_STACK],
[LB_CHECK_FILE([$LINUX/include/linux/fs_stack.h],[
	AC_MSG_CHECKING([if fs_stack.h can be compiled])
	LB_LINUX_TRY_COMPILE([
		#include <linux/fs_stack.h>
	],[],[
		AC_MSG_RESULT([yes])
		AC_DEFINE(HAVE_FS_STACK, 1, [Kernel has fs_stack])
	],[
		AC_MSG_RESULT([no])
	])
],
[])
])

#
# 2.6.18-53 does not have FS_RENAME_DOES_D_MOVE flag
#
AC_DEFUN([LC_FS_RENAME_DOES_D_MOVE],
[AC_MSG_CHECKING([if kernel has FS_RENAME_DOES_D_MOVE flag])
LB_LINUX_TRY_COMPILE([
        #include <linux/fs.h>
],[
        int v = FS_RENAME_DOES_D_MOVE;
],[
        AC_MSG_RESULT([yes])
        AC_DEFINE(HAVE_FS_RENAME_DOES_D_MOVE, 1, [kernel has FS_RENAME_DOES_D_MOVE flag])
],[
        AC_MSG_RESULT([no])
])
])

# 2.6.18-92 does not have strcasecmp or strncasecmp yet
AC_DEFUN([LC_STRCASECMP],
[AC_MSG_CHECKING([if kernel defines strcasecmp])
LB_LINUX_TRY_COMPILE([
        #include <linux/string.h>
],[
	int i = strcasecmp(NULL, NULL);
],[
	AC_MSG_RESULT([yes])
	AC_DEFINE(HAVE_STRCASECMP, 1, [strcasecmp found])
],[
        AC_MSG_RESULT([no])
])
])

#
# 2.6.32-220 changed permission to inode_permission
#
AC_DEFUN([LC_INODE_PERMISION],
[AC_MSG_CHECKING([have inode_permission])
LB_LINUX_TRY_COMPILE([
	#include <linux/fs.h>
],[
	inode_permission(NULL, 0);
],[
	AC_DEFINE(HAVE_INODE_PERMISION, 1,
		[have inode_permission])
	AC_MSG_RESULT([yes])
],[
	AC_MSG_RESULT([no])
])
])
#
# 2.6.27
#
AC_DEFUN([LC_INODE_PERMISION_2ARGS],
[AC_MSG_CHECKING([inode_operations->permission has two args])
LB_LINUX_TRY_COMPILE([
	#include <linux/fs.h>
],[
	struct inode *inode;
	inode->i_op->permission(NULL, 0);
],[
	AC_DEFINE(HAVE_INODE_PERMISION_2ARGS, 1,
		[inode_operations->permission has two args])
	AC_MSG_RESULT([yes])
],[
	AC_MSG_RESULT([no])
])
])

# LC_FILE_WRITEV
# 2.6.19 replaced writev with aio_write
AC_DEFUN([LC_FILE_WRITEV],
[AC_MSG_CHECKING([writev in fops])
LB_LINUX_TRY_COMPILE([
	#include <linux/fs.h>
],[
	struct file_operations *fops = NULL;
	fops->writev = NULL;
],[
	AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_FILE_WRITEV, 1,
		[use fops->writev])
],[
	AC_MSG_RESULT(no)
])
])

# LC_FILE_READV
# 2.6.19 replaced readv with aio_read
AC_DEFUN([LC_FILE_READV],
[AC_MSG_CHECKING([readv in fops])
LB_LINUX_TRY_COMPILE([
	#include <linux/fs.h>
],[
	struct file_operations *fops = NULL;
	fops->readv = NULL;
],[
	AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_FILE_READV, 1,
		[use fops->readv])
],[
	AC_MSG_RESULT(no)
])
])

# 2.6.23 change .sendfile to .splice_read
AC_DEFUN([LC_KERNEL_SPLICE_READ],
[AC_MSG_CHECKING([if kernel has .splice_read])
LB_LINUX_TRY_COMPILE([
	#include <linux/fs.h>
],[
	struct file_operations file;

	file.splice_read = NULL;
], [
	AC_MSG_RESULT([yes])
	AC_DEFINE(HAVE_KERNEL_SPLICE_READ, 1,
		[kernel has .slice_read])
],[
	AC_MSG_RESULT([no])
])
])

# 2.6.23 change .sendfile to .splice_read
# RHEL4 (-92 kernel) have both sendfile and .splice_read API
AC_DEFUN([LC_KERNEL_SENDFILE],
[AC_MSG_CHECKING([if kernel has .sendfile])
LB_LINUX_TRY_COMPILE([
	#include <linux/fs.h>
],[
	struct file_operations file;

	file.sendfile = NULL;
], [
	AC_MSG_RESULT([yes])
	AC_DEFINE(HAVE_KERNEL_SENDFILE, 1,
		[kernel has .sendfile])
],[
	AC_MSG_RESULT([no])
])
])

# 2.6.29 dentry_open has 4 arguments
AC_DEFUN([LC_DENTRY_OPEN_4ARGS],
[AC_MSG_CHECKING([dentry_open wants 4 parameters])
LB_LINUX_TRY_COMPILE([
	#include <linux/fs.h>
],[
 	dentry_open(NULL, NULL, 0, NULL);
],[
	AC_DEFINE(HAVE_DENTRY_OPEN_4ARGS, 1,
		[dentry_open wants 4 paramters])
	AC_MSG_RESULT([yes])
],[
	AC_MSG_RESULT([no])
])
])

# 2.6.23 has new page fault handling API
AC_DEFUN([LC_VM_OP_FAULT],
[AC_MSG_CHECKING([kernel has .fault in vm_operation_struct])
LB_LINUX_TRY_COMPILE([
	#include <linux/mm.h>
],[
	struct vm_operations_struct op;

	op.fault = NULL;
], [
	AC_MSG_RESULT([yes])
	AC_DEFINE(HAVE_VM_OP_FAULT, 1,
		[kernel has .fault in vm_operation_struct])
],[
	AC_MSG_RESULT([no])
])
])

#
# 2.6.18 vfs_symlink taken 4 paremater.
#
AC_DEFUN([LC_VFS_SYMLINK_4ARGS],
[AC_MSG_CHECKING([if vfs_symlink wants 4 arguments])
LB_LINUX_TRY_COMPILE([
	#include <linux/fs.h>
],[
	vfs_symlink(NULL, 0, 0, NULL);
],[
	AC_DEFINE(HAVE_VFS_SYMLINK_4ARGS, 1,
		[vfs_symlink wants 4 arguments])
	AC_MSG_RESULT([yes])
],[
	AC_MSG_RESULT([no])
])
])

#
# 
#
AC_DEFUN([LC_STRUCT_NAMEIDATA_PATH],
[AC_MSG_CHECKING([if struct nameidata has a path field])
LB_LINUX_TRY_COMPILE([
	#include <linux/fs.h>
	#include <linux/namei.h>
],[
	struct nameidata nd;

 	nd.path.dentry = NULL;
],[
	AC_MSG_RESULT([yes])
	AC_DEFINE(HAVE_PATH_IN_STRUCT_NAMEIDATA, 1, [struct nameidata has a path field])
],[
	AC_MSG_RESULT([no])
])
])

#
# 
#
AC_DEFUN([LC_STRUCT_FILE_PATH],
[AC_MSG_CHECKING([if struct file has a path field])
LB_LINUX_TRY_COMPILE([
	#include <linux/fs.h>
],[
	struct file file;

	file.f_path.dentry = NULL;
],[
	AC_MSG_RESULT([yes])
	AC_DEFINE(HAVE_PATH_IN_STRUCT_FILE, 1, [struct file has a path field])
],[
	AC_MSG_RESULT([no])
])
])

#
# 2.6.32-220 vfs_statfs taken path paremater.
#
AC_DEFUN([LC_VFS_STATFS_PATH],
[AC_MSG_CHECKING([if vfs_statfs wants path argument])
LB_LINUX_TRY_COMPILE([
        #include <linux/fs.h>
],[
	extern int vfs_statfs(struct path *path, struct kstatfs *buf)
],[
	AC_DEFINE(HAVE_VFS_STATFS_PATH, 1,
		[vfs_statfs wants path argument])
	AC_MSG_RESULT([yes])
],[
	AC_MSG_RESULT([no])
])
])

# 2.6.23 extract nfs export related data into exportfs.h
AC_DEFUN([LC_HAVE_EXPORTFS_H],
[LB_CHECK_FILE([$LINUX/include/linux/exportfs.h], [
	AC_DEFINE(HAVE_LINUX_EXPORTFS_H, 1,
		[kernel has include/exportfs.h])
],[
	AC_MSG_RESULT([no])
])
])

# 2.6.21 api change. 'register_sysctl_table' use only one argument,
# instead of more old which need two.
AC_DEFUN([LC_REGISTER_SYSCTL_2ARGS],
[AC_MSG_CHECKING([check register_sysctl_table wants 2 args])
LB_LINUX_TRY_COMPILE([
	#include <linux/sysctl.h>
],[
	return register_sysctl_table(NULL, 0);
],[
	AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_REGISTER_SYSCTL_2ARGS, 1,
		[register_sysctl_table wants 2 args])
],[
	AC_MSG_RESULT(NO)
])
])

# 2.6.23 lost dtor argument
AC_DEFUN([LC_KMEM_CACHE_CREATE_DTOR],
[AC_MSG_CHECKING([check kmem_cache_create has dtor argument])
LB_LINUX_TRY_COMPILE([
	#include <linux/slab.h>
],[
	kmem_cache_create(NULL, 0, 0, 0, NULL, NULL);
],[
	AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_KMEM_CACHE_CREATE_DTOR, 1,
		[kmem_cache_create has dtor argument])
],[
	AC_MSG_RESULT(NO)
])
])

# 2.6.24 request not use real numbers for ctl_name
AC_DEFUN([LC_SYSCTL_UNNUMBERED],
[AC_MSG_CHECKING([for CTL_UNNUMBERED])
LB_LINUX_TRY_COMPILE([
	#include <linux/sysctl.h>
],[
	#ifndef CTL_UNNUMBERED
	#error CTL_UNNUMBERED not exist in kernel
	#endif
],[
	AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_SYSCTL_UNNUMBERED, 1,
		[sysctl has CTL_UNNUMBERED])
],[
	AC_MSG_RESULT(NO)
])
])

# 2.6.19 API change
#panic_notifier_list use atomic_notifier operations
#
AC_DEFUN([LC_ATOMIC_PANIC_NOTIFIER],
[AC_MSG_CHECKING([panic_notifier_list is atomic])
LB_LINUX_TRY_COMPILE([
	#include <linux/notifier.h>
	#include <linux/kernel.h>
],[
	struct atomic_notifier_head panic_notifier_list;
],[
	AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_ATOMIC_PANIC_NOTIFIER, 1,
		[panic_notifier_list is atomic_notifier_head])
],[
	AC_MSG_RESULT(NO)
])
])

# See if sysctl proc_handler wants only 5 arguments (since 2.6.32)
AC_DEFUN([LC_5ARGS_SYSCTL_PROC_HANDLER],
[AC_MSG_CHECKING([if sysctl proc_handler wants 5 args])
LB_LINUX_TRY_COMPILE([
	#include <linux/sysctl.h>
],[
	struct ctl_table *table = NULL;
	int write = 1;
	void __user *buffer = NULL;
	size_t *lenp = NULL;
	loff_t *ppos = NULL;

	proc_handler *proc_handler;
	proc_handler(table, write, buffer, lenp, ppos);
],[
	AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_5ARGS_SYSCTL_PROC_HANDLER, 1,
		[sysctl proc_handler wants 5 args])
],[
 	AC_MSG_RESULT(no)
])
])

AC_DEFUN([LC_HAVE_OOM_H],
[LB_CHECK_FILE([$LINUX/include/linux/oom.h], [
	AC_DEFINE(HAVE_LINUX_OOM_H, 1,
		[kernel has include/oom.h])
],[
	AC_MSG_RESULT([no])
])
])

# 2.6.18 store oom parameters in task struct.
# 2.6.32 store oom parameters in signal struct
AC_DEFUN([LC_OOMADJ_IN_SIG],
[AC_MSG_CHECKING([kernel store oom parameters in task])
LB_LINUX_TRY_COMPILE([
 	#include <linux/sched.h>
],[
	struct signal_struct s;

	s.oom_adj = 0;
],[
	AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_OOMADJ_IN_SIG, 1,
		[kernel store a oom parameters in signal struct])
],[
	AC_MSG_RESULT(no)
])
])

# 2.6.32 set_cpus_allowed is no more defined if CONFIG_CPUMASK_OFFSTACK=yes
AC_DEFUN([LC_SET_CPUS_ALLOWED],
[AC_MSG_CHECKING([if kernel defines set_cpus_allowed])
LB_LINUX_TRY_COMPILE([
	#include <linux/sched.h>
],[
	struct task_struct *p = NULL;
	cpumask_t mask = { { 0 } };

	(void) set_cpus_allowed(p, mask);
],[
	AC_MSG_RESULT([yes])
 	AC_DEFINE(HAVE_SET_CPUS_ALLOWED, 1,
		[set_cpus_allowed is exported by the kernel])
],[
	AC_MSG_RESULT([no])
])
])

# 2.6.30 x86 node_to_cpumask has been removed. must use cpumask_of_node
AC_DEFUN([LC_EXPORT_CPUMASK_OF_NODE],
	[LB_CHECK_SYMBOL_EXPORT([node_to_cpumask_map],
		[arch/$LINUX_ARCH/mm/numa.c],
		[AC_DEFINE(HAVE_CPUMASK_OF_NODE, 1,
			[node_to_cpumask_map is exported by
			 the kernel])]) # x86_64
])

# The actual symbol exported varies among architectures, so we need
# to check many symbols (but only in the current architecture.)  No
# matter what symbol is exported, the kernel #defines node_to_cpumask
# to the appropriate function and that's what we use.
AC_DEFUN([LC_EXPORT_NODE_TO_CPUMASK],
[LB_CHECK_SYMBOL_EXPORT([node_to_cpumask],
	[arch/$LINUX_ARCH/mm/numa.c],
	[AC_DEFINE(HAVE_NODE_TO_CPUMASK, 1,
		[node_to_cpumask is exported by
		 the kernel])]) # x86_64
LB_CHECK_SYMBOL_EXPORT([node_to_cpu_mask],
	[arch/$LINUX_ARCH/kernel/smpboot.c],
	[AC_DEFINE(HAVE_NODE_TO_CPUMASK, 1,
		[node_to_cpumask is exported by
		 the kernel])]) # ia64
LB_CHECK_SYMBOL_EXPORT([node_2_cpu_mask],
	[arch/$LINUX_ARCH/kernel/smpboot.c],
	[AC_DEFINE(HAVE_NODE_TO_CPUMASK, 1,
		[node_to_cpumask is exported by
		 the kernel])]) # i386
])

#
# LC_FUNC_UNSHARE_FS_STRUCT
#
# unshare_fs_struct was introduced in 2.6.30 to prevent others to directly
# mess with copy_fs_struct
#
AC_DEFUN([LC_FUNC_UNSHARE_FS_STRUCT],
[AC_MSG_CHECKING([if kernel defines unshare_fs_struct()])
tmp_flags="$EXTRA_KCFLAGS"
EXTRA_KCFLAGS="-Werror"
LB_LINUX_TRY_COMPILE([
	#include <linux/sched.h>
	#include <linux/fs_struct.h>
],[
	unshare_fs_struct();
],[
	AC_MSG_RESULT([yes])
	AC_DEFINE(HAVE_UNSHARE_FS_STRUCT, 1, [unshare_fs_struct found])
],[
	AC_MSG_RESULT([no])
])
EXTRA_KCFLAGS="$tmp_flags"
])

#2.6.23 has new shrinker API
AC_DEFUN([LC_REGISTER_SHRINKER],
[AC_MSG_CHECKING([if kernel has register_shrinker])
LB_LINUX_TRY_COMPILE([
	#include <linux/mm.h>
],[
	register_shrinker(NULL);
], [
	AC_MSG_RESULT([yes])
	AC_DEFINE(HAVE_REGISTER_SHRINKER, 1,                                                                      
		[kernel has register_shrinker])
],[
	AC_MSG_RESULT([no])
])
])

#
# FC15 2.6.40-5 backported the "shrink_control" parameter to the memory
# pressure shrinker from Linux 3.0
#
AC_DEFUN([LC_SHRINK_CONTROL],
[AC_MSG_CHECKING([shrink_control is present])
LB_LINUX_TRY_COMPILE([
	#include <linux/mm.h>
],[
	struct shrink_control tmp = {0};
	tmp.nr_to_scan = sizeof(tmp);
],[
	AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_SHRINK_CONTROL, 1,
		[shrink_control is present])
],[
	AC_MSG_RESULT(no)
])
])

#
# RHEL6/2.6.32 want to have pointer to shrinker self pointer in handler function
#
AC_DEFUN([LC_SHRINKER_WANT_SHRINK_PTR],
[AC_MSG_CHECKING([shrinker want self pointer in handler])
LB_LINUX_TRY_COMPILE([
	#include <linux/mm.h>
],[
	struct shrinker *tmp = NULL;
	tmp->shrink(tmp, 0, 0);
],[
	AC_MSG_RESULT(yes)
	AC_DEFINE(HAVE_SHRINKER_WANT_SHRINK_PTR, 1,
		[shrinker want self pointer in handler])
],[
	AC_MSG_RESULT(no)
])
])

#
# LC_ADD_WAIT_QUEUE_EXCLUSIVE
#
# 2.6.34 adds __add_wait_queue_exclusive
#
AC_DEFUN([LC_ADD_WAIT_QUEUE_EXCLUSIVE],
[AC_MSG_CHECKING([if __add_wait_queue_exclusive exists])
LB_LINUX_TRY_COMPILE([
        #include <linux/wait.h>
],[
        wait_queue_head_t queue;
        wait_queue_t      wait;

        __add_wait_queue_exclusive(&queue, &wait);
],[
        AC_MSG_RESULT(yes)
        AC_DEFINE(HAVE___ADD_WAIT_QUEUE_EXCLUSIVE, 1,                                                             
                  [__add_wait_queue_exclusive exists])
],[
        AC_MSG_RESULT(no)
])
])

#
# LC_FUNC_DUMP_TRACE
#
# 2.6.23 exports dump_trace() so we can dump_stack() on any task
# 2.6.24 has stacktrace_ops.address with "reliable" parameter
#
AC_DEFUN([LC_FUNC_DUMP_TRACE],
[LB_CHECK_SYMBOL_EXPORT([dump_trace],
[kernel/ksyms.c arch/${LINUX_ARCH%_64}/kernel/traps_64.c arch/x86/kernel/dumpstack_32.c arch/x86/kernel/dumpstack_64.c],[
	tmp_flags="$EXTRA_KCFLAGS"
	EXTRA_KCFLAGS="-Werror"
	AC_MSG_CHECKING([whether we can really use dump_trace])
	LB_LINUX_TRY_COMPILE([
		struct task_struct;
		struct pt_regs;
		#include <asm/stacktrace.h>
	],[
	],[
		AC_MSG_RESULT(yes)
		AC_DEFINE(HAVE_DUMP_TRACE, 1, [dump_trace is exported])
	],[
		AC_MSG_RESULT(no)
	],[
	])
	AC_MSG_CHECKING([whether print_trace_address has reliable argument])
	LB_LINUX_TRY_COMPILE([
		struct task_struct;
		struct pt_regs;
		void print_addr(void *data, unsigned long addr, int reliable);
		#include <asm/stacktrace.h>
	],[
		struct stacktrace_ops ops;
		ops.address = print_addr;
	],[
		AC_MSG_RESULT(yes)
		AC_DEFINE(HAVE_TRACE_ADDRESS_RELIABLE, 1,
			  [print_trace_address has reliable argument])
	],[
		AC_MSG_RESULT(no)
	],[
	])
	AC_MSG_CHECKING([dump_trace want address])
	LB_LINUX_TRY_COMPILE([
		struct task_struct;
		struct pt_regs;
		#include <asm/stacktrace.h>
		#include <linux/stddef.h>
	],[
		dump_trace(NULL, NULL, NULL, 0, NULL, NULL);
	],[
		AC_MSG_RESULT(yes)
		AC_DEFINE(HAVE_DUMP_TRACE_ADDRESS, 1,
			  [dump_trace want address argument])
	],[
		AC_MSG_RESULT(no)
	],[
	])

EXTRA_KCFLAGS="$tmp_flags"
])
])

#
# LC_FUNC_SHOW_TASK
#
# we export show_task(), but not all kernels have it (yet)
#
AC_DEFUN([LC_FUNC_SHOW_TASK],
[LB_CHECK_SYMBOL_EXPORT([show_task],
[kernel/ksyms.c kernel/sched.c],[
AC_DEFINE(HAVE_SHOW_TASK, 1, [show_task is exported])
],[
	LB_CHECK_SYMBOL_EXPORT([sched_show_task],
	[kernel/ksyms.c kernel/sched.c],[
	AC_DEFINE(HAVE_SCHED_SHOW_TASK, 1, [sched_show_task is exported])
	],[])
])
])

#
# LC_STACKTRACE_OPS_HAVE_WALK_STACK
#
# 2.6.32-30.el6 adds a new 'walk_stack' field in 'struct stacktrace_ops'
#
AC_DEFUN([LC_STACKTRACE_OPS_HAVE_WALK_STACK],
[AC_MSG_CHECKING([if 'struct stacktrace_ops' has 'walk_stack' field])
LB_LINUX_TRY_COMPILE([
	#include <asm/stacktrace.h>
	unsigned long walkstack(struct thread_info *tinfo,
	                        unsigned long *stack,
	                        unsigned long bp,
	                        const struct stacktrace_ops *ops,
	                        void *data,
	                        unsigned long *end,
	                        int *graph);
],[
	struct stacktrace_ops ops;

	ops.walk_stack = walkstack;
],[
	AC_MSG_RESULT([yes])
	AC_DEFINE(STACKTRACE_OPS_HAVE_WALK_STACK, 1, ['struct stacktrace_ops' has 'walk_stack' field])
],[
	AC_MSG_RESULT([no])
])
])

#
# LC_PROG_LINUX
#
# MTFS linux kernel checks
#
AC_DEFUN([LC_PROG_LINUX],
[
	LC_STRCASECMP
	LC_FS_STACK
	LC_FS_RENAME_DOES_D_MOVE
	LC_INODE_PERMISION
	LC_INODE_PERMISION_2ARGS
	LC_FILE_WRITEV
	LC_FILE_READV
	LC_KERNEL_SPLICE_READ
	LC_KERNEL_SENDFILE
	LC_DENTRY_OPEN_4ARGS
	LC_VM_OP_FAULT
	LC_VFS_SYMLINK_4ARGS
	LC_STRUCT_NAMEIDATA_PATH
	LC_STRUCT_FILE_PATH
	LC_VFS_STATFS_PATH
	LC_HAVE_EXPORTFS_H
	LC_REGISTER_SYSCTL_2ARGS
	LC_KMEM_CACHE_CREATE_DTOR
	LC_SYSCTL_UNNUMBERED
	LC_ATOMIC_PANIC_NOTIFIER
	LC_5ARGS_SYSCTL_PROC_HANDLER
	LC_HAVE_OOM_H
	LC_OOMADJ_IN_SIG
	LC_SET_CPUS_ALLOWED
	LC_EXPORT_CPUMASK_OF_NODE
	LC_EXPORT_NODE_TO_CPUMASK
	LC_FUNC_UNSHARE_FS_STRUCT
	LC_REGISTER_SHRINKER
	LC_SHRINK_CONTROL
	LC_SHRINKER_WANT_SHRINK_PTR
	LC_ADD_WAIT_QUEUE_EXCLUSIVE
	LC_FUNC_DUMP_TRACE
	LC_FUNC_SHOW_TASK
	LC_STACKTRACE_OPS_HAVE_WALK_STACK
])

#
# LC_CONFIG_BACKEDN_LUSTRE
#
# whether to build lustre backend support
#
AC_DEFUN([LC_CONFIG_BACKEDN_LUSTRE],
[AC_MSG_CHECKING([whether to build lustre backend support])
AC_ARG_ENABLE([lustre-support],
        AC_HELP_STRING([--disable-lustre-support],
                        [disable lustre backend support]),
        [],[enable_lustre_support='yes'])
AC_MSG_RESULT([$enable_lustre_support])])

#
# LC_CONFIG_JUNCTION_SYNC_REPLICA_EXT
#
# whether to build sync replica junction for ext
#
AC_DEFUN([LC_CONFIG_JUNCTION_SYNC_REPLICA_EXT],
[AC_MSG_CHECKING([whether to build sync replica junction for ext])
AC_ARG_ENABLE([junction-sync-replica-ext],
        AC_HELP_STRING([--disable-junction-sync-replica-ext],
                        [disable sync replica junction for lowerfs ext]),
        [],[enable_junction_sync_replica_ext='yes'])
AC_MSG_RESULT([$enable_junction_sync_replica_ext])])

#
# LC_CONFIG_JUNCTION_ASYNC_REPLICA_EXT
#
# whether to build async replica junction for ext
#
AC_DEFUN([LC_CONFIG_JUNCTION_ASYNC_REPLICA_EXT],
[AC_MSG_CHECKING([whether to build sync replica junction for ext])
AC_ARG_ENABLE([junction-async-replica-ext],
        AC_HELP_STRING([--disable-junction-async-replica-ext],
                        [disable async replica junction for lowerfs ext]),
        [],[enable_junction_async_replica_ext='yes'])
AC_MSG_RESULT([$enable_junction_async_replica_ext])])

#
# LC_CONFIG_JUNCTION_ASYNC_CACHE_EXT
#
# whether to build async cache junction for ext
#
AC_DEFUN([LC_CONFIG_JUNCTION_ASYNC_CACHE_EXT],
[AC_MSG_CHECKING([whether to build async cache junction for ext])
AC_ARG_ENABLE([junction-async-cache-ext],
        AC_HELP_STRING([--disable-junction-async-cache-ext],
                        [disable async cache junction for lowerfs ext]),
        [],[enable_junction_async_cache_ext='yes'])
AC_MSG_RESULT([$enable_junction_async_cache_ext])])

#
# LC_CONFIG_JUNCTION_ASYNC_REPLICA_EXT_NFS
#
# whether to build sync replica junction for ext/nfs
#
AC_DEFUN([LC_CONFIG_JUNCTION_ASYNC_REPLICA_EXT_NFS],
[AC_MSG_CHECKING([whether to build async cache junction for ext/nfs])
AC_ARG_ENABLE([junction-async-replica-ext-nfs],
        AC_HELP_STRING([--disable-junction-async-replica-ext-nfs],
                        [disable async cache junction for lowerfs ext/nfs]),
        [],[enable_junction_async_replica_ext_nfs='yes'])
AC_MSG_RESULT([$enable_junction_async_replica_ext_nfs])])

#
# LC_CONFIG_JUNCTION_SYNC_REPLICA_TMPFS
#
# whether to build sync replica junction for tmpfs
#
AC_DEFUN([LC_CONFIG_JUNCTION_SYNC_REPLICA_TMPFS],
[AC_MSG_CHECKING([whether to build sync replica junction for tmpfs])
AC_ARG_ENABLE([junction-sync-replica-tmpfs],
        AC_HELP_STRING([--disable-junction-sync-replica-tmpfs],
                        [disable sync replica junction for lowerfs tmpfs]),
        [],[enable_junction_sync_replica_tmpfs='yes'])
AC_MSG_RESULT([$enable_junction_sync_replica_tmpfs])])

#
# LC_CONFIG_JUNCTION_SYNC_REPLICA_NFS
#
# whether to build sync replica junction for nfs
#
AC_DEFUN([LC_CONFIG_JUNCTION_SYNC_REPLICA_NFS],
[AC_MSG_CHECKING([whether to build sync replica junction for nfs])
AC_ARG_ENABLE([junction-sync-replica-nfs],
        AC_HELP_STRING([--disable-junction-sync-replica-nfs],
                        [disable sync replica junction for lowerfs nfs]),
        [],[enable_junction_sync_replica_nfs='yes'])
AC_MSG_RESULT([$enable_junction_sync_replica_nfs])])

#
# LC_CONFIG_JUNCTION_SYNC_REPLICA_NTFS3G
#
# whether to build sync replica junction for ntfs3g
#
AC_DEFUN([LC_CONFIG_JUNCTION_SYNC_REPLICA_NTFS3G],
[AC_MSG_CHECKING([whether to build sync replica junction for ntfs3g])
AC_ARG_ENABLE([junction-sync-replica-ntfs3g],
        AC_HELP_STRING([--disable-junction-sync-replica-ntfs3g],
                        [disable sync replica junction for lowerfs ntfs3g]),
        [],[enable_junction_sync_replica_ntfs3g='yes'])
AC_MSG_RESULT([$enable_junction_sync_replica_ntfs3g])])

#
# LC_CONFIG_JUNCTION_TRACE_EXT2
#
# whether to build trace junction for ext2
#
AC_DEFUN([LC_CONFIG_JUNCTION_TRACE_EXT2],
[AC_MSG_CHECKING([whether to build trace junction for ext2])
AC_ARG_ENABLE([junction-trace-ext2],
        AC_HELP_STRING([--disable-junction-trace-ext2],
                        [disable trace junction for lowerfs ext2]),
        [],[enable_junction_trace_ext2='yes'])
AC_MSG_RESULT([$enable_junction_trace_ext2])])

#
# LC_CONFIG_LOWERFS_EXT
#
# whether to build lowerfs support for ext 
#
AC_DEFUN([LC_CONFIG_LOWERFS_EXT],
[AC_MSG_CHECKING([whether to build lowerfs support for ext])
AC_ARG_ENABLE([lowerfs-ext],
        AC_HELP_STRING([--disable-lowerfs-ext],
                        [disable lowerfs support for ext]),
        [],[enable_lowerfs_ext='yes'])
AC_MSG_RESULT([$enable_lowerfs_ext])])

#
# LC_CONFIG_LOWERFS_NFS
#
# whether to build nfs backend support
#
AC_DEFUN([LC_CONFIG_LOWERFS_NFS],
[AC_MSG_CHECKING([whether to build lowerfs support for nfs])
AC_ARG_ENABLE([lowerfs-nfs],
        AC_HELP_STRING([--disable-lowerfs-nfs],
                        [disable lowerfs support for nfs]),
        [],[enable_lowerfs_nfs='yes'])
AC_MSG_RESULT([$enable_lowerfs_nfs])])

#
# LC_CONFIG_LOWERFS_TMPFS
#
# whether to build tmpfs lowerfs support
#
AC_DEFUN([LC_CONFIG_LOWERFS_TMPFS],
[AC_MSG_CHECKING([whether to build tmpfs lowerfs support])
AC_ARG_ENABLE([lowerfs-tmpfs],
        AC_HELP_STRING([--disable-lowerfs-tmpfs],
                        [disable tmpfs lowerfs support]),
        [],[enable_lowerfs_tmpfs='yes'])
AC_MSG_RESULT([$enable_lowerfs_tmpfs])])

#
# LC_CONFIG_LOWERFS_NTFS3G
#
# whether to build ntfs3g lowerfs support
#
AC_DEFUN([LC_CONFIG_LOWERFS_NTFS3G],
[AC_MSG_CHECKING([whether to build ntfs3g lowerfs support])
AC_ARG_ENABLE([lowerfs-ntfs3g],
        AC_HELP_STRING([--disable-lowerfs-ntfs3g],
                        [disable lowerfs support for ntfs3g]),
        [],[enable_lowerfs_ntfs3g='yes'])
AC_MSG_RESULT([$enable_lowerfs_ntfs3g])])

#
# LC_CONFIG_SUBJECT_SYNC_REPLICA
#
# whether to build sync_replica subject
#
AC_DEFUN([LC_CONFIG_SUBJECT_SYNC_REPLICA],
[AC_MSG_CHECKING([whether to build sync_replica subject])
AC_ARG_ENABLE([subject-sync-replica],
        AC_HELP_STRING([--disable-subject-sync-replica],
                        [disable sync_replica subject]),
        [],[enable_subject_sync_replica='yes'])
AC_MSG_RESULT([$enable_subject_sync_replica])])

#
# LC_CONFIG_SUBJECT_ASYNC_REPLICA
#
# whether to build async_replica subject
#
AC_DEFUN([LC_CONFIG_SUBJECT_ASYNC_REPLICA],
[AC_MSG_CHECKING([whether to build async_replica subject])
AC_ARG_ENABLE([subject-async-replica],
        AC_HELP_STRING([--disable-subject-async-replica],
                        [disable async_replica subject]),
        [],[enable_subject_async_replica='yes'])
AC_MSG_RESULT([$enable_subject_async_replica])])

#
# LC_CONFIG_SUBJECT_TRACE
#
# whether to build trace subject
#
AC_DEFUN([LC_CONFIG_SUBJECT_TRACE],
[AC_MSG_CHECKING([whether to build trace subject])
AC_ARG_ENABLE([subject-trace],
        AC_HELP_STRING([--disable-subject-trace],
                        [disable trace subject]),
        [],[enable_subject_trace='yes'])
AC_MSG_RESULT([$enable_subject_trace])])

#
# LC_CONFIG_MANAGE
#
# whether to build manage support
#
AC_DEFUN([LC_CONFIG_MANAGE],
[AC_MSG_CHECKING([whether to build manage support])
AC_ARG_ENABLE([manage],
        AC_HELP_STRING([--disable-manage],
                        [disable manage support]),
        [],[enable_manage='yes'])
AC_MSG_RESULT([$enable_manage])

AC_MSG_CHECKING([whether to build manage tests])
AC_ARG_ENABLE([manage-tests],
        AC_HELP_STRING([--enable-manage-tests],
                        [enable manage tests, if --disable-tests is used]),
        [],[enable_manage_tests=$enable_tests])
if test x$enable_manage != xyes ; then
        enable_manage_tests='no'
fi
AC_MSG_RESULT([$enable_manage_tests])
])

#
# LC_CONFIG_LIBMTFS
#
# whether to build libmtfs
#
AC_DEFUN([LC_CONFIG_LIBMTFS],
[AC_MSG_CHECKING([whether to build MTFS library])
AC_ARG_ENABLE([libmtfs],
        AC_HELP_STRING([--disable-libmtfs],
                        [disable building of mtfs library]),
        [],[enable_libmtfs='yes'])
AC_MSG_RESULT([$enable_libmtfs])

AC_MSG_CHECKING([whether to build libmtfs tests])
AC_ARG_ENABLE([libmtfs-tests],
        AC_HELP_STRING([--enable-libmtfs-tests],
                        [enable libmtfs tests, if --disable-tests is used]),
        [],[enable_libmtfs_tests=$enable_tests])
if test x$enable_libmtfs != xyes ; then
   enable_libmtfs_tests='no'
fi
AC_MSG_RESULT([$enable_libmtfs_tests])
])

#
#
# LC_CONFIG_LUSTRE_PATH
#
# Find paths for lustre
#
AC_DEFUN([LC_CONFIG_LUSTRE_PATH],
[AC_MSG_CHECKING([for Lustre sources])
AC_ARG_WITH([lustre],
        AC_HELP_STRING([--with-lustre=path],
                       [set path to lustre source (default=/usr/src/lustre)]),
        [LB_ARG_CANON_PATH([lustre], [LUSTRE])],
        [LUSTRE=/usr/src/lustre])
AC_MSG_RESULT([$LUSTRE])
AC_SUBST(LUSTRE)

# -------- check for lustre --------
LB_CHECK_FILE([$LUSTRE],[],
        [AC_MSG_ERROR([Lustre source $LUSTRE could not be found.])])

# -------- check for version --------
LB_CHECK_FILE([$LUSTRE/lustre/include/mtfs_version.h], [],[
        AC_MSG_WARN([Unpatched lustre source code detected.])
        AC_MSG_WARN([Lustre backend support cannot be built with an unpatched lustre source code;])
        AC_MSG_WARN([disabling lustre support])
        enable_lustre_support='no'
])

# -------- check for head directory --------
LB_CHECK_FILE([$LUSTRE/lustre/include],[],
	[AC_MSG_ERROR([Lustre head directory $LUSTRE/lustre/include could not be found.])])
	
# -------- define lustre dir --------
	LUSTRE_INCLUDE_DIR="$LUSTRE/lustre/include"
	#CPPFLAGS="$CPPFLAGS -I$LUSTRE_INCLUDE_DIR"
	EXTRA_KCFLAGS="$EXTRA_KCFLAGS -I$LUSTRE_INCLUDE_DIR"

]) # end of LC_CONFIG_LUSTRE_PATH


#
# LC_CONFIG_READLINE
#
# Build with readline
#
AC_DEFUN([LC_CONFIG_READLINE],
[
AC_MSG_CHECKING([whether to enable readline support])
AC_ARG_ENABLE(readline,
        AC_HELP_STRING([--disable-readline],
                        [disable readline support]),
        [],[enable_readline='yes'])
AC_MSG_RESULT([$enable_readline])

# -------- check for readline if enabled ----
if test x$enable_readline = xyes ; then
	LIBS_save="$LIBS"
	LIBS="-lncurses $LIBS"
	AC_CHECK_LIB([readline],[readline],[
	LIBREADLINE="-lreadline -lncurses"
	AC_DEFINE(HAVE_LIBREADLINE, 1, [readline library is available])
	],[
	LIBREADLINE=""
	])
	LIBS="$LIBS_save"
else
	LIBREADLINE=""
fi
AC_SUBST(LIBREADLINE)
]) # end of LC_CONFIG_READLINE

#
# LC_CONFIG_LUA
#
# Build with lua
#
AC_DEFUN([LC_CONFIG_LUA],
[
if test x$enable_manage = xyes ; then
	LIBS_save="$LIBS"
	LIBS="-ldl -lm $LIBS"
	AC_CHECK_LIB([lua],[luaL_newstate],[
	LIBLUA="-llua -ldl -lm"
	AC_DEFINE(HAVE_LIBLUA, 1, [lua library is available])
	],[
	AC_MSG_WARN([Lua library is not found.])
	AC_MSG_WARN([Hfsm cannot be built without Lua library.])
	AC_MSG_WARN([Disabling hfsm.])
	enable_manage="no"
	])
	LIBS="$LIBS_save"
	AC_SUBST(LIBREADLINE)
else
	LIBLUA=""
fi
AC_SUBST(LIBLUA)
]) # end of LC_CONFIG_LUA

#
# LC_CONFIGURE
#
# other configure checks
#
AC_DEFUN([LC_CONFIGURE],
[
if test $target_cpu == "i686" -o $target_cpu == "x86_64"; then
        CFLAGS="$CFLAGS -Werror"
fi

if test x$enable_lustre_support = xyes ; then
	LC_CONFIG_LUSTRE_PATH
fi

# check for -lz support
ZLIB=""
AC_CHECK_LIB([z],
	[adler32],
	[AC_CHECK_HEADERS([zlib.h],
		[ZLIB="-lz"
		 AC_DEFINE([HAVE_ADLER], 1,
		[support alder32 checksum type])],
		[AC_MSG_WARN([No zlib-devel package found,
		              unable to use adler32 checksum])])],
	[AC_MSG_WARN([No zlib package found, unable to use adler32 checksum])]
)
AC_SUBST(ZLIB)

LC_CONFIG_MEMORY_DEBUG
LC_CONFIG_MTFS_TAG
LC_CONFIG_READLINE
LC_CONFIG_LUA

CPPFLAGS="-I$PWD/mtfs/include $CPPFLAGS"
EXTRA_KCFLAGS="$EXTRA_KCFLAGS -I$PWD/mtfs/include"
])

#
# LC_CONDITIONALS
#
# AM_CONDITIONALS for mtfs
#
AC_DEFUN([LC_CONDITIONALS],
[
AM_CONDITIONAL(LIBMTFS, test x$enable_libmtfs = xyes)
AM_CONDITIONAL(LIBMTFS_TESTS, test x$enable_libmtfs_tests = xyes)
AM_CONDITIONAL(LUSTRE_SUPPORT, test x$enable_lustre_support = xyes)
AM_CONDITIONAL(JUNCTION_SYNC_REPLICA_EXT, test x$enable_junction_sync_replica_ext = xyes)
AM_CONDITIONAL(JUNCTION_ASYNC_CACHE_EXT, test x$enable_junction_async_cache_ext = xyes)
AM_CONDITIONAL(JUNCTION_ASYNC_REPLICA_EXT, test x$enable_junction_async_replica_ext = xyes)
AM_CONDITIONAL(JUNCTION_ASYNC_REPLICA_EXT_NFS, test x$enable_junction_async_replica_ext_nfs = xyes)
AM_CONDITIONAL(JUNCTION_SYNC_REPLICA_TMPFS, test x$enable_junction_sync_replica_tmpfs = xyes)
AM_CONDITIONAL(JUNCTION_SYNC_REPLICA_NFS, test x$enable_junction_sync_replica_nfs = xyes)
AM_CONDITIONAL(JUNCTION_SYNC_REPLICA_NTFS3G, test x$enable_junction_sync_replica_ntfs3g = xyes)
AM_CONDITIONAL(JUNCTION_TRACE_EXT2, test x$enable_junction_trace_ext2 = xyes)
AM_CONDITIONAL(LOWERFS_EXT, test x$enable_lowerfs_ext = xyes)
AM_CONDITIONAL(LOWERFS_NFS, test x$enable_lowerfs_nfs = xyes)
AM_CONDITIONAL(LOWERFS_TMPFS, test x$enable_lowerfs_tmpfs = xyes)
AM_CONDITIONAL(LOWERFS_NTFS3G, test x$enable_lowerfs_ntfs3g = xyes)
AM_CONDITIONAL(MANAGE, test x$enable_manage = xyes)
AM_CONDITIONAL(MANAGE_TESTS, test x$enable_manage_tests = xyes)
AM_CONDITIONAL(SUBJECT_ASYNC_REPLICA, test x$enable_subject_async_replica = xyes)
AM_CONDITIONAL(SUBJECT_SYNC_REPLICA, test x$enable_subject_sync_replica = xyes)
AM_CONDITIONAL(SUBJECT_TRACE, test x$enable_subject_trace = xyes)
])

#
# LC_CONFIG_FILES
#
# files that should be generated with AC_OUTPUT
#
AC_DEFUN([LC_CONFIG_FILES],
[AC_CONFIG_FILES([
mtfs/Makefile
mtfs/autoMakefile
mtfs/autoconf/Makefile
mtfs/debug/Makefile
mtfs/debug/autoMakefile
mtfs/doc/Makefile
mtfs/include/Makefile
mtfs/junctions/Makefile
mtfs/junctions/autoMakefile
mtfs/junctions/async_cache_ext/Makefile
mtfs/junctions/async_cache_ext/autoMakefile
mtfs/junctions/async_replica_ext/Makefile
mtfs/junctions/async_replica_ext/autoMakefile
mtfs/junctions/async_replica_ext_nfs/Makefile
mtfs/junctions/async_replica_ext_nfs/autoMakefile
mtfs/junctions/sync_replica_ext/Makefile
mtfs/junctions/sync_replica_ext/autoMakefile
mtfs/junctions/sync_replica_nfs/Makefile
mtfs/junctions/sync_replica_nfs/autoMakefile
mtfs/junctions/sync_replica_ntfs3g/Makefile
mtfs/junctions/sync_replica_ntfs3g/autoMakefile
mtfs/junctions/sync_replica_tmpfs/Makefile
mtfs/junctions/sync_replica_tmpfs/autoMakefile
mtfs/junctions/trace_ext2/Makefile
mtfs/junctions/trace_ext2/autoMakefile
mtfs/libuser/Makefile
mtfs/libuser/tests/Makefile
mtfs/lowerfs/Makefile
mtfs/lowerfs/autoMakefile
mtfs/lowerfs/ext/Makefile
mtfs/lowerfs/ext/autoMakefile
mtfs/lowerfs/nfs/Makefile
mtfs/lowerfs/nfs/autoMakefile
mtfs/lowerfs/ntfs3g/Makefile
mtfs/lowerfs/ntfs3g/autoMakefile
mtfs/lowerfs/tmpfs/Makefile
mtfs/lowerfs/tmpfs/autoMakefile
mtfs/lustre_support/Makefile
mtfs/lustre_support/autoMakefile
mtfs/manage/Makefile
mtfs/mtfs/Makefile
mtfs/mtfs/autoMakefile
mtfs/subjects/Makefile
mtfs/subjects/autoMakefile
mtfs/subjects/async_replica/Makefile
mtfs/subjects/async_replica/autoMakefile
mtfs/subjects/sync_replica/Makefile
mtfs/subjects/sync_replica/autoMakefile
mtfs/subjects/trace/Makefile
mtfs/subjects/trace/autoMakefile
mtfs/scripts/Makefile
mtfs/tests/Makefile
mtfs/tests/pjd_fstest/Makefile
mtfs/tests/src/Makefile
mtfs/utils/Makefile
])
])
