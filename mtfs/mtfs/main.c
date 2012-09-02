/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/module.h>
#include <linux/mount.h>
#include <parse_option.h>
#include <compat.h>
#include <proc.h>
#include <mtfs_selfheal.h>
#include <mtfs_device.h>
#include <mtfs_file.h>
#include "hide_internal.h"
#include "super_internal.h"
#include "dentry_internal.h"
#include "heal_internal.h"
#include "device_internal.h"

/* This definition must only appear after we include <linux/module.h> */
#ifndef MODULE_LICENSE
#define MODULE_LICENSE(foo)
#endif /* MODULE_LICENSE */

DECLARE_RWSEM(global_rwsem);

int mtfs_init_super(struct super_block *sb, struct mtfs_device *device, struct dentry *d_root)
{
	struct super_operations *sops = NULL;
	struct dentry_operations *dops = NULL;
	struct mtfs_operations *operations = NULL;
	mtfs_bindex_t bindex = 0;
	int ret = 0;
	HENTRY();

	mtfs_s2dev(sb) = device;

	operations = mtfs_dev2ops(device);

	sops = operations->sops;
	if (sops == NULL) {
		HDEBUG("super operations not supplied, use default\n");
		sops = &mtfs_sops;
	}

	dops = operations->dops;
	if (dops == NULL) {
		HDEBUG("dentry operations not supplied, use default\n");
		dops = &mtfs_dops;
	}

	for (bindex = 0; bindex < mtfs_s2bnum(sb); bindex++) {
		if (sb->s_maxbytes < mtfs_s2branch(sb, bindex)->s_maxbytes) {
			sb->s_maxbytes = mtfs_s2branch(sb, bindex)->s_maxbytes;
		}
	}
	sb->s_op = sops;
	sb->s_root = d_root;
	sb->s_root->d_op = dops;
	sb->s_root->d_sb = sb;
	sb->s_root->d_parent = sb->s_root;
	ret = mtfs_interpose(sb->s_root, sb, INTERPOSE_SUPER);

	HRETURN(ret);
}

int mtfs_init_recover(struct dentry *d_root)
{
	int ret = 0;
	struct dentry *hidden_child = NULL;
	struct dentry *hidden_parent = NULL;
	mtfs_bindex_t bindex = 0;
	const char *name = ".mtfs";
	HENTRY();

	HASSERT(d_root);
	for (bindex = 0; bindex < mtfs_d2bnum(d_root); bindex++) {
		hidden_parent = mtfs_d2branch(d_root, bindex);
		HASSERT(hidden_parent);
		hidden_child = mtfs_dchild_create(hidden_parent, name, strlen(name), S_IFDIR | S_IRWXU, 0, 0);
		if (IS_ERR(hidden_child)) {
			ret = PTR_ERR(hidden_child);
			HERROR("create branch[%d] of [%*s/%s] failed\n",
			       bindex, hidden_parent->d_name.len, hidden_parent->d_name.name, name);
			bindex--;
			goto out_put_recover;
		}
		mtfs_s2brecover(d_root->d_sb, bindex) = hidden_child;
	}
	goto out;
out_put_recover:
	for (; bindex >= 0; bindex--) {
		HASSERT(mtfs_s2brecover(d_root->d_sb, bindex));
		dput(mtfs_s2brecover(d_root->d_sb, bindex));
	}
out:
	HRETURN(ret);
}

void mtfs_finit_recover(struct dentry *d_root)
{
	mtfs_bindex_t bindex = 0;
	HASSERT(d_root);
	for (bindex = 0; bindex < mtfs_d2bnum(d_root); bindex++) {
		HASSERT(mtfs_s2brecover(d_root->d_sb, bindex));
		dput(mtfs_s2brecover(d_root->d_sb, bindex));
	}
}

