/*
 *  PlayStation 2 CD/DVD driver
 *
 *        Copyright (C) 2000-2002  Sony Computer Entertainment Inc.
 *
 *  This file is subject to the terms and conditions of the GNU General
 *  Public License Version 2. See the file "COPYING" in the main
 *  directory of this archive for more details.
 *
 * $Id$
 */

#define __NO_VERSION__
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include "cdvd.h"

static loff_t
cdvd_file__llseek(struct file *file, loff_t offset, int origin)
{
	return -ESPIPE;
}

static ssize_t
cdvd_file__read(struct file *filp, char *buffer, size_t count, loff_t *ppos)
{
	return -EINVAL;
}

static ssize_t
cdvd_file__write(struct file *filp, const char *buffer, size_t count, loff_t *ppos)
{
	return -EINVAL;
}

static int
cdvd_file__ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	return ps2cdvd_common_ioctl(cmd, arg);
}

static int
cdvd_file__open(struct inode *inode, struct file *filp)
{

	return 0;
}

static int
cdvd_file__release(struct inode *inode, struct file *filp)
{

	return 0;
}

struct file_operations ps2cdvd_altdev_fops = {
	owner:	THIS_MODULE,
	llseek:	cdvd_file__llseek,
	read:	cdvd_file__read,
	write:	cdvd_file__write,
	ioctl:	cdvd_file__ioctl,
	open:	cdvd_file__open,
	release:cdvd_file__release,
};
