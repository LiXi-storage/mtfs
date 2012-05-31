/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_LIBMTFSAPI_H__
#define __MTFS_LIBMTFSAPI_H__
#include <raid.h>
#include <mtfs_common.h>

#define MTFS_UTILS_VERSION "2.0.0"

enum mtfs_bstate {
	MTFS_BSTATE_UNSETTED = -1,
	MTFS_BSTATE_VALID = 0,
	MTFS_BSTATE_INVALID = 1,
};

struct mtfs_branch_valid {
	enum mtfs_bstate data_valid;
	enum mtfs_bstate attr_valid;
	enum mtfs_bstate xattr_valid;
};

struct mtfs_param {
	int verbose;
	int quiet;
	int recursive;
};

int mtfs_api_getstate(char *path, struct mtfs_param *param);
int mtfs_api_setbranch(const char *path, mtfs_bindex_t bindex, struct mtfs_branch_valid *valid, struct mtfs_param *param);
int mtfs_api_setraid(char *path, raid_type_t raid_pattern, struct mtfs_param *param);
int mtfs_api_rmbranch(const char *path, mtfs_bindex_t bindex, struct mtfs_param *param);
int mtfsctl_api_debug_kernel(const char *filename, struct mtfs_param *param);
#endif /* __MTFS_LIBMTFSAPI_H__ */
