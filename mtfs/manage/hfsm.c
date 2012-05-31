/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <stdio.h>
#include <string.h>
#include <debug.h>
#include <errno.h>
#include <memory.h>

#include "graphviz_internal.h"
#include "fsmL_internal.h"
#include "workspace_internal.h"
#include "component_internal.h"

struct Smain {
	int argc;
	char **argv;
	int status;
};

/*
** Copied from Lua/src/luaconf.h. luaconf.h may be included without LUA_USE_READLINE being defined.
@@ fsmL_readline defines how to show a prompt and then read a line from
@* the standard input.
@@ fsmL_saveline defines how to "save" a read line in a "history".
@@ fsmL_freeline defines how to free a line read by lua_readline.
**
*/
#ifdef HAVE_LIBREADLINE
#include <stdio.h>
#include <readline/readline.h>
#include <readline/history.h>
#define fsmL_readline(L,b,p)	((void)L, ((b)=readline(p)) != NULL)
#define fsmL_saveline(L,idx) do {                                      \
    if (lua_strlen(L,idx) > 0) {  /* non-empty line? */                \
        add_history(lua_tostring(L, idx));  /* add it to history */    \
    }                                                                  \
} while (0)
#define fsmL_freeline(L,b)	((void)L, free(b))
#else /* !HAVE_LIBREADLINE */
#define fsmL_readline(L,b,p) do {                                      \
    ((void)L, fputs(p, stdout), fflush(stdout),  /* show prompt */     \
    fgets(b, LUA_MAXINPUT, stdin) != NULL)  /* get line */             \
} while (0)
#define fsmL_saveline(L,idx)	{ (void)L; (void)idx; }
#define fsmL_freeline(L,b)	{ (void)L; (void)b; }
#endif /* !HAVE_LIBREADLINE */

static void fsmL_error(lua_State *L)
{
	const char *error_msg = NULL;

	if (!lua_isnil(L, -1)) {
		error_msg = lua_tostring(L, -1);
		if (error_msg == NULL) {
			error_msg = "error object is not a string";
		}
		HERROR("lua script returned error: %s\n", error_msg);
		lua_pop(L, 1);
	} else {
		error_msg = "no error msg";
		HERROR("%s\n", error_msg);
	}
}

static void fsmL_version(void) {
	fprintf(stdout, "fsm 0.0.1\n");
    fflush(stdout);
}

static void fsmL_set_prompt(lua_State *L)
{
	lua_pushstring(L, "hfsm>");
	lua_setfield(L, LUA_GLOBALSINDEX, "_PROMPT");
	lua_pushstring(L, "hfsm>>");
	lua_setfield(L, LUA_GLOBALSINDEX, "_PROMPT2");
}

static const char *fsmL_get_prompt(lua_State *L, int firstline)
{
	const char *p;

	lua_getfield(L, LUA_GLOBALSINDEX, firstline ? "_PROMPT" : "_PROMPT2");
	p = lua_tostring(L, -1);
	if (p == NULL) {
		p = (firstline ? LUA_PROMPT : LUA_PROMPT2);
	}
	lua_pop(L, 1);  /* remove global */
	return p;
}


static int fsmL_incomplete(lua_State *L, int status)
{
	if (status == LUA_ERRSYNTAX) {
		size_t lmsg;
		const char *msg = lua_tolstring(L, -1, &lmsg);
		const char *tp = msg + lmsg - (sizeof(LUA_QL("<eof>")) - 1);

		if (strstr(msg, LUA_QL("<eof>")) == tp) {
			lua_pop(L, 1);
			return 1;
		}
	}
	return 0;  /* else... */
}


static int fsmL_pushline(lua_State *L, int firstline)
{
	char buffer[LUA_MAXINPUT];
	char *b = buffer;
	size_t l;
	const char *prmt = fsmL_get_prompt(L, firstline);

	if (fsmL_readline(L, b, prmt) == 0) {
		return 0;  /* no input */
	}
	l = strlen(b);
	if (l > 0 && b[l-1] == '\n') { /* line ends with newline? */
		b[l-1] = '\0';  /* remove it */
	}

	if (firstline && b[0] == '=') { /* first line starts with `=' ? */
		lua_pushfstring(L, "return %s", b+1);  /* change it to `return' */
	} else {
		lua_pushstring(L, b);
	}
	fsmL_freeline(L, b);
	return 1;
}

