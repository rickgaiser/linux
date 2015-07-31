/*
 * PS3 NOR FLASH Storage Driver
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published
 * by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <linux/ata.h>
#include <linux/blkdev.h>
#include <linux/slab.h>

#include <asm/lv1call.h>
#include <asm/ps3stor.h>
#include <asm/firmware.h>


#define DEVICE_NAME		"ps3nflash"

#define BOUNCE_SIZE		(64*1024)

#define PS3NFLASH_MINORS	16


#define PS3NFLASH_NAME		"ps3nflash%c"


struct ps3nflash_private {
	spinlock_t lock;		/* Request queue spinlock */
	struct request_queue *queue;
	unsigned int blocking_factor;
	struct request *req;
	u64 raw_capacity;
	struct gendisk *gendisk[0];
};


static int ps3nflash_major;


static const struct block_device_operations ps3nflash_fops = {
	.owner		= THIS_MODULE,
};


static unsigned long region_flags[] =
{
	0x2, 0x2, 0x2, 0x2, 0x0, 0x0, 0x0, 0x0,
};
module_param_array(region_flags, ulong, NULL, S_IRUGO);
MODULE_PARM_DESC(region_flags, "Region flags");


static void ps3nflash_scatter_gather(struct ps3_storage_device *dev,
				     struct request *req, int gather)
{
	unsigned int offset = 0;
	struct req_iterator iter;
	struct bio_vec *bvec;
	unsigned int i = 0;
	size_t size;
	void *buf;

	rq_for_each_segment(bvec, req, iter) {
		unsigned long flags;
		dev_dbg(&dev->sbd.core,
			"%s:%u: bio %u: %u segs %u sectors from %lu\n",
			__func__, __LINE__, i, bio_segments(iter.bio),
			bio_sectors(iter.bio), iter.bio->bi_sector);

		size = bvec->bv_len;
		buf = bvec_kmap_irq(bvec, &flags);
		if (gather)
			memcpy(dev->bounce_buf+offset, buf, size);
		else
			memcpy(buf, dev->bounce_buf+offset, size);
		offset += size;
		flush_kernel_dcache_page(bvec->bv_page);
		bvec_kunmap_irq(buf, &flags);
		i++;
	}
}

static int ps3nflash_submit_request_sg(struct ps3_storage_device *dev,
				       struct request *req)
{
	struct ps3nflash_private *priv = ps3_system_bus_get_drvdata(&dev->sbd);
	int write = rq_data_dir(req), res;
	const char *op = write ? "write" : "read";
	u64 start_sector, sectors;
	unsigned int region_idx = MINOR(disk_devt(req->rq_disk)) / PS3NFLASH_MINORS;
	unsigned int region_id = dev->regions[region_idx].id;
	unsigned int region_flags = dev->regions[region_idx].flags;

#ifdef DEBUG
	unsigned int n = 0;
	struct bio_vec *bv;
	struct req_iterator iter;

	rq_for_each_segment(bv, req, iter)
		n++;
	dev_dbg(&dev->sbd.core,
		"%s:%u: %s req has %u bvecs for %u sectors\n",
		__func__, __LINE__, op, n, blk_rq_sectors(req));
#endif

	start_sector = blk_rq_pos(req) * priv->blocking_factor;
	sectors = blk_rq_sectors(req) * priv->blocking_factor;
	dev_dbg(&dev->sbd.core, "%s:%u: %s %llu sectors starting at %llu\n",
		__func__, __LINE__, op, sectors, start_sector);

	if (write) {
		ps3nflash_scatter_gather(dev, req, 1);

		res = lv1_storage_write(dev->sbd.dev_id, region_id,
					start_sector, sectors, region_flags,
					dev->bounce_lpar, &dev->tag);
	} else {
		res = lv1_storage_read(dev->sbd.dev_id, region_id,
				       start_sector, sectors, region_flags,
				       dev->bounce_lpar, &dev->tag);
	}
	if (res) {
		dev_err(&dev->sbd.core, "%s:%u: %s failed %d\n", __func__,
			__LINE__, op, res);
		__blk_end_request_all(req, -EIO);
		return 0;
	}

	priv->req = req;
	return 1;
}

static void ps3nflash_do_request(struct ps3_storage_device *dev,
				 struct request_queue *q)
{
	struct request *req;

	dev_dbg(&dev->sbd.core, "%s:%u\n", __func__, __LINE__);

