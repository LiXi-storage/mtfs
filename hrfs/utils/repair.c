/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <debug.h>
#include <memory.h>
#include "repair.h"
#include "libcoreutils/dirname.h"
#include "exit_status.h"

static int recursive = 0;
static int verbose = 0;

struct file_data {
	int             desc;	/* File descriptor  */
	char const      *name;	/* File name  */
	struct stat     stat;	/* File status */

	/* Buffer in which text of file is read. */
	uintmax_t *buffer;

	/*
	 * Allocated size of buffer, in bytes.  Always a multiple of
	 * sizeof *buffer.
	 */
	size_t bufsize;
	
	/* Number of valid bytes now in the buffer. */
	size_t buffered;
	
	/* 1 if at end of file.  */
	int eof;
};

/*
 * The file buffer, considered as an array of bytes rather than
 * as an array of words.
 */
#define FILE_BUFFER(f) ((char *) (f)->buffer)

struct comparison {
	struct file_data file[2];
	struct comparison const *parent;  /* parent, if a recursive comparison */
};

struct dirdata
{
	size_t nnames;	/* Number of names.  */
	char const **names;	/* Sorted names of files in dir, followed by 0.  */
	char *data;	/* Allocated storage for file names.  */
};

/* Read a directory and get its vector of names.  */
static int dir_read(struct file_data const *dir, struct dirdata *dirdata)
{
	register struct dirent *next;
	register size_t i;

	/* Address of block containing the files that are described.  */
	char const **names;

	/* Number of files in directory.  */
	size_t nnames;

	/* Allocated and used storage for file name data.  */
	char *data;
	size_t data_alloc, data_used;
	int ret = 0;

	dirdata->names = 0;
	dirdata->data = 0;
	nnames = 0;
	data = 0;

	/* Open the directory and check for errors.  */
	register DIR *reading = opendir(dir->name);
	if (!reading) {
		HERROR("failed to open directory %s: %s\n", dir->name, strerror(errno));
		ret = -1;
		goto out;
	}

	/* Initialize the table of filenames.  */
	data_alloc = 512;
	data_used = 0;
	dirdata->data = data = malloc(data_alloc);

	/*
	 * Read the directory entries, and insert the subfiles
	 * into the `data' table. 
	 */

	while ((errno = 0, (next = readdir (reading)) != 0)) {
		char *d_name = next->d_name;
		size_t d_size = _D_EXACT_NAMLEN (next) + 1;

		/* Ignore "." and "..".  */
		if (d_name[0] == '.'
		    && (d_name[1] == 0 || (d_name[1] == '.' && d_name[2] == 0))) {
			continue;
		}

		while (data_alloc < data_used + d_size) {
			dirdata->data = data = realloc (data, data_alloc *= 2);
	    }
		memcpy (data + data_used, d_name, d_size);
		data_used += d_size;
		nnames++;
	}
	
	if (errno) {
		int e = errno;
		closedir (reading);
		errno = e;
		ret = -1;
		goto free_data;
	}

	if (closedir(reading) != 0) {
		HERROR("failed to closedir %s: %s\n", dir->name, strerror(errno));
		ret = -1;
		goto free_data;
	}

	dirdata->names = names = malloc ((nnames + 1) * sizeof *names);
	if (names == NULL) {
		HERROR("not enough memory\n");
		ret = -1;
		goto free_data;
	}

	dirdata->nnames = nnames;
	for (i = 0;  i < nnames;  i++)
	{
		names[i] = data;
		data += strlen (data) + 1;
	}
	names[nnames] = 0;
	goto out;
//free_names:
	free(dirdata->names);
free_data:
	free(dirdata->data);
out:
	return ret;
}

void dump_dirdata(struct dirdata *dirdata)
{
	register size_t i;
	char const **names = dirdata->names;

	if (dirdata->nnames == 0) {
		HPRINT("directory is empty\n");
	} else {
		HPRINT("directory: \n");
		for (i = 0;  i < dirdata->nnames;  i++) {
			HPRINT("%s\n", names[i]);
		}
	}
}

