/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#include "cp.h"
#include <debug.h>
#include "exit_status.h"
#include "libcoreutils/dirname.h"
#include "libcoreutils/file-set.h"
#include "libcoreutils/hash-triple.h"
#include "libcoreutils/same-inode.h"
#include "libcoreutils/same.h"
#include "libcoreutils/cp-hash.h"
#include "libcoreutils/stat-macros.h"
#include "libcoreutils/savedir.h"
#include "libcoreutils/buffer-lcm.h"
#include "libcoreutils/xalloc.h"
#include "libcoreutils/full-write.h"
#include "libcoreutils/stat-time.h"
#include "libcoreutils/utimens.h"
#include "libcoreutils/ignore-value.h"
#include "libcoreutils/acl.h"
#include "libcoreutils/areadlink.h"
#include <attr/error_context.h>
#include <attr/libattr.h>

struct dir_list
{
	struct dir_list *parent;
	ino_t ino;
	dev_t dev;
};

#define SAME_OWNER(A, B) ((A).st_uid == (B).st_uid)
#define SAME_GROUP(A, B) ((A).st_gid == (B).st_gid)
#define SAME_OWNER_AND_GROUP(A, B) (SAME_OWNER (A, B) && SAME_GROUP (A, B))

# define XSTAT(X, Src_name, Src_sb) lstat(Src_name, Src_sb)

/* Initial size of the cp.dest_info hash table.  */
#define DEST_INFO_INITIAL_CAPACITY 61

/* Initialize the hash table implementing a set of F_triple entries
   corresponding to destination files.  */

#define ASSIGN_STRDUPA(DEST, S)		\
  do { DEST = strdupa (S); } while (0)

#define ASSIGN_BASENAME_STRDUPA(Dest, File_name)	\
  do							\
    {							\
      char *tmp_abns_;					\
      ASSIGN_STRDUPA (tmp_abns_, (File_name));		\
      Dest = last_component (tmp_abns_);		\
      strip_trailing_slashes (Dest);			\
    }							\
  while (0)

#define S_IRWXUGO (S_IRWXU | S_IRWXG | S_IRWXO)

#ifndef O_BINARY
# define O_BINARY 0
# define O_TEXT 0
#endif

/* Some systems, like Sequents, return st_blksize of 0 on pipes.
   Also, when running `rsh hpux11-system cat any-file', cat would
   determine that the output stream had an st_blksize of 2147421096.
   Conversely st_blksize can be 2 GiB (or maybe even larger) with XFS
   on 64-bit hosts.  Somewhat arbitrarily, limit the `optimal' block
   size to SIZE_MAX / 8 + 1.  (Dividing SIZE_MAX by only 4 wouldn't
   suffice, since "cat" sometimes multiplies the result by 4.)  If
   anyone knows of a system for which this limit is too small, please
   report it as a bug in this code.  */
#define ST_BLKSIZE(statbuf) ((0 < (statbuf).st_blksize \
                               && (statbuf).st_blksize <= SIZE_MAX / 8 + 1) \
                              ? (statbuf).st_blksize : DEV_BSIZE)

#define ST_NBLOCKS(statbuf) ((statbuf).st_blocks)
/* include here for SIZE_MAX.  */
#include <inttypes.h>

/*
 * Get or fake the disk device blocksize.
 * Usually defined by sys/param.h (if at all).\
 */
#if !defined DEV_BSIZE && defined BSIZE
# define DEV_BSIZE BSIZE
#endif
#if !defined DEV_BSIZE && defined BBSIZE /* SGI */
# define DEV_BSIZE BBSIZE
#endif
#ifndef DEV_BSIZE
# define DEV_BSIZE 4096
#endif

#ifndef ST_NBLOCKSIZE
#define ST_NBLOCKSIZE S_BLKSIZE
#endif

static bool copy_internal(char const *src_name, char const *dst_name,
               bool new_dst,
               dev_t device,
               struct dir_list *ancestors,
               const struct cp_options *x,
               bool command_line_arg,
               bool *first_dir_created_per_command_line_arg,
               bool *copy_into_self);

/* FILE is the last operand of this command.
   Return true if FILE is a directory.
   But report an error and exit if there is a problem accessing FILE,
   or if FILE does not exist but would have to refer to an existing
   directory if it referred to anything at all.

   If the file exists, store the file's status into *ST.
   Otherwise, set *NEW_DST.  */

static bool target_directory_operand(char const *file, struct stat *st, bool *new_dst)
{
	int err = (stat (file, st) == 0 ? 0 : errno);
	bool is_a_dir = !err && S_ISDIR (st->st_mode);

	if (err) {
		if (err != ENOENT) {
        	HERROR("failed to access %s: %s", file, strerror(err));
        }
		*new_dst = true;
    }
	return is_a_dir;
}

#define FILE_SYSTEM_PREFIX_LEN(Filename) 0

/* Return the longest suffix of F that is a relative file name.
   If it has no such suffix, return the empty string.  */

static char const *longest_relative_suffix(char const *f)
{
	for (f += FILE_SYSTEM_PREFIX_LEN (f); ISSLASH (*f); f++) {
		continue;
	}
	return f;
}

extern bool
chown_failure_ok (struct cp_options const *x)
{
	/*
	 * If non-root uses -p, it's ok if we can't preserve ownership.
	 * But root probably wants to know, e.g. if NFS disallows it,
	 * or if the target system doesn't support file ownership.
	 */

	return ((errno == EPERM || errno == EINVAL) && !x->chown_privileges);
}

/*
 * Concatenate two file name components, DIR and ABASE, in
 * newly-allocated storage and return the result.
 * The resulting file name F is such that the commands "ls F" and "(cd
 * DIR; ls BASE)" refer to the same file, where BASE is ABASE with any
 * file system prefixes and leading separators removed.
 * Arrange for a directory separator if necessary between DIR and BASE
 * in the result, removing any redundant separators.
 * In any case, if BASE_IN_RESULT is non-NULL, set
 * *BASE_IN_RESULT to point to the copy of ABASE in the returned
 * concatenation.  However, if ABASE begins with more than one slash,
 * set *BASE_IN_RESULT to point to the sole corresponding slash that
 * is copied into the result buffer.
 * Return NULL if malloc fails.
 */

char *mfile_name_concat(char const *dir, char const *abase, char **base_in_result)
{
	char const *dirbase = last_component (dir);
	size_t dirbaselen = base_len (dirbase);
	size_t dirlen = dirbase - dir + dirbaselen;
	size_t needs_separator = (dirbaselen && ! ISSLASH (dirbase[dirbaselen - 1]));

	char const *base = longest_relative_suffix (abase);
	size_t baselen = strlen (base);

	char *p_concat = malloc(dirlen + needs_separator + baselen + 1);
	char *p;

	if (p_concat == NULL) {
		return NULL;
	}

	p = mempcpy (p_concat, dir, dirlen);
	*p = DIRECTORY_SEPARATOR;
	p += needs_separator;

	if (base_in_result) {
		*base_in_result = p - IS_ABSOLUTE_FILE_NAME (abase);
	}

	p = mempcpy (p, base, baselen);
	*p = '\0';

	return p_concat;
}

#define STREQ(a, b) (strcmp (a, b) == 0)
#define xstrdup strdup

