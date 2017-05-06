/*
 * elevator sstf
 */
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/kernel.h>

/*
 * References:
 * [0] http://lxr.free-electrons.com/source/include/linux/blkdev.h
 * [1] http://lxr.free-electrons.com/source/include/linux/list.h
 */

struct sstf_data {
	struct list_head queue;
};

static void sstf_merged_requests(struct request_queue *q, struct request *rq,
				 struct request *next)
{
	list_del_init(&next->queuelist);
}

static int sstf_dispatch(struct request_queue *q, int force)
{
	struct sstf_data *sd = q->elevator->elevator_data;

	//Try to put the fields that are referenced together in the same cacheline. then
	//get the first element from a list
	if (!list_empty(&sd->queue)) {
		struct request *rq;
		rq = list_entry(sd->queue.next, struct request, queuelist);

		printk(KERN_DEBUG "SSTF: dispatching sector: %lu \n",blk_rq_pos(rq));
		
		list_del_init(&rq->queuelist);
		elv_dispatch_sort(q, rq); //dont know what it does gonna leave it here
		return 1;
	}
	return 0;
}

static void sstf_add_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *sd = q->elevator->elevator_data;
	struct list_head *curr_pos;
	struct request *curr_node, *next_node;

	//doesnt matter where rq is added if empty.
	if (list_empty(&sd->queue)) {
		printk(KERN_DEBUG "SSTF: queue list empty adding item to queue.\n");
		list_add_tail(&rq->queuelist, &sd->queue);
	} else {

		//list iterate
		printk(KERN_DEBUG "SSTF: add_request: begin iterating queue.\n");
		list_for_each(curr_pos,&sd->queue) {
			
			curr_node = list_entry(curr_pos, struct request, queuelist);
			next_node = list_entry(curr_pos->next, struct request, queuelist);

			//if request sector position is higher than current.
			if(blk_rq_pos(curr_node) < blk_rq_pos(rq)){
				printk(KERN_DEBUG "SSTF: add_request: inserting  item via insert sort.\n");
				//__list_add(&rq->queuelist,&curr_node->queuelist,&next_node->queuelist);
				list_add(&rq->queuelist,&curr_node->queuelist);
				break;
			}
		}
	}
}

static struct request *
sstf_former_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *sd = q->elevator->elevator_data;

	if (rq->queuelist.prev == &sd->queue)
		return NULL;
	return list_entry(rq->queuelist.prev, struct request, queuelist);
}

static struct request *
sstf_latter_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *sd = q->elevator->elevator_data;

	if (rq->queuelist.next == &sd->queue)
		return NULL;
	return list_entry(rq->queuelist.next, struct request, queuelist);
}

static int sstf_init_queue(struct request_queue *q, struct elevator_type *e)
{
	struct sstf_data *sd;
	//struct elevator_queue *eq;

	// eq = elevator_alloc(q, e);
	// if (!eq)
	// 	return -ENOMEM;

	sd = kmalloc_node(sizeof(*sd), GFP_KERNEL, q->node);
	if (!sd) {
		//kobject_put(&eq->kobj);
		return -ENOMEM;
	}
	//eq->elevator_data = sd;

	INIT_LIST_HEAD(&sd->queue);
	// sd->elv_start_position = 0; //starting position init to zero.
	// spin_lock_irq(q->queue_lock);
	// q->elevator = eq;
	// spin_unlock_irq(q->queue_lock);
	printk(KERN_DEBUG "SSTF: INIT queue\n");
	return 0;
}

static void sstf_exit_queue(struct elevator_queue *e)
{
	struct sstf_data *sd = e->elevator_data;

	BUG_ON(!list_empty(&sd->queue));
	kfree(sd);
}

static struct elevator_type elevator_sstf = {
	.ops = {
		.elevator_merge_req_fn		= sstf_merged_requests,
		.elevator_dispatch_fn		= sstf_dispatch,
		.elevator_add_req_fn		= sstf_add_request,
		.elevator_former_req_fn		= sstf_former_request,
		.elevator_latter_req_fn		= sstf_latter_request,
		.elevator_init_fn		= sstf_init_queue,
		.elevator_exit_fn		= sstf_exit_queue,
	},
	.elevator_name = "sstf",
	.elevator_owner = THIS_MODULE,
};

static int __init sstf_init(void)
{
	return elv_register(&elevator_sstf);
}

static void __exit sstf_exit(void)
{
	elv_unregister(&elevator_sstf);
}

module_init(sstf_init);
module_exit(sstf_exit);


MODULE_AUTHOR("Alessandro Lim, Kevin Turkington");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SSTF IO scheduler");