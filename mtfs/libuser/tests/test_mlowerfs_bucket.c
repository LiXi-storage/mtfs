/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <debug.h>
#include <memory.h>
#include <bitmap.h>
#include <mtfs_lowerfs.h>

int main()
{
	int ret = 0;
	struct mlowerfs_bucket *bucket = NULL;
	int i = 0;
	int operation_num = 0;
	struct mtfs_interval_node_extent extent;

	MTFS_ALLOC(bucket, sizeof(*bucket));
	if (bucket == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	fscanf(stdin, "%d", &operation_num);
	for (i = 0; i < operation_num; i ++) {
		fscanf(stdin, "%llu%llu", &extent.start, &extent.end);
		_mlowerfs_bucket_add(bucket, &extent);
	}
	_mlowerfs_bucket_dump(bucket);
out_free_bucket:
	MTFS_FREE(bucket, sizeof(*bucket));
out:
	return ret;
}
