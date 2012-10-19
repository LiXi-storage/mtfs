/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <mtfs_subject.h>
#include "support_internal.h"

int mtfs_subject_init(struct super_block *sb)
{
	int ret = 0;
	struct mtfs_operations *operations = NULL;
	MENTRY();

	operations = mtfs_s2ops(sb);
	if (operations->subject_ops && operations->subject_ops->mso_init) {
		ret = operations->subject_ops->mso_init(sb);
	}

	MRETURN(ret);
}

int mtfs_subject_fini(struct super_block *sb)
{
	int ret = 0;
	struct mtfs_operations *operations = NULL;
	MENTRY();

	operations = mtfs_s2ops(sb);
	if (operations->subject_ops && operations->subject_ops->mso_fini) {
		ret = operations->subject_ops->mso_fini(sb);
	}

	MRETURN(ret);
}
