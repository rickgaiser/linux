/* $Id$
 *
 *  Copyright (C) 1991, 1992, 1995  Linus Torvalds
 *  Copyright (C) 1996, 1997, 1998  Ralf Baechle
 *  Copyright (C) 2000, 2002  Sony Computer Entertainment Inc.
 *  Copyright (C) 2001  Paul Mundt <lethal@chaoticdreams.org>
 *  Copyright (C) 2010  Mega Man
 *
 * This file contains the time handling details for PC-style clocks as
 * found in some MIPS systems.
 */
/* TBD: Unfinished state. Rework code. */
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel_stat.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/timex.h>

#include <asm/bootinfo.h>
#include <asm/time.h>
#include <asm/mipsregs.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach-ps2/irq.h>
#include <asm/mach-ps2/ps2.h>

#define CPU_FREQ  294912000		/* CPU clock frequency (Hz) */
#define BUS_CLOCK (CPU_FREQ/2)		/* bus clock frequency (Hz) */
#define TM_COMPARE_VALUE  (BUS_CLOCK/256/HZ)	/* to generate 100Hz */
#define USECS_PER_JIFFY (1000000/HZ)

#define T0_BASE  0x10000000
#define T0_COUNT (T0_BASE + 0x00)
#define T0_MODE  (T0_BASE + 0x10)
#define T0_COMP  (T0_BASE + 0x20)

static uint32_t last_cycle_count;
static uint32_t timer_intr_delay;

/**
 * 	ps2_do_gettimeoffset - Get Time Offset
 *
 * 	Returns the time duration since the last timer
 * 	interrupt in usecs.
 */
static unsigned long ps2_do_gettimeoffset(void)
{
	unsigned int count;
	int delay;
	unsigned long res;

	count = read_c0_count();
	count -= last_cycle_count;
	count = (count * 1000 + (CPU_FREQ / 1000 / 2)) / (CPU_FREQ / 1000);
	delay = (timer_intr_delay * 10000 + (TM_COMPARE_VALUE / 2)) / TM_COMPARE_VALUE;
	res = delay + count;

	/*
	 * Due to possible jiffies inconsistencies, we need to check
	 * the result so that we'll get a timer that is monotonic.
	 */
	if (res >= USECS_PER_JIFFY)
		res = USECS_PER_JIFFY-1;

	return res;
}

u32 arch_gettimeoffset(void)
{
	// TBD: Time seems to be too fast. "ping" on console has often 10.0 ms.
	return ps2_do_gettimeoffset() * NSEC_PER_USEC;
}

unsigned long long sched_clock(void)
{
	unsigned long long rv;

	rv = (unsigned long long) (jiffies - INITIAL_JIFFIES);
	rv *= (unsigned long long) (NSEC_PER_SEC / HZ);
	rv += ((unsigned long long) NSEC_PER_USEC) * ((unsigned long long) ps2_do_gettimeoffset());
	return rv;
}

/**
 * 	ps2_timer_interrupt - Timer Interrupt Routine
 *
 * 	@regs:   registers as they appear on the stack
 *	         during a syscall/exception.
 * 	
 * 	Timer interrupt routine, wraps the generic timer_interrupt() but
 * 	sets the timer interrupt delay and clears interrupts first.
 */
static irqreturn_t ps2_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *cd = dev_id;

	/* Set the timer interrupt delay */
	timer_intr_delay = inl(T0_COUNT);
	last_cycle_count = read_c0_count();

	/* Clear the interrupt */
	outl(inl(T0_MODE), T0_MODE);

	cd->event_handler(cd);

	return IRQ_HANDLED;
}

static void timer0_set_mode(enum clock_event_mode mode,
                          struct clock_event_device *evt)
{
	switch (mode) {
	case CLOCK_EVT_MODE_PERIODIC:
		/* setup 100Hz interval timer */
		outl(0, T0_COUNT);
		outl(TM_COMPARE_VALUE, T0_COMP);

		/* busclk / 256, zret, cue, cmpe, equf */
		outl(2 | (1 << 6) | (1 << 7) | (1 << 8) | (1 << 10), T0_MODE);
		break;
	case CLOCK_EVT_MODE_ONESHOT:
	case CLOCK_EVT_MODE_UNUSED:
		break;
	case CLOCK_EVT_MODE_SHUTDOWN:
		/* Stop timer. */
		outl(0, T0_MODE);
		break;
	case CLOCK_EVT_MODE_RESUME:
		break;
	}
}

static struct irqaction timer0_irqaction = {
	.handler	= ps2_timer_interrupt,
	.flags		= IRQF_DISABLED | IRQF_PERCPU | IRQF_TIMER,
	.name		= "intc-timer0",
};

static struct clock_event_device timer0_clockevent_device = {
	.name		= "timer0",
	.features	= CLOCK_EVT_FEAT_PERIODIC,

	/* .mult, .shift, .max_delta_ns and .min_delta_ns left uninitialized */

	.rating		= 300, /* TBD: Check value. */
	.irq		= IRQ_INTC_TIMER0,
	.set_mode	= timer0_set_mode,
};

void __init plat_time_init(void)
{
	/* Setup interrupt */
	struct clock_event_device *cd = &timer0_clockevent_device;
	struct irqaction *action = &timer0_irqaction;
	unsigned int cpu = smp_processor_id();

	cd->cpumask = cpumask_of(cpu);
	clockevents_register_device(cd);
	action->dev_id = cd;
	setup_irq(IRQ_INTC_TIMER0, &timer0_irqaction);
}
