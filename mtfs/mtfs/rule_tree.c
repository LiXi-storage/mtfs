/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

/*
 * For both kernel and userspace use
 * DO NOT use anything special that opposes this purpose
 */
#include <debug.h>
#include <mtfs_string.h>
#include <mtfs_queue.h>
#include <memory.h>
#include <bsearch.h>
#include <rule_tree.h>

raid_type_t raid_default = RAID_TYPE_RAID0;

/*
 * Function: static inline void reverse_string(const char *string)
 * Purpose : Reverse a string.
 * Params  : string - string to reverse
 */
static inline void reverse_string(char *string)
{
	char tmp;
	char *head = (char *)string;
	char *tail = (char *)(string + strlen(string) - 1);

	if (strlen(string) <= 0) {
		return;
	}
	
	while (head < tail) {
		tmp = *head;
		*head = *tail;
		*tail = tmp;
		tail --;
		head ++;
	}
}

/*
 * Function: int rule_tree_dump(rule_tree_t *root)
 * Purpose : Dump a rule_tree to stdout
 * Params  : root - root of a rule_tree
 * Returns : !0 on error, 0 on success
 */
int rule_tree_dump(rule_tree_t *root)
{
	int ret = 0;
	queue_t tmp_queue;
	rule_tree_t *tmp_root = NULL;
	int depth = 0;
	int former = 0;
	char key;
	char teminate = '.';
	
	MASSERT(root != NULL);
	/* Easy to prove that queue length is no bigger than rule_number */
	ret = queue_initialise(&tmp_queue, MAX_RULE_NUM);
	if (ret) {
		MERROR("queue initialise failed\n");
		goto out;
	}
	
	tmp_root = root;
	depth = -1;
	while (tmp_root) {
		int i = 0;
		if (tmp_root->list_length != 0 && tmp_root->list != NULL) {
			for (i = 0; i < tmp_root->list_length; i++) {
				ret = __queue_add(&tmp_queue, (void *)tmp_root->list[i], 0);
				if (ret) {
					MERROR("queue full\n");
					goto free_all;
				}
			}
		}
		
		key = tmp_root->key;
		if (key == '\0') {
			key = teminate;
		}

		if (tmp_root->depth > depth) {
			if (depth != -1) {
				MPRINT("\n");
			}
			former = 0;
		}
		
		/* Print NULL lines */
		if (former < tmp_root->start) {
			if (former == 0) {
				depth =	tmp_root->depth;
				MPRINT("%c", teminate);
				former++;
			}
			for (i = former; i < tmp_root->start; i++) {
				MPRINT("|%c", teminate);
			}
		}
		
		if (tmp_root->depth > depth) {
			MPRINT("%c", key);
		} else {
			MPRINT("|%c", key);
		}
		
		for(i = tmp_root->start + 1; i <= tmp_root->end; i++) {
			MPRINT("_%c", key);
		}
		
		if (tmp_root->depth > depth) {
			depth = tmp_root->depth;
		}
		former = tmp_root->end + 1;
				
		tmp_root = queue_remove(&tmp_queue);
	}
	MPRINT("\n");
free_all:
	queue_free(&tmp_queue);
out:
	return ret;
}

/*
 * Function: void rule_tree_node_free(rule_tree_t * node)
 * Purpose : Free a node of rule_tree
 * Params  : node - node of a rule_tree
 */
void rule_tree_node_free(rule_tree_t *node)
{
		MASSERT(node);
		if (node->list != NULL) {
			MTFS_FREE(node->list, sizeof(*node->list) * node->list_length);
		}
		if (node->key_list != NULL) {
			MTFS_FREE(node->key_list, sizeof(*node->key_list) * node->list_length);
		}
		MTFS_FREE_PTR(node);
}

/*
 * Function: int rule_tree_destruct(rule_tree_t *root)
 * Purpose : Destruct a rule_tree
 * Params  : root - root of a rule_tree
 * Returns : !0 on error, 0 on success
 */