static int fsmL_loadline(lua_State *L)
{
 	int status = 0;

	lua_settop(L, 0);
	if (!fsmL_pushline(L, 1)) {
		return -1;  /* no input */
	}

	for (; ;) {  /* repeat until gets a complete line */
		status = luaL_loadbuffer(L, lua_tostring(L, 1), lua_strlen(L, 1), "=stdin");
		if (!fsmL_incomplete(L, status)) {
			break;  /* cannot try to add lines? */
		}

		if (!fsmL_pushline(L, 0)) {
			/* no more input? */
			return -1;
		}
		lua_pushliteral(L, "\n");  /* add a new line... */
		lua_insert(L, -2);  /* ...between the two lines */
		lua_concat(L, 3);  /* join them */
	}

	fsmL_saveline(L, 1);
	lua_remove(L, 1);  /* remove line */

	return status;
}
/*
 * fsmL_loadfile - Load Lua chunk
 * @L: Lua state
 * @fname: filename, if null, load from stdin
 */
static int fsmL_loadfile(lua_State *L, const char *fname)
{
	struct stat st;
	int ret = 0;
	HENTRY();

	if (fname) {
		if (stat(fname, &st) == -1) {
			HERROR("failed to stat file [%s]: %s\n", fname, strerror(errno));
			ret = errno;
			goto out;
		}
	}
 	ret = luaL_loadfile(L, fname);
 out:
	HRETURN(ret);
}

static int fsmL_traceback(lua_State *L)
{
	lua_getfield(L, LUA_GLOBALSINDEX, "debug");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return 1;
	}
	lua_getfield(L, -1, "traceback");
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 2);
		return 1;
	}
	lua_pushvalue(L, 1);  /* pass error message */
	lua_pushinteger(L, 2);  /* skip this function and traceback */
	lua_call(L, 2, 1);  /* call debug.traceback */
	return 1;
}

static int fsmL_docall(lua_State *L, int narg, int clear) {
	int status;
	int base = lua_gettop(L) - narg;  /* function index */
	HENTRY();

	lua_pushcfunction(L, fsmL_traceback);  /* push traceback function */
	lua_insert(L, base);  /* put it under chunk and args */
	/* TODO: signal? */
	status = lua_pcall(L, narg, (clear ? 0 : LUA_MULTRET), base);
	lua_remove(L, base);  /* remove traceback function */
	/* force a complete garbage collection in case of errors */
	if (status != 0) {
		lua_gc(L, LUA_GCCOLLECT, 0);
	}

	HRETURN(status);
}

void fsmL_dotty(lua_State *L) {
	int ret = 0;

	while ((ret = fsmL_loadline(L)) != -1) {
		if (ret == 0) {
			ret = fsmL_docall(L, 0, 0);
	    }

		if (ret) {
			fsmL_error(L);
			continue;
		}

		if (lua_gettop(L) > 0) {  /* any result to print? */
			lua_getglobal(L, "print");
			lua_insert(L, 1);
			ret = lua_pcall(L, lua_gettop(L)-1, 0, 0);
			if (ret) {
				const char *error_msg = lua_pushfstring(L,
				       "error calling " LUA_QL("print") " (%s)",
				       lua_tostring(L, -1));
				HDEBUG("%s\n", error_msg);
			}
		}
	}
	lua_settop(L, 0);  /* clear stack */
	fputs("\n", stdout);
	fflush(stdout);
}

static int fsmL_create_metatable(lua_State *L, const char *metatable)
{
	luaL_newmetatable (L, metatable);
	/* set its __gc field */
	lua_pushstring (L, "__gc");
	lua_settable (L, -2);

	return 1;
}

struct fsmL_lib fsmL_libs[] = {
	{"workspace", "Workspace Metatable", workspacelib},
	{NULL, NULL, NULL},
};

#ifdef HAVE_LIBREADLINE
static char *fsmL_command_generator(const char *text, int state)
{
	static struct fsmL_lib *lib;
	static const struct luaL_reg *sub_cmd;
	static int text_len;
	const char *name = NULL;
	char *matched_str = NULL;

	/* If this is the first time called on this word, state is 0 */
	if (!state) {
		lib = fsmL_libs;
		sub_cmd = lib->lib;
		text_len = (int)strlen(text);
	}

	/* Return next name in the command list that paritally matches test */
	while ( (name = lib->name) ) {
		int name_len = strlen(name);
		int text_len = strlen(text);
		const char *subname = NULL;

		if (text_len > name_len) {
			if ((strncasecmp(name, text, name_len) == 0) &&
				(text[name_len] == '.')) {
				/* Input test matches name+'.' */
				const char *subtest = text + strlen(name) + 1;
				int subtest_len = text_len - name_len - 1;

				while( (subname = sub_cmd->name) ) {
					sub_cmd++;
					if (strncasecmp(subname, subtest, subtest_len) == 0) {
						matched_str = calloc(name_len + strlen(subname) + 4, 1);
						sprintf(matched_str, "%s.%s()", name, subname);
						goto out;
					}
				}
			}
		} else if (strncasecmp(name, text, text_len) == 0) {
			/* Input text matches a part of name */
			if ( (subname = sub_cmd->name) ) {
				sub_cmd++;
				matched_str = calloc(name_len + strlen(subname) + 4, 1);
				sprintf(matched_str, "%s.%s()", name, subname);
				goto out;
			}
		}

		/* No sub_cmd mathed, check next name */
		lib++;
		sub_cmd = lib->lib;
	}
out:
	return matched_str;
}

