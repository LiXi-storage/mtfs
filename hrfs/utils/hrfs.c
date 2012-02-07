/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#include <cmd_parser.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <raid.h>
#include <unistd.h>
#include "libhrfsapi.h"
#include "repair.h"
#include <log.h>
#include "cp.h"
#include "hide.h"
#include "grouplock.h"
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define HRFS_UTILS_VERSION "2.0.0"
int hrfs_get_version(int argc, char **argv)
{
	printf("version: %s\n", HRFS_UTILS_VERSION);
	return 0;
}

static int hrfs_quit(int argc, char **argv)
{
	Parser_quit(argc, argv);
	return 0;
}

int hrfs_noop(int argc, char **argv)
{
	return 0;
}

static int hrfs_ignore_errors(int argc, char **argv)
{
	Parser_ignore_errors(1);
	return 0;
}

static int hrfs_example(int argc, char **argv);
static int hrfs_getstate(int argc, char **argv);
static int hrfs_setbranch(int argc, char **argv);
static int hrfs_setraid(int argc, char **argv);
static int hrfs_repair(int argc, char **argv);
static int hrfs_diff(int argc, char **argv);
static int hrfs_log(int argc, char **argv);
static int hrfs_cp(int argc, char **argv);
static int hrfs_hide(int argc, char **argv);
static int hrfs_grouplock(int argc, char **argv);
static int hrfs_rmbranch(int argc, char **argv);

command_t hrfs_cmdlist[] = {
	/* Metacommands */
	{"--ignore_errors", hrfs_ignore_errors, 0,
         "ignore errors that occur during script processing\n"
         "--ignore_errors"},

	/* User interface commands */
	{"help", Parser_help, 0, "help"},
	{"version", hrfs_get_version, 0,
         "print the version of this test\n"
         "usage: version"},
	{"exit", hrfs_quit, 0, "quit"},
	{"quit", hrfs_quit, 0, "quit"},
	{"example", hrfs_example, 0, "just an example\n"
	"usage: example [--quiet | -q] [--verbose | -v]\n"
	"               [--recursive | -r] <dir|file> ...\n"},

	/* ioctl commands */
	{"getstate", hrfs_getstate, 0,
	"To list the state info for a given file or directory.\n"
	"usage: getstate [--quiet | -q] [--verbose | -v]\n"
	"                <dir|file> ...\n"},
	{"setbranch", hrfs_setbranch, 0,
	"To set the state info for a branch of a given file or directory.\n"
	"usage: setbranch [--quiet | -q] [--verbose | -v]\n"
	"                [--branch | -b bindex] [--data | -d flag]"
	"                [--attr | -a flag] [--xattr | -x flag]"
	"                <dir|file> ...\n"},
	{"rmbranch", hrfs_rmbranch, 0,
	"To set the state info for a branch of a given file or directory.\n"
	"usage: setbranch [--quiet | -q] [--verbose | -v]\n"
	"                [--branch | -b bindex]"
	"                <dir|file> ...\n"},
	{"setraid", hrfs_setraid, 0,
	"To set a raid level for a given file or directory.\n"
	"usage: setraid [--quiet | -q] [--verbose | -v]\n"
	"                [--raid | -r raid_type] <dir|file> ...\n"},

	/* control commands */
	{"hide", hrfs_hide, 0,
	"To list the state info for a given file or directory.\n"
	"usage: hide [--quiet | -q] [--verbose | -v] [--unhide | -u]\n"
	"                <dir> ...\n"},

	/* repair commands */
	{"repair", hrfs_repair, 0,
	"To repair a given file or directory.\n"
	"usage: repair [--quiet | -q] [--verbose | -v]\n"
	"                               <dir|file> ...\n"},

	{"diff", hrfs_diff, 0,
	"To find differences between two files.\n"
	"usage: diff [--quiet | -q] [--verbose | -v]\n"
	"                               from-file to-file\n"},

	/* test commands */
	{"log", hrfs_log, 0,
	"To test hrfs log system.\n"
	"usage: log [--quiet | -q] [--verbose | -v]\n"
	"                               log_messages ...\n"},
	{"cp", hrfs_cp, 0,
	"To test hrfs cp.\n"
	"usage: cp [--quiet | -q] [--verbose | -v] \n"
	"          [--preserve-all | -p] SOURCE DEST\n"},
	{"grouplock", hrfs_grouplock, 0,
	"To test hrfs group lock.\n"
	"usage: grouplock [--quiet | -q] [--verbose | -v] \n"
	"          [--groupid | -g groupid] <dir|file>\n"},	
	{ 0, 0, 0, NULL }
};

