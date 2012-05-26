/*
 *
 *        Copyright (C) 2000, 2002  Sony Computer Entertainment Inc.
 *        Copyright (C) 2012 Mega Man
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * $Id$
 */
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/stat.h>
#include <linux/init.h>
#include <linux/fs.h>

#include "mcfs.h"
#include "mcfs_debug.h"

static struct dentry *ps2mcfs_null_lookup(struct inode *, struct dentry *, struct nameidata *);

struct file_operations ps2mcfs_null_operations = {
	/* NULL */
};

struct inode_operations ps2mcfs_null_inode_operations = {
	lookup:			ps2mcfs_null_lookup,
};

static struct dentry *
ps2mcfs_null_lookup(struct inode *dir, struct dentry *dentry, struct nameidata *name_i_data)
{
	return ERR_PTR(-ENOENT);
}
