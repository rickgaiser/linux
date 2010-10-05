/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2010 Mega Man
 */
#include <linux/console.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>

#include <asm/setup.h>
#include <asm/mach-ps2/sbios.h>

static void ps2sbios_console_write(struct console *con, const char *s, unsigned n);
static int ps2sbios_console_read(struct console *con, char *s, unsigned n);
static int ps2sbios_console_setup(struct console *con, char *options);

static struct uart_driver ps2sbioscon_reg = {
	.owner			= THIS_MODULE,
	.driver_name	= "ps2sbioscon",
	.dev_name		= "ttyS",
	.major			= TTY_MAJOR,
	.minor			= 64,
	.nr				= 1,
};

static struct console ps2sbios_console = {
	.name		= "ttyS",
	.write		= ps2sbios_console_write,
	.read		= ps2sbios_console_read,
	.device		= uart_console_device,
	.setup		= ps2sbios_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &ps2sbioscon_reg,
};

static int ps2sbios_console_setup(struct console *con, char *options)
{
	return 0;
}

static void ps2sbios_putc(char c)
{
	struct sb_putchar_arg putc_arg;

	putc_arg.c = c;
	ps2_sbios(SB_PUTCHAR, &putc_arg);
}

static void ps2sbios_console_write(struct console *con, const char *s, unsigned n)
{
	while (n-- && *s) {
		if (*s == '\n')
			ps2sbios_putc('\r');
		ps2sbios_putc(*s);
		s++;
	}
}

static int ps2sbios_console_read(struct console *con, char *s, unsigned n)
{
	return 0;
}

static int __init ps2sbios_console_init(void)
{
	/* Take over console after early boot console. */
	register_console(&ps2sbios_console);
	return 0;
}

console_initcall(ps2sbios_console_init);
