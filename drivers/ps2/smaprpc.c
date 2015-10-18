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

#include "smaprpc.h"

#define SMAP_BIND_RPC_ID 0x0815e000

#define SMAP_CMD_SEND 1
#define SMAP_CMD_SET_BUFFER 2
#define SMAP_CMD_GET_MAC_ADDR 3

#define SIF_SMAP_RECEIVE 0x07

typedef struct {
	struct t_SifCmdHeader sifcmd;
	u32 payload;
	u32 size;
} iop_sifCmdSmapIrq_t;

static u32 smap_rpc_data[2048] __attribute__ ((aligned(64)));

/*--------------------------------------------------------------------------*/

static int smaprpc_start_xmit(struct sk_buff *skb, struct net_device *net_dev);

static struct net_device_stats *smaprpc_get_stats(struct net_device *net_dev);

static int smaprpc_open(struct net_device *net_dev);

static int smaprpc_close(struct net_device *net_dev);

static int smaprpc_ioctl(struct net_device *net_dev, struct ifreq *ifr,
	int cmd);

static void smaprpc_rpcend_notify(void *arg);

static void smaprpc_rpc_setup(struct smaprpc_chan *smap);

static int smaprpc_thread(void *arg);

static void smaprpc_run(struct smaprpc_chan *smap);

static void smaprpc_start_xmit2(struct smaprpc_chan *smap);

static void smaprpc_skb_queue_init(struct smaprpc_chan *smap,
	struct sk_buff_head *head);
static void smaprpc_skb_enqueue(struct sk_buff_head *head,
	struct sk_buff *newsk);
static void smaprpc_skb_enqueue(struct sk_buff_head *head,
	struct sk_buff *newsk);
static struct sk_buff *smaprpc_skb_dequeue(struct sk_buff_head *head);

/*--------------------------------------------------------------------------*/

static void smaprpc_skb_queue_init(struct smaprpc_chan *smap,
	struct sk_buff_head *head)
{
	unsigned long flags;

	spin_lock_irqsave(&smap->spinlock, flags);
	(void) skb_queue_head_init(head);
	spin_unlock_irqrestore(&smap->spinlock, flags);

	return;
}

static void smaprpc_skb_enqueue(struct sk_buff_head *head,
	struct sk_buff *newsk)
{
	(void) skb_queue_tail(head, newsk);
	return;
}

static void smaprpc_skb_requeue(struct sk_buff_head *head,
	struct sk_buff *newsk)
{
	(void) skb_queue_head(head, newsk);
	return;
}

static struct sk_buff *smaprpc_skb_dequeue(struct sk_buff_head *head)
{
	struct sk_buff *skb;

	skb = skb_dequeue(head);
	return (skb);
}

/*--------------------------------------------------------------------------*/

/* return value: 0 if success, !0 if error */
static int smaprpc_start_xmit(struct sk_buff *skb, struct net_device *net_dev)
{
	struct smaprpc_chan *smap = netdev_priv(net_dev);

	unsigned long flags;

	spin_lock_irqsave(&smap->spinlock, flags);

	smaprpc_skb_enqueue(&smap->txqueue, skb);
	wake_up_interruptible(&smap->wait_smaprun);
	spin_unlock_irqrestore(&smap->spinlock, flags);
	return (0);
}

/*--------------------------------------------------------------------------*/

static struct net_device_stats *smaprpc_get_stats(struct net_device *net_dev)
{
	struct smaprpc_chan *smap = netdev_priv(net_dev);

	return (&smap->net_stats);
}

static void smaprpc_run(struct smaprpc_chan *smap)
{
	unsigned long flags;

	spin_lock_irqsave(&smap->spinlock, flags);
	while (smap->txqueue.qlen > 0) {
		spin_unlock_irqrestore(&smap->spinlock, flags);
		smaprpc_start_xmit2(smap);
		spin_lock_irqsave(&smap->spinlock, flags);
	}
	spin_unlock_irqrestore(&smap->spinlock, flags);
}


