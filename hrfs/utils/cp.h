/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef _HRFS_CP_H_
#define _HRFS_CP_H_
# include "libcoreutils/hash.h"
struct cp_options
{

	/*
	 * If true, attempt to give the copies the original files' permissions,
	 * owner, group, and timestamps.
	 */
	bool preserve_ownership;
	bool preserve_mode;
	bool preserve_timestamps;

	/*
	 * Enabled for mv, and for cp by the --preserve=links option.
	 * If true, attempt to preserve in the destination files any
	 * logical hard links between the source files.  If used with cp's
 	 * --no-dereference option, and copying two hard-linked files,
	 * the two corresponding destination files will also be hard linked.
	 *
	 * If used with cp's --dereference (-L) option, then, as that option implies,
	 * hard links are *not* preserved.  However, when copying a file F and
	 * a symlink S to F, the resulting S and F in the destination directory
	 * will be hard links to the same file (a copy of F).
	 */
	bool preserve_links;
  
	/*
	 * If true and any of the above (for preserve) file attributes cannot
	 * be applied to a destination file, treat it as a failure and return
	 * nonzero immediately.  E.g. for cp -p this must be true, for mv it
	 * must be false.
	 */
	bool require_preserve;
	
	/*
	 * If true, attempt to preserve extended attributes using libattr.
	 * Ignored if coreutils are compiled without xattr support.
	 */
	bool preserve_xattr;
	
	/*
	 * If true, copy directories recursively and copy special files
	 * as themselves rather than copying their contents.
	 */
	bool recursive;

	/*
     * This is a set of destination name/inode/dev triples.  Each such triple
     * represents a file we have created corresponding to a source file name
     * that was specified on the command line.  Use it to avoid clobbering
     * source files in commands like this:
     *   rm -rf a b c; mkdir a b c; touch a/f b/f; mv a/f b/f c
     * For now, it protects only regular files when copying (i.e. not renaming).
     * When renaming, it protects all non-directories.
     * Use dest_info_init to initialize it, or set it to NULL to disable
     * this feature.
     */
	Hash_table *dest_info;
	
	/* FIXME */
	Hash_table *src_info;

	/*
	 * If true, copy all files except (directories and, if not dereferencing
	 * them, symbolic links,) as if they were regular files.
	*/
	bool copy_as_regular;
	
	/*
	 * Whether this process has appropriate privileges to chown a file
	 * whose owner is not the effective user ID.
	 */
	bool chown_privileges;

	/*
	 * Whether this process has appropriate privileges to do the
	 * following operations on a file even when it is owned by some
	 * other user: set the file's atime, mtime, mode, or ACL; remove or
	 * rename an entry in the file even though it is a sticky directory,
	 * or to mount on the file.
	 */
	bool owner_privileges;
	
	int verbose;
};

int hrfs_api_copy (int n_files, char **file, struct cp_options *cp_options);
void cp_option_init (struct cp_options *cp_options);
#endif /* _HRFS_CP_H_ */
