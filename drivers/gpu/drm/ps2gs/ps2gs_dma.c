/*
 * ps2gs_dma.c  --  Playstation2 GS DMA
 *
 * Copyright (C) 2015, Rick Gaiser <rgaiser@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/dma-mapping.h>
#include <linux/dma-direction.h>
#include <linux/dmaengine.h>

#include <asm/mach-ps2/dma.h>

#include "ps2gs_dma.h"
#include "ps2gs_drv.h"


#define BUF_SIZE (4 * 1024)
unsigned char ps2gs_dma_buf[BUF_SIZE] __attribute__ ((aligned(16)));

#define SGL_MAX 256
struct ps2gs_dma_priv {
	struct dma_chan *channel;
	struct completion dma_completion;

	/* Scatter gather list */
	struct scatterlist *sgl;
	unsigned int sgl_used;
};
static struct ps2gs_dma_priv ps2dma;


static void ps2gs_dma_send_cb(void *id)
{
	//FUNC_DEBUG();

	complete(&ps2dma.dma_completion);
}

int ps2gs_dma_send_sg(struct scatterlist *sgl, int sg_nents)
{
	struct dma_chan *chan = ps2dma.channel;
	struct dma_async_tx_descriptor *desc;
	//dma_cookie_t cookie;
	int nr_sg;

	//FUNC_DEBUG();

	nr_sg = dma_map_sg(chan->device->dev, sgl, sg_nents, DMA_TO_DEVICE);
	if (nr_sg == 0)
		return -1;

	desc = dmaengine_prep_slave_sg(chan, sgl, nr_sg, DMA_TO_DEVICE, DMA_PREP_INTERRUPT);
	if (desc) {
		desc->callback = ps2gs_dma_send_cb;
		desc->callback_param = NULL;
		/*cookie =*/ dmaengine_submit(desc);

		init_completion(&ps2dma.dma_completion);
		dma_async_issue_pending(chan);
		wait_for_completion(&ps2dma.dma_completion);
	}

	return 0;
}

int ps2gs_dma_send(const void *ptr, size_t size)
{
	struct scatterlist sgl;

	//FUNC_DEBUG();

	sg_init_table(&sgl, 1);
	sg_set_buf(&sgl, ptr, size);
	sg_mark_end(&sgl);

	return ps2gs_dma_send_sg(&sgl, 1);
}

void ps2gs_dma_sgl_kick(void)
{
	//FUNC_DEBUG();

	if (ps2dma.sgl_used > 0) {
		ps2gs_dma_send_sg(ps2dma.sgl, ps2dma.sgl_used);
		ps2dma.sgl_used = 0;
		//ps2dma.buf_used = 0;
	}
}

void ps2gs_dma_sgl_add_cont(const void *ptr, size_t size)
{
	//FUNC_DEBUG();
	//printk("ps2gs: %s(0x%p, %d)\n", __func__, ptr, size);

	sg_set_buf(&ps2dma.sgl[ps2dma.sgl_used], ptr, size);

	ps2dma.sgl_used++;
	if (ps2dma.sgl_used == SGL_MAX) {
		ps2gs_dma_sgl_kick();
	}
}

#if 0
void ps2fb_sgl_add(const void *ptr, size_t size)
{
	unsigned int page;
	unsigned int start;
	unsigned int end;
	unsigned int s;
	unsigned int offset;
	void *cur_start;
	unsigned int cur_size;

	//FUNC_DEBUG();
	printk("ps2gs: %s(0x%p, %d)\n", __func__, ptr, size);

	start = (unsigned int) ptr;
	cur_start = 0;
	cur_size = 0;

	/* First align to page boundary */
	s = start & (PAGE_SIZE - 1);
	s = PAGE_SIZE - s;
	if (s < PAGE_SIZE) {
		if (s > size) {
			s = size;
		}
		offset = start & (PAGE_SIZE - 1);
		/* vmalloc_to_pfn is only working with redraw handler. */
		cur_start = phys_to_virt(PFN_PHYS(vmalloc_to_pfn((void *) start))) + offset;
		cur_size = ALIGN16(s);
		start += s;
		size -= s;
	}

	end = ALIGN16(start + size);

	for (page = start; page < end; page += PAGE_SIZE) {
		void *addr;
		unsigned int size;

		addr = phys_to_virt(PFN_PHYS(vmalloc_to_pfn((void *) page)));
		size = end - page;
		if (size > PAGE_SIZE) {
			size = PAGE_SIZE;
		}

		if (cur_size > 0) {
			if (addr == (cur_start + cur_size)) {
				/* contiguous physical memory. */
				cur_size += size;
			} else {
				ps2gs_dma_sgl_add_cont(cur_start, ALIGN16(cur_size));

				cur_size = size;
				cur_start = addr;
			}
		} else {
			cur_start = addr;
			cur_size = size;
		}
	}

	if (cur_size > 0) {
		ps2gs_dma_sgl_add_cont(cur_start, ALIGN16(cur_size));

		cur_size = 0;
		cur_start = 0;
	}
}
#endif

static bool ps2gs_dma_filter_fn(struct dma_chan *chan, void *param)
{
	FUNC_DEBUG();

	return chan->chan_id == DMA_GIF;
}

static struct dma_slave_config ps2gs_dma_config = {
	.direction	= DMA_TO_DEVICE,
};

int ps2gs_dma_init(struct ps2gs_device *gs)
{
	dma_cap_mask_t mask;
	int ret;

	FUNC_DEBUG();

	/* Allocate and initialize scatter gather list */
	ps2dma.sgl = vzalloc(SGL_MAX * sizeof(struct scatterlist));
	if (!ps2dma.sgl) {
		return -ENOMEM;
	}
	sg_init_table(ps2dma.sgl, SGL_MAX);
	ps2dma.sgl_used = 0;

	/* Configure DMA channel */
	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	ps2dma.channel = dma_request_channel(mask, ps2gs_dma_filter_fn, NULL);
	if (!ps2dma.channel) {
		dev_err(gs->dev, "unable to request DMA channel\n");
		vfree(ps2dma.sgl);
		return -ENOENT;
	}

	ret = dmaengine_slave_config(ps2dma.channel, &ps2gs_dma_config);
	if (ret)
		dev_warn(gs->dev, "DMA slave_config returned %d\n", ret);


	return 0;
}
