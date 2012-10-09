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
#include "io_internal.h"
#include "file_internal.h"

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

#define MTFS_RESERVE_ROOT    ".mtfs"
#define MTFS_RESERVE_RECOVER "RECOVER"
#define MTFS_RESERVE_CONFIG  "CONFIG"
#define MCI_MAGIC 0xEAC1C001

static int mtfs_config_write_branch(struct super_block *sb,
                                    struct mtfs_config *mc,
                                    mtfs_bindex_t bindex)
{
	int ret = 0;
	struct mtfs_config_info *mci = &mc->mc_info;
	struct file *hidden_file = NULL;
	struct dentry *hidden_dentry = mtfs_s2bdconfig(sb, bindex);
	struct vfsmount *hidden_mnt = mtfs_s2mntbranch(sb, bindex);
	ssize_t count = sizeof(*mci);
	ssize_t size = 0;
	loff_t off = 0;
	mm_segment_t old_fs;
	HENTRY();

	hidden_file = mtfs_dentry_open(dget(hidden_dentry),
	                               mntget(hidden_mnt),
	                               O_RDWR,
	                               current_cred());
	if (IS_ERR(hidden_file)) {
		ret = PTR_ERR(hidden_file);
		HERROR("failed to open branch[%d] of file [%*s], ret = %ld\n", 
		       bindex, hidden_dentry->d_name.len, hidden_dentry->d_name.name,
		       ret);
		goto out;
	}

	mci->mci_bindex = bindex;
	HASSERT(hidden_file->f_op);
	HASSERT(hidden_file->f_op->write);
	old_fs = get_fs();
	set_fs(get_ds());
	size = hidden_file->f_op->write(hidden_file, (void *)mci, count, &off);
	set_fs(old_fs);
	if (size != count) {
		HERROR("failed to write branch[%d] of file [%*s], ret = %d\n",
		       bindex, hidden_dentry->d_name.len, hidden_dentry->d_name.name,
		       size);
		ret = -EINVAL;
		goto out_close;
	}

out_close:
	fput(hidden_file);
out:
	HRETURN(ret);
}

int mtfs_config_write(struct super_block *sb,
                      struct mtfs_config *mc)
{
	int ret = 0;
	mtfs_bindex_t bindex = 0;
	struct mtfs_config_info *mci = &mc->mc_info;
	HENTRY();

	HASSERT(mci->mci_magic == MCI_MAGIC);
	HASSERT(mc->mc_valid);
	HASSERT(mci->mci_version > 0);

	for (bindex = 0; bindex < mtfs_s2bnum(sb); bindex++) {
		if (mc->mc_blatest[bindex]) {
			HDEBUG("skip write latest branch[%d]", bindex);
			continue;
		}

		HASSERT(mc->mc_nonlatest > 0);
		ret = mtfs_config_write_branch(sb, mc, bindex);
		if (ret) {
			HERROR("failed to write config of branch[%d]\n", bindex);
			goto out;
		}
		mc->mc_blatest[bindex] = 1;
		mc->mc_nonlatest--;
	}

	HASSERT(mc->mc_nonlatest == 0);
out:
	HRETURN(ret);
}

