/* -*- mode: c; c-basic-offset: 8; indent-tabs-mode: nil; -*-
 * vim:expandtab:shiftwidth=8:tabstop=8:
 *
 * GPL HEADER START
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License version 2 for more details (a copy is included
 * in the LICENSE file that accompanied this code).
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; If not, see
 * http://www.sun.com/software/products/swgfs/docs/GPLv2.pdf
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa Clara,
 * CA 95054 USA or visit www.sun.com if you need additional information or
 * have any questions.
 *
 * GPL HEADER END
 */
/*
 * Copyright  2008 Sun Microsystems, Inc. All rights reserved
 * Use is subject to license terms.
 */
/*
 * This file is part of Swgfs, http://www.swgfs.org/
 * Swgfs is a trademark of Sun Microsystems, Inc.
 */
#ifndef __KERNEL__

#include <libcfs/libcfs.h>
#include <libcfs/kp30.h>
#include <libcfs/user-bitops.h>

#define OFF_BY_START(start)     ((start)/BITS_PER_LONG)

#ifndef LIXI_20110608
//#include <stdio.h>
unsigned long find_next_bit(unsigned long *addr,
                            unsigned long size, unsigned long offset)
{
        unsigned long *word, *last;
        unsigned long first_bit, bit, base;

        word = addr + OFF_BY_START(offset);
        last = addr + OFF_BY_START(size-1);
        first_bit = offset % BITS_PER_LONG;
        base = offset - first_bit;

        if (offset >= size)
                return size;
        if (first_bit != 0) {
                long tmp = (*word) & (~0UL << first_bit);
                if (tmp != 0UL) {
	                bit = __ffs(tmp);
	                if (bit < BITS_PER_LONG) {
	                	      //printf("found in first_bit: %lu\n", base + bit);
	                        goto found;		
	                }
              	}
                word++;
                base += BITS_PER_LONG;
        }
        while (word <= last) {
                if (*word != 0UL) {
                        bit = __ffs(*word);
                        //printf("found in word: %lu\n", base + bit);
                        goto found;
                }
                word++;
                base += BITS_PER_LONG;
        }
        return size;
found:
        return base + bit;
}

unsigned long find_next_zero_bit(unsigned long *addr,
                                 unsigned long size, unsigned long offset)
{
        unsigned long *word, *last;
        unsigned long first_bit, bit, base;

        word = addr + OFF_BY_START(offset);
        last = addr + OFF_BY_START(size-1);
        first_bit = offset % BITS_PER_LONG;
        base = offset - first_bit;

        if (offset >= size)
                return size;
        if (first_bit != 0) {
                long tmp = (*word) & (~0UL << first_bit);
                if (tmp != 0UL) {
	                bit = __ffz(tmp);
	                if (bit < BITS_PER_LONG)
	                        goto found;
	                word++;
	              }
                base += BITS_PER_LONG;
        }
        while (word <= last) {
                if (*word != ~0UL) {
                        bit = __ffz(*word);
                        goto found;
                }
                word++;
                base += BITS_PER_LONG;
        }
        return size;
found:
        return base + bit;
}
#else /* LIXI_20110608 */
unsigned long find_next_bit(unsigned long *addr,
                            unsigned long size, unsigned long offset)
{
        unsigned long *word, *last;
        unsigned long first_bit, bit, base;

        word = addr + OFF_BY_START(offset);
        last = addr + OFF_BY_START(size-1);
        first_bit = offset % BITS_PER_LONG;
        base = offset - first_bit;

        if (offset >= size)
                return size;
        if (first_bit != 0) {
                int tmp = (*word++) & (~0UL << first_bit);
                bit = __ffs(tmp);
                if (bit < BITS_PER_LONG)
                        goto found;
                word++;
                base += BITS_PER_LONG;
        }
        while (word <= last) {
                if (*word != 0UL) {
                        bit = __ffs(*word);
                        goto found;
                }
                word++;
                base += BITS_PER_LONG;
        }
        return size;
found:
        return base + bit;
}

unsigned long find_next_zero_bit(unsigned long *addr,
                                 unsigned long size, unsigned long offset)
{
        unsigned long *word, *last;
        unsigned long first_bit, bit, base;

        word = addr + OFF_BY_START(offset);
        last = addr + OFF_BY_START(size-1);
        first_bit = offset % BITS_PER_LONG;
        base = offset - first_bit;

        if (offset >= size)
                return size;
        if (first_bit != 0) {
                int tmp = (*word++) & (~0UL << first_bit);
                bit = __ffz(tmp);
                if (bit < BITS_PER_LONG)
                        goto found;
                word++;
                base += BITS_PER_LONG;
        }
        while (word <= last) {
                if (*word != ~0UL) {
                        bit = __ffz(*word);
                        goto found;
                }
                word++;
                base += BITS_PER_LONG;
        }
        return size;
found:
        return base + bit;
}
#endif /* LIXI_20110608 */

#endif