typedef int (handle_file_t) (struct comparison const *parent, char const *name0, char const *name1);

/* Do struct stat *S, *T describe the same special file?  */
#  define same_special_file(s, t) \
     (((S_ISBLK ((s)->st_mode) && S_ISBLK ((t)->st_mode)) \
       || (S_ISCHR ((s)->st_mode) && S_ISCHR ((t)->st_mode))) \
      && (s)->st_rdev == (t)->st_rdev)

/* Do struct stat *S, *T describe the same file?  Answer -1 if unknown.  */
# define same_file(s, t) \
    ((((s)->st_ino == (t)->st_ino) && ((s)->st_dev == (t)->st_dev)) \
     || same_special_file (s, t))

# define same_file_attributes(s, t) \
   ((s)->st_mode == (t)->st_mode \
    && (s)->st_nlink == (t)->st_nlink \
    && (s)->st_uid == (t)->st_uid \
    && (s)->st_gid == (t)->st_gid \
    && (s)->st_size == (t)->st_size \
    && (s)->st_mtime == (t)->st_mtime \
    && (s)->st_ctime == (t)->st_ctime)

static int dir_loop(struct comparison const *cmp, int i)
{
	struct comparison const *p = cmp;

	while ((p = p->parent)) {
		if (0 < same_file(&p->file[i].stat, &cmp->file[i].stat)) {
			return true;
		}
	}
  return false;
}

#define file_name_cmp strcmp

static int
compare_names (char const *name1, char const *name2)
{
  return file_name_cmp(name1, name2);
}


static int compare_names_for_qsort(void const *file1, void const *file2)
{
  char const *const *f1 = file1;
  char const *const *f2 = file2;
  return compare_names(*f1, *f2);
}

int diff_dirs(struct comparison const *cmp, handle_file_t *handle_file)
{
	struct dirdata dirdata[2];
	char const **volatile names[2];
	int i;
	int volatile val = EXIT_SUCCESS;
	
	if ((cmp->file[0].desc == -1 || dir_loop(cmp, 0))
	    && (cmp->file[1].desc == -1 || dir_loop(cmp, 1))) {
		HERROR("%s: recursive directory loop\n", cmp->file[cmp->file[0].desc == -1].name);
		return EXIT_TROUBLE;
	}
	
	for (i = 0; i < 2; i++) {		
		if (dir_read(&cmp->file[i], &dirdata[i]) < 0) {
			HERROR("failed to read directory %s\n", cmp->file[i].name);
			//val = EXIT_TROUBLE;
			return EXIT_TROUBLE;
		}
	}
	
	if (val == EXIT_SUCCESS) {
		names[0] = dirdata[0].names;
		names[1] = dirdata[1].names;
		for (i = 0; i < 2; i++) {
			qsort(names[i], dirdata[i].nnames, sizeof *dirdata[i].names, compare_names_for_qsort);
		}
	
		/* Loop while files remain in one or both dirs.  */
		while (*names[0] || *names[1]) {
			/*
			 * Compare next name in dir 0 with next name in dir 1.
			 * At the end of a dir,
			 * pretend the "next name" in that dir is very large.
			 */
			int nameorder = (!*names[0] ? 1 : !*names[1] ? -1
			                : compare_names(*names[0], *names[1]));
			int v1 = (*handle_file) (cmp, 0 < nameorder ? 0 : *names[0]++, 
			                      nameorder < 0 ? 0 : *names[1]++);
	  	if (val < v1) {
	    	val = v1;
	    }
		}
	}
	for (i = 0; i < 2; i++) {
		free (dirdata[i].names);
		free (dirdata[i].data);
	}
	return val;
}

