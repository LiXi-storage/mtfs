/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * FROM: swgfs/tests
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

int main(int argc, char *argv[])
{
	int rc;

	if (argc != 3) {
		fprintf(stderr, "usage: %s from to\n", argv[0]);
		exit(1);
	}

	rc = rename(argv[1], argv[2]);
	printf("rename returned %d: %s\n", rc, strerror(errno));

	return rc;
}
