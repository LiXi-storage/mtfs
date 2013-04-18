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

	address = mtfs_kallsyms_lookup_name("xxx");
	printf("address: %lx", address);
	return ret;
}