char *concat(char const *s1, char const *s2, char const *s3)
{
  char *new = malloc(strlen (s1) + strlen (s2) + strlen (s3) + 1);
  sprintf(new, "%s%s%s", s1, s2, s3);
  return new;
}

/*
 * Yield the newly malloc'd pathname
 * of the file in DIR whose filename is FILE.
 */
char *dir_file_pathname(char const *dir, char const *file)
{
	char const *base = last_component(dir);
	char *tmp;
	size_t baselen = base_len(base);
	int omit_slash = (baselen == 0 || base[baselen] == '/');
	/*
	 * LIXI: origin is base[baselen - 1] == '/',
	 * but slash is already omitted by last_component
	 * then omit_slash is always false
	 */
	tmp = concat(dir, "/" + omit_slash, file);
	return tmp;
}

#define _(text) (text)
char const *file_type(struct stat const *st)
{
  /* See POSIX 1003.1-2001 XCU Table 4-8 lines 17093-17107 for some of
     these formats.

     To keep diagnostics grammatical in English, the returned string
     must start with a consonant.  */

  if (S_ISREG (st->st_mode))
    return st->st_size == 0 ? _("regular empty file") : _("regular file");

  if (S_ISDIR (st->st_mode))
    return _("directory");

  if (S_ISBLK (st->st_mode))
    return _("block special file");

  if (S_ISCHR (st->st_mode))
    return _("character special file");

  if (S_ISFIFO (st->st_mode))
    return _("fifo");

  if (S_ISLNK (st->st_mode))
    return _("symbolic link");

  if (S_ISSOCK (st->st_mode))
    return _("socket");

  if (S_TYPEISMQ (st))
    return _("message queue");

  if (S_TYPEISSEM (st))
    return _("semaphore");

  if (S_TYPEISSHM (st))
    return _("shared memory object");

#if 0
  if (S_TYPEISTMO (st))
    return _("typed memory object");
#endif

  return _("weird file");
}

/*
 * Least common multiple of two buffer sizes A and B.  However, if
 * either A or B is zero, or if the multiple is greater than LCM_MAX,
 * return a reasonable buffer size.
 */
size_t buffer_lcm(size_t a, size_t b, size_t lcm_max)
{
	size_t lcm, m, n, q, r;

	/* Yield reasonable values if buffer sizes are zero.  */
	if (!a) {
		return b ? b : 8 * 1024;
	}

	if (!b) {
		return a;
	}

	/* n = gcd (a, b) */
	for (m = a, n = b;  (r = m % n) != 0;  m = n, n = r) {
		continue;
	}

	/* Yield a if there is an overflow.  */
	q = a / n;
	lcm = q * b;
	return lcm <= lcm_max && lcm / b == q ? lcm : a;
}

#define TYPE_MAXIMUM(t) \
  ((t) (! TYPE_SIGNED (t) \
        ? (t) -1 \
        : ~ (~ (t) 0 << (sizeof (t) * CHAR_BIT - 1))))

#define STAT_BLOCKSIZE(s) ((s).st_blksize)

/*
 * Read NBYTES bytes from descriptor FD into BUF.
 * NBYTES must not be SIZE_MAX.
 * Return the number of characters successfully read.
 * On error, return SIZE_MAX, setting errno.
 * The number returned is always NBYTES unless end-of-file or error.
 */

size_t block_read(int fd, char *buf, size_t nbytes)
{
	char *bp = buf;
	char const *buflim = buf + nbytes;
	size_t readlim = MIN (SSIZE_MAX, SIZE_MAX);

	do {
		size_t bytes_remaining = buflim - bp;
		size_t bytes_to_read = MIN(bytes_remaining, readlim);
		ssize_t nread = read(fd, bp, bytes_to_read);
		if (nread <= 0) {
			if (nread == 0) {
				break;
			}
			/*
			 * Accommodate Tru64 5.1, which can't read more than INT_MAX
			 * bytes at a time.  They call that a 64-bit OS?
			 */
			if (errno == EINVAL && INT_MAX < bytes_to_read) {
				readlim = INT_MAX;
				continue;
		    }

			/*
			 * This is needed for programs that have signal handlers on
			 * older hosts without SA_RESTART.  It also accommodates
			 * ancient AIX hosts that set errno to EINTR after uncaught
			 * SIGCONT.  See <news:1r77ojINN85n@ftp.UU.NET>
			 * (1993-04-22).
			 */
			if (! SA_RESTART && errno == EINTR) {
				continue;
			}

			return SIZE_MAX;
		}
		bp += nread;
	} while (bp < buflim);

	return bp - buf;
}

