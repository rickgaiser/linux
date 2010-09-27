/* Copyright 2010 Mega Man */
/* TBD: Unfinished state. Rework code. */
#ifndef _PS2_SBIOS_H_
#define _PS2_SBIOS_H_

#include <linux/types.h>

typedef struct {
	uint32_t c;
} tge_sbcall_putc_arg_t;

int ps2_sbios(int sbcall, void *arg);

#endif
