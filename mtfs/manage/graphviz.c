/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <debug.h>

int graphviz_fd;
#define GRAPHVIZ_LOG_FILENAME "/var/log/hfsm.dot"

int graphviz_init()
{
	graphviz_fd = open(GRAPHVIZ_LOG_FILENAME, O_WRONLY | O_TRUNC | O_CREAT, 0666);
	return graphviz_fd;
}

void graphviz_finit()
{
	close(graphviz_fd);
}

void graphviz_print(const char *msg)
{
	size_t size = strlen(msg);
	size_t ret = 0; 

	printf(msg);
	ret = write(graphviz_fd, msg, size);
	if (ret != size) {
		MERROR("write error: expected to write %ld, written %ld\n", size, ret);
	}
	fsync(graphviz_fd);
}