char *file_name_concat(char const *dir, char const *abase, char **base_in_result)
{
	char *p = mfile_name_concat (dir, abase, base_in_result);
	return p;
}

/* Set *X to the default options for a value of type struct cp_options.  */

extern void
cp_options_default (struct cp_options *x)
{
	memset (x, 0, sizeof *x);
	x->chown_privileges = x->owner_privileges = (geteuid () == 0);
}

void cp_option_init(struct cp_options *cp_options)
{
	cp_options->recursive = 0;

	cp_options->preserve_ownership = 0;
	cp_options->preserve_mode = 0;
	cp_options->preserve_timestamps = 0;
	cp_options->require_preserve = 0;
	
	cp_options->copy_as_regular = 1;
	
	cp_options->verbose = 0;
}

extern void
dest_info_init (struct cp_options *x)
{
  x->dest_info
    = hash_initialize (DEST_INFO_INITIAL_CAPACITY,
                       NULL,
                       triple_hash,
                       triple_compare,
                       triple_free);
}

/* Initialize the hash table implementing a set of F_triple entries
   corresponding to source files listed on the command line.  */
extern void src_info_init (struct cp_options *x)
{

  /* Note that we use triple_hash_no_name here.
     Contrast with the use of triple_hash above.
     That is necessary because a source file may be specified
     in many different ways.  We want to warn about this
       cp a a d/
     as well as this:
       cp a ./a d/
  */
  x->src_info
    = hash_initialize (DEST_INFO_INITIAL_CAPACITY,
                       NULL,
                       triple_hash_no_name,
                       triple_compare,
                       triple_free);
}

/*
 * Return true if it's ok that the source and destination
 * files are the `same' by some measure.  The goal is to avoid
 * making the `copy' operation remove both copies of the file
 * in that case, while still allowing the user to e.g., move or
 * copy a regular file onto a symlink that points to it.
 * Try to minimize the cost of this function in the common case.
 * Set *RETURN_NOW if we've determined that the caller has no more
 * work to do and should return successfully, right away.

 * Set *UNLINK_SRC if we've determined that the caller wants to do
 * `rename (a, b)' where `a' and `b' are distinct hard links to the same
 * file. In that case, the caller should try to unlink `a' and then return
 * successfully.  Ideally, we wouldn't have to do that, and we'd be
 * able to rely on rename to remove the source file.  However, POSIX
 * mistakenly requires that such a rename call do *nothing* and return
 * successfully.
 */

static bool
same_file_ok(char const *src_name, struct stat const *src_sb,
             char const *dst_name, struct stat const *dst_sb,
             const struct cp_options *x, bool *return_now, bool *unlink_src)
{
	const struct stat *src_sb_link;
	const struct stat *dst_sb_link;
	struct stat tmp_dst_sb;
	struct stat tmp_src_sb;

	bool same_link;
	bool same = SAME_INODE (*src_sb, *dst_sb);

	*return_now = false;
	*unlink_src = false;

	same_link = same;

	/*
	 * If both the source and destination files are symlinks (and we'll
	 * know this here IFF preserving symlinks), then it's ok -- as long
	 * as they are distinct.
	 */
	if (S_ISLNK (src_sb->st_mode) && S_ISLNK (dst_sb->st_mode)) {
		return !same_name (src_name, dst_name);
	}
	src_sb_link = src_sb;
	dst_sb_link = dst_sb;

	/*
	 * If neither is a symlink, then it's ok as long as they aren't
	 * hard links to the same file.
	 */
	if (!S_ISLNK (src_sb_link->st_mode) && !S_ISLNK (dst_sb_link->st_mode)) {
		if (!SAME_INODE (*src_sb_link, *dst_sb_link)) {
			return true;
		}
	}

	if ( !S_ISLNK(src_sb_link->st_mode)) {
		tmp_src_sb = *src_sb_link;
	} else if (stat(src_name, &tmp_src_sb) != 0) {
		return true;
	}
	
	if ( !S_ISLNK(dst_sb_link->st_mode)) {
		tmp_dst_sb = *dst_sb_link;
	} else if (stat(dst_name, &tmp_dst_sb) != 0) {
		return true;
	}

	if ( !SAME_INODE(tmp_src_sb, tmp_dst_sb)) {
		return true;
	}
	return false;
}

/* FIXME: describe */
/* FIXME: rewrite this to use a hash table so we avoid the quadratic
   performance hit that's probably noticeable only on trees deeper
   than a few hundred levels.  See use of active_dir_map in remove.c  */

static bool is_ancestor(const struct stat *sb, const struct dir_list *ancestors)
{
	while (ancestors != 0) {
		if (ancestors->ino == sb->st_ino && ancestors->dev == sb->st_dev) {
			return true;
		}
		ancestors = ancestors->parent;
	}
  return false;
}

/*
 * Print --verbose output on standard output, e.g. `new' -> `old'.
 * If BACKUP_DST_NAME is non-NULL, then also indicate that it is
 * the name of a backup file.
 */
static void emit_verbose(char const *src, char const *dst)
{
  HDEBUG ("%s -> %s\n", src, dst);
}

/*
 * Read the contents of the directory SRC_NAME_IN, and recursively
 * copy the contents to DST_NAME_IN.  NEW_DST is true if
 * DST_NAME_IN is a directory that was created previously in the
 * recursion.   SRC_SB and ANCESTORS describe SRC_NAME_IN.
 * Set *COPY_INTO_SELF if SRC_NAME_IN is a parent of
 * FIRST_DIR_CREATED_PER_COMMAND_LINE_ARG  FIXME
 * (or the same as) DST_NAME_IN; otherwise, clear it.
 * Return true if successful.
 */

static bool copy_dir(char const *src_name_in, char const *dst_name_in, bool new_dst,
          const struct stat *src_sb, struct dir_list *ancestors,
          const struct cp_options *x,
          bool *first_dir_created_per_command_line_arg,
          bool *copy_into_self)
{
	char *name_space;
	char *namep;
	struct cp_options non_command_line_options = *x;
	bool ok = true;

	name_space = savedir(src_name_in);
	if (name_space == NULL) {
		/*
		 * This diagnostic is a bit vague because savedir can fail in
		 * several different ways.
		 */
		HERROR("cannot access %s", src_name_in);
		return false;
    }

	namep = name_space;
	while (*namep != '\0') {
		bool local_copy_into_self;
		char *src_name = file_name_concat (src_name_in, namep, NULL);
		char *dst_name = file_name_concat (dst_name_in, namep, NULL);

		ok &= copy_internal(src_name, dst_name, new_dst, src_sb->st_dev,
		                    ancestors, &non_command_line_options, false,
		                    first_dir_created_per_command_line_arg,
		                    &local_copy_into_self);
		*copy_into_self |= local_copy_into_self;

		free (dst_name);
		free (src_name);

		/*
		* If we're copying into self, there's no point in continuing,
		* and in fact, that would even infloop, now that we record only
		* the first created directory per command line argument.  */
		if (local_copy_into_self) {
			break;
		}

		namep += strlen (namep) + 1;
	}
	free (name_space);
	return ok;
}

/* Compute the greatest common divisor of U and V using Euclid's
   algorithm.  U and V must be nonzero.  */

