/* Copyright 2010 Mega Man */
/* TBD: Unfinished state. Rework code. */
#include <linux/console.h>
#include <linux/init.h>

#include <asm/mach-ps2/sbios.h>

int prom_putchar(char c)
{
	struct sb_putchar_arg putc_arg;

	putc_arg.c = c;
	ps2_sbios(SB_PUTCHAR, &putc_arg);

	return 1;
}
