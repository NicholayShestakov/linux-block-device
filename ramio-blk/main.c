#include <linux/module.h>
#include <linux/sched.h>
#include <linux/highmem.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>

#define MY_DEVICE_NAME "ramioblk"
#define MY_SECTOR_SIZE 512
#define MY_DEVICE_CAPACITY 32768 // 16M

struct ramio_block_dev {
        sector_t capacity;
        u8 *data;
        struct blk_mq_tag_set tag_set;
        struct request_queue *queue;
        struct gendisk *gd;
};

static struct ramio_block_dev *ramio_dev;

static blk_status_t ramio_queue_rq(struct blk_mq_hw_ctx *hctx,
                const struct blk_mq_queue_data *bd)
{
        struct request *req = bd->rq;
        struct ramio_block_dev *dev = hctx->queue->queuedata;
        unsigned long offset = blk_rq_pos(req) * MY_SECTOR_SIZE;
        unsigned long len = blk_rq_bytes(req);
        int dir = rq_data_dir(req);
        struct bio_vec bvec;
        struct req_iterator iter;

        blk_mq_start_request(req);
        pr_info("ramioblk: request started\n");
        
        if (offset + len > dev->capacity * MY_SECTOR_SIZE) {
                pr_info("ramioblk: request ended with IO error\n");
                blk_mq_end_request(req, BLK_STS_IOERR);
                return BLK_STS_IOERR;
        }

        rq_for_each_bvec(bvec, req, iter) {
                void *buffer = bvec_kmap_local(&bvec);
                size_t count = bvec.bv_len;

                if (dir == READ) {
                        memcpy(buffer, dev->data + offset, count);
                } else {
                        memcpy(dev->data + offset, buffer, count);
                }

                kunmap_local(buffer);
                offset += count;
        }

        pr_info("ramioblk: request successfully ended\n");
        blk_mq_end_request(req, BLK_STS_OK);
        return BLK_STS_OK;
}

static int ramio_open(struct gendisk *gd, blk_mode_t mode)
{
        return 0;
}

static void ramio_release(struct gendisk *gd)
{
}

static int ramio_ioctl(struct block_device *bdev, blk_mode_t mode,
                unsigned int cmd, unsigned long arg)
{
        return -ENOTTY;
}

static const struct block_device_operations ramio_fops = {
        .owner = THIS_MODULE,
        .open = ramio_open,
        .release = ramio_release,
        .ioctl = ramio_ioctl,
};

static const struct blk_mq_ops ramio_mq_ops = {
        .queue_rq = ramio_queue_rq,
};

static int __init ramio_init(void)
{
        int ret = 0;

        ramio_dev = kzalloc(sizeof(*ramio_dev), GFP_KERNEL);
        if (ramio_dev == NULL) {
                pr_err("ramioblk: could not allocate memory for device\n");
                return -ENOMEM;
        }

        ramio_dev->capacity = MY_DEVICE_CAPACITY;

        ramio_dev->data = vmalloc(MY_DEVICE_CAPACITY * MY_SECTOR_SIZE);
        if (ramio_dev->data == NULL) {
                pr_err("ramioblk: could not allocate memory for device storage\n");
                ret = -ENOMEM;
                goto free_dev;
        }

        ramio_dev->tag_set.ops = &ramio_mq_ops;
        ramio_dev->tag_set.nr_hw_queues = 1;
        ramio_dev->tag_set.queue_depth = 128;
        ramio_dev->tag_set.numa_node = NUMA_NO_NODE;
        ramio_dev->tag_set.flags = 0;
        ramio_dev->tag_set.cmd_size = 0;
        ramio_dev->tag_set.driver_data = ramio_dev;
        ramio_dev->tag_set.nr_maps = 1;

        ret = blk_mq_alloc_tag_set(&ramio_dev->tag_set);
        if (ret) {
                pr_err("ramioblk: cound not allocate memory for tag set\n");
                goto free_buffer;
        }

        int ramio_major = register_blkdev(0, MY_DEVICE_NAME);
        if (ramio_major < 0) {
                pr_err("ramioblk: could not register device in system\n");
                ret = ramio_major;
                goto free_tag_set;
        }

        ramio_dev->gd = blk_mq_alloc_disk(&ramio_dev->tag_set, NULL, ramio_dev);
        if (IS_ERR(ramio_dev->gd)) {
                pr_err("ramioblk: could not allocate memory for disk");
                ret = PTR_ERR(ramio_dev->gd);
                goto unregister_blkdev;
        }

        ramio_dev->queue = ramio_dev->gd->queue;
        ramio_dev->queue->queuedata = ramio_dev;
        
        ramio_dev->queue->limits.logical_block_size = MY_SECTOR_SIZE;
        ramio_dev->queue->limits.physical_block_size =  MY_SECTOR_SIZE;

        ramio_dev->gd->major = ramio_major;
        ramio_dev->gd->first_minor = 0;
        ramio_dev->gd->minors = 1;
        ramio_dev->gd->fops = &ramio_fops;
        snprintf(ramio_dev->gd->disk_name, 32, MY_DEVICE_NAME);

        set_capacity(ramio_dev->gd, ramio_dev->capacity);

        ret = add_disk(ramio_dev->gd);
        if (ret) {
                pr_err("ramioblk: could not add disk\n");
                goto push_disk;
        }

        pr_info("ramioblk: loaded successfully\n");
        return 0;

push_disk:
        put_disk(ramio_dev->gd);
unregister_blkdev:
        unregister_blkdev(ramio_major, MY_DEVICE_NAME);
free_tag_set:
        blk_mq_free_tag_set(&ramio_dev->tag_set);
free_buffer:
        vfree(ramio_dev->data);
free_dev:
        kfree(ramio_dev);
        return ret;
}

static void __exit ramio_exit(void)
{
        del_gendisk(ramio_dev->gd);
        blk_mq_free_tag_set(&ramio_dev->tag_set);
        vfree(ramio_dev->data);
        put_disk(ramio_dev->gd);
        kfree(ramio_dev);

        pr_info("ramioblk: unloaded successfully\n");
}

module_init(ramio_init);
module_exit(ramio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nicholay Shestakov");
MODULE_DESCRIPTION("RAM IO block device");
