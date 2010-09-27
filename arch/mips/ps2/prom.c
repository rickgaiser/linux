/* Copyright 2010 Mega Man */
/* TBD: Unfinished state. Rework code. */
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/bootmem.h>
#include <linux/string.h>
#include <linux/kernel.h>

#include <asm/addrspace.h>
#include <asm/bootinfo.h>
#include <asm/sections.h>

void __init prom_init(void)
{
	/* TBD: Get information from boot info structure. */
	printk("prom_init: TBD: Get information from boot info structure.\n");
}

void __init prom_free_prom_memory(void)
{
	unsigned long end;

	/*
	 * Free everything below the kernel itself but leave
	 * the first page reserved for the exception handlers.
	 * The first 0x10000 Bytes contain the SBIOS.
	 */

	end = __pa(&_text);

	free_init_pages("unused PROM memory", 0x10000, end);
}
