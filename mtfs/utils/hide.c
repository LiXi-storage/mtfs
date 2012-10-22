/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include "hide.h"
#include <stdio.h>
#include <mntent.h>
#include <string.h>
#include <dirent.h>
#include <debug.h>
#include <memory.h>
#include <parse_option.h>
#include <sys/mount.h>

int hide_dirs(char *source, int unhide)
{
	struct mount_option *mount_option = NULL;
	int ret = 0;
	mtfs_bindex_t bindex = 0;
	char option[] = "";
	int flags = 0;

	MTFS_ALLOC_PTR(mount_option);
	if (mount_option == NULL) {
		goto out;
	}

	ret = parse_dir_option(source, mount_option);
	if (ret) {
		goto out_free;
	}

	for (bindex = 0; bindex < mount_option->bnum; bindex++) {
		char *name = mount_option->branch[bindex].path;

		if (unhide) {
			ret = umount(name);
		} else {
			ret = mount(MTFS_HIDDEN_FS_DEV, name, MTFS_HIDDEN_FS_TYPE, flags, (void *)option);
		}
		if (ret) {
			MERROR("%s branch[%d] at %s failed: %s\n",
			       unhide ? "unhide" : "hide", bindex, name, strerror(errno));
			break;
		}
	}

	mount_option_fini(mount_option);
out_free:
	MTFS_FREE_PTR(mount_option);
out:
	return ret;
}

int mtfs_api_hide(const char *path, int unhide, struct mtfs_param *param)
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

	ret = hide_dirs(device, unhide);
free_device:
	MTFS_FREE(device, PATH_MAX);	
end_mnt:
	endmntent(fp);
out:
	return ret;
}
