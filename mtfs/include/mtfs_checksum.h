/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#ifndef __MTFS_CHECKSUM_H__
#define __MTFS_CHECKSUM_H__

#include "debug.h"

#if defined (__linux__) && defined(__KERNEL__) && (defined(CONFIG_CRC32) || defined(CONFIG_CRC32_MODULE))
#include <linux/crc32.h>
#else /* !defined (__linux__) && defined(__KERNEL__) && (defined(CONFIG_CRC32) || defined(CONFIG_CRC32_MODULE)) */
/* crc32_le lifted from the Linux kernel, which had the following to say:
 *
 * This code is in the public domain; copyright abandoned.
 * Liability for non-performance of this code is limited to the amount
 * you paid for it.  Since it is distributed for free, your refund will
 * be very very small.  If it breaks, you get to keep both pieces.
 */
#define CRCPOLY_LE 0xedb88320
/**
 * crc32_le() - Calculate bitwise little-endian Ethernet AUTODIN II CRC32
 * \param crc  seed value for computation.  ~0 for Ethernet, sometimes 0 for
 *             other uses, or the previous crc32 value if computing incrementally.
 * \param p  - pointer to buffer over which CRC is run
 * \param len- length of buffer \a p
 */
static inline __u32 crc32_le(__u32 crc, unsigned char const *p, size_t len)
{
	int i;
	while (len--) {
		crc ^= *p++;
		for (i = 0; i < 8; i++) {
			crc = (crc >> 1) ^ ((crc & 1) ? CRCPOLY_LE : 0);
		}
	}
	return crc;
}
#endif /* !defined (__linux__) && defined(__KERNEL__) && (defined(CONFIG_CRC32) || defined(CONFIG_CRC32_MODULE)) */

#if defined(__linux__) && defined(__KERNEL__)
#include <linux/zutil.h>
#ifndef HAVE_ADLER
#define HAVE_ADLER
#endif /* HAVE_ADLER */
#define adler32(a,b,l) zlib_adler32(a,b,l)
#else /* !defined(__linux__) && defined(__KERNEL__) */
#ifdef HAVE_ADLER
#include <zlib.h>
#endif
#endif /* !defined(__linux__) && defined(__KERNEL__) */

#ifdef X86_FEATURE_XMM4_2
/* Call Nehalem+ CRC32C harware acceleration instruction on individual bytes. */
static inline __u32 crc32c_hw_byte(__u32 crc, unsigned char const *p,
				   size_t bytes)
{
        while (bytes--) {
                __asm__ __volatile__ (
                        ".byte 0xf2, 0xf, 0x38, 0xf0, 0xf1"
                        : "=S"(crc)
                        : "0"(crc), "c"(*p)
                );
                p++;
        }

        return crc;
}

#if BITS_PER_LONG > 32
#define WORD_SHIFT 3
#define WORD_MASK  7
#define REX "0x48, "
#else /* ! BITS_PER_LONG > 32 */
#define WORD_SHIFT 2
#define WORD_MASK  3
#define REX ""
#endif /* ! BITS_PER_LONG > 32 */

/* Do we need to worry about unaligned input data here? */
static inline __u32 crc32c_hw(__u32 crc, unsigned char const *p, size_t len)
{
        unsigned int words = len >> WORD_SHIFT;
        unsigned int bytes = len &  WORD_MASK;
        long *ptmp = (long *)p;

        while (words--) {
                __asm__ __volatile__(
                        ".byte 0xf2, " REX "0xf, 0x38, 0xf1, 0xf1;"
                        : "=S"(crc)
                        : "0"(crc), "c"(*ptmp)
                );
                ptmp++;
        }

        if (bytes) {
		crc = crc32c_hw_byte(crc, (unsigned char *)ptmp, bytes);
	}

        return crc;
}
#else /* !X86_FEATURE_XMM4_2 */
/* We should never call this unless the CPU has previously been detected to
 * support this instruction in the SSE4.2 feature set. b=23549  */
static inline __u32 crc32c_hw(__u32 crc, unsigned char const *p,size_t len)
{
        MBUG();
}
#endif /* !X86_FEATURE_XMM4_2 */

typedef enum {
	MCHECKSUM_CRC32  = 0x00000001,
	MCHECKSUM_ADLER  = 0x00000002,
	MCHECKSUM_CRC32C = 0x00000004,
} mchecksum_type_t;

static inline __u32 mchecksum_init(mchecksum_type_t type)
{
	switch (type) {
	case MCHECKSUM_CRC32:
		return ~0U;
#ifdef HAVE_ADLER
	case MCHECKSUM_ADLER:
		return 1U;
#endif
	case MCHECKSUM_CRC32C:
		return ~0U;
        default:
                MERROR("Unknown checksum type (%x)!!!\n", type);
                MBUG();
	}
	return 0;
}


static inline __u32 mchecksum_compute(__u32 checksum, unsigned char const *p,
                                      size_t len, mchecksum_type_t type)
{
	switch (type) {
	case MCHECKSUM_CRC32:
		return crc32c_hw(checksum, p, len);
#ifdef HAVE_ADLER
	case MCHECKSUM_ADLER:
		return adler32(checksum, p, len);
#endif
	case MCHECKSUM_CRC32C:
		return crc32_le(checksum, p, len);
        default:
                MERROR("Unknown checksum type (%x)!!!\n", type);
                MBUG();
	}
	return 0;
}

/*
 * Return a bitmask of the checksum types supported on this system.
 *
 * If hardware crc32c support is not available, it is slower than Adler,
 * so don't include it, even if it could be emulated in software.
 */
static inline mchecksum_type_t mchecksum_types_supported(void)
{
        mchecksum_type_t ret = MCHECKSUM_CRC32;

#ifdef X86_FEATURE_XMM4_2
        if (cpu_has_xmm4_2) {
                ret |= MCHECKSUM_CRC32C;
	}
#endif /* X86_FEATURE_XMM4_2 */
#ifdef HAVE_ADLER
        ret |= MCHECKSUM_ADLER;
#endif
        return ret;
}

/*
 * These flags should be listed in order of descending performance, so that
 * in case multiple algorithms are supported the best one is used.
 */
static inline mchecksum_type_t mchecksum_type_select(void)
{
#ifdef X86_FEATURE_XMM4_2
        if (cpu_has_xmm4_2) {
                return MCHECKSUM_CRC32C;
	}
#endif /* X86_FEATURE_XMM4_2 */

#ifdef HAVE_ADLER
        return MCHECKSUM_ADLER;
#endif
        return MCHECKSUM_CRC32;
}
#endif /* __MTFS_CHECKSUM_H__ */
