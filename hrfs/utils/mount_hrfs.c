/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <mntent.h>
#include <string.h>
#include <getopt.h>
#include <sys/mount.h>
#include <hrfs_common.h>
#include <parse_option.h>
#include "hrfs_version.h"

int          verbose = 0;
int          nomtab = 0;
int          fake = 0;
int          force = 0;
int          retry = 0;
char         *progname = NULL;

#ifndef max
#define max(x,y) ((x)>(y) ? (x) : (y))
#endif

void usage(FILE *out)
{
	fprintf(out, "%s v"HRFS_VERSION_STRING"\n", progname);
	fprintf(out, "\nThis mount helper should only be invoked via the "
	        "mount (8) command,\ne.g. mount -t hrfs dev dir\n\n");
	fprintf(out, "usage: %s [-fhnv] [-o <mntopt>] <device> <mountpt>\n",
	        progname);
	fprintf(out,
	        "\t<device>: the disk device, or for a client:\n"
	        "\t<mountpt>: filesystem mountpoint (e.g. /mnt/swgfs)\n"
	        "\t-f|--fake: fake mount (updates /etc/mtab)\n"
	        "\t-o force|--force: force mount even if already in /etc/mtab\n"
	        "\t-h|--help: print this usage message\n"
	        "\t-n|--nomtab: do not update /etc/mtab after mount\n"
	        "\t-v|--verbose: print verbose config settings\n"
	        "\t<mntopt>: one or more comma separated of:\n"
	        "\t\t(no)flock,(no)user_xattr,(no)acl\n"
	        "\t\tretry=<num>: number of times mount is retried by client\n"
	);
}

static int check_mtab_entry(char *spec, char *mtpt, char *type)
{
	FILE *fp;
	struct mntent *mnt;

	fp = setmntent(MOUNTED, "r");
	if (fp == NULL) {
		return(0);
	}

	while ((mnt = getmntent(fp)) != NULL) {
		if (strcmp(mnt->mnt_fsname, spec) == 0 &&
			strcmp(mnt->mnt_dir, mtpt) == 0 &&
			strcmp(mnt->mnt_type, type) == 0) {
			endmntent(fp);
			return(EEXIST);
		}
	}
	endmntent(fp);

	return(0);
}

static int
update_mtab_entry(char *spec, char *mtpt, char *type, char *opts,
                  int flags, int freq, int pass)
{
	FILE *fp;
	struct mntent mnt;
	int rc = 0;

	mnt.mnt_fsname = spec;
	mnt.mnt_dir = mtpt;
	mnt.mnt_type = type;
	mnt.mnt_opts = opts ? opts : "";
	mnt.mnt_freq = freq;
	mnt.mnt_passno = pass;

	fp = setmntent(MOUNTED, "a+");
	if (fp == NULL) {
		fprintf(stderr, "%s: setmntent(%s): %s:",
		        progname, MOUNTED, strerror (errno));
		rc = 16;
	} else {
		if ((addmntent(fp, &mnt)) == 1) {
			fprintf(stderr, "%s: addmntent: %s:",
				progname, strerror (errno));
			rc = 16;
		}
		endmntent(fp);
	}

	return rc;
}

/*****************************************************************************
 *
 * This part was cribbed from util-linux/mount/mount.c.  There was no clear
 * license information, but many other files in the package are identified as
 * GNU GPL, so it's a pretty safe bet that was their intent.
 *
 ****************************************************************************/
struct opt_map {
	const char *opt;        /* option name */
	int inv;                /* true if flag value should be inverted */
	int mask;               /* flag mask value */
};
static const struct opt_map opt_map[] = {
	{ "defaults", 0, 0         },      /* default options */
	{ "rw",       1, MS_RDONLY },      /* read-write */
	{ "ro",       0, MS_RDONLY },      /* read-only */
	{ NULL,       0, 0         }
};

/*
 * 1  = don't pass on to hrfs
 * 0  = pass on to hrfs
 */
static int parse_one_option(const char *check, int *flagp)
{
	const struct opt_map *opt;

	for (opt = &opt_map[0]; opt->opt != NULL; opt++) {
		if (strncmp(check, opt->opt, strlen(opt->opt)) == 0) {
			if (opt->mask) {
				if (opt->inv) {
					*flagp &= ~(opt->mask);
				} else {
					*flagp |= opt->mask;
				}
			}
			return 1;
		}
	}
	/*
	 * Assume any unknown options are valid and pass them on.
	 * The mount will fail if hrfs doesn't recognize it.
	 */
	return 0;
}

