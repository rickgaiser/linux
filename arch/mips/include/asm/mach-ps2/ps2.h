/* Copyright 2010 Mega Man */
#ifndef __ASM_PS2_PS2_H
#define __ASM_PS2_PS2_H

/* Device name of PS2 SBIOS serial device. */
#define PS2_SBIOS_SERIAL_DEVICE_NAME "ttyS"

extern int ps2_pccard_present;
extern int ps2_pcic_type;
extern struct ps2_sysconf *ps2_sysconf;

extern void prom_putchar(char);

#endif
