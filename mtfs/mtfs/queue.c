/* Copied from kernel/drivers/scsi/arm/queue.c */

/*
 * For both kernel and userspace use
 * DO NOT use anything special that opposes this purpose
 */

#include <mtfs_queue.h>
#include <debug.h>
#include <memory.h>

/*
 * Function: int queue_initialise (queue_t *queue)
 * Purpose : initialise a queue
 * Params  : queue - queue to initialise
 * Returns : !0 on error, 0 on success
 */
int queue_initialise(queue_t *queue, unsigned int size)
{
	unsigned int nqueues = MAX_QUEUE;
	node_t *node;
	int ret = 0;
	MTFS_INIT_LIST_HEAD(&queue->head);

	MTFS_INIT_LIST_HEAD(&queue->free);

	if (!size) {
		nqueues = MAX_QUEUE;
	} else {
		nqueues = size;
	}

	MTFS_ALLOC(queue->alloc, sizeof(*node) * nqueues);
	if (!queue->alloc) {
		ret = -ENOMEM;
		goto out;
	}
	queue->size = sizeof(*node) * nqueues;

	for (node = queue->alloc; nqueues; node++, nqueues--) {
		node->data = NULL;
		mtfs_list_add(&node->list, &queue->free);
	}
out:
	return ret;
}

/*
 * Function: void queue_free (queue_t *queue)
 * Purpose : free a queue
 * Params  : queue - queue to free
 */
void queue_free(queue_t *queue)
{
	if (!mtfs_list_empty(&queue->head)) {
		MWARN("freeing non-empty queue %p\n", queue);
	}

	MTFS_FREE(queue->alloc, queue->size);
}

/*
 * Function: int __queue_add(queue_t *queue, void *data, int head)
 * Purpose : Add a node onto a queue, adding data to head.
 * Params  : queue - destination queue
 *	     data - data to add
 *	     head  - add command to head of queue
 * Returns : !0 on error, 0 on success
 */
int __queue_add(queue_t *queue, void *data, int head)
{
	node_t *node;
	mtfs_list_t *new;
	int ret = 0;

	if (mtfs_list_empty(&queue->free)) {
		ret = -ENOMEM; /* which errno? */
		goto empty;
	}

	new = queue->free.next;
	mtfs_list_del(new);

	node = mtfs_list_entry(new, node_t, list);
	node->data = data;

	if (head) {
		mtfs_list_add(new, &queue->head);
	} else {
		mtfs_list_add_tail(new, &queue->head);
	}

empty:
	return ret;
}

static void *__queue_remove(queue_t *queue, mtfs_list_t *ent)
{
	node_t *node;

	/*
	 * Move the entry from the "used" list onto the "free" list
	 */
	mtfs_list_del(ent);
	node = mtfs_list_entry(ent, node_t, list);

	mtfs_list_add(ent, &queue->free);
	return node->data;
}

/*
 * Function: data_t *queue_remove (queue)
 * Purpose : removes first item from a queue
 * Params  : queue   - queue to remove command from
 * Returns : data if successful (and a reference), or NULL if no command available
 */
void *queue_remove(queue_t *queue)
{
	void *data = NULL;

	if (!mtfs_list_empty(&queue->head)) {
		data = __queue_remove(queue, queue->head.next);
	}

	return data;
}

/*
 * Function: queue_remove_all_target(queue, target)
 * Purpose : remove all nodes from the queue for a specified target
 * Params  : queue  - queue to remove command from
 * Returns : nothing
 */
void queue_remove_all_target(queue_t *queue)
{
	mtfs_list_t *list;

	mtfs_list_for_each(list, &queue->head) {
			__queue_remove(queue, list);
	}
}

/*
 * Function: int queue_remove_data(queue_t *queue, void *data)
 * Purpose : remove a specific node from the queues
 * Params  : queue - queue to look in
 *	     data - node to find
 * Returns : 0 if not found
 */
int queue_remove_data(queue_t *queue, void *data)
{
	mtfs_list_t *list;
	int found = 0;

	mtfs_list_for_each(list, &queue->head) {
		node_t *node = mtfs_list_entry(list, node_t, list);
		if (node->data == data) {
			__queue_remove(queue, list);
			found = 1;
			break;
		}
	}

	return found;
}

void list_dump(mtfs_list_t *head)
{
	mtfs_list_t *pos;
	MPRINT("HEAD");
	mtfs_list_for_each(pos, head) {
		MPRINT("->%d", *((int *)mtfs_list_entry(pos, node_t, list)->data));
	}
	MPRINT("\n");
}

void queue_dump(queue_t *queue)
{
	list_dump(&queue->head);
}

