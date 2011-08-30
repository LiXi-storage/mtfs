# 1 "/root/hrfs/hrfs-1.5.31/build/conftest.c"
# 1 "/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp//"
# 1 "<built-in>"
# 1 "<command line>"
# 1 "./include/linux/autoconf.h" 1
# 1 "<command line>" 2
# 1 "/root/hrfs/hrfs-1.5.31/build/conftest.c"
# 27 "/root/hrfs/hrfs-1.5.31/build/conftest.c"
# 1 "/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/kmod.h" 1
# 22 "/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/kmod.h"
# 1 "/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/stddef.h" 1



# 1 "/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/compiler.h" 1
# 42 "/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/compiler.h"
# 1 "/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/compiler-gcc4.h" 1



# 1 "/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/compiler-gcc.h" 1
# 5 "/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/compiler-gcc4.h" 2
# 43 "/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/compiler.h" 2
# 5 "/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/stddef.h" 2
# 15 "/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/stddef.h"
enum {
 false = 0,
 true = 1
};
# 23 "/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/kmod.h" 2
# 1 "/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/errno.h" 1



# 1 "/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/errno.h" 1



# 1 "/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm-generic/errno.h" 1



# 1 "/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm-generic/errno-base.h" 1
# 5 "/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm-generic/errno.h" 2
# 5 "/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/asm/errno.h" 2
# 5 "/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/errno.h" 2
# 24 "/root/hrfs/kernel-2.6.18128.7.1.el5_swgfs.1.8.1.1.smp/include/linux/kmod.h" 2







extern int request_module(const char * name, ...) __attribute__ ((format (printf, 1, 2)));






struct key;
extern int call_usermodehelper_keys(char *path, char *argv[], char *envp[],
        struct key *session_keyring, int wait);

static inline __attribute__((always_inline)) int
call_usermodehelper(char *path, char **argv, char **envp, int wait)
{
 return call_usermodehelper_keys(path, argv, envp, ((void *)0), wait);
}

extern void usermodehelper_init(void);
extern int __exec_usermodehelper(char *path, char **argv, char **envp,
     struct key *ring);

struct file;
extern int call_usermodehelper_pipe(char *path, char *argv[], char *envp[],
        struct file **filp);
# 28 "/root/hrfs/hrfs-1.5.31/build/conftest.c" 2

int
main (void)
{

        int myretval=38 ;
        return myretval;

  ;
  return 0;
}
