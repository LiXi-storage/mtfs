/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <debug.h>
#include "libcfs_debug.h"

static int subsystem_mask = ~0;
static int debug_mask = ~0;

/* all strings nul-terminated; only the struct and hdr need to be freed */
struct dbg_line {
        struct ptldebug_header *hdr;
        char *file;
        char *fn;
        char *text;
};

int dbg_open_ctlhandle(const char *str)
{
	int fd;
	fd = open(str, O_WRONLY);
	if (fd < 0) {
		HERROR("open %s failed: %s\n", str,
		       strerror(errno));
		return -1;
	}
	return fd;
}

void dbg_close_ctlhandle(int fd)
{
	close(fd);
}

int dbg_write_cmd(int fd, char *str, int len)
{
	int rc  = write(fd, str, len);

	return (rc == len ? 0 : 1);
}

static void dump_hdr(unsigned long long offset, struct ptldebug_header *hdr)
{
        fprintf(stderr, "badly-formed record at offset = %llu\n", offset);
        fprintf(stderr, "  len = %u\n", hdr->ph_len);
        fprintf(stderr, "  flags = %x\n", hdr->ph_flags);
        fprintf(stderr, "  subsystem = %x\n", hdr->ph_subsys);
        fprintf(stderr, "  mask = %x\n", hdr->ph_mask);
        fprintf(stderr, "  cpu_id = %u\n", hdr->ph_cpu_id);
        fprintf(stderr, "  seconds = %u\n", hdr->ph_sec);
        fprintf(stderr, "  microseconds = %lu\n", (long)hdr->ph_usec);
        fprintf(stderr, "  stack = %u\n", hdr->ph_stack);
        fprintf(stderr, "  pid = %u\n", hdr->ph_pid);
        fprintf(stderr, "  host pid = %u\n", hdr->ph_extern_pid);
        fprintf(stderr, "  line number = %u\n", hdr->ph_line_num);
}

static int cmp_rec(const void *p1, const void *p2)
{
	struct dbg_line *d1 = *(struct dbg_line **)p1;
	struct dbg_line *d2 = *(struct dbg_line **)p2;

	if (d1->hdr->ph_sec < d2->hdr->ph_sec)
		return -1;
	if (d1->hdr->ph_sec == d2->hdr->ph_sec &&
	    d1->hdr->ph_usec < d2->hdr->ph_usec)
		return -1;
	if (d1->hdr->ph_sec == d2->hdr->ph_sec &&
	    d1->hdr->ph_usec == d2->hdr->ph_usec)
		return 0;
	return 1;
}

static void print_rec(struct dbg_line ***linevp, int used, int fdout)
{
	struct dbg_line **linev = *linevp;
	int i;

	qsort(linev, used, sizeof(struct dbg_line *), cmp_rec);
	for (i = 0; i < used; i++) {
		struct dbg_line *line = linev[i];
		struct ptldebug_header *hdr = line->hdr;
		char out[4097];
		char *buf = out;
		int bytes;
		ssize_t bytes_written;

		bytes = sprintf(out, "%08x:%08x:%u%s:%u.%06llu:%u:%u:%u:(%s:%u:%s()) %s",
				hdr->ph_subsys, hdr->ph_mask,
				hdr->ph_cpu_id,
				hdr->ph_flags & PH_FLAG_FIRST_RECORD ? "F" : "",
				hdr->ph_sec, (unsigned long long)hdr->ph_usec,
				hdr->ph_stack, hdr->ph_pid, hdr->ph_extern_pid,
				line->file, hdr->ph_line_num, line->fn, line->text);
		while (bytes > 0) {
			bytes_written = write(fdout, buf, bytes);
			if (bytes_written <= 0)
				break;
			bytes -= bytes_written;
			buf += bytes_written;
		}
		free(line->hdr);
		free(line);
	}
	free(linev);
	*linevp = NULL;
}

static int add_rec(struct dbg_line *line, struct dbg_line ***linevp, int *lenp,
                   int used)
{
        struct dbg_line **linev = *linevp;

        if (used == *lenp) {
                int nlen = *lenp + 4096;
                int nsize = nlen * sizeof(struct dbg_line *);

                linev = realloc(*linevp, nsize);
                if (!linev)
                        return -ENOMEM;

                *linevp = linev;
                *lenp = nlen;
        }
        linev[used] = line;

        return 0;
}

