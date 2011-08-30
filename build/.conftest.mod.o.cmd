cmd_/root/hrfs/hrfs-1.5.31/build/conftest.mod.o := gcc -Wp,-MD,/root/hrfs/hrfs-1.5.31/build/.conftest.mod.o.d  -nostdinc -isystem /usr/lib/gcc/x86_64-redhat-linux/4.1.2/include -D__KERNEL__ -I/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/arch/x86/include -I/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include -I/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include -I/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include2 -include include/linux/autoconf.h  -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Wstrict-prototypes -Wundef -Werror-implicit-function-declaration -fwrapv -Os  -mtune=generic -m64 -mno-red-zone -mcmodel=kernel -pipe -fno-reorder-blocks -Wno-sign-compare -fno-asynchronous-unwind-tables -funit-at-a-time -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -fomit-frame-pointer -g  -fno-stack-protector -Wdeclaration-after-statement -Wno-pointer-sign -Werror-implicit-function-declaration  -g -I/root/hrfs/hrfs-1.5.31/libcfs/include   -D"KBUILD_STR(s)=\#s" -D"KBUILD_BASENAME=KBUILD_STR(conftest.mod)"  -D"KBUILD_MODNAME=KBUILD_STR(conftest)" -DMODULE -c -o /root/hrfs/hrfs-1.5.31/build/conftest.mod.o /root/hrfs/hrfs-1.5.31/build/conftest.mod.c

