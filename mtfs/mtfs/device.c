/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/ctype.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <compat.h>
#include <mtfs_list.h>
#include <mtfs_proc.h>
#include <mtfs_dentry.h>
#include "device_internal.h"
#include "junction_internal.h"
#include "lowerfs_internal.h"
#include "subject_internal.h"

static struct mtfs_device *mtfs_device_alloc(struct mount_option *mount_option)
{
	struct mtfs_device *device = NULL;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bnum = mount_option->bnum;

	MASSERT(bnum > 0 && bnum <= MTFS_BRANCH_MAX);
	MTFS_SLAB_ALLOC_PTR(device, mtfs_device_cache);
	if (device == NULL) {
		goto out;
	}

	mtfs_dev2bnum(device) = bnum;
	mtfs_dev2flags(device) = mount_option->mo_flags;

	for(bindex = 0; bindex < bnum; bindex++) {
		int length = mount_option->branch[bindex].length;

		mtfs_dev2blength(device, bindex) = length;
		mtfs_dev2namelen(device) += length;
		MTFS_ALLOC(mtfs_dev2bpath(device, bindex), length);
		if (mtfs_dev2bpath(device, bindex) == NULL) {
			bindex --;
			goto out_free_path;
		}
		memcpy(mtfs_dev2bpath(device, bindex), mount_option->branch[bindex].path, length - 1);
	}

	MTFS_ALLOC(mtfs_dev2name(device), mtfs_dev2namelen(device));
	if (mtfs_dev2name(device) == NULL) {
		goto out_free_path;
	}

	for(bindex = 0; bindex < bnum; bindex++) {
		append_dir(mtfs_dev2name(device), mount_option->branch[bindex].path);
	}

	goto out;
//out_free_name:
	MTFS_FREE(mtfs_dev2name(device), mtfs_dev2namelen(device));
out_free_path:
	for(; bindex >= 0; bindex--) {
		MTFS_FREE(mtfs_dev2bpath(device, bindex), mtfs_dev2blength(device, bindex));
	}
//out_free_branch:
	MTFS_SLAB_FREE_PTR(device, mtfs_device_cache);
	MASSERT(device == NULL);
out:
	return device;
}

static void mtfs_device_free(struct mtfs_device *device)
{
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bnum = mtfs_dev2bnum(device);
	MASSERT(device != NULL);

	for(bindex = 0; bindex < bnum; bindex++) {
		MTFS_FREE(mtfs_dev2bpath(device, bindex), mtfs_dev2blength(device, bindex));
	}
	MTFS_FREE(mtfs_dev2name(device), mtfs_dev2namelen(device));
	MTFS_SLAB_FREE_PTR(device, mtfs_device_cache);
}

spinlock_t mtfs_device_lock = SPIN_LOCK_UNLOCKED;
static MTFS_LIST_HEAD(mtfs_devs);

static struct mtfs_device *_mtfs_search_device(struct mtfs_device *device)
{
	struct mtfs_device *found = NULL;
	mtfs_list_t        *p = NULL;
	mtfs_bindex_t       bindex = 0;
	mtfs_bindex_t       bnum = 0;
	struct dentry      *d_root  = mtfs_dev2sb(device)->s_root;
	struct dentry      *d_root_tmp = NULL;
	int                 have_same = 0;
	int                 have_diff = 0;

	mtfs_list_for_each(p, &mtfs_devs) {
		found = mtfs_list_entry(p, typeof(*found), md_list);
		d_root_tmp = mtfs_dev2sb(found)->s_root;

		MASSERT(d_root_tmp);
		bnum = mtfs_dev2bnum(device);
		if (bnum > mtfs_dev2bnum(found)) {
			bnum = mtfs_dev2bnum(found);
		}
		if (bnum != mtfs_dev2bnum(found) ||
		    bnum != mtfs_dev2bnum(device)) {
		    have_diff = 1;
		}

		for(bindex = 0; bindex < bnum; bindex++) {
			//if ((strcmp(mtfs_dev2bpath(found, bindex), mtfs_dev2bpath(device, bindex)) == 0) ||
			if (mtfs_d2branch(d_root, bindex) == mtfs_d2branch(d_root_tmp, bindex)) {
				/*
				 * Only lowerfs dentry comparison maybe enough.
				 * Path comparison seems to be wasteful and unuseful.
				 */
				have_same = 1;
			} else {
				have_diff = 1;
			}
		}

		if (have_same) {
			if (!have_diff) {
				return found;
			} else {
				return ERR_PTR(-EEXIST);
			}
		}
	}
	return NULL;
}

