/*
 * ps2gs_plane.h  --  Playstation2 GS Planes
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

#ifndef __PS2GS_PLANE_H__
#define __PS2GS_PLANE_H__

struct drm_crtc;
struct drm_framebuffer;
struct ps2gs_device;
struct ps2gs_format_info;

struct ps2gs_plane {
	struct ps2gs_device *dev;
	struct drm_crtc *crtc;

	bool enabled;

	const struct ps2gs_format_info *format;

	unsigned long dma;
	unsigned int pitch;

	unsigned int crtc_x;
	unsigned int crtc_y;
	unsigned int crtc_w;
	unsigned int crtc_h;

	unsigned int src_x;
	unsigned int src_y;
	unsigned int src_w;
	unsigned int src_h;
};

int ps2gs_plane_init		(struct ps2gs_device *gs);
int ps2gs_plane_register	(struct ps2gs_device *gs);
void ps2gs_plane_setup		(struct ps2gs_plane *gsplane);
void ps2gs_plane_compute_base	(struct ps2gs_plane *gsplane, struct drm_framebuffer *fb);
int ps2gs_plane_mode_set	(struct ps2gs_plane *gsplane, struct drm_crtc *crtc,
				 struct drm_framebuffer *fb, int crtc_x, int crtc_y,
				 unsigned int crtc_w, unsigned int crtc_h,
				 uint32_t src_x, uint32_t src_y,
				 uint32_t src_w, uint32_t src_h);

#endif /* __PS2GS_PLANE_H__ */
