/* Copyright 2010 Mega Man */
/* TBD: Unfinished state. Rework code. */
#include <linux/kernel.h>
#include <linux/console.h>
#include <linux/init.h>

#include <asm/mach-ps2/ps2.h>
#include <asm/mach-ps2/sbios.h>

void prom_putchar(char c)
{
	struct sb_putchar_arg putc_arg;

	putc_arg.c = c;
	sbios(SB_PUTCHAR, &putc_arg);
}

int ps2_printf(const char *fmt, ...)
{
	char buffer[60];
	va_list args;
	int r;
	int i;

	va_start(args, fmt);
	r = vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end(args);
	for (i = 0; i < r; i++) {
		prom_putchar(buffer[i]);
	}
	return r;
}
