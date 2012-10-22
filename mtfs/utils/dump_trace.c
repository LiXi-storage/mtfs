/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <stdio.h>
#include <string.h>
#include <mntent.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <debug.h>
#include <memory.h>
#include <parse_option.h>
#include <mtfs_common.h>
#include <mtfs_trace.h>
#include <mtfs_io.h>
#include "dump_trace.h"

static inline char *full_path(const char *dir, const char *child)
{
	char *path = NULL;

	path = calloc(strlen(dir) + strlen(child) + 2, 1);
	if (path == NULL) {
		goto out;
	}

	strcpy(path, dir);
	path[strlen(dir)] = '/';
	path[strlen(dir) + 1] = 0;
	strcat(path, child);

out:
	return path;
}

void mtrace_dump(struct mtfs_io_trace *trace)
{
	MPRINT("len = %d\n", trace->head.mrh_len);
	MPRINT("sequence = %llu\n", trace->head.mrh_sequence);
	MPRINT("type = %d\n", trace->type);
	MPRINT("result = %d:%p:%ld\n", trace->result.ret,
	       trace->result.ptr,
	       trace->result.size);
	MPRINT("start = %lu.%06lu\n",
	       trace->start.tv_sec,
	       trace->start.tv_usec);
	MPRINT("end = %lu.%06lu\n",
	       trace->end.tv_sec,
	       trace->end.tv_usec);
}

int dump_trace(char *source)
{
	struct mount_option *mount_option = NULL;
	int ret = 0;
	char *trace_mnt_path = NULL;
	char *trace_file_path = NULL;
	int fd = 0;
	ssize_t size = 0;
	ssize_t count = 0;
	struct mtfs_io_trace trace;
	MENTRY();
	

	MTFS_ALLOC_PTR(mount_option);
	if (mount_option == NULL) {
		goto out;
	}

	ret = parse_dir_option(source, mount_option);
	if (ret) {
		MERROR("failed parse dir option %s", source);
		goto out_free_option;
	}

	trace_mnt_path = mount_option->branch[mount_option->bnum - 1].path;

	trace_file_path = full_path(trace_mnt_path, MTFS_RESERVE_ROOT"/"MTFS_RESERVE_RECORD);
	if (trace_file_path == NULL) {
		MERROR("failed to get path %s/%s", trace_mnt_path, MTFS_RESERVE_ROOT"/"MTFS_RESERVE_RECORD);
		goto out_fini_option;
	}

	fd = open(trace_file_path, O_RDONLY);
	if (fd < 0) {
		MERROR("failed to open file %s\n", trace_file_path);
		ret = fd;
		goto out_free_path;
	}

	do {
		count = sizeof(trace);
		size = read(fd, &trace, count);
		if (size == count) {
			mtrace_dump(&trace);
		}
	} while(size == count);

	if (size != 0) {
		ret = -1;
		MERROR("failed to read file %s, ret = %ld\n", trace_file_path, size);
	}
	
//out_close_file:
	close(fd);
out_free_path:
	free(trace_file_path);
out_fini_option:
	mount_option_fini(mount_option);
out_free_option:
	MTFS_FREE_PTR(mount_option);
out:
	MRETURN(ret);
}

int mtfs_api_dumptrace(const char *path, struct mtfs_param *param)
{
	FILE *fp = NULL;
	int ret = 0;
	char rpath[PATH_MAX] = {'\0'};
	char *device = NULL;
	struct mntent *mnt = NULL;
	int len = 0;
	int out_len = 0;

	fp = setmntent(MOUNTED, "r");
	if (fp == NULL) {
		MERROR("open %s failed: %s\n", MOUNTED, strerror(errno));
		ret = -errno;
		goto out;
	}

	if (realpath(path, rpath) == NULL) {
		MERROR("invalid path '%s': %s\n", path, strerror(errno));
		ret = -errno;
		goto end_mnt;
	}
	
	MTFS_ALLOC(device, PATH_MAX);
	if (device == NULL) {
		MERROR("Not enough memory\n");
		ret = -ENOMEM;
		goto end_mnt;
	}

	mnt = getmntent(fp);
	while (feof(fp) == 0 && ferror(fp) == 0) {
		if (mtfsapi_is_mtfs_mnt(mnt)) {
			len = strlen(mnt->mnt_dir);
			if (len > out_len &&
			    !strncmp(rpath, mnt->mnt_dir, len)) {
				out_len = len;
				memset(device, 0, PATH_MAX);
				strncpy(device, mnt->mnt_fsname, PATH_MAX);
			}
		}
		mnt = getmntent(fp);
	}

	if (out_len <= 0) {
		MERROR("%s isn't mounted on mtfs\n", path);
		ret = -EINVAL;
		goto free_device;
	}

	ret = dump_trace(device);
free_device:
	MTFS_FREE(device, PATH_MAX);	
end_mnt:
	endmntent(fp);
out:
	return ret;
}
