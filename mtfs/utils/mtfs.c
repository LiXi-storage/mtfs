/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cmd_parser.h>
#include <raid.h>
#include "libmtfsapi.h"
#include "hide.h"
#include "grouplock.h"
#include "dump_trace.h"

int mtfs_get_version(int argc, char **argv)
{
	printf("version: %s\n", MTFS_UTILS_VERSION);
	return 0;
}

static int mtfs_quit(int argc, char **argv)
{
	Parser_quit(argc, argv);
	return 0;
}

int mtfs_noop(int argc, char **argv)
{
	return 0;
}

static int mtfs_ignore_errors(int argc, char **argv)
{
	Parser_ignore_errors(1);
	return 0;
}

static int mtfs_example(int argc, char **argv);
static int mtfs_getstate(int argc, char **argv);
static int mtfs_setbranch(int argc, char **argv);
static int mtfs_setraid(int argc, char **argv);
static int mtfs_hide(int argc, char **argv);
static int mtfs_grouplock(int argc, char **argv);
static int mtfs_rmbranch(int argc, char **argv);
static int mtfs_dumptrace(int argc, char **argv);

command_t mtfs_cmdlist[] = {
	/* Metacommands */
	{"--ignore_errors", mtfs_ignore_errors, 0,
         "ignore errors that occur during script processing\n"
         "--ignore_errors"},

	/* User interface commands */
	{"help", Parser_help, 0, "help"},
	{"version", mtfs_get_version, 0,
         "print the version of this test\n"
         "usage: version"},
	{"exit", mtfs_quit, 0, "quit"},
	{"quit", mtfs_quit, 0, "quit"},
	{"example", mtfs_example, 0, "just an example\n"
	"usage: example [--quiet | -q] [--verbose | -v]\n"
	"               [--recursive | -r] <dir|file> ...\n"},

	/* ioctl commands */
	{"getstate", mtfs_getstate, 0,
	"To list the state info for a given file or directory.\n"
	"usage: getstate [--quiet | -q] [--verbose | -v]\n"
	"                <dir|file> ...\n"},
	{"setbranch", mtfs_setbranch, 0,
	"To set the state info for a branch of a given file or directory.\n"
	"usage: setbranch [--quiet | -q] [--verbose | -v]\n"
	"                [--branch | -b bindex] [--data | -d flag]"
	"                [--attr | -a flag] [--xattr | -x flag]"
	"                <dir|file> ...\n"},
	{"rmbranch", mtfs_rmbranch, 0,
	"To set the state info for a branch of a given file or directory.\n"
	"usage: setbranch [--quiet | -q] [--verbose | -v]\n"
	"                [--branch | -b bindex]"
	"                <dir|file> ...\n"},
	{"setraid", mtfs_setraid, 0,
	"To set a raid level for a given file or directory.\n"
	"usage: setraid [--quiet | -q] [--verbose | -v]\n"
	"                [--raid | -r raid_type] <dir|file> ...\n"},

	/* control commands */
	{"hide", mtfs_hide, 0,
	"To list the state info for a given file or directory.\n"
	"usage: hide [--quiet | -q] [--verbose | -v] [--unhide | -u]\n"
	"                <dir> ...\n"},

	{"dumptrace", mtfs_dumptrace, 0,
	"To dump the trace info for trace file system.\n"
	"usage: dumptrace [--quiet | -q] [--verbose | -v]\n"
	"                <dir> ...\n"},

	/* repair commands */

	/* test commands */
	{"grouplock", mtfs_grouplock, 0,
	"To test mtfs group lock.\n"
	"usage: grouplock [--quiet | -q] [--verbose | -v] \n"
	"          [--groupid | -g groupid] <dir|file>\n"},	
	{ 0, 0, 0, NULL }
};

static int mtfs_getstate(int argc, char **argv)
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
	
	if (optind >= argc) {
		rc = CMD_HELP;
		goto out;
	}

	do {
		rc = mtfs_api_getstate(argv[optind], &param);
	} while (++optind < argc && !rc);

	if (rc) {
		fprintf(stderr, "error: %s failed for %s, ret = %d.\n",
		        argv[0], argv[optind - 1], rc);
	}
