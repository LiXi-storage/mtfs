/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_LIBHRFSAPI_H__
#define __HRFS_LIBHRFSAPI_H__
#include <raid.h>
#include <hrfs_common.h>

typedef struct hrfs_param {
	int verbose;
	int quiet;
	int recursive;
} hrfs_param_t;

int hrfs_api_getstate(char *path, hrfs_param_t *param);
int hrfs_api_setstate(char *path, raid_type_t raid_pattern, hrfs_param_t *param);
#endif /* __HRFS_LIBHRFSAPI_H__ */
