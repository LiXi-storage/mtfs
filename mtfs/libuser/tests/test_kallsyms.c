/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <debug.h>
#include <stdlib.h>
#include <string.h>
#include <compat.h>

int main()
{
	int ret = 0;
	unsigned long address = 0;
 	ret = mtfs_symbol_get("ext3", "ext3_bread", &address);
	if (ret) {
		printf("not found\n");
	} else {
		printf("address: %lx\n", address);
	}
	return ret;
}