out:
	return rc;
}

static int mtfs_setraid(int argc, char **argv)
{
	struct option long_opts[] = {
		{"quiet", no_argument, 0, 'q'},
		{"verbose", no_argument, 0, 'v'},
		{"raid", required_argument, 0, 'r'},
		{0, 0, 0, 0}
	};
	char short_opts[] = "qvr:";
	int c = 0;
	int rc = 0;
	struct mtfs_param param = { 0 };
	raid_type_t raid_type = 0;
	char *raid_type_arg = NULL;
	char *end = NULL;
	
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
		case 'r':
			raid_type_arg = optarg;
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
	
	if (optind >= argc) {
		rc = CMD_HELP;
		goto out;
	}

	/* Get the raid pattern */
	if (raid_type_arg == NULL) {
		rc = CMD_HELP;
		goto out;
	}
	
	raid_type = strtoul(raid_type_arg, &end, 0);
	if (*end != '\0' || (!raid_type_is_valid(raid_type))) {
		fprintf(stderr, "error: %s: bad raid pattern '%s'\n",
  	        argv[0], raid_type_arg);
		rc = CMD_HELP;
		goto out;
	}

	do {
		rc = mtfs_api_setraid(argv[optind], raid_type, &param);
		//mtfs_api_getstate(argv[optind], &param);
	} while (++optind < argc && !rc);


	if (rc) {
		fprintf(stderr, "error: %s failed for %s, ret = %d.\n",
		        argv[0], argv[optind - 1], rc);
	}
out:
	return rc;
}

/*
 * setbranch -b bindex -d 1 -x 1 -a 1 /path/to/mtfs/file
*/
static int mtfs_setbranch(int argc, char **argv)
{
	struct option long_opts[] = {
		{"quiet", no_argument, 0, 'q'},
		{"verbose", no_argument, 0, 'v'},
		{"branch", no_argument, 0, 'b'},
		{"data", no_argument, 0, 'd'},
		{"attr", no_argument, 0, 'a'},
		{"xattr", no_argument, 0, 'x'},
		{0, 0, 0, 0}
	};
	char short_opts[] = "qvb:d:a:x:";
	int c = 0;
	int rc = 0;
	struct mtfs_param param = { 0 };
	char *end = NULL;
	char *branch_arg = NULL;
	char *data_arg = NULL;
	char *attr_arg = NULL;
	char *xattr_arg = NULL;
	struct mtfs_branch_valid valid = {MTFS_BSTATE_UNSETTED, MTFS_BSTATE_UNSETTED, MTFS_BSTATE_UNSETTED};
	int is_valid = MTFS_BSTATE_UNSETTED;
	mtfs_bindex_t bindex = 0;

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
		case 'b':
			branch_arg = optarg;
			break;
		case 'd':
			data_arg = optarg;
			break;
		case 'a':
			attr_arg = optarg;
			break;
		case 'x':
			xattr_arg = optarg;
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
	
	if (optind >= argc) {
		rc = CMD_HELP;
		goto out;
	}

	if (branch_arg == NULL) {
		rc = CMD_HELP;
		goto out;
	}

	bindex = strtoul(branch_arg, &end, 0);
	if (bindex < 0 ||
	    *end != '\0') {
		fprintf(stderr, "error: %s: bad branch index '%s'\n",
		        argv[0], branch_arg);
		        rc = CMD_HELP;
		        goto out;
	}

	if (data_arg != NULL) {
		is_valid = strtoul(data_arg, &end, 0);
		if ((is_valid != 0 && is_valid != 1) ||
		    *end != '\0') {
			fprintf(stderr, "error: %s: bad data valid flag '%s'\n",
			        argv[0], data_arg);
			        rc = CMD_HELP;
			        goto out;
		}
		valid.data_valid = is_valid;
	}

	if (attr_arg != NULL) {
		is_valid = strtoul(attr_arg, &end, 0);
		if ((is_valid != 0 && is_valid != 1) ||
		    *end != '\0') {
			fprintf(stderr, "error: %s: bad attr valid flag '%s'\n",
			        argv[0], attr_arg);
			        rc = CMD_HELP;
			        goto out;
		}
		valid.attr_valid = is_valid;
	}

	if (xattr_arg != NULL) {
		is_valid = strtoul(xattr_arg, &end, 0);
		if ((is_valid != 0 && is_valid != 1) ||
		    *end != '\0') {
			fprintf(stderr, "error: %s: bad xattr valid flag '%s'\n",
			        argv[0], xattr_arg);
			        rc = CMD_HELP;
			        goto out;
		}
		valid.xattr_valid = is_valid;
	}

	do {
		rc = mtfs_api_setbranch(argv[optind], bindex, &valid, &param);
	} while (++optind < argc && !rc);

	if (rc) {
		fprintf(stderr, "error: %s failed for %s, ret = %d.\n",
		        argv[0], argv[optind - 1], rc);
	}
out:
	return rc;
}

