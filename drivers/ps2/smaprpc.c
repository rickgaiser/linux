/*
 *  PlayStation 2 Ethernet device driver
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

#include <linux/platform_device.h>
#include <linux/kthread.h>

#include <asm/mach-ps2/sbios.h>
#include <asm/mach-ps2/iopmodules.h>

#include "smaprpc.h"

#define SMAP_BIND_RPC_ID 0x0815e000

#define SMAP_CMD_SEND 1
#define SMAP_CMD_SET_BUFFER 2
#define SMAP_CMD_GET_MAC_ADDR 3
#define SMAP_CMD_SET_MAC_ADDR 4

#define SIF_SMAP_RECEIVE 0x07

#define CMD_SMAP_RW 0x17 // is this CMD number free?
struct ps2_smap_cmd_rw {
	struct t_SifCmdHeader sifcmd;
	u32 write;
	u32 addr;
	u32 size;
	u32 callback;
	u32 spare[16-4];
};

typedef struct {
	struct t_SifCmdHeader sifcmd;
	u32 payload;
	u32 size;
} iop_sifCmdSmapIrq_t;

static u32 smap_rpc_data[2048] __attribute__ ((aligned(64)));
static u8 smap_cmd_buffer[64] __attribute__ ((aligned(64)));
#define DATA_BUFFER_SIZE (8*1024)
u8 _data_buffer[DATA_BUFFER_SIZE] __attribute((aligned(64)));
static u8 * _data_buffer_pointer = _data_buffer;
#define DATA_BUFFER_USED()	(_data_buffer_pointer-_data_buffer)
#define DATA_BUFFER_FREE()	(DATA_BUFFER_SIZE-DATA_BUFFER_USED())
#define DATA_BUFFER_RESET()	_data_buffer_pointer = _data_buffer

/*--------------------------------------------------------------------------*/

static int smaprpc_start_xmit(struct sk_buff *skb, struct net_device *net_dev);

static struct net_device_stats *smaprpc_get_stats(struct net_device *net_dev);

static int smaprpc_open(struct net_device *net_dev);

static int smaprpc_close(struct net_device *net_dev);

static int smaprpc_ioctl(struct net_device *net_dev, struct ifreq *ifr,
	int cmd);

static void smaprpc_rpcend_notify(void *arg);

static void smaprpc_rpc_setup(struct smaprpc_chan *smap);

/*--------------------------------------------------------------------------*/

#define DMA_SIZE_ALIGN(s) (((s)+15)&~15)
static void smaprpc_dma_write(struct smaprpc_chan *smap, dma_addr_t ee_addr, size_t datasize, u32 iop_addr)
{
	struct ps2_smap_cmd_rw *cmd = (struct ps2_smap_cmd_rw *)smap_cmd_buffer;

	cmd->write    = 1;
	cmd->addr     = iop_addr;
	cmd->size     = datasize;
	cmd->callback = 1;

	while (ps2sif_sendcmd(CMD_SMAP_RW, cmd, DMA_SIZE_ALIGN(sizeof(struct ps2_smap_cmd_rw))
			, (void *)ee_addr, (void *)iop_addr, DMA_SIZE_ALIGN(datasize)) == 0) {
		cpu_relax();
	}
}

/* return value: 0 if success, !0 if error */
static int smaprpc_start_xmit(struct sk_buff *skb, struct net_device *net_dev)
{
	struct smaprpc_chan *smap = netdev_priv(net_dev);
	dma_addr_t ee_addr;
	u32 offset;
	unsigned long flags;

	/* Reset the ring-buffer if needed */
	if (DATA_BUFFER_FREE() < skb->len)
		DATA_BUFFER_RESET();
	offset = _data_buffer_pointer - _data_buffer;

	/* Copy data to ring-buffer */
	memcpy(_data_buffer_pointer, skb->data, skb->len);

	/* Flush caches */
	ee_addr = dma_map_single(NULL, _data_buffer_pointer, skb->len, DMA_TO_DEVICE);

	/* Start async data transfer to IOP */
	smaprpc_dma_write(smap, ee_addr, skb->len, smap->iop_data_buffer_addr + offset);

	spin_lock_irqsave(&smap->spinlock, flags);
	smap->tx_queued++;
	if (smap->tx_queued >= 5) {
		//printk("%s: TX strt -> sleep(%d)\n", smap->net_dev->name, smap->tx_queued);
		netif_stop_queue(smap->net_dev);
	}
	spin_unlock_irqrestore(&smap->spinlock, flags);

	/* Advance the ring-buffer */
	_data_buffer_pointer += DMA_SIZE_ALIGN(skb->len);

	dev_kfree_skb(skb);

	return 0;
}

