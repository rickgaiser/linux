/*
 * ps2gs_fb.h  --  Playstation2 GS Framebuffer Oprations
 *
 * Copyright (C) 2015, Rick Gaiser <rgaiser@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __PS2GS_FB_H__
#define __PS2GS_FB_H__


struct drm_framebuffer;


int ps2gs_fb_init(void);
void ps2gs_fb_redraw(struct drm_framebuffer *fb);


#endif /* __PS2GS_FB_H__ */