static inline size_t
gcd (size_t u, size_t v)
{
  do
    {
      size_t t = u % v;
      u = v;
      v = t;
    }
  while (v);

  return u;
}

/* Compute the least common multiple of U and V.  U and V must be
   nonzero.  There is no overflow checking, so callers should not
   specify outlandish sizes.  */

static inline size_t
lcm (size_t u, size_t v)
{
  return u * (v / gcd (u, v));
}

/* Return PTR, aligned upward to the next multiple of ALIGNMENT.
   ALIGNMENT must be nonzero.  The caller must arrange for ((char *)
   PTR) through ((char *) PTR + ALIGNMENT - 1) to be addressable
   locations.  */

static inline void *
ptr_align (void const *ptr, size_t alignment)
{
  char const *p0 = ptr;
  char const *p1 = p0 + alignment - 1;
  return (void *) (p1 - (size_t) p1 % alignment);
}

/* As of Mar 2009, 32KiB is determined to be the minimium
   blksize to best minimize system call overhead.
   This can be tested with this script with the results
   shown for a 1.7GHz pentium-m with 2GB of 400MHz DDR2 RAM:

   for i in $(seq 0 10); do
     size=$((8*1024**3)) #ensure this is big enough
     bs=$((1024*2**$i))
     printf "%7s=" $bs
     dd bs=$bs if=/dev/zero of=/dev/null count=$(($size/$bs)) 2>&1 |
     sed -n 's/.* \([0-9.]* [GM]B\/s\)/\1/p'
   done

      1024=734 MB/s
      2048=1.3 GB/s
      4096=2.4 GB/s
      8192=3.5 GB/s
     16384=3.9 GB/s
     32768=5.2 GB/s
     65536=5.3 GB/s
    131072=5.5 GB/s
    262144=5.7 GB/s
    524288=5.7 GB/s
   1048576=5.8 GB/s

   Note that this is to minimize system call overhead.
   Other values may be appropriate to minimize file system
   or disk overhead.  For example on my current GNU/Linux system
   the readahead setting is 128KiB which was read using:

   file="."
   device=$(df -P --local "$file" | tail -n1 | cut -d' ' -f1)
   echo $(( $(blockdev --getra $device) * 512 ))

   However there isn't a portable way to get the above.
   In the future we could use the above method if available
   and default to io_blksize() if not.
 */
enum { IO_BUFSIZE = 32*1024 };
static inline size_t
io_blksize (struct stat sb)
{
  return MAX (IO_BUFSIZE, ST_BLKSIZE (sb));
}

static void
set_author (const char *dst_name, int dest_desc, const struct stat *src_sb)
{
  (void) dst_name;
  (void) dest_desc;
  (void) src_sb;
}

int
lchmod (char const *f, mode_t m)
{
  errno = ENOSYS;
  return -1;
}

/* Change the file mode bits of the file identified by DESC or NAME to MODE.
   Use DESC if DESC is valid and fchmod is available, NAME otherwise.  */

static int
fchmod_or_lchmod (int desc, char const *name, mode_t mode)
{
	if (0 <= desc) {
		return fchmod (desc, mode);
	}
	return lchmod (name, mode);
}

static bool
owner_failure_ok (struct cp_options const *x)
{
  return ((errno == EPERM || errno == EINVAL) && !x->owner_privileges);
}

/* Set the owner and owning group of DEST_DESC to the st_uid and
 * st_gid fields of SRC_SB.  If DEST_DESC is undefined (-1), set
 * the owner and owning group of DST_NAME instead; for
 * safety prefer lchown if the system supports it since no
 * symbolic links should be involved.  DEST_DESC must
 * refer to the same file as DEST_NAME if defined.
 * Upon failure to set both UID and GID, try to set only the GID.
 * NEW_DST is true if the file was newly created; otherwise,
 * DST_SB is the status of the destination.
 * Return 1 if the initial syscall succeeds, 0 if it fails but it's OK
 * not to preserve ownership, -1 otherwise.
 */
static int
set_owner (const struct cp_options *x, char const *dst_name, int dest_desc,
           struct stat const *src_sb, bool new_dst,
           struct stat const *dst_sb)
{
	uid_t uid = src_sb->st_uid;
	gid_t gid = src_sb->st_gid;

	/*
	 * Naively changing the ownership of an already-existing file before
	 * changing its permissions would create a window of vulnerability if
	 * the file's old permissions are too generous for the new owner and
	 * group.  Avoid the window by first changing to a restrictive
	 * temporary mode if necessary.
	 */

	if (!new_dst && (x->preserve_mode)) {
		mode_t old_mode = dst_sb->st_mode;
		mode_t new_mode = src_sb->st_mode;
		mode_t restrictive_temp_mode = old_mode & new_mode & S_IRWXU;
		
		if (qset_acl (dst_name, dest_desc, restrictive_temp_mode) != 0) {
			if (! owner_failure_ok (x)) {
				HERROR("clearing permissions for %s: %s\n", dst_name, strerror(errno));
				return -x->require_preserve;
			}
		}
    }
    
    if (dest_desc != -1) {
    	if (fchown (dest_desc, uid, gid) == 0) {
    		return 1;
    	}
    	if (errno == EPERM || errno == EINVAL) {
			/*
			 * We've failed to set *both*.  Now, try to set just the group
			 * ID, but ignore any failure here, and don't change errno.
			 */
			int saved_errno = errno;
			ignore_value (fchown (dest_desc, -1, gid));
			errno = saved_errno;
    	}
    } else {
		if (lchown (dst_name, uid, gid) == 0) {
			return 1;
		}
		if (errno == EPERM || errno == EINVAL) {
			/*
			 * We've failed to set *both*.  Now, try to set just the group
			 * ID, but ignore any failure here, and don't change errno.
			 */
			int saved_errno = errno;
			ignore_value (lchown (dst_name, -1, gid));
			errno = saved_errno;
		}
	}

	if (! chown_failure_ok (x)) {
		HERROR("failed to preserve ownership for %s: %s\n", dst_name, strerror(errno));
		if (x->require_preserve) {
			return -1;
		}
    }

	return 0;
}

# define ATTRIBUTE_UNUSED __attribute__ ((__unused__))

static void
copy_attr_allerror (struct error_context *ctx ATTRIBUTE_UNUSED,
                 char const *fmt, ...)
{
	int err = errno;
	va_list ap;

	/* use verror module to print error message */
	va_start (ap, fmt);
	HERROR("copy_attr_allerror failed: %s\n", strerror(err));
	//verror (0, err, fmt, ap);
	va_end (ap);
}

static char const *
copy_attr_quote (struct error_context *ctx ATTRIBUTE_UNUSED, char const *str)
{
  return str;
}

static void
copy_attr_free (struct error_context *ctx ATTRIBUTE_UNUSED,
                char const *str ATTRIBUTE_UNUSED)
{
}

/* If positive SRC_FD and DST_FD descriptors are passed,
   then copy by fd, otherwise copy by name.  */

