/* Copyright 2010 Mega Man */
/* TBD: Unfinished state. Rework code. */
#include <linux/console.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/param.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/pm.h>

void __init plat_mem_setup(void)
{
	printk("plat_mem_setup: TBD: Memory initialisation incomplete.\n");
#if 0
	_machine_restart = ps2_machine_restart;
	_machine_halt = ps2_machine_halt;
	pm_power_off = ps2_machine_power_off;
#endif

	ioport_resource.start = ~0UL;
	ioport_resource.end = 0UL;
}
