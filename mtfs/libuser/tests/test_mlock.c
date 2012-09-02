/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <cfg.h>
#include <debug.h>
#include <stdlib.h>
#include <mtfs_lock.h>

#define MAX_TYPE_LENGTH 1024
#define MAX_PARAM_LENGTH 1024

int main()
{
	struct mlock_resource resource;
	struct mlock *lock1 = NULL;
	struct mlock *lock2 = NULL;
	int ret = 0;
	struct mlock_enqueue_info einfo;

	mlock_resource_init(&resource);

	einfo.mode = MLOCK_MODE_WRITE;
	einfo.data.mlp_extent.start = 0;
	einfo.data.mlp_extent.end = 1001;
	lock1 = mlock_enqueue(&resource, &einfo);
	if (lock1 == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	einfo.mode = MLOCK_MODE_WRITE;
	einfo.data.mlp_extent.start = 1001;
	einfo.data.mlp_extent.end = 10002;
	lock2 = mlock_enqueue(&resource, &einfo);
	if (lock2 == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	mlock_cancel(lock1);

out:
	return ret;
}
