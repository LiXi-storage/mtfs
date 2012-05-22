/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <memory.h>
#include <mtfs_random.h>
#include <debug.h>
#include <multithread.h>
#include <debug.h>
#include <memory.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/prctl.h>

typedef struct block {
	char *data;
	size_t block_size;
} block_t;

#define MAX_BUFF_LENGTH 104857600
#define MAX_LSEEK_OFFSET MAX_BUFF_LENGTH
pthread_cond_t cond;
pthread_rwlock_t rwlock;
pthread_mutex_t mutex;
#define MAX_BNUM 16
int bnum = 2;
char *branch_path[MAX_BNUM];
int branch_fd[MAX_BNUM];
char *path;
int fd;

static inline block_t *alloc_block(int block_size)
{
	block_t *block = NULL;
	
	HASSERT(block_size > 0);
	
	HRFS_ALLOC_PTR(block);
	if (block == NULL) {
		goto out;
	}
	
	HRFS_ALLOC(block->data, block_size);
	if (block->data == NULL) {
		goto free_block;
	}

	block->block_size = block_size;
	goto out;

free_block:
	HRFS_FREE_PTR(block);
out:
	return block;
}

static inline int free_block(block_t *block)
{
	HRFS_FREE(block->data, block->block_size);
	HRFS_FREE_PTR(block);
	return 0;
}

int random_fill_block(block_t *block)
{
	static int fill_char = 0;
	static int fill_max = 0;
	int fill_time = 0;
	int char_number = block->block_size;
	int left_char = 0;
	int i = 0;
	int j = 0;
	int data = 0;
	int position = 0;
	
	if (fill_char == 0) {
		if (RAND_MAX >= 0xFFFFFF) {
			fill_char = 3;
			fill_max = 0xFFFFFF;
		} else if (RAND_MAX >= 0xFFFF) {
			fill_char = 2;
			fill_max = 0xFFFF;
		} else if (RAND_MAX >= 0xFF) {
			fill_char = 1;
			fill_max = 0xFF;
		}
		//HASSERT(fill_char);
	}
	
	fill_time = char_number / fill_char;
	left_char = char_number - fill_time * fill_char;

	for(i = 0, position = 0; i < fill_time; i++) {
		data = mtfs_random() & fill_max;
		for(j = 0; j < fill_char; j++) {
			block->data[position] = data;
			data >>= 8;
			position++;
		}
	}

	for(; position < char_number; position ++) {
		block->data[position] = mtfs_random() & 0xFF;
	}
	return 0;
}

int dump_block(block_t *block)
{
	int i = 0;

	HPRINT("block_size: %lu\n", block->block_size);
	for(i = 0; i < block->block_size; i++) {
		HPRINT("%02X", (uint8_t)(block->data[i]));
	}
	HPRINT("\n");
	return 0;
}

int cmp_block(block_t *s1, block_t *s2)
{
	HASSERT(s1->block_size == s2->block_size);

	return memcmp(s1->data, s2->data, s1->block_size);
}

off_t random_get_offset()
{
	HASSERT(MAX_LSEEK_OFFSET >= 1);
	return mtfs_rand_range(0, MAX_LSEEK_OFFSET);
}

#define MAX_USLEEP_TIME 1000
void sleep_random()
{
	useconds_t useconds = mtfs_rand_range(1, MAX_USLEEP_TIME);
	usleep(useconds);
}

