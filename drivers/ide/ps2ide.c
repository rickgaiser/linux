/*
 *  Sony Playstation 2 IDE Driver
 *
 *     Copyright (C) 2011 by Mega Man
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License.  See the file COPYING in the main directory of this archive for
 *  more details.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/ide.h>

#define PS2_HDD_BASE 0xb4000040

static void __init ps2ide_setup_ports(struct ide_hw *hw, unsigned long base,
				      int irq)
{
	int i;

	memset(hw, 0, sizeof(*hw));

	for (i = 0; i < 8; i++)
		hw->io_ports_array[i] = base + i * 2;

	hw->io_ports.ctl_addr = base + 0x1c;

	hw->irq = irq;
}

static const struct ide_port_info ps2ide_port_info = {
	.host_flags		= IDE_HFLAG_MMIO | IDE_HFLAG_NO_DMA,
	.irq_flags		= IRQF_SHARED,
	.chipset		= ide_generic,
};

static int __init ps2ide_init(void)
{
	struct ide_hw hw, *hws[] = { &hw };
	struct ide_port_info d = ps2ide_port_info;

	if (ps2_pccard_present == 0x0100) {
		printk(KERN_INFO "ide: Sony Playstation 2 IDE controller\n");
		ps2ide_setup_ports(&hw, PS2_HDD_BASE, IRQ_SBUS_PCIC);
		/* TBD: Add DMA support. */
		return ide_host_add(&d, hws, 1, NULL);
	} else {
		return -ENODEV;
	}
}

module_init(ps2ide_init);

MODULE_LICENSE("GPL");
