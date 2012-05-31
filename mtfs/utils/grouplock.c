/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <debug.h>
#include "libmtfsapi.h"
#define LL_IOC_GROUP_LOCK               _IOW ('f', 158, long)
#define LL_IOC_GROUP_UNLOCK             _IOW ('f', 159, long)

int mtfs_api_group_lock(const char *filename, int fd, int group_id, const struct mtfs_param *param)
{
	int ret = 0;

	HDEBUG("Start locking\n");
	if ((ret = ioctl(fd, LL_IOC_GROUP_LOCK, group_id)) == -1) {
		HERROR("Failed to GROUP_LOCK %s, rc = %d\n", filename, ret);
	} else {
		HDEBUG("Locking succeed\n");
	}

	return ret;
}

int mtfs_api_group_unlock(const char *filename, int fd, int group_id, const struct mtfs_param *param)
{
	int ret = 0;

	HDEBUG("Start unlocking\n");
	if ((ret = ioctl(fd, LL_IOC_GROUP_UNLOCK, group_id)) == -1) {
		HERROR("Failed to GROUP_LOCK %s, rc = %d\n", filename, ret);
	} else {
		HDEBUG("Unlocking succeed\n");
	}

	return ret;
}
