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

int hide_dirs(char *source, int unhide)
{
	mount_option_t mount_option;
	int ret = 0;
	mtfs_bindex_t bindex = 0;
	char option[] = "";
	int flags = 0;

	ret = parse_dir_option(source, &mount_option);
	if (ret) {
		goto out;
	}

	for (bindex = 0; bindex < mount_option.bnum; bindex++) {
		char *name = mount_option.branch[bindex].path;

		if (unhide) {
			ret = umount(name);
		} else {
			ret = mount(HRFS_HIDDEN_FS_DEV, name, HRFS_HIDDEN_FS_TYPE, flags, (void *)option);
		}
		if (ret) {
			HERROR("%s branch[%d] at %s failed: %s\n",
			       unhide ? "unhide" : "hide", bindex, name, strerror(errno));
			break;
		}
	}

	mount_option_finit(&mount_option);
out:
	return ret;
}

/* Is this a mtfs fs? */
int mtfsapi_is_mtfs_mnt(struct mntent *mnt)
{
	return (strcmp(mnt->mnt_type, HRFS_FS_TYPE) == 0);
}

/* Is this a mtfs-hidden fs? */
int mtfsapi_is_mtfs_hidden_mnt(struct mntent *mnt)
{
	return (strcmp(mnt->mnt_type, HRFS_HIDDEN_FS_TYPE) == 0);
}

int mtfs_api_hide(const char *path, int unhide, mtfs_param_t *param)
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
		HERROR("open %s failed: %s\n", MOUNTED, strerror(errno));
		ret = -errno;
		goto out;
	}

	if (realpath(path, rpath) == NULL) {
		HERROR("invalid path '%s': %s\n", path, strerror(errno));
		ret = -errno;
		goto end_mnt;
	}
	
	HRFS_ALLOC(device, PATH_MAX);
	if (device == NULL) {
		HERROR("Not enough memory\n");
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
		HERROR("%s isn't mounted on mtfs\n", path);
		ret = -EINVAL;
		goto free_device;
	}

	ret = hide_dirs(device, unhide);
free_device:
	HRFS_FREE(device, PATH_MAX);	
end_mnt:
	endmntent(fp);
out:
	return ret;
}
