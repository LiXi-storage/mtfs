/*
 * Copyright (C) 2011 Li Xi <pkuelelixi@gmail.com>
 */

#include <debug.h>
#include <memory.h>
#include <stdlib.h>
#include <mtfs_lock.h>
#include <multithread.h>

#define LOCK_STATE_NULL     0
#define LOCK_STATE_GRANTED  1
#define LOCK_STATE_BLOCKED  2


struct lock_info {
	struct mlock *lock;
	struct mlock_enqueue_info einfo;
	int state;
};

struct lock_control {
	pthread_mutex_t req_mutex;
	pthread_cond_t req_cond;
	pthread_cond_t ack_cond;

	int req_count;
	struct mlock_resource *resource;
	struct lock_info *lock_array;

	char operation;
	int index;
};

static void control_init(struct lock_control *control,
                         struct mlock_resource *resource,
                         struct lock_info *lock_array)
{
	pthread_mutex_init(&control->req_mutex, NULL);
	pthread_cond_init(&control->req_cond, NULL);	
	pthread_cond_init(&control->ack_cond, NULL);
	control->req_count = 0;
	control->resource = resource;
	control->lock_array = lock_array;
	control->operation = 0;
	control->index = -1;
}

void cleanup(void *arg)
{
	struct lock_control *control = (struct lock_control *)(arg);

	pthread_mutex_unlock(&control->req_mutex);
}

static void *
mtfs_locker(struct thread_info *info)
{
	struct lock_control *control = (struct lock_control *)(info->thread_data);
	struct lock_info *lock_info = NULL;
	struct mlock_enqueue_info einfo;
	char operation = 0;
	struct mlock *lock = NULL;
	int block = 0;
	int ret = 0;

	ret = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	if (ret) {
		MERROR("pthread_setcancelstate\n");
	}

	pthread_cleanup_push(cleanup, control);

	while (1) {
		block = 0;
		pthread_mutex_lock(&control->req_mutex);
		while (control->req_count < 1) {
			pthread_cond_wait(&control->req_cond, &control->req_mutex);
		}

		control->req_count --;
		MASSERT(control->req_count == 0);
		lock_info = &control->lock_array[control->index];
		einfo = lock_info->einfo;
		einfo.flag |= MLOCK_FL_BLOCK_NOWAIT;
		operation = control->operation;
		if (operation == '+') {
			MASSERT(lock_info->state == 0);
			lock_info->lock = mlock_enqueue(control->resource, &einfo);
			if (IS_ERR(lock_info->lock)) {
				MDEBUG("lock[%d]  blocked\n",
				       control->index);
				lock_info->state = LOCK_STATE_BLOCKED;
				lock_info->lock = NULL;
				block = 1;
			} else {
				MDEBUG("lock[%d] granted\n",
				       control->index);
				lock_info->state = LOCK_STATE_GRANTED;
			}
		} else {
			MASSERT(control->operation == '-');
			MASSERT(lock_info->state == LOCK_STATE_GRANTED);
			mlock_cancel(lock_info->lock);
			lock_info->lock = NULL;
			lock_info->state = LOCK_STATE_NULL;
		}
		pthread_cond_signal(&control->ack_cond);
		
		pthread_mutex_unlock(&control->req_mutex);

		if (block) {
			MASSERT(operation == '+');
			einfo.flag &= ~MLOCK_FL_BLOCK_NOWAIT;
			lock = mlock_enqueue(control->resource, &einfo);
			MASSERT(!IS_ERR(lock));
			pthread_mutex_lock(&control->req_mutex);
			lock_info->lock = lock;
			lock_info->state = LOCK_STATE_GRANTED;
			pthread_mutex_unlock(&control->req_mutex);
		}
	}
	pthread_cleanup_pop(0);
}
#define MAX_TIME 3

static int lock_check(struct lock_control *control, struct lock_info *lock_info, int state)
{
	int ret = 0;
	int time = 0;

	pthread_mutex_lock(&control->req_mutex);
	switch (state) {
	case LOCK_STATE_NULL:
		if (lock_info->state != LOCK_STATE_NULL ||
		    lock_info->lock != NULL) {
			MERROR("error actual state %d\n", lock_info->state);
			ret = -1;
		}
		break;
	case LOCK_STATE_GRANTED:
		while (lock_info->state == LOCK_STATE_BLOCKED && time < MAX_TIME) {
			pthread_mutex_unlock(&control->req_mutex);
			sleep(1);
			time++;
			pthread_mutex_lock(&control->req_mutex);
		}

		if (lock_info->state != LOCK_STATE_GRANTED ||
		    lock_info->lock == NULL ||
		    mlock_state(lock_info->lock) != MLOCK_STATE_GRANTED) {
			MERROR("error actual state %d\n", lock_info->state);
			ret = -1;
		}
		break;
	case LOCK_STATE_BLOCKED:
		if (lock_info->state != LOCK_STATE_BLOCKED) {
			MERROR("error actual state %d\n", lock_info->state);
			ret =-1;
		}
		break;
	default:
		MERROR("error input state %d\n", state);
		ret = -1;
		break;
	}
	pthread_mutex_unlock(&control->req_mutex);

	return ret;
}

