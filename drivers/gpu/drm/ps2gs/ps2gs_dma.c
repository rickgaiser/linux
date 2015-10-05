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


static struct dma_chan *channel;


struct completion dma_completion;
static void ps2gs_dma_send_cb(void *id)
{
	//FUNC_DEBUG();

	complete(&dma_completion);
}

int ps2gs_dma_send_sg(struct scatterlist *sgl, int sg_nents)
{
	struct dma_chan *chan = channel;
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

		init_completion(&dma_completion);
		dma_async_issue_pending(chan);
		wait_for_completion(&dma_completion);
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

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	channel = dma_request_channel(mask, ps2gs_dma_filter_fn, NULL);
	if (!channel) {
		dev_err(gs->dev, "unable to request DMA channel\n");
		return -ENOENT;
	}

	ret = dmaengine_slave_config(channel, &ps2gs_dma_config);
	if (ret)
		dev_warn(gs->dev, "DMA slave_config returned %d\n", ret);


	return 0;
}
