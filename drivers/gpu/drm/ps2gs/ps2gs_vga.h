/*
 * ps2gs_vga.h  --  Playstation2 GS VGA DAC and Connector
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

#ifndef __PS2GS_VGA_H__
#define __PS2GS_VGA_H__

struct ps2gs_device;

int ps2gs_vga_init(struct ps2gs_device *gs);

#endif /* __PS2GS_VGA_H__ */