void file_block_read(struct file_data *current, size_t size)
{
	if (size && ! current->eof) {
		size_t s = block_read(current->desc, FILE_BUFFER(current) + current->buffered, size);
		if (s == SIZE_MAX) {
			HERROR("failed to block_read\n");
		}
		current->buffered += s;
		current->eof = s < size;
	}
}

#if 0
/* If CHANGES, briefly report that two files differed.
   Return 2 if trouble, CHANGES otherwise.  */
static int briefly_report (int changes, struct file_data const filevec[])
{
	if (changes) {
		char const *label0 = file_label[0] ? file_label[0] : filevec[0].name;
		char const *label1 = file_label[1] ? file_label[1] : filevec[1].name;

		if (brief) {
			HPRINT("Files %s and %s differ\n", label0, label1);
		} else {
		  HPRINT("Binary files %s and %s differ\n", label0, label1);
		  changes = 2;
		}
	}
  return changes;
}
#endif

/* Report the differences of two files.  */
int diff_2_files(struct comparison *cmp)
{
	int changes;
	int f;
	
	/* treat files as binary */
	
	/* Files with different lengths must be different.  */
	if (cmp->file[0].stat.st_size != cmp->file[1].stat.st_size
	    && (cmp->file[0].desc < 0 || S_ISREG (cmp->file[0].stat.st_mode))
	    && (cmp->file[1].desc < 0 || S_ISREG (cmp->file[1].stat.st_mode))) {
		changes = 1;
	} else if(cmp->file[0].desc == cmp->file[1].desc) {
		changes = 0;
	} else {
		/* Scan both files, a buffer at a time, looking for a difference.  */
		
		/* Allocate same-sized buffers for both files.  */
		size_t lcm_max = PTRDIFF_MAX - 1;
		size_t buffer_size = buffer_lcm (STAT_BLOCKSIZE (cmp->file[0].stat), 
	                                   STAT_BLOCKSIZE (cmp->file[1].stat),
	                                   lcm_max);
		for (f = 0; f < 2; f++) {
			cmp->file[f].buffer = realloc (cmp->file[f].buffer, buffer_size);
		}
		
		for (;; cmp->file[0].buffered = cmp->file[1].buffered = 0) {
			/* Read a buffer's worth from both files.  */
			for (f = 0; f < 2; f++) {
				if (0 <= cmp->file[f].desc) {
					file_block_read(&cmp->file[f], buffer_size - cmp->file[f].buffered);
				}
			}

			/* If the buffers differ, the files differ.  */
			if (cmp->file[0].buffered != cmp->file[1].buffered 
			    || memcmp(cmp->file[0].buffer, cmp->file[1].buffer, cmp->file[0].buffered)) {
				changes = 1;
				break;
			}
				
			/* If we reach end of file, the files are the same.  */
			if (cmp->file[0].buffered != buffer_size) {
				changes = 0;
				break;
			}
		}
	}
	//changes = briefly_report (changes, cmp->file);
	if (cmp->file[0].buffer != cmp->file[1].buffer) {
		free (cmp->file[0].buffer);
	}
	free (cmp->file[1].buffer);
	return changes;
}

