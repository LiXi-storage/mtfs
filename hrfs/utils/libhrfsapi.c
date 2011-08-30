/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#include <hrfs_ioctl.h>
#include <debug.h>
#include <memory.h>
#include "libhrfsapi.h"

#define SPACE2 "  "
#define SPACE4 SPACE2 SPACE2
#define SPACE8 SPACE4 SPACE4

void hrfs_dump_state(hrfs_user_state_t *state)
{
	hrfs_bindex_t bindex = 0;

	HASSERT(state);

	HPRINT("bnum: %d\n", state->bnum);
	/* Besure to make enough space for data */
	HPRINT(SPACE4 "bindex" SPACE4  SPACE4 "flag\n");
	for(bindex = 0; bindex < state->bnum; bindex++) {
		HPRINT(SPACE4 "%6u" SPACE4 "%#8x\n", bindex, state->state[bindex].flag);
	}
	return;
}

int hrfs_api_getstate(char *path, hrfs_param_t *param)
{
	int ret = 0;
	int len = strlen(path);
	int state_size = 0;
	int fd = 0;
	hrfs_user_state_t *state = NULL;

	if (len > PATH_MAX) {
		HERROR("Path name '%s' is too long\n", path);
		ret = -EINVAL;
		goto out;
	}
	
	state_size = hrfs_user_state_size(HRFS_BRANCH_MAX);
	HRFS_ALLOC(state, state_size);
	if (state == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		HERROR("Fail to open '%s': %s\n", path, strerror(errno));
		ret = errno;
		goto free_state;
	}
	
	ret = ioctl(fd, HRFS_IOCTL_GET_FLAG, (void *)state);
	if (ret) {
		HERROR("Fail to getstate '%s'\n", path);
		goto free_state;
	}
	
	HPRINT("%s\n", path);
	hrfs_dump_state(state);
	HPRINT("\n");
free_state:
	HRFS_FREE(state, state_size);
out:
	return ret;
}

int hrfs_api_setstate(char *path, raid_type_t raid_type, hrfs_param_t *param)
{
	int ret = 0;
	int len = strlen(path);
	int state_size = 0;
	int fd = 0;
	hrfs_user_state_t *state = NULL;
	hrfs_bindex_t bindex = 0;

	if (len > PATH_MAX) {
		HERROR("path name '%s' is too long\n", path);
		ret = -EINVAL;
		goto out;
	}
	
	if (!raid_type_is_valid(raid_type)) {
		HERROR("raid pattern '%d' is not valid\n", raid_type);
		ret = -EINVAL;
		goto out;
	}

	state_size = hrfs_user_state_size(HRFS_BRANCH_MAX);
	HRFS_ALLOC(state, state_size);
	if (state == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	
	fd = open(path, O_RDONLY);
	if (fd < 0) {
		HERROR("fail to open '%s': %s\n", path, strerror(errno));
		ret = errno;
		goto free_state;
	}

	ret = ioctl(fd, HRFS_IOCTL_GET_FLAG, (void *)state);
	if (ret) {
		HERROR("fail to getstate '%s', ret = %d\n", path, ret);
		goto free_state;
	}
	
	HPRINT("%s\n", path);
	HPRINT("Before set:\n");
	hrfs_dump_state(state);

	for (bindex = 0; bindex < state->bnum; bindex++) {
		state->state[bindex].flag &= ~(HRFS_FLAG_RAID_MASK);
		state->state[bindex].flag |= (raid_type & HRFS_FLAG_RAID_MASK);
		state->state[bindex].flag |= HRFS_FLAG_SETED;
	}

	ret = ioctl(fd, HRFS_IOCTL_SET_FLAG, (void *)state);
	if (ret) {
		HERROR("fail to setstate '%s', ret = %d\n", path, ret);
		goto free_state;
	}	
	
	HPRINT("After set:\n");
	hrfs_dump_state(state);	
	HPRINT("\n");
free_state:
	HRFS_FREE(state, state_size);
out:
	return ret;
}