static bool
copy_attr (char const *src_path, int src_fd,
           char const *dst_path, int dst_fd, struct cp_options const *x)
{
	int ret;
	bool all_errors = false;
	bool some_errors = (!all_errors);
	struct error_context ctx =
	{
		.error = all_errors ? copy_attr_allerror : copy_attr_allerror,
		.quote = copy_attr_quote,
		.quote_free = copy_attr_free
	};
	if (0 <= src_fd && 0 <= dst_fd)
		ret = attr_copy_fd (src_path, src_fd, dst_path, dst_fd, 0,
		         (all_errors || some_errors ? &ctx : NULL));
	else
		ret = attr_copy_file (src_path, dst_path, 0,
	            (all_errors || some_errors ? &ctx : NULL));

	return ret == 0;
}

/* Return the user's umask, caching the result.  */

extern mode_t
cached_umask (void)
{
  static mode_t mask = (mode_t) -1;
  if (mask == (mode_t) -1)
    {
      mask = umask (0);
      umask (mask);
    }
  return mask;
}


/*
 * Copy a regular file from SRC_NAME to DST_NAME.
 * If the source file contains holes, copies holes and blocks of zeros
 * in the source file as holes in the destination file.
 * (Holes are read as zeroes by the `read' system call.)
 * When creating the destination, use DST_MODE & ~OMITTED_PERMISSIONS
 * as the third argument in the call to open, adding
 * OMITTED_PERMISSIONS after copying as needed.
 * X provides many option settings.
 * Return true if successful.
 * *NEW_DST is as in copy_internal.
 * SRC_SB is the result of calling XSTAT (aka stat) on SRC_NAME.
 */
static bool
copy_reg(char const *src_name, char const *dst_name,
         const struct cp_options *x,
         mode_t dst_mode, mode_t omitted_permissions, bool *new_dst,
         struct stat const *src_sb)
{
	char *buf;
	char *buf_alloc = NULL;
	char *name_alloc = NULL;
	int dest_desc;
	int dest_errno;
	int source_desc;
	mode_t src_mode = src_sb->st_mode;
	struct stat sb;
	struct stat src_open_sb;
	bool return_val = true;
	bool data_copy_required = true;

	source_desc = open (src_name,
	                    (O_RDONLY | O_BINARY | O_NOFOLLOW ));
	if (source_desc < 0) {
		HERROR("cannot open %s for reading: %s\n", src_name, strerror(errno));
		return false;
	}
	
	if (fstat(source_desc, &src_open_sb) != 0) {
		HERROR("cannot fstat %s: %s\n", src_name, strerror(errno));
		return_val = false;
		goto close_src_desc;
	}
	/*
	 * Compare the source dev/ino from the open file to the incoming,
	 * saved ones obtained via a previous call to stat.
	 */
	if (! SAME_INODE(*src_sb, src_open_sb)) {
		HERROR("skipping file %s, as it was replaced while being copied\n", src_name);
		return_val = false;
		goto close_src_desc;
	}
	/*
	 * The semantics of the following open calls are mandated
	 * by the specs for both cp and mv.
	 */
	if (! *new_dst) {
		dest_desc = open (dst_name, O_WRONLY | O_TRUNC | O_BINARY);
		dest_errno = errno;
		/*
		 * When using cp --preserve=context to copy to an existing destination,
		 * use the default context rather than that of the source.  Why?
		 * 1) the src context may prohibit writing, and
		 * 2) because it's more consistent to use the same context
		 * that is used when the destination file doesn't already exist.
		 */
	}
	if (*new_dst) {
		int open_flags = O_WRONLY | O_CREAT | O_BINARY;
		dest_desc = open (dst_name, open_flags | O_EXCL,
		                  dst_mode & ~omitted_permissions);
		dest_errno = errno;
		/*
		 * When trying to copy through a dangling destination symlink,
		 * the above open fails with EEXIST.  If that happens, and
		 * lstat'ing the DST_NAME shows that it is a symlink, then we
		 * have a problem: trying to resolve this dangling symlink to
		 * a directory/destination-entry pair is fundamentally racy,
		 * so punt.  If x->open_dangling_dest_symlink is set (cp sets
		 * that when POSIXLY_CORRECT is set in the environment), simply
		 * call open again, but without O_EXCL (potentially dangerous).
		 * If not, fail with a diagnostic.  These shenanigans are necessary
		 * only when copying, i.e., not in move_mode.
		 */
		if (dest_desc < 0 && dest_errno == EEXIST) {
			struct stat dangling_link_sb;
			if (lstat (dst_name, &dangling_link_sb) == 0
			    && S_ISLNK (dangling_link_sb.st_mode)) {
				HERROR("not writing through dangling symlink %s\n", dst_name);
				return_val = false;
				goto close_src_desc;
			}	
		}
		if (dest_desc < 0 && dest_errno == EISDIR
		    && *dst_name && dst_name[strlen (dst_name) - 1] == '/') {
			dest_errno = ENOTDIR;
		}
	} else {
		omitted_permissions = 0;
	}
	if (dest_desc < 0) {
		HERROR("cannot create regular file %s: %s\n", dst_name, strerror(dest_errno));
		return_val = false;
		goto close_src_desc;
	}
	
	if (fstat (dest_desc, &sb) != 0) {
		HERROR("cannot fstat %s: %s\n", dst_name, strerror(dest_errno));
		return_val = false;
		goto close_src_and_dst_desc;
	}

