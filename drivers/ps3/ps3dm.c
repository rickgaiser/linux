
/*
 *  PS3 Dispatcher Manager.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/slab.h>

#include <asm/firmware.h>
#include <asm/lv1call.h>
#include <asm/ps3.h>
#include <asm/ps3dm.h>

#include "vuart.h"

static struct ps3dm {
	struct mutex mutex;
	struct ps3_system_bus_device *dev;
} *ps3dm;

static int timeout = 5000;	/* in msec ( 5 sec ) */
module_param(timeout, int, 0644);

static int ps3dm_vuart_write(struct ps3_system_bus_device *dev,
	const void *buf, unsigned int size)
{
	int error;

	dev_dbg(&dev->core, " -> %s:%d\n", __func__, __LINE__);

	error = ps3_vuart_write(dev, buf, size);

	dev_dbg(&dev->core, " <- %s:%d\n", __func__, __LINE__);

	return error ? error : size;
}

#define POLLING_INTERVAL  25	/* in msec */

static int ps3dm_vuart_read(struct ps3_system_bus_device *dev,
	void *buf, unsigned int size, int timeout)
{
	int error;
	int loopcnt = 0;

	dev_dbg(&dev->core, " -> %s:%d\n", __func__, __LINE__);

	timeout = (timeout + POLLING_INTERVAL - 1) / POLLING_INTERVAL;

	while (loopcnt++ <= timeout) {
		error = ps3_vuart_read(dev, buf, size);
		if (!error)
			return size;

		if (error != -EAGAIN) {
			printk(KERN_ERR "%s: ps3_vuart_read failed %d\n",
			       __func__, error);
			return error;
		}

		msleep(POLLING_INTERVAL);
	}

	return -EWOULDBLOCK;
}

int ps3dm_write(const void *buf, unsigned int size)
{
	int error;

	BUG_ON(!ps3dm);

	mutex_lock(&ps3dm->mutex);

	error = ps3dm_vuart_write(ps3dm->dev, buf, size);

	mutex_unlock(&ps3dm->mutex);

	return error ? error : size;
}

EXPORT_SYMBOL_GPL(ps3dm_write);

int ps3dm_read(void *buf, unsigned int size)
{
	int error;

	BUG_ON(!ps3dm);

	mutex_lock(&ps3dm->mutex);

	error = ps3dm_vuart_read(ps3dm->dev, buf, size, timeout);

	mutex_unlock(&ps3dm->mutex);

	return error ? error : size;
}

EXPORT_SYMBOL_GPL(ps3dm_read);

int ps3dm_do_request(struct ps3dm_hdr *sendbuf, unsigned int sendbuf_size,
	struct ps3dm_hdr *recvbuf, unsigned int recvbuf_size)
{
	int res;

	BUG_ON(!ps3dm);
	BUG_ON(sendbuf_size < PS3DM_HDR_SIZE);
	BUG_ON(recvbuf_size < PS3DM_HDR_SIZE);

	mutex_lock(&ps3dm->mutex);

	res = ps3dm_vuart_write(ps3dm->dev, sendbuf, sendbuf_size);
	if (res != sendbuf_size) {
		printk(KERN_ERR
		       "%s: ps3dm_vuart_write() failed (result=%d)\n",
			__func__, res);
		goto err;
	}

	res = ps3dm_vuart_read(ps3dm->dev, recvbuf, PS3DM_HDR_SIZE, timeout);
	if (res != PS3DM_HDR_SIZE) {
		printk(KERN_ERR
		       "%s: ps3dm_vuart_read() failed (result=%d)\n",
			__func__, res);
		goto err;
	}

	res = ps3dm_vuart_read(ps3dm->dev, recvbuf + 1, recvbuf->response_size, timeout);
	if (res < 0) {
		printk(KERN_ERR
		       "%s: ps3dm_vuart_read() failed (result=%d)\n",
			__func__, res);
		goto err;
	}

	mutex_unlock(&ps3dm->mutex);

	return 0;

err:

	mutex_unlock(&ps3dm->mutex);

	return res;
}

EXPORT_SYMBOL_GPL(ps3dm_do_request);

static int __devinit ps3dm_probe(struct ps3_system_bus_device *dev)
{
	dev_dbg(&dev->core, "%s:%d\n", __func__, __LINE__);

	if (ps3dm) {
		dev_err(&dev->core, "Only one ps3dm device is supported\n");
		return -EBUSY;
	}

	ps3dm = kzalloc(sizeof(*ps3dm), GFP_KERNEL);
	if (!ps3dm)
		return -ENOMEM;

	mutex_init(&ps3dm->mutex);
	ps3dm->dev = dev;

	return 0;
}

static int ps3dm_remove(struct ps3_system_bus_device *dev)
{
	dev_dbg(&dev->core, "%s:%d\n", __func__, __LINE__);

	if (ps3dm) {
		kfree(ps3dm);
		ps3dm = NULL;
	}

	dev_dbg(&dev->core, " <- %s:%d\n", __func__, __LINE__);

	return 0;
}

static void ps3dm_shutdown(struct ps3_system_bus_device *dev)
{
	dev_dbg(&dev->core, "%s:%d\n", __func__, __LINE__);
	ps3dm_remove(dev);
	dev_dbg(&dev->core, "%s:%d\n", __func__, __LINE__);
}

static struct ps3_vuart_port_driver ps3dm_driver = {
	.core.match_id = PS3_MATCH_ID_DISPATCHER_MANAGER,
	.core.core.name = "ps3dm",
	.probe = ps3dm_probe,
	.remove = ps3dm_remove,
	.shutdown = ps3dm_shutdown,
};

static int __init ps3dm_module_init(void)
{
	int error;

	if (!firmware_has_feature(FW_FEATURE_PS3_LV1))
		return -ENODEV;

	pr_debug(" <- %s:%d\n", __func__, __LINE__);

	error = ps3_vuart_port_driver_register(&ps3dm_driver);
	if (error) {
		printk(KERN_ERR
		       "%s: ps3_vuart_port_driver_register failed %d\n",
		       __func__, error);
		return error;
	}

	pr_debug(" <- %s:%d\n", __func__, __LINE__);

	return error;
}

static void __exit ps3dm_module_exit(void)
{
	pr_debug(" -> %s:%d\n", __func__, __LINE__);
	ps3_vuart_port_driver_unregister(&ps3dm_driver);
	pr_debug(" <- %s:%d\n", __func__, __LINE__);
}

subsys_initcall(ps3dm_module_init);
module_exit(ps3dm_module_exit);

MODULE_AUTHOR("Graf Chokolo");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PS3 Dispatcher Manager");
MODULE_ALIAS(PS3_MODULE_ALIAS_DISPATCHER_MANAGER);
