/*
 *  Copyright (C) 2015, Rick Gaiser <rgaiser@gmail.com>
 *
 *  Based on dma-jz4740.c
 *    Copyright (C) 2013, Lars-Peter Clausen <lars@metafoo.de>
 *
 *  PS2 DMA support
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General	 Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 */

#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/irq.h>
#include <linux/clk.h>

#include <asm/mach-ps2/dma.h>

#include "virt-dma.h"

#define PS2_DMA_NR_CHANS 10

#define PS2_REG_DMA_CHCR_OFFSET			0x0000
#define PS2_REG_DMA_MADR_OFFSET			0x0010
#define PS2_REG_DMA_QWC_OFFSET			0x0020
#define PS2_REG_DMA_TADR_OFFSET			0x0030
#define PS2_REG_DMA_SADR_OFFSET			0x0080

#define PS2_REG_DMA_CHCR(x)			(ps2dma_channels[(x)].base + PS2_REG_DMA_CHCR_OFFSET) /* Channel Control Register */
#define PS2_REG_DMA_MADR(x)			(ps2dma_channels[(x)].base + PS2_REG_DMA_MADR_OFFSET) /* Transfer Address Register */
#define PS2_REG_DMA_QWC(x)			(ps2dma_channels[(x)].base + PS2_REG_DMA_QWC_OFFSET)  /* Transfer Data Size Register */
#define PS2_REG_DMA_TADR(x)			(ps2dma_channels[(x)].base + PS2_REG_DMA_TADR_OFFSET) /* Tag Address Register */
#define PS2_REG_DMA_SADR(x)			(ps2dma_channels[(x)].base + PS2_REG_DMA_SADR_OFFSET) /* SPR Transfer Address Register */

#define PS2_DMA_CHCR_DIR			BIT(0) /* Direction: 0 = to Memory, 1 = from Memory */
#define PS2_DMA_CHCR_MOD_NORMAL			(0<<2) /* Mode: normal */
#define PS2_DMA_CHCR_MOD_CHAIN			(1<<2) /* Mode: chain */
#define PS2_DMA_CHCR_MOD_INTERLEAVE		(2<<2) /* Mode: interleave */
#define PS2_DMA_CHCR_STR			BIT(8) /* Start: 0 = Stop, 1 = Start */
#define PS2_DMA_CHCR_TIE			BIT(7) /* Tag Interrupt Enable */

struct ps2_dma_sg {
	dma_addr_t addr;
	unsigned int len;
};

struct ps2_dma_desc {
	struct virt_dma_desc vdesc;

	enum dma_transfer_direction direction;
	bool cyclic;

	unsigned int num_sgs;
	struct ps2_dma_sg sg[];
};

struct ps2_dma_chan {
	struct virt_dma_chan vchan;
	unsigned int id;

	unsigned int cmd;

	struct ps2_dma_desc *desc;
	unsigned int next_sg;
};

struct ps2_dma_dev {
	struct dma_device ddev;
	void __iomem *base;
	struct clk *clk;

	struct ps2_dma_chan chan[PS2_DMA_NR_CHANS];
};

struct dma_channel ps2dma_channels[] = {
	{ IRQ_DMAC_0, 0x10008000, DMA_SENDCH, 0, "VIF0 DMA", },
	{ IRQ_DMAC_1, 0x10009000, DMA_SENDCH, 0, "VIF1 DMA", },
	{ IRQ_DMAC_2, 0x1000a000, DMA_SENDCH, 0, "GIF DMA", },
	{ IRQ_DMAC_3, 0x1000b000, DMA_RECVCH, 0, "fromIPU DMA", },
	{ IRQ_DMAC_4, 0x1000b400, DMA_SENDCH, 0, "toIPU DMA", },
	{ IRQ_DMAC_5, 0x1000c000, DMA_RECVCH, 0, "fromSIF0 DMA", },
	{ IRQ_DMAC_6, 0x1000c400, DMA_SENDCH, 0, "toSIF1 DMA", },
	{ IRQ_DMAC_7, 0x1000c800, DMA_SENDCH, 0, "SIF2 DMA", },
	{ IRQ_DMAC_8, 0x1000d000, DMA_RECVCH, 1, "fromSPR DMA", },
	{ IRQ_DMAC_9, 0x1000d400, DMA_SENDCH, 1, "toSPR DMA", },
};

static struct ps2_dma_dev *ps2_dma_chan_get_dev(
	struct ps2_dma_chan *chan)
{
	return container_of(chan->vchan.chan.device, struct ps2_dma_dev,
		ddev);
}

static struct ps2_dma_chan *to_ps2_dma_chan(struct dma_chan *c)
{
	return container_of(c, struct ps2_dma_chan, vchan.chan);
}