int mtfs_read_super(struct super_block *sb, void *input, int silent)
{
	int ret = 0;
	mount_option_t mount_option;
	mtfs_bindex_t bnum = 0;
	mtfs_bindex_t bindex = 0;
	struct dentry *d_root = NULL;
	static const struct qstr name = { .name = "/", .len = 1 };
	struct mtfs_device *device = NULL;
	HENTRY();

	ret = mtfs_parse_options((char *)input, &mount_option);
	if (unlikely(ret)) {
		HERROR("fail to parse mount option\n");
		goto out;
	}

	bnum = mount_option.bnum;
	HASSERT(bnum > 0);

	d_root = d_alloc(NULL, &name);
	if (unlikely(d_root == NULL)) {
		ret = -ENOMEM;
		HERROR("d_alloc failed\n");
		goto out_option_finit;
	}
	/* Otherwise dput will crash, see d_alloc_root */
	d_root->d_parent = d_root;

	ret = mtfs_d_alloc(d_root, bnum);
	if (unlikely(ret)) {
		goto out_dput;
	}

	ret = mtfs_s_alloc(sb, bnum);
	if (unlikely(ret)) {
		goto out_d_free;
	}

	for (bindex = 0; bindex < bnum; bindex++) {
		char *name = NULL;
		struct nameidata nd;
		struct dentry *dentry = NULL;
		struct vfsmount *mnt = NULL;

		name = mount_option.branch[bindex].path;
		HASSERT(name);
		HDEBUG("using directory: %s\n", name);
		ret = path_lookup(name, LOOKUP_FOLLOW, &nd);
		if (unlikely(ret)) {
			HERROR("error accessing hidden directory '%s'\n", name);
			bindex--;
			goto out_put;
		}
#ifdef HAVE_PATH_IN_STRUCT_NAMEIDATA
		dentry = nd.path.dentry;
		mnt = nd.path.mnt;
#else /* !HAVE_PATH_IN_STRUCT_NAMEIDATA */
		dentry = nd.dentry;
		mnt = nd.mnt;
#endif /* !HAVE_PATH_IN_STRUCT_NAMEIDATA */
		if (unlikely(dentry->d_inode == NULL)) {
			ret = -ENOENT; /* Which errno? */
			HERROR("directory %s has no inode\n", name);
			goto out_put;
		}
		mtfs_d2branch(d_root, bindex) = dentry;
		mtfs_s2mntbranch(sb, bindex) = mnt;
		mtfs_s2branch(sb, bindex) = dentry->d_sb;
	}
	bindex --;

	sb->s_root = d_root;
	d_root->d_sb = sb;

	ret = mtfs_init_recover(d_root);
	if (unlikely(ret)) {
		goto out_put;
	}

	device = mtfs_newdev(sb, &mount_option);
	HASSERT(device);
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		goto out_finit_recover;
	}

	HASSERT(mtfs_s2bnum(sb) == bnum);
	HASSERT(mtfs_d2bnum(d_root) == bnum);

	HDEBUG("d_count = %d\n", atomic_read(&d_root->d_count));
	ret = mtfs_init_super(sb, device, d_root);
	if (unlikely(ret)) {
		goto out_free_dev;
	}
	goto out_option_finit;
out_free_dev:
	mtfs_freedev(mtfs_s2dev(sb));
out_finit_recover:
	mtfs_finit_recover(d_root);
out_put:
	for(; bindex >=0; bindex--) {
		HASSERT(mtfs_d2branch(d_root, bindex));
		dput(mtfs_d2branch(d_root, bindex));

		HASSERT(mtfs_s2mntbranch(sb, bindex));
		mntput(mtfs_s2mntbranch(sb, bindex));
	}
	/*
	 * Clear hear, since sb->s_root may set to d_root
	 * Otherwise, shrink_dcache_for_umount() will
	 * encounter a sb->s_root which has been freed
	 */
	sb->s_root = NULL;
//out_s_free:
	mtfs_s_free(sb);
out_d_free:
	mtfs_d_free(d_root);
out_dput:
	dput(d_root);
out_option_finit:
	mount_option_finit(&mount_option);
out:
	HRETURN(ret);
}

static int mtfs_get_sb(struct file_system_type *fs_type,
		int flags, const char *dev_name,
		void *raw_data, struct vfsmount *mnt)
{
	return get_sb_nodev(fs_type, flags, raw_data, mtfs_read_super, mnt);
}

void mtfs_kill_block_super(struct super_block *sb)
{
	generic_shutdown_super(sb);
}

static struct file_system_type mtfs_fs_type = {
	owner:     THIS_MODULE,
	name:      MTFS_FS_TYPE,
	get_sb:    mtfs_get_sb,
	kill_sb:   mtfs_kill_block_super,
	fs_flags:  MTFS_RENAME_DOES_D_MOVE,
};

static void mtfs_inode_info_init_once(void *vptr)
{
	struct mtfs_inode_info *hi = (struct mtfs_inode_info *)vptr;

	inode_init_once(&hi->mii_inode);
}

struct mtfs_cache_info {
	struct kmem_cache **cache;
	const char *name;
	size_t size;
	void (*ctor)(void *obj); /* For linux-2.6.30 upper */
};

struct kmem_cache *mtfs_file_info_cache;
struct kmem_cache *mtfs_dentry_info_cache;
struct kmem_cache *mtfs_inode_info_cache;
struct kmem_cache *mtfs_sb_info_cache;
struct kmem_cache *mtfs_device_cache;
struct kmem_cache *mtfs_oplist_cache;
struct kmem_cache *mtfs_lock_cache;
struct kmem_cache *mtfs_interval_cache;

static struct mtfs_cache_info mtfs_cache_infos[] = {
	{
		.cache = &mtfs_file_info_cache,
		.name = "mtfs_file_cache",
		.size = sizeof(struct mtfs_file_info),
	},
	{
		.cache = &mtfs_dentry_info_cache,
		.name = "mtfs_dentry_info_cache",
		.size = sizeof(struct mtfs_dentry_info),
	},
	{
		.cache = &mtfs_inode_info_cache,
		.name = "mtfs_inode_cache",
		.size = sizeof(struct mtfs_inode_info),
		.ctor = mtfs_inode_info_init_once, /* For linux-2.6.30 upper */
	},
	{
		.cache = &mtfs_sb_info_cache,
		.name = "mtfs_sb_cache",
		.size = sizeof(struct mtfs_sb_info),
	},
	{
		.cache = &mtfs_device_cache,
		.name = "mtfs_device_cache",
		.size = sizeof(struct mtfs_device),
	},
	{
		.cache = &mtfs_oplist_cache,
		.name = "mtfs_oplist_cache",
		.size = sizeof(struct mtfs_operation_list),
	},
	{
		.cache = &mtfs_lock_cache,
		.name = "mtfs_lock_cache",
		.size = sizeof(struct mlock),
	},
	{
		.cache = &mtfs_interval_cache,
		.name = "mtfs_interval_cache",
		.size = sizeof(struct mlock_interval),
	},
};
	