	while ((req = blk_fetch_request(q))) {
		if (req->cmd_type == REQ_TYPE_FS) {
			if (ps3nflash_submit_request_sg(dev, req))
				break;
		} else {
			blk_dump_rq_flags(req, DEVICE_NAME " bad request");
			__blk_end_request_all(req, -EIO);
			continue;
		}
	}
}

static void ps3nflash_request(struct request_queue *q)
{
	struct ps3_storage_device *dev = q->queuedata;
	struct ps3nflash_private *priv = ps3_system_bus_get_drvdata(&dev->sbd);

	if (priv->req) {
		dev_dbg(&dev->sbd.core, "%s:%u busy\n", __func__, __LINE__);
		return;
	}

	ps3nflash_do_request(dev, q);
}

static irqreturn_t ps3nflash_interrupt(int irq, void *data)
{
	struct ps3_storage_device *dev = data;
	struct ps3nflash_private *priv;
	struct request *req;
	int res, read, error;
	u64 tag, status;
	const char *op;

	res = lv1_storage_get_async_status(dev->sbd.dev_id, &tag, &status);

	if (tag != dev->tag)
		dev_err(&dev->sbd.core,
			"%s:%u: tag mismatch, got %llx, expected %llx\n",
			__func__, __LINE__, tag, dev->tag);

	if (res) {
		dev_err(&dev->sbd.core, "%s:%u: res=%d status=0x%llx\n",
			__func__, __LINE__, res, status);
		return IRQ_HANDLED;
	}

	priv = ps3_system_bus_get_drvdata(&dev->sbd);
	req = priv->req;
	if (!req) {
		dev_dbg(&dev->sbd.core,
			"%s:%u non-block layer request completed\n", __func__,
			__LINE__);
		dev->lv1_status = status;
		complete(&dev->done);
		return IRQ_HANDLED;
	}

	read = !rq_data_dir(req);
	op = read ? "read" : "write";

	if (status) {
		dev_dbg(&dev->sbd.core, "%s:%u: %s failed 0x%llx\n", __func__,
			__LINE__, op, status);
		error = -EIO;
	} else {
		dev_dbg(&dev->sbd.core, "%s:%u: %s completed\n", __func__,
			__LINE__, op);
		error = 0;
		if (read)
			ps3nflash_scatter_gather(dev, req, 0);
	}

	spin_lock(&priv->lock);
	__blk_end_request_all(req, error);
	priv->req = NULL;
	ps3nflash_do_request(dev, priv->queue);
	spin_unlock(&priv->lock);

	return IRQ_HANDLED;
}

static int __devinit ps3nflash_probe(struct ps3_system_bus_device *_dev)
{
	struct ps3_storage_device *dev = to_ps3_storage_device(&_dev->core);
	struct ps3nflash_private *priv;
	int error;
	unsigned int regidx, devidx;
	struct request_queue *queue;
	struct gendisk *gendisk;

	if (dev->blk_size < 512) {
		dev_err(&dev->sbd.core,
			"%s:%u: cannot handle block size %llu\n", __func__,
			__LINE__, dev->blk_size);
		return -EINVAL;
	}

	priv = kzalloc(sizeof(*priv) + dev->num_regions * sizeof(gendisk), GFP_KERNEL);
	if (!priv) {
		error = -ENOMEM;
		goto fail;
	}

	ps3_system_bus_set_drvdata(_dev, priv);
	spin_lock_init(&priv->lock);

	dev->bounce_size = BOUNCE_SIZE;
	dev->bounce_buf = kmalloc(BOUNCE_SIZE, GFP_DMA);
	if (!dev->bounce_buf) {
		error = -ENOMEM;
		goto fail_free_priv;
	}

	for (regidx = 0; regidx < dev->num_regions; regidx++)
		dev->regions[regidx].flags = region_flags[regidx];

	error = ps3stor_setup(dev, ps3nflash_interrupt);
	if (error)
		goto fail_free_bounce;

	priv->raw_capacity = dev->regions[0].size;

	queue = blk_init_queue(ps3nflash_request, &priv->lock);
	if (!queue) {
		dev_err(&dev->sbd.core, "%s:%u: blk_init_queue failed\n",
			__func__, __LINE__);
		error = -ENOMEM;
		goto fail_teardown;
	}

	priv->queue = queue;
	queue->queuedata = dev;

	blk_queue_bounce_limit(queue, BLK_BOUNCE_HIGH);

	blk_queue_max_hw_sectors(queue, dev->bounce_size >> 9);
	blk_queue_segment_boundary(queue, -1UL);
	blk_queue_dma_alignment(queue, dev->blk_size-1);
	blk_queue_logical_block_size(queue, dev->blk_size);

	blk_queue_flush(queue, REQ_FLUSH);

	blk_queue_max_segments(queue, -1);
	blk_queue_max_segment_size(queue, dev->bounce_size);

	for (devidx = 0; devidx < dev->num_regions; devidx++)
	{
		if (test_bit(devidx, &dev->accessible_regions) == 0)
			continue;

		gendisk = alloc_disk(PS3NFLASH_MINORS);
		if (!gendisk) {
			dev_err(&dev->sbd.core, "%s:%u: alloc_disk failed\n", __func__,
				__LINE__);
			error = -ENOMEM;
			goto fail_cleanup_queue;
		}

		priv->gendisk[devidx] = gendisk;
		gendisk->major = ps3nflash_major;
		gendisk->first_minor = devidx * PS3NFLASH_MINORS;
		gendisk->fops = &ps3nflash_fops;
		gendisk->queue = queue;
		gendisk->private_data = dev;
		gendisk->driverfs_dev = &dev->sbd.core;
		snprintf(gendisk->disk_name, sizeof(gendisk->disk_name), PS3NFLASH_NAME,
			 devidx+'a');
		priv->blocking_factor = dev->blk_size >> 9;
		set_capacity(gendisk,
		   	 dev->regions[devidx].size*priv->blocking_factor);

		dev_info(&dev->sbd.core,
			 "%s (%llu MiB total, %lu MiB region)\n",
			 gendisk->disk_name, priv->raw_capacity >> 11,
			 get_capacity(gendisk) >> 11);

		add_disk(gendisk);
	}

	return 0;

fail_cleanup_queue:
	blk_cleanup_queue(queue);
fail_teardown:
	ps3stor_teardown(dev);
fail_free_bounce:
	kfree(dev->bounce_buf);
fail_free_priv:
	kfree(priv);
	ps3_system_bus_set_drvdata(_dev, NULL);
fail:
	return error;
}

