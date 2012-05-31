/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_RULE_TREE_H__
#define __MTFS_RULE_TREE_H__
#include <raid.h>	

typedef struct rule {
	char *string;
	raid_type_t raid_type;
} rule_t;

extern raid_type_t raid_default;

typedef struct rule_tree {
	int depth; /* position of key charactor */
	int start; /* start of range */
	int end; /* end of range */
	char key; /* charactor of key */
	int list_length;
	struct rule_tree **list; /* list of sons*/
	char *key_list; /* keys of sons*/
	raid_type_t rule;
} rule_tree_t;

#define MAX_RULE_NUM 32
#define MAX_RULE_LEN 32
rule_tree_t *rule_tree_construct(rule_t *rule_array, unsigned int rule_number);
int rule_tree_dump(rule_tree_t *root);
raid_type_t rule_tree_search(rule_tree_t *root, const char *string);
int rule_tree_destruct(rule_tree_t *root);
#endif /* __MTFS_RULE_TREE_H__ */