static void handleSmapTXDone(void *data, void *arg)
{
	//struct ps2_smap_cmd_rw *cmd = (struct ps2_smap_cmd_rw *)data;
	struct smaprpc_chan *smap = (struct smaprpc_chan *)arg;

	if (smap->tx_queued == 0) {
		printk("%s: TX done -> error, received too many interrupts\n", smap->net_dev->name);
	}
	else {
		smap->tx_queued--;
	}

	if ((smap->tx_queued <= 3) && netif_queue_stopped(smap->net_dev)) {
		//printk("%s: TX done -> wake (%d)\n", smap->net_dev->name, smap->tx_queued);
		netif_wake_queue(smap->net_dev);
	}
}

/*--------------------------------------------------------------------------*/

static struct net_device_stats *smaprpc_get_stats(struct net_device *net_dev)
{
	struct smaprpc_chan *smap = netdev_priv(net_dev);

	return (&smap->net_stats);
}

static int smaprpc_open(struct net_device *net_dev)
{
	struct smaprpc_chan *smap = netdev_priv(net_dev);

	smap->flags |= SMAPRPC_F_OPENED;

	return (0);					/* success */
}

static int
smaprpc_set_mac_address(struct net_device *net_dev, void *p)
{
	int rv;
	struct completion compl;
	struct sockaddr *addr = p;
	struct smaprpc_chan *smap = netdev_priv(net_dev);

	if (netif_running(net_dev))
		return -EBUSY;

	init_completion(&compl);
	down(&smap->smap_rpc_sema);

		memset(smap_rpc_data, 0, 32);
		memcpy(smap_rpc_data, addr->sa_data, ETH_ALEN);

		do {
			rv = ps2sif_callrpc(&smap->cd_smap_rpc, SMAP_CMD_SET_MAC_ADDR,
				SIF_RPCM_NOWAIT,
				(void *) smap_rpc_data, 32, // send
				NULL, 0, // receive
				(ps2sif_endfunc_t) smaprpc_rpcend_notify, (void *) &compl);
		} while (rv == -E_SIF_PKT_ALLOC);

		if (rv != 0) {
			printk("%s: SMAP_CMD_SET_MAC_ADDR failed, (%d)\n", smap->net_dev->name,
				rv);
		} else {
			wait_for_completion(&compl);
			memcpy(net_dev->dev_addr, addr->sa_data, ETH_ALEN);
		}

	up(&smap->smap_rpc_sema);

	return 0;
}

static int smaprpc_close(struct net_device *net_dev)
{
	struct smaprpc_chan *smap = netdev_priv(net_dev);

	unsigned long flags;

	spin_lock_irqsave(&smap->spinlock, flags);
	smap->flags &= ~SMAPRPC_F_OPENED;
	spin_unlock_irqrestore(&smap->spinlock, flags);

	return (0);					/* success */
}

/*--------------------------------------------------------------------------*/

static int smaprpc_ioctl(struct net_device *net_dev, struct ifreq *ifr, int cmd)
{
	int retval = 0;

	printk("%s: PlayStation 2 SMAP ioctl %d\n", net_dev->name, cmd);

	switch (cmd) {
	default:
		retval = -EOPNOTSUPP;
		break;
	}

	return (retval);
}