static struct ps2_dma_desc *to_ps2_dma_desc(struct virt_dma_desc *vdesc)
{
	return container_of(vdesc, struct ps2_dma_desc, vdesc);
}

static inline uint32_t ps2_dma_read(struct ps2_dma_dev *dmadev,
	unsigned int reg)
{
	return inl(/*dmadev->base +*/ reg);
}

static inline void ps2_dma_write(struct ps2_dma_dev *dmadev,
	unsigned reg, uint32_t val)
{
	outl(val, /*dmadev->base +*/ reg);
}

static inline void ps2_dma_write_mask(struct ps2_dma_dev *dmadev,
	unsigned int reg, uint32_t val, uint32_t mask)
{
	uint32_t tmp;

	tmp = ps2_dma_read(dmadev, reg);
	tmp &= ~mask;
	tmp |= val;
	ps2_dma_write(dmadev, reg, tmp);
}

static struct ps2_dma_desc *ps2_dma_alloc_desc(unsigned int num_sgs)
{
	return kzalloc(sizeof(struct ps2_dma_desc) +
		sizeof(struct ps2_dma_sg) * num_sgs, GFP_ATOMIC);
}

static int ps2_dma_slave_config(struct dma_chan *c,
	const struct dma_slave_config *config)
{
	struct ps2_dma_chan *chan = to_ps2_dma_chan(c);

	switch (config->direction) {
	case DMA_MEM_TO_DEV:
	case DMA_DEV_TO_MEM:
		break;
	default:
		return -EINVAL;
	}

	/* STR: Start */
	chan->cmd  = PS2_DMA_CHCR_STR;
	/* TIE: Tag Interrupt Enable */
	chan->cmd |= PS2_DMA_CHCR_TIE;
	/* DIR: Direction */
	chan->cmd |= (config->direction == DMA_MEM_TO_DEV) ? PS2_DMA_CHCR_DIR : 0;
	/*
	 * MOD: Mode
	 * Enable destination chain mode for dma packets from IOP
	 */
	chan->cmd |= (config->direction == DMA_DEV_TO_MEM) ? PS2_DMA_CHCR_MOD_CHAIN : PS2_DMA_CHCR_MOD_NORMAL;

	return 0;
}

static int ps2_dma_terminate_all(struct dma_chan *c)
{
	struct ps2_dma_chan *chan = to_ps2_dma_chan(c);
	struct ps2_dma_dev *dmadev = ps2_dma_chan_get_dev(chan);
	unsigned long flags;
	LIST_HEAD(head);

	spin_lock_irqsave(&chan->vchan.lock, flags);
	ps2_dma_write_mask(dmadev, PS2_REG_DMA_CHCR(chan->id), 0, PS2_DMA_CHCR_STR);
	chan->desc = NULL;
	vchan_get_all_descriptors(&chan->vchan, &head);
	spin_unlock_irqrestore(&chan->vchan.lock, flags);

	vchan_dma_desc_free_list(&chan->vchan, &head);

	return 0;
}

static int ps2_dma_control(struct dma_chan *chan, enum dma_ctrl_cmd cmd,
	unsigned long arg)
{
	struct dma_slave_config *config = (struct dma_slave_config *)arg;

	switch (cmd) {
	case DMA_SLAVE_CONFIG:
		return ps2_dma_slave_config(chan, config);
	case DMA_TERMINATE_ALL:
		return ps2_dma_terminate_all(chan);
	default:
		return -ENOSYS;
	}
}

static int ps2_dma_start_transfer(struct ps2_dma_chan *chan)
{
	struct ps2_dma_dev *dmadev = ps2_dma_chan_get_dev(chan);
	struct virt_dma_desc *vdesc;
	struct ps2_dma_sg *sg;

	if (!chan->desc) {
		vdesc = vchan_next_desc(&chan->vchan);
		if (!vdesc)
			return 0;
		chan->desc = to_ps2_dma_desc(vdesc);
		chan->next_sg = 0;
	}

	if (chan->next_sg == chan->desc->num_sgs)
		chan->next_sg = 0;

	sg = &chan->desc->sg[chan->next_sg];

	ps2_dma_write(dmadev, PS2_REG_DMA_MADR(chan->id), sg->addr);
	ps2_dma_write(dmadev, PS2_REG_DMA_QWC(chan->id), sg->len >> 4);

	chan->next_sg++;

	ps2_dma_write(dmadev, PS2_REG_DMA_CHCR(chan->id), chan->cmd);

	return 0;
}

