
/*
 * PS3 HTAB Driver
 *
 * Copyright (C) 2011 glevand (geoffrey.levand@mail.ru)
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

#define DEVICE_NAME		"ps3htab"

#define HTAB_SIZE		(1 << CONFIG_PS3_HTAB_SIZE)

static u64 ps3htab_lpar_addr;

static u8 *ps3htab;

static loff_t ps3htab_llseek(struct file *file, loff_t offset, int origin)
{
	loff_t res;

	mutex_lock(&file->f_mapping->host->i_mutex);

	switch (origin) {
	case 1:
		offset += file->f_pos;
		break;
	case 2:
		offset += HTAB_SIZE;
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

static ssize_t ps3htab_read(struct file *file, char __user *buf,
			   size_t count, loff_t *pos)
{
	u64 size;
	u8 *src;
	int res;

	pr_debug("%s:%u: Reading %zu bytes at position %lld to U0x%p\n",
		 __func__, __LINE__, count, *pos, buf);

	size = HTAB_SIZE;
	if (*pos >= size || !count)
		return 0;

	if (*pos + count > size) {
		pr_debug("%s:%u Truncating count from %zu to %llu\n", __func__,
			 __LINE__, count, size - *pos);
		count = size - *pos;
	}

	src = ps3htab + *pos;

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

static ssize_t ps3htab_write(struct file *file, const char __user *buf,
		            size_t count, loff_t *pos)
{
	u64 size;
	u8 *dst;
	int res;

	pr_debug("%s:%u: Writing %zu bytes at position %lld from U0x%p\n",
		 __func__, __LINE__, count, *pos, buf);

	size = HTAB_SIZE;
	if (*pos >= size || !count)
		return 0;

	if (*pos + count > size) {
		pr_debug("%s:%u Truncating count from %zu to %llu\n", __func__,
			 __LINE__, count, size - *pos);
		count = size - *pos;
	}

	dst = ps3htab + *pos;

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

static const struct file_operations ps3htab_fops = {
	.owner		= THIS_MODULE,
	.llseek		= ps3htab_llseek,
	.read		= ps3htab_read,
	.write		= ps3htab_write,
};

static struct miscdevice ps3htab_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= DEVICE_NAME,
	.fops	= &ps3htab_fops,
};

static int __init ps3htab_init(void)
{
	int res;

	res = lv1_map_htab(0, &ps3htab_lpar_addr);
	if (res) {
		pr_debug("%s:%u: htab map failed %d\n", __func__, __LINE__, res);
		res = -EFAULT;
		goto fail;
	}

	ps3htab = ioremap(ps3htab_lpar_addr, HTAB_SIZE);
	if (!ps3htab) {
		pr_debug("%s:%d: ioremap failed\n", __func__, __LINE__);
		res = -EFAULT;
		goto fail_lpar_unmap;
	}

	res = misc_register(&ps3htab_misc);
	if (res) {
		pr_debug("%s:%u: misc_register failed %d\n",
			 __func__, __LINE__, res);
		goto fail_iounmap;
	}

	pr_debug("%s:%u: registered misc device %d\n",
		 __func__, __LINE__, ps3htab_misc.minor);

	return 0;

fail_iounmap:

	iounmap(ps3htab);

fail_lpar_unmap:

	lv1_unmap_htab(ps3htab_lpar_addr);

fail:

	return res;
}

static void __exit ps3htab_exit(void)
{
	misc_deregister(&ps3htab_misc);

	iounmap(ps3htab);

	lv1_unmap_htab(ps3htab_lpar_addr);
}

module_init(ps3htab_init);
module_exit(ps3htab_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("PS3 HTAB Driver");
MODULE_AUTHOR("glevand");
