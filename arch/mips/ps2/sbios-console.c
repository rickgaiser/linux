/* Copyright 2010 Mega Man */
/* TBD: Unfinished state. Rework code. */
#include <linux/console.h>
#include <linux/init.h>

#include <asm/mach-ps2/sbios.h>

int prom_putchar(char c)
{
	tge_sbcall_putc_arg_t putc_arg;

	putc_arg.c = c;
	ps2_sbios(3, &putc_arg);

	return 1;
}
