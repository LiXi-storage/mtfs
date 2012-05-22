/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __HRFS_RANDOM_H__
#define __HRFS_RANDOM_H__
#if defined (__linux__) && defined(__KERNEL__)

#else /* !(defined (__linux__) && defined(__KERNEL__)) */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <debug.h>

static inline void mtfs_random_init(int seed)
{
	if (seed == 0) {
		int f = open("/dev/urandom", O_RDONLY);

		if (f < 0 || read(f, &seed, sizeof(seed)) < sizeof(seed))
			seed = time(0);
		if (f > 0)
			close(f);
	}
	srand(seed);
}

#define mtfs_random() rand()

static inline int mtfs_rand_range(int lower_bound, int upper_bound)
{
	int num = 0;
	int range = upper_bound - lower_bound;
	
	HASSERT(range > 0 && range < RAND_MAX);

	num = mtfs_random(); /* [0, RAND_MAX] */
	num %= range;        /* [0, range - 1] */ 
	num += lower_bound;  /* [lower_bound, upper_bound] */ 
	
	return num;
}

#endif /* !defined (__linux__) && defined(__KERNEL__) */
#endif /* __HRFS_RANDOM_H__ */
