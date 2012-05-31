/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef _MTFS_GROUPLOCL_H_
#define _MTFS_GROUPLOCL_H_
#include "libmtfsapi.h"
int mtfs_api_open(const char *filename, int flags, mode_t mode, const struct mtfs_param *param);
int mtfs_api_group_lock(const char *filename, int fd, int group_id, const struct mtfs_param *param);
int mtfs_api_group_unlock(const char *filename, int fd, int group_id, const struct mtfs_param *param);
#endif /* _MTFS_GROUPLOCL_H_ */
