/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#include <parse_option.h>
#include <stdio.h>
#define MAX_MOUNT_OPTION_LENGTH 1024
int main()
{
	int ret = 0;
	char input[MAX_MOUNT_OPTION_LENGTH];
	mount_option_t mount_option;
	
	fscanf(stdin, "%s", input);
	ret = hrfs_parse_options(input, &mount_option);
	if (!ret) {
		mount_option_dump(&mount_option);
		mount_option_finit(&mount_option);
	}
	return ret;
}
