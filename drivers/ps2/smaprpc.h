/*
 *  PlayStation 2 Ethernet device driver header file
 *
 *  Copyright (C) 2000-2002 Sony Computer Entertainment Inc.
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

#ifndef	__SMAP_H__
#define	__SMAP_H__

#include <linux/version.h>

#include <linux/module.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/semaphore.h>

#include <asm/io.h>
#include <asm/pgtable.h>
#include <asm/mach-ps2/sifdefs.h>
#include <asm/mach-ps2/dmarelay.h>

/*
 * SMAP control structure(smap channel)
 */
struct smaprpc_chan {
	spinlock_t spinlock;
	struct net_device *net_dev;
	u_int32_t flags;
	u_int32_t irq;
	struct net_device_stats net_stats;

	ps2sif_clientdata_t cd_smap_rpc;
	int rpc_initialized;
	struct semaphore smap_rpc_sema;

	void *shared_addr;
	unsigned int shared_size;

	u32 iop_data_buffer_addr;
	u32 iop_data_buffer_size;
	volatile u32 tx_queued;
};

/* flags */
#define	SMAPRPC_F_OPENED		(1<<0)

#endif /* __SMAP_H__ */
