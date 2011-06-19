/*
 * irq.c
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>

#include <asm/bootinfo.h>
#include <asm/io.h>
#include <asm/mipsregs.h>
#include <asm/system.h>
#include <asm/irq_cpu.h>
#include <asm/mach-ps2/irq.h>
#include <asm/mach-ps2/speed.h>
#include <asm/mach-ps2/ps2.h>

#define INTC_STAT	0x1000f000
#define INTC_MASK	0x1000f010
#define DMAC_STAT	0x1000e010
#define DMAC_MASK	0x1000e010
#define GS_CSR		0x12001000
#define GS_IMR		0x12001010

#define SBUS_SMFLG	0x1000f230
#define SBUS_AIF_INTSR	0x18000004
#define SBUS_AIF_INTEN	0x18000006
#define SBUS_PCIC_EXC1	0x1f801476
#define SBUS_PCIC_CSC1	0x1f801464
#define SBUS_PCIC_IMR1	0x1f801468
#define SBUS_PCIC_TIMR	0x1f80147e
#define SBUS_PCIC3_TIMR	0x1f801466

/*
 * INTC
 */
static volatile unsigned long intc_mask = 0;

static inline void intc_enable_irq(unsigned int irq_nr)
{
	if (!(intc_mask & (1 << irq_nr))) {
		intc_mask |= (1 << irq_nr);
		outl(1 << irq_nr, INTC_MASK);
	}
}

static inline void intc_disable_irq(unsigned int irq_nr)
{
	if ((intc_mask & (1 << irq_nr))) {
		intc_mask &= ~(1 << irq_nr);
		outl(1 << irq_nr, INTC_MASK);
	}
}

static unsigned int intc_startup_irq(unsigned int irq_nr)
{
	intc_enable_irq(irq_nr);
	return 0;
}

static void intc_shutdown_irq(unsigned int irq_nr)
{
	intc_disable_irq(irq_nr);
}

static void intc_ack_irq(unsigned int irq_nr)
{
	intc_disable_irq(irq_nr);
	outl(1 << irq_nr, INTC_STAT);
}

static void intc_end_irq(unsigned int irq_nr)
{
	if (!(irq_desc[irq_nr].status & (IRQ_DISABLED|IRQ_INPROGRESS))) {
		intc_enable_irq(irq_nr);
	} else {
		printk("warning: end_irq %d did not enable (%x)\n", 
				irq_nr, irq_desc[irq_nr].status);
	}
}

static struct irq_chip intc_irq_type = {
	.name		= "EE INTC",
	.startup	= intc_startup_irq,
	.shutdown	= intc_shutdown_irq,
	.unmask		= intc_enable_irq,
	.mask		= intc_disable_irq,
	.mask_ack	= intc_ack_irq,
	.end		= intc_end_irq,
};

/*
 * DMAC
 */
static volatile unsigned long dmac_mask = 0;

static inline void dmac_enable_irq(unsigned int irq_nr)
{
	unsigned int dmac_irq_nr = irq_nr - IRQ_DMAC;

	if (!(dmac_mask & (1 << dmac_irq_nr))) {
		dmac_mask |= (1 << dmac_irq_nr);
		outl(1 << (dmac_irq_nr + 16), DMAC_MASK);
	}
}

static inline void dmac_disable_irq(unsigned int irq_nr)
{
	unsigned int dmac_irq_nr = irq_nr - IRQ_DMAC;

	if ((dmac_mask & (1 << dmac_irq_nr))) {
		dmac_mask &= ~(1 << dmac_irq_nr);
		outl(1 << (dmac_irq_nr + 16), DMAC_MASK);
	}
}

static unsigned int dmac_startup_irq(unsigned int irq_nr)
{
	dmac_enable_irq(irq_nr);
	return 0;
}

static void dmac_shutdown_irq(unsigned int irq_nr)
{
	dmac_disable_irq(irq_nr);
}

static void dmac_ack_irq(unsigned int irq_nr)
{
	unsigned int dmac_irq_nr = irq_nr - IRQ_DMAC;

	dmac_disable_irq(irq_nr);
	outl(1 << dmac_irq_nr, DMAC_STAT);
}

