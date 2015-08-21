/*
 *  PlayStation 2 DMA
 *
 *  Copyright (C) 2000-2001 Sony Computer Entertainment Inc.
 *  Copyright (C) 2010-2013 Juergen Urban
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __ASM_PS2_DMA_H
#define __ASM_PS2_DMA_H

#include <linux/spinlock.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <asm/types.h>
#include <asm/io.h>

//#define DEBUG
#ifdef DEBUG
#define DPRINT(fmt, args...) \
	printk(__FILE__ ": " fmt, ## args)
#define DSPRINT(fmt, args...) \
	prom_printf(__FILE__ ": " fmt, ## args)
#else
#define DPRINT(fmt, args...)
#define DSPRINT(fmt, args...)
#endif

#define DMA_VIF0	0
#define DMA_VIF1	1
#define DMA_GIF		2
#define DMA_IPU_from	3
#define DMA_IPU_to	4
#define DMA_SPR_from	5
#define DMA_SPR_to	6

#define DMA_SENDCH	0
#define DMA_RECVCH	1

#define DMA_TRUNIT	16
#define DMA_ALIGN(x)	((__typeof__(x))(((unsigned long)(x) + (DMA_TRUNIT - 1)) & ~(DMA_TRUNIT - 1)))

#define DMA_TRUNIT_IMG		128
#define DMA_ALIGN_IMG(x)	((__typeof__(x))(((unsigned long)(x) + (DMA_TRUNIT_IMG - 1)) & ~(DMA_TRUNIT_IMG - 1)))

#define DMA_TIMEOUT		(HZ / 2)
#define DMA_POLLING_TIMEOUT	1500000

/* DMA registers */

#define PS2_D_STAT	0x1000e010
#define PS2_D_ENABLER	0x1000f520
#define PS2_D_ENABLEW	0x1000f590

#if 0
#define PS2_Dn_CHCR	0x0000
#define PS2_Dn_MADR	0x0010
#define PS2_Dn_QWC	0x0020
#define PS2_Dn_TADR	0x0030
#define PS2_Dn_SADR	0x0080

#define CHCR_STOP	0x0000
#define CHCR_SENDN	0x0101
#define CHCR_SENDC	0x0105
#define CHCR_SENDC_TTE	0x0145
#define CHCR_RECVN	0x0100

#define READDMAREG(ch, x)		inl((ch)->base + (x))
#define WRITEDMAREG(ch, x, value)	outl((value), (ch)->base + (x))
#define DMABREAK(ch)	\
    do { \
	 outl(inl(PS2_D_ENABLER) | (1 << 16), PS2_D_ENABLEW); \
	 READDMAREG((ch), PS2_Dn_CHCR); \
	 READDMAREG((ch), PS2_Dn_CHCR); \
	 WRITEDMAREG((ch), PS2_Dn_CHCR, CHCR_STOP); \
	 outl(inl(PS2_D_ENABLER) & ~(1 << 16), PS2_D_ENABLEW); } while (0)
#define IS_DMA_RUNNING(ch)	((READDMAREG((ch), PS2_Dn_CHCR) & 0x0100) != 0)
#endif

#define DMATAG_SET(qwc, id, addr)	\
	((u64)(qwc) | ((u64)(id) << 16) | ((u64)(addr) << 32))
#define DMATAG_REFE	0x0000
#define DMATAG_CNT	0x1000
#define DMATAG_NEXT	0x2000
#define DMATAG_REF	0x3000
#define DMATAG_REFS	0x4000
#define DMATAG_CALL	0x5000
#define DMATAG_RET	0x6000
#define DMATAG_END	0x7000

/* DMA channel structures */

struct dma_channel {
    int irq;				/* DMA interrupt IRQ # */
    unsigned long base;			/* DMA register base addr */
    int direction;			/* data direction */
    int isspr;				/* true if DMA for scratchpad RAM */
    char *device;			/* request_irq() device name */
};

extern struct dma_channel ps2dma_channels[];

#endif /* __ASM_PS2_DMA_H */
