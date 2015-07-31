
/*
 * PS3 SPE MMIO Driver
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
#include <linux/uaccess.h>
#include <linux/mmzone.h>
#include <linux/io.h>
#include <linux/mm.h>

#include <asm/lv1call.h>
#include <asm/ps3.h>

#define DEVICE_NAME		"ps3spemmio"

#define NUM_SPE			8
#define MMAP_START_ADDR		0x20000000000ull
#define MMAP_SIZE		(NUM_SPE * 0x80000ull + NUM_SPE * 0x2000ull)

static u64 ps3spemmio_lpar_addr;

static u8 *ps3spemmio;

static loff_t ps3spemmio_llseek(struct file *file, loff_t offset, int origin)
{
	loff_t res;

	mutex_lock(&file->f_mapping->host->i_mutex);

	switch (origin) {
	case 1:
		offset += file->f_pos;
		break;
	case 2:
		offset += MMAP_SIZE;
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

static ssize_t ps3spemmio_read(struct file *file, char __user *buf,
	size_t count, loff_t *pos)
{
	u64 size;
	u8 *src;
	int res;

	pr_debug("%s:%u: Reading %zu bytes at position %lld to U0x%p\n",
		 __func__, __LINE__, count, *pos, buf);

	size = MMAP_SIZE;
	if (*pos >= size || !count)
		return 0;

	if (*pos + count > size) {
		pr_debug("%s:%u Truncating count from %zu to %llu\n", __func__,
			 __LINE__, count, size - *pos);
		count = size - *pos;
	}

	src = ps3spemmio + *pos;

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

static ssize_t ps3spemmio_write(struct file *file, const char __user *buf,
	size_t count, loff_t *pos)
{
	u64 size;
	u8 *dst;
	int res;

	pr_debug("%s:%u: Writing %zu bytes at position %lld from U0x%p\n",
		 __func__, __LINE__, count, *pos, buf);

	size = MMAP_SIZE;
	if (*pos >= size || !count)
		return 0;

	if (*pos + count > size) {
		pr_debug("%s:%u Truncating count from %zu to %llu\n", __func__,
			 __LINE__, count, size - *pos);
		count = size - *pos;
	}

	dst = ps3spemmio + *pos;

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

static const struct file_operations ps3spemmio_fops = {
	.owner		= THIS_MODULE,
	.llseek		= ps3spemmio_llseek,
	.read		= ps3spemmio_read,
	.write		= ps3spemmio_write,
};

static struct miscdevice ps3spemmio_misc = {
	.minor	= MISC_DYNAMIC_MINOR,
	.name	= DEVICE_NAME,
	.fops	= &ps3spemmio_fops,
};

static int __init ps3spemmio_init(void)
{
	int res;

	res = lv1_undocumented_function_114(MMAP_START_ADDR, PAGE_SHIFT, MMAP_SIZE, &ps3spemmio_lpar_addr);
	if (res) {
		pr_debug("%s:%u: lpar map failed %d\n", __func__, __LINE__, res);
		res = -EFAULT;
		goto fail;
	}

	ps3spemmio = ioremap_flags(ps3spemmio_lpar_addr, MMAP_SIZE, _PAGE_NO_CACHE | 3);
	if (!ps3spemmio) {
		pr_debug("%s:%d: ioremap_flags failed\n", __func__, __LINE__);
		res = -EFAULT;
		goto fail_lpar_unmap;
	}

	res = misc_register(&ps3spemmio_misc);
	if (res) {
		pr_debug("%s:%u: misc_register failed %d\n",
			 __func__, __LINE__, res);
		goto fail_iounmap;
	}

	pr_debug("%s:%u: registered misc device %d\n",
		 __func__, __LINE__, ps3spemmio_misc.minor);

	return 0;

fail_iounmap:

	iounmap(ps3spemmio);

fail_lpar_unmap:

	lv1_undocumented_function_115(ps3spemmio_lpar_addr);

fail:

	return res;
}

static void __exit ps3spemmio_exit(void)
{
	misc_deregister(&ps3spemmio_misc);

	iounmap(ps3spemmio);

	lv1_undocumented_function_115(ps3spemmio_lpar_addr);
}

module_init(ps3spemmio_init);
module_exit(ps3spemmio_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("PS3 SPE MMIO Driver");
MODULE_AUTHOR("Graf Chokolo");
