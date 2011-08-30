/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

/*
 * For both kernel and userspace use
 * DO NOT use anything special that opposes this purpose
 */

/* Copy from kernel/drivers/scsi/arm/queue.c */
#include <hrfs_queue.h>
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
	HRFS_INIT_LIST_HEAD(&queue->head);

	HRFS_INIT_LIST_HEAD(&queue->free);

	if (!size) {
		nqueues = MAX_QUEUE;
	} else {
		nqueues = size;
	}

	HRFS_ALLOC(queue->alloc, sizeof(*node) * nqueues);
	if (!queue->alloc) {
		ret = -ENOMEM;
		goto out;
	}
	queue->size = sizeof(*node) * nqueues;

	for (node = queue->alloc; nqueues; node++, nqueues--) {
		node->data = NULL;
		hrfs_list_add(&node->list, &queue->free);
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
	if (!hrfs_list_empty(&queue->head)) {
		HWARN("freeing non-empty queue %p\n", queue);
	}

	HRFS_FREE(queue->alloc, queue->size);
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
	hrfs_list_t *new;
	int ret = 0;

	if (hrfs_list_empty(&queue->free)) {
		ret = -ENOMEM; /* which errno? */
		goto empty;
	}

	new = queue->free.next;
	hrfs_list_del(new);

	node = hrfs_list_entry(new, node_t, list);
	node->data = data;

	if (head) {
		hrfs_list_add(new, &queue->head);
	} else {
		hrfs_list_add_tail(new, &queue->head);
	}

empty:
	return ret;
}

static void *__queue_remove(queue_t *queue, hrfs_list_t *ent)
{
	node_t *node;

	/*
	 * Move the entry from the "used" list onto the "free" list
	 */
	hrfs_list_del(ent);
	node = hrfs_list_entry(ent, node_t, list);

	hrfs_list_add(ent, &queue->free);
	return node->data;
}

/*
 * Function: data_t *queue_remove (queue)
 * Purpose : removes first SCSI command from a queue
 * Params  : queue   - queue to remove command from
 * Returns : data if successful (and a reference), or NULL if no command available
 */
void *queue_remove(queue_t *queue)
{
	void *data = NULL;

	if (!hrfs_list_empty(&queue->head)) {
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
	hrfs_list_t *list;

	hrfs_list_for_each(list, &queue->head) {
			__queue_remove(queue, list);
	}
}

/*
 * Function: int queue_remove_data(Queue_t *queue, Scsi_Cmnd *SCpnt)
 * Purpose : remove a specific node from the queues
 * Params  : queue - queue to look in
 *	     SCpnt - command to find
 * Returns : 0 if not found
 */
int queue_remove_data(queue_t *queue, void *data)
{
	hrfs_list_t *list;
	int found = 0;

	hrfs_list_for_each(list, &queue->head) {
		node_t *node = hrfs_list_entry(list, node_t, list);
		if (node->data == data) {
			__queue_remove(queue, list);
			found = 1;
			break;
		}
	}

	return found;
}

void list_dump(hrfs_list_t *head)
{
	hrfs_list_t *pos;
	HPRINT("HEAD");
	hrfs_list_for_each(pos, head) {
		HPRINT("->%d", *((int *)hrfs_list_entry(pos, node_t, list)->data));
	}
	HPRINT("\n");
}

void queue_dump(queue_t *queue)
{
	list_dump(&queue->head);
}

