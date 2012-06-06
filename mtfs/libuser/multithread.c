/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <debug.h>
#include <multithread.h>
#include <cfg.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <compat.h>

struct mtfs_hlist_head thread_head;

thread_info_t *create_thread_info(const char *identifier)
{
	thread_info_t *thread_info = NULL;
	
	thread_info = calloc(1, sizeof(*thread_info));
	if (thread_info == NULL) {
		HERROR("Not enough memory\n");
		goto out;
	}
	
	strncpy(thread_info->identifier, identifier, sizeof(thread_info->identifier));
out:
	return thread_info;
}

int destroy_thread_info(thread_info_t *thread_info)
{
	free(thread_info);
	return 0;
}

int add_thread_info(thread_info_t *thread_info)
{
	int ret = 0;
	
	if(find_thread_info(thread_info->identifier) != NULL) {
		HERROR("found existed thread info, unexpted\n");
		ret = -1;
		goto out;
	}
	mtfs_hlist_add_head(&(thread_info->hnode), &thread_head);
out:
	return ret;
}

int delete_thread_info(thread_info_t *thread_info)
{
	struct mtfs_hlist_node *pos = NULL;
	thread_info_t *tpos = NULL;
	int ret = -1;
	
	mtfs_hlist_for_each_entry(tpos, pos, &thread_head, hnode) {
		if (tpos == thread_info) {
			mtfs_hlist_del(pos);
			ret = 0;
			break;
		}
	}
	return ret;
}


thread_info_t *find_thread_info(const char *identifier)
{
	thread_info_t *thread_info = NULL;
	struct mtfs_hlist_node *pos = NULL;
	
	mtfs_hlist_for_each_entry(thread_info, pos, &thread_head, hnode) {
		if (strcmp(thread_info->identifier, identifier) == 0) {
			HDEBUG("thread %s found\n", thread_info->identifier);
			return thread_info;
			break;
		}
	}
	return NULL;	
}

int create_thread(const char *identifier, void *(*start_routine)(thread_info_t *), thread_info_t *thread_info)
{
	int ret = -1;
		
	ret = add_thread_info(thread_info);
	if (ret == -1) {
		HERROR("fail to add thread information for thread %s\n", identifier);
		goto out;		
	}	
	
	ret = pthread_create(&thread_info->thread, NULL, (void *(*)(void *))start_routine, (void *)thread_info);
	if (ret != 0) {
		HERROR("fail to create thread %s\n", identifier);
		ret = -1;
		goto thread_info_added_err;				
	}
	
	ret = 0;
	goto out;
thread_info_added_err:
	delete_thread_info(thread_info);
out:
	return ret;
}

#define CORE_NUM (4)
/* todo: get CORE_NUM! */
int thread_setaffinity(pthread_t thread, int cpu)
{
	cpu_set_t cpuset;
	int i;
	int ret = 0;
        
	CPU_ZERO(&cpuset);
	CPU_SET(cpu, &cpuset);

	if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset)) {
		HERROR("fail to pthread_setaffinity_np\n");
		ret = -1;
		goto out;
	}

	if (pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset)) {
		HERROR("fail to pthread_getaffinity_np\n");
		ret = -1;
		goto out;
	}

	for (i = 0; i < CORE_NUM; i++) {
		if (CPU_ISSET(i, &cpuset)) {
			HDEBUG("thread runing on CPU %d\n", i);
			break;
		}
	}
out:
	return ret;
}

int create_thread_group(const thread_group_t *thread_group, thread_data_t *thread_data_template)
{
	int ret = 0;
	char identifier[THREAD_IDENTIFIER_LENGTH]; /* todo: identifier[%u] length */
	unsigned int i = 0;
	thread_info_t *thread_info = NULL;

	
	if (thread_group == NULL || thread_group->thread_number == 0
		|| thread_group->start_routine == NULL) {
		HERROR("thread group illegal\n");
		ret = -1;
		goto out;
	}
	
	for (i = 0; i < thread_group->thread_number; i++) {
		snprintf(identifier, sizeof(identifier), "%s[%u]", thread_group->identifier, i);
		thread_info = create_thread_info(identifier);
		if (thread_info == NULL) {
			HERROR("fail to create thread information for thread %s\n", identifier);
			ret = -1;
			goto out;
		}

		/* init thread_info */
		if (thread_data_template) {
			thread_info->thread_data = *thread_data_template;
		}
		
		ret = create_thread(identifier, thread_group->start_routine, thread_info);
		if (ret == -1) {
			HERROR("fail to create thread for thread %s\n", identifier);
			destroy_thread_info(thread_info);
			goto out;
		}
	}
out:
	return ret;
}

int create_thread_groups(const thread_group_t *thread_groups, const int group_number)
{
	int ret = 0;
	int i = 0;
	for (i = 0; i < group_number; i++) {
		ret = create_thread_group(&(thread_groups[i]), NULL);
		if (ret == -1) {
			HERROR("fail to create thread group %s\n", thread_groups[i].identifier);
			goto out;
		}
	}
out:
	return ret;
}