static void dmac_end_irq(unsigned int irq_nr)
{
	if (!(irq_desc[irq_nr].status & (IRQ_DISABLED|IRQ_INPROGRESS))) {
		dmac_enable_irq(irq_nr);
	} else {
		printk("warning: end_irq %d did not enable (%x)\n", 
				irq_nr, irq_desc[irq_nr].status);
	}
}

static struct irq_chip dmac_irq_type = {
	.name		= "EE DMAC",
	.startup	= dmac_startup_irq,
	.shutdown	= dmac_shutdown_irq,
	.unmask		= dmac_enable_irq,
	.mask		= dmac_disable_irq,
	.mask_ack	= dmac_ack_irq,
	.end		= dmac_end_irq,
};

/*
 * GS
 */
static volatile unsigned long gs_mask = 0;

void ps2_setup_gs_imr(void)
{
	outl(0xff00, GS_IMR);
	outl(1 << IRQ_INTC_GS, INTC_STAT);
	outl((~gs_mask & 0x7f) << 8, GS_IMR);
}

static inline void gs_enable_irq(unsigned int irq_nr)
{
	unsigned int gs_irq_nr = irq_nr - IRQ_GS;

	gs_mask |= (1 << gs_irq_nr);
	ps2_setup_gs_imr();
}

static inline void gs_disable_irq(unsigned int irq_nr)
{
	unsigned int gs_irq_nr = irq_nr - IRQ_GS;

	gs_mask &= ~(1 << gs_irq_nr);
	ps2_setup_gs_imr();
}

static unsigned int gs_startup_irq(unsigned int irq_nr)
{
	gs_enable_irq(irq_nr);
	return 0;
}

static void gs_shutdown_irq(unsigned int irq_nr)
{
	gs_disable_irq(irq_nr);
}

static void gs_ack_irq(unsigned int irq_nr)
{
	unsigned int gs_irq_nr = irq_nr - IRQ_GS;

	outl(0xff00, GS_IMR);
	outl(1 << IRQ_INTC_GS, INTC_STAT);
	outl(1 << gs_irq_nr, GS_CSR);
}

static void gs_end_irq(unsigned int irq_nr)
{
	outl((~gs_mask & 0x7f) << 8, GS_IMR);
	if (!(irq_desc[irq_nr].status & (IRQ_DISABLED|IRQ_INPROGRESS))) {
		gs_enable_irq(irq_nr);
	} else {
		printk("warning: end_irq %d did not enable (%x)\n", 
				irq_nr, irq_desc[irq_nr].status);
	}
}

static struct irq_chip gs_irq_type = {
	.name		= "GS",
	.startup	= gs_startup_irq,
	.shutdown	= gs_shutdown_irq,
	.unmask		= gs_enable_irq,
	.mask		= gs_disable_irq,
	.mask_ack	= gs_ack_irq,
	.end		= gs_end_irq,
};

/*
 * SBUS
 */
static volatile unsigned long sbus_mask = 0;

static inline unsigned long sbus_enter_irq(void)
{
	unsigned long istat = 0;

	intc_ack_irq(IRQ_INTC_SBUS);
	if (inl(SBUS_SMFLG) & (1 << 8)) {
		outl(1 << 8, SBUS_SMFLG);
		switch (ps2_pcic_type) {
		case 1:
			if (inw(SBUS_PCIC_CSC1) & 0x0080) {
				outw(0xffff, SBUS_PCIC_CSC1);
				istat |= 1 << (IRQ_SBUS_PCIC - IRQ_SBUS);
			}
			break;
		case 2:
			if (inw(SBUS_PCIC_CSC1) & 0x0080) {
				outw(0xffff, SBUS_PCIC_CSC1);
				istat |= 1 << (IRQ_SBUS_PCIC - IRQ_SBUS);
			}
			break;
		case 3:
			istat |= 1 << (IRQ_SBUS_PCIC - IRQ_SBUS);
			break;
		}
	}

	if (inl(SBUS_SMFLG) & (1 << 10)) {
		outl(1 << 10, SBUS_SMFLG);
		istat |= 1 << (IRQ_SBUS_USB - IRQ_SBUS);
	}
	return istat;
}

