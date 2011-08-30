/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * FROM: swgfs/tests
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <unistd.h>
#include <time.h>
#include <limits.h>
#include <sys/ioctl.h>

#if 0
#include <linux/ldiskfs_fs.h>
#endif
#ifdef LIXI_20110408
#include <libswgfs.h>
#include <swgfs_lib.h>
#include <obd.h>
#endif

struct option longopts[] = {
	{"ea", 0, 0, 'e'},
	{"lookup", 0, 0, 'l'},
	{"random", 0, 0, 'r'},
	{"stat", 0, 0, 's'},
	{NULL, 0, 0, 0},
};
char *shortopts = "ehlr:s0123456789";

static int usage(char *prog, FILE *out)
{
        fprintf(out,
		"Usage: %s [-r rand_seed] {-s|-e|-l} filenamebase total_files iterations\n"
               "-r : random seed\n"
               "-s : regular stat() calls\n"
               "-e : open then GET_EA ioctl\n"
               "-l : lookup ioctl only\n", prog);
        exit(out == stderr);
}

#ifndef LONG_MAX
#define LONG_MAX (1 << ((8 * sizeof(long)) - 1))
#endif

int main(int argc, char ** argv)
{
        long i, c, count, iter = LONG_MAX, mode = 0, offset;
        long int start, length = LONG_MAX, last, rc = 0;
        char parent[4096], *t;
	char *prog = argv[0], *base;
	int seed = 0;
	int fd = -1;

	while ((c = getopt_long(argc, argv, shortopts, longopts, NULL)) != -1) {
		char *e;
		switch (c) {
		case 'r':
			seed = strtoul(optarg, &e, 0);
			if (*e) {
				fprintf(stderr, "bad -r option %s\n", optarg);
				usage(prog, stderr);
			}
			break;
		case 'e':
		case 'l':
		case 's':
			mode = c;
			break;
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			if (length == LONG_MAX)
				length = c - '0';
			else
				length = length * 10 + (c - '0');
			break;
		case 'h':
			usage(prog, stdout);
		case '?':
			usage(prog, stderr);
		}
	}

	if (optind + 2 + (length == LONG_MAX) != argc) {
		fprintf(stderr, "missing filenamebase, total_files, or iterations\n");
		usage(prog, stderr);
	}

        base = argv[optind];
        if (strlen(base) > 4080) {
                fprintf(stderr, "filenamebase too long\n");
                exit(1);
        }

	if (seed == 0) {
		int f = open("/dev/urandom", O_RDONLY);

		if (f < 0 || read(f, &seed, sizeof(seed)) < sizeof(seed))
			seed = time(0);
		if (f > 0)
			close(f);
	}

	printf("using seed %u\n", seed);
	srand(seed);

        count = strtoul(argv[optind + 1], NULL, 0);
	if (length == LONG_MAX) {
		iter = strtoul(argv[optind + 2], NULL, 0);
		printf("running for %lu iterations\n", iter);
	} else
		printf("running for %lu seconds\n", length);

        start = last = time(0);

        t = strrchr(base, '/');
        if (t == NULL) {
                strcpy(parent, ".");
                offset = -1;
        } else {
                strncpy(parent, base, t - base);
                offset = t - base + 1;
        }

	if (mode == 'l') {
		fd = open(parent, O_RDONLY);
		if (fd < 0) {
			printf("open(%s) error: %s\n", parent,
			       strerror(errno));
			exit(errno);
		}
	}

	for (i = 0; i < iter && time(0) - start < length; i++) {
		char filename[4096];
		int tmp;

		tmp = random() % count;
		sprintf(filename, "%s%d", base, tmp);

		if (mode == 'e') {
#if 0
			fd = open(filename, O_RDWR|O_LARGEFILE);
			if (fd < 0) {
				printf("open(%s) error: %s\n", filename,
				strerror(errno));
				break;
			}
			rc = ioctl(fd, LDISKFS_IOC_GETEA, NULL);
			if (rc < 0) {
				printf("ioctl(%s) error: %s\n", filename,
				strerror(errno));
				break;
				}
			close(fd);
			break;
#endif
		} else if (mode == 's') {
			struct stat buf;

			rc = stat(filename, &buf);
			if (rc < 0) {
				printf("stat(%s) error: %s\n", filename,
				strerror(errno));
				break;
			}
		} else if (mode == 'l') {
#ifdef LIXI_20110408
			struct obd_ioctl_data data;
			char rawbuf[8192];
			char *buf = rawbuf;
			int max = sizeof(rawbuf);

			memset(&data, 0, sizeof(data));
			data.ioc_version = OBD_IOCTL_VERSION;
			data.ioc_len = sizeof(data);
			if (offset >= 0)
				data.ioc_inlbuf1 = filename + offset;
			else
				data.ioc_inlbuf1 = filename;
			ata.ioc_inllen1 = strlen(data.ioc_inlbuf1) + 1;

			if (obd_ioctl_pack(&data, &buf, max)) {
				printf("ioctl_pack failed.\n");
				break;
			}

			rc = ioctl(fd, IOC_MDC_LOOKUP, buf);
			if (rc < 0) {
				printf("ioctl(%s) error: %s\n", filename,
				strerror(errno));
				break;
			}
#endif
		}
		if ((i % 10000) == 0) {
			printf(" - stat %lu (time %ld ; total %ld ; last %ld)\n",
				i, time(0), time(0) - start, time(0) - last);
			last = time(0);
		}
	}

	if (mode == 'l')
		close(fd);

	printf("total: %lu stats in %ld seconds: %f stats/second\n", i,
		time(0) - start, ((float)i / (time(0) - start)));

	exit(rc);
}
