/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef _MTFS_LIBCFS_DEBUG_H_
#define _MTFS_LIBCFS_DEBUG_H_
#define DAEMON_CTL_NAME         "/proc/sys/lnet/daemon_file"
#define SUBSYS_DEBUG_CTL_NAME   "/proc/sys/lnet/subsystem_debug"
#define DEBUG_CTL_NAME          "/proc/sys/lnet/debug"
#define DUMP_KERNEL_CTL_NAME    "/proc/sys/lnet/dump_kernel"

int dbg_open_ctlhandle(const char *str);
void dbg_close_ctlhandle(int fd);
int dbg_write_cmd(int fd, char *str, int len);
int parse_buffer(int fdin, int fdout);
#endif /* _MTFS_LIBCFS_DEBUG_H_ */