static int hrfs_getstate(int argc, char **argv)
{
	struct option long_opts[] = {
		{"quiet", no_argument, 0, 'q'},
		{"verbose", no_argument, 0, 'v'},
		{0, 0, 0, 0}
	};
	char short_opts[] = "qv";
	int c = 0;
	int rc = 0;
	hrfs_param_t param = { 0 };
	
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
		rc = hrfs_api_getstate(argv[optind], &param);
	} while (++optind < argc && !rc);

	if (rc) {
		fprintf(stderr, "error: %s failed for %s, ret = %d.\n",
		        argv[0], argv[optind - 1], rc);
	}
out:
	return rc;
}

static int hrfs_setraid(int argc, char **argv)
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
	hrfs_param_t param = { 0 };
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
		rc = hrfs_api_setraid(argv[optind], raid_type, &param);
		//hrfs_api_getstate(argv[optind], &param);
	} while (++optind < argc && !rc);


	if (rc) {
		fprintf(stderr, "error: %s failed for %s, ret = %d.\n",
		        argv[0], argv[optind - 1], rc);
	}
out:
	return rc;
}

/*
 * setbranch -b bindex -d 1 -x 1 -a 1 /path/to/hrfs/file
*/
static int hrfs_setbranch(int argc, char **argv)
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
	hrfs_param_t param = { 0 };
	char *end = NULL;
	char *branch_arg = NULL;
	char *data_arg = NULL;
	char *attr_arg = NULL;
	char *xattr_arg = NULL;
	struct hrfs_branch_valid valid = {HRFS_BSTATE_UNSETTED, HRFS_BSTATE_UNSETTED, HRFS_BSTATE_UNSETTED};
	int is_valid = HRFS_BSTATE_UNSETTED;
	hrfs_bindex_t bindex = 0;

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
		rc = hrfs_api_setbranch(argv[optind], bindex, &valid, &param);
	} while (++optind < argc && !rc);

	if (rc) {
		fprintf(stderr, "error: %s failed for %s, ret = %d.\n",
		        argv[0], argv[optind - 1], rc);
	}
out:
	return rc;
}

/*
 * rmbranch -b bindex /path/to/hrfs/file
*/
static int hrfs_rmbranch(int argc, char **argv)
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
	hrfs_param_t param = { 0 };
	char *end = NULL;
	char *branch_arg = NULL;
	hrfs_bindex_t bindex = 0;

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
		rc = hrfs_api_rmbranch(argv[optind], bindex, &param);
	} while (++optind < argc && !rc);

	if (rc) {
		fprintf(stderr, "error: %s failed for %s, ret = %d.\n",
		        argv[0], argv[optind - 1], rc);
	}
out:
	return rc;
}

static int hrfs_example(int argc, char **argv)
{
	struct option long_opts[] = {
		{"quiet", no_argument, 0, 'q'},
		{"verbose", no_argument, 0, 'v'},
		{0, 0, 0, 0}
	};
	char short_opts[] = "qv";
	int c = 0;
	int rc = 0;
	hrfs_param_t param = { 0 };
	
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

static int hrfs_repair(int argc, char **argv)
{
	struct option long_opts[] = {
		{"quiet", no_argument, 0, 'q'},
		{"verbose", no_argument, 0, 'v'},
		{"recursive", no_argument, 0, 'r'},
		{0, 0, 0, 0}
	};
	char short_opts[] = "qvr";
	int c = 0;
	int rc = 0;
	hrfs_param_t param = { 0 };
	
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
		case 'r':
			param.recursive++;
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
		rc = hrfs_api_repair(argv[optind], &param);
	} while (++optind < argc && !rc);

	if (rc) {
		fprintf(stderr, "error: %s failed for %s, rc = %d.\n",
		        argv[0], argv[optind - 1], rc);
	}
out:
	return rc;
}

