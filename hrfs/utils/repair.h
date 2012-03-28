/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_REPAIR_H__
#define __HRFS_REPAIR_H__
#include "libhrfsapi.h"
int hrfs_api_diff(char const *name0, char const *name1, hrfs_param_t *param);
int hrfs_api_repair(char *path, hrfs_param_t *param);
#endif /* __HRFS_REPAIR_H__ */
