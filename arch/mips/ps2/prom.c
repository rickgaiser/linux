/* Copyright 2010-2012 Mega Man */
/* TBD: Unfinished state. Rework code. */
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/bootmem.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include <asm/addrspace.h>
#include <asm/bootinfo.h>
#include <asm/sections.h>
#include <asm/mach-ps2/bootinfo.h>
#include <asm/mach-ps2/ps2.h>
#include <asm/mach-ps2/sbios.h>

#define SBIOS_BASE	0x80001000
#define SBIOS_SIGNATURE	4

int ps2_pccard_present;
int ps2_pcic_type;
struct ps2_sysconf *ps2_sysconf;

EXPORT_SYMBOL(ps2_sysconf);
EXPORT_SYMBOL(ps2_pcic_type);
EXPORT_SYMBOL(ps2_pccard_present);

static struct ps2_bootinfo ps2_bootinfox;
struct ps2_bootinfo *ps2_bootinfo = &ps2_bootinfox;

static void sbios_prints(const char *text)
{
	printk("SBIOS: %s", text);
}

void __init prom_init(void)
{
	struct ps2_bootinfo *bootinfo;
	int version;

	memset(&ps2_bootinfox, 0, sizeof(struct ps2_bootinfo));
	ps2_bootinfox.sbios_base = SBIOS_BASE;

	if (*(uint32_t *)(SBIOS_BASE + SBIOS_SIGNATURE) == 0x62325350) {
	    bootinfo = (struct ps2_bootinfo *)phys_to_virt(PS2_BOOTINFO_OLDADDR);
	    memcpy(ps2_bootinfo, bootinfo, PS2_BOOTINFO_OLDSIZE);
	}

	/* get command line parameters */
	if (ps2_bootinfo->opt_string != 0) {
	    strncpy(arcs_cmdline, (const char *) ((uintptr_t) ps2_bootinfo->opt_string), COMMAND_LINE_SIZE);
	    arcs_cmdline[COMMAND_LINE_SIZE - 1] = '\0';
	}

	ps2_pccard_present = ps2_bootinfo->pccard_type;
	ps2_pcic_type = ps2_bootinfo->pcic_type;
	ps2_sysconf = &ps2_bootinfo->sysconf;

	version = sbios(SB_GETVER, 0);
	printk("PlayStation 2 SIF BIOS: %04x\n", version);

	sbios(SB_SET_PRINTS_CALLBACK, sbios_prints);

	/* Remove restriction to /BWLINUX path in mc calls. */
	/* This patches the string in SBIOS. */
	if (version == 0x200) {
		/* Patch beta kit */
		*((volatile unsigned char *)0x80007c20) = 0;
	} else if (version == 0x250) {
		/* Patch 1.0 kit */
		*((volatile unsigned char *)0x800081b0) = 0;
	}
}

void __init prom_free_prom_memory(void)
{
	unsigned long end;

	/*
	 * Free everything below the kernel itself but leave
	 * the first page reserved for the exception handlers.
	 * The first 0x10000 Bytes contain the SBIOS.
	 */

	end = __pa(&_text);

	/* TBD: Check if this is necessary or wrong. */
	// TBD: free_init_pages("unused PROM memory", 0x10000, end);
}