/*
 * rmbranch -b bindex /path/to/mtfs/file
*/
static int mtfs_rmbranch(int argc, char **argv)
{
	struct option long_opts[] = {
		{"quiet", no_argument, 0, 'q'},
		{"verbose", no_argument, 0, 'v'},
		{"branch", no_argument, 0, 'b'},
		{0, 0, 0, 0}
	};
	char short_opts[] = "qvb:d:a:x:";
	int c = 0;
	int rc = 0;
	struct mtfs_param param = { 0 };
	char *end = NULL;
	char *branch_arg = NULL;
	mtfs_bindex_t bindex = 0;

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
		case 'b':
			branch_arg = optarg;
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
	
	if (optind >= argc) {
		rc = CMD_HELP;
		goto out;
	}

	if (branch_arg == NULL) {
		rc = CMD_HELP;
		goto out;
	}

	bindex = strtoul(branch_arg, &end, 0);
	if (bindex < 0 ||
	    *end != '\0') {
		fprintf(stderr, "error: %s: bad branch index '%s'\n",
		        argv[0], branch_arg);
		        rc = CMD_HELP;
		        goto out;
	}

	do {
		rc = mtfs_api_rmbranch(argv[optind], bindex, &param);
	} while (++optind < argc && !rc);

	if (rc) {
		fprintf(stderr, "error: %s failed for %s, ret = %d.\n",
		        argv[0], argv[optind - 1], rc);
	}
out:
	return rc;
}

static int mtfs_example(int argc, char **argv)
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
	
	if (optind >= argc) {
		rc = CMD_HELP;
		goto out;
	}
	
	do {
		printf("arg[%d] = %s\n", optind, argv[optind]);
		rc = 0;
	} while (++optind < argc && !rc);

	if (rc) {
		fprintf(stderr, "error: %s failed for %s, rc = %d.\n",
		        argv[0], argv[optind - 1], rc);
	}
out:
	return rc;
}

static int mtfs_hide(int argc, char **argv)
{
	struct option long_opts[] = {
		{"quiet", no_argument, 0, 'q'},
		{"verbose", no_argument, 0, 'v'},
		{"unhide", no_argument, 0, 'u'},
		{0, 0, 0, 0}
	};
	char short_opts[] = "qvu";
	int c = 0;
	int rc = 0;
	struct mtfs_param param = { 0 };
	int unhide = 0;

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
		case 'u':
			unhide++;
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
	
	if (optind >= argc) {
		rc = CMD_HELP;
		goto out;
	}

	do {
		rc = mtfs_api_hide(argv[optind], unhide, &param);
	} while (++optind < argc && !rc);

	if (rc) {
		fprintf(stderr, "error: %s failed for %s, rc = %d.\n",
		        argv[0], argv[optind - 1], rc);
	}
out:
	return rc;
}

