/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <debug.h>
#include <memory.h>
#include <bitmap.h>
#include <mtfs_random.h>
#include <mtfs_lowerfs.h>
#define MAX_INTERVAL_NUMBER 1024

typedef enum operation_type {
	OPERATION_TYPE_ADD = 0,
	OPERATION_TYPE_CHECK,
	OPERATION_TYPE_NUM,
} operation_type_t;

int random_test(struct mlowerfs_bucket *bucket, mtfs_bitmap_t *bitmap, int limit)
{
	int ret = 0;
	operation_type_t operation = 0;
	struct mtfs_interval_node_extent extent;
	int i = 0;
	int interval_limit = limit;

	MASSERT(bucket);
	operation = random() % OPERATION_TYPE_NUM;
	extent.start = random() % MAX_INTERVAL_NUMBER;
	if (interval_limit > MAX_INTERVAL_NUMBER - extent.start) {
		interval_limit = MAX_INTERVAL_NUMBER - extent.start;
	}
	extent.end = extent.start + random() % interval_limit;
	MASSERT(operation >= OPERATION_TYPE_ADD && operation < OPERATION_TYPE_NUM);
	MASSERT(extent.start >= 0 && extent.start <= extent.end);
	MASSERT(extent.end < MAX_INTERVAL_NUMBER);

	switch(operation) {
	case OPERATION_TYPE_ADD:
		_mlowerfs_bucket_add(bucket, &extent);
		for (i = extent.start; i <= extent.end; i++) {
			mtfs_bitmap_set(bitmap, i);
		}
		break;
	case OPERATION_TYPE_CHECK:
		mtfs_foreach_bit(bitmap, i) {
			if (!_mlowerfs_bucket_check(bucket, (__u64)i)) {
				_mlowerfs_bucket_dump(bucket);
				ret = -1;
				break;
			}
		}
		_mlowerfs_bucket_is_valid(bucket);
		break;
	default:
		MERROR("Unexpected operation: %d\n", operation);
		ret = -1;
		break;
	}

	return ret;
}

int main()
{
	int ret = 0;
	struct mlowerfs_bucket *bucket = NULL;
	int i = 0;
	int j = 0;
	int k = 0;
	//int operation_num = MAX_INTERVAL_NUMBER * 100; 4
	int operation_num = MAX_INTERVAL_NUMBER * 2;
	struct mtfs_interval_node_extent extent;
	int iter_num = 100;
	int interval_num = MAX_INTERVAL_NUMBER / 64;
	mtfs_bitmap_t *bitmap = NULL;

	MTFS_ALLOC(bucket, sizeof(*bucket));
	if (bucket == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	bitmap = mtfs_bitmap_allocate(MAX_INTERVAL_NUMBER);
	if (bitmap == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out_free_bucket;
	}

	mtfs_random_init(0);
	for (i = 0; i < iter_num; i++) {
		for(j = 1; j <= interval_num; j++) {
			memset(bucket, 0, sizeof(*bucket));
			//printf("interval: %d\n", j);
			for (k = 0; k < operation_num; k++) {
				ret = random_test(bucket, bitmap, j);
				if (ret) {
					goto out_free_bitmap;
				}
			}
			//_mlowerfs_bucket_dump(bucket);
		}
	}
out_free_bitmap:
	mtfs_bitmap_freee(bitmap);
out_free_bucket:
	MTFS_FREE(bucket, sizeof(*bucket));
out:
	return ret;
}