static void smaprpc_rpc_setup(struct smaprpc_chan *smap)
{
	int loop;

	int rv;

	volatile int j;

	struct completion compl;

	if (smap->rpc_initialized) {
		return;
	}
	init_completion(&compl);

	/* bind smaprpc.irx module */
	for (loop = 100; loop; loop--) {
		rv = ps2sif_bindrpc(&smap->cd_smap_rpc, SMAP_BIND_RPC_ID,
			SIF_RPCM_NOWAIT, smaprpc_rpcend_notify, (void *) &compl);
		if (rv < 0) {
			printk("%s: smap rpc setup: bind rv = %d.\n", smap->net_dev->name,
				rv);
			break;
		}
		wait_for_completion(&compl);
		if (smap->cd_smap_rpc.serve != 0)
			break;
		j = 0x010000;
		while (j--);
	}
	if (smap->cd_smap_rpc.serve == 0) {
		printk("%s: smap rpc setup: bind error 1, network will not work\n",
			smap->net_dev->name);
		return;
	}

	memset(smap_rpc_data, 0, 32);
	do {
		rv = ps2sif_callrpc(&smap->cd_smap_rpc, SMAP_CMD_GET_MAC_ADDR,
			SIF_RPCM_NOWAIT,
			(void *) smap_rpc_data, 32,
			smap_rpc_data, sizeof(smap_rpc_data),
			(ps2sif_endfunc_t) smaprpc_rpcend_notify, (void *) &compl);
	} while (rv == -E_SIF_PKT_ALLOC);
	if (rv != 0) {
		printk("%s: SMAP_CMD_GET_MAC_ADDR failed, (%d)\n", smap->net_dev->name,
			rv);
	} else {
		wait_for_completion(&compl);
		memcpy(smap->net_dev->dev_addr, &smap_rpc_data[1], ETH_ALEN);
		printk("%s: MAC %02x:%02x:%02x:%02x:%02x:%02x\n", smap->net_dev->name,
			smap->net_dev->dev_addr[0],
			smap->net_dev->dev_addr[1],
			smap->net_dev->dev_addr[2],
			smap->net_dev->dev_addr[3],
			smap->net_dev->dev_addr[4], smap->net_dev->dev_addr[5]);
	}

	smap->shared_size = 32 * 1024;
	smap->shared_addr = kmalloc(smap->shared_size, GFP_KERNEL);
	if (smap->shared_addr != NULL) {
		smap_rpc_data[0] = virt_to_phys(smap->shared_addr);
		smap_rpc_data[1] = smap->shared_size;
		do {
			rv = ps2sif_callrpc(&smap->cd_smap_rpc, SMAP_CMD_SET_BUFFER,
				SIF_RPCM_NOWAIT,
				(void *) smap_rpc_data, 32,
				smap_rpc_data, 3*4,
				(ps2sif_endfunc_t) smaprpc_rpcend_notify, (void *) &compl);
		} while (rv == -E_SIF_PKT_ALLOC);
		if (rv != 0) {
			printk("%s: SMAP_CMD_SET_BUFFER failed, (rv = %d). Receive will not work.\n",
				smap->net_dev->name, rv);
		} else {
			wait_for_completion(&compl);
			if (smap_rpc_data[0] != 0) {
				printk("%s: SMAP_CMD_SET_BUFFER failed, (0x%08x). Receive will not work.\n",
					smap->net_dev->name, smap_rpc_data[0]);
			}
			smap->iop_data_buffer_addr = smap_rpc_data[1];
			smap->iop_data_buffer_size = smap_rpc_data[2];
			dev_info(&smap->net_dev->dev, "rpc setup: iop cmd buffer @ %pad, size = %d\n", (void *)smap->iop_data_buffer_addr, smap->iop_data_buffer_size);
		}
	} else {
		printk("%s: Failed to allocate receive buffer. Receive will not work.\n",
			smap->net_dev->name);
	}
	smap->rpc_initialized = -1;
}

static void smaprpc_rpcend_notify(void *arg)
{
	complete((struct completion *) arg);
	return;
}

static void handleSmapIRQ(iop_sifCmdSmapIrq_t * pkt, void *arg)
{
	struct smaprpc_chan *smap = (struct smaprpc_chan *) arg;

	struct sk_buff *skb;

	u8 *data;

	data = phys_to_virt(pkt->payload);
	dma_cache_inv((unsigned long) data, pkt->size);

	skb = netdev_alloc_skb(smap->net_dev, pkt->size + 2);
	if (skb == NULL) {
		printk("%s:handleSmapIRQ, skb alloc error\n", smap->net_dev->name);
		return;
	}
	skb_reserve(skb, 2);		/* 16 byte align the data fields */
	skb_copy_to_linear_data(skb, data, pkt->size);
	skb_put(skb, pkt->size);
	skb->dev = smap->net_dev;
	skb->protocol = eth_type_trans(skb, smap->net_dev);
	smap->net_dev->last_rx = jiffies;
	netif_rx(skb);
}