static void ps2_dma_chan_irq(struct ps2_dma_chan *chan)
{
	spin_lock(&chan->vchan.lock);
	if (chan->desc) {
		if (chan->desc->cyclic) {
			vchan_cyclic_callback(&chan->desc->vdesc);
		} else {
			if (chan->next_sg == chan->desc->num_sgs) {
				list_del(&chan->desc->vdesc.node);
				vchan_cookie_complete(&chan->desc->vdesc);
				chan->desc = NULL;
			}
		}
	}
	ps2_dma_start_transfer(chan);
	spin_unlock(&chan->vchan.lock);
}

static irqreturn_t ps2_dma_irq(int irq, void *arg)
{
	struct ps2_dma_chan *chan = arg;

	ps2_dma_chan_irq(chan);

	return IRQ_HANDLED;
}

static void ps2_dma_issue_pending(struct dma_chan *c)
{
	struct ps2_dma_chan *chan = to_ps2_dma_chan(c);
	unsigned long flags;

	spin_lock_irqsave(&chan->vchan.lock, flags);
	if (vchan_issue_pending(&chan->vchan) && !chan->desc)
		ps2_dma_start_transfer(chan);
	spin_unlock_irqrestore(&chan->vchan.lock, flags);
}

static struct dma_async_tx_descriptor *ps2_dma_prep_slave_sg(
	struct dma_chan *c, struct scatterlist *sgl,
	unsigned int sg_len, enum dma_transfer_direction direction,
	unsigned long flags, void *context)
{
	struct ps2_dma_chan *chan = to_ps2_dma_chan(c);
	struct ps2_dma_desc *desc;
	struct scatterlist *sg;
	unsigned int i;

	desc = ps2_dma_alloc_desc(sg_len);
	if (!desc)
		return NULL;

	for_each_sg(sgl, sg, sg_len, i) {
		desc->sg[i].addr = sg_dma_address(sg);
		desc->sg[i].len = sg_dma_len(sg);
	}

	desc->num_sgs = sg_len;
	desc->direction = direction;
	desc->cyclic = false;

	return vchan_tx_prep(&chan->vchan, &desc->vdesc, flags);
}

static struct dma_async_tx_descriptor *ps2_dma_prep_dma_cyclic(
	struct dma_chan *c, dma_addr_t buf_addr, size_t buf_len,
	size_t period_len, enum dma_transfer_direction direction,
	unsigned long flags, void *context)
{
	struct ps2_dma_chan *chan = to_ps2_dma_chan(c);
	struct ps2_dma_desc *desc;
	unsigned int num_periods, i;

	if (buf_len % period_len)
		return NULL;

	num_periods = buf_len / period_len;

	desc = ps2_dma_alloc_desc(num_periods);
	if (!desc)
		return NULL;

	for (i = 0; i < num_periods; i++) {
		desc->sg[i].addr = buf_addr;
		desc->sg[i].len = period_len;
		buf_addr += period_len;
	}

	desc->num_sgs = num_periods;
	desc->direction = direction;
	desc->cyclic = true;

	return vchan_tx_prep(&chan->vchan, &desc->vdesc, flags);
}

static size_t ps2_dma_desc_residue(struct ps2_dma_chan *chan,
	struct ps2_dma_desc *desc, unsigned int next_sg)
{
	struct ps2_dma_dev *dmadev = ps2_dma_chan_get_dev(chan);
	unsigned int residue, count;
	unsigned int i;

	residue = 0;

	for (i = next_sg; i < desc->num_sgs; i++)
		residue += desc->sg[i].len;

	if (next_sg != 0) {
		count = ps2_dma_read(dmadev,
			PS2_REG_DMA_QWC(chan->id));
		residue += count << 4;
	}

	return residue;
}

static enum dma_status ps2_dma_tx_status(struct dma_chan *c,
	dma_cookie_t cookie, struct dma_tx_state *state)
{
	struct ps2_dma_chan *chan = to_ps2_dma_chan(c);
	struct virt_dma_desc *vdesc;
	enum dma_status status;
	unsigned long flags;

	status = dma_cookie_status(c, cookie, state);
	if (status == DMA_SUCCESS || !state)
		return status;

	spin_lock_irqsave(&chan->vchan.lock, flags);
	vdesc = vchan_find_desc(&chan->vchan, cookie);
	if (cookie == chan->desc->vdesc.tx.cookie) {
		state->residue = ps2_dma_desc_residue(chan, chan->desc,
				chan->next_sg);
	} else if (vdesc) {
		state->residue = ps2_dma_desc_residue(chan,
				to_ps2_dma_desc(vdesc), 0);
	} else {
		state->residue = 0;
	}
	spin_unlock_irqrestore(&chan->vchan.lock, flags);

	return status;
}

