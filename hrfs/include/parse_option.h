/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_PARSE_OPTION_H__
#define __HRFS_PARSE_OPTION_H__
#include <memory.h>
#include <debug.h>

struct mount_barnch {
	int length;
	char *path;
};

typedef struct mount_option {
	int bnum;
	struct mount_barnch *branch;
	int debug_level;
} mount_option_t;

static inline int mount_option_init(mount_option_t *mount_option, int bnum)
{
	int ret = 0;

	HASSERT(bnum > 0);
	HASSERT(mount_option);

	memset(mount_option, 0, sizeof(*mount_option));
	mount_option->bnum = bnum;

	HRFS_ALLOC(mount_option->branch, sizeof(*mount_option->branch) * bnum);
	if (mount_option->branch == NULL) {
		goto out;
	}

out:
	return ret;
}

static inline int mount_option_finit(struct mount_option *option)
{
	int ret = 0;
	int i = 0;
	
	HASSERT(option);

	if (option->branch) {
		for(i = 0; i < option->bnum; i++) {
			if (option->branch[i].path) {
				HRFS_FREE(option->branch[i].path, option->branch[i].length);
			}
		}
		HRFS_FREE(option->branch, sizeof(*option->branch) * option->bnum);
	}
	memset(option, 0, sizeof(*option));

	return ret;
}

static inline struct mount_option *mount_option_alloc(void)
{
	struct mount_option *option = NULL;
	
	HRFS_ALLOC_PTR(option);
	if (option == NULL) {
		goto out;
	}
	HASSERT(option);
	HASSERT(option->bnum == 0);
	HASSERT(option->debug_level == 0);
out:
	return option;
}

static inline void mount_option_free(struct mount_option *option)
{
	HASSERT(option);
	mount_option_finit(option);
	HRFS_FREE_PTR(option);
	HASSERT(!option);
}

static inline void mount_option_dump(struct mount_option *option)
{
	int i = 0;
	HASSERT(option);
	HASSERT(option->branch);
	HASSERT(option->bnum > 0);
	
	HPRINT("bnum = %d\n", option->bnum);
	for(i = 0; i < option->bnum; i++) {
		HASSERT(option->branch[i].path);
		HPRINT("%s", option->branch[i].path);
		if (i < option->bnum - 1) {
			HPRINT(":");
		}
	}
	HPRINT("\n");
	HPRINT("debug_level = %d\n", option->debug_level);
}

static inline void append_dir(char *dirs, const char *one)
{
	if (*dirs) {
		strcat(dirs, ":");
	}
	strcat(dirs, one);
}
/* Parse options from mount. Returns 0 on success */
int hrfs_parse_options(char *input, mount_option_t *mount_option);
int parse_dir_option(char *dir_option, mount_option_t *mount_option);
#endif /* __HRFS_PARSE_OPTION_H__ */
