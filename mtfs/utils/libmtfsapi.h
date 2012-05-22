/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_LIBHRFSAPI_H__
#define __HRFS_LIBHRFSAPI_H__
#include <raid.h>
#include <mtfs_common.h>

enum mtfs_bstate {
	HRFS_BSTATE_UNSETTED = -1,
	HRFS_BSTATE_VALID = 0,
	HRFS_BSTATE_INVALID = 1,
};

struct mtfs_branch_valid {
	enum mtfs_bstate data_valid;
	enum mtfs_bstate attr_valid;
	enum mtfs_bstate xattr_valid;
};

typedef struct mtfs_param {
	int verbose;
	int quiet;
	int recursive;
} mtfs_param_t;

int mtfs_api_getstate(char *path, mtfs_param_t *param);
int mtfs_api_setbranch(const char *path, mtfs_bindex_t bindex, struct mtfs_branch_valid *valid, mtfs_param_t *param);
int mtfs_api_setraid(char *path, raid_type_t raid_pattern, mtfs_param_t *param);
int mtfs_api_rmbranch(const char *path, mtfs_bindex_t bindex, mtfs_param_t *param);
#endif /* __HRFS_LIBHRFSAPI_H__ */
