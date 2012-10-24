/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <linux/ctype.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <compat.h>
#include <mtfs_proc.h>
#include <mtfs_dentry.h>
#include "device_internal.h"
#include "junction_internal.h"
#include "support_internal.h"
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

	device->bnum = bnum;

	for(bindex = 0; bindex < bnum; bindex++) {
		int length = mount_option->branch[bindex].length;

		mtfs_dev2blength(device, bindex) = length;
		device->name_length += length;
		MTFS_ALLOC(mtfs_dev2bpath(device, bindex), length);
		if (mtfs_dev2bpath(device, bindex) == NULL) {
			bindex --;
			goto out_free_path;
		}
		memcpy(mtfs_dev2bpath(device, bindex), mount_option->branch[bindex].path, length - 1);
	}

	MTFS_ALLOC(device->device_name, device->name_length);
	if (device->device_name == NULL) {
		goto out_free_path;
	}

	for(bindex = 0; bindex < bnum; bindex++) {
		append_dir(device->device_name, mount_option->branch[bindex].path);
	}

	goto out;
//out_free_name:
	MTFS_FREE(device->device_name, device->name_length);
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
	MTFS_FREE(device->device_name, device->name_length);
	MTFS_SLAB_FREE_PTR(device, mtfs_device_cache);
}

spinlock_t mtfs_device_lock = SPIN_LOCK_UNLOCKED;
LIST_HEAD(mtfs_devs);

static struct mtfs_device *_mtfs_search_device(struct mtfs_device *device)
{
	struct mtfs_device *found = NULL;
	struct list_head *p = NULL;
	mtfs_bindex_t bindex = 0;
	mtfs_bindex_t bnum = 0;
	struct dentry *d_root  = device->sb->s_root;
	struct dentry *d_root_tmp = NULL;
	int have_same = 0;
	int have_diff = 0;