static int _mtfs_register_device(struct mtfs_device *device)
{
	struct mtfs_device *found = NULL;
	int ret = 0;

	found = _mtfs_search_device(device);
	if (found != NULL) {
		if (found == device) {
			MERROR("register same devices %s for mutiple times\n",
			       mtfs_dev2name(device));
		} else {
			MERROR("register multiple devices for %s\n",
			       mtfs_dev2name(device));
		}
		ret = -EEXIST;
	} else {
		list_add(&device->md_list, &mtfs_devs);
	}

	return ret;
}

static int mtfs_device_proc_read_name(char *page, char **start, off_t off, int count,
                               int *eof, void *data)
{
	int ret = 0;
	struct mtfs_device *device = (struct mtfs_device *)data;

	*eof = 1;
	ret = snprintf(page, count, "%s\n", mtfs_dev2name(device));
	return ret;
}

static int mtfs_device_proc_read_bnum(char *page, char **start, off_t off, int count,
                               int *eof, void *data)
{
	int ret = 0;
	struct mtfs_device *device = (struct mtfs_device *)data;

	*eof = 1;
	ret = snprintf(page, count, "%d\n", mtfs_dev2bnum(device));
	return ret;
}

static int mtfs_device_proc_read_checksum(char *page, char **start, off_t off, int count,
                                   int *eof, void *data)
{
	int ret = 0;
	struct mtfs_device *device = (struct mtfs_device *)data;

	*eof = 1;
	ret = snprintf(page, count, "%d\n", mtfs_dev2checksum(device) ? 1 : 0);
	return ret;
}

static int mtfs_device_proc_read_noabort(char *page, char **start, off_t off, int count,
                                  int *eof, void *data)
{
	int ret = 0;
	struct mtfs_device *device = (struct mtfs_device *)data;

	*eof = 1;
	ret = snprintf(page, count, "%d\n", mtfs_dev2noabort(device) ? 1 : 0);
	return ret;
}

static const char *mdevice_debug2str(int write_type)
{
	switch (write_type) {
	default:
		return NULL;
	case MDEVICE_WRITE_NORMAL:
		return MDEVICE_WRITE_NORMAL_STRING;
	case MDEVICE_WRITE_TRUNCATE:
		return MDEVICE_WRITE_TRUNCATE_STRING;
        }
}

static int mdevice_str2debug(const char *buf)
{
	int ret = -EINVAL;

	if (strncmp(MDEVICE_WRITE_NORMAL_STRING,
	            buf,
	            strlen(MDEVICE_WRITE_NORMAL_STRING)) == 0) {
		ret = MDEVICE_WRITE_NORMAL;
	} else if (strncmp(MDEVICE_WRITE_TRUNCATE_STRING,
	           buf,
	           strlen(MDEVICE_WRITE_TRUNCATE_STRING)) == 0) {
		ret = MDEVICE_WRITE_TRUNCATE;
	}

	return ret;
}

static int mdevice_proc_debug_read(char *page, char **start, off_t off, int count,
                                   int *eof, void *data)
{
	int ret = 0;
	struct mtfs_device *device = (struct mtfs_device *)data;

	*eof = 1;
	ret = snprintf(page, count, "%s\n", mdevice_debug2str(mtfs_dev2write(device)));

	return ret;
}

static int mdevice_proc_debug_write(struct file *file, const char *buffer,
                                    unsigned long count, void *data)
{
	int ret = 0;
	char kern_buf[20];
	struct mtfs_device *device = (struct mtfs_device *)data;
	int debug_write = 0;
	MENTRY();

	if (count > (sizeof(kern_buf) - 1)) {
		ret = -EINVAL;
		goto out;
	}

	if (copy_from_user(kern_buf, buffer, count)) {
		ret = -EINVAL;
		goto out;
	}
	kern_buf[count] = '\0';

	debug_write = mdevice_str2debug(kern_buf);
	if (debug_write < 0) {
		goto out;
	}

	mtfs_dev2write(device) = debug_write;
out:
	if (ret) {
		MRETURN(ret);
	}
	MRETURN(count);
}