	if (data_copy_required) {
		typedef uintptr_t word;
		off_t n_read_total = 0;

		/* Choose a suitable buffer size; it may be adjusted later.  */
		size_t buf_alignment = lcm (getpagesize (), sizeof (word));
		size_t buf_alignment_slop = sizeof (word) + buf_alignment - 1;
		size_t buf_size = io_blksize (sb);

		/* Deal with sparse files.  */
		bool last_write_made_hole = false;
		bool make_holes = false;

		if (S_ISREG (sb.st_mode)) {
			/* Use a heuristic to determine whether SRC_NAME contains any sparse
			 * blocks.  If the file has fewer blocks than would normally be
			 * needed for a file of its size, then at least one of the blocks in
			 * the file is a hole.
			 */
			if (S_ISREG (src_open_sb.st_mode)
				&& ST_NBLOCKS (src_open_sb) < src_open_sb.st_size / ST_NBLOCKSIZE) {
				make_holes = true;
			}
		}

		/*
		 * If not making a sparse file, try to use a more-efficient
		 * buffer size.
		 */
		if (! make_holes) {
			/*
			 * Compute the least common multiple of the input and output
			 * buffer sizes, adjusting for outlandish values.
			 */
			size_t blcm_max = MIN (SIZE_MAX, SSIZE_MAX) - buf_alignment_slop;
			size_t blcm = buffer_lcm (io_blksize (src_open_sb), buf_size,
			                          blcm_max);
			/*
			 * Do not bother with a buffer larger than the input file, plus one
			 * byte to make sure the file has not grown while reading it.
			 */
			if (S_ISREG (src_open_sb.st_mode) && src_open_sb.st_size < buf_size) {
				buf_size = src_open_sb.st_size + 1;
			}

			/*
			 * However, stick with a block size that is a positive multiple of
			 * blcm, overriding the above adjustments.  Watch out for
			 * overflow.
			 */
			buf_size += blcm - 1;
			buf_size -= buf_size % blcm;
			if (buf_size == 0 || blcm_max < buf_size) {
				buf_size = blcm;
			}
		}

		/* Make a buffer with space for a sentinel at the end.  */
		buf_alloc = xmalloc (buf_size + buf_alignment_slop);
		buf = ptr_align (buf_alloc, buf_alignment);
		while (true) {
			word *wp = NULL;
			ssize_t n_read = read (source_desc, buf, buf_size);

			if (n_read < 0) {
#ifdef EINTR
				if (errno == EINTR) {
					continue;
				}
#endif
				HERROR("reading %s: %s\n", src_name, strerror(errno));
				return_val = false;
				goto close_src_and_dst_desc;
			}
			if (n_read == 0) {
				break;
			}

			n_read_total += n_read;

			if (make_holes) {
				char *cp;

				/* Sentinel to stop loop.  */
				buf[n_read] = '\1';
				/*
				 * Usually, buf[n_read] is not the byte just before a "word"
				 * (aka uintptr_t) boundary.  In that case, the word-oriented
				 * test below (*wp++ == 0) would read some uninitialized bytes
				 * after the sentinel.  To avoid false-positive reports about
				 * this condition (e.g., from a tool like valgrind), set the
				 * remaining bytes -- to any value.
				 */
				memset (buf + n_read + 1, 0, sizeof (word) - 1);

				/* Find first nonzero *word*, or the word with the sentinel.  */
				wp = (word *) buf;
				while (*wp++ == 0) {
					continue;
				}

				/* Find the first nonzero *byte*, or the sentinel.  */
				cp = (char *) (wp - 1);
				while (*cp++ == 0) {
					continue;
				}

				if (cp <= buf + n_read) {
					/* Clear to indicate that a normal write is needed. */
					wp = NULL;
				} else {
					/*
					 *  We found the sentinel, so the whole input block was zero.
                     * Make a hole.
                     */
                    if (lseek (dest_desc, n_read, SEEK_CUR) < 0) {
                    	HERROR("cannot lseek %s: %s\n", dst_name, strerror(errno));
                    	return_val = false;
                    	goto close_src_and_dst_desc;
                    }
                    last_write_made_hole = true;
				}
			}
			if (!wp) {
				size_t n = n_read;
				if (full_write (dest_desc, buf, n) != n) {
					HERROR("writing %s: %s\n", dst_name, strerror(errno));
					return_val = false;
					goto close_src_and_dst_desc;
				}
				last_write_made_hole = false;
				/* It is tempting to return early here upon a short read from a
				 * regular file.  That would save the final read syscall for each
				 * file.  Unfortunately that doesn't work for certain files in
				 * /proc with linux kernels from at least 2.6.9 .. 2.6.29.
				 */
			}
		}
		/*
		 * If the file ends with a `hole', we need to do something to record
		 * the length of the file.  On modern systems, calling ftruncate does
		 * the job.  On systems without native ftruncate support, we have to
		 * write a byte at the ending position.  Otherwise the kernel would
		 * truncate the file at the end of the last write operation.
		 */
		if (last_write_made_hole) {
			if (ftruncate (dest_desc, n_read_total) < 0) {
				HERROR("truncating %s: %s\n", dst_name, strerror(errno));
				return_val = false;
				goto close_src_and_dst_desc;
			}
        }
	}
	if (x->preserve_timestamps) {
		struct timespec timespec[2];

		timespec[0] = get_stat_atime (src_sb);
		timespec[1] = get_stat_mtime (src_sb);
		if (fdutimens (dest_desc, dst_name, timespec) != 0) {
			HERROR("preserving times for %s: %s\n", dst_name, strerror(errno));
			if (x->require_preserve) {
				return_val = false;
				goto close_src_and_dst_desc;
			}
		}
	}
	/*
	 * Set ownership before xattrs as changing owners will
	 * clear capabilities.
	 */
	if (x->preserve_ownership && ! SAME_OWNER_AND_GROUP (*src_sb, sb)) {
		switch (set_owner (x, dst_name, dest_desc, src_sb, *new_dst, &sb)) {
		case -1:
			return_val = false;
			goto close_src_and_dst_desc;

		case 0:
			src_mode &= ~ (S_ISUID | S_ISGID | S_ISVTX);
			break;
        }
    }

	/*
	 * To allow copying xattrs on read-only files, temporarily chmod u+rw.
	 * This workaround is required as an inode permission check is done
	 * by xattr_permission() in fs/xattr.c of the GNU/Linux kernel tree.
	 */
	if (x->preserve_xattr) {
		bool access_changed = false;
		int ret = 0;

		if (!(sb.st_mode & S_IWUSR) && geteuid () != 0) {
			access_changed = fchmod_or_lchmod (dest_desc, dst_name, 0600) == 0;
		}

		ret = copy_attr(src_name, source_desc, dst_name, dest_desc, x);
		if (!ret) {
			HERROR("failed to copy_attr from %s to %s\n", src_name, dst_name);
		}

		if (access_changed) {
			fchmod_or_lchmod (dest_desc, dst_name, dst_mode & ~omitted_permissions);
		}
	}
	set_author (dst_name, dest_desc, src_sb);
	if (x->preserve_mode) {
		if (copy_acl (src_name, source_desc, dst_name, dest_desc, src_mode) != 0
          && x->require_preserve) {
          	return_val = false;
        }
	} else if (omitted_permissions) {
		omitted_permissions &= ~ cached_umask ();
		if (omitted_permissions
		    && fchmod_or_lchmod (dest_desc, dst_name, dst_mode) != 0) {
		    HERROR("preserving permissions for %s: %s\n", dst_name, strerror(errno));
		    if (x->require_preserve) {
		    	return_val = false;
		    }
		}
	}
close_src_and_dst_desc:
	if (close (dest_desc) < 0) {
		HERROR("closing %s: %s\n", dst_name, strerror(errno));
		return_val = false;
	}
close_src_desc:
	if (close (source_desc) < 0) {
		HERROR("closing %s: %s\n", src_name, strerror(errno));
		return_val = false;
	}
	free (buf_alloc);
	free (name_alloc);
	return return_val;
}

/* Set the timestamp of symlink, FILE, to TIMESPEC.
   If this system lacks support for that, simply return 0.  */
static inline int
utimens_symlink (char const *file, struct timespec const *timespec)
{
	int err = lutimens (file, timespec);
	/*
	 * When configuring on a system with new headers and libraries, and
	 * running on one with a kernel that is old enough to lack the syscall,
	 * utimensat fails with ENOSYS.  Ignore that.
	 */
	if (err && errno == ENOSYS) {
		err = 0;
	}
	return err;
}
/*
 * Pointers to the file names:  they're used in the diagnostic that is issued
 * when we detect the user is trying to copy a directory into itself.
 */
static char const *top_level_src_name;
static char const *top_level_dst_name;

/* Copy the file SRC_NAME to the file DST_NAME.  The files may be of
 * any type.  NEW_DST should be true if the file DST_NAME cannot
 * exist because its parent directory was just created; NEW_DST should
 * be false if DST_NAME might already exist.  DEVICE is the device
 * number of the parent directory, or 0 if the parent of this file is
 * not known.  ANCESTORS points to a linked, null terminated list of
 * devices and inodes of parent directories of SRC_NAME.  COMMAND_LINE_ARG
 * is true iff SRC_NAME was specified on the command line.
 * FIRST_DIR_CREATED_PER_COMMAND_LINE_ARG is both input and output.
 * Set *COPY_INTO_SELF if SRC_NAME is a parent of (or the
 * same as) DST_NAME; otherwise, clear it.
 * Return true if successful.
 */