static void mtfs_free_kmem_caches(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(mtfs_cache_infos); i++) {
		struct mtfs_cache_info *info;

		info = &mtfs_cache_infos[i];
		if (*(info->cache)) {
			kmem_cache_destroy(*(info->cache));
		}
	}
}

static int mtfs_init_kmem_caches(void)
{
	int i = 0;
	int ret = 0;

	for (i = 0; i < ARRAY_SIZE(mtfs_cache_infos); i++) {
		struct mtfs_cache_info *info;

		info = &mtfs_cache_infos[i];
#ifdef HAVE_KMEM_CACHE_CREATE_DTOR
        	*(info->cache) = kmem_cache_create(info->name, info->size,
        	                                   0, SLAB_HWCACHE_ALIGN, NULL, NULL);
#else /* !HAVE_KMEM_CACHE_CREATE_DTOR */
        	*(info->cache) = kmem_cache_create(info->name, info->size,
        	                                   0, SLAB_HWCACHE_ALIGN, NULL);
#endif /* !HAVE_KMEM_CACHE_CREATE_DTOR */
		if (*(info->cache) == NULL) {
			mtfs_free_kmem_caches();
			HERROR("kmem_cache_create failed\n");
			ret = -ENOMEM;
			goto out;
		}
	}

out:
	return ret;
}


/* Root for /proc/fs/mtfs */
struct proc_dir_entry *mtfs_proc_root = NULL;
struct proc_dir_entry *mtfs_proc_device = NULL;

struct mtfs_proc_vars mtfs_proc_vars_base[] = {
	{ "device_list", mtfs_proc_read_devices, NULL, NULL },
	{ 0 }
};

#define MTFS_PROC_DEVICE_NAME "devices"

int mtfs_insert_proc(void)
{
	int ret = 0;
	HENTRY();

	mtfs_proc_root = mtfs_proc_register("fs/mtfs", NULL,
                                        mtfs_proc_vars_base, NULL);
	if (unlikely(mtfs_proc_root == NULL)) {
		HERROR("failed to register /proc/fs/mtfs\n");
		ret = -ENOMEM;
		goto out;
	}

	mtfs_proc_device = mtfs_proc_register(MTFS_PROC_DEVICE_NAME, mtfs_proc_root,
                                        NULL, NULL);
	if (unlikely(mtfs_proc_device == NULL)) {
		HERROR("failed to register /proc/fs/mtfs"MTFS_PROC_DEVICE_NAME"\n");
		ret = -ENOMEM;
		goto out;
	}
out:
	HRETURN(ret);
}

void mtfs_remove_proc(void)
{
	if (mtfs_proc_root) {
		mtfs_proc_remove(&mtfs_proc_root);
	}
	if (mtfs_proc_device) {
		mtfs_proc_remove(&mtfs_proc_device);
	}		
}

static int __init mtfs_init(void)
{
	int ret = 0;

	HDEBUG("Registering mtfs\n");

	ret = mtfs_init_kmem_caches();
	if (ret) {
		HERROR("Failed to allocate one or more kmem_cache objects\n");
		goto out;
	}

	ret = mtfs_insert_proc();
	if (ret) {
		HERROR("failed to insert_proc: error %d\n", ret);
		goto out_free_kmem;
	}
	
	ret = register_filesystem(&mtfs_fs_type);
	if (ret) {
		HERROR("failed to register filesystem type mtfs: error %d\n", ret);
		goto out_remove_proc;
	}

	ret = register_filesystem(&mtfs_hidden_fs_type);
	if (ret) {
		HERROR("failed to register filesystem type hidden_mtfs: error %d\n", ret);
		goto out_unregister_mtfs;
	}

	goto out;
out_unregister_mtfs:
	unregister_filesystem(&mtfs_fs_type);
out_remove_proc:
	mtfs_remove_proc();
out_free_kmem:
	mtfs_free_kmem_caches();
out:
	return ret;
}

static void __exit mtfs_exit(void)
{
	HDEBUG("Unregistering mtfs\n");
	unregister_filesystem(&mtfs_hidden_fs_type);
	unregister_filesystem(&mtfs_fs_type);
	mtfs_remove_proc();
	mtfs_free_kmem_caches();
}

MODULE_AUTHOR("MulTi File System Development Workgroup");
MODULE_DESCRIPTION("mtfs");
MODULE_LICENSE("GPL");

module_init(mtfs_init)
module_exit(mtfs_exit)
