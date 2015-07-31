
/*
 * PS3 Dispatcher Manager Proxy Driver
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
#include <asm/ps3dm.h>
#include <asm/ps3dmproxy.h>

#define DEVICE_NAME		"ps3dmproxy"

static ssize_t ps3dmproxy_read(struct file *file, char __user *buf,
	size_t count, loff_t *pos)
{
	char *p;
	int res;

	if (!count)
		return 0;

	if (buf) {
		p = kmalloc(count, GFP_KERNEL);
		if (!p)
			return -ENOMEM;

		res = ps3dm_read(p, count);
		if (res)
			goto fail_free;

		if (copy_to_user(buf, p, count))
			count = -EFAULT;

		kfree(p);
	}

	if (count > 0)
		*pos += count;

	return count;

fail_free:

	kfree(p);

	return res;
}

static ssize_t ps3dmproxy_write(struct file *file, const char __user *buf,
	size_t count, loff_t *pos)
{
	char *p;
	int res;

	if (!count)
		return 0;

	if (buf) {
		p = kmalloc(count, GFP_KERNEL);
		if (!p)
			return -ENOMEM;

		if (copy_from_user(p, buf, count)) {
			res = -EFAULT;
			goto fail_free;
		}

		res = ps3dm_write(p, count);
		if (res)
			count = res;

		kfree(p);
	}

	if (count > 0)
		*pos += count;

	return count;

fail_free:

	kfree(p);

	return res;
}

static int ps3dmproxy_user_to_lpar_addr(u64 user_addr, u64 *lpar_addr)
{
	struct page **pages;
	int res;

	pr_debug(" -> %s:%d\n", __func__, __LINE__);

	pages = kmalloc(1 * sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		pr_debug("%s:%d: kmalloc failed\n", __func__, __LINE__);
		return -ENOMEM;
	}

	down_read(&current->mm->mmap_sem);

	res = get_user_pages(current, current->mm, user_addr & PAGE_MASK, 1, 0, 0, pages, NULL);
	if (!res) {
		pr_debug("%s:%d: get_user_pages failed (%d)\n", __func__, __LINE__, res);
		goto fail_get_user_pages;
	}

	up_read(&current->mm->mmap_sem);

	*lpar_addr = ps3_mm_phys_to_lpar(__pa(page_address(pages[0])));

	page_cache_release(pages[0]);

	kfree(pages);

	pr_debug(" <- %s:%d: ok\n", __func__, __LINE__);

	return 0;

fail_get_user_pages:

	up_read(&current->mm->mmap_sem);

	kfree(pages);

	pr_debug(" <- %s:%d: failed\n", __func__, __LINE__);

	return res;
}

static long ps3dmproxy_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *) arg;
	int res;

	pr_debug("%s:%d cmd %x\n", __func__, __LINE__, cmd);

	switch (cmd) {
	case PS3DMPROXY_IOCTL_USER_TO_LPAR_ADDR:
		{
			struct ps3dmproxy_ioctl_user_to_lpar_addr user_to_lpar_addr;

			if (copy_from_user(&user_to_lpar_addr, argp, sizeof(user_to_lpar_addr))) {
				pr_debug("%s:%d: copy_from_user failed\n", __func__, __LINE__);
				return -EFAULT;
			}

			res = ps3dmproxy_user_to_lpar_addr(user_to_lpar_addr.user_addr & PAGE_MASK,
				&user_to_lpar_addr.lpar_addr);
			if (res) {
				pr_debug("%s:%d: ps3dmproxy_user_to_lpar_addr failed (%d)\n",
					__func__, __LINE__, res);
				return res;
			}

			user_to_lpar_addr.lpar_addr += user_to_lpar_addr.user_addr & ~PAGE_MASK;

			if (copy_to_user(argp, &user_to_lpar_addr, sizeof(user_to_lpar_addr))) {
				pr_debug("%s:%d: copy_to_user failed\n", __func__, __LINE__);
				return -EFAULT;
			}

			return 0;
		}

	case PS3DMPROXY_IOCTL_GET_REPO_NODE_VAL:
		{
			struct ps3dmproxy_ioctl_get_repo_node_val get_repo_node_val;

			if (copy_from_user(&get_repo_node_val, argp, sizeof(get_repo_node_val))) {
				pr_debug("%s:%d: copy_from_user failed\n", __func__, __LINE__);
				return -EFAULT;
			}

			res = lv1_get_repository_node_value(get_repo_node_val.lpar_id,
				get_repo_node_val.key[0], get_repo_node_val.key[1],
				get_repo_node_val.key[2], get_repo_node_val.key[3],
				&get_repo_node_val.val[0], &get_repo_node_val.val[1]);
			if (res) {
				pr_debug("%s:%d: lv1_get_repository_node_value failed (%d)\n",
					__func__, __LINE__, res);
				return res;
			}

			if (copy_to_user(argp, &get_repo_node_val, sizeof(get_repo_node_val))) {
				pr_debug("%s:%d: copy_to_user failed\n", __func__, __LINE__);
				return -EFAULT;
			}

			return 0;
		}

	case PS3DMPROXY_IOCTL_DO_REQUEST:
		{
			struct ps3dmproxy_ioctl_do_request do_request;
			struct ps3dm_hdr *sendbuf, *recvbuf;

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

			recvbuf = kmalloc(do_request.recvbuf_size, GFP_KERNEL);
			if (!recvbuf) {
				pr_debug("%s:%d: kmalloc failed\n", __func__, __LINE__);
				res = -ENOMEM;
				goto kfree_sendbuf;
			}

			res = ps3dm_do_request(sendbuf, do_request.sendbuf_size,
				recvbuf, do_request.recvbuf_size);
			if (res) {
				pr_debug("%s:%d: ps3dm_do_request failed (%d)\n", __func__, __LINE__, res);
				goto kfree_recvbuf;
			}

			if (copy_to_user((void __user *) do_request.recvbuf, recvbuf,
				do_request.recvbuf_size)) {
				pr_debug("%s:%d: copy_to_user failed\n", __func__, __LINE__);
				res = -EFAULT;
				goto kfree_recvbuf;
			}

			res = 0;

		kfree_recvbuf:

			kfree(recvbuf);

		kfree_sendbuf:

			kfree(sendbuf);

			return res;
		}
	}

	return -EFAULT;
}

static const struct file_operations ps3dmproxy_fops = {
	.owner		= THIS_MODULE,
	.read		= ps3dmproxy_read,
	.write		= ps3dmproxy_write,
	.unlocked_ioctl	= ps3dmproxy_ioctl,
	.compat_ioctl	= ps3dmproxy_ioctl,
};

static struct miscdevice ps3dmproxy_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= DEVICE_NAME,
	.fops	= &ps3dmproxy_fops,
};

static int __init ps3dmproxy_init(void)
{
	int res;

	res = misc_register(&ps3dmproxy_misc);
	if (res) {
		pr_debug("%s:%u: misc_register failed %d\n",
			 __func__, __LINE__, res);
		return res;
	}

	pr_debug("%s:%u: registered misc device %d\n",
		 __func__, __LINE__, ps3dmproxy_misc.minor);

	return 0;
}

static void __exit ps3dmproxy_exit(void)
{
	misc_deregister(&ps3dmproxy_misc);
}

module_init(ps3dmproxy_init);
module_exit(ps3dmproxy_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PS3 Dispatcher Manager Proxy Driver");
MODULE_AUTHOR("Graf Chokolo");
