/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_COMPONENT_INTERNAL_H__
#define __HRFS_COMPONENT_INTERNAL_H__
#include "fsmL_internal.h"
#include <mtfs_list.h>

struct pin_map_table
{
	char *sub_component_name;
	char *sub_pin_name;
	char *pin_name;
};

struct pin {
	int is_input;
	char *name;
	struct component *parent;

	/*
	 * A pin can be connected from mutiple pins, 
	 * but can connect to only one pin.
	 */
	struct mtfs_list_head prevs;
	struct pin *next;

	/*
	 * For place in prevs field of the next pin 
	 */
	struct mtfs_list_head pin_list;
};

struct filter {
	int selector;
};

struct graph {
	struct mtfs_list_head subcomponents;
};

struct source {
	int thread_num; /* How many thread to gennerate */
	
};

#define COMPONENT_READ_MODE (0x0001)
#define COMPONENT_WRITE_MODE (0x0002)
struct component {
	char *name; /* This field of an type is NULL */
	struct component *parent; /* For graph placement */
	struct mtfs_list_head component_list; /* For placing in parent graph */
	int ref; /* refrenced times */
	int mode; /* rw mode */
	struct component *type; /* This field of an type pointed to itself */
	int pin_num;
	struct pin *pins;
	/*
	 * A graph can be packed to component
	 * If this is a graph_generated_component then my_graph is setted.
	 * Otherwise, my_filter is setted.
	 */
	int is_graph;
	union {
		struct filter this_filter;
		struct graph this_graph;
	};
};

extern const struct luaL_reg componentlib[];
struct component *component_alloc(int pin_num);
void component_free(struct component *component);
struct component *component_type_get(const char *type_name);
void component_type_put(struct component *type);
int component_rename(struct component *component, const char *name);
int component_subs_eachdo(struct component *parent, int (*func)(struct component *subcomponent));
int component_insert(struct component *component, struct component *subcomponent);
int component_print(struct component *component);
char *component_path(struct component *component);
struct component *component_search(struct component *parent, const char *name);
int component_remove(struct component *component);
int component_types_eachdo(int (*func)(struct component *subcomponent));
int component_print_type(struct component *component);
int component_declare_filter(const char *type_name, const char *inpin_name, int out_num, const char **outpin_names, int selector);
struct component *component_new(struct component *parent, const char *type_name, const char *instance_name);
struct component *component_copy(struct component *old_component, struct component *parent, const char *new_name);
struct component *component_new_notype(struct component *parent, const char *name);
int component_copy_subs(struct component *old_parent, struct component *parent);
struct component *component_search_type(const char *type_name);
void component_type_dump(struct component *type);
void component_instance_dump(struct component *instance);
int component_link(struct component *source_component, const char *source_pin_name,
                   struct component *dest_component, const char *dest_pin_name);
int component_unlink(struct component *source_component, const char *source_pin_name,
                     struct component *dest_component, const char *dest_pin_name);
int component_dump(struct component *component);
int component_pack(struct component *component,
                   int table_number, struct pin_map_table *tables);
int component_declare_graph(struct component *component, const char *type_name);
int component_types_init();
#endif /* __HRFS_COMPONENT_INTERNAL_H__ */