static void append_option(char *options, const char *one)
{
	if (*options) {
		strcat(options, ",");
	}
	strcat(options, one);
}


int parse_options(char *orig_options, int *flagp)
{
	char *options = NULL;
	char *nextopt = NULL;
	char *opt = NULL;
	int ret = 0;

	options = calloc(strlen(orig_options) + 1, 1);
	if (options == NULL) {
		ret = ENOMEM;
		goto out;
	}

	*flagp = 0;
	nextopt = orig_options;
	while ((opt = strsep(&nextopt, ","))) {
		if (!*opt) {
			/* empty option */
			continue;
		}
		if (strncmp(opt, "force", 5) == 0) {
			++force;
			printf("force: %d\n", force);
		} else if (parse_one_option(opt, flagp) == 0) {
			/* pass this on as an option */
			append_option(options, opt);
		}
	}

	strcpy(orig_options, options);
	free(options);
out:
	return ret;
}

char *reslove_source_path(const char *origin_source)
{
	int ret = 0;
	mount_option_t mount_option;
	hrfs_bindex_t bindex = 0;
	char *path = NULL;
	char *source = NULL;
	char rpath[PATH_MAX] = {'\0'};
	int source_len = 0;

	source = strdup(origin_source);
	ret = parse_dir_option(source, &mount_option);
	free(source);
	if (ret) {
		source = NULL;
		goto out;
	}

	/* TODO: BUG if longer than a page? */
	source_len = mount_option.bnum * (PATH_MAX + 1);
	if (source_len > 4095) {
		source_len = 4095;
	}
	source = calloc(source_len, 1);
	if (source == NULL) {
		goto finit_option;
	}

	for (bindex = 0; bindex < mount_option.bnum; bindex++) {
		path = mount_option.branch[bindex].path;
		if (realpath(path, rpath) == NULL) {
			fprintf(stderr, "Failed to get realpath for '%s' at branch[%d]: %s\n",
			        path, bindex, strerror(errno));
			free(source);
			source = NULL;
			goto finit_option;
		}
		append_dir(source, rpath);
		if (verbose) {
			printf("branch[%d] = %s\n", bindex, rpath);
		}
	}
finit_option:
	mount_option_finit(&mount_option);
out:
	return source;
}

