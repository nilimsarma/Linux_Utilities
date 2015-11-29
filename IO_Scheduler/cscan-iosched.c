/*
 * elevator cscan
 */
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

struct cscan_data {
	unsigned int curr;
    struct rb_root sort_list[2]; /* Used to keep a sorted list of requests*/
	sector_t last_sector;
};
/*
static void cscan_merged_requests(struct request_queue *q, struct request *rq,
				 struct request *next)
{
	list_del_init(&next->queuelist);
}
*/

static int cscan_dispatch(struct request_queue *q, int force)
{
	struct request *rq;
	struct cscan_data *cd = q->elevator->elevator_data;
	
	struct rb_node *node = rb_first(&(cd->sort_list[cd->curr]));
	if(!node) {
		cd->curr = 1-cd->curr;
		node = rb_first(&(cd->sort_list[cd->curr]));
	}

	if(node) {
		rq = rb_entry_rq(node);
		cd->last_sector = rq_end_sector(rq);
		elv_rb_del(&(cd->sort_list[cd->curr]), rq);
		elv_dispatch_add_tail(q, rq);
		return 1;
	}	
	return 0;
}

static void cscan_add_request(struct request_queue *q, struct request *rq)
{
	struct cscan_data *cd = q->elevator->elevator_data;
	struct rb_root *root;
        
    if(blk_rq_pos(rq) > cd->last_sector) {
		root = &(cd->sort_list[cd->curr]);
    } else {
		root = &(cd->sort_list[1-cd->curr]);
    }
	elv_rb_add(root, rq);
}

/*
static struct request *
cscan_former_request(struct request_queue *q, struct request *rq)
{
	struct cscan_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.prev == &nd->queue)
		return NULL;
	return list_entry(rq->queuelist.prev, struct request, queuelist);
}

static struct request *
cscan_latter_request(struct request_queue *q, struct request *rq)
{
	struct cscan_data *nd = q->elevator->elevator_data;

	if (rq->queuelist.next == &nd->queue)
		return NULL;
	return list_entry(rq->queuelist.next, struct request, queuelist);
}
*/

static int cscan_init_queue(struct request_queue *q, struct elevator_type *e)
{
	struct cscan_data *cd;
	struct elevator_queue *eq;

	eq = elevator_alloc(q, e);
	if (!eq)
		return -ENOMEM;

	cd = kmalloc_node(sizeof(*cd), GFP_KERNEL, q->node);
	if (!cd) {
		kobject_put(&eq->kobj);
		return -ENOMEM;
	}
	eq->elevator_data = cd;
		
	cd->sort_list[0] = RB_ROOT;
	cd->sort_list[1] = RB_ROOT;
	cd->curr = 0;
	cd->last_sector = 0;

	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);
	return 0;
}

static void cscan_exit_queue(struct elevator_queue *e)
{
	struct cscan_data *cd = e->elevator_data;

	//BUG_ON(!list_empty(&cd->queue));
	BUG_ON(!(cd->sort_list[0].rb_node == NULL));
	BUG_ON(!(cd->sort_list[1].rb_node == NULL));
	kfree(cd);
}

static struct elevator_type elevator_cscan = {
	.ops = {
		.elevator_merge_req_fn		= cscan_merged_requests,
		.elevator_dispatch_fn		= cscan_dispatch,
		.elevator_add_req_fn		= cscan_add_request,
		.elevator_former_req_fn		= cscan_former_request,
		.elevator_latter_req_fn		= cscan_latter_request,
		.elevator_init_fn		= cscan_init_queue,
		.elevator_exit_fn		= cscan_exit_queue,
	},
	.elevator_name = "cscan",
	.elevator_owner = THIS_MODULE,
};

static int __init cscan_init(void)
{
	printk(KERN_ALERT "\ncscan_init\n");
	return elv_register(&elevator_cscan);
}

static void __exit cscan_exit(void)
{
	printk(KERN_ALERT "\ncscan_exit\n");
	elv_unregister(&elevator_cscan);
}

module_init(cscan_init);
module_exit(cscan_exit);


MODULE_AUTHOR("Nilim Sarma");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CSCAN IO scheduler");