static bool copy_internal(char const *src_name, char const *dst_name,
               bool new_dst,
               dev_t device,
               struct dir_list *ancestors,
               const struct cp_options *x,
               bool command_line_arg,
               bool *first_dir_created_per_command_line_arg,
               bool *copy_into_self)
{
	struct stat src_sb;
	struct stat dst_sb;
	mode_t src_mode;
	mode_t dst_mode = 0;
	mode_t dst_mode_bits;
	mode_t omitted_permissions;
	bool restore_dst_mode = false;
	char *earlier_file = NULL;
	//char *dst_backup = NULL;
	//bool backup_succeeded = false;
	bool delayed_ok;
	bool copied_as_regular = false;
	bool dest_is_symlink = false;
	bool have_dst_lstat = false;

	*copy_into_self = false;
	
	if (XSTAT(x, src_name, &src_sb) != 0) {
		HERROR("failed to stat %s: %s\n", src_name, strerror(errno));
		return false;
    }
    
    src_mode = src_sb.st_mode;

	if (S_ISDIR (src_mode) && !x->recursive) {
		HERROR("omitting directory %s\n", src_name);
		return false;
	}
	
	/*
	 * Detect the case in which the same source file appears more than
	 * once on the command line and no backup option has been selected.
	 * If so, simply warn and don't copy it the second time.
	 * This check is enabled only if x->src_info is non-NULL.
	 */
	if (command_line_arg) {
		if (! S_ISDIR(src_sb.st_mode)
            && seen_file(x->src_info, src_name, &src_sb)) {
			HERROR("warning: source file %s specified more than once\n", src_name);
			return true;
        }
		record_file(x->src_info, src_name, &src_sb);
	}

	if (!new_dst) {
		/*
		 * Regular files can be created by writing through symbolic
		 * links, but other files cannot.  So use stat on the
		 * destination when copying a regular file, and lstat otherwise.
		 * However, if we intend to unlink or remove the destination
		 * first, use lstat, since a copy won't actually be made to the
		 * destination in that case.
		 */
		bool use_stat = (S_ISREG(src_mode)
		                  || (x->copy_as_regular
		                      && !(S_ISDIR(src_mode) || S_ISLNK(src_mode))));
		if ((use_stat
		     ? stat(dst_name, &dst_sb)
		     : lstat(dst_name, &dst_sb))
            != 0) {
			if (errno != ENOENT) {
				HERROR("cannot stat %s: %s\n", dst_name, strerror(errno));
				return false;
            } else {
				new_dst = true;
			}
		} else {
			bool return_now;
			bool unlink_src;
			/*
			 * Here, we know that dst_name exists, at least to the point
			 * that it is stat'able or lstat'able.
			 */
			have_dst_lstat = !use_stat;
			if (!same_file_ok(src_name, &src_sb, dst_name, &dst_sb,
			    x, &return_now, &unlink_src)) {
				HERROR("%s and %s are the same file\n", src_name, dst_name);
				return false;
            }
            
            /* overwrite_prompt */

			if (return_now) {
				return true;
			}
			
			if (!S_ISDIR(dst_sb.st_mode)) {
				if (S_ISDIR(src_mode)) {
					HERROR("cannot overwrite non-directory %s with directory %s\n", dst_name, src_name);
					return false;
				}
				/*
				 * Don't let the user destroy their data, even if they try hard:
				 * This mv command must fail (likewise for cp):
				 *    rm -rf a b c; mkdir a b c; touch a/f b/f; mv a/f b/f c
				 * Otherwise, the contents of b/f would be lost.
				 * In the case of `cp', b/f would be lost if the user simulated
				 * a move using cp and rm.
				 * Note that it works fine if you use --backup=numbered.
				 */
				if (command_line_arg && seen_file(x->dest_info, dst_name, &dst_sb)) {
					HERROR("will not overwrite just-created %s with %s\n", dst_name, src_name);
					return false;
				}
			}
			
			if (!S_ISDIR(src_mode)) {
				if (S_ISDIR(dst_sb.st_mode)) {
					HERROR("cannot overwrite directory %s with non-directory\n", dst_name);
					return false;			
				}
			}

			if (!S_ISDIR (dst_sb.st_mode)
				&& ((x->preserve_links && 1 < dst_sb.st_nlink)
				    || (!S_ISREG(src_sb.st_mode)))) {
				if (unlink (dst_name) != 0 && errno != ENOENT) {
					HERROR("cannot remove %s\n", dst_name);
					return false;
				}
				new_dst = true;
				if (x->verbose) {
					HDEBUG("removed %s\n", dst_name);
				}
			}
		}
	}

	/*
	 * Ensure we don't try to copy through a symlink that was
	 * created by a prior call to this function.
	 */
	if (command_line_arg && x->dest_info) {
		bool lstat_ok = true;
		struct stat tmp_buf;
		struct stat *dst_lstat_sb;

		if (have_dst_lstat) {
			dst_lstat_sb = &dst_sb;
		} else {
			if (lstat(dst_name, &tmp_buf) == 0) {
				dst_lstat_sb = &tmp_buf;
			} else {
				lstat_ok = false;
			}
		}

		/* Never copy through a symlink we've just created.  */
		if (lstat_ok
		    && S_ISLNK(dst_lstat_sb->st_mode)
		    && seen_file(x->dest_info, dst_name, dst_lstat_sb)) {
			HERROR("will not copy %s through just-created symlink %s\n",
			       src_name, dst_name);
			return false;
        }
	}

	/*
	 * If the source is a directory, we don't always create the destination
	 * directory.  So --verbose should not announce anything until we're
	 * sure we'll create a directory.
	 */
	if (x->verbose && !S_ISDIR(src_mode)) {
		emit_verbose(src_name, dst_name);
	}
	if (x->preserve_links && 1 < src_sb.st_nlink) {
		earlier_file = remember_copied(dst_name, src_sb.st_ino, src_sb.st_dev);
	} else if (x->recursive && S_ISDIR(src_mode)) {
		if (command_line_arg) {
			earlier_file = remember_copied (dst_name, src_sb.st_ino, src_sb.st_dev);
		} else {
			earlier_file = src_to_dest_lookup (src_sb.st_ino, src_sb.st_dev);
		}
	}
	/*
	 * Did we copy this inode somewhere else (in this command line argument)
	 * and therefore this is a second hard link to the inode?
	 */
	if (earlier_file) {
		/*
		 * Avoid damaging the destination file system by refusing to preserve
		 * hard-linked directories (which are found at least in Netapp snapshot
		 * directories).
		 */
		if (S_ISDIR(src_mode)) {
			/*
			 * If src_name and earlier_file refer to the same directory entry,
			 * then warn about copying a directory into itself.
			 */
			if (same_name(src_name, earlier_file)) {
				HERROR("cannot copy a directory, %s, into itself, %s\n", top_level_src_name, top_level_dst_name);
				*copy_into_self = true;
				goto un_backup;
			} else {
				HERROR("will not create hard link %s to directory %s\n", dst_name, earlier_file);
				goto un_backup;
			}
		} else {
			/* We want to guarantee that symlinks are not followed.  */
			bool link_failed = (linkat (AT_FDCWD, earlier_file, AT_FDCWD,
                                      dst_name, 0) != 0);
			/*
			 * If the link failed because of an existing destination,
			 * remove that file and then call link again.
			 */
			if (link_failed && errno == EEXIST) {
				if (unlink(dst_name) != 0) {
					HERROR("cannot remove %s\n", dst_name);
					goto un_backup;
				}
				if (x->verbose) {
					HDEBUG("removed %s\n", dst_name);
				}
				link_failed = (linkat(AT_FDCWD, earlier_file, AT_FDCWD,
                                     dst_name, 0) != 0);
			}

			if (link_failed) {
				HERROR("cannot create hard link %s to %s\n", dst_name, earlier_file);
				goto un_backup;
			}
			return true;
		}
	}

	/*
	 * If the ownership might change, or if it is a directory (whose
	 * special mode bits may change after the directory is created),
	 * omit some permissions at first, so unauthorized users cannot nip
	 * in before the file is ready.
	 */
	dst_mode_bits = src_mode & CHMOD_MODE_BITS;
	omitted_permissions = (dst_mode_bits
	                       & (x->preserve_ownership ? S_IRWXG | S_IRWXO
	                       : S_ISDIR (src_mode) ? S_IWGRP | S_IWOTH
	                       : 0));
	delayed_ok = true;
	
	if (S_ISDIR(src_mode)) {
		struct dir_list *dir;
		 /*
		  * If this directory has been copied before during the
          * recursion, there is a symbolic link to an ancestor
          * directory of the symbolic link.  It is impossible to
          * continue to copy this, unless we've got an infinite disk.
          */
        if (is_ancestor(&src_sb, ancestors)) {
			HERROR("cannot copy cyclic symbolic link %s\n", src_name);
			goto un_backup;
        }

        dir = alloca(sizeof *dir);
		dir->parent = ancestors;
		dir->ino = src_sb.st_ino;
		dir->dev = src_sb.st_dev;

		if (new_dst || !S_ISDIR(dst_sb.st_mode)) {
			/*
			 * POSIX says mkdir's behavior is implementation-defined when
			 * (src_mode & ~S_IRWXUGO) != 0.  However, common practice is
			 * to ask mkdir to copy all the CHMOD_MODE_BITS, letting mkdir
			 * decide what to do with S_ISUID | S_ISGID | S_ISVTX.
			 */
			if (mkdir(dst_name, dst_mode_bits & ~omitted_permissions) != 0) {
				HERROR("cannot create directory %s: %s\n", dst_name, strerror(errno));
				goto un_backup;
			}
			/*
			 * We need search and write permissions to the new directory
			 * for writing the directory's contents. Check if these
			 * permissions are there.
			 */
			if (lstat (dst_name, &dst_sb) != 0) {
				HERROR("cannot stat %s: %s\n", dst_name, strerror(errno));
				goto un_backup;
			} else if ((dst_sb.st_mode & S_IRWXU) != S_IRWXU) {
				/* Make the new directory searchable and writable.  */
				dst_mode = dst_sb.st_mode;
				restore_dst_mode = true;

#define lchmod chmod
				if (lchmod(dst_name, dst_mode | S_IRWXU) != 0) {
					HERROR("setting permissions for %s: %s\n", dst_name, strerror(errno));
					goto un_backup;
				}
			}
			/*
			 * Record the created directory's inode and device numbers into
			 * the search structure, so that we can avoid copying it again.
			 * Do this only for the first directory that is created for each
			 * source command line argument.
			 */
			if (!*first_dir_created_per_command_line_arg) {
				remember_copied(dst_name, dst_sb.st_ino, dst_sb.st_dev);
				*first_dir_created_per_command_line_arg = true;
			}

			if (x->verbose) {
				emit_verbose(src_name, dst_name);
			}
		}
		/*
		 * Copy the contents of the directory.  Don't just return if
		 * this fails -- otherwise, the failure to read a single file
		 * in a source directory would cause the containing destination
		 * directory not to have owner/perms set properly.
		 */
		delayed_ok = copy_dir(src_name, dst_name, new_dst, &src_sb, dir, x,
		                      first_dir_created_per_command_line_arg,
		                      copy_into_self);
	} else if (S_ISREG (src_mode)
	           || (x->copy_as_regular && !S_ISLNK (src_mode))) {
		copied_as_regular = true;
		/*
		 * POSIX says the permission bits of the source file must be
		 * used as the 3rd argument in the open call.  Historical
		 * practice passed all the source mode bits to 'open', but the extra
		 * bits were ignored, so it should be the same either way.
		 */
		if (! copy_reg (src_name, dst_name, x, src_mode & S_IRWXUGO,
		    omitted_permissions, &new_dst, &src_sb)) {
		    goto un_backup;
		}
	} else if (S_ISFIFO (src_mode)) {
		/*
		 * Use mknod, rather than mkfifo, because the former preserves
		 * the special mode bits of a fifo on Solaris 10, while mkfifo
		 * does not.  But fall back on mkfifo, because on some BSD systems,
		 * mknod always fails when asked to create a FIFO.
		 */
		if (mknod (dst_name, src_mode & ~omitted_permissions, 0) != 0) {
			if (mkfifo (dst_name, src_mode & ~S_IFIFO & ~omitted_permissions) != 0) {
				HERROR("cannot create fifo %s: %s\n", dst_name, strerror(errno));
				goto un_backup;
			}
        }
	} else if (S_ISBLK (src_mode) || S_ISCHR (src_mode) || S_ISSOCK (src_mode)) {
		if (mknod (dst_name, src_mode & ~omitted_permissions, src_sb.st_rdev)
          != 0) {
          	HERROR("cannot create special file %s: %s\n", dst_name, strerror(errno));
          	goto un_backup;
        }
	} else if (S_ISLNK (src_mode)) {
		char *src_link_val = areadlink_with_size (src_name, src_sb.st_size);
		dest_is_symlink = true;
		if (src_link_val == NULL) {
			HERROR("cannot read symbolic link %s: %s\n", src_name, strerror(errno));
			goto un_backup;
		}

		if (symlink (src_link_val, dst_name) == 0) {
			free (src_link_val);
		} else {
			int saved_errno = errno;
			bool same_link = false;

			free (src_link_val);

			if (! same_link) {
				HERROR("cannot create symbolic link %s: %s\n", dst_name, strerror(saved_errno));
				goto un_backup;
			}
		}

		if (x->preserve_ownership) {
			/*
			 * Preserve the owner and group of the just-`copied'
			 * symbolic link, if possible.
			 */
			if (lchown (dst_name, src_sb.st_uid, src_sb.st_gid) != 0
			    && ! chown_failure_ok (x)) {
			    HERROR("failed to preserve ownership for %s: %s\n", dst_name, strerror(errno));
			    goto un_backup;
			} else {
				/*
				 * Can't preserve ownership of symlinks.
				 * FIXME: maybe give a warning or even error for symlinks
				 * in directories with the sticky bit set -- there, not
				 * preserving owner/group is a potential security problem.
				 */
			}
		}
	} else {
		HERROR("%s has unknown file type\n", src_name);
		goto un_backup;
	}

	if (command_line_arg && x->dest_info) {
		/*
		 * Now that the destination file is very likely to exist,
		 * add its info to the set.
		 */
		struct stat sb;
		if (lstat (dst_name, &sb) == 0) {
			record_file (x->dest_info, dst_name, &sb);
		}
	}
	/*
	 * If we've just created a hard-link due to cp's --link option,
	 * we're done.
	 */
	if (copied_as_regular) {
		return delayed_ok;
	}
	/*
	 * POSIX says that `cp -p' must restore the following:
	 * - permission bits
	 * - setuid, setgid bits
	 * - owner and group
	 * If it fails to restore any of those, we may give a warning but
	 * the destination must not be removed.
	 * FIXME: implement the above.
	 */

	/*
	 * Adjust the times (and if possible, ownership) for the copy.
	 * chown turns off set[ug]id bits for non-root,
	 * so do the chmod last.
	 */
	if (x->preserve_timestamps) {
		struct timespec timespec[2];
		timespec[0] = get_stat_atime (&src_sb);
		timespec[1] = get_stat_mtime (&src_sb);

		if ((dest_is_symlink
		     ? utimens_symlink (dst_name, timespec)
		     : utimens (dst_name, timespec))
		    != 0) {
			HERROR("preserving times for %s: %s\n", dst_name, strerror(errno));
			if (x->require_preserve) {
				return false;
			}
        }
    }
    /* The operations beyond this point may dereference a symlink.  */
    if (dest_is_symlink) {
		return delayed_ok;
	}
	/* Avoid calling chown if we know it's not necessary.  */
	if (x->preserve_ownership
	    && (new_dst || !SAME_OWNER_AND_GROUP (src_sb, dst_sb))) {
	    switch (set_owner (x, dst_name, -1, &src_sb, new_dst, &dst_sb)) {
		case -1:
			return false;

		case 0:
			src_mode &= ~ (S_ISUID | S_ISGID | S_ISVTX);
			break;
	    }
	}
	set_author (dst_name, -1, &src_sb);
	if (x->preserve_xattr && ! copy_attr (src_name, -1, dst_name, -1, x)) {
		HERROR("copy_attr from %s to %s failed\n", src_name, dst_name);
	}
	
	if (x->preserve_mode) {
		if (copy_acl (src_name, -1, dst_name, -1, src_mode) != 0
		    && x->require_preserve) {
			return false;
		}
	} else {
		if (omitted_permissions) {
			omitted_permissions &= ~ cached_umask ();

			if (omitted_permissions && !restore_dst_mode) {
				/*
				 * Permissions were deliberately omitted when the file
				 * was created due to security concerns.  See whether
				 * they need to be re-added now.  It'd be faster to omit
				 * the lstat, but deducing the current destination mode
				 * is tricky in the presence of implementation-defined
				 * rules for special mode bits.
				 */
				if (new_dst && lstat (dst_name, &dst_sb) != 0) {
					HERROR("cannot stat %s: %s\n", dst_name, strerror(errno));
					return false;
				}
				dst_mode = dst_sb.st_mode;
				if (omitted_permissions & ~dst_mode) {
					restore_dst_mode = true;
            	}
            }
        }
        if (restore_dst_mode) {
        	if (lchmod (dst_name, dst_mode | omitted_permissions) != 0) {
        		HERROR("preserving permissions for %s: %s\n", dst_name, strerror(errno));
        		if (x->require_preserve) {
        			return false;
        		}
        	}
        }
	}	
	return delayed_ok;
un_backup:
	if (earlier_file == NULL) {
		forget_created (src_sb.st_ino, src_sb.st_dev);
	}
    return false;
}