static int char2state(char c)
{
	switch (c) {
	case 'G': return LOCK_STATE_GRANTED;
	case 'N': return LOCK_STATE_NULL;
	case 'B': return LOCK_STATE_BLOCKED;
	default: MERROR("unrecongnized mode %c\n", c); return -1;
	}
}

static mlock_mode_t char2mod(char c)
{
	switch (c) {
	case 'R': return MLOCK_MODE_READ;
	case 'W': return MLOCK_MODE_WRITE;
	case 'D': return MLOCK_MODE_DIRTY;
	case 'C': return MLOCK_MODE_CLEAN;
	case 'K': return MLOCK_MODE_CHECK;
	case 'F': return MLOCK_MODE_FLUSH;
	default: MERROR("unrecongnized mode %c\n", c); return 0;
	}
}

int main()
{
	struct mlock_resource resource;
	int ret = 0;
	int i = 0;
	int index = 0;
	struct lock_info *lock_array = NULL;
	int lock_number = 10;
	char mode;
	char operation;
	int effect_index = 0;
	char state;
	struct thread_group thread_group;
	struct lock_control *control = NULL;
	int state_value;

	fscanf(stdin, "%d", &lock_number);
	if (lock_number <= 0) {
		MERROR("lock number error\n", lock_number);
		ret = -EINVAL;
		goto out;
	}

	MTFS_ALLOC(lock_array, sizeof(*lock_array) * lock_number);
	if (lock_array == NULL) {
		ret = -ENOMEM;
		MERROR("failed to alloc lock array\n");
		goto out;
	}

	for (i = 0; i < lock_number; i++) {
		fscanf(stdin, "%d %c %d%d",
		       &index,
		       &mode,
		       &lock_array[i].einfo.data.mlp_extent.start,
		       &lock_array[i].einfo.data.mlp_extent.end);
		if (index != i) {
			ret = -EINVAL;
			MERROR("index error %d\n", index);
			goto out_free_array;
		}

		lock_array[i].einfo.mode = char2mod(mode);
		if (lock_array[i].einfo.mode == 0) {
			ret = -EINVAL;
			MERROR("invalid mode\n");
			goto out_free_array;
		}

		if (lock_array[i].einfo.data.mlp_extent.start >
		    lock_array[i].einfo.data.mlp_extent.end) {
		    	ret = -EINVAL;
			MERROR("interval error [%d, %d]\n",
			       lock_array[i].einfo.data.mlp_extent.start,
			       lock_array[i].einfo.data.mlp_extent.end);
			goto out_free_array;
		}
		printf("%d %c %d %d\n",
		       index,
		       mode,
		       lock_array[i].einfo.data.mlp_extent.start,
		       lock_array[i].einfo.data.mlp_extent.end);
	}

	MTFS_ALLOC_PTR(control);
	if (control == NULL) {
		ret = -ENOMEM;
		MERROR("failed to alloc control\n");
		goto out_free_array;
	}
	mlock_resource_init(&resource);
	control_init(control, &resource, lock_array);

	thread_group.thread_number = lock_number;
	thread_group.identifier = "mlocker";
	thread_group.start_routine = mtfs_locker;
	ret = create_thread_group(&thread_group, control);
	if (ret == -1) {
		MERROR("failed to create thread\n");
		goto out_free_control;
	}
	ret = 0;

	while(fscanf(stdin, "%d", &index) != EOF) {
		if (index < 0 || index >= lock_number) {
			MERROR("index error %d\n",
			       index);
			goto out_stop_group;
		}

		fscanf(stdin, " %c", &operation);
		if (operation != '+' && operation != '-') {
			MERROR("operation error %c\n",
			       operation);
			goto out_stop_group;
		}

		printf("%c %d\n",
		       operation,
		       index);

		pthread_mutex_lock(&control->req_mutex);
		{
			control->operation = operation;
			control->index = index;

			control->req_count ++;
			MASSERT(control->req_count == 1);
			pthread_cond_signal(&control->req_cond);
			while (control->req_count > 0) {
				pthread_cond_wait(&control->ack_cond, &control->req_mutex);
			}
		}
		pthread_mutex_unlock(&control->req_mutex);

		for (i = 0; i < lock_number; i++) {
			fscanf(stdin, "%d %c",
			       &effect_index,
			       &state);
			if (effect_index != i) {
				ret = -EINVAL;
				MERROR("index error %d\n", effect_index);
				goto out_stop_group;
			}
			state_value = char2state(state);
			if (state_value < 0) {
				ret = -EINVAL;
				MERROR("index error %d\n", effect_index);
				goto out_stop_group;
			}
			printf("%d %c\n",
			       effect_index,
			       state);

			ret = lock_check(control, &lock_array[i], state_value);
			if (ret) {
				MERROR("lock[%d] state error\n", effect_index);
				goto out_stop_group;
			}
		}
	}
out_stop_group:
	stop_thread_group(&thread_group);
out_free_control:
	MTFS_FREE_PTR(control);
out_free_array:
	MTFS_FREE(lock_array, sizeof(*lock_array) * lock_number);
out:
	ret = 0;
	return ret;
}