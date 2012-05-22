/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <debug.h>
#include <memory.h>
#include "rule_tree.h"

#if 0
/* functions for tests */
void list_dump(cfs_list_t *head)
{
	cfs_list_t *pos;
	HPRINT("HEAD");
	cfs_list_for_each(pos, head) {
		HPRINT"->%c", ((rule_tree_t *)(cfs_list_entry(pos, node_t, list)->data))->key);
	}
	HPRINT("\n");
}

void queue_dump(queue_t *queue)
{
	list_dump(&queue->head);
}


void string_dump(char **string, unsigned int string_number)
{
	int i;
	for (i = 0; i < string_number; i ++) {
		HPRINT("%s\n", string[i]);
	}
}
#endif

int main()
{
	int i = 0;
	rule_tree_t *root = NULL;
	int ret = 0;
	raid_type_t raid_type;
	int raid;
	int str_num = 0;
	rule_t *rule_array = NULL;
	char check_str[MAX_RULE_LEN * 2];
	int check_num = 0;

	
	fscanf(stdin, "%d", &str_num);
	HRFS_ALLOC(rule_array, sizeof(*rule_array) * str_num);
	if (rule_array == NULL) {
		ret = -ENOMEM;
		goto free_all;
	}
	
	for (i = 0; i < str_num; i ++) {
		HRFS_ALLOC(rule_array[i].string, MAX_RULE_LEN * 2);
		if (rule_array[i].string == NULL) {
			ret = -ENOMEM;
			goto free_all;
		}
		fscanf(stdin, "%s%d", rule_array[i].string, &raid);
		rule_array[i].raid_type = raid;
		if (rule_array[i].raid_type == RAID_TYPE_NONE) {
			ret = -EINVAL;
			goto free_all;
		}
	}
	
	/* construct tree */
	root = rule_tree_construct(rule_array, str_num);
	if (IS_ERR(root)) {
		ret = PTR_ERR(root);
		HERROR("construct rule tree failed\n");
		goto free_all;
	}
	
	rule_tree_dump(root);
	
	fscanf(stdin, "%d", &check_num);
	for(i = 0; i < check_num; i++) {
		fscanf(stdin, "%s", check_str);
		raid_type = rule_tree_search(root, check_str);
		raid = raid_type;
		HPRINT("%d\n", raid);
	}
	
	ret = rule_tree_destruct(root);
free_all:
	if (rule_array != NULL) {
		for (i = 0; i < str_num; i ++) {
			if (rule_array[i].string != NULL) {
				HRFS_FREE(rule_array[i].string, MAX_RULE_LEN * 2);
			}			
		}	
		HRFS_FREE(rule_array, sizeof(*rule_array) * str_num);
	}
	return ret;
}
