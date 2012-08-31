/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef _MTFS_COMPAT_H_
#define _MTFS_COMPAT_H_

#if defined(__linux__) && defined(__KERNEL__)
#ifdef HAVE_FS_RENAME_DOES_D_MOVE
#define MTFS_RENAME_DOES_D_MOVE   FS_RENAME_DOES_D_MOVE
#else /* !HAVE_FS_RENAME_DOES_D_MOVE */
#define MTFS_RENAME_DOES_D_MOVE   FS_ODD_RENAME
#endif /* !HAVE_FS_RENAME_DOES_D_MOVE */

#include <linux/cpumask.h>
#ifdef for_each_possible_cpu
#define mtfs_for_each_possible_cpu(cpu) for_each_possible_cpu(cpu)
#elif defined(for_each_cpu)
#define mtfs_for_each_possible_cpu(cpu) for_each_cpu(cpu)
#endif

#ifndef HAVE_STRCASECMP
extern int strncasecmp(const char *s1, const char *s2, size_t n);
#endif

#else /* !defined(__linux__) && defined(__KERNEL__) */

#ifndef offsetof
#define offsetof(typ,memb)     ((unsigned long)((char *)&(((typ *)0)->memb)))
#endif

/* cardinality of array */
#define sizeof_array(a) ((sizeof (a)) / (sizeof ((a)[0])))

#ifndef container_of
/* given a pointer @ptr to the field @member embedded into type (usually
 * struct) @type, return pointer to the embedding instance of @type. */
#define container_of(ptr, type, member) \
        ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))
#endif /* !container_of */

#define EXPORT_SYMBOL(s)
#define MODULE_AUTHOR(s)
#define MODULE_DESCRIPTION(s)
#define MODULE_LICENSE(s)
#define MODULE_PARM(a, b)
#define MODULE_PARM_DESC(a, b)

#endif  /* !defined(__linux__) && defined(__KERNEL__) */
#endif /* _MTFS_COMPAT_H_ */