int main(int argc, char *const argv[])
{
	char default_options[] = "";
	char *options = default_options;
	char *origin_source = NULL;
	char *source = NULL;
	char *target = NULL;
	char *ptr = NULL;
	int opt = 0;
	int i = 0;
	int rc = 0;
	int flags = 0;
	int optlen = 0;
	char *optcopy = NULL;
	static struct option long_opt[] = {
		{"fake", 0, 0, 'f'},
		{"force", 0, 0, 1},
		{"help", 0, 0, 'h'},
		{"nomtab", 0, 0, 'n'},
		{"options", 1, 0, 'o'},
		{"verbose", 0, 0, 'v'},
		{0, 0, 0, 0}
	};
	
	progname = strrchr(argv[0], '/');
	progname = progname ? progname + 1 : argv[0];

	while ((opt = getopt_long(argc, argv, "fhno:v",
		   long_opt, NULL)) != EOF){
		switch (opt) {
		case 1:
			++force;
			printf("force: %d\n", force);
			break;
		case 'f':
			++fake;
			printf("fake: %d\n", fake);
			break;
		case 'h':
			usage(stdout);
			break;
		case 'n':
			++nomtab;
			printf("nomtab: %d\n", nomtab);
			break;
		case 'o':
			options = optarg;
			break;
		case 'v':
			++verbose;
			break;
		default:
			fprintf(stderr, "%s: unknown option '%c'\n",
			        progname, opt);
			        usage(stderr);
			break;
		}
	}

	if (optind + 2 > argc) {
		fprintf(stderr, "%s: too few arguments\n", progname);
		usage(stderr);
		rc = EINVAL;
		goto out;
	}

	origin_source = argv[optind];
	if (origin_source == NULL) {
		usage(stderr);
		goto out;
	}

	target = argv[optind + 1];
	ptr = target + strlen(target) - 1;
	while ((ptr > target) && (*ptr == '/')) {
		*ptr = 0;
		ptr--;
	}

	if (verbose) {
		for (i = 0; i < argc; i++) {
			printf("arg[%d] = %s\n", i, argv[i]);
		}
		printf("source = %s, target = %s\n", origin_source, target);
		printf("options = %s\n", options);
	}

	rc = parse_options(options, &flags);
	if (rc) {
		fprintf(stderr, "%s: can't parse options: %s\n",
		        progname, options);
		rc = EINVAL;
		goto out;
	}
	
	source = reslove_source_path(origin_source);
	if (source == NULL) {
		fprintf(stderr, "%s: can't reslove device option %s\n",
		        progname, origin_source);
		rc = EINVAL;
		goto out;
	}

	if (verbose) {
		printf("resolved source = %s\n", source);
	}

	if (!force) {
		/*
		 * TODO: need stricter check, not enough
		 * e.g. dir1:dir2, dir2:dir1
		 */
		rc = check_mtab_entry(source, target, "hrfs");
		if (rc && !(flags & MS_REMOUNT)) {
			fprintf(stderr, "%s: according to %s %s is "
			        "already mounted on %s\n",
			        progname, MOUNTED, source, target);
			rc = EEXIST;
			goto free_source;
		}
		if (!rc && (flags & MS_REMOUNT)) {
			fprintf(stderr, "%s: according to %s %s is "
			        "not already mounted on %s\n",
			        progname, MOUNTED, source, target);
			rc = ENOENT;
			goto free_source;
		}
	}

	if (flags & MS_REMOUNT) {
		nomtab++;
	}

 	rc = access(target, F_OK);
	if (rc) {
		rc = errno;
		fprintf(stderr, "%s: %s inaccessible: %s\n", progname, target,
		        strerror(errno));
		goto free_source;
	}

	/*
	 * In Linux 2.4, the target device doesn't get passed to any of our
	 * functions.  So we'll stick it on the end of the options.
	 */
	optlen = strlen(options) + strlen(",device=") + strlen(source) + 1;
	optcopy = malloc(optlen);
	if (optcopy == NULL) {
		fprintf(stderr, "can't allocate memory to optcopy\n");
		rc = ENOMEM;
		goto free_source;
	}
	strcpy(optcopy, options);
	if (*optcopy) {
		strcat(optcopy, ",");
	}
	strcat(optcopy, "device=");
	strcat(optcopy, source);

	if (verbose) {
		printf("mounting device %s at %s, flags=%#x options=%s\n",
		       source, target, flags, optcopy);
	}

	if (!fake) {
		for (i = 0, rc = -EAGAIN; i <= retry && rc != 0; i++) {
			rc = mount(source, target, "hrfs", flags, (void *)optcopy);
			if (rc) {
				if (verbose) {
					fprintf(stderr, "%s: mount %s at %s "
					        "failed: %s retries left: "
					        "%d\n", basename(progname),
					        source, target,
					        strerror(errno), retry-i);
				}

				if (retry) {
					sleep(1 << max((i/2), 5));
				} else {
					rc = errno;
				}
			}
		}
	}

	if (rc) {
		rc = errno;
		fprintf(stderr, "%s: mount %s at %s failed: %s\n", progname,
		        source, target, strerror(errno));
		switch (errno) {
		case ENODEV:
			fprintf(stderr, "Are the hrfs modules loaded?\n");
			break;
		case ENOTBLK:
			fprintf(stderr, "Do you need -o loop?\n");
			break;
		case EINVAL:
			fprintf(stderr, "Are the mount options correct?\n");
			break;
		case ENOENT:
			fprintf(stderr, "Are the source directorys exist?\n");
			break;
		default:
			break;
		}
		goto free_optcopy;
	}

	if (!nomtab) {
		if (verbose) {
			printf("Adding to mtab\n");
		}
		rc = update_mtab_entry(source, target, "hrfs", options, 0, 0, 0);
		if (rc) {
			fprintf(stderr, "Failed to update mtabl\n");
		}
	}

	goto free_optcopy;
free_optcopy:
	free(optcopy);
free_source:
	free(source);
out:
	return rc;
}
