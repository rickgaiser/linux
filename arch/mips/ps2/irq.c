/* Copyright 2010 Mega Man */
/* TBD: Unfinished state. Rework code. */
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/kernel.h>

#include <asm/io.h>
#include <asm/irq.h>
#include <asm/irq_cpu.h>

asmlinkage void plat_irq_dispatch(void)
{
	/* TBD: No IRQ handling. */
	printk("plat_irq_dispatch: TBD: No IRQ handling.\n");
}

void __init arch_init_irq(void)
{
	/* TBD: No IRQ handling. */
	printk("arch_init_irq: TBD: No IRQ handling.\n");
}
