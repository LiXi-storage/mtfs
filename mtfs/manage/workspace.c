/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */
#include "fsmL_internal.h"
#include "workspace_internal.h"
#include "component_internal.h"
#include <debug.h>
#include <memory.h>
#include <log.h>
#include <graphviz_internal.h>

struct component component_root;
static struct component *component_workspace = NULL;

int component_root_init()
{
	int ret = 0;
	HENTRY();

	MTFS_STRDUP(component_root.name, "/");
	if (component_root.name == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	component_root.is_graph = 1;
	component_root.parent = &component_root;

	MTFS_INIT_LIST_HEAD(&component_root.this_graph.subcomponents);
	component_workspace = &component_root;
out:
	HRETURN(ret);
}

int component_root_finit()
{
	HENTRY();
	MTFS_FREE_STR(component_root.name);
	HRETURN(0);
}

static int workspace_list_wrap(lua_State *L)
{
	int top = 0;
	int i = 0;
	HENTRY();

	top = lua_gettop(L);
	if (top == 0) {
		printf(".. (graph)\n");
		component_subs_eachdo(component_workspace, component_print);
		goto out;
	}

	for(i = 1; i <= top; i++) {
		if (!lua_isstring(L, i)) {
			HERROR("argument error: argument[%d] is expected to be a string, got %s\n", i, luaL_typename(L, i));
		} else {
			struct component *component = NULL;
			const char *name = NULL;

			name = lua_tostring(L, i);
			HASSERT(name);
			component = component_search(component_workspace, name);
			if (component) {
				component_dump(component);
			} else {
				HERROR("%s: No such component\n", name);
			}
		}
	}
out:
	HRETURN(0);
}

static int workspace_pwd_wrap(lua_State *L)
{
	char *path = NULL;
	int top = 0;
	HENTRY();

	top = lua_gettop(L);
	if (top != 0) {
		HERROR("argument error: too many arguments\n");
		goto out;
	}

	path = component_path(component_workspace);
	if (path == NULL) {
		HERROR("failed to get path\n");
		goto out;
	}
	
	printf("%s\n", path);
	free(path);
out:
	HRETURN(0);
}

static int workspace_cd(const char *name)
{
	struct component *found = NULL;
	int ret = 0;
	HENTRY();

	if (strcmp(name, "..") == 0) {
		component_workspace = component_workspace->parent;
		goto out;
	}

	found = component_search(component_workspace, name);
	if (found == NULL || !found->is_graph) {
		if (found == NULL) {
			HERROR("sub graph not found\n");
			ret = -ENOMEM; /* Which errno? */
		} else {
			HERROR("Not a graph\n");
			ret = -ENOMEM; /* Which errno? */
		}
		goto out;
	} else {
		component_workspace = found;
	}
out:
	HRETURN(ret);
}

static int workspace_cd_wrap(lua_State *L)
{
	const char *name = NULL;
	int ret = 0;
	int top = 0;
	HENTRY();

	top = lua_gettop(L);
	if (top != 1) {
		HERROR("argument error: too %s arguments\n", top > 1 ? "many" : "few" );
		goto out;
	}

	if (!lua_isstring(L, 1)) {
		HERROR("argument error: argument[%d] is expected to be a string, got %s\n", 1, luaL_typename(L, 1));
	} else {
		name = lua_tostring(L, 1);
		HASSERT(name);
		ret = workspace_cd(name);
	}

out:
	HRETURN(0);
}

static int workspace_type_wrap(lua_State *L)
{
	HENTRY();
	workspace_cd("types");
	workspace_list_wrap(L);
	workspace_cd("..");
	HRETURN(0);
}

static int workspace_copy_wrap(lua_State *L)
{
	const char *old_name = NULL;
	const char *new_name = NULL;
	struct component *new_component = NULL;
	struct component *old_component = NULL;
	int ret = 0;
	int top = 0;
	HENTRY();

	top = lua_gettop(L);
	if (top != 2) {
		HERROR("argument error: too %s arguments\n", top > 2 ? "many" : "few" );
		goto out;
	}

	if (!lua_isstring(L, 1) || !lua_isstring(L, 2)) {
		if (lua_isstring(L, 1)) {
			HERROR("argument error: argument[%d] is expected to be a string, got %s\n", 1, luaL_typename(L, 1));
		} else {
			HERROR("argument error: argument[%d] is expected to be a string, got %s\n", 2, luaL_typename(L, 2));
		}
		goto out;
	} else {
		old_name = lua_tostring(L, 1);
		new_name = lua_tostring(L, 2);
	}

	HASSERT(old_name);
	HASSERT(new_name);

	new_component = component_search(component_workspace, new_name);
	if (new_component != NULL) {
		HERROR("failed to copy, %s already exists\n", new_name);
		ret = -ENOMEM; /* Which errno? */
		goto out;
	}

	old_component = component_search(component_workspace, old_name);
	if (old_component == NULL) {
		HERROR("failed to copy, %s does not exist\n", old_name);
		ret = -ENOMEM; /* Which errno? */
		goto out;
	}
	new_component = component_copy(old_component, component_workspace, new_name);
out:
	HRETURN(0);
}

static int workspace_rename_wrap(lua_State *L)
{
	const char *old_name = NULL;
	const char *new_name = NULL;
	struct component *new_component = NULL;
	struct component *old_component = NULL;
	int ret = 0;
	int top = 0;
	HENTRY();

	top = lua_gettop(L);
	if (top != 2) {
		HERROR("argument error: too %s arguments\n", top > 2 ? "many" : "few" );
		goto out;
	}

	if (!lua_isstring(L, 1) || !lua_isstring(L, 2)) {
		if (!lua_isstring(L, 1)) {
			HERROR("argument error: argument[%d] is expected to be a string, got %s\n", 1, luaL_typename(L, 1));
		} else {
			HERROR("argument error: argument[%d] is expected to be a string, got %s\n", 2, luaL_typename(L, 2));
		}
		goto out;
	}

	old_name = lua_tostring(L, 1);
	new_name = lua_tostring(L, 2);

	HASSERT(old_name);
	HASSERT(new_name);

	new_component = component_search(component_workspace, new_name);
	if (new_component != NULL) {
		HERROR("failed to rename, %s already exist\n", new_name);
		ret = -ENOMEM; /* Which errno? */
		goto out;
	}

	old_component = component_search(component_workspace, old_name);
	if (old_component == NULL) {
		HERROR("failed to rename, %s does not exist\n", old_name);
		ret = -ENOMEM; /* Which errno? */
		goto out;
	}
	ret = component_rename(old_component, new_name);
out:
  HRETURN(ret);
}

static int digit_number(unsigned int number)
{
	int digit = 1;
	unsigned int max_number = 9;
	HENTRY();

	while(number > max_number) {
		max_number = max_number * 10 + 9;
		digit++;
	}
	HRETURN(digit);
}

#define MAX_NAME_LENGTH (1024)

static int workspace_declare_graph(lua_State *L, const char *name)
{
	char *type = NULL;
	int ret = 0;
	struct component *component = NULL;
	HENTRY();

	type = fsmL_field_getstring(L, 1, "type");
	if (type == NULL) {
		ret = -EINVAL;
		goto out;
	}

	component = component_search(component_workspace, name);
	if (component == NULL) {
		HERROR("failed to decalre: %s does not exists\n", name);
		ret = -EINVAL;
		goto out_free_type;
	}

	if (component->type) {
		HERROR("failed to decalre: component %s already has a type %s\n",
		       component->name, component->type->name);
		ret = -EINVAL;
		goto out_free_type;
	}

	if (component->pins == NULL) {
		HERROR("failed to decalre: component %s does not have any pins\n",
		       component->name);
		ret = -EINVAL;
		goto out_free_type;
	}

	ret = component_declare_graph(component, type);
out_free_type:
	MTFS_FREE_STR(type);
out:
	HRETURN(ret);
}

static int workspace_declare_filter(lua_State *L)
{
	char *type = NULL;
	int out_num = 0;
	int selector = 0;
	char *inpin_name = NULL;
	char **outpin_names = NULL;
	int outpin_num = 0;
	int ret = 0;
	int i = 0;
	HENTRY();

	type = fsmL_field_getstring(L, 1, "type");
	if (type == NULL) {
		goto out;
	}

	lua_getfield(L, 1, "inpin_name");
	if (lua_isstring(L, -1)) {
		MTFS_STRDUP(inpin_name, lua_tostring(L, -1));
		lua_remove(L, -1);
	} else if(lua_isnil(L, -1)) {
		HDEBUG("inpin_name not given, using in_0\n");
		MTFS_STRDUP(inpin_name, "in_0");
		lua_remove(L, -1);
	} else {
		HERROR("argument error: inpin_name is expected to be a string, got %s\n", luaL_typename(L, -1));
		lua_remove(L, -1);
		goto out_free_type;
	}

	lua_getfield(L, 1, "out_num");
	if (lua_isnumber(L, -1)) {
		out_num = lua_tointeger(L, -1);
		lua_remove(L, -1);
		if (out_num <= 0) {
			HERROR("argument error: out_num is expected to greater than 0, got %d\n", out_num);
			goto out_free_inpin;
		}
	} else {
		HERROR("argument error: out_num is expected to be a number, got %s\n", luaL_typename(L, -1));
		lua_remove(L, -1);
		goto out_free_inpin;
	}

	MTFS_ALLOC(outpin_names, sizeof(*outpin_names) * out_num);
	if (outpin_names == NULL) {
		HERROR("not enough memory\n");
		goto out_free_inpin;
	}

	lua_getfield(L, 1, "outpin_names");
	if (lua_istable(L, -1)) {
		outpin_num = lua_objlen(L, -1);
		if (outpin_num < 0 || outpin_num > out_num) {
			HERROR("argument error: too many outpin_names given, expected number is less or equal to %d\n", out_num);
			/* Pop outpin_names */
			lua_remove(L, -1);
			goto out_free_outpin_names;		
		}

		for(i = 0; i < outpin_num; i++) {
			int j = 0;
			lua_rawgeti(L, -1, i + 1);
			if (lua_isstring(L, -1)) {
				MTFS_STRDUP(outpin_names[i], lua_tostring(L, -1));
				lua_remove(L, -1);
			} else {
				HERROR("argument error: outpin_names[%d] is expected to be a string, got %s\n", i, luaL_typename(L, -1));
				lua_remove(L, -1);
				/* Pop outpin_names */
				lua_remove(L, -1);
				goto out_free_outpin;
			}

			for (j = 0; j < i; j ++) {
				if (strcmp(outpin_names[i], outpin_names[j]) == 0) {
					HERROR("argument error: outpin_names[%d] is the same with outpin_names[%d] %s\n",
                           j, i, outpin_names[j]);
					lua_remove(L, -1);
					/* Pop outpin_names */
					lua_remove(L, -1);
					goto out_free_outpin;
				}
			}
			if (strcmp(outpin_names[i], inpin_name) == 0) {
				HERROR("argument error: outpin_names[%d] is the same with inpin_name %s\n",
				       i, inpin_name);
				lua_remove(L, -1);
				/* Pop outpin_names */
				lua_remove(L, -1);
				goto out_free_outpin;
			}
		}
	} else {
		HDEBUG("no outpin_names setted, using default out_*\n");
		i = 0;
	}
	/* Pop outpin_names */
	lua_remove(L, -1);

	HDEBUG("lua_rawgeti out\n");
	if (i < out_num) {
		int number = digit_number(out_num - 1);
		for(; i < out_num; i++) {
			int tmp = digit_number(i);
			int pos = 0;

			outpin_names[i] = calloc(1, MAX_NAME_LENGTH);
			if (outpin_names[i] == NULL) {
				HERROR("not enough memory\n");
				goto out_free_outpin;
			}
			snprintf(outpin_names[i], MAX_NAME_LENGTH, "out_");
			pos = 4;
			for (; tmp < number; tmp++) {
				outpin_names[i][pos]= '0';
				pos++;
			}
			snprintf(outpin_names[i] + pos, MAX_NAME_LENGTH - pos, "%d", i);
		}
	}

	HDEBUG("inpin_names[0]: %s\n", inpin_name);
	for(i = 0; i < out_num; i++) {
		HDEBUG("outpin_names[%d] = %s\n", i, outpin_names[i]);
	}

	lua_getfield(L, 1, "selector");
	if (lua_isfunction(L, -1)) {
		/* Pop selector when ref */
		selector = luaL_ref(L, LUA_REGISTRYINDEX);
	} else {
		HERROR("argument error: selector is expected to be a funtion, got %s\n", luaL_typename(L, -1));
		/* Pop selector */
		lua_remove(L, -1);
		goto out_free_outpin;
	}

	ret = component_declare_filter(type, (const char*)inpin_name, out_num, (const char**)outpin_names, selector);
	if (ret) {
		goto out_unref_func;
	}
	goto out_free_outpin;
#if 0
	/* Push selector to stack */
	lua_rawgeti(L, LUA_REGISTRYINDEX, selector);
	/* lua_call will pop all argument and funtion itself */
	lua_call(L, 0, 0);
	luaL_unref(L, LUA_REGISTRYINDEX, selector);
#endif

out_unref_func:
	luaL_unref(L, LUA_REGISTRYINDEX, selector);
out_free_outpin:
	/* Free outpin_name[i] */
	for(i = 0; i < out_num; i++) {
		if (outpin_names[i]) {
			MTFS_FREE_STR(outpin_names[i]);
		}
	}
out_free_outpin_names:
	/* Free outpin_names */
	MTFS_FREE(outpin_names, sizeof(*outpin_names) * out_num);
out_free_inpin:
	MTFS_FREE_STR(inpin_name);
out_free_type:
	MTFS_FREE_STR(type);
out:
	HRETURN(0);
}

static int workspace_declare_wrap(lua_State *L)
{
	int top = 0;
	const char *name = NULL;
	HENTRY();

	top = lua_gettop(L);
	if (top != 1) {
		HERROR("argument error: argument number is expected to be 1, got %d\n", top);
		goto out;
	}

	if (!lua_istable(L, 1)) {
		HERROR("argument error: table expected, got %s\n", luaL_typename(L, 1));
		goto out;
	}

	lua_getfield(L, 1, "name");
	if (lua_isstring(L, -1)) {
		name = lua_tostring(L, -1);
		workspace_declare_graph(L, name);
	} else if (lua_isnil(L, -1)) {
		workspace_declare_filter(L);
	} else {
		HERROR("argument error: type is expected to be a string, got %s\n", luaL_typename(L, -1));
	}
	lua_remove(L, -1);
out:
	HRETURN(0);
}

static int workspace_new_wrap(lua_State *L)
{
	const char *type_name = NULL;
	const char *instance_name = NULL;
	int top = 0;
	int ret = 0;
	HENTRY();

	top = lua_gettop(L);
	if (top != 1) {
		HERROR("component new: bad argument number, expected 1 got %d\n", top);
		goto out;
	}

	if (!lua_istable(L, 1)) {
		HERROR("argument error: table expected, got %s\n", luaL_typename(L, 1));
		goto out;
	}

	lua_getfield(L, 1, "type");
	if (lua_isstring(L, -1)) {
		type_name = lua_tostring(L, -1);
	} else if(lua_isnil(L, -1)) {
		HDEBUG("type_name not given, newing a component without type\n");
		type_name = NULL;
	} else {
		HERROR("argument error: type is expected to be a string, got %s\n", luaL_typename(L, -1));
		goto out_remove_type_name;
	}

	lua_getfield(L, 1, "name");
	if (lua_isstring(L, -1)) {
		instance_name = lua_tostring(L, -1);
	} else {
		HERROR("argument error: type is expected to be a string, got %s\n", luaL_typename(L, -1));
		goto out_remove_instance_name;
	}

	if(instance_name == NULL) {
		HERROR("component new: bad name\n");
		ret = -ENOMEM; /* Which errno? */
	} else {
		struct component *new_component = NULL;
		if (type_name == NULL) {
			new_component = component_new_notype(component_workspace, instance_name);
		} else {
			new_component = component_new(component_workspace, type_name, instance_name);
		}
		if (IS_ERR(new_component)) {
			ret = PTR_ERR(new_component);
		}
	}
out_remove_instance_name:
	lua_remove(L, -1);
out_remove_type_name:
	lua_remove(L, -1);
out:
	HRETURN(0);
}

static int workspace_help_wrap(lua_State *L)
{
	HENTRY();
	printf("Filter commands:\n"
	       "    help(): show this message\n"
	       "    new({type=<type>, name=<name>}): alloc a component in workspace\n"
	       "    destroy(name): destroy a sub graph or sub component\n"
	       "    ls(): list all sub graphs and sub components in workspace\n"
	       "    type(): list all component types declared\n"
	       "    declare({type=<type>, out_num=<out_num>, selector=<selector>}): declare a component type\n");
	HRETURN(0);
}

static int workspace_link_wrap(lua_State *L)
{
	const char *source_name = NULL;
	const char *source_pin = NULL;
	const char *dest_name = NULL;
	const char *dest_pin = NULL;
	struct component *source_component = NULL;
	struct component *dest_component = NULL;
	int ret = 0;
	int i = 0;
	int top = 0;
	HENTRY();

	top = lua_gettop(L);
	if (top != 4) {
		HERROR("argument error: too %s arguments\n", top > 4 ? "many" : "few" );
		goto out;
	}

	for (i = 1; i <= 4; i++) {
		if (!lua_isstring(L, i)) {
			HERROR("argument error: argument[%d] is expected to be a string, got %s\n", i, luaL_typename(L, i));
			goto out;
		}
	}
	
	source_name = lua_tostring(L, 1);
	source_pin = lua_tostring(L, 2);
	dest_name = lua_tostring(L, 3);
	dest_pin = lua_tostring(L, 4);

	HASSERT(source_name);
	HASSERT(source_pin);
	HASSERT(dest_name);
	HASSERT(dest_pin);
	
	if (strcmp(source_name, dest_name) == 0) {
		HERROR("argument error: linking component %s to itself is not allowed\n", source_name);
		goto out;
	}	

	source_component = component_search(component_workspace, source_name);
	if (source_component == NULL) {
		HERROR("failed to link, %s does not exists\n", source_name);
		ret = -ENOMEM; /* Which errno? */
		goto out;
	}

	dest_component = component_search(component_workspace, dest_name);
	if (dest_component == NULL) {
		HERROR("failed to link, %s does not exist\n", dest_name);
		ret = -ENOMEM; /* Which errno? */
		goto out;
	}
	ret = component_link(source_component, source_pin, dest_component, dest_pin);
out:
	HRETURN(0);
}

static int workspace_unlink_wrap(lua_State *L)
{
	const char *source_name = NULL;
	const char *source_pin = NULL;
	const char *dest_name = NULL;
	const char *dest_pin = NULL;
	struct component *source_component = NULL;
	struct component *dest_component = NULL;
	int ret = 0;
	int i = 0;
	int top = 0;
	HENTRY();

	top = lua_gettop(L);
	if (top != 4) {
		HERROR("argument error: too %s arguments\n", top > 4 ? "many" : "few" );
		goto out;
	}

	for (i = 1; i <= 4; i++) {
		if (!lua_isstring(L, i)) {
			HERROR("argument error: argument[%d] is expected to be a string, got %s\n", i, luaL_typename(L, i));
			goto out;
		}
	}
	
	source_name = lua_tostring(L, 1);
	source_pin = lua_tostring(L, 2);
	dest_name = lua_tostring(L, 3);
	dest_pin = lua_tostring(L, 4);

	HASSERT(source_name);
	HASSERT(source_pin);
	HASSERT(dest_name);
	HASSERT(dest_pin);
	
	if (strcmp(source_name, dest_name) == 0) {
		HERROR("argument error: linking component %s to itself is not allowed\n", source_name);
		goto out;
	}	

	source_component = component_search(component_workspace, source_name);
	if (source_component == NULL) {
		HERROR("failed to link, %s does not exists\n", source_name);
		ret = -ENOMEM; /* Which errno? */
		goto out;
	}

	dest_component = component_search(component_workspace, dest_name);
	if (dest_component == NULL) {
		HERROR("failed to link, %s does not exist\n", dest_name);
		ret = -ENOMEM; /* Which errno? */
		goto out;
	}
	ret = component_unlink(source_component, source_pin, dest_component, dest_pin);
out:
	HRETURN(0);
}

static int workspace_general_wrap(lua_State *L, int (*func)(struct component *subcomponent))
{
	int top = 0;
	int i = 0;
	int ret = 0;
	HENTRY();

	top = lua_gettop(L);
	if (top == 0) {
		HERROR("argument error: too few arguments\n");
		goto out;
	}

	for(i = 1; i <= top; i++) {
		if (!lua_isstring(L, i)) {
			HERROR("argument error: argument[%d] is expected to be a string, got %s\n", i, luaL_typename(L, i));
		} else {
			struct component *component = NULL;
			const char *name = NULL;

			name = lua_tostring(L, i);
			HASSERT(name);
			component = component_search(component_workspace, name);
			if (component) {
				ret = func(component);
			} else {
				HERROR("%s: No such component\n", name);
			}
		}
	}
out:
	HRETURN(0);
}

static int workspace_remove_wrap(lua_State *L)
{
	int ret = 0;
	HENTRY();

	ret = workspace_general_wrap(L, component_remove);
	HRETURN(0);
}

#define MAX_BUFF_SIZE (2048)
int graphviz_indent = 0;
void code_line_print(char *buff)
{
	int i = 0;
	for (i = 0; i < graphviz_indent; i ++) {
		graphviz_print("\t");
	}
	graphviz_print(buff);
	graphviz_print("\n");
}

static int component_instance_dump2graphviz(struct component *component)
{
	char *buff = NULL;
	int ret = 0;
	HENTRY();

	MTFS_ALLOC(buff, MAX_BUFF_SIZE);
	if (buff == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	snprintf(buff, MAX_BUFF_SIZE, "%s [label= \"%s\"];",
	         component->name, component->name);
	code_line_print(buff);
	MTFS_FREE(buff, MAX_BUFF_SIZE);
out:
	HRETURN(ret);
}

static int component_outlinks2brother_dump2graphviz(struct component *component)
{
	char *buff = NULL;
	int ret = 0;
	HENTRY();

	MTFS_ALLOC(buff, MAX_BUFF_SIZE);
	if (buff == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	if (component->pins) {
		int i = 0;
		for (i = 0; i < component->pin_num; i++) {
			struct pin *next = NULL;
			struct component *next_component = NULL;
			
			if (component->pins[i].is_input) {
				continue;
			}
			
			next = component->pins[i].next;
			if (next == NULL) {
				continue;
			}

			next_component = next->parent;
			if (next_component->parent == component->parent) {
					snprintf(buff, MAX_BUFF_SIZE, "%s->%s;",
					         component->name, next_component->name);
					code_line_print(buff);
			}
		}
	}

	MTFS_FREE(buff, MAX_BUFF_SIZE);
out:
	HRETURN(ret);
}

static int workspace_dump2graphviz_wrap(lua_State *L)
{
	int top = 0;
	int i = 0;
	HENTRY();

	code_line_print("digraph G {");
	graphviz_indent++;
	top = lua_gettop(L);
	if (top == 0) {
		component_subs_eachdo(component_workspace, component_instance_dump2graphviz);
		component_subs_eachdo(component_workspace, component_outlinks2brother_dump2graphviz);
		goto out;
	}

	for(i = 1; i <= top; i++) {
		if (!lua_isstring(L, i)) {
			HERROR("argument error: argument[%d] is expected to be a string, got %s\n", i, luaL_typename(L, i));
		} else {
			struct component *component = NULL;
			const char *name = NULL;

			name = lua_tostring(L, i);
			HASSERT(name);
			component = component_search(component_workspace, name);
			if (component) {
				component_instance_dump2graphviz(component);
			} else {
				HERROR("%s: No such component\n", name);
			}
		}
	}
out:
	graphviz_indent--;
	code_line_print("}");
	HASSERT(graphviz_indent == 0);
	HRETURN(0);
}

static int workspace_pack_wrap(lua_State *L)
{
	int top = 0;
	char *name = NULL;
	int pin_num = 0;
	int i = 0;
	struct pin_map_table *pin_tables = NULL;
	struct component* component = NULL;
	HENTRY();

	top = lua_gettop(L);
	if (top != 1) {
		HERROR("component new: bad argument number, expected 1 got %d\n", top);
		goto out;
	}

	if (!lua_istable(L, 1)) {
		HERROR("argument error: table expected, got %s\n", luaL_typename(L, 1));
		goto out;
	}

	name = fsmL_field_getstring(L, 1, "name");
	if (!name) {
		goto out;
	}

	lua_getfield(L, 1, "pins");
	if (!lua_istable(L, -1)) {
		HERROR("argument error: type is expected to be a table, got %s\n", luaL_typename(L, -1));
		lua_remove(L, -1);
		goto out_free_name;
	}

	pin_num = lua_objlen(L, -1);
	if (pin_num <= 0) {
		HERROR("argument error: pin number expected to be greater than zero, got %d\n", pin_num);
		/* Pop pin_tables */
		lua_remove(L, -1);
		goto out_free_name;
	}

	HDEBUG("pin_tables: %d\n", pin_num);
	MTFS_ALLOC(pin_tables, sizeof(*pin_tables) * pin_num);
	if (pin_tables == NULL) {
		HERROR("not enough memory\n");
		/* Pop pin_tables */
		lua_remove(L, -1);
		goto out_free_name;
	}

	for(i = 0; i < pin_num; i++) {
		int j = 0;

		HDEBUG("pin_tables[%d]\n", i);
		lua_rawgeti(L, -1, i + 1);
		if (!lua_istable(L, -1)) {
			HERROR("argument error: table expected, got %s\n", luaL_typename(L, -1));
			/* Pop pin_table */
			lua_remove(L, -1);
			/* Pop pin_tables */
			lua_remove(L, -1);
			goto out_free_tables;
		}

		pin_tables[i].sub_component_name = fsmL_field_getstring(L, -1, "sub");
		if (pin_tables[i].sub_component_name == NULL) {
			/* Pop pin_table */
			lua_remove(L, -1);
			/* Pop pin_tables */
			lua_remove(L, -1);
			goto out_free_tables;
		}
		HDEBUG("pin_tables[%d].sub_component_name = %s\n", i, pin_tables[i].sub_component_name);

		pin_tables[i].sub_pin_name = fsmL_field_getstring(L, -1, "pin");
		if (pin_tables[i].sub_pin_name == NULL) {
			/* Pop pin_table */
			lua_remove(L, -1);
			/* Pop pin_tables */
			lua_remove(L, -1);
			goto out_free_tables;
		}
		HDEBUG("pin_tables[%d].sub_pin_name = %s\n", i, pin_tables[i].sub_pin_name);

		pin_tables[i].pin_name = fsmL_field_getstring(L, -1, "name");
		if (pin_tables[i].pin_name == NULL) {
			/* Pop pin_table */
			lua_remove(L, -1);
			/* Pop pin_tables */
			lua_remove(L, -1);
			goto out_free_tables;
		}
		HDEBUG("pin_tables[%d].pin_name = %s\n", i, pin_tables[i].pin_name);
		/* Pop pin_table */
		lua_remove(L, -1);

		for(j = 0; j < i; j++) {
			if (strcmp(pin_tables[i].pin_name, pin_tables[j].pin_name) == 0) {
				HERROR("pin_tables[%d] have same pin name with pin_tables[%d]\n", i, j);
				/* Pop pin_tables */
				lua_remove(L, -1);
				goto out_free_tables;
			}
		}
	}
	/* Pop pin_tables */
	lua_remove(L, -1);
	component = component_search(component_workspace, name);
	if (component == NULL) {
		HERROR("failed to pack, %s does not exists\n", name);
		goto out_free_tables;
	}
	component_pack(component, pin_num, pin_tables);
out_free_tables:
	for(i = 0; i < pin_num; i++) {
		if (pin_tables[i].sub_component_name) {
			MTFS_FREE_STR(pin_tables[i].sub_component_name);
		}
		if (pin_tables[i].sub_pin_name) {
			MTFS_FREE_STR(pin_tables[i].sub_pin_name);
		}
		if (pin_tables[i].pin_name) {
			MTFS_FREE_STR(pin_tables[i].pin_name);
		}
	}
	MTFS_FREE(pin_tables, sizeof(*pin_tables) * pin_num);
out_free_name:
	MTFS_FREE_STR(name);
out:
	HRETURN(0);
}

const struct luaL_reg workspacelib[] = {
	{"new", workspace_new_wrap},
	{"help", workspace_help_wrap},
	{"pwd", workspace_pwd_wrap},
	{"cd", workspace_cd_wrap},
	{"ls", workspace_list_wrap},
	{"type", workspace_type_wrap},
	{"pack", workspace_pack_wrap},
	{"rm", workspace_remove_wrap},
	{"mv", workspace_rename_wrap},
	{"cp", workspace_copy_wrap},
	{"declare", workspace_declare_wrap},
	{"link", workspace_link_wrap},
	{"unlink", workspace_unlink_wrap},
	{"dump", workspace_dump2graphviz_wrap},
	{NULL, NULL},
};
