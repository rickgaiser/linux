/*
 * ps2gs_kms.h  --  Playstation2 GS Mode Setting
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

#ifndef __PS2GS_KMS_H__
#define __PS2GS_KMS_H__

#include <linux/types.h>

#include <drm/drm_crtc.h>

struct ps2gs_device;

struct ps2gs_format_info {
	u32 fourcc;
	unsigned int bpp;
};

struct ps2gs_encoder {
	struct drm_encoder encoder;
};

#define to_ps2gs_encoder(e) \
	container_of(e, struct ps2gs_encoder, encoder)

struct ps2gs_connector {
	struct drm_connector connector;
	struct ps2gs_encoder *encoder;
};

#define to_ps2gs_connector(c) \
	container_of(c, struct ps2gs_connector, connector)

const struct ps2gs_format_info *ps2gs_format_info(u32 fourcc);

struct drm_encoder *
ps2gs_connector_best_encoder(struct drm_connector *connector);
void ps2gs_encoder_mode_prepare(struct drm_encoder *encoder);
void ps2gs_encoder_mode_set(struct drm_encoder *encoder,
			      struct drm_display_mode *mode,
			      struct drm_display_mode *adjusted_mode);
void ps2gs_encoder_mode_commit(struct drm_encoder *encoder);

int ps2gs_modeset_init(struct ps2gs_device *gs);

int ps2gs_dumb_create(struct drm_file *file, struct drm_device *dev,
			struct drm_mode_create_dumb *args);

#endif /* __PS2GS_KMS_H__ */
