/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <memory.h>
#include <mtfs_random.h>
#include <debug.h>
#include <inttypes.h>

typedef struct block {
	char *data;
	size_t block_size;
} block_t;

#define MAX_BLOCK_SIZE   1024
#define MAX_LSEEK_OFFSET 1048576

static inline block_t *alloc_block(int block_size)
{
	block_t *block = NULL;
	
	MASSERT(block_size > 0);
	
	MTFS_ALLOC_PTR(block);
	if (block == NULL) {
		goto out;
	}
	
	MTFS_ALLOC(block->data, block_size);
	if (block->data == NULL) {
		goto free_block;
	}

	block->block_size = block_size;
	goto out;

free_block:
	MTFS_FREE_PTR(block);
out:
	return block;
}

static inline int free_block(block_t *block)
{
	MTFS_FREE(block->data, block->block_size);
	MTFS_FREE_PTR(block);
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
		//MASSERT(fill_char);
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
	
	MPRINT("block_size: %zd\n", block->block_size);
	for(i = 0; i < block->block_size; i++) {
		MPRINT("%02X", (uint8_t)(block->data[i]));
	}
	MPRINT("\n");
	return 0;
}

size_t random_get_lseek_offset()
{
	MASSERT(MAX_LSEEK_OFFSET >= 1);
	return mtfs_rand_range(0, MAX_LSEEK_OFFSET);
}

size_t random_get_count(size_t size)
{
	MASSERT(size >= 1);
	return mtfs_rand_range(1, size);
}

typedef struct rw_test_info {
	int fd;
	char *file_path;
} rw_test_info_t;


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void *rw_test_init(char *file_path)
{
	rw_test_info_t *rw_info = NULL;
	
	MTFS_ALLOC_PTR(rw_info);
	if (rw_info == NULL) {
		MERROR("Not enough memory\n");
		goto out;
	}
	
	rw_info->fd = open(file_path, O_CREAT | O_RDWR, 0600);
	if (rw_info->fd < 0) {
		MERROR("Failed to open file %s\n", file_path);
		goto free_info;
	}
	
	rw_info->file_path = strdup(file_path);
	goto out;
free_info:
	MTFS_FREE_PTR(rw_info);
out:
	return (void *)rw_info;
}

enum {
	OPERATION_TYPE_CHANGE = 0,
	OPERATION_TYPE_LSEEK,
	OPERATION_TYPE_WRITE,
	OPERATION_TYPE_READ,
	OPERATION_TYPE_TRUNCATE,
	OPERATION_TYPE_NUM,
};

int random_get_operation()
{
	return mtfs_rand_range(OPERATION_TYPE_CHANGE, OPERATION_TYPE_TRUNCATE);
}

#define TRUNCATE_LOOP 1
#define CHANGE_LOOP 1

int rw_test_run(block_t *block, rw_test_info_t *rw_info)
{
	int ret = 0;
	ssize_t size = 0;
	off_t offset = 0;
	ssize_t rw_size = 0;
	int operation = 0;
	static int truncate_loop = 0;
	static int change_loop = 0;

again:
	operation = random_get_operation();

	switch (operation)
	{
	case OPERATION_TYPE_CHANGE:
		if (change_loop < CHANGE_LOOP) {
			change_loop++;
			goto again;
		}
		change_loop = 0;
		random_fill_block(block);
		MERROR("change\n");
		goto again;
		break;
	case OPERATION_TYPE_LSEEK:
		offset = lseek(rw_info->fd, random_get_lseek_offset(), SEEK_SET);
		if (offset < 0) {
			MERROR("failed to lseek file %s: ret = %d\n",
			       rw_info->file_path, errno);
			ret = -1;
		}
		MERROR("lseek\n");
		break;
	case OPERATION_TYPE_WRITE:
		rw_size = random_get_count(block->block_size);
		size = write(rw_info->fd,
		             block->data,
		             rw_size);
		if (size != rw_size) {
			MERROR("failed to write file %s: writed %zd expect %zd\n",
			       rw_info->file_path, size, rw_size);
			ret = -1;
		}
		MERROR("write %zd\n", rw_size);
		break;
	case OPERATION_TYPE_READ:
		rw_size = random_get_count(block->block_size);
		size = read(rw_info->fd,
		            block->data,
		            rw_size);
		if (size < 0) {
			MDEBUG("failed to write file %s: read %zd expect %zd\n",
			       rw_info->file_path, size, rw_size);
		}
		MERROR("read %zd\n", rw_size);
		break;
	case OPERATION_TYPE_TRUNCATE:
		if (truncate_loop < TRUNCATE_LOOP) {
			truncate_loop++;
			goto again;
		}
		truncate_loop = 0;
		rw_size = random_get_lseek_offset();
		ret = ftruncate(rw_info->fd, rw_size);
		if (ret) {
			MERROR("failed to truncate file %s to %zd, ret = %d\n",
			       rw_info->file_path, rw_size, ret);
		}
		MERROR("truncate %zd\n", rw_size);
		break;
	default:
		MERROR("unable to recongnize operation type\n");
		ret = -1;
		break;
	}

	return ret;
}

int rw_test_finit(rw_test_info_t *rw_info)
{
	close(rw_info->fd);
	free(rw_info->file_path);
	MTFS_FREE_PTR(rw_info);

	return 0;
}

void usage(const char *progname)
{
	fprintf(stderr, "usage: %s [-n loop_count] [-v] [-x] [file]\n"
		"\t-n: number of test loops (default 1000000)\n"
		"\t-r: use randmon block size\n"		
		"\t-x: don't exit on error\n", progname);
}

int main(int argc, char *argv[])
{
	int ret = 0;
	size_t block_size = 1048576;
	int loop_count = 1000000;
	block_t *block = NULL;
	void *test_info = NULL;
	int i = 0;
	int c;
	char *end = NULL;

	while ((c = getopt(argc, argv, "b:n:vx")) != EOF) {
		switch(c) {
		case 'b':
			i = strtoul(optarg, &end, 0);
			if (i && end != NULL && *end == '\0') {
				block_size = i;
			} else {
				MERROR("bad block size '%s'\n", optarg);
				usage(argv[0]);
				return 1;
			}
			break;
		case 'n':
			i = strtoul(optarg, &end, 0);
			if (i && end != NULL && *end == '\0') {
				loop_count = i;
			} else {
				MERROR("bad loop count '%s'\n", optarg);
				usage(argv[0]);
				return 1;
			}
			break;
		default:
			usage(argv[0]);
			return 1;
		}
	}
	
	if (argc != optind + 1) {
		usage(argv[0]);
		return 1;
	}

	test_info = rw_test_init(argv[optind]);
	if (test_info == NULL) {
		MERROR("failed to init test\n");
		goto out;
	}	

	mtfs_random_init(0);

	block = alloc_block(MAX_BLOCK_SIZE);
	if (block == NULL) {
		MERROR("Not enough memory\n");
		goto finit_info;
	}
	random_fill_block(block);

	for (i = 0; i < loop_count; i++)
	{
		ret = rw_test_run(block, test_info);
		if (ret) {
			MERROR("Fail to run test %d\n", i);
			break;
		}
	}

	free_block(block);
finit_info:
	rw_test_finit(test_info);
out:
	return 0;
}
