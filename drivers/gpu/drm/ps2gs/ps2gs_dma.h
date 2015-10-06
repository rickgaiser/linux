/*
 * ps2gs_dma.h  --  Playstation2 GS DMA
 *
 * Copyright (C) 2015, Rick Gaiser <rgaiser@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __PS2GS_DMA_H__
#define __PS2GS_DMA_H__

#include <linux/types.h>

struct ps2gs_device;
struct scatterlist;

int ps2gs_dma_init(struct ps2gs_device *gs);

/* Send immediately */
int ps2gs_dma_send(const void *ptr, size_t size);
int ps2gs_dma_send_sg(struct scatterlist *sgl, int sg_nents);

/* Send via scatter gather list */
extern unsigned char ps2gs_dma_buf[];
void ps2gs_dma_sgl_kick(void);
void ps2gs_dma_sgl_add_cont(const void *ptr, size_t size);
void ps2gs_dma_sgl_add(const void *ptr, size_t size);

#endif /* __PS2GS_DMA_H__ */
