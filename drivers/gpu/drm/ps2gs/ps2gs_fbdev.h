/*
 * ps2gs_fbdev.h  --  Playstation2 GS FBDEV
 *
 * Copyright (C) 2015, Rick Gaiser <rgaiser@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __PS2GS_FBDEV_H__
#define __PS2GS_FBDEV_H__

struct ps2gs_device;

int ps2gs_fbdev_init(struct ps2gs_device *gs);
void ps2gs_fbdev_fini(struct ps2gs_device *gs);

#endif /* __PS2GS_FBDEV_H__ */
