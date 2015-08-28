/*
 * ps2gs_vga.c  --  Playstation2 GS VGA DAC and Connector
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

#include "ps2gs_drv.h"
#include "ps2gs_kms.h"
#include "ps2gs_vga.h"

/* -----------------------------------------------------------------------------
 * Connector
 */

static int ps2gs_vga_connector_get_modes(struct drm_connector *connector)
{
	int count = 0;

	FUNC_DEBUG();

	/* Just add a static list of modes */
	count = drm_add_modes_noedid(connector, 640, 480);
	count = drm_add_modes_noedid(connector, 800, 600);
	count = drm_add_modes_noedid(connector, 1024, 768);
	count = drm_add_modes_noedid(connector, 1280, 1024);

	return count;
}

static int ps2gs_vga_connector_mode_valid(struct drm_connector *connector,
					    struct drm_display_mode *mode)
{
	FUNC_DEBUG();

	return MODE_OK;
}

static const struct drm_connector_helper_funcs connector_helper_funcs = {
	.get_modes = ps2gs_vga_connector_get_modes,
	.mode_valid = ps2gs_vga_connector_mode_valid,
	.best_encoder = ps2gs_connector_best_encoder,
};

static void ps2gs_vga_connector_destroy(struct drm_connector *connector)
{
	FUNC_DEBUG();

	drm_sysfs_connector_remove(connector);
	drm_connector_cleanup(connector);
}

static enum drm_connector_status
ps2gs_vga_connector_detect(struct drm_connector *connector, bool force)
{
	FUNC_DEBUG();

	return connector_status_unknown;
}

static const struct drm_connector_funcs connector_funcs = {
	.dpms = drm_helper_connector_dpms,
	.detect = ps2gs_vga_connector_detect,
	.fill_modes = drm_helper_probe_single_connector_modes,
	.destroy = ps2gs_vga_connector_destroy,
};

static int ps2gs_vga_connector_init(struct ps2gs_device *gs,
				      struct ps2gs_encoder *renc)
{
	struct ps2gs_connector *rcon;
	struct drm_connector *connector;
	int ret;

	FUNC_DEBUG();

	rcon = devm_kzalloc(gs->dev, sizeof(*rcon), GFP_KERNEL);
	if (rcon == NULL)
		return -ENOMEM;

	connector = &rcon->connector;
	connector->display_info.width_mm = 0;
	connector->display_info.height_mm = 0;

	ret = drm_connector_init(gs->ddev, connector, &connector_funcs,
				 DRM_MODE_CONNECTOR_VGA);
	if (ret < 0)
		return ret;

	drm_connector_helper_add(connector, &connector_helper_funcs);
	ret = drm_sysfs_connector_add(connector);
	if (ret < 0)
		return ret;

	drm_helper_connector_dpms(connector, DRM_MODE_DPMS_OFF);
	drm_object_property_set_value(&connector->base,
		gs->ddev->mode_config.dpms_property, DRM_MODE_DPMS_OFF);

	ret = drm_mode_connector_attach_encoder(connector, &renc->encoder);
	if (ret < 0)
		return ret;

	connector->encoder = &renc->encoder;
	rcon->encoder = renc;

	return 0;
}

/* -----------------------------------------------------------------------------
 * Encoder
 */

static void ps2gs_vga_encoder_dpms(struct drm_encoder *encoder, int mode)
{
	FUNC_DEBUG();
}

static bool ps2gs_vga_encoder_mode_fixup(struct drm_encoder *encoder,
					   const struct drm_display_mode *mode,
					   struct drm_display_mode *adjusted_mode)
{
	FUNC_DEBUG();

	return true;
}

static const struct drm_encoder_helper_funcs encoder_helper_funcs = {
	.dpms		= ps2gs_vga_encoder_dpms,
	.mode_fixup	= ps2gs_vga_encoder_mode_fixup,
	.prepare	= ps2gs_encoder_mode_prepare,
	.commit		= ps2gs_encoder_mode_commit,
	.mode_set	= ps2gs_encoder_mode_set,
};

static const struct drm_encoder_funcs encoder_funcs = {
	.destroy	= drm_encoder_cleanup,
};

int ps2gs_vga_init(struct ps2gs_device *gs)
{
	struct ps2gs_encoder *renc;
	int ret;

	FUNC_DEBUG();

	renc = devm_kzalloc(gs->dev, sizeof(*renc), GFP_KERNEL);
	if (renc == NULL)
		return -ENOMEM;

	ret = drm_encoder_init(gs->ddev, &renc->encoder, &encoder_funcs,
			       DRM_MODE_ENCODER_DAC);
	if (ret < 0)
		return ret;

	drm_encoder_helper_add(&renc->encoder, &encoder_helper_funcs);

	return ps2gs_vga_connector_init(gs, renc);
}
