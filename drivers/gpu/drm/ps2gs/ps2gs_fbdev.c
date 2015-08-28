/*
 * ps2gs_fbdev.c  --  Playstation2 GS FBDEV
 *
 * Copyright (C) 2015, Rick Gaiser <rgaiser@gmail.com>
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

#include "ps2gs_drv.h"
#include "ps2gs_fbdev.h"


int ps2gs_fbdev_init(struct ps2gs_device *gs)
{
	struct drm_device *dev = gs->ddev;
	struct drm_fbdev_cma *fbdev;

	FUNC_DEBUG();

	fbdev = drm_fbdev_cma_init(dev, 24, dev->mode_config.num_crtc,
				   dev->mode_config.num_connector);
	if (IS_ERR(fbdev))
		return PTR_ERR(fbdev);

	gs->fbdev = fbdev;

	return 0;
}

void ps2gs_fbdev_fini(struct ps2gs_device *gs)
{
	FUNC_DEBUG();

	if (gs->fbdev)
		drm_fbdev_cma_fini(gs->fbdev);
}