static struct mtfs_config *mtfs_config_read_branch(struct super_block *sb,
                                                   mtfs_bindex_t bindex)
{
	int ret = 0;
	struct mtfs_config *mc = NULL;
	struct mtfs_config_info *mci = NULL;
	struct file *hidden_file = NULL;
	struct dentry *hidden_dentry = mtfs_s2bdconfig(sb, bindex);
	struct vfsmount *hidden_mnt = mtfs_s2mntbranch(sb, bindex);
	ssize_t count = 0;
	ssize_t size = 0;
	loff_t off = 0;
	mm_segment_t old_fs;
	HENTRY();

	MTFS_ALLOC_PTR(mc);
	if (mc == NULL) {
		ret = -ENOMEM;
		HERROR("not enough memory\n");
		goto out;
	}
	mci = &mc->mc_info;

	hidden_file = mtfs_dentry_open(dget(hidden_dentry),
	                               mntget(hidden_mnt),
	                               O_RDONLY,
	                               current_cred());
	if (IS_ERR(hidden_file)) {
		ret = PTR_ERR(hidden_file);
		HERROR("failed to open branch[%d] of file [%*s], ret = %ld\n", 
		       bindex, hidden_dentry->d_name.len, hidden_dentry->d_name.name,
		       ret);
		goto out_free_mc;
	}

	count = i_size_read(hidden_file->f_dentry->d_inode);
	if (count != sizeof(*mci) && count != 0) {
		HERROR("size of branch[%d] of file [%*s] does not match, " 
		       "expected %lu, got %lu\n",
		       bindex, hidden_dentry->d_name.len,
		       hidden_dentry->d_name.name,
		       sizeof(*mci), count);
		ret = -EINVAL;
		goto out_close;
	}

	if (count == 0) {
		HDEBUG("size of branch[%d] of file [%*s] is zero\n",
		       bindex, hidden_dentry->d_name.len,
		       hidden_dentry->d_name.name);
		goto out_close;
	}

	HASSERT(hidden_file->f_op);
	HASSERT(hidden_file->f_op->read);
	old_fs = get_fs();
	set_fs(get_ds());
	size = hidden_file->f_op->read(hidden_file, (void *)mci, count, &off);
	set_fs(old_fs);
	if (size != count) {
		HERROR("failed to read branch[%d] of file [%*s], ret = %d\n",
		       bindex, hidden_dentry->d_name.len,
		       hidden_dentry->d_name.name,
		       size);
		ret = -EINVAL;
		goto out_close;
	}
	ret = 0;

	if (mci->mci_magic != MCI_MAGIC) {
		HERROR("bad maigc of branch[%d] of file [%*s], "
		       "expected %x, got %x\n",
		       bindex, hidden_dentry->d_name.len,
		       hidden_dentry->d_name.name,
		       MCI_MAGIC, mci->mci_magic);
		ret = -EINVAL;
		goto out_close;
	}

	if (mci->mci_bnum != mtfs_s2bnum(sb)) {
		HERROR("bad bnum of branch[%d] of file [%*s], "
		       "expected %d, got %d\n",
		       bindex, hidden_dentry->d_name.len,
		       hidden_dentry->d_name.name,
		       mtfs_s2bnum(sb), mci->mci_bnum);
		ret = -EINVAL;
		goto out_close;
	}

	if (mci->mci_bindex != bindex) {
		HERROR("bad bindex of branch[%d] of file [%*s], "
		       "expected %d, got %d\n",
		       bindex, hidden_dentry->d_name.len,
		       hidden_dentry->d_name.name,
		       bindex, mci->mci_bindex);
		ret = -EINVAL;
		goto out_close;
	}
	mc->mc_valid = 1;
out_close:
	fput(hidden_file);
out_free_mc:
	if (ret) {
		MTFS_FREE_PTR(mc);
		mc = NULL;
	}
out:
	if (ret) {
		HASSERT(mc == NULL);
		mc = ERR_PTR(ret);
	}
	HASSERT(mc != NULL);
	HRETURN(mc);
}

/* If no config file is written, return NULL */
struct mtfs_config *mtfs_config_read(struct super_block *sb)
{
	int ret = 0;
	struct mtfs_config **mc_array = NULL;
	struct mtfs_config *mc_chosen = NULL;
	mtfs_bindex_t bnum = mtfs_s2bnum(sb);
	mtfs_bindex_t bindex = 0;
	__u64 highest_version = 0;
	HENTRY();

	MTFS_ALLOC(mc_array, sizeof(*mc_array) * bnum);
	if (mc_array == NULL) {
		ret = -ENOMEM;
		HERROR("not enough memory\n");
		goto out;
	}

	for (bindex = 0; bindex < bnum; bindex++) {
		mc_array[bindex] = mtfs_config_read_branch(sb, bindex);
		if (IS_ERR(mc_array[bindex])) {
			ret = PTR_ERR(mc_array[bindex]);
			HERROR("failed to read branch[%d], ret = %d\n",
			       bindex, ret);
			bindex--;
			goto out_free;
		}
	}

	for (bindex = 0; bindex < bnum; bindex++) {
		if (mc_array[bindex]->mc_valid &&
		    highest_version < mc_array[bindex]->mc_info.mci_version) {
			highest_version = mc_array[bindex]->mc_info.mci_version;
			mc_chosen = mc_array[bindex];
		}
	}

