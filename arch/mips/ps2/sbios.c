/* Copyright 2010 Mega Man */
/* TBD: Unfinished state. Rework code. */
#include <linux/module.h>

#include <asm/mach-ps2/sbios.h>

#define SBIOS_BASE  0x80001000

typedef int (sbios_t) (int sbcall, void *arg);

EXPORT_SYMBOL(ps2_sbios);

int ps2_sbios(int sbcall, void *arg)
{
	sbios_t *sbios;

	sbios = *((sbios_t **) SBIOS_BASE);

	return sbios(sbcall, arg);
}