static int compare_files(struct comparison const *parent, char const *name0, char const *name1)
{
	struct comparison cmp;
	int status = EXIT_SUCCESS;
	char *free0 = NULL;
	char *free1 = NULL;
	register int f;
	int same_files;

	/*
	 * If this is directory comparison, perhaps we have a file
	 * that exists only in one of the directories.
	 * If so, just print a message to that effect.
	 */
	if (!(name0 && name1)) {
		char const *name = name0 ? name0 : name1;
		char const *dir = parent->file[!name0].name;
		
		HPRINT("Only in %s: %s\n", dir, name);
		return EXIT_FAILURE;
	}

	memset (cmp.file, 0, sizeof cmp.file);
	cmp.parent = parent;

  /* cmp.file[f].desc markers */
#define NONEXISTENT (-1) /* nonexistent file */
#define UNOPENED (-2) /* unopened file (e.g. directory) */
#define ERRNO_ENCODE(errno) (-3 - (errno)) /* encoded errno value */
#define ERRNO_DECODE(desc) (-3 - (desc)) /* inverse of ERRNO_ENCODE */

#define DIR_P(f) (S_ISDIR (cmp.file[f].stat.st_mode) != 0)

	cmp.file[0].desc = UNOPENED;
	cmp.file[1].desc = UNOPENED;
	
	if (!parent) {
		free0 = NULL;
		free1 = NULL;
		cmp.file[0].name = name0;
		cmp.file[1].name = name1;
	} else {
		cmp.file[0].name = free0 = dir_file_pathname(parent->file[0].name, name0);
		cmp.file[1].name = free1 = dir_file_pathname(parent->file[1].name, name1);
	}
	
	/* Stat the files.  */
	for (f = 0; f < 2; f++) {
		if (f && file_name_cmp(cmp.file[f].name, cmp.file[0].name) == 0) {
			cmp.file[f].desc = cmp.file[0].desc;
			cmp.file[f].stat = cmp.file[0].stat;
		} else if (stat (cmp.file[f].name, &cmp.file[f].stat) != 0) {
			cmp.file[f].desc = ERRNO_ENCODE (errno);
		}
	}
	
	for (f = 0; f < 2; f++) {
		int e = ERRNO_DECODE (cmp.file[f].desc);
		if (0 <= e) {
			errno = e;
			HERROR("fail to stat %s: %s", cmp.file[f].name, strerror(e));
			status = EXIT_TROUBLE;
		}
	}
	
	if (status == EXIT_SUCCESS && !parent && DIR_P (0) != DIR_P (1)) {
		/*
		 * If one is a directory, and it was specified in the command line,
		 * use the file in that dir with the other file's basename.
		 */

		int fnm_arg = DIR_P (0);
		int dir_arg = 1 - fnm_arg;
		char const *fnm = cmp.file[fnm_arg].name;
		char const *dir = cmp.file[dir_arg].name;
		char const *filename = cmp.file[dir_arg].name = free0 = dir_file_pathname (dir, last_component (fnm));

		if (stat(filename, &cmp.file[dir_arg].stat) != 0) {
			HERROR("fail to stat %s: %s", cmp.file[f].name, strerror(errno));
			status = EXIT_TROUBLE;
		}
	}

	if (status != EXIT_SUCCESS) {
		/* One of the files should exist but does not.  */
	} else if ((same_files
	       = (cmp.file[0].desc != NONEXISTENT
	       && cmp.file[1].desc != NONEXISTENT
	       && 0 < same_file(&cmp.file[0].stat, &cmp.file[1].stat)
	       && same_file_attributes(&cmp.file[0].stat, &cmp.file[1].stat)))) {
		/*
		 * The two named files are actually the same physical file.
		 * We know they are identical without actually reading them.
		 */
	} else if (DIR_P (0) & DIR_P (1)){
		if (parent && !recursive) {
			HPRINT("Common subdirectories: %s and %s\n", cmp.file[0].name, cmp.file[1].name);
		} else {
			status = diff_dirs(&cmp, compare_files);
		}
	} else if ((DIR_P (0) | DIR_P (1))
	           || (parent
	               &&(! S_ISREG(cmp.file[0].stat.st_mode)
	                  || ! S_ISREG(cmp.file[1].stat.st_mode)))) {
		/* We have two files that are not to be compared.  */
		HPRINT("File %s is a %s while file %s is a %s\n", cmp.file[0].name, 
		       file_type(&cmp.file[0].stat), cmp.file[1].name,
		       file_type(&cmp.file[1].stat));
		/* This is a difference.  */
		status = EXIT_FAILURE;
	} else if (S_ISREG (cmp.file[0].stat.st_mode)
	           && S_ISREG (cmp.file[1].stat.st_mode)
	           && cmp.file[0].stat.st_size != cmp.file[1].stat.st_size){
	  HPRINT("File %s and %s differ\n", cmp.file[0].name, cmp.file[1].name);
	  status = EXIT_FAILURE;
	} else {
		/* Both exist and neither is a directory.  */
		
		/* Open the files and record their descriptors.  */
		int oflags = O_RDONLY;
		if (cmp.file[0].desc == UNOPENED) {
			if ((cmp.file[0].desc = open(cmp.file[0].name, oflags, 0)) < 0) {
				HERROR("failed to open file %s: %s\n", cmp.file[0].name, strerror(errno));
				status = EXIT_TROUBLE;
			}
		}
		
		if (cmp.file[1].desc == UNOPENED) {
			if ((cmp.file[1].desc = open(cmp.file[1].name, oflags, 0)) < 0) {
				HERROR("failed to open file %s: %s\n", cmp.file[1].name, strerror(errno));
				status = EXIT_TROUBLE;
			}
		}
	
		/* Compare the files, if no error was found.  */
		if (status == EXIT_SUCCESS) {
			status = diff_2_files(&cmp);
			/* repair two file */
			if (status == EXIT_FAILURE) {
				HPRINT("File %s and %s differ\n", cmp.file[0].name, cmp.file[1].name);
				//repair_file();
			}
		}
			
			/* Close the file descriptors.  */
		if (0 <= cmp.file[0].desc && close(cmp.file[0].desc) != 0) {
			HERROR("failed to close %s: %s\n", cmp.file[0].name, strerror(errno));
			status = EXIT_TROUBLE;
		}
		
		if (0 <= cmp.file[1].desc && close(cmp.file[1].desc) != 0) {
			HERROR("failed to close %s: %s\n", cmp.file[1].name, strerror(errno));
			status = EXIT_TROUBLE;
		}
	}
	/*
	 * Now the comparison has been done, if no error prevented it,
	 * and STATUS is the value this function will return.
	 */

	if (status == EXIT_SUCCESS) {
	} else {
		/*
		 * Flush stdout so that the user sees differences immediately.
		 * This can hurt performance, unfortunately.
		 */
		if (fflush (stdout) != 0) {
			HERROR("failed to fflush stdout: %s\n", strerror(errno));
		}
	}

	free (free0);
	free (free1);
  
	return status;
}

int hrfs_api_repair(char *path, hrfs_param_t *param)
{
	int ret = 0;
	struct dirdata dirdata;
	struct file_data dir;
	
	dir.name = path;

	if (unlikely(dir_read(&dir, &dirdata) < 0)) {
		HERROR("failed to read directory %s", path);
		goto out;
	}

	dump_dirdata(&dirdata);
	
	free(dirdata.names);
	free(dirdata.data);
out:
	return ret;
}

int hrfs_api_diff(char const *name0, char const *name1, hrfs_param_t *param)
{
	int ret = 0;
	if (param->recursive) {
		recursive = true;
	} else {
		recursive = false;
	}

	if (param->verbose) {
		verbose = true;
	} else {
		verbose = false;
	}	

	ret = compare_files(NULL, name0, name1);
	//HPRINT("ret = %d\n", ret);
	return ret;
}
