/*
 * Copied from Lustre-2.5.2
 */

#ifndef __MTFS_CONTEXT_H__
#define __MTFS_CONTEXT_H__

#if defined(__linux__) && defined(__KERNEL__)

#include <linux/fs_struct.h>

struct mtfs_ucred {
	__u32			 luc_uid;
	__u32			 luc_gid;
	__u32			 luc_fsuid;
	__u32			 luc_fsgid;
	kernel_cap_t		 luc_cap;
	__u32			 luc_umask;
	struct group_info	*luc_ginfo;
	const struct cred	*luc_cred;
	const struct cred	*luc_real_cred;
};

struct mtfs_run_ctxt {
        struct vfsmount         *pwdmnt;
        struct dentry           *pwd;
        mm_segment_t             fs;
        struct mtfs_ucred        luc;
        int                      ngroups;
        struct group_info       *group_info;
#ifdef OBD_CTXT_DEBUG
        __u32                    magic;
#endif
};

#ifdef MTFS_CONTEXT_DEBUG
#define MASSERT_CTXT_MAGIC(magic) MASSERT((magic) == MTFS_RUN_CTXT_MAGIC)
#define MASSERT_NOT_KERNEL_CTXT(msg) MASSERTF(!segment_eq(get_fs(), get_ds()),\
					      msg)
#define MASSERT_KERNEL_CTXT(msg) MASSERTF(segment_eq(get_fs(), get_ds()), msg)
#define MTFS_RUN_CTXT_MAGIC      0xC0FFEEAA
#define MTFS_SET_CTXT_MAGIC(ctxt) (ctxt)->magic = MTFS_RUN_CTXT_MAGIC

#else /* !MTFS_CONTEXT_DEBUG */
#define MASSERT_CTXT_MAGIC(magic) do {} while(0)
#define MASSERT_NOT_KERNEL_CTXT(msg) do {} while(0)
#define MASSERT_KERNEL_CTXT(msg) do {} while(0)
#define MTFS_SET_CTXT_MAGIC(ctxt) do {} while(0)
#endif /* !MTFS_CONTEXT_DEBUG */

static inline void mtfs_set_fs_pwd(struct fs_struct *fs, struct vfsmount *mnt,
				   struct dentry *dentry)
{
	struct path path;
	struct path old_pwd;

	path.mnt = mnt;
	path.dentry = dentry;
	write_lock(&fs->lock);
	old_pwd = fs->pwd;
	path_get(&path);
	fs->pwd = path;
	write_unlock(&fs->lock);

	if (old_pwd.dentry)
		path_put(&old_pwd);
}

/* lvfs_linux.c */
int mtfs_push_ctxt(struct mtfs_run_ctxt *save,
		   struct mtfs_run_ctxt *new_ctx,
		   struct mtfs_ucred *uc);
void mtfs_pop_ctxt(struct mtfs_run_ctxt *saved,
		   struct mtfs_run_ctxt *new_ctx,
		   struct mtfs_ucred *cred);
#else /* !defined (__linux__) && defined(__KERNEL__) */
#error This head is only for kernel space use
#endif /* !defined (__linux__) && defined(__KERNEL__) */

#endif /* __MTFS_CONTEXT_H__ */