static inline void sbus_leave_irq(void)
{
	unsigned short mask;

	if (ps2_pccard_present == 0x0100) {
		mask = inw(SPD_R_INTR_ENA);
		outw(0, SPD_R_INTR_ENA);
		outw(mask, SPD_R_INTR_ENA);
	}

	switch (ps2_pcic_type) {
	case 1: case 2:
		mask = inw(SBUS_PCIC_TIMR);
		outw(1, SBUS_PCIC_TIMR);
		outw(mask, SBUS_PCIC_TIMR);
		break;
	case 3:
		mask = inw(SBUS_PCIC3_TIMR);
		outw(1, SBUS_PCIC3_TIMR);
		outw(mask, SBUS_PCIC3_TIMR);
		break;
	}

	intc_enable_irq(IRQ_INTC_SBUS);
}

static inline void sbus_enable_irq(unsigned int irq_nr)
{
	unsigned int sbus_irq_nr = irq_nr - IRQ_SBUS;

	sbus_mask |= (1 << sbus_irq_nr);

	switch (irq_nr) {
	case IRQ_SBUS_PCIC:
		switch (ps2_pcic_type) {
		case 1:
			outw(0xff7f, SBUS_PCIC_IMR1);
			break;
		case 2:
			outw(0, SBUS_PCIC_TIMR);
			break;
		case 3:
			outw(0, SBUS_PCIC3_TIMR);
			break;
		}
		break;
	case IRQ_SBUS_USB:
		break;
	}
}

static inline void sbus_disable_irq(unsigned int irq_nr)
{
	unsigned int sbus_irq_nr = irq_nr - IRQ_SBUS;

	sbus_mask &= ~(1 << sbus_irq_nr);

	switch (irq_nr) {
	case IRQ_SBUS_PCIC:
		switch (ps2_pcic_type) {
		case 1:
			outw(0xffff, SBUS_PCIC_IMR1);
			break;
		case 2:
			outw(1, SBUS_PCIC_TIMR);
			break;
		case 3:
			outw(1, SBUS_PCIC3_TIMR);
			break;
		}
		break;
	case IRQ_SBUS_USB:
		break;
	}
}

static unsigned int sbus_startup_irq(unsigned int irq_nr)
{
	sbus_enable_irq(irq_nr);
	return 0;
}

static void sbus_shutdown_irq(unsigned int irq_nr)
{
	sbus_disable_irq(irq_nr);
}

static void sbus_ack_irq(unsigned int irq_nr)
{
}

static void sbus_end_irq(unsigned int irq_nr)
{
	if (!(irq_desc[irq_nr].status & (IRQ_DISABLED|IRQ_INPROGRESS))) {
	} else {
		printk("warning: end_irq %d did not enable (%x)\n", 
				irq_nr, irq_desc[irq_nr].status);
	}
}

static struct irq_chip sbus_irq_type = {
	.name		= "IOP",
	.startup	= sbus_startup_irq,
	.shutdown	= sbus_shutdown_irq,
	.unmask		= sbus_enable_irq,
	.mask		= sbus_disable_irq,
	.mask_ack	= sbus_ack_irq,
	.end		= sbus_end_irq,
};

static struct irqaction cascade_gs_irqaction = {
	.handler = no_action,
	.name = "GS cascade",
};

static struct irqaction cascade_sbus_irqaction = {
	.handler = no_action,
	.name = "SBUS cascade",
};

static struct irqaction cascade_intc_irqaction = {
	.handler = no_action,
	.name = "INTC cascade",
};

static struct irqaction cascade_dmac_irqaction = {
	.handler = no_action,
	.name = "DMAC cascade",
};

