
/*
 * PS3 ENCDEC Storage Driver
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

#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/uaccess.h>

#include <asm/lv1call.h>
#include <asm/ps3stor.h>
#include <asm/ps3encdec.h>

#define DEVICE_NAME		"ps3encdec"

#define BOUNCE_SIZE		(64 * 1024)

struct ps3encdec_private {
	struct mutex mutex;
};

static struct ps3_storage_device *ps3encdec_dev;

static long ps3encdec_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct ps3_storage_device *dev = ps3encdec_dev;
	struct ps3encdec_private *priv = ps3_system_bus_get_drvdata(&dev->sbd);
	void __user *argp = (void __user *) arg;
	int res = -EFAULT;

	mutex_lock(&priv->mutex);

	pr_debug("%s:%d cmd %x\n", __func__, __LINE__, cmd);

	switch (cmd) {
	case PS3ENCDEC_IOCTL_DO_REQUEST:
		{
			struct ps3encdec_ioctl_do_request do_request;
			void *cmdbuf;
			u64 cmdbuf_lpar;

			if (copy_from_user(&do_request, argp, sizeof(do_request))) {
				pr_debug("%s:%d: copy_from_user failed\n", __func__, __LINE__);
				res = -EFAULT;
				break;
			}

			BUG_ON(do_request.respbuf_size > BOUNCE_SIZE);

			pr_debug("%s:%d: cmd (%llx) cmdbuf_size (%lld) respbuf_size (%lld)\n",
				__func__, __LINE__, do_request.cmd, do_request.cmdbuf_size,
				do_request.respbuf_size);

			cmdbuf = kmalloc(do_request.cmdbuf_size, GFP_KERNEL);
			if (!cmdbuf) {
				pr_debug("%s:%d: kmalloc failed\n", __func__, __LINE__);
				res = -ENOMEM;
				break;
			}

			if (copy_from_user(cmdbuf, (const void __user *) do_request.cmdbuf,
				do_request.cmdbuf_size)) {
				pr_debug("%s:%d: copy_from_user failed\n", __func__, __LINE__);
				res = -EFAULT;
				goto kfree_cmdbuf;
			}

			cmdbuf_lpar = ps3_mm_phys_to_lpar(__pa(cmdbuf));

			res = ps3stor_send_command(dev, do_request.cmd,
				cmdbuf_lpar, do_request.cmdbuf_size,
				dev->bounce_lpar, do_request.respbuf_size);
			if (res) {
				pr_debug("%s:%d: ps3stor_send_command failed (%d)\n",
					__func__, __LINE__, res);
				res = -EFAULT;
				goto kfree_cmdbuf;
			}

			if (copy_to_user((void __user *) do_request.respbuf, dev->bounce_buf,
				do_request.respbuf_size)) {
				pr_debug("%s:%d: copy_to_user failed\n", __func__, __LINE__);
				res = -EFAULT;
				goto kfree_cmdbuf;
			}

			res = 0;

		kfree_cmdbuf:

			kfree(cmdbuf);

			break;
		}
	}

	mutex_unlock(&priv->mutex);

	return res;
}

static irqreturn_t ps3encdec_interrupt(int irq, void *data)
{
	struct ps3_storage_device *dev = data;
	int res;
	u64 tag, status;

	res = lv1_storage_get_async_status(dev->sbd.dev_id, &tag, &status);

	if (tag != dev->tag)
		dev_err(&dev->sbd.core,
			"%s:%u: tag mismatch, got %llx, expected %llx\n",
			__func__, __LINE__, tag, dev->tag);

	if (res) {
		dev_err(&dev->sbd.core, "%s:%u: res=%d status=0x%llx\n",
			__func__, __LINE__, res, status);
	} else {
		dev->lv1_status = status;
		complete(&dev->done);
	}

	return IRQ_HANDLED;
}

static const struct file_operations ps3encdec_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= ps3encdec_ioctl,
	.compat_ioctl	= ps3encdec_ioctl,
};

static struct miscdevice ps3encdec_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= DEVICE_NAME,
	.fops	= &ps3encdec_fops,
};

static int __devinit ps3encdec_probe(struct ps3_system_bus_device *_dev)
{
	struct ps3_storage_device *dev = to_ps3_storage_device(&_dev->core);
	struct ps3encdec_private *priv;
	int error;

	if (ps3encdec_dev) {
		dev_err(&dev->sbd.core,
			"Only one ENCDEC device is supported\n");
		return -EBUSY;
	}

	ps3encdec_dev = dev;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		error = -ENOMEM;
		goto fail;
	}

	ps3_system_bus_set_drvdata(&dev->sbd, priv);
	mutex_init(&priv->mutex);

	dev->bounce_size = BOUNCE_SIZE;
	dev->bounce_buf = kmalloc(BOUNCE_SIZE, GFP_DMA);
	if (!dev->bounce_buf) {
		error = -ENOMEM;
		goto fail_free_priv;
	}

	error = ps3stor_setup(dev, ps3encdec_interrupt);
	if (error)
		goto fail_free_bounce;

	ps3encdec_misc.parent = &dev->sbd.core;

	error = misc_register(&ps3encdec_misc);
	if (error) {
		dev_err(&dev->sbd.core, "%s:%u: misc_register failed %d\n",
			__func__, __LINE__, error);
		goto fail_teardown;
	}

	dev_info(&dev->sbd.core, "%s:%u: registered misc device %d\n",
		 __func__, __LINE__, ps3encdec_misc.minor);

	return 0;

fail_teardown:

	ps3stor_teardown(dev);

fail_free_bounce:

	kfree(dev->bounce_buf);

fail_free_priv:

	kfree(priv);
	ps3_system_bus_set_drvdata(&dev->sbd, NULL);

fail:

	ps3encdec_dev = NULL;

	return error;
}

static int ps3encdec_remove(struct ps3_system_bus_device *_dev)
{
	struct ps3_storage_device *dev = to_ps3_storage_device(&_dev->core);

	misc_deregister(&ps3encdec_misc);
	ps3stor_teardown(dev);
	kfree(ps3_system_bus_get_drvdata(&dev->sbd));
	ps3_system_bus_set_drvdata(&dev->sbd, NULL);
	kfree(dev->bounce_buf);
	ps3encdec_dev = NULL;

	return 0;
}

static struct ps3_system_bus_driver ps3encdec = {
	.match_id	= PS3_MATCH_ID_STOR_ENCDEC,
	.core.name	= DEVICE_NAME,
	.core.owner	= THIS_MODULE,
	.probe		= ps3encdec_probe,
	.remove		= ps3encdec_remove,
	.shutdown	= ps3encdec_remove,
};


static int __init ps3encdec_init(void)
{
	return ps3_system_bus_driver_register(&ps3encdec);
}

static void __exit ps3encdec_exit(void)
{
	ps3_system_bus_driver_unregister(&ps3encdec);
}

module_init(ps3encdec_init);
module_exit(ps3encdec_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PS3 ENCDEC Storage Driver");
MODULE_AUTHOR("Graf Chokolo");
MODULE_ALIAS(PS3_MODULE_ALIAS_STOR_ENCDEC);