static int ps2_dma_alloc_chan_resources(struct dma_chan *c)
{
	return 0;
}

static void ps2_dma_free_chan_resources(struct dma_chan *c)
{
	vchan_free_chan_resources(to_virt_chan(c));
}

static void ps2_dma_desc_free(struct virt_dma_desc *vdesc)
{
	kfree(container_of(vdesc, struct ps2_dma_desc, vdesc));
}

static int ps2_dma_probe(struct platform_device *pdev)
{
	struct ps2_dma_chan *chan;
	struct ps2_dma_dev *dmadev;
	struct dma_device *dd;
	unsigned int i;
	struct resource *res;
	int ret;
	int irq;

	dmadev = devm_kzalloc(&pdev->dev, sizeof(*dmadev), GFP_KERNEL);
	if (!dmadev)
		return -EINVAL;

	dd = &dmadev->ddev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	/* NOTE: Use devm_ioremap_resource when porting to newer kernel */
	dmadev->base = devm_request_and_ioremap(&pdev->dev, res);
	if (IS_ERR(dmadev->base))
		return PTR_ERR(dmadev->base);

	dmadev->clk = clk_get(&pdev->dev, "dma");
	if (IS_ERR(dmadev->clk))
		return PTR_ERR(dmadev->clk);

	clk_prepare_enable(dmadev->clk);

	dma_cap_set(DMA_SLAVE, dd->cap_mask);
	dma_cap_set(DMA_CYCLIC, dd->cap_mask);
	dd->device_alloc_chan_resources = ps2_dma_alloc_chan_resources;
	dd->device_free_chan_resources = ps2_dma_free_chan_resources;
	dd->device_tx_status = ps2_dma_tx_status;
	dd->device_issue_pending = ps2_dma_issue_pending;
	dd->device_prep_slave_sg = ps2_dma_prep_slave_sg;
	dd->device_prep_dma_cyclic = ps2_dma_prep_dma_cyclic;
	dd->device_control = ps2_dma_control;
	dd->dev = &pdev->dev;
	INIT_LIST_HEAD(&dd->channels);

	for (i = 0; i < PS2_DMA_NR_CHANS; i++) {
		chan = &dmadev->chan[i];
		chan->id = i;
		chan->vchan.desc_free = ps2_dma_desc_free;
		vchan_init(&chan->vchan, dd);
	}

	ret = dma_async_device_register(dd);
	if (ret)
		return ret;
#if 0
	irq = platform_get_irq(pdev, 0);
	ret = request_irq(irq, ps2_dma_irq, 0, dev_name(&pdev->dev), dmadev);
	if (ret)
		goto err_unregister;
#else
	for (i = 0; i < PS2_DMA_NR_CHANS; i++) {
		/* SIF has its own dma handling */
		/* Do not use SIF (5,6,7) for DMA for now */
		if((i <= 4) || (i >= 8)) {
			irq = ps2dma_channels[i].irq;
			ret = request_irq(irq, ps2_dma_irq, 0, ps2dma_channels[i].device, &dmadev->chan[i]);
			if (ret)
				goto err_unregister;
		}
	}
#endif
	platform_set_drvdata(pdev, dmadev);

	return 0;

err_unregister:
	dma_async_device_unregister(dd);
	return ret;
}

static int ps2_dma_remove(struct platform_device *pdev)
{
	struct ps2_dma_dev *dmadev = platform_get_drvdata(pdev);
	int i, irq;

#if 0
	irq = platform_get_irq(pdev, 0);
	free_irq(irq, dmadev);
#else
	for (i = 0; i < PS2_DMA_NR_CHANS; i++) {
		/* SIF has its own dma handling */
		/* Do not use SIF (5,6,7) for DMA for now */
		if((i <= 4) || (i >= 8)) {
			irq = ps2dma_channels[i].irq;
			free_irq(irq, dmadev);
		}
	}
#endif
	dma_async_device_unregister(&dmadev->ddev);
	clk_disable_unprepare(dmadev->clk);

	return 0;
}

static struct platform_driver ps2_dma_driver = {
	.probe = ps2_dma_probe,
	.remove = ps2_dma_remove,
	.driver = {
		.name = "ps2-dma",
	},
};

static int __init ps2_dma_init(void)
{
	return platform_driver_register(&ps2_dma_driver);
}
subsys_initcall(ps2_dma_init);

static void __exit ps2_dma_exit(void)
{
	platform_driver_unregister(&ps2_dma_driver);
}
module_exit(ps2_dma_exit);

MODULE_AUTHOR("Rick Gaiser <rgaiser@gmail.com>");
MODULE_DESCRIPTION("PS2 DMA driver");
MODULE_LICENSE("GPL v2");
