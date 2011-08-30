/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_COMMON_H__
#define __HRFS_COMMON_H__

#ifdef CONFIG_HRFS_BRANCH_MAX
#if CONFIG_HRFS_BRANCH_MAX < 128
typedef signed char hrfs_bindex_t;
#define HRFS_BRANCH_MAX 127
#else /* !(CONFIG_HRFS_BRANCH_MAX < 128) */
#define HRFS_BRANCH_MAX 32767
typedef short hrfs_bindex_t;
#endif /* !(CONFIG_HRFS_BRANCH_MAX < 128) */
#else /* !define CONFIG_HRFS_BRANCH_MAX */
#define HRFS_BRANCH_MAX 32767
typedef short hrfs_bindex_t;
#endif /* !define CONFIG_HRFS_BRANCH_MAX */

/*
 * Same to macros defined in swgfs/include/swgfs_idl.h
 * We should keep this ture:
 * When flag is zero by default,
 * it means file data is proper and not under recovering.
 * Lowest 4 bits are for raid_pattern.
 */
#define HRFS_FLAG_PRIMARY                0x00000010
#define HRFS_FLAG_DATABAD                0x00000020
#define HRFS_FLAG_RECOVERING             0x00000040
#define HRFS_FLAG_SETED                  0x00000080

#define HRFS_FLAG_MDS_MASK               0x000000ff
#define HRFS_FLAG_MDS_SYMBOL             0xc0ffee00

#define HRFS_FLAG_RAID_MASK              0x0000000f

#define HRFS_HIDDEN_FS_TYPE "hrfs-hidden"
#define HRFS_FS_TYPE "hrfs"
#define HRFS_HIDDEN_FS_DEV "none"

#define HRFS_DEFAULT_PRIMARY_BRANCH 0

static inline int hrfs_is_primary_bindex(hrfs_bindex_t bindex)
{
	return bindex == HRFS_DEFAULT_PRIMARY_BRANCH;
}

static inline hrfs_bindex_t hrfs_get_primary_bindex(void)
{
	return HRFS_DEFAULT_PRIMARY_BRANCH;
}
#if 0
static inline int hrfs_flag_is_valid(__u32 hrfs_flag)
{
        int rc = 0;

        if (((hrfs_flag & HRFS_FLAG_SETED) == 0) &&
            (hrfs_flag != 0)) {
                /* If not seted, then it is zero */
                goto out;
        }

        if ((hrfs_flag & (~HRFS_FLAG_MDS_MASK)) != 0) {
                /* Should not set unused flag*/
                goto out;
        }

        if ((hrfs_flag & HRFS_FLAG_RECOVERING) &&
            (!(hrfs_flag & HRFS_FLAG_DATABAD))) {
            	/* Branch being recovered should be bad*/
            	goto out;
        }
        rc = 1;
out:
        return rc;
}
#endif
#endif /* __HRFS_COMMON_H__ */
