/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#include "device_internal.h"
#include "hrfs_internal.h"
#include "junction_internal.h"

static struct hrfs_device *hrfs_device_alloc(mount_option_t *mount_option)
{
	struct hrfs_device *device = NULL;
	hrfs_bindex_t bindex = 0;
	hrfs_bindex_t bnum = mount_option->bnum;

	HRFS_SLAB_ALLOC_PTR(device, hrfs_device_cache);
	if (device == NULL) {
		goto out;
	}

	device->bnum = bnum;

	HRFS_ALLOC(device->branch, sizeof(*device->branch) * bnum);
	if (device->branch == NULL) {
		goto out_free_device;
	}

	for(bindex = 0; bindex < bnum; bindex++) {
		int length = mount_option->branch[bindex].length;

		hrfs_dev2blength(device, bindex) = length;
		device->name_length += length;
		HRFS_ALLOC(hrfs_dev2bpath(device, bindex), length);
		if (hrfs_dev2bpath(device, bindex) == NULL) {
			bindex --;
			goto out_free_path;
		}
		memcpy(hrfs_dev2bpath(device, bindex), mount_option->branch[bindex].path, length - 1);
	}

	HRFS_ALLOC(device->device_name, device->name_length);
	if (device->device_name == NULL) {
		goto out_free_path;
	}

	for(bindex = 0; bindex < bnum; bindex++) {
		append_dir(device->device_name, mount_option->branch[bindex].path);
	}

	goto out;
//out_free_name:
	HRFS_FREE(device->device_name, device->name_length);
out_free_path:
	for(; bindex >= 0; bindex--) {
		HRFS_FREE(hrfs_dev2bpath(device, bindex), hrfs_dev2blength(device, bindex));
	}
//out_free_branch:
	HRFS_FREE(device->branch, sizeof(*device->branch) * bnum);
out_free_device:
	HRFS_SLAB_FREE_PTR(device, hrfs_device_cache);
	HASSERT(device == NULL);
out:
	return device;
}

static void hrfs_device_free(struct hrfs_device *device)
{
	hrfs_bindex_t bindex = 0;
	hrfs_bindex_t bnum = hrfs_dev2bnum(device);
	HASSERT(device != NULL);

	for(bindex = 0; bindex < bnum; bindex++) {
		HRFS_FREE(hrfs_dev2bpath(device, bindex), hrfs_dev2blength(device, bindex));
	}
	HRFS_FREE(device->branch, sizeof(*(device->branch)) * bnum);
	HRFS_FREE(device->device_name, device->name_length);
	HRFS_SLAB_FREE_PTR(device, hrfs_device_cache);
}

spinlock_t hrfs_device_lock = SPIN_LOCK_UNLOCKED;
LIST_HEAD(hrfs_devs);

static struct hrfs_device *_hrfs_search_device(struct hrfs_device *device)
{
	struct hrfs_device *found = NULL;
	struct list_head *p = NULL;
	hrfs_bindex_t bindex = 0;
	hrfs_bindex_t bnum = 0;
	struct dentry *d_root  = device->sb->s_root;
	struct dentry *d_root_tmp = NULL;
	int have_same = 0;
	int have_diff = 0;

