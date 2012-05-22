/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef _HRFS_HIDE_H_
#define _HRFS_HIDE_H_
#include "libmtfsapi.h"

int mtfs_api_hide(const char *mnt, int unhide, mtfs_param_t *param);
#endif /* _HRFS_HIDE_H_ */
