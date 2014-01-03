/*
 * Copied from Lustre-2.5.2
 */

#include <linux/pagemap.h>
#include <linux/module.h>
#include <linux/mount.h>
#include <linux/sched.h>
#include <debug.h>
#include <mtfs_context.h>

static void push_group_info(struct mtfs_run_ctxt *save,
			    struct group_info *ginfo)
{
	if (!ginfo) {
		save->ngroups = current_cred()->group_info->ngroups;
		current_cred()->group_info->ngroups = 0;
	} else {
		struct cred *cred;
		task_lock(current);
		save->group_info = current_cred()->group_info;
		if ((cred = prepare_creds())) {
			cred->group_info = ginfo;
			commit_creds(cred);
		}
		task_unlock(current);
	}
}

static void pop_group_info(struct mtfs_run_ctxt *save,
			   struct group_info *ginfo)
{
	if (!ginfo) {
		current_cred()->group_info->ngroups = save->ngroups;
	} else {
		struct cred *cred;
		task_lock(current);
		if ((cred = prepare_creds())) {
			cred->group_info = save->group_info;
			commit_creds(cred);
		}
		task_unlock(current);
	}
}

/* push / pop to root of obd store */
int mtfs_push_ctxt(struct mtfs_run_ctxt *save,
		   struct mtfs_run_ctxt *new_ctx,
		   struct mtfs_ucred *uc)
{
	//MASSERT_NOT_KERNEL_CTXT("already in kernel context!\n");
	MASSERT_CTXT_MAGIC(new_ctx->magic);
	MTFS_SET_CTXT_MAGIC(save);

	save->fs = get_fs();
	MASSERT(atomic_read(&current->fs->pwd.dentry->d_count));
	MASSERT(atomic_read(&new_ctx->pwd->d_count));
	save->pwd = dget(current->fs->pwd.dentry);
	save->pwdmnt = mntget(current->fs->pwd.mnt);
	save->luc.luc_umask = current_umask();
	save->ngroups = current_cred()->group_info->ngroups;

	MASSERT(save->pwd);
	MASSERT(save->pwdmnt);
	MASSERT(new_ctx->pwd);
	MASSERT(new_ctx->pwdmnt);

	if (uc) {
		struct cred *cred = prepare_creds();
		if (cred == NULL)
			return -ENOMEM;

		save->luc.luc_cred = current->cred;
		save->luc.luc_real_cred = NULL;
		/* Get reference in saved */
		get_cred(current->cred);

		cred->uid = uc->luc_uid;
		cred->gid = uc->luc_gid;
		cred->fsuid = uc->luc_fsuid;
		cred->fsgid = uc->luc_fsgid;
		cred->cap_effective = uc->luc_cap;
		if (current->cred != current->real_cred) {
			save->luc.luc_real_cred = current->real_cred;
			/* Get reference in saved */
			get_cred(save->luc.luc_real_cred);

			/* Remedy for put_cred(current->cred) in revert_creds() */
			get_cred(current->cred);
			revert_creds(current->real_cred);
		}
		/* Remedy for 2 put_cred(current->cred) in commit_creds() */
		get_cred(current->cred);
		get_cred(current->cred);
		commit_creds(cred);

		push_group_info(save,
				uc->luc_ginfo ?: NULL);
	}
	current->fs->umask = 0;
	set_fs(new_ctx->fs);
	mtfs_set_fs_pwd(current->fs, new_ctx->pwdmnt, new_ctx->pwd);
	return 0;
}
EXPORT_SYMBOL(mtfs_push_ctxt);

void mtfs_pop_ctxt(struct mtfs_run_ctxt *saved,
		   struct mtfs_run_ctxt *new_ctx,
		   struct mtfs_ucred *uc)
{
	MASSERT_CTXT_MAGIC(saved->magic);
	MASSERT_KERNEL_CTXT("popping non-kernel context!\n");

	MASSERTF(current->fs->pwd.dentry == new_ctx->pwd, "%p != %p\n",
		 current->fs->pwd.dentry, new_ctx->pwd);
	MASSERTF(current->fs->pwd.mnt == new_ctx->pwdmnt, "%p != %p\n",
		 current->fs->pwd.mnt, new_ctx->pwdmnt);

	set_fs(saved->fs);
	mtfs_set_fs_pwd(current->fs, saved->pwdmnt, saved->pwd);

	dput(saved->pwd);
	mntput(saved->pwdmnt);
	current->fs->umask = saved->luc.luc_umask;
	if (uc) {
		if (saved->luc.luc_real_cred) {
			commit_creds((struct cred *)saved->luc.luc_real_cred);
			override_creds(saved->luc.luc_cred);
			/* Remedy for get_cred(saved->luc.luc_cred)
			 * in override_creds() */
			put_cred(saved->luc.luc_cred);
			/* Release reference in saved */
			put_cred(saved->luc.luc_real_cred);
		} else {
			commit_creds((struct cred *)saved->luc.luc_cred);
		}
		/* Release reference in saved */
		put_cred(saved->luc.luc_cred);

		pop_group_info(saved,
			       uc->luc_ginfo ?: NULL);
	}
}
EXPORT_SYMBOL(mtfs_pop_ctxt);
