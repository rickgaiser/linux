/*
 * iopheap.c
 *
 *	Copyright (C) 2000-2002  Sony Computer Entertainment Inc.
 *	Copyright (C) 2010       Mega Man
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * $Id$
 */
/* TBD: Unfinished state. Rework code. */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/semaphore.h>

#include <asm/mach-ps2/ps2.h>
#include <asm/mach-ps2/sifdefs.h>
#include <asm/mach-ps2/sbios.h>

static DECLARE_MUTEX(iopheap_sem);

EXPORT_SYMBOL(ps2sif_allociopheap);
EXPORT_SYMBOL(ps2sif_freeiopheap);
EXPORT_SYMBOL(ps2sif_virttobus);
EXPORT_SYMBOL(ps2sif_bustovirt);

int __init ps2sif_initiopheap(void)
{
    volatile int i;
    int result;
    int err;

    while (1) {
	down(&iopheap_sem);
	err = sbios_rpc(SBR_IOPH_INIT, NULL, &result);
	up(&iopheap_sem);

	if (err < 0)
	    return -1;
	if (result == 0)
	    break;
	i = 0x100000;
	while (i--)
	    ;
    }
    return 0;
}

dma_addr_t ps2sif_allociopheap(int size)
{
    struct sbr_ioph_alloc_arg arg;
    int result;
    int err;
    
    arg.size = size;

    down(&iopheap_sem);
    err = sbios_rpc(SBR_IOPH_ALLOC, &arg, &result);
    up(&iopheap_sem);

    if (err < 0)
		return 0;
    return result;
}

int ps2sif_freeiopheap(dma_addr_t addr)
{
    struct sbr_ioph_free_arg arg;
    int result;
    int err;
    
    arg.addr = addr;

    down(&iopheap_sem);
    err = sbios_rpc(SBR_IOPH_FREE, &arg, &result);
    up(&iopheap_sem);

    if (err < 0)
		return -1;
    return result;
}

dma_addr_t ps2sif_virttobus(volatile void *a)
{
	return((unsigned int)a - PS2_IOP_HEAP_BASE);
}

void *ps2sif_bustovirt(dma_addr_t a)
{
	return((void *)a + PS2_IOP_HEAP_BASE);
}
