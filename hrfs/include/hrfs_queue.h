/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
 */

#ifndef __HRFS_QUEUE_H__
#define __HRFS_QUEUE_H__
#include <hrfs_list.h>
#include <hrfs_types.h>

#define MAX_QUEUE	32

typedef struct queue {
	hrfs_list_t head;
	hrfs_list_t free;
	//spinlock_t queue_lock;
	size_t size;      /* size of allocated mem */
	void *alloc;			/* start of allocated mem */
} queue_t;


typedef struct node {
	hrfs_list_t list;
	void *data;
} node_t;

/*
 * Function: void queue_initialise (queue_t *queue)
 * Purpose : initialise a queue
 * Params  : queue - queue to initialise
 *           size - longest length of queue 
 */
int queue_initialise(queue_t *queue, unsigned int size);


/*
 * Function: void queue_free (Queue_t *queue)
 * Purpose : free a queue
 * Params  : queue - queue to free
 */
void queue_free (queue_t *queue);

/*
 * Function: int __queue_add(queue_t *queue, void *data, int head)
 * Purpose : Add a node onto a queue, adding data to head.
 * Params  : queue - destination queue
 *	     data - data to add
 *	     head  - add command to head of queue
 * Returns : !0 on error, 0 on success
 */
int __queue_add(queue_t *queue, void *data, int head);

/*
 * Function: data_t *queue_remove (queue)
 * Purpose : removes first SCSI command from a queue
 * Params  : queue   - queue to remove command from
 * Returns : Scsi_Cmnd if successful (and a reference), or NULL if no command available
 */
void *queue_remove(queue_t *queue);

/*
 * Function: queue_remove_all_target(queue, target)
 * Purpose : remove all nodes from the queue for a specified target
 * Params  : queue  - queue to remove command from
 * Returns : nothing
 */
void queue_remove_all_target(queue_t *queue);

/*
 * Function: int queue_remove_data(Queue_t *queue, Scsi_Cmnd *SCpnt)
 * Purpose : remove a specific node from the queues
 * Params  : queue - queue to look in
 *	     SCpnt - command to find
 * Returns : 0 if not found
 */
int queue_remove_data(queue_t *queue, void *data);
#endif /* __HRFS_QUEUE_H__ */