#define HDR_SIZE sizeof(*hdr)

int parse_buffer(int fdin, int fdout)
{
	struct dbg_line *line;
	struct ptldebug_header *hdr;
	char buf[4097], *ptr;
	unsigned long dropped = 0, kept = 0, bad = 0;
	struct dbg_line **linev = NULL;
	int linev_len = 0;
	int rc;
	HENTRY();
 
	hdr = (void *)buf;
  
	while (1) {
		int first_bad = 1;
		int count;
 
		count = HDR_SIZE;
		ptr = buf;
	readhdr:
		rc = read(fdin, ptr, count);
		if (rc <= 0)
			goto print;
  
		ptr += rc;
		count -= rc;
		if (count > 0)
			goto readhdr;
		
		if (hdr->ph_len > 4094 ||       /* is this header bogus? */
		    hdr->ph_stack > 65536 ||
		    hdr->ph_sec < (1 << 30) ||
		    hdr->ph_usec > 1000000000 ||
		    hdr->ph_line_num > 65536) {
			if (first_bad)
				dump_hdr(lseek(fdin, 0, SEEK_CUR), hdr);
			bad += first_bad;
			first_bad = 0;
 
			/* try to restart on next line */
			while (count < HDR_SIZE && buf[count] != '\n')
				count++;
			if (buf[count] == '\n')
				count++; /* move past '\n' */
			if (HDR_SIZE - count > 0) {
				int left = HDR_SIZE - count;

				memmove(buf, buf + count, left);
				ptr = buf + left;

				goto readhdr;
			}

			continue;
		}
  
		if (hdr->ph_len == 0)
			continue;

		count = hdr->ph_len - HDR_SIZE;
	readmore:
		rc = read(fdin, ptr, count);
		if (rc <= 0)
			break;
  
		ptr += rc;
		count -= rc;
		if (count > 0)
			goto readmore;
 
		first_bad = 1;

		if ((hdr->ph_subsys && !(subsystem_mask & hdr->ph_subsys)) ||
		    (hdr->ph_mask && !(debug_mask & hdr->ph_mask))) {
			dropped++;
			continue;
		}
  
	retry_alloc:
		line = malloc(sizeof(*line));
		if (line == NULL) {
			 if (linev) {
				HERROR("error: line malloc(%u): "
					"printing accumulated records\n",
					(unsigned int)sizeof(*line));
				print_rec(&linev, kept, fdout);

				goto retry_alloc;
			}
			HERROR("error: line malloc(%u): exiting\n",
				(unsigned int)sizeof(*line));
			break;
		}

		line->hdr = malloc(hdr->ph_len + 1);
		if (line->hdr == NULL) {
			free(line);
			if (linev) {
				HERROR("error: hdr malloc(%u): "
				       "printing accumulated records\n",
				       hdr->ph_len + 1);
				print_rec(&linev, kept, fdout);
 
				goto retry_alloc;
			}
			HERROR("error: hdr malloc(%u): exiting\n",
			       hdr->ph_len + 1);
			break;
		}
  
		ptr = (void *)line->hdr;
		memcpy(line->hdr, buf, hdr->ph_len);
		ptr[hdr->ph_len] = '\0';

		ptr += sizeof(*hdr);
		line->file = ptr;
		ptr += strlen(line->file) + 1;
		line->fn = ptr;
		ptr += strlen(line->fn) + 1;
		line->text = ptr;
 
	retry_add:
		if (add_rec(line, &linev, &linev_len, kept) < 0) {
			if (linev) {
				HERROR("error: add_rec[%u] failed; "
				       "print accumulated records\n",
				       linev_len);
				print_rec(&linev, kept, fdout);

				goto retry_add;
			}
			HERROR("error: add_rec[0] failed; exiting\n");
			break;
		}
		kept++;
	}

print:
	if (linev)
		print_rec(&linev, kept, fdout);

	printf("Debug log: %lu lines, %lu kept, %lu dropped, %lu bad.\n",
		dropped + kept + bad, kept, dropped, bad);

	HRETURN(0);
}