void __init arch_init_irq(void)
{
	int i;
	int rv;

	/* init CPU irqs */
	mips_cpu_irq_init();

	for (i = 0; i < MIPS_CPU_IRQ_BASE; i++) {
		struct irq_chip *handler;

		if (i < IRQ_DMAC) {
			handler = &intc_irq_type;
		} else if (i < IRQ_GS) {
			handler = &dmac_irq_type;
		} else if (i < IRQ_SBUS) {
			handler = &gs_irq_type;
		} else {
			handler = &sbus_irq_type;
		}
		set_irq_chip_and_handler(i, handler, handle_level_irq);
	}

	/* initialize interrupt mask */
	intc_mask = 0;
	outl(inl(INTC_MASK), INTC_MASK);
	outl(inl(INTC_STAT), INTC_STAT);
	dmac_mask = 0;
	outl(inl(DMAC_MASK), DMAC_MASK);
	gs_mask = 0;
	outl(0xff00, GS_IMR);
	outl(0x00ff, GS_CSR);
	sbus_mask = 0;
	outl((1 << 8) | (1 << 10), SBUS_SMFLG);

	/* Enable cascaded GS IRQ. */
	rv = setup_irq(IRQ_INTC_GS, &cascade_gs_irqaction);
	if (rv) {
		printk("Failed to setup GS IRQ (rv = %d).\n", rv);
	}

	/* Enable cascaded SBUS IRQ. */
	rv = setup_irq(IRQ_INTC_SBUS, &cascade_sbus_irqaction);
	if (rv) {
		printk("Failed to setup SBUS IRQ (rv = %d).\n", rv);
	}

	/* Enable INTC interrupt. */
	rv = setup_irq(IRQ_C0_INTC, &cascade_intc_irqaction);
	if (rv) {
		printk("Failed to setup INTC (rv = %d).\n", rv);
	}

	/* Enable DMAC interrupt. */
	rv = setup_irq(IRQ_C0_DMAC, &cascade_dmac_irqaction);
	if (rv) {
		printk("Failed to setup DMAC (rv = %d).\n", rv);
	}
}

static void gs_irqdispatch(void);
static void sbus_irqdispatch(void);

/*
 * INT0 (INTC interrupt)
 * interrupts 0 - 15
 */
void int0_irqdispatch(void)
{
	int i;
	unsigned long int0 = inl(INTC_STAT) & intc_mask;

	if (int0 & (1 << IRQ_INTC_GS)) {
		gs_irqdispatch();
		return;
	} else if (int0 & (1 << IRQ_INTC_SBUS)) {
		sbus_irqdispatch();
		return;
	}

	for (i = 2; i < 16; i++) {
		if ((int0 & (1 << i))) {
			do_IRQ(IRQ_INTC + i);
			break;
		}
	}
}

/*
 * INT1 (DMAC interrupt)
 * interrupts 16 - 31
 */
static void int1_irqdispatch(void)
{
	int i;
	unsigned long int1 = inl(DMAC_STAT) & dmac_mask;

	for (i = 0; i < 16; i++) {
		if ((int1 & (1 << i))) {
			do_IRQ(IRQ_DMAC + i);
			break;
		}
	}
}

/*
 * GS interrupt (INT0 cascade)
 * interrupts 32 - 39
 */
static void gs_irqdispatch(void)
{
	int i;
	unsigned long gs_int = inl(GS_CSR) & gs_mask;

	for (i = 0; i < 7; i++) {
		if ((gs_int & (1 << i))) {
			do_IRQ(IRQ_GS + i);
			break;
		}
	}
}

/*
 * SBUS interrupt (INT0 cascade)
 * interrupts 40 - 47
 */
static void sbus_irqdispatch(void)
{
	int i;
	unsigned long sbus_int;

	preempt_disable();
	sbus_int = sbus_enter_irq() & sbus_mask;
	for (i = 0; i < 7; i++) {
		if ((sbus_int & (1 << i))) {
			do_IRQ(IRQ_SBUS + i);
		}
	}
	sbus_leave_irq();
	preempt_enable_no_resched();
}

asmlinkage void plat_irq_dispatch(void)
{
	unsigned int pending = read_c0_status() & read_c0_cause();

	/*
	 * First we check for r4k counter/timer IRQ.
	 */
	if (pending & CAUSEF_IP2) {
		/* INTC interrupt. */
		int0_irqdispatch();
	} else if (pending & CAUSEF_IP3) {
		/* DMAC interrupt. */
		int1_irqdispatch();
	} else if (pending & CAUSEF_IP7) {
		/* Timer interrupt. */
		do_IRQ(IRQ_C0_IRQ7);
	} else {
		printk("plat_irq_dispatch: unknown interrupt 0x%08x pending.\n", pending);
	}
}
