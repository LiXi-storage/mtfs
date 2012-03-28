/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_SOURCE_INTERNAL_H__
#define __HRFS_SOURCE_INTERNAL_H__
#include "fsmL_internal.h"
#include "component_internal.h"

struct source {
	/* Type fields */
	struct hrfs_list_head type_list; /* For type registering */
	char *type_name; /* Type name */
	int ref; /* Type refered times */

	/* Instance fields */
	char *name; /* This field of an type is NULL */
	struct hrfs_list_head graph_list; /* For placing in parent global graph */

	struct input_pin *out; /* The pin this source will go */

};

#endif /* __HRFS_SOURCE_INTERNAL_H__ */
