/* Copyright 2010 Mega Man */
/* TBD: Unfinished state. Rework code. */
#include <linux/console.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/module.h>
#include <linux/param.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/pm.h>
#include <linux/platform_device.h>

#include <asm/bootinfo.h>
#include <asm/mach-ps2/ps2.h>
#include <asm/mach-ps2/dma.h>
#include <asm/mach-ps2/sifdefs.h>

static struct resource usb_ohci_resources[] = {
	[0] = {
		.start	= 0xbf801600,
		.end	= 0xbf801700,
		/* OHCI needs IO memory. */
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_SBUS_USB,
		.end	= IRQ_SBUS_USB,
		.flags	= IORESOURCE_IRQ,
	},
};

static u64 usb_ohci_dma_mask = 0xffffffffUL;
static struct platform_device usb_ohci_device = {
	.name		= "ps2_ohci",
	.id		= -1,
	.dev = {
		/* TBD: DMA must be allocated from IOP heap. */
		.dma_mask		= &usb_ohci_dma_mask,
		.coherent_dma_mask	= 0xffffffff,
	},
	.num_resources	= ARRAY_SIZE(usb_ohci_resources),
	.resource	= usb_ohci_resources,
};

static struct platform_device smap_device = {
	.name           = "ps2smap",
};

static struct platform_device smaprpc_device = {
	.name           = "ps2smaprpc",
};

void __init plat_mem_setup(void)
{
	printk("plat_mem_setup: TBD: Memory initialisation incomplete.\n");
#if 0
	_machine_restart = ps2_machine_restart;
	_machine_halt = ps2_machine_halt;
	pm_power_off = ps2_machine_power_off;
#endif

	/* IO port (out and in functions). */
	ioport_resource.start = 0x10000000;
	ioport_resource.end   = 0x1FFFFFFF;

	/* IO memory. */
	iomem_resource.start = 0x00000000;
	iomem_resource.end   = KSEG2 - 1;

	/* Exeception vectors. */
	add_memory_region(0x00000000, 0x00001000, BOOT_MEM_RAM);
	/* Reserved for SBIOS. */
	add_memory_region(0x00001000, 0x0000F000, BOOT_MEM_RESERVED);
	/* Free memory. */
	add_memory_region(0x00010000, 0x01ff0000, BOOT_MEM_RAM);

	/* Set base address of outb()/outw()/outl()/outq()/
	 * inb()/inw()/inl()/inq().
	 * This memory region is uncached.
	 */
	set_io_port_base(0xA0000000);

#ifdef CONFIG_SERIAL_PS2_SBIOS_DEFAULT
	/* Use PS2 SBIOS as default console. */
	add_preferred_console(PS2_SBIOS_SERIAL_DEVICE_NAME, 0, NULL);
#endif
}

void ps2_dev_init(void)
{
	ps2dma_init();
	ps2sif_init();
	platform_device_register(&usb_ohci_device);

	switch (ps2_pccard_present) {
	case 0x0100:
		platform_device_register(&smap_device);
		break;
	case 0x0200:
		platform_device_register(&smaprpc_device);
		break;
	default:
		printk("No SMAP network device found.");
		break;
	}
}
