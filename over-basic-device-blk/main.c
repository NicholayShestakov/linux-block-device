#include "asm-generic/errno-base.h"
#include "linux/bio.h"
#include "linux/blk_types.h"
#include "linux/device.h"
#include "linux/err.h"
#include "linux/file.h"
#include "linux/fs.h"
#include "linux/gfp_types.h"
#include "linux/moduleparam.h"
#include "linux/nodemask_types.h"
#include "linux/printk.h"
#include "linux/stddef.h"
#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>

#define DEVICE_NAME "obdblk"

static char *base_device_name = NULL;
module_param(base_device_name, charp, 0444);

struct obd_block_dev {
        struct file *base_file;
        struct gendisk *gd;
        int major;
};

static struct obd_block_dev *obd_dev;

static unsigned bytes_written = 0;
static unsigned bytes_read = 0;

static ssize_t obd_statistics_show(struct device *dev, struct device_attribute *attr, char *buf)
{
        return sprintf(buf, "== obdblk statistics ==\n - Bytes written: %u\n - Bytes read: %u\n",
                        bytes_written, bytes_read);
}

static DEVICE_ATTR_RO(obd_statistics);

static void obd_submit_bio(struct bio *bio)
{
        struct obd_block_dev *dev = bio->bi_bdev->bd_disk->private_data;

        if (bio_data_dir(bio))
                bytes_written += bio->bi_iter.bi_size;
        else
                bytes_read += bio->bi_iter.bi_size;

        struct block_device *base_device = file_bdev(dev->base_file);
        bio_set_dev(bio, base_device);
        submit_bio(bio);
}

static int obd_open(struct gendisk *gd, blk_mode_t mode)
{
        return 0;
}

static void obd_release(struct gendisk *gd)
{
}

static const struct block_device_operations obd_fops = {
        .owner = THIS_MODULE,
        .open = obd_open,
        .release = obd_release,
        .submit_bio = obd_submit_bio
};

static int __init obd_init(void)
{
        int ret;

        if (!base_device_name) {
                pr_err("obdblk: base device name is missing\n");
                return -EINVAL;
        }

        obd_dev = kzalloc(sizeof(*obd_dev), GFP_KERNEL);
        if (!obd_dev) {
                pr_err("obdblk: memory allocation error\n");
                return -ENOMEM;
        }

        obd_dev->base_file = bdev_file_open_by_path(base_device_name,
                        BLK_OPEN_READ | BLK_OPEN_WRITE, obd_dev, NULL);
        if (IS_ERR(obd_dev->base_file)) {
                pr_err("obdblk: base device file opening error\n");
                ret = PTR_ERR(obd_dev->base_file);
                goto free_dev;
        }
       
        struct queue_limits lim = file_bdev(obd_dev->base_file)->bd_disk->queue->limits;

        obd_dev->major = register_blkdev(0, DEVICE_NAME);
        if (obd_dev->major < 0) {
                pr_err("obdblk: major registering error\n");
                ret = obd_dev->major;
                goto close_file;
        }
       
        obd_dev->gd = blk_alloc_disk(&lim, NUMA_NO_NODE);
        if (IS_ERR(obd_dev->gd)) {
                ret = PTR_ERR(obd_dev->gd);
                goto unregister_blk;
        }

        obd_dev->gd->major = obd_dev->major;
        obd_dev->gd->first_minor = 0;
        obd_dev->gd->minors = 1;
        obd_dev->gd->fops = &obd_fops;
        obd_dev->gd->private_data = obd_dev;
        snprintf(obd_dev->gd->disk_name, 32, DEVICE_NAME);

        set_capacity(obd_dev->gd, bdev_nr_sectors(
                                file_bdev(obd_dev->base_file)));

        ret = add_disk(obd_dev->gd);
        if (ret) {
                pr_err("obdblk: gendisk addition error\n");
                goto put_disk;
        }

        ret = device_create_file(disk_to_dev(obd_dev->gd),
                        &dev_attr_obd_statistics);
        if (ret) {
                pr_err("obdblk: creation sysfs file error\n");
                goto del_disk;
        }
        
        pr_info("obdblk: successfully loaded over %s\n", base_device_name);
        return 0;

del_disk:
        del_gendisk(obd_dev->gd);
put_disk:
        put_disk(obd_dev->gd);
unregister_blk:
        unregister_blkdev(obd_dev->major, DEVICE_NAME);
close_file:
        fput(obd_dev->base_file);
free_dev:
        kfree(obd_dev);
        return ret;
}

static void __exit obd_exit(void)
{
        if (obd_dev) {
                device_remove_file(disk_to_dev(obd_dev->gd),
                                &dev_attr_obd_statistics);
                del_gendisk(obd_dev->gd);
                put_disk(obd_dev->gd);
                unregister_blkdev(obd_dev->major, DEVICE_NAME);
                fput(obd_dev->base_file);
                kfree(obd_dev);
        }
        pr_info("obdblk: unloaded successfully\n");
}

module_init(obd_init);
module_exit(obd_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nicholay Shestakov");
MODULE_DESCRIPTION("Block device over basic device");
