/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <mtfs_ioctl.h>
#include <debug.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include "libmtfsapi.h"

#define SPACE2 "  "
#define SPACE4 SPACE2 SPACE2
#define SPACE8 SPACE4 SPACE4

void mtfs_dump_state(struct mtfs_user_flag *state)
{
	mtfs_bindex_t bindex = 0;

	HASSERT(state);

	HPRINT("bnum: %d\n", state->bnum);
	/* Besure to make enough space for data */
	HPRINT(SPACE4 "bindex" SPACE4  SPACE4 "flag\n");
	for(bindex = 0; bindex < state->bnum; bindex++) {
		HPRINT(SPACE4 "%6u" SPACE4 "%#8x\n", bindex, state->state[bindex].flag);
	}
	return;
}

int mtfs_api_getstate(char *path, struct mtfs_param *param)
{
	int ret = 0;
	int len = strlen(path);
	int state_size = 0;
	int fd = 0;
	struct mtfs_user_flag *state = NULL;

	if (len > PATH_MAX) {
		HERROR("Path name '%s' is too long\n", path);
		ret = -EINVAL;
		goto out;
	}
	
	state_size = mtfs_user_flag_size(MTFS_BRANCH_MAX);
	MTFS_ALLOC(state, state_size);
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
	
	ret = ioctl(fd, MTFS_IOCTL_GET_FLAG, (void *)state);
	if (ret) {
		HERROR("Fail to getstate '%s'\n", path);
		goto free_state;
	}
	
	HPRINT("%s\n", path);
	mtfs_dump_state(state);
	HPRINT("\n");
free_state:
	MTFS_FREE(state, state_size);
out:
	return ret;
}

int mtfs_api_setraid(char *path, raid_type_t raid_type, struct mtfs_param *param)
{
	int ret = 0;
	int len = strlen(path);
	int state_size = 0;
	int fd = 0;
	struct mtfs_user_flag *state = NULL;
	mtfs_bindex_t bindex = 0;

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

	state_size = mtfs_user_flag_size(MTFS_BRANCH_MAX);
	MTFS_ALLOC(state, state_size);
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

	ret = ioctl(fd, MTFS_IOCTL_GET_FLAG, (void *)state);
	if (ret) {
		HERROR("fail to getstate '%s', ret = %d\n", path, ret);
		goto free_state;
	}
	
	HPRINT("%s\n", path);
	HPRINT("Before set:\n");
	mtfs_dump_state(state);

	for (bindex = 0; bindex < state->bnum; bindex++) {
		state->state[bindex].flag &= ~(MTFS_FLAG_RAID_MASK);
		state->state[bindex].flag |= (raid_type & MTFS_FLAG_RAID_MASK);
		state->state[bindex].flag |= MTFS_FLAG_SETED;
	}

	ret = ioctl(fd, MTFS_IOCTL_SET_FLAG, (void *)state);
	if (ret) {
		HERROR("fail to setstate '%s', ret = %d\n", path, ret);
		goto free_state;
	}	
	
	HPRINT("After set:\n");
	mtfs_dump_state(state);	
	HPRINT("\n");
free_state:
	MTFS_FREE(state, state_size);
out:
	return ret;
}

