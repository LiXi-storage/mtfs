cmd_/root/hrfs/hrfs-1.5.31/build/conftest.i := gcc -E -Wp,-MD,/root/hrfs/hrfs-1.5.31/build/.conftest.i.d  -nostdinc -isystem /usr/lib/gcc/x86_64-redhat-linux/4.1.2/include -D__KERNEL__ -I/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/arch/x86/include -I/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include -I/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include -I/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include2 -include include/linux/autoconf.h  -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Wstrict-prototypes -Wundef -Werror-implicit-function-declaration -fwrapv -Os  -mtune=generic -m64 -mno-red-zone -mcmodel=kernel -pipe -fno-reorder-blocks -Wno-sign-compare -fno-asynchronous-unwind-tables -funit-at-a-time -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -fomit-frame-pointer -g  -fno-stack-protector -Wdeclaration-after-statement -Wno-pointer-sign -Werror-implicit-function-declaration  -g -I/root/hrfs/hrfs-1.5.31/libcfs/include  -DMODULE -D"KBUILD_STR(s)=\#s" -D"KBUILD_BASENAME=KBUILD_STR(conftest)"  -D"KBUILD_MODNAME=KBUILD_STR(conftest)"   -o /root/hrfs/hrfs-1.5.31/build/conftest.i /root/hrfs/hrfs-1.5.31/build/conftest.c

deps_/root/hrfs/hrfs-1.5.31/build/conftest.i := \
  /root/hrfs/hrfs-1.5.31/build/conftest.c \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/kmod.h \
    $(wildcard include/config/kmod.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/stddef.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/compiler.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/compiler-gcc4.h \
    $(wildcard include/config/forced/inlining.h) \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/compiler-gcc.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/errno.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/errno.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm-generic/errno.h \
  /root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm-generic/errno-base.h \

/root/hrfs/hrfs-1.5.31/build/conftest.i: $(deps_/root/hrfs/hrfs-1.5.31/build/conftest.i)

$(deps_/root/hrfs/hrfs-1.5.31/build/conftest.i):
