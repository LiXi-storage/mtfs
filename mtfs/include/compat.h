/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_COMPAT_H__
#define __MTFS_COMPAT_H__

#if defined(__linux__) && defined(__KERNEL__)
#include <linux/cpumask.h>
#include <linux/kallsyms.h>

#ifdef HAVE_FS_RENAME_DOES_D_MOVE
#define MTFS_RENAME_DOES_D_MOVE   FS_RENAME_DOES_D_MOVE
#else /* !HAVE_FS_RENAME_DOES_D_MOVE */
#define MTFS_RENAME_DOES_D_MOVE   FS_ODD_RENAME
#endif /* !HAVE_FS_RENAME_DOES_D_MOVE */

#ifdef for_each_possible_cpu
#define mtfs_for_each_possible_cpu(cpu) for_each_possible_cpu(cpu)
#elif defined(for_each_cpu)
#define mtfs_for_each_possible_cpu(cpu) for_each_cpu(cpu)
#endif

#ifndef HAVE_STRCASECMP
extern int strncasecmp(const char *s1, const char *s2, size_t n);
#endif

#ifndef HAVE_KALLSYMS_LOOKUP_NAME
extern unsigned long mtfs_kallsyms_lookup_name(const char *name);
#else /* !HAVE_KALLSYMS_LOOKUP_NAME */
#define mtfs_kallsyms_lookup_name kallsyms_lookup_name
#endif /* !HAVE_KALLSYMS_LOOKUP_NAME */

extern int mtfs_symbol_get(const char *module_name,
                           const char *symbol_name,
                           unsigned long *address,
                           struct module **owner);
extern void mtfs_symbol_put(struct module *owner);
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

extern int mtfs_symbol_get(const char *module_name,
                           const char *symbol_name,
                           unsigned long *address);
#endif  /* !defined(__linux__) && defined(__KERNEL__) */
#endif /* __MTFS_COMPAT_H__ */
