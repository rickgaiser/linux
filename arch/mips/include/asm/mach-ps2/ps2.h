/* Copyright 2010 Mega Man */
#ifndef __ASM_PS2_PS2_H
#define __ASM_PS2_PS2_H

#include <linux/kernel.h>

/* Device name of PS2 SBIOS serial device. */
#define PS2_SBIOS_SERIAL_DEVICE_NAME "ttyS"

/* Base address for hardware. */
#define PS2_HW_BASE 0x10000000

/* Base address for IOP memory. */
#define PS2_IOP_HEAP_BASE 0xbc000000

extern int ps2_pccard_present;
extern int ps2_pcic_type;
extern struct ps2_sysconf *ps2_sysconf;

extern void prom_putchar(char);
extern int ps2_printf(const char *fmt, ...);
void ps2_dev_init(void);
extern int ps2sif_initiopheap(void);
extern int ps2rtc_init(void);
void ps2_halt(int mode);
int ps2_powerbutton_init(void);
int ps2_powerbutton_enable_auto_shutoff(int enable_auto_shutoff);

#endif
