/*
 * Elevator LOOK I/O Scheduler
 * Reference: 
 * http://classes.engr.oregonstate.edu/eecs/fall2011/cs411/proj03.pdf
 */
#include <linux/blkdev.h>
#include <linux/elevator.h>
#include <linux/bio.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>

struct sstf_data {
	struct list_head queue;

   sector_t current_head_pos;
   int direction;
};

static void sstf_merged_requests(struct request_queue *que, struct request *req, struct request *next) {
	list_del_init(&next->queuelist);
}

static int sstf_dispatch(struct request_queue *que, int force) {
	struct sstf_data *ssd = que->elevator->elevator_data;

	if (!list_empty(&ssd->queue)) {
		struct request *req, *next_req, *prev_req;
      struct list_head *list;

      struct current_pos = blk_rq_pos(req);

      next_req = list_entry(ssd->queue.next, struct request, queuelist);
      prev_req = list_entry(ssd->queue.prev, struct request, queuelist);

      if (next_req == prev_req) {
         req = next_req;
      } else {
         /* forward */
         if(ssd->direction == 1) {
            if(next_req->__sector > ssd->current_head_pos) {
               req = next_req;
            } else {
               ssd->direction = 0;
               req = prev_req;
            }
         } else {
            if(prev_req->__sector < ssd->current_head_pos) {
               req = prev_req;
            } else {
               ssd->direction = 1;
               req = next_req;
            }
         }
      }

		list_del_init(&req->queuelist);

      ssd->current_head_pos = blk_rq_pos(req) + blk_rq_sectors(req);

		elv_dispatch_sort(que, req);
		return 1;
	}
	return 0;
}

static void sstf_add_request(struct request_queue *que, struct request *req) {
	struct sstf_data *ssd = que->elevator->elevator_data;
   struct request *prev_req, *next_req;

   if(list_empty(&ssd->queue)) {
      list_add(&req->queuelist, &ssd->queue);
   }

   if(list_empty(&ssd->queue)) {
      next_req = list_entry(ssd->queue.next, struct request, queuelist);
      prev_req = list_entry(ssd->queue.prev, struct request, queuelist);


      while (blk_rq_pos(req) > blk_rq_pos(next_req)) {
         next_req = list_entry(next_req->queuelist.next, struct request, queuelist);
         prev_req = list_entry(prev_req->queuelist.prev, struct request, queuelist);
      }

   }

	list_add_tail(&req->queuelist, &ssd->queue);
}

static struct request *
sstf_former_request(struct request_queue *que, struct request *req) {
	struct sstf_data *ssd = que->elevator->elevator_data;

	if (req->queuelist.prev == &ssd->queue)
		return NULL;
	return list_entry(req->queuelist.prev, struct request, queuelist);
}

static struct request *
sstf_latter_request(struct request_queue *que, struct request *req) {
	struct sstf_data *ssd = que->elevator->elevator_data;

	if (req->queuelist.next == &ssd->queue)
		return NULL;
	return list_entry(req->queuelist.next, struct request, queuelist);
}

static int sstf_init_queue(struct request_queue *que, struct elevator_type *ele) {
	struct sstf_data *ssd;
	struct elevator_queue *eq;

	eq = elevator_alloc(que, ele);
	if (!eq)
		return -ENOMEM;

	ssd = kmalloc_node(sizeof(*ssd), GFP_KERNEL, que->node);
	if (!ssd) {
		kobject_put(&eq->kobj);
		return -ENOMEM;
	}
	eq->elevator_data = ssd;

	INIT_LIST_HEAD(&ssd->queue);

	spin_lock_irq(que->queue_lock);
	que->elevator = eq;
	spin_unlock_irq(que->queue_lock);
	return 0;
}

static void sstf_exit_queue(struct elevator_queue *ele) {
	struct sstf_data *ssd = ele->elevator_data;

	BUG_ON(!list_empty(&ssd->queue));
	kfree(ssd);
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

static int __init sstf_init(void) {
	return elv_register(&elevator_sstf);
}

static void __exit sstf_exit(void) {
	elv_unregister(&elevator_sstf);
}

module_init(sstf_init);
module_exit(sstf_exit);


MODULE_AUTHOR("Amber Horvath, Davian Lukman, Trung Huynh");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LOOK I/O scheduler");
