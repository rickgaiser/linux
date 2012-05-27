/*
 *  PlayStation 2 IOP memory management utility
 *
 *        Copyright (C) 2000, 2001  Sony Computer Entertainment Inc.
 *        Copyright (C) 2012        Mega Man
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * $Id$
 */
#ifndef __PS2_IOPMEM_H
#define __PS2_IOPMEM_H

#include <linux/kernel.h>
#include <linux/semaphore.h>

#define PS2IOPMEM_MAXMEMS	16

struct ps2iopmem_entry {
	dma_addr_t addr;
	int size;
};

struct ps2iopmem_list {
	struct semaphore	lock;
	struct ps2iopmem_entry	mems[PS2IOPMEM_MAXMEMS];
};

void ps2iopmem_init(struct ps2iopmem_list *);
void ps2iopmem_end(struct ps2iopmem_list *);
int ps2iopmem_alloc(struct ps2iopmem_list *iml, int size);
void ps2iopmem_free(struct ps2iopmem_list *iml, int hdl);
unsigned long ps2iopmem_getaddr(struct ps2iopmem_list *, int hdl, int size);
#endif /* __PS2_IOPMEM_H */
