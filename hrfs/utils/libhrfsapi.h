/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_LIBHRFSAPI_H__
#define __HRFS_LIBHRFSAPI_H__
#include <raid.h>
#include <hrfs_common.h>

enum hrfs_bstate {
	HRFS_BSTATE_UNSETTED = -1,
	HRFS_BSTATE_VALID = 0,
	HRFS_BSTATE_INVALID = 1,
};

struct hrfs_branch_valid {
	enum hrfs_bstate data_valid;
	enum hrfs_bstate attr_valid;
	enum hrfs_bstate xattr_valid;
};

typedef struct hrfs_param {
	int verbose;
	int quiet;
	int recursive;
} hrfs_param_t;

int hrfs_api_getstate(char *path, hrfs_param_t *param);
int hrfs_api_setbranch(const char *path, hrfs_bindex_t bindex, struct hrfs_branch_valid *valid, hrfs_param_t *param);
int hrfs_api_setraid(char *path, raid_type_t raid_pattern, hrfs_param_t *param);
int hrfs_api_rmbranch(const char *path, hrfs_bindex_t bindex, hrfs_param_t *param);
#endif /* __HRFS_LIBHRFSAPI_H__ */