static int ps3nflash_remove(struct ps3_system_bus_device *_dev)
{
	struct ps3_storage_device *dev = to_ps3_storage_device(&_dev->core);
	struct ps3nflash_private *priv = ps3_system_bus_get_drvdata(&dev->sbd);
	unsigned int devidx;

	for (devidx = 0; devidx < dev->num_regions; devidx++)
	{
		if (test_bit(devidx, &dev->accessible_regions) == 0)
			continue;

		del_gendisk(priv->gendisk[devidx]);
	}

	blk_cleanup_queue(priv->queue);

	for (devidx = 0; devidx < dev->num_regions; devidx++)
	{
		if (test_bit(devidx, &dev->accessible_regions) == 0)
			continue;

		put_disk(priv->gendisk[devidx]);
	}

	ps3stor_teardown(dev);
	kfree(dev->bounce_buf);
	kfree(priv);
	ps3_system_bus_set_drvdata(_dev, NULL);
	return 0;
}

static struct ps3_system_bus_driver ps3nflash = {
	.match_id	= PS3_MATCH_ID_STOR_FLASH,
	.core.name	= DEVICE_NAME,
	.core.owner	= THIS_MODULE,
	.probe		= ps3nflash_probe,
	.remove		= ps3nflash_remove,
	.shutdown	= ps3nflash_remove,
};


static int __init ps3nflash_init(void)
{
	int error;

	if (!firmware_has_feature(FW_FEATURE_PS3_LV1))
		return -ENODEV;

	error = register_blkdev(0, DEVICE_NAME);
	if (error <= 0) {
		printk(KERN_ERR "%s:%u: register_blkdev failed %d\n", __func__,
		       __LINE__, error);
		return error;
	}
	ps3nflash_major = error;

	pr_info("%s:%u: registered block device major %d\n", __func__,
		__LINE__, ps3nflash_major);

	error = ps3_system_bus_driver_register(&ps3nflash);
	if (error)
		unregister_blkdev(ps3nflash_major, DEVICE_NAME);

	return error;
}

static void __exit ps3nflash_exit(void)
{
	ps3_system_bus_driver_unregister(&ps3nflash);
	unregister_blkdev(ps3nflash_major, DEVICE_NAME);
}

module_init(ps3nflash_init);
module_exit(ps3nflash_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("PS3 NOR FLASH Storage Driver");
MODULE_AUTHOR("Graf Chokolo");
MODULE_ALIAS(PS3_MODULE_ALIAS_STOR_FLASH);
