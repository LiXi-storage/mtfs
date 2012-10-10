/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_BITMAP_H__
#define __MTFS_BITMAP_H__

#if defined(__KERNEL__)
#if !defined(BITS_PER_LONG)
#error "BITS_PER_LONG not defined"
#endif /* !defined(BITS_PER_LONG) */
#else /* !defined(__KERNEL__) */
#if !defined(__WORDSIZE)
#error "__WORDSIZE not defined"
#else /* defined(__WORDSIZE) */
#define BITS_PER_LONG __WORDSIZE
#endif /* defined(__WORDSIZE) */
#endif /* !defined(__KERNEL__) */

typedef struct {
        int             size;
        unsigned long   data[0];
} mtfs_bitmap_t;

#define MTFS_BITMAP_SIZE(nbits) \
     (((nbits/BITS_PER_LONG)+1)*sizeof(long)+sizeof(mtfs_bitmap_t))

static inline
mtfs_bitmap_t *MTFS_ALLOCATE_BITMAP(int size)
{
        mtfs_bitmap_t *ptr;

        MTFS_ALLOC(ptr, MTFS_BITMAP_SIZE(size));
        if (ptr == NULL)
                return(ptr);

        ptr->size = size;

        return(ptr);
}

#define MTFS_FREE_BITMAP(ptr)        MTFS_FREE(ptr, MTFS_BITMAP_SIZE(ptr->size))

#if defined(__linux__) && defined(__KERNEL__)
#define mtfs_test_bit(nr, addr)              test_bit(nr, addr)
#define mtfs_set_bit(nr, addr)               set_bit(nr, addr)
#define mtfs_clear_bit(nr, addr)             clear_bit(nr, addr)
#define mtfs_test_and_set_bit(nr, addr)      test_and_set_bit(nr, addr)
#define mtfs_test_and_clear_bit(nr, addr)    test_and_clear_bit(nr, addr)
#define mtfs_find_first_bit(addr, size)      find_first_bit(addr, size)
#define mtfs_find_first_zero_bit(addr, size) find_first_zero_bit(addr, size)
#define mtfs_find_next_bit(addr, size, off)  find_next_bit(addr, size, off)
#define mtfs_find_next_zero_bit(addr, size, off) \
        find_next_zero_bit(addr, size, off)
#else /* !defined(__linux__) && defined(__KERNEL__) */
static __inline__ int mtfs_test_bit(int nr, const unsigned long *addr)
{
        return ((1UL << (nr & (BITS_PER_LONG - 1))) &
                ((addr)[nr / BITS_PER_LONG])) != 0;
}

/* test if bit nr is set in bitmap addr; returns previous value of bit nr */
static __inline__ int mtfs_test_and_set_bit(int nr, unsigned long *addr)
{
        unsigned long mask;

        addr += nr / BITS_PER_LONG;
        mask = 1UL << (nr & (BITS_PER_LONG - 1));
        nr = (mask & *addr) != 0;
        *addr |= mask;
        return nr;
}

#define mtfs_set_bit(n, a) mtfs_test_and_set_bit(n, a)

/* clear bit nr in bitmap addr; returns previous value of bit nr*/
static __inline__ int mtfs_test_and_clear_bit(int nr, unsigned long *addr)
{
        unsigned long mask;

        addr += nr / BITS_PER_LONG;
        mask = 1UL << (nr & (BITS_PER_LONG - 1));
        nr = (mask & *addr) != 0;
        *addr &= ~mask;
        return nr;
}

#define mtfs_clear_bit(n, a) mtfs_test_and_clear_bit(n, a)



/* using binary seach */
static __inline__ unsigned long __mtfs_fls(long data)
{
	int pos = 32;

	if (!data)
		return 0;

#if BITS_PER_LONG == 64
        /* If any bit of the high 32 bits are set, shift the high
         * 32 bits down and pretend like it is a 32-bit value. */
        if ((data & 0xFFFFFFFF00000000llu)) {
                data >>= 32;
                pos += 32;
        }
#endif

	if (!(data & 0xFFFF0000u)) {
		data <<= 16;
		pos -= 16;
	}
	if (!(data & 0xFF000000u)) {
		data <<= 8;
		pos -= 8;
	}
	if (!(data & 0xF0000000u)) {
		data <<= 4;
		pos -= 4;
	}
	if (!(data & 0xC0000000u)) {
		data <<= 2;
		pos -= 2;
	}
	if (!(data & 0x80000000u)) {
		data <<= 1;
		pos -= 1;
	}
	return pos;
}

