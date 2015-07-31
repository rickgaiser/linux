
/*
 * PS3 Storage Manager Driver
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
#include <asm/ps3.h>
#include <asm/ps3stormgr.h>

#define DEVICE_NAME		"ps3stormgr"

static long ps3stormgr_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *) arg;
	int res;

	pr_debug("%s:%d cmd %x\n", __func__, __LINE__, cmd);

	switch (cmd) {
	case PS3STORMGR_IOCTL_CREATE_REGION:
		{
			struct ps3stormgr_ioctl_create_region create_region;
			u64 tag;

			if (copy_from_user(&create_region, argp, sizeof(create_region))) {
				pr_debug("%s:%d: copy_from_user failed\n", __func__, __LINE__);
				return -EFAULT;
			}

			res = lv1_undocumented_function_250(create_region.dev_id, create_region.start_sector,
				create_region.sector_count, 0, create_region.laid,
				&create_region.region_id, &tag);
			if (res) {
				pr_debug("%s:%d: lv1_undocumented_function_250 failed (%d)\n",
					__func__, __LINE__, res);
				return res;
			}

			if (copy_to_user(argp, &create_region, sizeof(create_region))) {
				pr_debug("%s:%d: copy_to_user failed\n", __func__, __LINE__);
				return -EFAULT;
			}

			return 0;
		}

	case PS3STORMGR_IOCTL_DELETE_REGION:
		{
			struct ps3stormgr_ioctl_delete_region delete_region;
			u64 tag;

			if (copy_from_user(&delete_region, argp, sizeof(delete_region))) {
				pr_debug("%s:%d: copy_from_user failed\n", __func__, __LINE__);
				return -EFAULT;
			}

			res = lv1_undocumented_function_251(delete_region.dev_id, delete_region.region_id,
				&tag);
			if (res) {
				pr_debug("%s:%d: lv1_undocumented_function_251 failed (%d)\n",
					__func__, __LINE__, res);
				return res;
			}

			return 0;
		}

	case PS3STORMGR_IOCTL_SET_REGION_ACL:
		{
			struct ps3stormgr_ioctl_set_region_acl set_region_acl;
			u64 tag;

			if (copy_from_user(&set_region_acl, argp, sizeof(set_region_acl))) {
				pr_debug("%s:%d: copy_from_user failed\n", __func__, __LINE__);
				return -EFAULT;
			}

			res = lv1_undocumented_function_252(set_region_acl.dev_id, set_region_acl.region_id,
				set_region_acl.laid, set_region_acl.access_rights, &tag);
			if (res) {
				pr_debug("%s:%d: lv1_undocumented_function_252 failed (%d)\n",
					__func__, __LINE__, res);
				return res;
			}

			return 0;
		}

	case PS3STORMGR_IOCTL_GET_REGION_ACL:
		{
			struct ps3stormgr_ioctl_get_region_acl get_region_acl;

			if (copy_from_user(&get_region_acl, argp, sizeof(get_region_acl))) {
				pr_debug("%s:%d: copy_from_user failed\n", __func__, __LINE__);
				return -EFAULT;
			}

			res = lv1_undocumented_function_253(get_region_acl.dev_id, get_region_acl.region_id,
				get_region_acl.entry_index, &get_region_acl.laid, &get_region_acl.access_rights);
			if (res) {
				pr_debug("%s:%d: lv1_undocumented_function_253 failed (%d)\n",
					__func__, __LINE__, res);
				return res;
			}

			if (copy_to_user(argp, &get_region_acl, sizeof(get_region_acl))) {
				pr_debug("%s:%d: copy_to_user failed\n", __func__, __LINE__);
				return -EFAULT;
			}

			return 0;
		}
	}

	return -EFAULT;
}

static const struct file_operations ps3stormgr_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= ps3stormgr_ioctl,
	.compat_ioctl	= ps3stormgr_ioctl,
};

static struct miscdevice ps3stormgr_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= DEVICE_NAME,
	.fops	= &ps3stormgr_fops,
};

static int __init ps3stormgr_init(void)
{
	int res;

	res = misc_register(&ps3stormgr_misc);
	if (res) {
		pr_debug("%s:%u: misc_register failed %d\n",
			 __func__, __LINE__, res);
		return res;
	}

	pr_debug("%s:%u: registered misc device %d\n",
		 __func__, __LINE__, ps3stormgr_misc.minor);

	return 0;
}

static void __exit ps3stormgr_exit(void)
{
	misc_deregister(&ps3stormgr_misc);
}

module_init(ps3stormgr_init);
module_exit(ps3stormgr_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PS3 Storage Manager Driver");
MODULE_AUTHOR("Graf Chokolo");