	if (mc_chosen == NULL) {
		goto out_free_all;
	}

	for (bindex = 0; bindex < bnum; bindex++) {
		if (!mc_array[bindex]->mc_valid ||
		    mc_array[bindex]->mc_info.mci_version < highest_version) {
		    	mc_chosen->mc_nonlatest ++;
			continue;
		} else {
			mc_chosen->mc_blatest[bindex] = 1;
		}
	}

out_free_all:
	bindex = bnum - 1;
out_free:
	for(; bindex >=0; bindex--) {
		if (mc_array[bindex] != mc_chosen) {
			MTFS_FREE_PTR(mc_array[bindex]);
		}
	}

	MTFS_FREE(mc_array, sizeof(*mc_array) * bnum);
out:
	if (ret) {
		mc_chosen = ERR_PTR(ret);
	}
	HRETURN(mc_chosen);
}

struct mtfs_config *mtfs_config_init(struct mount_option *mount_option)
{
	struct mtfs_config *mc = NULL;
	struct mtfs_config_info *mci = NULL;
	HENTRY();
	
	MTFS_ALLOC_PTR(mc);
	if (mc == NULL) {
		HERROR("not enough memory\n");
		goto out;
	}
	mci = &mc->mc_info;

	mci->mci_magic = MCI_MAGIC;
	mci->mci_version = 1;
	mci->mci_bindex= 0;
	mci->mci_bnum = mount_option->bnum;
	mc->mc_valid = 1;
	mc->mc_nonlatest = mount_option->bnum;
out:
	HRETURN(mc);
}

void mtfs_config_dump(struct mtfs_config_info *mci)
{
	HPRINT("mci_magic = %x\n", mci->mci_magic);
	HPRINT("mci_version = %llx\n", mci->mci_version);
	HPRINT("mci_bindex = %d\n", mci->mci_bindex);
	HPRINT("mci_bnum = %d\n", mci->mci_bnum);
}

int mtfs_reserve_init_branch(struct dentry *d_root, mtfs_bindex_t bindex)
{
	struct dentry *hidden_child = NULL;
	struct dentry *hidden_parent = NULL;
	char *name = NULL;
	int ret = 0;
	struct super_block *sb = d_root->d_sb;
	HENTRY();

	hidden_parent = mtfs_d2branch(d_root, bindex);
	HASSERT(hidden_parent);
	name = MTFS_RESERVE_ROOT;
	hidden_child = mtfs_dchild_create(hidden_parent,
	                                  name, strlen(name),
	                                  S_IFDIR | S_IRWXU, 0, 0);
	if (IS_ERR(hidden_child)) {
		ret = PTR_ERR(hidden_child);
		HERROR("create branch[%d] of [%*s/%s] failed, ret = %d\n",
		       bindex, hidden_parent->d_name.len,
		       hidden_parent->d_name.name, name, ret);
		goto out;
	}
	mtfs_s2bdreserve(sb, bindex) = hidden_child;

	hidden_parent = mtfs_s2bdreserve(sb, bindex);
	HASSERT(hidden_parent);
	name = MTFS_RESERVE_RECOVER;
	hidden_child = mtfs_dchild_create(hidden_parent,
	                                  name, strlen(name),
	                                  S_IFDIR | S_IRWXU, 0, 0);
	if (IS_ERR(hidden_child)) {
		ret = PTR_ERR(hidden_child);
		HERROR("create branch[%d] of [%*s/%s] failed, ret = %d\n",
		       bindex, hidden_parent->d_name.len,
		       hidden_parent->d_name.name, name, ret);
		goto out_put_reserve;
	}
	mtfs_s2bdrecover(sb, bindex) = hidden_child;

	hidden_parent = mtfs_s2bdreserve(sb, bindex);
	HASSERT(hidden_parent);
	name = MTFS_RESERVE_CONFIG;
	hidden_child = mtfs_dchild_create(hidden_parent,
	                                  name, strlen(name),
	                                  S_IFREG | S_IRWXU, 0, 0);
	if (IS_ERR(hidden_child)) {
		ret = PTR_ERR(hidden_child);
		HERROR("create branch[%d] of [%*s/%s] failed, ret = %d\n",
		       bindex, hidden_parent->d_name.len,
		       hidden_parent->d_name.name, name, ret);
		goto out_put_recover;
	}
	mtfs_s2bdconfig(sb, bindex) = hidden_child;

	goto out;
out_put_recover:
	dput(mtfs_s2bdrecover(sb, bindex));
	mtfs_s2bdrecover(sb, bindex) = NULL;
out_put_reserve:
	dput(mtfs_s2bdreserve(sb, bindex));
	mtfs_s2bdreserve(sb, bindex) = NULL;
out:
	HRETURN(ret);
}

