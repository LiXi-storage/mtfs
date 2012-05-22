/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef _HRFS_GROUPLOCL_H_
#define _HRFS_GROUPLOCL_H_
#include "libmtfsapi.h"
int mtfs_api_open(const char *filename, int flags, mode_t mode, const mtfs_param_t *param);
int mtfs_api_group_lock(const char *filename, int fd, int group_id, const mtfs_param_t *param);
int mtfs_api_group_unlock(const char *filename, int fd, int group_id, const mtfs_param_t *param);
#endif /* _HRFS_GROUPLOCL_H_ */
