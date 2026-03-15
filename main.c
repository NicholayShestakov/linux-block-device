#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>

#define MY_DEVICE_NAME "myblk"
#define MY_SECTOR_SIZE 512
#define MY_DEVICE_CAPACITY 32768 // 16M

struct my_block_dev {
        sector_t capacity;
        u8 *data;
        struct blk_mq_tag_set tag_set;
        struct request_queue *queue;
        struct gendisk *gd;
};

static struct my_block_dev *my_dev;

static blk_status_t my_queue_rq(struct blk_mq_hw_ctx *hctx,
                const struct blk_mq_queue_data *bd)
{
        struct request *req = bd->rq;
        struct my_block_dev *dev = hctx->queue->queuedata;
        unsigned long offset = blk_rq_pos(req) * MY_SECTOR_SIZE;
        unsigned long len = blk_rq_bytes(req);
        int dir = rq_data_dir(req);
        struct bio_vec bvec;
        struct req_iterator iter;

        blk_mq_start_request(req);
        pr_info("myblk: request started\n");
        
        if (offset + len > dev->capacity * MY_SECTOR_SIZE) {
                pr_info("myblk: request ended with IO error\n");
                blk_mq_end_request(req, BLK_STS_IOERR);
                return BLK_STS_IOERR;
        }

        rq_for_each_segment(bvec, req, iter) {
                void *buffer = page_address(bvec.bv_page) + bvec.bv_offset;
                size_t count = bvec.bv_len;

                if (dir == READ) {
                        memcpy(buffer, dev->data + offset, count);
                } else {
                        memcpy(dev->data + offset, buffer, count);
                }
                offset += count;
        }

        pr_info("myblk: request successfully ended\n");
        blk_mq_end_request(req, BLK_STS_OK);
        return BLK_STS_OK;
}

static int my_open(struct gendisk *gd, blk_mode_t mode)
{
        return 0;
}

static void my_release(struct gendisk *gd)
{
}

static int my_ioctl(struct block_device *bdev, blk_mode_t mode,
                unsigned int cmd, unsigned long arg)
{
        return -ENOTTY;
}

static const struct block_device_operations my_fops = {
        .owner = THIS_MODULE,
        .open = my_open,
        .release = my_release,
        .ioctl = my_ioctl,
};

static const struct blk_mq_ops my_mq_ops = {
        .queue_rq = my_queue_rq,
};

static int __init my_init(void)
{
        int ret = 0;

        my_dev = kzalloc(sizeof(*my_dev), GFP_KERNEL);
        if (my_dev == NULL) {
                pr_err("myblk: could not allocate memory for device\n");
                return -ENOMEM;
        }

        my_dev->capacity = MY_DEVICE_CAPACITY;

        my_dev->data = vmalloc(MY_DEVICE_CAPACITY * MY_SECTOR_SIZE);
        if (my_dev->data == NULL) {
                pr_err("myblk: could not allocate memory for device storage\n");
                ret = -ENOMEM;
                goto free_dev;
        }

        my_dev->tag_set.ops = &my_mq_ops;
        my_dev->tag_set.nr_hw_queues = 1;
        my_dev->tag_set.queue_depth = 128;
        my_dev->tag_set.numa_node = NUMA_NO_NODE;
        my_dev->tag_set.flags = 0;
        my_dev->tag_set.cmd_size = 0;
        my_dev->tag_set.driver_data = my_dev;
        my_dev->tag_set.nr_maps = 1;

        ret = blk_mq_alloc_tag_set(&my_dev->tag_set);
        if (ret) {
                pr_err("myblk: cound not allocate memory for tag set\n");
                goto free_buffer;
        }

        int my_major = register_blkdev(0, MY_DEVICE_NAME);
        if (my_major < 0) {
                pr_err("myblk: could not register device in system\n");
                ret = my_major;
                goto free_tag_set;
        }

        my_dev->gd = blk_mq_alloc_disk(&my_dev->tag_set, NULL, my_dev);
        if (IS_ERR(my_dev->gd)) {
                pr_err("myblk: could not allocate memory for disk");
                ret = PTR_ERR(my_dev->gd);
                goto unregister_blkdev;
        }

        my_dev->queue = my_dev->gd->queue;
        my_dev->queue->queuedata = my_dev;
        
        my_dev->queue->limits.logical_block_size = MY_SECTOR_SIZE;
        my_dev->queue->limits.physical_block_size =  MY_SECTOR_SIZE;

        my_dev->gd->major = my_major;
        my_dev->gd->first_minor = 0;
        my_dev->gd->minors = 1;
        my_dev->gd->fops = &my_fops;
        snprintf(my_dev->gd->disk_name, 32, MY_DEVICE_NAME);

        set_capacity(my_dev->gd, my_dev->capacity);

        ret = add_disk(my_dev->gd);
        if (ret) {
                pr_err("myblk: could not add disk\n");
                goto push_disk;
        }

        pr_info("myblk: loaded successfully\n");
        return 0;

push_disk:
        put_disk(my_dev->gd);
unregister_blkdev:
        unregister_blkdev(my_major, MY_DEVICE_NAME);
free_tag_set:
        blk_mq_free_tag_set(&my_dev->tag_set);
free_buffer:
        vfree(my_dev->data);
free_dev:
        kfree(my_dev);
        return ret;
}

static void __exit my_exit(void)
{
        del_gendisk(my_dev->gd);
        blk_mq_free_tag_set(&my_dev->tag_set);
        vfree(my_dev->data);
        put_disk(my_dev->gd);
        kfree(my_dev);

        pr_info("myblk: unloaded successfully\n");
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nicholay Shestakov");
MODULE_DESCRIPTION("Simple block device");