void mtfs_reserve_fini_branch(struct super_block *sb, mtfs_bindex_t bindex)
{
	HASSERT(mtfs_s2bdconfig(sb, bindex));
	dput(mtfs_s2bdconfig(sb, bindex));
	mtfs_s2bdconfig(sb, bindex) = NULL;

	HASSERT(mtfs_s2bdrecover(sb, bindex));
	dput(mtfs_s2bdrecover(sb, bindex));
	mtfs_s2bdrecover(sb, bindex) = NULL;

	HASSERT(mtfs_s2bdreserve(sb, bindex));
	dput(mtfs_s2bdreserve(sb, bindex));
	mtfs_s2bdreserve(sb, bindex) = NULL;
}

int mtfs_reserve_init(struct dentry *d_root, struct mount_option *mount_option)
{
	int ret = 0;
	mtfs_bindex_t bindex = 0;
	struct mtfs_config *mc = NULL;
	struct super_block *sb = d_root->d_sb;
	HENTRY();

	HASSERT(d_root);
	for (bindex = 0; bindex < mtfs_d2bnum(d_root); bindex++) {
		ret = mtfs_reserve_init_branch(d_root, bindex);
		if (ret) {
			HERROR("failed to init branch[%d]\n", bindex);
			bindex--;
			goto out_fini;
		}
	}
	bindex = mtfs_d2bnum(d_root) -1 ;

	mc = mtfs_config_read(d_root->d_sb); 
	if (IS_ERR(mc)) {
		HERROR("failed to read config\n");
		ret = PTR_ERR(mc);
		goto out_fini;
	}

	if (mc == NULL) {
		HDEBUG("generating config\n");
		mc = mtfs_config_init(mount_option);
		if (mc == NULL) {
			HERROR("failed to init config\n");
			goto out_fini;
		}
	}

	if (mc->mc_nonlatest) {
		ret = mtfs_config_write(d_root->d_sb, mc);
		if (ret) {
			HERROR("failed to write config, ret = %d\n", ret);
			goto out_free_mc;
		}
	}

	mtfs_s2config(sb) = mc;

	goto out;
out_free_mc:
	MTFS_FREE_PTR(mc);
out_fini:
	for (; bindex >= 0; bindex--) {
		mtfs_reserve_fini_branch(d_root->d_sb, bindex);
	}
out:
	HRETURN(ret);
}

void mtfs_reserve_fini(struct super_block *sb)
{
	mtfs_bindex_t bindex = 0;

	HASSERT(sb);
	for (bindex = 0; bindex < mtfs_s2bnum(sb); bindex++) {
		mtfs_reserve_fini_branch(sb, bindex);
	}

	MTFS_FREE_PTR(mtfs_s2config(sb));
}

int mtfs_read_super(struct super_block *sb, void *input, int silent)
{
	int ret = 0;
	struct mount_option mount_option;
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

	ret = mtfs_reserve_init(d_root, &mount_option);
	if (unlikely(ret)) {
		goto out_put;
	}

	device = mtfs_newdev(sb, &mount_option);
	HASSERT(device);
	if (IS_ERR(device)) {
		ret = PTR_ERR(device);
		goto out_finit_reserve;
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
out_finit_reserve:
	mtfs_reserve_fini(sb);
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
	mount_option_fini(&mount_option);
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
struct kmem_cache *mtfs_io_cache;

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
	{
		.cache = &mtfs_io_cache,
		.name = "mtfs_io_cache",
		.size = sizeof(struct mtfs_io),
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