int rule_tree_destruct(rule_tree_t *root)
{
	int ret = 0;
	queue_t tmp_queue;
	rule_tree_t *tmp_root = NULL;
	
	MASSERT(root != NULL);
	/* Easy to prove that queue length is no bigger than rule_number */
	ret = queue_initialise(&tmp_queue, MAX_RULE_NUM);
	if (ret) {
		MERROR("queue initialise failed\n");
		goto out;
	}
	
	tmp_root = root;
	while (tmp_root) {
		if (tmp_root->list_length != 0 && tmp_root->list != NULL) {
			int i = 0;

			for (i = 0; i < tmp_root->list_length; i++) {
				ret = __queue_add(&tmp_queue, (void *)tmp_root->list[i], 0);
				if (ret) {
					MERROR("queue full\n");
					goto free_all;
				}
			}
		}
		
		rule_tree_node_free(tmp_root);
		tmp_root = queue_remove(&tmp_queue);
	}
free_all:
	while((tmp_root = queue_remove(&tmp_queue)) != NULL) {
		rule_tree_node_free(tmp_root);
		MERROR("memory leak deteced\n");
	}	
	queue_free(&tmp_queue);
out:
	return ret;
}

/*
 * Function: rule_tree_t *__rule_tree_construct(rule_t *rule_array, unsigned int rule_number)
 * Purpose : Construct a rule_tree
 * Params  : rule_array - array of rule_string which is reversed, sorted and checked
 *	     rule_number  - rule number
 * Returns : root of tree constructed
 */
rule_tree_t *__rule_tree_construct(const rule_t *rule_array, unsigned int rule_number)
{
	int ret = 0;
	queue_t queue;
	queue_t tmp_queue;
	rule_tree_t *root = NULL;
	rule_tree_t *tmp_root = NULL;
	
	/* Easy to prove that queue length is no bigger than rule_number */
	ret = queue_initialise(&queue, rule_number);
	if (ret) {
		MERROR("queue initialise failed\n");
		goto out;
	}
	
	ret = queue_initialise(&tmp_queue, rule_number);
	if (ret) {
		MERROR("temporary queue initialise failed\n");
		goto free_queue;
	}	

	MTFS_ALLOC_PTR(root);
	if (root == NULL) {
		ret = -ENOMEM;
		goto free_queue;
	}
	
	root->depth = 0;
	root->start = 0;
	root->end = rule_number - 1;
	root->key = '/';
	root->list = NULL;
	root->list_length = 0;
	root->key_list = NULL;
	root->rule = RAID_TYPE_NONE;
	
	tmp_root = root;	
	while (tmp_root) {
		int i = 0;
		int list_length = 0;
		rule_tree_t *new = NULL;
		
		for(i = tmp_root->start; i <= tmp_root->end; i ++) {
			char key = rule_array[i].string[tmp_root->depth];
			
			MTFS_ALLOC_PTR(new);		
			if (!new) {
				ret = -ENOMEM;
				goto free_all;
			}
			
			new->depth = tmp_root->depth + 1;			
			new->start = i;
			do {
				i++;
			} while((i <= tmp_root->end) &&
				(tmp_root->depth <= strlen(rule_array[i].string)) && 
				(rule_array[i].string[tmp_root->depth]== key));
			i--;
			new->end = i;
			new->key = key;
			new->list = NULL;
			new->list_length = 0;
			new->key_list = NULL;
			new->rule = RAID_TYPE_NONE;
			
			ret = __queue_add(&tmp_queue, (void *)new, 0);
			if (ret) {
					MERROR("queue full\n");
					MTFS_FREE_PTR(new);
					goto free_all;
			}
			if (new->key != '\0') {
				list_length++;
			} else {
				/* No same rule allowed */
				MASSERT(new->start == new->end);
				new->rule = rule_array[new->end].raid_type;
			}
		}
		
		if (list_length > 0) {
			tmp_root->list_length = list_length;
			MTFS_ALLOC(tmp_root->list, sizeof(*tmp_root->list) * list_length);
			MTFS_ALLOC(tmp_root->key_list, sizeof(*tmp_root->key_list) * list_length);
		}
			
		i = 0;
		while((new = queue_remove(&tmp_queue)) != NULL) {
			if (new->key != '\0') {
				tmp_root->list[i] = new;
				tmp_root->key_list[i] = new->key;
				i++;
				ret = __queue_add(&queue, (void *)new, 0);
				if (ret) {
						MERROR("queue full\n");
						MTFS_FREE_PTR(new);
						goto free_all;
				}
			} else {
				tmp_root->rule = new->rule;
				MDEBUG("rule set\n");
				MTFS_FREE_PTR(new);
			}
		}
		MASSERT(i == list_length);
		
		tmp_root = queue_remove(&queue);
	}
	goto free_queue_data;
	
free_all:
	rule_tree_destruct(root);

free_queue_data:
	while((tmp_root = queue_remove(&queue)) != NULL) {
		rule_tree_node_free(tmp_root);
	}

	while((tmp_root = queue_remove(&tmp_queue)) != NULL) {
		rule_tree_node_free(tmp_root);
	}	
	queue_free(&tmp_queue);

free_queue:
	queue_free(&queue);
out:
	if (ret != 0) {
		root = ERR_PTR(ret);
	}
	return root;
}

