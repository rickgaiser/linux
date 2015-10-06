/*
 * ps2gs_drv.h  --  Playstation2 GS DRM driver
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

#ifndef __PS2GS_DRV_H__
#define __PS2GS_DRV_H__

#include <linux/kernel.h>

#include "ps2gs_crtc.h"
#include "ps2gs_plane.h"

#define FUNC_DEBUG() printk("ps2gs: %s\n", __func__)

struct device;
struct drm_device;
struct drm_fbdev_cma;

struct ps2gs_device {
	struct device *dev;

	struct drm_device *ddev;
	struct drm_fbdev_cma *fbdev;

	struct ps2gs_crtc gscrtc;
	struct ps2gs_plane gsplane;
};

#endif /* __PS2GS_DRV_H__ */