extern int ps2_pccard_present;

static const struct net_device_ops smaprpc_netdev_ops = {
	.ndo_open		= smaprpc_open,
	.ndo_stop		= smaprpc_close,
	.ndo_do_ioctl		= smaprpc_ioctl,
	.ndo_start_xmit		= smaprpc_start_xmit,
	.ndo_get_stats		= smaprpc_get_stats,
	.ndo_validate_addr	= eth_validate_addr,
	.ndo_set_mac_address 	= smaprpc_set_mac_address,
	.ndo_change_mtu		= eth_change_mtu,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller = NULL,
#endif
};

static int __devinit smaprpc_probe(struct platform_device *dev)
{
	struct net_device *net_dev = NULL;
	struct smaprpc_chan *smap = NULL;

	if (ps2_pccard_present == 0) {
		printk("PlayStation 2 Ethernet device NOT present.\n");
		return (-ENODEV);
	}

	if (load_module_firmware("ps2/smaprpc.irx", 0) < 0) {
		printk("ps2smaprpc: loading ps2/smaprpc.irx failed\n");
		return -ENODEV;
	}

	net_dev = alloc_etherdev(sizeof(struct smaprpc_chan));
	if (!net_dev) {
		return -ENOMEM;
	}

	SET_NETDEV_DEV(net_dev, &dev->dev);
	platform_set_drvdata(dev, net_dev);

	smap = netdev_priv(net_dev);

	/* clear control structure */
	memset(smap, 0, sizeof(struct smaprpc_chan));

	/* init network device structure */
	ether_setup(net_dev);
	smap->net_dev = net_dev;
	smap->tx_queued = 0;

	net_dev->netdev_ops = &smaprpc_netdev_ops;

	spin_lock_init(&smap->spinlock);
	init_MUTEX(&smap->smap_rpc_sema);

	if (ps2sif_addcmdhandler(SIF_SMAP_RECEIVE, handleSmapIRQ, smap) < 0) {
		printk("Failed to initialize smap IRQ handler. Receive will not work.\n");
	}

	if (ps2sif_addcmdhandler(CMD_SMAP_RW, handleSmapTXDone, smap) < 0) {
		printk("Failed to initialize smap IRQ handler. Receive will not work.\n");
	}

	if (register_netdev(net_dev)) {
		goto error;
	}
	smaprpc_rpc_setup(smap);
	if (smap->rpc_initialized) {
		printk("PlayStation 2 SMAP(Ethernet) rpc device driver.\n");
		return (0);				/* success */
	}
	unregister_netdev(net_dev);
error:
	printk("PlayStation 2 SMAP(Ethernet) rpc device not found.\n");
	free_netdev(net_dev);
	return (-ENODEV);
}

static int __devexit smaprpc_driver_remove(struct platform_device *pdev)
{
	struct net_device *net_dev = platform_get_drvdata(pdev);
	struct smaprpc_chan *smap = netdev_priv(net_dev);

	if (smap->rpc_initialized) {
		/* Remove interrupt handler. */
		struct sb_sifremovecmdhandler_arg param;

		param.fid = SIF_SMAP_RECEIVE;
		if (sbios(SB_SIFREMOVECMDHANDLER, &param) < 0) {
			printk("Failed to remove smap IRQ handler.\n");
		}
	}

	if (smap->shared_addr != NULL) {
		kfree(smap->shared_addr);
	}

	if (net_dev->flags & IFF_UP)
		dev_close(net_dev);

	unregister_netdev(net_dev);

	/* XXX: Disable device. */

	free_netdev(net_dev);
	return 0;
}

static struct platform_driver smap_driver = {
	.probe	= smaprpc_probe,
	.remove	= __devexit_p(smaprpc_driver_remove),
	.driver	= {
		.name	= "ps2smaprpc",
		.owner	= THIS_MODULE,
	},
};

static int __init smaprpc_init(void)
{
	return platform_driver_register(&smap_driver);
}

static void __exit smaprpc_exit(void)
{
	platform_driver_unregister(&smap_driver);
	return;
}

module_init(smaprpc_init);
module_exit(smaprpc_exit);

MODULE_AUTHOR("Juergen Urban");
MODULE_DESCRIPTION("PlayStation 2 SMAP(Ethernet) rpc device driver");
MODULE_LICENSE("GPL");