/*
 * Function: raid_type_t rule_tree_search(rule_tree_t *root, const char *string)
 * Purpose : Search a rule_tree
 * Params  : root - root of a rule_tree
 *	     string - file name to be searched
 * Returns : longest matched raid_type, raid_default if not matched
 */
raid_type_t rule_tree_search(rule_tree_t *root, const char *string)
{
	rule_tree_t *tmp_root = NULL;
	raid_type_t rule = raid_default;
	const char *ptr = NULL;
	const char *tmp_string = &string[strlen(string)];

	MASSERT(root != NULL);
	MASSERT(string != NULL);
	
	tmp_root = root;
	while (tmp_root) {
		if (tmp_root->rule != RAID_TYPE_NONE) {
			MDEBUG("found one rule\n");
			rule = tmp_root->rule;
		}

		tmp_string--;
		if (tmp_string < string) {
			break;
		}
		
		if (tmp_root->list_length == 0) {
			break;
		} else {
			MASSERT(tmp_root->key != '\0');
			MASSERT(tmp_root->list != NULL);
			MASSERT(tmp_root->key_list != NULL);
			MDEBUG("judge %c\n", *tmp_string);
			ptr = bsearch(tmp_string, tmp_root->key_list, tmp_root->list_length,
										 sizeof(char), compare_char_pointer);
			if (ptr == NULL) {
				break;
			}
			MDEBUG("found %c\n", *tmp_string);
			tmp_root = tmp_root->list[ptr - tmp_root->key_list];
		}
	}
	
	return rule;
}

/*
 * Function: const rule_t *have_equal_string(const rule_t *rule_array, unsigned int str_num)
 * Purpose : find equal string in a sorted string array
 * Params  : rule_array - rule_array to search
 *           str_num - number of rule 
 * Returns : rule that equal to its follower
 */
const rule_t *have_equal_string(const rule_t *rule_array, unsigned int str_num)
{
	int i;
	
	for (i = 0; i < str_num -1 ; i ++) {
		if (strcmp(rule_array[i].string, rule_array[i + 1].string) == 0) {
			return &rule_array[i];
		}
	}
	return NULL;
}

static inline int compare_rule_pointer(const void *p1, const void *p2)
{
	return strcmp(((rule_t *)p1)->string,((rule_t *)p2)->string);
}

/*
 * Function: rule_tree_t *rule_tree_construct(rule_t *rule_array, unsigned int rule_number)
 * Purpose : Construct a rule_tree
 * Params  : rule_array - array of rule
 *	     rule_number  - rule number
 * Returns : root of tree constructed
 */
rule_tree_t *rule_tree_construct(rule_t *rule_array, unsigned int rule_number)
{
	int ret = 0;
	rule_tree_t *root = NULL;
	const rule_t *equal_rule = NULL;
	int i = 0;

	MASSERT(rule_array != NULL);
	/* Judge for distruct */
	MASSERT(rule_number < MAX_RULE_NUM);
	
	/* step1: reverse */
	for (i = 0; i < rule_number; i ++) {
		MASSERT(rule_array[i].string != NULL);
		if (strlen(rule_array[i].string) > MAX_RULE_LEN) {
			ret = -EINVAL;
			goto out;
		}
		reverse_string(rule_array[i].string);
	}

	/* step2: sort */
	mtfs_sort(rule_array, rule_number, sizeof(rule_t), compare_rule_pointer);
#if 0
	for (i = 0; i < rule_number; i ++) {
		MPRINT("%s->%s\n", rule_array[i].string, raid_type2string(rule_array[i].raid_type));
	}
#endif
	
	/* step3: find equal */
	equal_rule = have_equal_string(rule_array, rule_number);
	if (equal_rule != NULL) {
		MERROR("Found equal rule string: %s = %s\n", equal_rule->string, raid_type2string(equal_rule->raid_type));
		ret = -EINVAL;
		goto out;
	}
	
	/* step4: construct tree */
	root = __rule_tree_construct(rule_array, rule_number);
	if (IS_ERR(root)) {
		goto out;
	}	

out:
	if (ret) {
		root = ERR_PTR(ret);
	}
	return root;
}
