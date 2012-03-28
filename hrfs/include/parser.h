/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_PARSER_H__
#define __HRFS_PARSER_H__
#if defined (__linux__) && defined(__KERNEL__)
#include <linux/parser.h>
#else /* !(defined (__linux__) && defined(__KERNEL__)) */
/*
 * Copy from linux/include/linux/parser.h
 * Only for userspace use
 *
 * Header for lib/parser.c
 * Intended use of these functions is parsing filesystem argument lists,
 * but could potentially be used anywhere else that simple option=arg
 * parsing is required.
 */


/* associates an integer enumerator with a pattern string. */
struct match_token {
	int token;
	char *pattern;
};

typedef struct match_token match_table_t[];

/* Maximum number of arguments that match_token will find in a pattern */
enum {MAX_OPT_ARGS = 3};

/* Describe the location within a string of a substring */
typedef struct {
	char *from;
	char *to;
} substring_t;

int match_token(char *, match_table_t table, substring_t args[]);
int match_int(substring_t *, int *result);
int match_octal(substring_t *, int *result);
int match_hex(substring_t *, int *result);
void match_strcpy(char *, substring_t *);
char *match_strdup(substring_t *);
#endif /* !(defined (__linux__) && defined(__KERNEL__)) */
#endif /* __HRFS_PARSER_H__ */
