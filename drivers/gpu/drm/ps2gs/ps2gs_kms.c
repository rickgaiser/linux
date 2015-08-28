/*
 * ps2gs_kms.c  --  Playstation2 GS Mode Setting
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

#include "ps2gs_crtc.h"
#include "ps2gs_drv.h"
#include "ps2gs_kms.h"
#include "ps2gs_fbdev.h"
#include "ps2gs_regs.h"
#include "ps2gs_vga.h"

/* -----------------------------------------------------------------------------
 * Format helpers
 */

static const struct ps2gs_format_info ps2gs_format_infos[] = {
	{
		.fourcc = DRM_FORMAT_RGBA5551,
		.bpp = 16,
	}, {
		.fourcc = DRM_FORMAT_RGBX5551,
		.bpp = 16,
	}, {
		.fourcc = DRM_FORMAT_RGB888,
		.bpp = 24,
	}, {
		.fourcc = DRM_FORMAT_RGBA8888,
		.bpp = 32,
	}, {
		.fourcc = DRM_FORMAT_RGBX8888,
		.bpp = 32,
	},
};

const struct ps2gs_format_info *ps2gs_format_info(u32 fourcc)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(ps2gs_format_infos); ++i) {
		if (ps2gs_format_infos[i].fourcc == fourcc)
			return &ps2gs_format_infos[i];
	}

	return NULL;
}

/* -----------------------------------------------------------------------------
 * Common connector and encoder functions
 */

struct drm_encoder *
ps2gs_connector_best_encoder(struct drm_connector *connector)
{
	struct ps2gs_connector *gscon = to_ps2gs_connector(connector);

	//FUNC_DEBUG();

	return &gscon->encoder->encoder;
}

void ps2gs_encoder_mode_prepare(struct drm_encoder *encoder)
{
	//FUNC_DEBUG();
}

void ps2gs_encoder_mode_set(struct drm_encoder *encoder,
			      struct drm_display_mode *mode,
			      struct drm_display_mode *adjusted_mode)
{
	//FUNC_DEBUG();
}

void ps2gs_encoder_mode_commit(struct drm_encoder *encoder)
{
	//FUNC_DEBUG();
}

/* -----------------------------------------------------------------------------
 * Frame buffer
 */

int ps2gs_dumb_create(struct drm_file *file, struct drm_device *dev,
			struct drm_mode_create_dumb *args)
{
	unsigned int min_pitch = DIV_ROUND_UP(args->width * args->bpp, 8);
	unsigned int align;

	FUNC_DEBUG();

	/* Width must be a multiple of 64 pixels */
	align = 64 * args->bpp / 8;
	args->pitch = roundup(max(args->pitch, min_pitch), align);

	return drm_gem_cma_dumb_create(file, dev, args);
}

static struct drm_framebuffer *
ps2gs_fb_create(struct drm_device *dev, struct drm_file *file_priv,
		  struct drm_mode_fb_cmd2 *mode_cmd)
{
	const struct ps2gs_format_info *format;
	unsigned int align;

	FUNC_DEBUG();

	format = ps2gs_format_info(mode_cmd->pixel_format);
	if (format == NULL) {
		dev_dbg(dev->dev, "unsupported pixel format %08x\n",
			mode_cmd->pixel_format);
		return ERR_PTR(-EINVAL);
	}

	/* Width must be a multiple of 64 pixels */
	align = 64 * format->bpp / 8;
	if (mode_cmd->pitches[0] & (align - 1) ||
	    mode_cmd->pitches[0] >= 2048) {
		dev_dbg(dev->dev, "invalid pitch value %u\n",
			mode_cmd->pitches[0]);
		return ERR_PTR(-EINVAL);
	}

	return drm_fb_cma_create(dev, file_priv, mode_cmd);
}

static const struct drm_mode_config_funcs ps2gs_mode_config_funcs = {
	.fb_create = ps2gs_fb_create,
};

int ps2gs_modeset_init(struct ps2gs_device *gs)
{
	struct drm_device *dev = gs->ddev;
	struct drm_encoder *encoder;
	int ret;

	FUNC_DEBUG();

	drm_mode_config_init(dev);

	dev->mode_config.min_width = 0;
	dev->mode_config.min_height = 0;
	dev->mode_config.max_width = 1920;
	dev->mode_config.max_height = 1200;
	dev->mode_config.funcs = &ps2gs_mode_config_funcs;

	ret = ps2gs_plane_init(gs);
	if (ret < 0)
		return ret;

	ret = ps2gs_crtc_create(gs);
	if (ret < 0)
		return ret;

	ps2gs_vga_init(gs);

	/* Set the possible CRTCs */
	list_for_each_entry(encoder, &dev->mode_config.encoder_list, head) {
		encoder->possible_crtcs = 1;
	}

	ret = ps2gs_plane_register(gs);
	if (ret < 0)
		return ret;

	drm_kms_helper_poll_init(dev);

	drm_helper_disable_unused_functions(dev);

	ps2gs_fbdev_init(gs);

	return 0;
}
