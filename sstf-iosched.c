#include <linux/blkdev.h>
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/kernel.h>


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
	struct sstf_data *data = q->elevator->elevator_data;

	if (!list_empty(&data->queue)) {
		struct request *rq = list_entry(data->queue.next, struct request, queuelist);

		printk(KERN_DEBUG "SSTF: sector is dispatched at: %llu\n",blk_rq_pos(rq));

		list_del_init(&rq->queuelist);

		elv_dispatch_add_tail(q, rq);
		
		return 1;
	}
	return 0;
}

static void sstf_add_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *data = q->elevator->elevator_data;
	struct list_head *curr;
	struct list_head *temp;
	struct request *current_node;

	if (list_empty(&data->queue)) {
		printk(KERN_DEBUG "SSTF: queue empty. \n");

		list_add(&rq->queuelist, &data->queue);
	} else {
		list_for_each_safe(curr, temp, &data->queue) {
			current_node = list_entry(curr, struct request, queuelist);

			if (blk_rq_pos(current_node) < blk_rq_pos(rq)) {
				printk(KERN_DEBUG "SSTF: adding a request \n");

				list_add(&rq->queuelist, &current_node->queuelist);
				break;
			}
		}
	}
}

static struct request *
sstf_former_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *data = q->elevator->elevator_data;

	if (rq->queuelist.prev == &data->queue)
		return NULL;

	return list_entry(rq->queuelist.prev, struct request, queuelist);
}

static struct request *
sstf_latter_request(struct request_queue *q, struct request *rq)
{
	struct sstf_data *data = q->elevator->elevator_data;

	if (rq->queuelist.next == &data->queue)
		return NULL;
	return list_entry(rq->queuelist.next, struct request, queuelist);
}

static int sstf_init_queue(struct request_queue *q, struct elevator_type *e)
{
	struct sstf_data *data;
	struct elevator_queue *eq;

	eq = elevator_alloc(q, e);
	if (!eq)
		return -ENOMEM;

	data = kmalloc_node(sizeof(*data), GFP_KERNEL, q->node);
	if (!data) {
		kobject_put(&eq->kobj);
		return -ENOMEM;
	}
	eq->elevator_data = data;

	INIT_LIST_HEAD(&data->queue);

	spin_lock_irq(q->queue_lock);
	q->elevator = eq;
	spin_unlock_irq(q->queue_lock);

	printk(KERN_DEBUG "SSTF: queue created\n");
	return 0;
}

static void sstf_exit_queue(struct elevator_queue *e)
{
	struct sstf_data *data = e->elevator_data;

	BUG_ON(!list_empty(&data->queue));
	kfree(data);
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
MODULE_AUTHOR("Amber Horvath, Davian Lukman, Trung Huynh");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LOOK I/O scheduler");

