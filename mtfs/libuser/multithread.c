/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <debug.h>
#include <memory.h>
#include <multithread.h>
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

struct thread_info *create_thread_info(const char *identifier)
{
	struct thread_info *thread_info = NULL;

	MTFS_ALLOC_PTR(thread_info);
	if (thread_info == NULL) {
		MERROR("Not enough memory\n");
		goto out;
	}
	
	strncpy(thread_info->identifier, identifier, sizeof(thread_info->identifier));
out:
	return thread_info;
}

int destroy_thread_info(struct thread_info *thread_info)
{
	MTFS_FREE_PTR(thread_info);
	return 0;
}

int add_thread_info(struct thread_info *thread_info)
{
	int ret = 0;
	
	if(find_thread_info(thread_info->identifier) != NULL) {
		MERROR("found existed thread info, unexpted\n");
		ret = -1;
		goto out;
	}
	mtfs_hlist_add_head(&(thread_info->hnode), &thread_head);
out:
	return ret;
}

int delete_thread_info(struct thread_info *thread_info)
{
	struct mtfs_hlist_node *pos = NULL;
	struct thread_info *tpos = NULL;
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


struct thread_info *find_thread_info(const char *identifier)
{
	struct thread_info *thread_info = NULL;
	struct mtfs_hlist_node *pos = NULL;
	
	mtfs_hlist_for_each_entry(thread_info, pos, &thread_head, hnode) {
		if (strcmp(thread_info->identifier, identifier) == 0) {
			MDEBUG("thread %s found\n", thread_info->identifier);
			return thread_info;
			break;
		}
	}
	return NULL;	
}

int create_thread(const char *identifier, void *(*start_routine)(struct thread_info *), struct thread_info *thread_info)
{
	int ret = -1;
		
	ret = add_thread_info(thread_info);
	if (ret == -1) {
		MERROR("fail to add thread information for thread %s\n", identifier);
		goto out;		
	}	
	
	ret = pthread_create(&thread_info->thread, NULL, (void *(*)(void *))start_routine, (void *)thread_info);
	if (ret != 0) {
		MERROR("fail to create thread %s\n", identifier);
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
		MERROR("fail to pthread_setaffinity_np\n");
		ret = -1;
		goto out;
	}

	if (pthread_getaffinity_np(thread, sizeof(cpu_set_t), &cpuset)) {
		MERROR("fail to pthread_getaffinity_np\n");
		ret = -1;
		goto out;
	}

	for (i = 0; i < CORE_NUM; i++) {
		if (CPU_ISSET(i, &cpuset)) {
			MDEBUG("thread runing on CPU %d\n", i);
			break;
		}
	}
out:
	return ret;
}

int create_thread_group(struct thread_group *thread_group, void *thread_data_template)
{
	int ret = 0;
	char identifier[THREAD_IDENTIFIER_LENGTH]; /* todo: identifier[%u] length */
	unsigned int i = 0;
	struct thread_info *thread_info = NULL;

	if (thread_group == NULL || thread_group->thread_number == 0
		|| thread_group->start_routine == NULL) {
		MERROR("thread group illegal\n");
		ret = -1;
		goto out;
	}

	MTFS_ALLOC(thread_group->thread_infos,
	           sizeof(* thread_group->thread_infos) * thread_group->thread_number); 
	if (thread_group->thread_infos == NULL) {
		MERROR("not enough memory\n");
		ret = -1;
		goto out;
	}
	
	for (i = 0; i < thread_group->thread_number; i++) {
		snprintf(identifier, sizeof(identifier), "%s[%u]", thread_group->identifier, i);
		thread_info = create_thread_info(identifier);
		if (thread_info == NULL) {
			MERROR("fail to create thread information for thread %s\n", identifier);
			ret = -1;
			MBUG();
			goto out;
		}

		/* init thread_info */
		if (thread_data_template) {
			thread_info->thread_data = thread_data_template;
		}
		
		ret = create_thread(identifier, thread_group->start_routine, thread_info);
		if (ret == -1) {
			MERROR("fail to create thread for thread %s\n", identifier);
			destroy_thread_info(thread_info);
			MBUG();
			goto out;
		}
		thread_group->thread_infos[i] = thread_info;
	}
out:
	return ret;
}

int create_thread_groups(struct thread_group *thread_groups, const int group_number)
{
	int ret = 0;
	int i = 0;
	for (i = 0; i < group_number; i++) {
		ret = create_thread_group(&(thread_groups[i]), NULL);
		if (ret == -1) {
			MERROR("fail to create thread group %s\n", thread_groups[i].identifier);
			goto out;
		}
	}
out:
	return ret;
}

void stop_thread_group(struct thread_group *thread_group)
{
	int i = 0;
	int ret = 0;

	for (i = 0; i < thread_group->thread_number; i++) {
		ret = pthread_cancel(thread_group->thread_infos[i]->thread);
		if (ret) {
			MERROR("failed to cancel thread\n");
		}
		ret = pthread_join(thread_group->thread_infos[i]->thread, NULL);
		if (ret) {
			MERROR("failed to join thread\n");
		}
		
		destroy_thread_info(thread_group->thread_infos[i]);
	}
	MTFS_FREE(thread_group->thread_infos,
	          sizeof(*thread_group->thread_infos) * thread_group->thread_number); 
}

void stop_thread_groups(struct thread_group *thread_groups, const int group_number)
{
	int i = 0;
	
	for (i = 0; i < group_number; i++) {
		stop_thread_group(&(thread_groups[i]));
	}
}