static int mtfs_dumptrace(int argc, char **argv)
{
	struct option long_opts[] = {
		{"quiet", no_argument, 0, 'q'},
		{"verbose", no_argument, 0, 'v'},
		{"unhide", no_argument, 0, 'u'},
		{0, 0, 0, 0}
	};
	char short_opts[] = "qvu";
	int c = 0;
	int rc = 0;
	struct mtfs_param param = { 0 };

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
	
	if (optind >= argc) {
		rc = CMD_HELP;
		goto out;
	}

	do {
		rc = mtfs_api_dumptrace(argv[optind], &param);
	} while (++optind < argc && !rc);

	if (rc) {
		fprintf(stderr, "error: %s failed for %s, rc = %d.\n",
		        argv[0], argv[optind - 1], rc);
	}
out:
	return rc;
}

#define DEFAULT_GROUP_ID 777
int stop = 0;
void handler(int signal)
{
	static long last_time;
	long now = time(0);

	printf("looping...(two continuous Ctrl+c to quit)\n");
	if (signal == SIGQUIT) {
		stop = 1;
	} else if (signal == SIGINT) {
		if (now - last_time < 2) {
			stop = 1;
		}
		last_time = now;
	}
	alarm(60);
}

struct grouplock_file {
	int fd;
	int locked;
};
static int mtfs_grouplock(int argc, char **argv)
{
	struct option long_opts[] = {
		{"quiet", no_argument, 0, 'q'},
		{"verbose", no_argument, 0, 'v'},
		{"groupid", required_argument, 0, 'g'},
		{"loop", no_argument, 0, 'l'},
		{0, 0, 0, 0}
	};
	char short_opts[] = "qvg:l";
	int c = 0;
	int rc = 0;
	struct mtfs_param param = { 0 };
	int group_id = DEFAULT_GROUP_ID;
	char *endptr = NULL;
	struct grouplock_file *file;
	int loop = 0;
	int file_number = 0;
	int i = 0;

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
		case 'g':
			group_id = strtol(optarg, &endptr, 10);
			if (optarg == endptr) {
				fprintf(stderr, "Bad group id: %s\n", optarg);
				rc = CMD_HELP;
				goto out;
			}
			break;
		case 'l':
			loop++;
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

	file_number = argc - optind;
	if (file_number <= 0) {
		rc = CMD_HELP;
		goto out;
	}

	file = calloc(file_number, sizeof(*file));
	if (file == NULL) {
		fprintf(stderr, "error: Not enough memory\n");
		rc = -1;
		goto out;
	}

	for(i = 0; i < file_number; i++) {
		char *filename = argv[optind + i];

		file[i].fd = open(filename, 0, O_NONBLOCK, &param);
		if (file[i].fd == -1) {
			rc = -1;
			goto undo;
		}
		
		rc = mtfs_api_group_lock(filename, file[i].fd, group_id, &param);
		if (rc) {
			goto undo;
		} else {
			file[i].locked = 1;
		}
	}

	if (loop) {
		signal(SIGQUIT, handler);
		signal(SIGINT, handler);
		signal(SIGALRM, handler);
		signal(SIGUSR1, handler);
		alarm(60);

		printf("looping...(two continuous Ctrl+c to quit)\n");
		while(!stop) {
			sleep(10);
		}
	}

undo:
	for(i = 0; i < file_number; i++) {
		char *filename = argv[optind + i];

		if (file[i].fd > 0) {
			if (file[i].locked) {
				mtfs_api_group_unlock(filename, file[i].fd, group_id, &param);
			}
			close(file[i].fd);
		}
	}
	free(file);
out:
	if (rc) {
		fprintf(stderr, "error: %s failed for %s, rc = %d.\n",
		        argv[0], argv[optind - 1], rc);
	}
	return rc;
}

int main(int argc, char **argv)
{
	int rc;
	setlinebuf(stdout); //?
	Parser_init("mtfs > ", mtfs_cmdlist);
	if (argc > 1) {
		rc = Parser_execarg(argc - 1, argv + 1, mtfs_cmdlist);
	} else {
		rc = Parser_commands();
	}

	Parser_exit(argc, argv);
	return rc;
}
