/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_JUNCTION_INTERNAL_H__
#define __MTFS_JUNCTION_INTERNAL_H__
#include <mtfs_junction.h>
struct mtfs_junction *junction_get(const char *subject,
                                   const char *primary_type,
                                   const char **secondary_types);
void junction_put(struct mtfs_junction *junction);
#endif /* __MTFS_JUNCTION_INTERNAL_H__ */