#if 0
static char *fsmL_command_nop_generator(const char *text, int state)
{
	return NULL;
}

static char *skipwhitespace(char * s)
{
	char * t;
	int len;

	len = (int)strlen(s);
	for (t = s; t <= s + len && isspace(*t); t++);
	return(t);
}
#endif

/* probably called by readline */
static char **fsmL_command_completion(char *text, int start, int end)
{
#if 0
	char *tail = rl_line_buffer + strlen(rl_line_buffer);
	char *head = rl_line_buffer;
	head = skipwhitespace(head);
	/* skip backward to whitespace */
	for (; tail >= head && !isspace(*tail); tail--);
	/*
	 * head point to first non whitespace.
	 * tail point to last whitespace.
	 * When tail != head:
	 * **aaa**bb
	 * **h***t**
	 * When tail == head:
	 * **aaa
	 * **h**
	 */
	if (head != tail) {
		HDEBUG("no completion\n");
		/* This is not first command, no completion */
		return rl_completion_matches(text, fsmL_command_nop_generator);
	}
#endif
	return rl_completion_matches(text, fsmL_command_generator);
}

#define HISTORY	100		/* Don't let history grow unbounded    */
static int fsmL_command_init()
{
	using_history();
	stifle_history(HISTORY);
	rl_attempted_completion_function = (CPPFunction *)fsmL_command_completion;
	rl_completion_entry_function = (void *)fsmL_command_generator;

	return 0;
}

#else /* !HAVE_LIBREADLINE */
static int fsmL_command_init()
{
	return 0;
}
#endif /* !HAVE_LIBREADLINE */

static int fsmL_register_libs(lua_State *L)
{
	struct fsmL_lib *lib = NULL;

	for (lib = fsmL_libs; lib != NULL; lib++) {
		if (lib->metatable_name == NULL || 
		    lib->name == NULL ||
		    lib->lib == NULL) {
			break;
		}
	    fsmL_create_metatable(L, lib->metatable_name);
		luaL_openlib(L, lib->name, lib->lib, 0);
		HDEBUG("command [%s] loaded\n", lib->name);
	}
	return 0;
}

static int fsmL_init(lua_State *L)
{
	int ret = 0;

	lua_gc(L, LUA_GCSTOP, 0);  /* stop collector during initialization */
	luaL_openlibs(L);  /* open libraries */
	lua_gc(L, LUA_GCRESTART, 0);
	fsmL_register_libs(L);
	return ret;
}

static int fsmL_pmain(lua_State *L) {
	int ret = 0;
	struct Smain *s = (struct Smain *)lua_touserdata(L, 1);
	HENTRY();

	if (s) {
	}

	ret = fsmL_init(L);
	if (ret) {
		HERROR("failed to init\n");
		goto out;
	}

	if (isatty(0)) {
		fsmL_set_prompt(L);
		fsmL_command_init();
		fsmL_version();
		fsmL_dotty(L);
	} else {
		ret = fsmL_loadfile(L, NULL) || fsmL_docall(L, 0, 1);
		if (ret && lua_isnil(L, -1)){
			const char *msg = lua_tostring(L, -1);
			if (msg == NULL) {
				msg = "(error object is not a string)";
			}
			HERROR("%s\n", msg);
			lua_pop(L, 1);
		}
	}
out:
	HRETURN(ret);
}

static int fsmL_start(struct Smain *s)
{
	int ret = 0;
	lua_State *L = NULL;
	
	L = lua_open();
	if (L == NULL) {
		HERROR("cannot create state: not enough memory");
		ret = -ENOMEM;
		goto out;
	}

	ret = lua_cpcall(L, &fsmL_pmain, s);
	if (ret) {
		fsmL_error(L);
	}
	lua_close(L);
out:
	return ret;
}

int main(int argc, char *argv[])
{
	int ret = 0;
	struct Smain s;

	/* Do init jobs */
	component_root_init();
	component_types_init();
	graphviz_init();

	s.argc = argc;
	s.argv = argv;
	ret = fsmL_start(&s);

	graphviz_finit();

	return ret;
}

char *fsmL_field_getstring(lua_State *L, int index, const char *name)
{
	char *value = NULL;
	lua_getfield(L, index, name);
	if (!lua_isstring(L, -1)) {
		HERROR("argument error:"
		       "%s is expected to be a string, got %s\n",
		       name, luaL_typename(L, -1));
		goto out;
	}
	MTFS_STRDUP(value, lua_tostring(L, -1));
	lua_remove(L, -1);
out:
	return value;	
}