	list_for_each(p, &hrfs_devs) {
		found = list_entry(p, typeof(*found), device_list);
		d_root_tmp = found->sb->s_root;

		HASSERT(d_root_tmp);
		bnum = device->bnum;
		if (bnum > found->bnum) {
			bnum = found->bnum;
		}
		if (bnum != found->bnum ||
		    bnum != device->bnum) {
		    have_diff = 1;
		}

		for(bindex = 0; bindex < bnum; bindex++) {
			//if ((strcmp(hrfs_dev2bpath(found, bindex), hrfs_dev2bpath(device, bindex)) == 0) ||
			if (hrfs_d2branch(d_root, bindex) == hrfs_d2branch(d_root_tmp, bindex)) {
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

struct hrfs_device *hrfs_search_device(struct hrfs_device *device)
{
	struct hrfs_device *found = NULL;
	
	spin_lock(&hrfs_device_lock);
	found = _hrfs_search_device(device);
	spin_unlock(&hrfs_device_lock);
	return found;
}

static int _hrfs_register_device(struct hrfs_device *device)
{
	struct hrfs_device *found = NULL;
	int ret = 0;

	found = _hrfs_search_device(device);
	if (found != NULL) {
		if (found == device) {
			HERROR("try to register same devices %s for mutiple times\n",
			        device->device_name);
		} else {
			HERROR("try to register multiple devices for %s\n",
			        device->device_name);
		}
		ret = -EEXIST;
	} else {
		list_add(&device->device_list, &hrfs_devs);
	}

	return ret;
}

int hrfs_device_proc_read_name(char *page, char **start, off_t off, int count,
                              int *eof, void *data)
{
	int ret = 0;
	struct hrfs_device *device = (struct hrfs_device *)data;
	char *ptr = page;

	*eof = 1;
	ret = snprintf(ptr, count, "%s\n", device->device_name);
	ptr += ret;
	return ret;
}

int hrfs_device_proc_read_bnum(char *page, char **start, off_t off, int count,
                              int *eof, void *data)
{
	int ret = 0;
	struct hrfs_device *device = (struct hrfs_device *)data;
	char *ptr = page;

	*eof = 1;
	ret = snprintf(ptr, count, "%d\n", device->bnum);
	ptr += ret;
	return ret;
}

int hrfs_device_proc_read_noabort(char *page, char **start, off_t off, int count,
                              int *eof, void *data)
{
	int ret = 0;
	struct hrfs_device *device = (struct hrfs_device *)data;
	char *ptr = page;

	*eof = 1;
	ret = snprintf(ptr, count, "%d\n", device->no_abort);
	ptr += ret;
	return ret;
}

struct hrfs_proc_vars hrfs_proc_vars_device[] = {
	{ "device_name", hrfs_device_proc_read_name, NULL, NULL },
	{ "bnum", hrfs_device_proc_read_bnum, NULL, NULL },
	{ "no_abort", hrfs_device_proc_read_noabort, NULL, NULL },
	{ 0 }
};

#define HRFS_ERRNO_INACTIVE "inactive"
int hrfs_device_branch_proc_read_errno(char *page, char **start, off_t off, int count,
                                       int *eof, void *data)
{
	int ret = 0;
	struct hrfs_device_branch *dev_branch = (struct hrfs_device_branch *)data;
	char *ptr = page;

	*eof = 1;
	if (dev_branch->debug.active) {
		ret = snprintf(ptr, count, "%d\n", dev_branch->debug.errno);
	} else {
		ret = snprintf(ptr, count, HRFS_ERRNO_INACTIVE"\n");
	}
	ptr += ret;
	return ret;
}


static int hrfs_device_branch_proc_write_errno(struct file *file, const char *buffer,
                                               unsigned long count, void *data)
{
	int ret = 0;
	char kern_buf[20];
	char *end = NULL;
	char *pbuf = NULL;
	int sign = 1;
	int var = 0;
	struct hrfs_device_branch *dev_branch = (struct hrfs_device_branch *)data;
	HENTRY();

	if (count > (sizeof(kern_buf) - 1)) {
		ret = -EINVAL;
		goto out;
	}

	if (copy_from_user(kern_buf, buffer, count)) {
		ret = -EINVAL;
		goto out;
	}
	kern_buf[count] = '\0';

	if (strncmp(HRFS_ERRNO_INACTIVE, kern_buf, strlen(HRFS_ERRNO_INACTIVE)) == 0) {
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
		HRETURN(ret);
	}
	HRETURN(count);
}
struct hrfs_proc_vars hrfs_proc_vars_device_branch[] = {
	{ "errno", hrfs_device_branch_proc_read_errno, hrfs_device_branch_proc_write_errno, NULL },
	{ 0 }
};

int hrfs_device_proc_register(struct hrfs_device *device)
{
	int ret = 0;
	unsigned int hash_num = 0;
	char name[PATH_MAX];
	hrfs_bindex_t bindex = 0;
	struct hrfs_device_branch *dev_branch = NULL;

	hash_num = full_name_hash(device->device_name, device->name_length);
	sprintf(name, "%x", hash_num);
	device->proc_entry = hrfs_proc_register(name, hrfs_proc_device,
                                          hrfs_proc_vars_device, device);
	if (unlikely(device->proc_entry == NULL)) {
		HERROR("failed to register proc for device %s\n",
		       device->device_name);
		ret = -ENOMEM;
		goto out;
	}

	for (bindex = 0; bindex < hrfs_dev2bnum(device); bindex++) {
		sprintf(name, "branch%d", bindex);
		dev_branch = hrfs_dev2branch(device, bindex);
		dev_branch->proc_entry = hrfs_proc_register(name, device->proc_entry,
                                                hrfs_proc_vars_device_branch, dev_branch);
		if (unlikely(dev_branch->proc_entry == NULL)) {
			HERROR("failed to register proc for %s of device %s\n",
			       name, device->device_name);
			bindex--;
			ret = -ENOMEM;
			goto out_unregister;
		}
	}
	goto out;
out_unregister:
	for (; bindex >= 0; bindex--) {
		dev_branch = hrfs_dev2branch(device, bindex);
		hrfs_proc_remove(&dev_branch->proc_entry);
	}
	hrfs_proc_remove(&device->proc_entry);
out:
	return ret;
}


void hrfs_device_proc_unregister(struct hrfs_device *device)
{
	hrfs_bindex_t bindex = 0;

	HASSERT(device->proc_entry);
	for (bindex = 0; bindex < hrfs_dev2bnum(device); bindex++) {
		hrfs_proc_remove(&hrfs_dev2bproc(device, bindex));
	}
	hrfs_proc_remove(&device->proc_entry);
}

static int hrfs_register_device(struct hrfs_device *device)
{
	int ret = 0;
	
	spin_lock(&hrfs_device_lock);
	ret = _hrfs_register_device(device);
	spin_unlock(&hrfs_device_lock);

	return ret;
}

void _hrfs_unregister_device(struct hrfs_device *device)
{
	struct hrfs_device *found;
	struct list_head *p;

	list_for_each(p, &hrfs_devs) {
		found = list_entry(p, typeof(*found), device_list);
		if (found == device) {
			list_del(p);
			break;
		}
	}
}

void hrfs_unregister_device(struct hrfs_device *device)
{
	spin_lock(&hrfs_device_lock);
	_hrfs_unregister_device(device);
	spin_unlock(&hrfs_device_lock);
}

struct hrfs_device *hrfs_newdev(struct super_block *sb, mount_option_t *mount_option)
{
	struct hrfs_device *newdev = NULL;
	int ret = 0;
	struct lowerfs_operations *lowerfs_ops = NULL;
	hrfs_bindex_t bindex = 0;
	struct super_block *hidden_sb = NULL;
	hrfs_bindex_t bnum = hrfs_s2bnum(sb);
	const char *primary_type = NULL;
	const char **secondary_types = NULL;
	int secondary_number = 0;

	if (bnum > 1) {
		HRFS_ALLOC(secondary_types, sizeof(*secondary_types) * bnum);
		if (secondary_types == NULL) {
			goto out;
		}
	}

	newdev = hrfs_device_alloc(mount_option);
	if (newdev == NULL) {
		ret = -ENOMEM;
		goto out;
	}

	/*
	 * TODO: check whether branches could cooperate with each other.
	 * And then choose a proper set of inode/dentry/file operation as the hrfs's. 
	 * Only support branches with same lowerfs type now, but this is not checked.
	 */
	for (bindex = 0; bindex < bnum; bindex++) {
		hidden_sb = hrfs_s2branch(sb, bindex);
		if (bindex == HRFS_DEFAULT_PRIMARY_BRANCH) {
			primary_type = hidden_sb->s_type->name;
		} else {
			/* TODO: judge equation to speed up junction search */
			secondary_types[secondary_number] = hidden_sb->s_type->name;
			secondary_number ++;
		}
		lowerfs_ops = lowerfs_get_ops(hidden_sb->s_type->name);
		if (IS_ERR(lowerfs_ops)) {
			HERROR("lowerfs type [%s] not supported yet\n", hidden_sb->s_type->name);
			ret = PTR_ERR(lowerfs_ops);
			goto out_put_module;
		}
		hrfs_dev2bops(newdev, bindex) = lowerfs_ops;
	}
	secondary_types[secondary_number] = NULL;

	newdev->junction = junction_get(primary_type, secondary_types);
	if (IS_ERR(newdev->junction)) {
		HERROR("junction type [%s] not supported yet\n", primary_type); /* TODO: print secondary type */
		ret = PTR_ERR(newdev->junction);
		goto out_put_module;
	}

	newdev->sb = sb;

	ret = hrfs_register_device(newdev);
	if (ret) {
		goto out_put_junction;
	}

	ret = hrfs_device_proc_register(newdev);
	if (ret) {
		goto out_unregister_device;
	}
	goto out;
out_unregister_device:
	hrfs_unregister_device(newdev);
out_put_junction:
	junction_put(newdev->junction);
out_put_module:
	for (bindex = 0; bindex < bnum; bindex++) {
		lowerfs_ops = hrfs_dev2bops(newdev, bindex);
		if (lowerfs_ops) {
			lowerfs_put_ops(lowerfs_ops);
		}
	}

	hrfs_device_free(newdev);
out:
	if (secondary_types) {
		HRFS_FREE(secondary_types, sizeof(char *) * bnum);
	}

	if (ret) {
		newdev = ERR_PTR(ret);
	}
	return newdev;
}

void hrfs_freedev(struct hrfs_device *device)
{
	struct lowerfs_operations *lowerfs_ops = NULL;
	hrfs_bindex_t bnum = hrfs_dev2bnum(device);
	hrfs_bindex_t bindex = 0;

	hrfs_device_proc_unregister(device);
	hrfs_unregister_device(device);
	for (bindex = 0; bindex < bnum; bindex++) {
		lowerfs_ops = hrfs_dev2bops(device, bindex);
		HASSERT(lowerfs_ops);
		lowerfs_put_ops(lowerfs_ops);
	}
	junction_put(device->junction);
	hrfs_device_free(device);
}

int hrfs_proc_read_devices(char *page, char **start, off_t off,
                           int count, int *eof, void *data)
{
	int ret = 0;
	struct list_head *p = NULL;
	struct hrfs_device *found = NULL;
	char *ptr = page;
	unsigned int hash_num = 0;
	char hash_name[9];

	*eof = 1;

	spin_lock(&hrfs_device_lock);
	list_for_each(p, &hrfs_devs) {
		found = list_entry(p, typeof(*found), device_list);
		hash_num = full_name_hash(found->device_name, found->name_length);
		sprintf(hash_name, "%x", hash_num);
		ret += snprintf(ptr, count, "%s %s\n", found->device_name, hash_name);
		ptr += ret;
	}
	spin_unlock(&hrfs_device_lock);
	return ret;
}

int hrfs_device_branch_errno(struct hrfs_device *device, hrfs_bindex_t bindex)
{
	struct hrfs_device_branch *dev_branch = hrfs_dev2branch(device, bindex);

	if (dev_branch->debug.active) {
		HASSERT(dev_branch->debug.errno < 0);
		return dev_branch->debug.errno;
	}
	return 0;
}