int begin = 0;
void *write_little_proc(thread_info_t *thread_info)
{
	size_t block_size = 1;
	size_t count = 0;
	block_t *block = NULL;
	off_t offset = 0;
	off_t writed = 0;
	int ret = 0;

	ret = prctl(PR_SET_NAME, thread_info->identifier, 0, 0, 0);
	if (ret) {
		HERROR("failed to set thead name\n");
		goto out;
	}	

	block = alloc_block(block_size);
	if (block == NULL) {
		HERROR("Not enough memory\n");
		goto out;
	}

	random_fill_block(block);
	while (1) {
		pthread_rwlock_rdlock(&rwlock);
		{
			//pthread_mutex_lock(&mutex);
			//{
			//	pthread_cond_wait(&cond, &mutex);
				count = block_size;
				offset = random_get_offset();
				HPRINT("writing %ld at %ld\n", count, offset);
				writed = pwrite(fd, block->data, count, offset);
				if (writed != count) {
					HERROR("failed to write, "
					       "expected %ld, got %ld\n", 
					       count, writed);
					exit(0);
				}
				HPRINT("writed %ld at %ld\n", writed, offset);
			//}
			//pthread_mutex_unlock(&mutex);
		}
		pthread_rwlock_unlock(&rwlock);
		sleep_random();
	}

	free_block(block);	
out:
	return NULL;
}

void *write_proc(thread_info_t *thread_info)
{
	size_t block_size = MAX_BUFF_LENGTH;
	size_t count = 0;
	block_t *block = NULL;
	off_t offset = 0;
	off_t writed = 0;
	int ret = 0;

	ret = prctl(PR_SET_NAME, thread_info->identifier, 0, 0, 0);
	if (ret) {
		HERROR("failed to set thead name\n");
		goto out;
	}	

	block = alloc_block(block_size);
	if (block == NULL) {
		HERROR("Not enough memory\n");
		goto out;
	}

	random_fill_block(block);
	while (1) {
		pthread_rwlock_rdlock(&rwlock);
		{
			pthread_cond_broadcast(&cond);
			count = block_size;
			offset = 0;
			HPRINT("writing %ld at %ld\n", count, offset);
			writed = pwrite(fd, block->data, count, offset);
			if (writed != count) {
				HERROR("failed to write, "
				       "expected %ld, got %ld\n", 
				       count, writed);
				exit(0);
			}
			HPRINT("writed %ld at %ld\n", writed, offset);
			begin = 0;
		}
		pthread_rwlock_unlock(&rwlock);
		sleep_random();
	}

	free_block(block);	
out:
	return NULL;
}

void *check_proc(thread_info_t *thread_info)
{
	char *buf = NULL;
	size_t count = MAX_BUFF_LENGTH;
	off_t read_count = 0;
	off_t first_read_count = 0;
	char *first_buf = NULL;
	int done = 0;
	struct stat *stat_buf = NULL;
	struct stat *first_stat_buf = NULL;
	int ret = 0;
	int i = 0;

	ret = prctl(PR_SET_NAME, thread_info->identifier, 0, 0, 0);
	if (ret) {
		HERROR("failed to set thead name\n");
		goto out;
	}	

	HRFS_ALLOC(buf, count);
	if (buf == NULL) {
		HERROR("not enough memory\n");
		goto out;
	}

	HRFS_ALLOC(first_buf, count);
	if (first_buf == NULL) {
		HERROR("not enough memory\n");
		goto out;
	}

	HRFS_ALLOC_PTR(stat_buf);
	if (stat_buf == NULL) {
		HERROR("not enough memory\n");
		goto out;
	}

	HRFS_ALLOC_PTR(first_stat_buf);
	if (first_stat_buf == NULL) {
		HERROR("not enough memory\n");
		goto out;
	}

	while (1) {
		pthread_rwlock_wrlock(&rwlock);
		{
			HPRINT("checking\n");
			ret = stat(branch_path[0], first_stat_buf);
			if (ret) {
				HERROR("failed to stat\n");
				goto out;
			}

			ret = lseek(branch_fd[0], 0, SEEK_SET);
			if (ret) {
				HERROR("failed to lseek\n");
				goto out;
			}

			for (i = 1; i < bnum; i++) {
				ret = stat(branch_path[i], stat_buf);
				if (ret) {
					HERROR("failed to stat\n");
					goto out;
				}

				if (stat_buf->st_size != first_stat_buf->st_size) {
					HERROR("size differ\n");
					goto out;
				}

				ret = lseek(branch_fd[i], 0, SEEK_SET);
				if (ret) {
					HERROR("failed to lseek\n");
					goto out;
				}
			}



			done = 0;
			do {
				first_read_count = read(branch_fd[0], first_buf, count);
				if (ret < 0) {
					HERROR("failed to read\n");
					goto out;
				}

				for (i = 1; i < bnum; i++) {
					read_count = read(branch_fd[i], buf, count);
					if (read_count < 0) {
						HERROR("failed to read\n");
						goto out;
					}

					if (read_count != first_read_count) {
						HERROR("read count differ\n");
						goto out;
					}

					//if (memcmp(first_buf, buf, read_count) != 0) {
					//	HERROR("buff differ\n");
					//	goto out;
					//}
				}
			} while (first_read_count != 0);
			HPRINT("checked\n");
		}
		pthread_rwlock_unlock(&rwlock);
		sleep_random();
	}
out:
	if (first_buf != NULL) {
		HRFS_FREE(first_buf, count);
	}
	if (buf != NULL) {
		HRFS_FREE(buf, count);
	}
	if (stat_buf != NULL) {
		HRFS_FREE_PTR(stat_buf);
	}	
	if (first_stat_buf != NULL) {
		HRFS_FREE_PTR(first_stat_buf);
	}
	exit(1);
	return NULL;
}

