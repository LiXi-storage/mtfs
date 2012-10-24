#include <linux/module.h>
#include <memory.h>
#include <debug.h>
#include <spinlock.h>
#include <mtfs_interval_tree.h>
#include <mtfs_async.h>
#include <mtfs_list.h>

void masync_bucket_init(struct masync_bucket *bucket)
{
	MENTRY();

	mtfs_spin_lock_init(&bucket->mab_lock);
	bucket->mab_root = NULL;
	bucket->mab_size = 0;

	_MRETURN();
}
EXPORT_SYMBOL(masync_bucket_init);

static enum mtfs_interval_iter masync_overlap_cb(struct mtfs_interval_node *node,
                                                 void *args)
{
	struct list_head *extent_list = NULL;
	struct mtfs_interval *extent_node = NULL;

	extent_node = mtfs_node2interval(node);
	extent_list = (struct list_head *)args;
	mtfs_list_add_tail(&extent_node->mi_linkage, extent_list);

	return MTFS_INTERVAL_ITER_CONT;
}

void masync_bucket_add(struct masync_bucket *bucket,
                       struct mtfs_interval_node_extent *extent)
{
	MTFS_LIST_HEAD(extent_list);
	struct mtfs_interval *tmp_extent = NULL;
	MENTRY();

	mtfs_interval_search(bucket->mab_root, extent, masync_overlap_cb, &extent_list);
	mtfs_list_for_each_entry(tmp_extent, &extent_list, mi_linkage) {
		//MERROR();
	}

	_MRETURN();
}
EXPORT_SYMBOL(masync_bucket_add);

