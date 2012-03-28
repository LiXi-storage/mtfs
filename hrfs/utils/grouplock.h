/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef _HRFS_GROUPLOCL_H_
#define _HRFS_GROUPLOCL_H_
#include "libhrfsapi.h"
int hrfs_api_open(const char *filename, int flags, mode_t mode, const hrfs_param_t *param);
int hrfs_api_group_lock(const char *filename, int fd, int group_id, const hrfs_param_t *param);
int hrfs_api_group_unlock(const char *filename, int fd, int group_id, const hrfs_param_t *param);
#endif /* _HRFS_GROUPLOCL_H_ */
