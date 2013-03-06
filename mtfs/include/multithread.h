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

struct thread_info {
	char identifier[THREAD_IDENTIFIER_LENGTH];
	struct mtfs_hlist_node hnode;
	pthread_t thread;
	unsigned int cpu;
	void *thread_data;
	bool setaffinity;
};

struct thread_group {
	unsigned int thread_number;
	char *identifier;
	void *(*start_routine)(struct thread_info *);
	struct thread_info **thread_infos;
};

struct thread_info *create_thread_info(const char *identifier);
struct thread_info *find_thread_info(const char *identifier);
int add_thread_info(struct thread_info *thread_info);
int delete_thread_info(struct thread_info *thread_info);
int create_thread(const char *identifier, void *(*start_routine)(struct thread_info *), struct thread_info *thread_info);
int create_thread_group(struct thread_group *thread_group, void *thread_data_template);
int create_thread_groups(struct thread_group *thread_groups, const int group_number);
int thread_setaffinity(pthread_t thread, int cpu);
void stop_thread_group(struct thread_group *thread_group);
#endif /* ! defined (__linux__) && defined(__KERNEL__) */
#endif /*__MTFS_MULTITHREAD_H__ */
