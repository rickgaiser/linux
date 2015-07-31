
/*
 * PS3 HVC Driver
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
#include <asm/ps3hvc.h>

#define DEVICE_NAME		"ps3hvc"

static long ps3hvc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *) arg;
	int res = -EFAULT;

	pr_debug("%s:%d cmd %x\n", __func__, __LINE__, cmd);

	switch (cmd) {
	case PS3HVC_IOCTL_HVCALL:
		{
			struct ps3hvc_ioctl_hvcall *hvcall;

			hvcall = kmalloc(sizeof(struct ps3hvc_ioctl_hvcall) +  16 * sizeof(u64), GFP_KERNEL);
			if (!hvcall) {
				pr_debug("%s:%d: kmalloc failed\n", __func__, __LINE__);
				res = -ENOMEM;
				break;
			}

			if (copy_from_user(hvcall, argp, sizeof(struct ps3hvc_ioctl_hvcall))) {
				pr_debug("%s:%d: copy_from_user failed\n", __func__, __LINE__);
				res = -EFAULT;
				goto kfree_arg;
			}

			if ((hvcall->in_args + hvcall->out_args) > 16) {
				res = -EFAULT;
				goto kfree_arg;
			}

			if (copy_from_user(hvcall, argp, sizeof(struct ps3hvc_ioctl_hvcall) +
				(hvcall->in_args + hvcall->out_args) * sizeof(u64))) {
				pr_debug("%s:%d: copy_from_user failed\n", __func__, __LINE__);
				res = -EFAULT;
				goto kfree_arg;
			}

			switch (hvcall->number) {
			/* lv1_undocumented_function_8 */
			case PS3HVC_HVCALL_UNDOCUMENTED_FUNCTION_8:
				{
					if ((hvcall->in_args != 0) && (hvcall->out_args != 1)) {
						res = -EFAULT;
						goto kfree_arg;
					}

					hvcall->result = lv1_undocumented_function_8(&hvcall->args[0]);

					break;
				}

			/* lv1_connect_irq_plug_ext */
			case PS3HVC_HVCALL_CONNECT_IRQ_PLUG_EXT:
				{
					if ((hvcall->in_args != 5) && (hvcall->out_args != 0)) {
						res = -EFAULT;
						goto kfree_arg;
					}

					hvcall->result = lv1_connect_irq_plug_ext(hvcall->args[0],
						hvcall->args[1], hvcall->args[2], hvcall->args[3],
						hvcall->args[4]);

					break;
				}

			/* lv1_disconnect_irq_plug_ext */
			case PS3HVC_HVCALL_DISCONNECT_IRQ_PLUG_EXT:
				{
					if ((hvcall->in_args != 3) && (hvcall->out_args != 0)) {
						res = -EFAULT;
						goto kfree_arg;
					}

					hvcall->result = lv1_disconnect_irq_plug_ext(hvcall->args[0],
						hvcall->args[1], hvcall->args[2]);

					break;
				}

			/* lv1_construct_event_receive_port */
			case PS3HVC_HVCALL_CONSTRUCT_EVENT_RECEIVE_PORT:
				{
					if ((hvcall->in_args != 0) && (hvcall->out_args != 1)) {
						res = -EFAULT;
						goto kfree_arg;
					}

					hvcall->result = lv1_construct_event_receive_port(&hvcall->args[0]);

					break;
				}

			/* lv1_destruct_event_receive_port */
			case PS3HVC_HVCALL_DESTRUCT_EVENT_RECEIVE_PORT:
				{
					if ((hvcall->in_args != 1) && (hvcall->out_args != 0)) {
						res = -EFAULT;
						goto kfree_arg;
					}

					hvcall->result = lv1_destruct_event_receive_port(hvcall->args[0]);

					break;
				}

			/* lv1_shutdown_logical_partition */
			case PS3HVC_HVCALL_SHUTDOWN_LOGICAL_PARTITION:
				{
					if ((hvcall->in_args != 1) && (hvcall->out_args != 0)) {
						res = -EFAULT;
						goto kfree_arg;
					}

					hvcall->result = lv1_shutdown_logical_partition(hvcall->args[0]);

					break;
				}

			/* lv1_get_logical_partition_id */
			case PS3HVC_HVCALL_GET_LPAR_ID:
				{
					if ((hvcall->in_args != 0) && (hvcall->out_args != 1)) {
						res = -EFAULT;
						goto kfree_arg;
					}

					hvcall->result = lv1_get_logical_partition_id(&hvcall->args[0]);

					break;
				}

			/* lv1_create_repository_node */
			case PS3HVC_HVCALL_CREATE_REPO_NODE:
				{
					if ((hvcall->in_args != 7) && (hvcall->out_args != 0)) {
						res = -EFAULT;
						goto kfree_arg;
					}

					hvcall->result = lv1_create_repository_node(hvcall->args[0],
						hvcall->args[1], hvcall->args[2], hvcall->args[3],
						hvcall->args[4], hvcall->args[5], hvcall->args[6]);

					break;
				}

			/* lv1_get_repository_node_value */
			case PS3HVC_HVCALL_GET_REPO_NODE_VAL:
				{
					if ((hvcall->in_args != 5) && (hvcall->out_args != 2)) {
						res = -EFAULT;
						goto kfree_arg;
					}

					hvcall->result = lv1_get_repository_node_value(hvcall->args[0],
						hvcall->args[1], hvcall->args[2], hvcall->args[3],
						hvcall->args[4], &hvcall->args[5], &hvcall->args[6]);

					break;
				}

			/* lv1_modify_repository_node_value */
			case PS3HVC_HVCALL_MODIFY_REPO_NODE_VAL:
				{
					if ((hvcall->in_args != 7) && (hvcall->out_args != 0)) {
						res = -EFAULT;
						goto kfree_arg;
					}

					hvcall->result = lv1_modify_repository_node_value(hvcall->args[0],
						hvcall->args[1], hvcall->args[2], hvcall->args[3],
						hvcall->args[4], hvcall->args[5], hvcall->args[6]);

					break;
				}

			/* lv1_remove_repository_node */
			case PS3HVC_HVCALL_RM_REPO_NODE:
				{
					if ((hvcall->in_args != 5) && (hvcall->out_args != 0)) {
						res = -EFAULT;
						goto kfree_arg;
					}

					hvcall->result = lv1_remove_repository_node(hvcall->args[0],
						hvcall->args[1], hvcall->args[2], hvcall->args[3], hvcall->args[4]);

					break;
				}

			/* lv1_set_dabr */
			case PS3HVC_HVCALL_SET_DABR:
				{
					if ((hvcall->in_args != 2) && (hvcall->out_args != 0)) {
						res = -EFAULT;
						goto kfree_arg;
					}

					hvcall->result = lv1_set_dabr(hvcall->args[0], hvcall->args[1]);

					break;
				}

			/* lv1_undocumented_function_102 */
			case PS3HVC_HVCALL_UNDOCUMENTED_FUNCTION_102:
				{
					if ((hvcall->in_args != 0) && (hvcall->out_args != 1)) {
						res = -EFAULT;
						goto kfree_arg;
					}

					hvcall->result = lv1_undocumented_function_102(&hvcall->args[0]);

					break;
				}

			/* lv1_undocumented_function_107 */
			case PS3HVC_HVCALL_UNDOCUMENTED_FUNCTION_107:
				{
					if ((hvcall->in_args != 6) && (hvcall->out_args != 1)) {
						res = -EFAULT;
						goto kfree_arg;
					}

					hvcall->result = lv1_undocumented_function_107(hvcall->args[0], hvcall->args[1],
						hvcall->args[2], hvcall->args[3], hvcall->args[4], hvcall->args[5],
						&hvcall->args[6]);

					break;
				}

			/* lv1_undocumented_function_109 */
			case PS3HVC_HVCALL_UNDOCUMENTED_FUNCTION_109:
				{
					if ((hvcall->in_args != 1) && (hvcall->out_args != 0)) {
						res = -EFAULT;
						goto kfree_arg;
					}

					hvcall->result = lv1_undocumented_function_109(hvcall->args[0]);

					break;
				}

			/* lv1_get_version_info */
			case PS3HVC_HVCALL_GET_VERSION_INFO:
				{
					if ((hvcall->in_args != 0) && (hvcall->out_args != 1)) {
						res = -EFAULT;
						goto kfree_arg;
					}

					hvcall->result = lv1_get_version_info(&hvcall->args[0]);

					break;
				}

			/* lv1_undocumented_function_182 */
			case PS3HVC_HVCALL_UNDOCUMENTED_FUNCTION_182:
				{
					if ((hvcall->in_args != 1) && (hvcall->out_args != 1)) {
						res = -EFAULT;
						goto kfree_arg;
					}

					hvcall->result = lv1_undocumented_function_182(hvcall->args[0], &hvcall->args[1]);

					break;
				}

			/* lv1_undocumented_function_183 */
			case PS3HVC_HVCALL_UNDOCUMENTED_FUNCTION_183:
				{
					if ((hvcall->in_args != 2) && (hvcall->out_args != 0)) {
						res = -EFAULT;
						goto kfree_arg;
					}

					hvcall->result = lv1_undocumented_function_183(hvcall->args[0], hvcall->args[1]);

					break;
				}

			/* lv1_net_control */
			case PS3HVC_HVCALL_NET_CONTROL:
				{
					if ((hvcall->in_args != 6) && (hvcall->out_args != 2)) {
						res = -EFAULT;
						goto kfree_arg;
					}

					hvcall->result = lv1_net_control(hvcall->args[0], hvcall->args[1], hvcall->args[2],
						hvcall->args[3], hvcall->args[4], hvcall->args[5],
						&hvcall->args[6], &hvcall->args[7]);

					break;
				}

			/* lv1_undocumented_function_231 */
			case PS3HVC_HVCALL_UNDOCUMENTED_FUNCTION_231:
				{
					if ((hvcall->in_args != 1) && (hvcall->out_args != 0)) {
						res = -EFAULT;
						goto kfree_arg;
					}

					hvcall->result = lv1_undocumented_function_231(hvcall->args[0]);

					break;
				}

			/* lv1_get_rtc */
			case PS3HVC_HVCALL_GET_RTC:
				{
					if ((hvcall->in_args != 0) && (hvcall->out_args != 2)) {
						res = -EFAULT;
						goto kfree_arg;
					}

					hvcall->result = lv1_get_rtc(&hvcall->args[0], &hvcall->args[1]);

					break;
				}

			/* lv1_panic */
			case PS3HVC_HVCALL_PANIC:
				{
					if ((hvcall->in_args != 1) && (hvcall->out_args != 0)) {
						res = -EFAULT;
						goto kfree_arg;
					}

					hvcall->result = lv1_panic(hvcall->args[0]);

					break;
				}

			default:
				{
					res = -EFAULT;
					goto kfree_arg;
				}
			}

			if (copy_to_user(argp, hvcall, sizeof(struct ps3hvc_ioctl_hvcall) +
				(hvcall->in_args + hvcall->out_args) * sizeof(u64))) {
				pr_debug("%s:%d: copy_to_user failed\n", __func__, __LINE__);
				res = -EFAULT;
				goto kfree_arg;
			}

			res = 0;

		kfree_arg:

			kfree(hvcall);

			break;
		}
	}

	return res;
}

static const struct file_operations ps3hvc_fops = {
	.owner		= THIS_MODULE,
	.unlocked_ioctl	= ps3hvc_ioctl,
	.compat_ioctl	= ps3hvc_ioctl,
};

static struct miscdevice ps3hvc_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= DEVICE_NAME,
	.fops	= &ps3hvc_fops,
};

static int __init ps3hvc_init(void)
{
	int res;

	res = misc_register(&ps3hvc_misc);
	if (res) {
		pr_debug("%s:%u: misc_register failed %d\n",
			 __func__, __LINE__, res);
		return res;
	}

	pr_debug("%s:%u: registered misc device %d\n",
		 __func__, __LINE__, ps3hvc_misc.minor);

	return 0;
}

static void __exit ps3hvc_exit(void)
{
	misc_deregister(&ps3hvc_misc);
}

module_init(ps3hvc_init);
module_exit(ps3hvc_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PS3 HVC Driver");
MODULE_AUTHOR("Graf Chokolo");
