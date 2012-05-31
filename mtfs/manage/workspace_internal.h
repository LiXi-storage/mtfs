/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_WORKSPACE_INTERNAL_H__
#define __MTFS_WORKSPACE_INTERNAL_H__
#include "fsmL_internal.h"

extern const struct luaL_reg workspacelib[];
int component_root_init();
int component_root_finit();

extern struct component component_root;
#endif /* __MTFS_WORKSPACE_INTERNAL_H__ */
