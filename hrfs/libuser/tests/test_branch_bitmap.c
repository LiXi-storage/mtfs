/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#include <debug.h>
#include <memory.h>
#include <bitmap.h>
#include <hrfs_random.h>
#define MAX_BRANCH_NUMBER 128

typedef enum operation_type {
	OPERATION_TYPE_SET = 0,
	OPERATION_TYPE_CLEAR,
	OPERATION_TYPE_CHECK,
	OPERATION_TYPE_NUM,
} operation_type_t;

int random_test(bitmap_t *bitmap, int *array)
{
	int ret = 0;
	operation_type_t operation = 0;
	int nbit = 0;
	int output = 0;
	int expect = 0;
	
	HASSERT(bitmap);
	HASSERT(array);
	
	operation = random() % OPERATION_TYPE_NUM;
	nbit = random() % MAX_BRANCH_NUMBER;
	HASSERT(operation >= OPERATION_TYPE_SET && operation < OPERATION_TYPE_NUM);
	HASSERT(nbit >= 0 && nbit < MAX_BRANCH_NUMBER);
	switch(operation) {
	case OPERATION_TYPE_SET:
		cfs_bitmap_set(bitmap, nbit);
		array[nbit] = 1;
		break;
	case OPERATION_TYPE_CLEAR:
		cfs_bitmap_clear(bitmap, nbit);
		array[nbit] = 0;
		break;
	case OPERATION_TYPE_CHECK:
		//fprintf(stderr, "%d ", nbit);
		output = cfs_bitmap_check(bitmap, nbit);
		HASSERT(nbit >= 0 && nbit < MAX_BRANCH_NUMBER);
		expect = array[nbit];
		if (output != expect) {
			HERROR("BUG found: expect %d, got %d\n", expect, output);
			ret = 1;
		}
		break;
	default:
		HERROR("Unexpected operation: %d\n", operation);
		ret = -1;
		break;
	}

	return ret;
}

int main()
{
	int ret = 0;
	bitmap_t *bitmap = NULL;
	int *array = NULL;
	int i = 0;
	
	bitmap = cfs_bitmap_allocate(MAX_BRANCH_NUMBER);
	if (bitmap == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	
	HRFS_ALLOC(array, MAX_BRANCH_NUMBER * sizeof(*array));
	if (array == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	/* HRFS_ALLOC will do this for us */
	//memset(array, 0, MAX_BRANCH_NUMBER * sizeof(*array));

	hrfs_random_init(0);
	for(i = 0; i < 1000000; i++) {
		ret = random_test(bitmap, array);
		if (ret) {
			goto error;
		}
	}

	goto out;
error:
	HPRINT("Seted bits: ");
	cfs_bitmap_dump(bitmap);
out:
	if (bitmap) {
		cfs_bitmap_freee(bitmap);
	}
	if (array) {
		HRFS_FREE(array, MAX_BRANCH_NUMBER * sizeof(*array));
	}
	return ret;
}
