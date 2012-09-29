/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <parse_option.h>
#include <stdio.h>
#define MAX_MOUNT_OPTION_LENGTH 1024
int main()
{
	int ret = 0;
	char input[MAX_MOUNT_OPTION_LENGTH];
	struct mount_option mount_option;
	
	fscanf(stdin, "%s", input);
	ret = mtfs_parse_options(input, &mount_option);
	if (!ret) {
		mount_option_dump(&mount_option);
		mount_option_fini(&mount_option);
	}
	return ret;
}
