#include <linux/init.h>
#include <linux/module.h>

#include <linux/percpu.h>
#include <linux/cryptohash.h>
#include <libcfs/libcfs.h>
#include "memory.h"
#include "debug.h"

MODULE_LICENSE("GPL");

#define HRFS_LOG_PREFIX "hrfs"

#undef err
#define err(format,  arg...) printk(KERN_ERR     HRFS_LOG_PREFIX ": " format "\n" , ## arg)
#undef info
#define info(format, arg...) printk(KERN_INFO    HRFS_LOG_PREFIX ": " format "\n" , ## arg)
#undef warn
#define warn(format, arg...) printk(KERN_WARNING HRFS_LOG_PREFIX ": " format "\n" , ## arg)

/* Most helpful debugging aid */
#define assert(expr) ((void) ((expr) ? 0 : (err("assert failed at line %d",__LINE__))))


static __u32 random_in[8];
DEFINE_PER_CPU(__u32 [4], random_int_hash);
/*
 * like get_random_int in drivers/char/random.c
 */
unsigned int
get_random_int(void)
{
	__u32 *hash = get_cpu_var(random_int_hash);
	int ret;

	hash[0] += current->pid + jiffies + get_cycles() + (int)(long)&ret;
	ret = half_md4_transform(hash, random_in);
	put_cpu_var(random_int_hash);
	
	return ret;
}

/*
 * get_random_int_in_range() returns a int in the range [start, end]

 */
int 
random_int_in_range(int start, int end)
{
	unsigned int range = end - start + 1;
	assert(start <= end);

	return get_random_int() % range + start;
}

int check_stripe(int offset) {
	return 0;
}

int select_stripe_offset(int start, int end, int total)
{
	int offset = 0;
	int i = 0;
	int checked = 0;
	
	assert(start >= 0 && start < total);
	assert(end >= start && end < 2 * total);
	
	/*
	 * first select randomly, then sequentially, 
	 * in a easy and effective way
	 */
	offset = random_int_in_range(start, end);
	if (offset >= total) {
		offset -= total;
	}
	if (unlikely(check_stripe(offset))) {
		checked = offset;
		offset = -1;
		for (i = start; i < end; i++) {
			if (i >= total) {
				i -= total;
			}
			
			if (i == checked) {
				continue;
			}

			if (check_stripe(i)) {
				offset = i;
				break;
			}
		}
	}	
	
	assert(offset >= 0 && offset < total);
	return offset;
}

int calculate_independent_range(int offset, int width, int total, int *range_start, int *range_end)
{
	int ret = 0;
	int start = offset + width;
	int end = offset + total - width;

	assert(offset >= 0 && offset < total);	
	assert(width >= 0 && width <= total);
	
	if (start > end) {
		ret = -1;
		goto out;
	}
	
	if (start >= total) {
		start -= total;
		end -= total;
	}
	
	assert(start >= 0 && start < total);
	assert(end >= start && end < 2 * total);
	*range_start = start;
	*range_end = end;
out:
	return ret;
}

static int myinit(void)
{
	int i = 0;

	CDEBUG(D_CONSOLE, "random module inited.\n");
	printk("Module inited\n");
	
	
	for(i = 0; i < 100; i++) {
		printk("get random %d\n", select_stripe_offset(0, 59, 60));
	}

	printk("Memroy inited\n");
	return 0;
}

static void myexit(void)
{
	CDEBUG(D_CONSOLE, "random module exited.\n");
}

module_init(myinit);
module_exit(myexit);
