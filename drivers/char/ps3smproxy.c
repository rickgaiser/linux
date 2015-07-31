
/*
 * PS3 System Manager Proxy Driver
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
#include <linux/mm.h>
#include <linux/pagemap.h>

#include <asm/lv1call.h>
#include <asm/ps3.h>
#include <asm/ps3smproxy.h>

#define DEVICE_NAME		"ps3smproxy"

static long ps3smproxy_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *) arg;
	int res;

	pr_debug("%s:%d cmd %x\n", __func__, __LINE__, cmd);

	switch (cmd) {
	case PS3SMPROXY_IOCTL_DO_REQUEST:
		{
			struct ps3smproxy_ioctl_do_request do_request;
			void *sendbuf, *recvbuf = NULL;

			if (copy_from_user(&do_request, argp, sizeof(do_request))) {
				pr_debug("%s:%d: copy_from_user failed\n", __func__, __LINE__);
				return -EFAULT;
			}

			WARN_ON(do_request.sendbuf_size > 4096);
			WARN_ON(do_request.recvbuf_size > 4096);

			pr_debug("%s:%d: sendbuf_size (%lld) recvbuf_size (%lld)\n",
				__func__, __LINE__, do_request.sendbuf_size, do_request.recvbuf_size);

			sendbuf = kmalloc(do_request.sendbuf_size, GFP_KERNEL);
			if (!sendbuf) {
				pr_debug("%s:%d: kmalloc failed\n", __func__, __LINE__);
				return -ENOMEM;
			}

			if (copy_from_user(sendbuf, (const void __user *) do_request.sendbuf,
				do_request.sendbuf_size)) {
				pr_debug("%s:%d: copy_from_user failed\n", __func__, __LINE__);
				res = -EFAULT;
				goto kfree_sendbuf;
			}

			if (do_request.recvbuf_size) {
				recvbuf = kmalloc(do_request.recvbuf_size, GFP_KERNEL);
				if (!recvbuf) {
					pr_debug("%s:%d: kmalloc failed\n", __func__, __LINE__);
					res = -ENOMEM;
					goto kfree_sendbuf;
				}
			}

			res = ps3_sys_manager_do_request(sendbuf, do_request.sendbuf_size,
				recvbuf, do_request.recvbuf_size);
			if (res) {
				pr_debug("%s:%d: ps3_sys_manager_do_request failed (%d)\n", __func__, __LINE__, res);
				goto kfree_recvbuf;
			}

			if (do_request.recvbuf_size) {
				if (copy_to_user((void __user *) do_request.recvbuf, recvbuf,
					do_request.recvbuf_size)) {
					pr_debug("%s:%d: copy_to_user failed\n", __func__, __LINE__);
					res = -EFAULT;
					goto kfree_recvbuf;
				}
			}

			res = 0;

		kfree_recvbuf:

			if (recvbuf)
				kfree(recvbuf);

		kfree_sendbuf:

			kfree(sendbuf);

			return res;
		}
	}

	return -EFAULT;
}

static const struct file_operations ps3smproxy_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= ps3smproxy_ioctl,
	.compat_ioctl	= ps3smproxy_ioctl,
};

static struct miscdevice ps3smproxy_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= DEVICE_NAME,
	.fops	= &ps3smproxy_fops,
};

static int __init ps3smproxy_init(void)
{
	int res;

	res = misc_register(&ps3smproxy_misc);
	if (res) {
		pr_debug("%s:%u: misc_register failed %d\n",
			 __func__, __LINE__, res);
		return res;
	}

	pr_debug("%s:%u: registered misc device %d\n",
		 __func__, __LINE__, ps3smproxy_misc.minor);

	return 0;
}

static void __exit ps3smproxy_exit(void)
{
	misc_deregister(&ps3smproxy_misc);
}

module_init(ps3smproxy_init);
module_exit(ps3smproxy_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PS3 System Manager Proxy Driver");
MODULE_AUTHOR("glevand");