const thread_group_t thread_groups[] = {
	{4, "write_little", (void *(*)(thread_info_t *))write_little_proc},
	{1, "write", (void *(*)(thread_info_t *))write_proc},
	{1, "check", (void *(*)(thread_info_t *))check_proc},
	{0, NULL, NULL}, /* for end detection */
};

void usage(const char *progname)
{
	fprintf(stderr, "usage: %s [-n loop_count] [-v] [-x] [file]\n"
		"\t-n: number of test loops (default 1000000)\n"
		"\t-b: block size when -r is not set (default 1048576)\n"
		"\t-r: use randmon block size\n"		
		"\t-x: don't exit on error\n", progname);
}

int main(int argc, char *argv[])
{
	int ret = 0;
	//int seconds = 100;
	int i = 0;

	if (argc != 4) {
		HERROR("argument error\n");
		ret = -EINVAL;
		goto out;
	}

	path = argv[1];
	fd = open(path, O_WRONLY);
	if (fd < 0) {
		HERROR("failed to open %s: %s\n", path, strerror(errno));
		ret = errno;
		goto out;
	}
	
	for (i = 0; i < 2; i++) {
		branch_path[i] = argv[i + 2];
		branch_fd[i] = open(branch_path[i], O_RDONLY);
		if (branch_fd[i] < 0) {
			HERROR("failed to open %s: %s\n", branch_path[i], strerror(errno));
			ret = errno;
			goto out;
		}
	}

	ret = pthread_mutex_init(&mutex, NULL);
	if (ret) {
		goto out;
	}

	ret = pthread_rwlock_init(&rwlock, NULL);
	if (ret) {
		goto out;
	}


	ret = pthread_cond_init(&cond, NULL);
	if (ret == -1) {
		goto out;
	}

/*
	ret = create_thread_group(&thread_groups[0], NULL);
	if (ret == -1) {
		goto out;
	}
*/

	ret = create_thread_group(&thread_groups[1], NULL);
	if (ret == -1) {
		goto out;
	}

/*
	ret = create_thread_group(&thread_groups[2], NULL);
	if (ret == -1) {
		goto out;
	}
*/

	while(1) {
	//for (i = 0; i < seconds / 10 + 1; i++) {
		sleep(10);
		HPRINT(".");
		HFLUSH();
	}
	pthread_rwlock_destroy(&rwlock);
	pthread_cond_destroy(&cond);
	pthread_mutex_destroy(&mutex);
out:
	return ret;
}