static __inline__ unsigned long __mtfs_ffs(long data)
{
        int pos = 0;

#if BITS_PER_LONG == 64
        if ((data & 0xFFFFFFFF) == 0) {
                pos += 32;
                data >>= 32;
        }
#endif
        if ((data & 0xFFFF) == 0) {
                pos += 16;
                data >>= 16;
        }
        if ((data & 0xFF) == 0) {
                pos += 8;
                data >>= 8;
        }
        if ((data & 0xF) == 0) {
                pos += 4;
                data >>= 4;
        }
        if ((data & 0x3) == 0) {
                pos += 2;
                data >>= 2;
        }
        if ((data & 0x1) == 0)
                pos += 1;

        return pos;
}

#define __mtfs_ffz(x)	__mtfs_ffs(~(x))
#define __mtfs_flz(x)	__mtfs_fls(~(x))

#define OFF_BY_START(start)     ((start)/BITS_PER_LONG)

unsigned long mtfs_find_next_bit(unsigned long *addr,
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
                bit = __mtfs_ffs(tmp);
                if (bit < BITS_PER_LONG)
                        goto found;
                word++;
                base += BITS_PER_LONG;
        }
        while (word <= last) {
                if (*word != 0UL) {
                        bit = __mtfs_ffs(*word);
                        goto found;
                }
                word++;
                base += BITS_PER_LONG;
        }
        return size;
found:
        return base + bit;
}

unsigned long mtfs_find_next_zero_bit(unsigned long *addr,
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
                bit = __mtfs_ffz(tmp);
                if (bit < BITS_PER_LONG)
                        goto found;
                word++;
                base += BITS_PER_LONG;
        }
        while (word <= last) {
                if (*word != ~0UL) {
                        bit = __mtfs_ffz(*word);
                        goto found;
                }
                word++;
                base += BITS_PER_LONG;
        }
        return size;
found:
        return base + bit;
}

#define mtfs_find_first_bit(addr,size)     (mtfs_find_next_bit((addr),(size),0))
#define mtfs_find_first_zero_bit(addr,size)  \
        (mtfs_find_next_zero_bit((addr),(size),0))
#endif /* !defined(__linux__) && defined(__KERNEL__) */

static inline
void mtfs_bitmap_set(mtfs_bitmap_t *bitmap, int nbit)
{
        mtfs_set_bit(nbit, bitmap->data);
}

static inline
void mtfs_bitmap_clear(mtfs_bitmap_t *bitmap, int nbit)
{
        mtfs_test_and_clear_bit(nbit, bitmap->data);
}

static inline
int mtfs_bitmap_check(mtfs_bitmap_t *bitmap, int nbit)
{
        return mtfs_test_bit(nbit, bitmap->data);
}

static inline
int mtfs_bitmap_test_and_clear(mtfs_bitmap_t *bitmap, int nbit)
{
        return mtfs_test_and_clear_bit(nbit, bitmap->data);
}

/* return 0 is bitmap has none set bits */
static inline
int mtfs_bitmap_check_empty(mtfs_bitmap_t *bitmap)
{
        return mtfs_find_first_bit(bitmap->data, bitmap->size) == bitmap->size;
}

#define mtfs_foreach_bit(bitmap, pos) \
	for((pos)=mtfs_find_first_bit((bitmap)->data, bitmap->size);   \
            (pos) < (bitmap)->size;                               \
            (pos) = mtfs_find_next_bit((bitmap)->data, (bitmap)->size, (pos)))

static inline void mtfs_bitmap_dump(mtfs_bitmap_t *bitmap)
{
	int i = 0;
	mtfs_foreach_bit(bitmap, i) {
		MPRINT("%d ", i);
	}
	MPRINT("\n");
}

static inline mtfs_bitmap_t *mtfs_bitmap_allocate(int size)
{
	mtfs_bitmap_t *ptr = NULL;

	MTFS_ALLOC(ptr, MTFS_BITMAP_SIZE(size));
	if (ptr == NULL) {
		return (ptr);
	}

	/* Clear all bits */
	memset(ptr, 0, MTFS_BITMAP_SIZE(size));
	ptr->size = size;

	return (ptr);
}

#define mtfs_bitmap_freee(ptr) MTFS_FREE(ptr, MTFS_BITMAP_SIZE(ptr->size));
#endif /* __MTFS_BITMAP_H__ */
