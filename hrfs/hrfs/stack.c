/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

/* Copy from linux-2.6.30/fs/stack.c */

#include <linux/module.h>
#include <linux/fs.h>
#if 1 /* TODO: HAVE_STACK */
#include <linux/fs_stack.h>
#else
#include <hrfs_stack.h>

/* does _NOT_ require i_mutex to be held.
 *
 * This function cannot be inlined since i_size_{read,write} is rather
 * heavy-weight on 32-bit systems
 */
void fsstack_copy_inode_size(struct inode *dst, const struct inode *src)
{
	i_size_write(dst, i_size_read((struct inode *)src));
	dst->i_blocks = src->i_blocks;
}
EXPORT_SYMBOL_GPL(fsstack_copy_inode_size);

/* copy all attributes; get_nlinks is optional way to override the i_nlink
 * copying
 */
void fsstack_copy_attr_all(struct inode *dest, const struct inode *src,
				int (*get_nlinks)(struct inode *))
{
	dest->i_mode = src->i_mode;
	dest->i_uid = src->i_uid;
	dest->i_gid = src->i_gid;
	dest->i_rdev = src->i_rdev;
	dest->i_atime = src->i_atime;
	dest->i_mtime = src->i_mtime;
	dest->i_ctime = src->i_ctime;
	dest->i_blkbits = src->i_blkbits;
	dest->i_flags = src->i_flags;

	/*
	 * Update the nlinks AFTER updating the above fields, because the
	 * get_links callback may depend on them.
	 */
	if (!get_nlinks)
		dest->i_nlink = src->i_nlink;
	else
		dest->i_nlink = (*get_nlinks)(dest);
}
EXPORT_SYMBOL_GPL(fsstack_copy_attr_all);
#endif
