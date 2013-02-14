/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <cmd_parser.h>
#include "libmtfsapi.h"
#include "mtfs_debug.h"

int mtfsctl_get_version(int argc, char **argv)
{
	printf("version: %s\n", MTFS_UTILS_VERSION);
	return 0;
}

static int mtfsctl_quit(int argc, char **argv)
{
	Parser_quit(argc, argv);
	return 0;
}

int mtfsctl_noop(int argc, char **argv)
{
	return 0;
}

static int mtfsctl_ignore_errors(int argc, char **argv)
{
	Parser_ignore_errors(1);
	return 0;
}

static int mtfsctl_debug_kernel(int argc, char **argv);
static int mtfsctl_debug_file(int argc, char **argv);

command_t mtfsctl_cmdlist[] = {
	/* Metacommands */
	{"===== metacommands =======", mtfsctl_noop, 0, "metacommands"},
	{"--ignore_errors", mtfsctl_ignore_errors, 0,
         "ignore errors that occur during script processing\n"
         "--ignore_errors"},

	/* User interface commands */
	{"======== control =========", mtfsctl_noop, 0, "control commands"},
	{"help", Parser_help, 0, "help"},
	{"version", mtfsctl_get_version, 0,
         "print the version of this test\n"
         "usage: version"},
	{"exit", mtfsctl_quit, 0, "quit"},
	{"quit", mtfsctl_quit, 0, "quit"},

	/* control commands */
	{"debug_kernel", mtfsctl_debug_kernel, 0,
	"get debug buffer and dump to a file\n"
	"usage: debug_kernel [file]"},
        {"debug_file", mtfsctl_debug_file, 0,
         "convert a binary debug file dumped by the kernel to ASCII text\n"
         "usage: debug_file <input> [output]"},
	{ 0, 0, 0, NULL }
};

int mtfsctl_debug_file(int argc, char **argv)
{
	int    fdin;
	int    fdout;
	int    rc;

	if (argc > 3 || argc < 2) {
		fprintf(stderr, "usage: %s <input> [output]\n", argv[0]);
		return 0;
	}

	fdin = open(argv[1], O_RDONLY | O_LARGEFILE);
	if (fdin < 0) {
		fprintf(stderr, "open(%s) failed: %s\n", argv[1],
			strerror(errno));
		return 1;
	}
	if (argc > 2) {
		fdout = open(argv[2],
		             O_CREAT | O_TRUNC | O_WRONLY | O_LARGEFILE,
		             0600);
		if (fdout < 0) {
			fprintf(stderr, "open(%s) failed: %s\n", argv[2],
			        strerror(errno));
			close(fdin);
			return 1;
		}
	} else {
		fdout = fileno(stdout);
	}

	rc = parse_buffer(fdin, fdout);

	close(fdin);
	if (fdout != fileno(stdout))
		close(fdout);

	return rc;
}

int mtfsctl_debug_kernel(int argc, char **argv)
{
	struct option long_opts[] = {
		{"quiet", no_argument, 0, 'q'},
		{"verbose", no_argument, 0, 'v'},
		{0, 0, 0, 0}
	};
	char short_opts[] = "qv";
	int c = 0;
	int rc = 0;
	struct mtfs_param param = { 0 };
	const char *filename = NULL;
	
	optind = 0;
	while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
		switch (c) {
		case 'q':
 			param.quiet++;
			param.verbose = 0;
			break;
		case 'v':
			param.verbose++;
			param.quiet = 0;
			break;
		case '?':
			rc = CMD_HELP;
			goto out;
		default:
			fprintf(stderr, "error: %s: option '%s' unrecognized\n",
			        argv[0], argv[optind - 1]);
			rc = CMD_HELP;
			goto out;
		}
	}

	if (optind == argc - 1) {
		filename = argv[optind];
	} else if(optind == argc) {
		/* No filename */
		filename = NULL;
	} else {
		rc = CMD_HELP;
		goto out;
	}

	rc = mtfsctl_api_debug_kernel(filename, &param);
out:
	return rc;
}

int main(int argc, char **argv)
{
	int rc;
	setlinebuf(stdout); //?
	Parser_init("mtfsctl > ", mtfsctl_cmdlist);
	if (argc > 1) {
		rc = Parser_execarg(argc - 1, argv + 1, mtfsctl_cmdlist);
	} else {
		rc = Parser_commands();
	}

	Parser_exit(argc, argv);
	return rc;
}
