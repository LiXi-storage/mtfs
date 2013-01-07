/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_PARSE_OPTION_H__
#define __MTFS_PARSE_OPTION_H__
#include <memory.h>
#include <debug.h>

struct mount_barnch {
	int length;
	char *path;
};

#define MTFS_SBI_NOABORT  0x01
#define MTFS_SBI_CHECKSUM 0x02

struct mount_option {
	int    bnum;
	struct mount_barnch *branch;
	char  *mo_subject;
	__u32  mo_flags;   /* Flags set when mounting */
};

static inline int mount_option_init(struct mount_option *mount_option, int bnum)
{
	int ret = 0;

	MASSERT(bnum > 0);
	MASSERT(mount_option);

	mount_option->bnum = bnum;

	MTFS_ALLOC(mount_option->branch, sizeof(*mount_option->branch) * bnum);
	if (mount_option->branch == NULL) {
		goto out;
	}

out:
	return ret;
}

static inline int mount_option_fini(struct mount_option *option)
{
	int ret = 0;
	int i = 0;
	
	MASSERT(option);

	if (option->branch) {
		for(i = 0; i < option->bnum; i++) {
			if (option->branch[i].path) {
				MTFS_FREE(option->branch[i].path, option->branch[i].length);
			}
		}
		MTFS_FREE(option->branch, sizeof(*option->branch) * option->bnum);
	}

	if (option->mo_subject) {
		MTFS_FREE_STR(option->mo_subject);
	}
	memset(option, 0, sizeof(*option));

	return ret;
}

static inline struct mount_option *mount_option_alloc(void)
{
	struct mount_option *option = NULL;
	
	MTFS_ALLOC_PTR(option);
	if (option == NULL) {
		goto out;
	}
	MASSERT(option);
	MASSERT(option->bnum == 0);
out:
	return option;
}

static inline void mount_option_free(struct mount_option *option)
{
	MASSERT(option);
	mount_option_fini(option);
	MTFS_FREE_PTR(option);
	MASSERT(!option);
}

static inline void mount_option_dump(struct mount_option *option)
{
	int i = 0;
	MASSERT(option);
	MASSERT(option->branch);
	MASSERT(option->bnum > 0);
	
	MPRINT("bnum = %d\n", option->bnum);
	for(i = 0; i < option->bnum; i++) {
		MASSERT(option->branch[i].path);
		MPRINT("%s", option->branch[i].path);
		if (i < option->bnum - 1) {
			MPRINT(":");
		}
	}
	MPRINT("\n");
	MPRINT("subject = %s\n", option->mo_subject);
	MPRINT("\n");
}

static inline void append_dir(char *dirs, const char *one)
{
	if (*dirs) {
		strcat(dirs, ":");
	}
	strcat(dirs, one);
}

/* Parse options from mount. Returns 0 on success */
int mtfs_parse_options(char *input, struct mount_option *mount_option);
int parse_dir_option(char *dir_option, struct mount_option *mount_option);
#endif /* __MTFS_PARSE_OPTION_H__ */