deps_/root/hrfs/hrfs-1.5.31/build/conftest.mod.o := \
  /root/hrfs/hrfs-1.5.31/build/conftest.mod.c \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/module.h \
    $(wildcard include/config/modules.h) \
    $(wildcard include/config/modversions.h) \
    $(wildcard include/config/unused/symbols.h) \
    $(wildcard include/config/module/unload.h) \
    $(wildcard include/config/kallsyms.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/sched.h \
    $(wildcard include/config/detect/softlockup.h) \
    $(wildcard include/config/split/ptlock/cpus.h) \
    $(wildcard include/config/keys.h) \
    $(wildcard include/config/bsd/process/acct.h) \
    $(wildcard include/config/taskstats.h) \
    $(wildcard include/config/audit.h) \
    $(wildcard include/config/inotify/user.h) \
    $(wildcard include/config/schedstats.h) \
    $(wildcard include/config/task/delay/acct.h) \
    $(wildcard include/config/smp.h) \
    $(wildcard include/config/utrace.h) \
    $(wildcard include/config/rt/mutexes.h) \
    $(wildcard include/config/debug/mutexes.h) \
    $(wildcard include/config/trace/irqflags.h) \
    $(wildcard include/config/lockdep.h) \
    $(wildcard include/config/numa.h) \
    $(wildcard include/config/cpusets.h) \
    $(wildcard include/config/compat.h) \
    $(wildcard include/config/ptrace.h) \
    $(wildcard include/config/hotplug/cpu.h) \
    $(wildcard include/config/preempt.h) \
    $(wildcard include/config/pm.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/auxvec.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/auxvec.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/param.h \
    $(wildcard include/config/hz.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/capability.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/types.h \
    $(wildcard include/config/uid16.h) \
    $(wildcard include/config/resources/64bit.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/posix_types.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/stddef.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/compiler.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/compiler-gcc4.h \
    $(wildcard include/config/forced/inlining.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/compiler-gcc.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/posix_types.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/types.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/spinlock.h \
    $(wildcard include/config/debug/spinlock.h) \
    $(wildcard include/config/debug/lock/alloc.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/preempt.h \
    $(wildcard include/config/debug/preempt.h) \
    $(wildcard include/config/preempt/notifiers.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/thread_info.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/bitops.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/bitops.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/alternative.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/cpufeature.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm-generic/bitops/sched.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm-generic/bitops/hweight.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm-generic/bitops/ext2-non-atomic.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm-generic/bitops/le.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/byteorder.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/byteorder/little_endian.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/byteorder/swab.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/byteorder/generic.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm-generic/bitops/minix.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/thread_info.h \
    $(wildcard include/config/debug/stack/usage.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/page.h \
    $(wildcard include/config/flatmem.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/const.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/bug.h \
    $(wildcard include/config/bug.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/stringify.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm-generic/bug.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm-generic/memory_model.h \
    $(wildcard include/config/discontigmem.h) \
    $(wildcard include/config/sparsemem.h) \
    $(wildcard include/config/out/of/line/pfn/to/page.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm-generic/page.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/pda.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/cache.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/kernel.h \
    $(wildcard include/config/preempt/voluntary.h) \
    $(wildcard include/config/debug/spinlock/sleep.h) \
    $(wildcard include/config/printk.h) \
  /usr/lib/gcc/x86_64-redhat-linux/4.1.2/include/stdarg.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/linkage.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/linkage.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/log2.h \
    $(wildcard include/config/arch/has/ilog2/u32.h) \
    $(wildcard include/config/arch/has/ilog2/u64.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/cache.h \
    $(wildcard include/config/x86/l1/cache/shift.h) \
    $(wildcard include/config/x86/vsmp.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/mmsegment.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/list.h \
    $(wildcard include/config/debug/list.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/poison.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/prefetch.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/processor.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/segment.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/sigcontext.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/threads.h \
    $(wildcard include/config/nr/cpus.h) \
    $(wildcard include/config/base/small.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/msr.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/current.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/system.h \
    $(wildcard include/config/unordered/io.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/irqflags.h \
    $(wildcard include/config/trace/irqflags/support.h) \
    $(wildcard include/config/x86.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/irqflags.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/percpu.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/personality.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/cpumask.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/bitmap.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/string.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/string.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/spinlock_types.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/spinlock_types.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/lockdep.h \
    $(wildcard include/config/lock/stat.h) \
    $(wildcard include/config/generic/hardirqs.h) \
    $(wildcard include/config/prove/locking.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/debug_locks.h \
    $(wildcard include/config/debug/locking/api/selftests.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/stacktrace.h \
    $(wildcard include/config/stacktrace.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/spinlock.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/atomic.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm-generic/atomic.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/rwlock.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/spinlock_api_smp.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/timex.h \
    $(wildcard include/config/time/interpolation.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/time.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/seqlock.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/timex.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/8253pit.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/vsyscall.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/hpet.h \
    $(wildcard include/config/hpet/emulate/rtc.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/jiffies.h \
    $(wildcard include/config/tick/divider.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/calc64.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/div64.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm-generic/div64.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/rbtree.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/errno.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/errno.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm-generic/errno.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm-generic/errno-base.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/nodemask.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/numa.h \
    $(wildcard include/config/nodes/shift.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/semaphore.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/wait.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/rwsem.h \
    $(wildcard include/config/rwsem/generic/spinlock.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/rwsem-spinlock.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/ptrace.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/mmu.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/cputime.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm-generic/cputime.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/smp.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/smp.h \
    $(wildcard include/config/x86/local/apic.h) \
    $(wildcard include/config/x86/io/apic.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/fixmap.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/apicdef.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/vsyscall32.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/mpspec.h \
    $(wildcard include/config/acpi.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/io_apic.h \
    $(wildcard include/config/pci/msi.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/apic.h \
    $(wildcard include/config/x86/good/apic.h) \
    $(wildcard include/config/xen.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/pm.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/sem.h \
    $(wildcard include/config/sysvipc.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/ipc.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/ipcbuf.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/sembuf.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/signal.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/signal.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm-generic/signal.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/siginfo.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm-generic/siginfo.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/securebits.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/fs_struct.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/completion.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/pid.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/rcupdate.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/percpu.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/slab.h \
    $(wildcard include/config/slob.h) \
    $(wildcard include/config/debug/slab.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/gfp.h \
    $(wildcard include/config/dma/is/dma32.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/mmzone.h \
    $(wildcard include/config/force/max/zoneorder.h) \
    $(wildcard include/config/memory/hotplug.h) \
    $(wildcard include/config/flat/node/mem/map.h) \
    $(wildcard include/config/have/memory/present.h) \
    $(wildcard include/config/need/node/memmap/size.h) \
    $(wildcard include/config/need/multiple/nodes.h) \
    $(wildcard include/config/have/arch/early/pfn/to/nid.h) \
    $(wildcard include/config/sparsemem/extreme.h) \
    $(wildcard include/config/nodes/span/other/nodes.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/init.h \
    $(wildcard include/config/hotplug.h) \
    $(wildcard include/config/acpi/hotplug/memory.h) \
    $(wildcard include/config/acpi/hotplug/memory/module.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/memory_hotplug.h \
    $(wildcard include/config/have/arch/nodedata/extension.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/notifier.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/mutex.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/topology.h \
    $(wildcard include/config/sched/smt.h) \
    $(wildcard include/config/sched/mc.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/topology.h \
    $(wildcard include/config/acpi/numa.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm-generic/topology.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/mmzone.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/sparsemem.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/kmalloc_sizes.h \
    $(wildcard include/config/mmu.h) \
    $(wildcard include/config/large/allocs.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/seccomp.h \
    $(wildcard include/config/seccomp.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/futex.h \
    $(wildcard include/config/futex.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/rtmutex.h \
    $(wildcard include/config/debug/rt/mutexes.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/plist.h \
    $(wildcard include/config/debug/pi/list.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/param.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/resource.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/resource.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm-generic/resource.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/timer.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/hrtimer.h \
    $(wildcard include/config/no/idle/hz.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/ktime.h \
    $(wildcard include/config/ktime/scalar.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/aio.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/workqueue.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/aio_abi.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/sysdev.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/kobject.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/sysfs.h \
    $(wildcard include/config/sysfs.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/kref.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/stat.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/stat.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/kmod.h \
    $(wildcard include/config/kmod.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/elf.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/elf-em.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/elf.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/user.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/compat.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/moduleparam.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/tracepoint.h \
    $(wildcard include/config/tracepoints.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/marker.h \
    $(wildcard include/config/markers.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/local.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/module.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/vermagic.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/utsrelease.h \

/root/hrfs/hrfs-1.5.31/build/conftest.mod.o: $(deps_/root/hrfs/hrfs-1.5.31/build/conftest.mod.o)

$(deps_/root/hrfs/hrfs-1.5.31/build/conftest.mod.o):