/*
 * Copy the file SRC_NAME to the file DST_NAME.  The files may be of
 * any type.  NONEXISTENT_DST should be true if the file DST_NAME
 * is known not to exist (e.g., because its parent directory was just
 * created);  NONEXISTENT_DST should be false if DST_NAME might already
 * exist.  OPTIONS is ... FIXME-describe
 * Set *COPY_INTO_SELF if SRC_NAME is a parent of (or the
 * same as) DST_NAME; otherwise, set clear it.
 * Return true if successful.
 */

bool copy(char const *src_name, char const *dst_name,
      bool nonexistent_dst, const struct cp_options *options,
      bool *copy_into_self)
{
	bool first_dir_created_per_command_line_arg = 0;

	/*
	 * Record the file names: they're used in case of error, when copying
	 * a directory into itself.  I don't like to make these tools do *any*
	 * extra work in the common case when that work is solely to handle
	 * exceptional cases, but in this case, I don't see a way to derive the
	 * top level source and destination directory names where they're used.
	 * An alternative is to use COPY_INTO_SELF and print the diagnostic
	 * from every caller -- but I don't want to do that.
	 */
	top_level_src_name = src_name;
	top_level_dst_name = dst_name;
	return copy_internal(src_name, dst_name, nonexistent_dst, 0, NULL,
	                     options, 1,
	                     &first_dir_created_per_command_line_arg,
	                     copy_into_self);
}

