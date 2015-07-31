
/*
 * PS3 RAM Driver
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
#include <asm/ps3ram.h>

#define DEVICE_NAME		"ps3ram"

#define RAM_SIZE		(256 * 1024 * 1024)

static u64 ps3ram_lpar_addr;

static u8 *ps3ram;

static loff_t ps3ram_llseek(struct file *file, loff_t offset, int origin)
{
	loff_t res;

	mutex_lock(&file->f_mapping->host->i_mutex);

	switch (origin) {
	case 1:
		offset += file->f_pos;
		break;
	case 2:
		offset += RAM_SIZE;
		break;
	}

	if (offset < 0) {
		res = -EINVAL;
		goto out;
	}

	file->f_pos = offset;
	res = file->f_pos;

out:
	mutex_unlock(&file->f_mapping->host->i_mutex);

	return res;
}

static ssize_t ps3ram_read(struct file *file, char __user *buf,
			   size_t count, loff_t *pos)
{
	u64 size;
	u8 *src;
	int res;

	pr_debug("%s:%u: Reading %zu bytes at position %lld to U0x%p\n",
		 __func__, __LINE__, count, *pos, buf);

	size = RAM_SIZE;
	if (*pos >= size || !count)
		return 0;

	if (*pos + count > size) {
		pr_debug("%s:%u Truncating count from %zu to %llu\n", __func__,
			 __LINE__, count, size - *pos);
		count = size - *pos;
	}

	src = ps3ram + *pos;

	pr_debug("%s:%u: copy %lu bytes from 0x%p to U0x%p\n",
		 __func__, __LINE__, count, src, buf);

	if (buf) {
		if (copy_to_user(buf, src, count)) {
			res = -EFAULT;
			goto fail;
		}
	}

	*pos += count;

	return count;

fail:

	return res;
}

static ssize_t ps3ram_write(struct file *file, const char __user *buf,
		            size_t count, loff_t *pos)
{
	u64 size;
	u8 *dst;
	int res;

	pr_debug("%s:%u: Writing %zu bytes at position %lld from U0x%p\n",
		 __func__, __LINE__, count, *pos, buf);

	size = RAM_SIZE;
	if (*pos >= size || !count)
		return 0;

	if (*pos + count > size) {
		pr_debug("%s:%u Truncating count from %zu to %llu\n", __func__,
			 __LINE__, count, size - *pos);
		count = size - *pos;
	}

	dst = ps3ram + *pos;

	pr_debug("%s:%u: copy %lu bytes from U0x%p to 0x%p\n",
		 __func__, __LINE__, count, buf, dst);

	if (buf) {
		if (copy_from_user(dst, buf, count)) {
			res = -EFAULT;
			goto fail;
		}
	}

	*pos += count;

	return count;

fail:

	return res;
}

static int ps3ram_user_to_lpar_addr(u64 user_addr, u64 *lpar_addr)
{
	struct page **pages;
	int res;

	pages = kmalloc(1 * sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return -ENOMEM;

	down_read(&current->mm->mmap_sem);

	res = get_user_pages(current, current->mm, user_addr & PAGE_MASK, 1, 0, 0, pages, NULL);
	if (!res)
		goto fail_get_user_pages;

	up_read(&current->mm->mmap_sem);

	*lpar_addr = ps3_mm_phys_to_lpar(__pa(page_address(pages[0])));

	page_cache_release(pages[0]);

	kfree(pages);

	return 0;

fail_get_user_pages:

	up_read(&current->mm->mmap_sem);

	kfree(pages);

	return res;
}

static long ps3ram_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *) arg;
	int res;

	switch (cmd) {
	case PS3RAM_IOCTL_USER_TO_LPAR_ADDR:
		{
			struct ps3ram_ioctl_user_to_lpar_addr user_to_lpar_addr;

			if (copy_from_user(&user_to_lpar_addr, argp, sizeof(user_to_lpar_addr)))
				return -EFAULT;

			res = ps3ram_user_to_lpar_addr(user_to_lpar_addr.user_addr & PAGE_MASK,
				&user_to_lpar_addr.lpar_addr);
			if (res)
				return res;

			user_to_lpar_addr.lpar_addr += user_to_lpar_addr.user_addr & ~PAGE_MASK;

			if (copy_to_user(argp, &user_to_lpar_addr, sizeof(user_to_lpar_addr)))
				return -EFAULT;

			return 0;
		}
	}

	return -EFAULT;
}

static const struct file_operations ps3ram_fops = {
	.owner		= THIS_MODULE,
	.llseek		= ps3ram_llseek,
	.read		= ps3ram_read,
	.write		= ps3ram_write,
	.unlocked_ioctl	= ps3ram_ioctl,
	.compat_ioctl	= ps3ram_ioctl,
};

static struct miscdevice ps3ram_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= DEVICE_NAME,
	.fops	= &ps3ram_fops,
};

static int __init ps3ram_init(void)
{
	int res;

	res = lv1_undocumented_function_114(0, PAGE_SHIFT, RAM_SIZE, &ps3ram_lpar_addr);
	if (res) {
		pr_debug("%s:%u: lpar map failed %d\n", __func__, __LINE__, res);
		res = -EFAULT;
		goto fail;
	}

	ps3ram = ioremap(ps3ram_lpar_addr, RAM_SIZE);
	if (!ps3ram) {
		pr_debug("%s:%d: ioremap failed\n", __func__, __LINE__);
		res = -EFAULT;
		goto fail_lpar_unmap;
	}

	res = misc_register(&ps3ram_misc);
	if (res) {
		pr_debug("%s:%u: misc_register failed %d\n",
			 __func__, __LINE__, res);
		goto fail_iounmap;
	}

	pr_debug("%s:%u: registered misc device %d\n",
		 __func__, __LINE__, ps3ram_misc.minor);

	return 0;

fail_iounmap:

	iounmap(ps3ram);

fail_lpar_unmap:

	lv1_undocumented_function_115(ps3ram_lpar_addr);

fail:

	return res;
}

static void __exit ps3ram_exit(void)
{
	misc_deregister(&ps3ram_misc);

	iounmap(ps3ram);

	lv1_undocumented_function_115(ps3ram_lpar_addr);
}

module_init(ps3ram_init);
module_exit(ps3ram_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("PS3 RAM Driver");
MODULE_AUTHOR("Graf Chokolo");