static struct mtfs_proc_vars mtfs_proc_vars_device[] = {
	{ "device_name", mtfs_device_proc_read_name, NULL, NULL },
	{ "bnum", mtfs_device_proc_read_bnum, NULL, NULL },
	{ "checksum", mtfs_device_proc_read_checksum, NULL, NULL },
	{ "noabort", mtfs_device_proc_read_noabort, NULL, NULL },
	{ "debug_write", mdevice_proc_debug_read, mdevice_proc_debug_write, NULL },
	{ 0 }
};

#define MTFS_ERRNO_INACTIVE "inactive"
static int mtfs_device_branch_proc_read_errno(char *page, char **start, off_t off, int count,
                                       int *eof, void *data)
{
	int ret = 0;
	struct mtfs_device_branch *dev_branch = (struct mtfs_device_branch *)data;

	*eof = 1;
	if (dev_branch->mdb_debug.mbd_active) {
		ret = snprintf(page, count, "%d\n", dev_branch->mdb_debug.mbd_errno);
	} else {
		ret = snprintf(page, count, MTFS_ERRNO_INACTIVE"\n");
	}
	return ret;
}

static int mtfs_device_branch_proc_write_errno(struct file *file, const char *buffer,
                                               unsigned long count, void *data)
{
	int ret = 0;
	char kern_buf[20];
	char *end = NULL;
	char *pbuf = NULL;
	int sign = 1;
	int var = 0;
	struct mtfs_device_branch *dev_branch = (struct mtfs_device_branch *)data;
	MENTRY();

	if (count > (sizeof(kern_buf) - 1)) {
		ret = -EINVAL;
		goto out;
	}

	if (copy_from_user(kern_buf, buffer, count)) {
		ret = -EINVAL;
		goto out;
	}
	kern_buf[count] = '\0';

	if (strncmp(MTFS_ERRNO_INACTIVE, kern_buf, strlen(MTFS_ERRNO_INACTIVE)) == 0) {
		dev_branch->mdb_debug.mbd_active = 0;
		goto out;
	}

	pbuf = kern_buf;
	if (*pbuf == '-') {
		sign = -1;
		pbuf++;
	}
	var = (int)simple_strtoul(pbuf, &end, 10) * sign;
	if (pbuf == end) {
		ret = -EINVAL;
		goto out;
	}

	if (var >= 0) {
		ret = -EINVAL;
		goto out;
	}

	dev_branch->mdb_debug.mbd_active = 1;
	dev_branch->mdb_debug.mbd_errno = var;
out:
	if (ret) {
		MRETURN(ret);
	}
	MRETURN(count);
}

static const char *mtfs_bops_mask2str(int mask)
{
	switch (1 << mask) {
	default:
		return NULL;
	case BOPS_MASK_WRITE:
		return "write_ops";
	case BOPS_MASK_READ:
		return "read_ops";
	}
}

static int mtfs_device_proc_bops_emask_read(char *page, char **start, off_t off, int count,
                                     int *eof, void *data)
{
	int ret = 0;
	struct mtfs_device_branch *dev_branch = (struct mtfs_device_branch *)data;
	int mask = dev_branch->mdb_debug.mbd_bops_emask;
	MENTRY();

	*eof = 1;

	ret = mtfs_common_mask2str(page, count, mask, mtfs_bops_mask2str);
	MRETURN(ret);
}