static int hrfs_diff(int argc, char **argv)
{
	struct option long_opts[] = {
		{"quiet", no_argument, 0, 'q'},
		{"verbose", no_argument, 0, 'v'},
		{"recursive", no_argument, 0, 'r'},
		{0, 0, 0, 0}
	};
	char short_opts[] = "qvr";
	int c = 0;
	int rc = 0;
	hrfs_param_t param = { 0 };
	
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
			param.recursive++;
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
	
	if (argc - optind != 2) {
		rc = CMD_HELP;
		goto out;
	}
	rc = hrfs_api_diff(argv[optind], argv[optind + 1], &param);
out:
	return rc;
}

static int hrfs_log(int argc, char **argv)
{
	struct option long_opts[] = {
		{"quiet", no_argument, 0, 'q'},
		{"verbose", no_argument, 0, 'v'},
		{0, 0, 0, 0}
	};
	char short_opts[] = "qv";
	int c = 0;
	int rc = 0;
	hrfs_param_t param = { 0 };
	
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

	hrfs_log_init(HRFS_LOG_FILENAME, HRFS_LOGLEVEL_DEBUG);
	HLOG_DEBUG("Here is log test\n");
	do {
		HLOG_DEBUG("arg[%d] = %s\n", optind, argv[optind]);
		rc = 0;
	} while (++optind < argc && !rc);

	hrfs_log_finit();

	if (rc) {
		fprintf(stderr, "error: %s failed for %s, rc = %d.\n",
		        argv[0], argv[optind - 1], rc);
	}
out:
	return rc;
}

static int hrfs_cp(int argc, char **argv)
{
	struct option long_opts[] = {
		{"quiet", no_argument, 0, 'q'},
		{"verbose", no_argument, 0, 'v'},
		{"archive", no_argument, 0, 'a'},
		{"recursive", no_argument, 0, 'r'},
		{"preserve", no_argument, 0, 'p'},
		{0, 0, 0, 0}
	};
	char short_opts[] = "apqrv";
	int c = 0;
	int rc = 0;
	hrfs_param_t param = { 0 };
	struct cp_options cp_options = { 0 };

	cp_option_init(&cp_options);

	optind = 0;
	while ((c = getopt_long(argc, argv, short_opts, long_opts, NULL)) != -1) {
		switch (c) {
		case 'a':
          /* Like -dR --preserve=all with reduced failure diagnostics.  */
          cp_options.preserve_links = 1;
          cp_options.preserve_ownership = 1;
          cp_options.preserve_mode = 1;
          cp_options.preserve_timestamps = 1;
          cp_options.require_preserve = 1;
          cp_options.preserve_xattr = 1;
          cp_options.recursive = 1;
          break;
		case 'q':
 			param.quiet++;
			param.verbose = 0;
			break;
		case 'v':
			param.verbose++;
			cp_options.verbose++;
			param.quiet = 0;
			break;
		case 'p':
			cp_options.preserve_ownership = 1;
			cp_options.preserve_mode = 1;
			cp_options.preserve_timestamps = 1;
			cp_options.require_preserve = 1;
			break;
		case 'r':
        	cp_options.recursive = 1;
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
	
	if (cp_options.recursive) {
    	cp_options.copy_as_regular = 0;
	}

	if (optind >= argc - 1) {
		/* Should have source and dest */
		rc = CMD_HELP;
		goto out;
	}

	rc = hrfs_api_copy(argc - optind, argv + optind, &cp_options);
	if (rc) {
		fprintf(stderr, "error: %s failed for %s, rc = %d.\n",
		        argv[0], argv[optind - 1], rc);
	}
out:
	return rc;
}

static int hrfs_hide(int argc, char **argv)
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
	hrfs_param_t param = { 0 };
	struct cp_options cp_options = { 0 };
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
			cp_options.verbose++;
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
		rc = hrfs_api_hide(argv[optind], unhide, &param);
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
static int hrfs_grouplock(int argc, char **argv)
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
	hrfs_param_t param = { 0 };
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
		
		rc = hrfs_api_group_lock(filename, file[i].fd, group_id, &param);
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
				hrfs_api_group_unlock(filename, file[i].fd, group_id, &param);
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
	Parser_init("hrfs > ", hrfs_cmdlist);
	if (argc > 1) {
		rc = Parser_execarg(argc - 1, argv + 1, hrfs_cmdlist);
	} else {
		rc = Parser_commands();
	}

	Parser_exit(argc, argv);
	return rc;
}
