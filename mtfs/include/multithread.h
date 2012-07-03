/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */
#ifndef __MTFS_MULTITHREAD_H__
#define __MTFS_MULTITHREAD_H__
#if defined (__linux__) && defined(__KERNEL__)
#error This head is only for user space use
#else /* defined (__linux__) && defined(__KERNEL__) */

#include <mtfs_list.h>
#include <stdbool.h>
#include <pthread.h>

#define THREAD_IDENTIFIER_LENGTH (40)
#define DEFAULT_THREAD_FIFO_DIRECTORY "/tmp"

typedef union thread_data {
	void *data;
} thread_data_t;

typedef struct thread_info {
	char identifier[THREAD_IDENTIFIER_LENGTH];
	struct mtfs_hlist_node hnode;
	pthread_t thread;
	unsigned int cpu;
	thread_data_t thread_data;
	bool setaffinity;
} thread_info_t;

typedef struct thread_group {
	unsigned int thread_number;
	char *identifier;
	void *(*start_routine)(thread_info_t *);
	//thread_data_t *thread_data_template;
} thread_group_t;

thread_info_t *create_thread_info(const char *identifier);
thread_info_t *find_thread_info(const char *identifier);
int add_thread_info(thread_info_t *thread_info);
int delete_thread_info(thread_info_t *thread_info);
int create_thread(const char *identifier, void *(*start_routine)(thread_info_t *), thread_info_t *thread_info);
int create_thread_group(const thread_group_t *thread_group, thread_data_t *thread_data_template);
int create_thread_groups(const thread_group_t *thread_groups, const int group_number);
int thread_setaffinity(pthread_t thread, int cpu);
#endif /* ! defined (__linux__) && defined(__KERNEL__) */
#endif /*__MTFS_MULTITHREAD_H__ */
