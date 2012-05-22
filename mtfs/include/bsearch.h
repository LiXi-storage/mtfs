/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_BSEARCH_H__
#define __HRFS_BSEARCH_H__

#if defined (__linux__) && defined(__KERNEL__)

#include <linux/sort.h>
#define mtfs_sort(base, num, size, cmp) sort(base, num, size, cmp, NULL)

/*
 * bsearch - binary search an array of elements
 * @key: pointer to item being searched for
 * @base: pointer to data to sort
 * @num: number of elements
 * @size: size of each element
 * @cmp: pointer to comparison function
 *
 * This function does a binary search on the given array.  The
 * contents of the array should already be in ascending sorted order
 * under the provided comparison function.
 *
 * Note that the key need not have the same type as the elements in
 * the array, e.g. key could be a string and the comparison function
 * could compare the string with the struct's name field.  However, if
 * the key and elements in the array are of the same type, you can use
 * the same comparison function for both sort() and bsearch().
 */
static inline void *bsearch(const void *key, const void *base, size_t num, size_t size,
	      int (*cmp)(const void *key, const void *elt))
{
	int start = 0, end = num - 1, mid, result;
	if (num == 0)
		return NULL;

	while (start <= end) {
		mid = (start + end) / 2;
		result = cmp(key, base + mid * size);
		if (result < 0)
			end = mid - 1;
		else if (result > 0)
			start = mid + 1;
		else
			return (void *)base + mid * size;
	}

	return NULL;
}
#else /* ! defined (__linux__) && defined(__KERNEL__) */
#include <stdlib.h>
#define mtfs_sort(base, num, size, cmp) qsort(base, num, size, cmp)
#endif

static inline int compare_string_pointer(const void *p1, const void *p2)
{
	/*
	 * The actual arguments to this function are "pointers to
   * pointers to char", but strcmp() arguments are "pointers
   * to char", hence the following cast plus dereference
   */
	return strcmp(* (char * const *)p1,* (char * const *) p2);
}

static inline int compare_char_pointer(const void *p1, const void *p2)
{
	return ((int)(* (char *)p1)) - ((int)(* (char *)p2));
}


#endif /* __HRFS_BSEARCH_H__ */