int mtfs_api_setbranch(const char *path, mtfs_bindex_t bindex, struct mtfs_branch_valid *valid, struct mtfs_param *param)
{
	int ret = 0;
	int len = strlen(path);
	int state_size = 0;
	int fd = 0;
	struct mtfs_user_flag *state = NULL;

	if (len > PATH_MAX) {
		HERROR("path name '%s' is too long\n", path);
		ret = -EINVAL;
		goto out;
	}

	state_size = mtfs_user_flag_size(MTFS_BRANCH_MAX);
	MTFS_ALLOC(state, state_size);
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

	ret = ioctl(fd, MTFS_IOCTL_GET_FLAG, (void *)state);
	if (ret) {
		HERROR("fail to setbranch %d of '%s', ret = %d\n",
		       bindex, path, ret);
		goto free_state;
	}

	HPRINT("%s\n", path);
	HPRINT("Before set:\n");
	mtfs_dump_state(state);

	if (bindex >= state->bnum) {
		HERROR("bindex %d is illegal, bnum of %s is %d\n",
		       bindex, path, state->bnum);
		ret = -EINVAL;
		goto free_state;
	}

	if (valid->data_valid != MTFS_BSTATE_UNSETTED) {
		if (valid->data_valid == MTFS_BSTATE_VALID) {
			state->state[bindex].flag &= ~(MTFS_FLAG_DATABAD);
		} else if (valid->data_valid == MTFS_BSTATE_INVALID) {
			state->state[bindex].flag |= MTFS_FLAG_DATABAD;
		} else {
			HERROR("Invalid data_valid\n");
			ret = -EINVAL;
			goto free_state;
		}
	}

	/* TODO: attr valid bit */
	if (valid->attr_valid != MTFS_BSTATE_UNSETTED) {
		if (valid->attr_valid == MTFS_BSTATE_VALID) {
			state->state[bindex].flag &= ~(MTFS_FLAG_DATABAD);
		} else if (valid->attr_valid == MTFS_BSTATE_INVALID) {
			state->state[bindex].flag |= MTFS_FLAG_DATABAD;
		} else {
			HERROR("Invalid data_valid\n");
			ret = -EINVAL;
			goto free_state;
		}
	}

	/* TODO: xattr valid bit */
	if (valid->xattr_valid != MTFS_BSTATE_UNSETTED) {
		if (valid->xattr_valid == MTFS_BSTATE_VALID) {
			state->state[bindex].flag &= ~(MTFS_FLAG_DATABAD);
		} else if (valid->xattr_valid == MTFS_BSTATE_INVALID) {
			state->state[bindex].flag |= MTFS_FLAG_DATABAD;
		} else {
			HERROR("Invalid data_valid\n");
			ret = -EINVAL;
			goto free_state;
		}
	}

        state->state[bindex].flag |= MTFS_FLAG_SETED;
	ret = ioctl(fd, MTFS_IOCTL_SET_FLAG, (void *)state);
	if (ret) {
		HERROR("fail to setstate '%s', ret = %d\n", path, ret);
		goto free_state;
	}	
	
	HPRINT("After set:\n");
	mtfs_dump_state(state);	
	HPRINT("\n");
free_state:
	MTFS_FREE(state, state_size);
out:
	return ret;
}

#include <sys/types.h>
#include <dirent.h>
/* set errno upon failure */
static DIR *opendir_parent(char *path)
{
	DIR *parent = NULL;
	char *entry_name = NULL;
	char c;

	entry_name = strrchr(path, '/');
	if (entry_name == NULL) {
		return opendir(".");
	}

	c = entry_name[1];
	entry_name[1] = '\0';
	parent = opendir(path);
	entry_name[1] = c;
	return parent;
}

static char *get_entry_name(char *path)
{
	char *entry_name = strrchr(path, '/');
	entry_name = (entry_name == NULL ? path : entry_name + 1);

	return entry_name;
}

int mtfs_api_rmbranch(const char *path, mtfs_bindex_t bindex, struct mtfs_param *param)
{
	int ret = 0;
	int len = strlen(path);
	char *buff = NULL;
	DIR *dir = NULL;
	char *entry_name = NULL;
	struct mtfs_remove_branch_info *remove_info = NULL;

	if (len > PATH_MAX) {
		HERROR("path name '%s' is too long\n", path);
		ret = -EINVAL;
		goto out;
	}

	MTFS_ALLOC_PTR(remove_info);
	if (remove_info == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	MTFS_ALLOC(buff, PATH_MAX + 1);
	if (buff == NULL) {
		ret = -ENOMEM;
		goto out_free_info;
	}

	strncpy(buff, path, PATH_MAX + 1);

	dir = opendir_parent(buff);
	if (dir == NULL) {
		ret = -errno;
		goto out_free_dir;
	}

	entry_name = get_entry_name(buff);
	strncpy(remove_info->name, entry_name, strlen(entry_name));
	remove_info->bindex = bindex;

	ret = ioctl(dirfd(dir), MTFS_IOCTL_REMOVE_BRANCH,
	            (void *)remove_info);

	closedir(dir);
out_free_dir:
	MTFS_FREE(buff, PATH_MAX + 1);
out_free_info:
	MTFS_FREE_PTR(remove_info);
out:
	return ret;
}
