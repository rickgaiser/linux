/*
 *	arch/mips/include/asm/mach-ps2/ioremap.h
 *
 *      Copyright 2010 Mega Man
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */
#ifndef __ASM_MACH_PS2_IOREMAP_H
#define __ASM_MACH_PS2_IOREMAP_H

#include <linux/types.h>

#include <asm/mach-ps2/ps2.h>

/* There are no 64 bit addresses. No fixup is needed. */
static inline phys_t fixup_bigphys_addr(phys_t phys_addr, phys_t size)
{
	return phys_addr;
}

static inline void __iomem *plat_ioremap(phys_t offset, unsigned long size,
	unsigned long flags)
{
	if ((offset >= PS2_IOP_HEAP_BASE) && (offset < CKSEG2)) {
		/* Memory is already mapped. */
		if (flags & _CACHE_UNCACHED) {
			return (void __iomem *)
				(unsigned long)CKSEG1ADDR(offset);
		} else {
			return (void __iomem *)
				(unsigned long)CKSEG0ADDR(offset);
		}
	} else if ((offset >= PS2_HW_BASE) && (offset < CKSEG0)) {
		/* Memory is already mapped. */
		if (flags & _CACHE_UNCACHED) {
			return (void __iomem *)
				(unsigned long)CKSEG1ADDR(offset);
		} else {
			return (void __iomem *)
				(unsigned long)CKSEG0ADDR(offset);
		}
	}
	/* Memory will be page mapped by kernel. */
	return NULL;
}

static inline int plat_iounmap(const volatile void __iomem *addr)
{
	unsigned long kseg_addr;

	kseg_addr = (unsigned long) addr;
	if ((kseg_addr >= CKSEG0) && (kseg_addr < CKSEG2)) {
		/* Memory is always mapped in kernel mode. No unmap possible. */
		return 1;
	}
	return 0;
}

#endif /* __ASM_MACH_PS2_IOREMAP_H */
