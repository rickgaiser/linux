/*
 * ps2gs_crtc.h  --  Playstation2 GS CRTCs
 *
 * Copyright (C) 2015, Rick Gaiser <rgaiser@gmail.com>
 *
 * Based on rcar_du
 *   Copyright (C) 2013 Renesas Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __PS2GS_CRTC_H__
#define __PS2GS_CRTC_H__

#include <drm/drmP.h>
#include <drm/drm_crtc.h>

struct ps2gs_device;
struct ps2gs_plane;
struct task_struct;

struct ps2gs_crtc {
	struct drm_crtc crtc;

	struct task_struct *task;

	unsigned int mmio_offset;
	bool started;

	struct drm_pending_vblank_event *event;
	int dpms;

	struct ps2gs_plane *gsplane;
};

int ps2gs_crtc_create(struct ps2gs_device *gs);
void ps2gs_crtc_enable_vblank(struct ps2gs_crtc *rcrtc, bool enable);
void ps2gs_crtc_irq(struct ps2gs_crtc *rcrtc);
void ps2gs_crtc_cancel_page_flip(struct ps2gs_crtc *rcrtc,
				   struct drm_file *file);
void ps2gs_crtc_suspend(struct ps2gs_crtc *rcrtc);
void ps2gs_crtc_resume(struct ps2gs_crtc *rcrtc);

#endif /* __PS2GS_CRTC_H__ */
