/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <mtfs_subject.h>
#include "lowerfs_internal.h"

int msubject_super_init(struct super_block *sb)
{
	int ret = 0;
	struct mtfs_operations *operations = NULL;
	MENTRY();

	operations = mtfs_s2ops(sb);
	if (operations->subject_ops &&
	    operations->subject_ops->mso_super_init) {
		ret = operations->subject_ops->mso_super_init(sb);
	}

	MRETURN(ret);
}

int msubject_super_fini(struct super_block *sb)
{
	int ret = 0;
	struct mtfs_operations *operations = NULL;
	MENTRY();

	operations = mtfs_s2ops(sb);
	if (operations->subject_ops &&
	    operations->subject_ops->mso_super_fini) {
		ret = operations->subject_ops->mso_super_fini(sb);
	}

	MRETURN(ret);
}

int msubject_inode_init(struct inode *inode)
{
	int ret = 0;
	struct mtfs_operations *operations = NULL;
	MENTRY();

	operations = mtfs_i2ops(inode);
	if (operations->subject_ops &&
	    operations->subject_ops->mso_inode_init) {
		ret = operations->subject_ops->mso_inode_init(inode);
	}

	MRETURN(ret);
}

int msubject_inode_fini(struct inode *inode)
{
	int ret = 0;
	struct mtfs_operations *operations = NULL;
	MENTRY();

	operations = mtfs_i2ops(inode);
	if (operations->subject_ops &&
	    operations->subject_ops->mso_inode_fini) {
		ret = operations->subject_ops->mso_inode_fini(inode);
	}

	MRETURN(ret);
}
