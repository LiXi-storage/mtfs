/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef _MTFS_COMPAT_H_
#define _MTFS_COMPAT_H_

#ifdef HAVE_FS_RENAME_DOES_D_MOVE
#define MTFS_RENAME_DOES_D_MOVE   FS_RENAME_DOES_D_MOVE
#else /* !HAVE_FS_RENAME_DOES_D_MOVE */
#define MTFS_RENAME_DOES_D_MOVE   FS_ODD_RENAME
#endif /* !HAVE_FS_RENAME_DOES_D_MOVE */

#endif /* _MTFS_COMPAT_H_ */
