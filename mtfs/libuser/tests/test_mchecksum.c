/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <debug.h>
#include <stdlib.h>
#include <string.h>
#include <mtfs_checksum.h>

#define MAX_MOUNT_OPTION_LENGTH 1024
#define AGAIN_SLOT "AGAIN"
#define AGAIN_SLOT_LEN strlen(AGAIN_SLOT)
int main()
{
	__u32 checksum1 = 0;
	__u32 checksum2 = 0;
	char input[MAX_MOUNT_OPTION_LENGTH];
	int ret = 0;
	mchecksum_type_t type = mchecksum_type_select();
	int number = 0;
	int again = 0;

	checksum1 = mchecksum_init(type);
	checksum2 = mchecksum_init(type);
	while (1) {
		ret = fscanf(stdin, "%s", input);
		if (ret == EOF) {
			if (!again) {
				fprintf(stderr ,"invalid input\n");
				ret = -EINVAL;
				goto out;
			} else {
				break;
			}
		} else if (strlen(input) == AGAIN_SLOT_LEN &&
			   strcmp(input, AGAIN_SLOT) == 0) {
			again = 1;
			continue;
		} else {
			if (!again) {
				checksum1 = mchecksum_compute(checksum1, input,
				                  strlen(input), type);
			} else {
				checksum2 = mchecksum_compute(checksum2, input,
				                  strlen(input), type);
			}
		}
	}

	if (checksum1 == checksum2) {
		printf("equal\n");
	} else {
		printf("differ\n");
	}
	ret = 0;
out:
	return ret;
}
