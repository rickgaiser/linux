/*
 * ps2gs_plane.c  --  Playstation2 GS Planes
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

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_gem_cma_helper.h>

#include "ps2gs_drv.h"
#include "ps2gs_kms.h"
#include "ps2gs_plane.h"
#include "ps2gs_regs.h"

struct ps2gs_kms_plane {
	struct drm_plane plane;
	struct ps2gs_plane *gsplane;
};

static inline struct ps2gs_plane *to_ps2gs_plane(struct drm_plane *plane)
{
	return container_of(plane, struct ps2gs_kms_plane, plane)->gsplane;
}

void ps2gs_plane_compute_base(struct ps2gs_plane *gsplane,
				struct drm_framebuffer *fb)
{
	//struct drm_gem_cma_object *gem;

	FUNC_DEBUG();

	//gem = drm_fb_cma_get_gem_obj(fb, 0);
	gsplane->dma = 0;//gem->paddr + fb->offsets[0];
}

#define VCK 4 // FIXME: Depends on the resolution (4==PAL/NTSC)
#define PSM 1 // FIXME: 1 == 24bit color
#define MAGH (VCK-1)
#define MAGV (1-1)
void ps2gs_plane_setup(struct ps2gs_plane *gsplane)
{
	u32 dispfb  = GS_REG_DISPFB2;  // GS_REG_DISPFB2
	u32 display = GS_REG_DISPLAY2; // GS_REG_DISPLAY2

	FUNC_DEBUG();

	outq(GS_SET_DISPFB(gsplane->dma / 2048, gsplane->src_w / 64, PSM, gsplane->src_x, gsplane->src_y), dispfb);
	outq(GS_SET_DISPLAY(gsplane->crtc_x * VCK, gsplane->crtc_y, MAGH, MAGV, (gsplane->crtc_w * VCK)-1, gsplane->crtc_h-1), display);
}

int
ps2gs_plane_mode_set(struct ps2gs_plane *gsplane, struct drm_crtc *crtc,
		       struct drm_framebuffer *fb, int crtc_x, int crtc_y,
		       unsigned int crtc_w, unsigned int crtc_h,
		       uint32_t src_x, uint32_t src_y,
		       uint32_t src_w, uint32_t src_h)
{
	struct ps2gs_device *gs = gsplane->dev;
	const struct ps2gs_format_info *format;

	FUNC_DEBUG();

	//format = ps2gs_format_info(fb->pixel_format);
	format = ps2gs_format_info(DRM_FORMAT_RGB888);
	if (format == NULL) {
		dev_dbg(gs->dev, "%s: unsupported format %08x\n", __func__,
			fb->pixel_format);
		return -EINVAL;
	}

	if (src_w != crtc_w || src_h != crtc_h) {
		dev_dbg(gs->dev, "%s: scaling not supported\n", __func__);
		return -EINVAL;
	}

	gsplane->crtc = crtc;
	gsplane->format = format;
	gsplane->pitch = fb->pitches[0];

	gsplane->crtc_x = crtc_x;
	gsplane->crtc_y = crtc_y;
	gsplane->crtc_w = crtc_w;
	gsplane->crtc_h = crtc_h;

	gsplane->src_x = src_x;
	gsplane->src_y = src_y;
	gsplane->src_w = src_w;
	gsplane->src_h = src_h;

	ps2gs_plane_compute_base(gsplane, fb);
	ps2gs_plane_setup(gsplane);

	gsplane->enabled = true;

	return 0;
}

static int
ps2gs_plane_update(struct drm_plane *plane, struct drm_crtc *crtc,
		       struct drm_framebuffer *fb, int crtc_x, int crtc_y,
		       unsigned int crtc_w, unsigned int crtc_h,
		       uint32_t src_x, uint32_t src_y,
		       uint32_t src_w, uint32_t src_h)
{
	FUNC_DEBUG();

	return ps2gs_plane_mode_set(to_ps2gs_plane(plane), crtc, fb,
				crtc_x, crtc_y, crtc_w, crtc_h,
				src_x >> 16, src_y >> 16,
				src_w >> 16, src_h >> 16);
}

static int ps2gs_plane_disable(struct drm_plane *plane)
{
	struct ps2gs_plane *gsplane = to_ps2gs_plane(plane);

	FUNC_DEBUG();

	if (!gsplane->enabled)
		return 0;

	gsplane->enabled = false;

	gsplane->crtc = NULL;
	gsplane->format = NULL;

	return 0;
}

static int ps2gs_plane_set_property(struct drm_plane *plane,
				      struct drm_property *property,
				      uint64_t value)
{
	FUNC_DEBUG();

	return 0;
}

static const struct drm_plane_funcs ps2gs_plane_funcs = {
	.update_plane	= ps2gs_plane_update,
	.disable_plane	= ps2gs_plane_disable,
	.set_property	= ps2gs_plane_set_property,
	.destroy	= drm_plane_cleanup,
};

static const uint32_t formats[] = {
	DRM_FORMAT_RGBA5551,
	DRM_FORMAT_RGBX5551,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_RGBA8888,
	DRM_FORMAT_RGBX8888,
};

int ps2gs_plane_init(struct ps2gs_device *gs)
{
	FUNC_DEBUG();

	gs->gsplane.dev = gs;

	return 0;
}

int ps2gs_plane_register(struct ps2gs_device *gs)
{
	struct ps2gs_kms_plane *plane;
	int ret;

	FUNC_DEBUG();

	plane = devm_kzalloc(gs->dev, sizeof(*plane), GFP_KERNEL);
	if (plane == NULL)
		return -ENOMEM;

	ret = drm_plane_init(gs->ddev, &plane->plane, 1,
			     &ps2gs_plane_funcs, formats,
			     ARRAY_SIZE(formats), true);
	if (ret < 0)
		return ret;

	return 0;
}
