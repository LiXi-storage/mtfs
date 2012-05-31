/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_FSML_INTERNAL_H__
#define __MTFS_FSML_INTERNAL_H__

/* For lua_stdin_is_tty() */
#define lua_c

#ifdef HAVE_LIBREADLINE
/*
 * This should work.
 * But libcfs and readline both define RETURN.
 * So we have to walk around in order to compile successfully.
 */
//#define LUA_USE_READLINE
#endif /* HAVE_LIBREADLINE */

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

struct fsmL_lib {
	const char *name;
	const char *metatable_name;
	const struct luaL_reg *lib;
};

char *fsmL_field_getstring(lua_State *L, int index, const char *name);
#endif /* __MTFS_FSML_INTERNAL_H__ */