static void smaprpc_start_xmit2(struct smaprpc_chan *smap)
{
	int rv;

	struct completion compl;

	struct sk_buff *skb;

	unsigned long flags;

	spin_lock_irqsave(&smap->spinlock, flags);
	skb = smaprpc_skb_dequeue(&smap->txqueue);
	spin_unlock_irqrestore(&smap->spinlock, flags);
	if (skb == NULL)
		return;

	init_completion(&compl);

	down(&smap->smap_rpc_sema);
	memcpy(smap_rpc_data, skb->data, skb->len);
	do {
		rv = ps2sif_callrpc(&smap->cd_smap_rpc, SMAP_CMD_SEND,
			SIF_RPCM_NOWAIT,
			(void *) smap_rpc_data, skb->len,
			smap_rpc_data, sizeof(smap_rpc_data),
			(ps2sif_endfunc_t) smaprpc_rpcend_notify, (void *) &compl);
	} while (rv == -E_SIF_PKT_ALLOC);
	if (rv != 0) {
		printk("%s: smaprpc_start_xmit2: callrpc failed, (%d)\n",
			smap->net_dev->name, rv);

		spin_lock_irqsave(&smap->spinlock, flags);
		smaprpc_skb_requeue(&smap->txqueue, skb);
		spin_unlock_irqrestore(&smap->spinlock, flags);
	} else {
		wait_for_completion(&compl);

		dev_kfree_skb(skb);
	}
	up(&smap->smap_rpc_sema);
}

static int smaprpc_open(struct net_device *net_dev)
{
	struct smaprpc_chan *smap = netdev_priv(net_dev);

	smap->flags |= SMAPRPC_F_OPENED;
	smaprpc_skb_queue_init(smap, &smap->txqueue);

	return (0);					/* success */
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
		printk("%s: smap rpc setup: bind error 1, network will not work on slim PSTwo\n",
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
				smap_rpc_data, 4,
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
		}
	} else {
		printk("%s: Failed to allocate receive buffer. Receive will not work.\n",
			smap->net_dev->name);
	}
	smap->rpc_initialized = -1;
}

static int smaprpc_thread(void *arg)
{
	struct smaprpc_chan *smap = (struct smaprpc_chan *) arg;

	while (1) {
		smaprpc_run(smap);

		interruptible_sleep_on(&smap->wait_smaprun);

		if (kthread_should_stop())
			break;
	}

	return 0;
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

	dma_cache_inv((unsigned long) pkt, sizeof(*pkt));
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
	.ndo_set_mac_address 	= NULL,
	.ndo_change_mtu		= eth_change_mtu,
#ifdef CONFIG_NET_POLL_CONTROLLER
	.ndo_poll_controller = NULL,
#endif
};

static int smaprpc_probe(struct platform_device *dev)
{
	struct net_device *net_dev = NULL;
	struct smaprpc_chan *smap = NULL;
	struct sb_sifaddcmdhandler_arg addcmdhandlerparam;

	if (ps2_pccard_present != 0x0200) {
		printk("PlayStation 2 HDD/Ethernet device NOT present (slim PSTwo).\n");
		return (-ENODEV);
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

	net_dev->netdev_ops = &smaprpc_netdev_ops;

	spin_lock_init(&smap->spinlock);
	sema_init(&smap->smap_rpc_sema, 1);
	init_waitqueue_head(&smap->wait_smaprun);

	addcmdhandlerparam.fid = SIF_SMAP_RECEIVE;
	addcmdhandlerparam.func = handleSmapIRQ;
	addcmdhandlerparam.data = (void *) smap;
	if (sbios(SB_SIFADDCMDHANDLER, &addcmdhandlerparam) < 0) {
		printk("Failed to initialize smap IRQ handler. Receive will not work.\n");
	}

	if (register_netdev(net_dev)) {
		goto error;
	}
	smaprpc_rpc_setup(smap);

	if (smap->rpc_initialized) {
		smap->smaprun_task = kthread_run(smaprpc_thread, smap, "ps2smaprpc");

		printk("Slim PlayStation 2 SMAP(Ethernet) device driver.\n");

		return (0);				/* success */
	}
	unregister_netdev(net_dev);
error:
	printk("Slim PlayStation 2 SMAP(Ethernet) device not found.\n");
	free_netdev(net_dev);
	return (-ENODEV);
}

static int smaprpc_driver_remove(struct platform_device *pdev)
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

	if (smap->smaprun_task != NULL) {
		kthread_stop(smap->smaprun_task);
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
	.remove	= smaprpc_driver_remove,
	.driver	= {
		.name	= "ps2smaprpc",
		.owner	= THIS_MODULE,
	},
};
module_platform_driver(smap_driver);

MODULE_AUTHOR("Juergen Urban");
MODULE_DESCRIPTION("PlayStation 2 ethernet device driver for slim PSTwo");
MODULE_LICENSE("GPL");