	list_for_each(p, &mtfs_devs) {
		found = list_entry(p, typeof(*found), device_list);
		d_root_tmp = found->sb->s_root;

		MASSERT(d_root_tmp);
		bnum = device->bnum;
		if (bnum > found->bnum) {
			bnum = found->bnum;
		}
		if (bnum != found->bnum ||
		    bnum != device->bnum) {
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

struct mtfs_device *mtfs_search_device(struct mtfs_device *device)
{
	struct mtfs_device *found = NULL;
	
	spin_lock(&mtfs_device_lock);
	found = _mtfs_search_device(device);
	spin_unlock(&mtfs_device_lock);
	return found;
}

static int _mtfs_register_device(struct mtfs_device *device)
{
	struct mtfs_device *found = NULL;
	int ret = 0;

	found = _mtfs_search_device(device);
	if (found != NULL) {
		if (found == device) {
			MERROR("try to register same devices %s for mutiple times\n",
			        device->device_name);
		} else {
			MERROR("try to register multiple devices for %s\n",
			        device->device_name);
		}
		ret = -EEXIST;
	} else {
		list_add(&device->device_list, &mtfs_devs);
	}

	return ret;
}

int mtfs_device_proc_read_name(char *page, char **start, off_t off, int count,
                              int *eof, void *data)
{
	int ret = 0;
	struct mtfs_device *device = (struct mtfs_device *)data;
	char *ptr = page;

	*eof = 1;
	ret = snprintf(ptr, count, "%s\n", device->device_name);
	ptr += ret;
	return ret;
}

int mtfs_device_proc_read_bnum(char *page, char **start, off_t off, int count,
                              int *eof, void *data)
{
	int ret = 0;
	struct mtfs_device *device = (struct mtfs_device *)data;
	char *ptr = page;

	*eof = 1;
	ret = snprintf(ptr, count, "%d\n", device->bnum);
	ptr += ret;
	return ret;
}

int mtfs_device_proc_read_noabort(char *page, char **start, off_t off, int count,
                              int *eof, void *data)
{
	int ret = 0;
	struct mtfs_device *device = (struct mtfs_device *)data;
	char *ptr = page;

	*eof = 1;
	ret = snprintf(ptr, count, "%d\n", device->no_abort);
	ptr += ret;
	return ret;
}

struct mtfs_proc_vars mtfs_proc_vars_device[] = {
	{ "device_name", mtfs_device_proc_read_name, NULL, NULL },
	{ "bnum", mtfs_device_proc_read_bnum, NULL, NULL },
	{ "no_abort", mtfs_device_proc_read_noabort, NULL, NULL },
	{ 0 }
};

#define MTFS_ERRNO_INACTIVE "inactive"
int mtfs_device_branch_proc_read_errno(char *page, char **start, off_t off, int count,
                                       int *eof, void *data)
{
	int ret = 0;
	struct mtfs_device_branch *dev_branch = (struct mtfs_device_branch *)data;
	char *ptr = page;

	*eof = 1;
	if (dev_branch->debug.active) {
		ret = snprintf(ptr, count, "%d\n", dev_branch->debug.errno);
	} else {
		ret = snprintf(ptr, count, MTFS_ERRNO_INACTIVE"\n");
	}
	ptr += ret;
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
		dev_branch->debug.active = 0;
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

	dev_branch->debug.active = 1;
	dev_branch->debug.errno = var;
out:
	if (ret) {
		MRETURN(ret);
	}
	MRETURN(count);
}

const char *mtfs_bops_bit2str(__u32 bit)
{
	switch (bit) {
	case BOPS_MASK_WRITE:
		return "write_ops";
	case BOPS_MASK_READ:
		return "read_ops";
	default:
		return NULL;
	}
	return NULL;
}

int mtfs_device_proc_bops_emask_read(char *page, char **start, off_t off, int count,
                                       int *eof, void *data)
{
	int ret = 0;
	struct mtfs_device_branch *dev_branch = (struct mtfs_device_branch *)data;
	char *ptr = page;
	__u64 mask = dev_branch->debug.bops_emask;
	const char *token = NULL;
	__u64 bit = 0;
	int i = 0;
	int first = 1;
	MENTRY();

	*eof = 1;
	for (i = 0; i < 32; i++) {
		bit = 1 << i;
		if ((mask & bit) == 0) {
			continue;
		}

		token = mtfs_bops_bit2str(bit);
		if (token == NULL) {
			/* unused bit */
			continue;
		}

		MERROR("matched %d\n", i);
		if (first) {
			ret += snprintf(ptr + ret, count - ret, "%s", token);
			first = 0;
		} else {
			ret += snprintf(ptr + ret, count - ret, " %s", token);
		}
	}
	ret += snprintf(ptr + ret, count - ret, "\n");

	MRETURN(ret);
}

int mtfs_bops_str2bit(const char *str, size_t length, __u64 *mask)
{
	const char *token = NULL;
	int i = 0;
	int ret = 0;
	__u64 bit = 0;;
	MENTRY();

	MASSERT(str);
	MASSERT(length > 0);
	for (i = 0; i < 32; i++) {
		bit = 1 << i;

		token = mtfs_bops_bit2str(bit);
		if (token == NULL) {
			/* unused bit */
			continue;
		}

		if (length != strlen(token)) {
			continue;
		}

		ret = strncasecmp(str, token, length);
		if (ret == 0) {
			*mask = bit;
			goto out;
		}
	}
	ret = -EINVAL;
out:
	MRETURN(ret);
}

/*
 * <str> must be a list of tokens separated by
 * whitespace and optionally an operator ('+' or '-').  If an operator
 * appears first in <str>, '*mask' is used as the starting point
 * (relative), otherwise 0 is used (absolute).  An operator applies to
 * all following tokens up to the next operator.
 */
int mtfs_bops_str2mask(const char *str, __u32 *mask)
{
	int ret = 0;
	__u64 mask_tmp = 0;
	char op = 0;
	int matched = 0;
	__u64 mask_bit = 0;
	int n = 0;
	MENTRY();

	while (*str != 0) {
		while (isspace(*str)) {
			/* skip whitespace */
			str++;
		}

		if (*str == 0) {
			break;
		}

		if (*str == '+' || *str == '-') {
			op = *str++;

			/* op on first token == relative */
			if (!matched) {
				mask_tmp = *mask;
			}

			while (isspace(*str)) {
				/* skip whitespace */
				str++;
			}

			if (*str == 0) {
				/* trailing op */
				ret = -EINVAL;
				goto out;
			}
		}

		/* find token length */
		for (n = 0; str[n] != 0 && !isspace(str[n]); n++);
		MASSERT(n > 0);

		/* match token */
		ret = mtfs_bops_str2bit(str, n, &mask_bit);
		if (ret) {
			goto out;
		}

		matched = 1;
		if (op == '-') {
			mask_tmp &= ~mask_bit;
		} else {
			mask_tmp |= mask_bit;
		}

		str += n;
	}

	if (matched) {
		*mask = mask_tmp;
	} else {
		*mask = 0;
	}
out:
	MRETURN(ret);
}

static int mtfs_device_proc_bops_emask_write(struct file *file, const char *buffer,
                                               unsigned long count, void *data)
{
	int ret = 0;
	char *kern_buf = NULL;
	struct mtfs_device_branch *dev_branch = (struct mtfs_device_branch *)data;
	MENTRY();

	MTFS_ALLOC(kern_buf, count + 1);
	if (kern_buf == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(kern_buf, buffer, count)) {
		ret = -EINVAL;
		goto out_free_buf;
	}
	kern_buf[count] = '\0';

	ret = mtfs_bops_str2mask(kern_buf, &(dev_branch->debug.bops_emask));
out_free_buf:
	MTFS_FREE(kern_buf, count + 1);
out:
	if (ret) {
		MRETURN(ret);
	}
	MRETURN(count);
}

struct mtfs_proc_vars mtfs_proc_vars_device_branch[] = {
	{ "errno", mtfs_device_branch_proc_read_errno, mtfs_device_branch_proc_write_errno, NULL },
	{ "bops_emask", mtfs_device_proc_bops_emask_read, mtfs_device_proc_bops_emask_write, NULL },
	{ 0 }
};

int mtfs_device_proc_register(struct mtfs_device *device)
{
	int ret = 0;
	unsigned int hash_num = 0;
	char *name = NULL;
	mtfs_bindex_t bindex = 0;
	struct mtfs_device_branch *dev_branch = NULL;

	MTFS_ALLOC(name, PATH_MAX);
	if (name == NULL) {
		MERROR("not enough memory\n");
		ret = -ENOMEM;
		goto out;
	}

	hash_num = full_name_hash(device->device_name, device->name_length);
	sprintf(name, "%x", hash_num);
	device->proc_entry = mtfs_proc_register(name, mtfs_proc_device,
                                          mtfs_proc_vars_device, device);
	if (unlikely(device->proc_entry == NULL)) {
		MERROR("failed to register proc for device %s\n",
		       device->device_name);
		ret = -ENOMEM;
		goto out;
	}

	for (bindex = 0; bindex < mtfs_dev2bnum(device); bindex++) {
		sprintf(name, "branch%d", bindex);
		dev_branch = mtfs_dev2branch(device, bindex);
		dev_branch->proc_entry = mtfs_proc_register(name, device->proc_entry,
                                                mtfs_proc_vars_device_branch, dev_branch);
		if (unlikely(dev_branch->proc_entry == NULL)) {
			MERROR("failed to register proc for %s of device %s\n",
			       name, device->device_name);
			bindex--;
			ret = -ENOMEM;
			goto out_unregister;
		}
	}
	goto out_free_name;
out_unregister:
	for (; bindex >= 0; bindex--) {
		dev_branch = mtfs_dev2branch(device, bindex);
		mtfs_proc_remove(&dev_branch->proc_entry);
	}
	mtfs_proc_remove(&device->proc_entry);
out_free_name:
	MTFS_FREE(name, PATH_MAX);
out:
	return ret;
}


void mtfs_device_proc_unregister(struct mtfs_device *device)
{
	mtfs_bindex_t bindex = 0;

	MASSERT(device->proc_entry);
	for (bindex = 0; bindex < mtfs_dev2bnum(device); bindex++) {
		mtfs_proc_remove(&mtfs_dev2bproc(device, bindex));
	}
	mtfs_proc_remove(&device->proc_entry);
}

static int mtfs_register_device(struct mtfs_device *device)
{
	int ret = 0;
	
	spin_lock(&mtfs_device_lock);
	ret = _mtfs_register_device(device);
	spin_unlock(&mtfs_device_lock);

	return ret;
}

void _mtfs_unregister_device(struct mtfs_device *device)
{
	struct mtfs_device *found;
	struct list_head *p;

	list_for_each(p, &mtfs_devs) {
		found = list_entry(p, typeof(*found), device_list);
		if (found == device) {
			list_del(p);
			break;
		}
	}
}

void mtfs_unregister_device(struct mtfs_device *device)
{
	spin_lock(&mtfs_device_lock);
	_mtfs_unregister_device(device);
	spin_unlock(&mtfs_device_lock);
}

struct mtfs_device *mtfs_newdev(struct super_block *sb, struct mount_option *mount_option)
{
	struct mtfs_device *newdev = NULL;
	int ret = 0;
	struct lowerfs_operations *lowerfs_ops = NULL;
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
		if (bindex == MTFS_DEFAULT_PRIMARY_BRANCH) {
			primary_type = hidden_sb->s_type->name;
		} else {
			/* TODO: judge equation to speed up junction search */
			secondary_types[secondary_number] = hidden_sb->s_type->name;
			secondary_number ++;
		}
		lowerfs_ops = lowerfs_get_ops(hidden_sb->s_type->name);
		if (IS_ERR(lowerfs_ops)) {
			MERROR("lowerfs type [%s] not supported yet\n", hidden_sb->s_type->name);
			ret = PTR_ERR(lowerfs_ops);
			goto out_put_module;
		}
		mtfs_dev2bops(newdev, bindex) = lowerfs_ops;
	}
	secondary_types[secondary_number] = NULL;

	newdev->junction = junction_get(mount_option->mo_subject, primary_type, secondary_types);
	if (IS_ERR(newdev->junction)) {
		MERROR("junction not supported yet, type = %s, subject = %s\n",
		       primary_type, mount_option->mo_subject); /* TODO: print secondary type */
		ret = PTR_ERR(newdev->junction);
		goto out_put_module;
	}

	newdev->sb = sb;

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
	junction_put(newdev->junction);
out_put_module:
	for (bindex = 0; bindex < bnum; bindex++) {
		lowerfs_ops = mtfs_dev2bops(newdev, bindex);
		if (lowerfs_ops) {
			lowerfs_put_ops(lowerfs_ops);
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
	struct lowerfs_operations *lowerfs_ops = NULL;
	mtfs_bindex_t bnum = mtfs_dev2bnum(device);
	mtfs_bindex_t bindex = 0;

	mtfs_device_proc_unregister(device);
	mtfs_unregister_device(device);
	for (bindex = 0; bindex < bnum; bindex++) {
		lowerfs_ops = mtfs_dev2bops(device, bindex);
		MASSERT(lowerfs_ops);
		lowerfs_put_ops(lowerfs_ops);
	}
	junction_put(device->junction);
	mtfs_device_free(device);
}

int mtfs_proc_read_devices(char *page, char **start, off_t off,
                           int count, int *eof, void *data)
{
	int ret = 0;
	struct list_head *p = NULL;
	struct mtfs_device *found = NULL;
	char *ptr = page;
	unsigned int hash_num = 0;
	char hash_name[9];

	*eof = 1;

	spin_lock(&mtfs_device_lock);
	list_for_each(p, &mtfs_devs) {
		found = list_entry(p, typeof(*found), device_list);
		hash_num = full_name_hash(found->device_name, found->name_length);
		sprintf(hash_name, "%x", hash_num);
		ret += snprintf(ptr, count, "%s %s\n", found->device_name, hash_name);
		ptr += ret;
	}
	spin_unlock(&mtfs_device_lock);
	return ret;
}

int mtfs_device_branch_errno(struct mtfs_device *device, mtfs_bindex_t bindex, __u32 emask)
{
	struct mtfs_device_branch *dev_branch = mtfs_dev2branch(device, bindex);
	int errno = -EIO;

	if ((emask & dev_branch->debug.bops_emask) != 0) {
		if (dev_branch->debug.active) {
			MASSERT(dev_branch->debug.errno < 0);
			errno = dev_branch->debug.errno;
		}
		return errno;
	}
	return 0;
}
EXPORT_SYMBOL(mtfs_device_branch_errno);