static int mtfs_device_proc_bops_emask_write(struct file *file, const char *buffer,
                                             unsigned long count, void *data)
{
	int ret = 0;
	char *tmpstr = NULL;
	struct mtfs_device_branch *dev_branch = (struct mtfs_device_branch *)data;
	MENTRY();

	MTFS_ALLOC(tmpstr, count + 1);
	if (tmpstr == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	ret = mtfs_trace_copyin_string(tmpstr, count + 1, buffer, count);
	if (ret < 0) {
		goto out_free_buf;
	}

	ret = mtfs_common_str2mask(&(dev_branch->mdb_debug.mbd_bops_emask),
	                           tmpstr, mtfs_bops_mask2str, 0);
out_free_buf:
	MTFS_FREE(tmpstr, count + 1);
out:
	if (ret) {
		MRETURN(ret);
	}
	MRETURN(count);
}

static struct mtfs_proc_vars mtfs_proc_vars_device_branch[] = {
	{ "errno", mtfs_device_branch_proc_read_errno, mtfs_device_branch_proc_write_errno, NULL },
	{ "bops_emask", mtfs_device_proc_bops_emask_read, mtfs_device_proc_bops_emask_write, NULL },
	{ 0 }
};

static int mtfs_device_proc_register(struct mtfs_device *device)
{
	int ret = 0;
	unsigned int hash_num = 0;
	char *name = NULL;
	mtfs_bindex_t bindex = 0;

	MTFS_ALLOC(name, PATH_MAX);
	if (name == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	hash_num = full_name_hash(mtfs_dev2name(device), mtfs_dev2namelen(device));
	sprintf(name, "%x", hash_num);
	mtfs_dev2proc(device) = mtfs_proc_register(name, mtfs_proc_device,
                                                   mtfs_proc_vars_device, device);
	if (unlikely(mtfs_dev2proc(device) == NULL)) {
		MERROR("failed to register proc for device [%s]\n",
		       mtfs_dev2name(device));
		ret = -ENOMEM;
		goto out;
	}

	for (bindex = 0; bindex < mtfs_dev2bnum(device); bindex++) {
		sprintf(name, "branch%d", bindex);
		mtfs_dev2bproc(device, bindex) = mtfs_proc_register(name, mtfs_dev2proc(device),
                                                                    mtfs_proc_vars_device_branch,
                                                                    mtfs_dev2branch(device, bindex));
		if (unlikely(mtfs_dev2bproc(device, bindex) == NULL)) {
			MERROR("failed to register proc for branch[%d] of device [%s]\n",
			       bindex, mtfs_dev2name(device));
			bindex--;
			ret = -ENOMEM;
			goto out_unregister;
		}
	}
	goto out_free_name;
out_unregister:
	for (; bindex >= 0; bindex--) {
		mtfs_proc_remove(&mtfs_dev2bproc(device, bindex));
	}
	mtfs_proc_remove(&mtfs_dev2proc(device));
out_free_name:
	MTFS_FREE(name, PATH_MAX);
out:
	return ret;
}


static void mtfs_device_proc_unregister(struct mtfs_device *device)
{
	mtfs_bindex_t bindex = 0;

	MASSERT(mtfs_dev2proc(device));
	for (bindex = 0; bindex < mtfs_dev2bnum(device); bindex++) {
		mtfs_proc_remove(&mtfs_dev2bproc(device, bindex));
	}
	mtfs_proc_remove(&mtfs_dev2proc(device));
}

static int mtfs_register_device(struct mtfs_device *device)
{
	int ret = 0;
	
	spin_lock(&mtfs_device_lock);
	ret = _mtfs_register_device(device);
	spin_unlock(&mtfs_device_lock);

	return ret;
}

static void _mtfs_unregister_device(struct mtfs_device *device)
{
	struct mtfs_device    *found;
	mtfs_list_t           *p;

	mtfs_list_for_each(p, &mtfs_devs) {
		found = mtfs_list_entry(p, typeof(*found), md_list);
		if (found == device) {
			list_del(p);
			break;
		}
	}
}

static void mtfs_unregister_device(struct mtfs_device *device)
{
	spin_lock(&mtfs_device_lock);
	_mtfs_unregister_device(device);
	spin_unlock(&mtfs_device_lock);
}

struct mtfs_device *mtfs_newdev(struct super_block *sb, struct mount_option *mount_option)
{
	struct mtfs_device *newdev = NULL;
	int ret = 0;
	struct mtfs_lowerfs *lowerfs = NULL;
	mtfs_bindex_t bindex = 0;
	struct super_block *hidden_sb = NULL;
	mtfs_bindex_t bnum = mtfs_s2bnum(sb);
	const char *primary_type = NULL;
	const char **secondary_types = NULL;
	int secondary_number = 0;
	MENTRY();

	MTFS_ALLOC(secondary_types, sizeof(*secondary_types) * bnum);
	if (secondary_types == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	newdev = mtfs_device_alloc(mount_option);
	if (newdev == NULL) {
		ret = -ENOMEM;
		goto out_free_types;
	}

	/*
	 * TODO: check whether branches could cooperate with each other.
	 * And then choose a proper set of inode/dentry/file operation as the mtfs's. 
	 * Only support branches with same lowerfs type now, but this is not checked.
	 */
	for (bindex = 0; bindex < bnum; bindex++) {
		hidden_sb = mtfs_s2branch(sb, bindex);
		if (bindex == 0) {
			primary_type = hidden_sb->s_type->name;
		} else {
			/* TODO: judge equation to speed up junction search */
			secondary_types[secondary_number] = hidden_sb->s_type->name;
			secondary_number ++;
		}
		lowerfs = mlowerfs_get(hidden_sb->s_type->name);
		if (IS_ERR(lowerfs)) {
			MERROR("lowerfs type [%s] not supported yet\n", hidden_sb->s_type->name);
			ret = PTR_ERR(lowerfs);
			goto out_put_module;
		}
		mtfs_dev2blowerfs(newdev, bindex) = lowerfs;
	}
	secondary_types[secondary_number] = NULL;

	mtfs_dev2junction(newdev) = junction_get(mount_option->mo_subject, primary_type, secondary_types);
	if (IS_ERR(mtfs_dev2junction(newdev))) {
		MERROR("junction not supported yet, type = %s, subject = %s\n",
		       primary_type, mount_option->mo_subject); /* TODO: print secondary type */
		ret = PTR_ERR(mtfs_dev2junction(newdev));
		goto out_put_module;
	}

	mtfs_dev2sb(newdev) = sb;

	ret = mtfs_register_device(newdev);
	if (ret) {
		goto out_put_junction;
	}

	ret = mtfs_device_proc_register(newdev);
	if (ret) {
		goto out_unregister_device;
	}
	goto out_free_types;

out_unregister_device:
	mtfs_unregister_device(newdev);
out_put_junction:
	junction_put(mtfs_dev2junction(newdev));
out_put_module:
	for (bindex = 0; bindex < bnum; bindex++) {
		lowerfs = mtfs_dev2blowerfs(newdev, bindex);
		if (lowerfs) {
			mlowerfs_put(lowerfs);
		}
	}

	mtfs_device_free(newdev);
out_free_types:
	MTFS_FREE(secondary_types, sizeof(*secondary_types) * bnum);
out:
	if (ret) {
		newdev = ERR_PTR(ret);
	}

	MRETURN(newdev);
}

void mtfs_freedev(struct mtfs_device *device)
{
	struct mtfs_lowerfs *lowerfs = NULL;
	mtfs_bindex_t bnum = mtfs_dev2bnum(device);
	mtfs_bindex_t bindex = 0;

	mtfs_device_proc_unregister(device);
	mtfs_unregister_device(device);
	for (bindex = 0; bindex < bnum; bindex++) {
		lowerfs = mtfs_dev2blowerfs(device, bindex);
		MASSERT(lowerfs);
		mlowerfs_put(lowerfs);
	}
	junction_put(mtfs_dev2junction(device));
	mtfs_device_free(device);
}

int mtfs_proc_read_devices(char *page, char **start, off_t off,
                           int count, int *eof, void *data)
{
	int                 ret = 0;
	mtfs_list_t        *p = NULL;
	struct mtfs_device *found = NULL;
	unsigned int        hash_num = 0;
	char                hash_name[9];

	*eof = 1;

	spin_lock(&mtfs_device_lock);
	mtfs_list_for_each(p, &mtfs_devs) {
		if (ret >= count - 1) {
			MERROR("not enough memory for proc read\n");
			break;
		}
		found = mtfs_list_entry(p, typeof(*found), md_list);
		hash_num = full_name_hash(mtfs_dev2name(found),
		                          mtfs_dev2namelen(found));
		sprintf(hash_name, "%x", hash_num);
		ret += snprintf(page + ret, count - ret, "%s %s\n",
		                mtfs_dev2name(found), hash_name);
	}
	spin_unlock(&mtfs_device_lock);
	return ret;
}

#define MTFS_DEFAULT_ERRNO (-EIO)
int mtfs_device_branch_errno(struct mtfs_device *device,
                             mtfs_bindex_t bindex, __u32 emask)
{
	struct mtfs_device_branch *dev_branch = mtfs_dev2branch(device, bindex);
	int errno = 0;
	MENTRY();

	if ((emask & dev_branch->mdb_debug.mbd_bops_emask) != 0) {
		if (dev_branch->mdb_debug.mbd_active) {
			MASSERT(dev_branch->mdb_debug.mbd_errno < 0);
			errno = dev_branch->mdb_debug.mbd_errno;
		} else {
			errno = MTFS_DEFAULT_ERRNO;
		}
	}

	MRETURN(errno);
}
EXPORT_SYMBOL(mtfs_device_branch_errno);