static bool do_copy(int n_files, char **file, struct cp_options *cp_options)
{
	const char *target_directory = NULL;
	struct stat sb;
	bool new_dst = 0;
	bool ret = true;

	if (n_files <= 1) {
		if (n_files <= 0) {
			HERROR("missing file operand\n");
		} else {
			HERROR("missing destination file operand after %s\n", file[0]);
		}
		ret = false;
		goto out;
	}
	
	if (2 <= n_files && target_directory_operand(file[n_files - 1], &sb, &new_dst)) {
		target_directory = file[--n_files];
	} else if (2 < n_files) {
		HERROR("target %s is not a directory\n", file[n_files - 1]);
		ret = false;
		goto out;
	}

	if (target_directory) {
		/*
		 * cp file1...filen edir
		 * Copy the files `file1' through `filen'
		 * to the existing directory `edir'.
		 */
		int i;
		/* Initialize these hash tables only if we'll need them.
		 * The problems they're used to detect can arise only if
		 * there are two or more files to copy.
		 */
		if (2 <= n_files) {
			dest_info_init(cp_options);
			src_info_init(cp_options);
		}

		for (i = 0; i < n_files; i++) {
			char *arg = file[i];
			char *arg_base;
			char *dst_name;
			bool copy_into_self;
			
			/* Append the last component of `arg' to `target_directory'.  */
			ASSIGN_BASENAME_STRDUPA (arg_base, arg);
			/* For `cp -R source/.. dest', don't copy into `dest/..'. */
			dst_name = (STREQ (arg_base, "..")
			            ? xstrdup(target_directory)
			            : file_name_concat(target_directory, arg_base, NULL));
			if (dst_name == NULL) {
				ret = -ENOMEM;
				goto out;
			}
			ret = copy(arg, dst_name, new_dst, cp_options, &copy_into_self);
			if (!ret) {
				goto out;
			}
		}
	} else { /* !target_directory */
		char const *source = file[0];
		char const *dest = file[1];
		bool unused;

		ret = copy(source, dest, 0, cp_options, &unused);
		if (ret) {
			goto out;
		}
	}	
	
out:
	return ret;
}

int hrfs_api_copy(int n_files, char **file, struct cp_options *cp_options)
{
	int ret = 0;

	/* Allocate space for remembering copied and created files.  */
	hash_init ();
	if(!do_copy(n_files, file, cp_options)) {
		ret = -1;
	}
	forget_all ();

	return ret;
}
