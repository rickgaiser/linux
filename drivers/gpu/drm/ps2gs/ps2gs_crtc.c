/*
 * ps2gs_crtc.c  --  Playstation2 GS CRTC
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

#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/freezer.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_gem_cma_helper.h>

#include "ps2gs_crtc.h"
#include "ps2gs_drv.h"
#include "ps2gs_fb.h"
#include "ps2gs_kms.h"
#include "ps2gs_plane.h"
#include "ps2gs_regs.h"
#include "ps2gs_vga.h"

#define PS2GS_REG_SMODE2_DPMS_ON	0x0
#define PS2GS_REG_SMODE2_DPMS_STANDBY	0x1
#define PS2GS_REG_SMODE2_DPMS_SUSPEND	0x2
#define PS2GS_REG_SMODE2_DPMS_OFF	0x3

#define to_ps2gs_crtc(c)	container_of(c, struct ps2gs_crtc, crtc)

//static void ps2gs_crtc_set_display_timing(struct ps2gs_crtc *gscrtc)
//{
//	FUNC_DEBUG();
//}

static void ps2gs_crtc_start(struct ps2gs_crtc *gscrtc)
{
	FUNC_DEBUG();

	if (gscrtc->started)
		return;

	/* Configure display timings */
	//ps2gs_crtc_set_display_timing(gscrtc);

	/* Setup plane */
	gscrtc->gsplane->enabled = true;
	ps2gs_plane_setup(gscrtc->gsplane);

	/* VESA DPMS ON */
	outq(GS_SET_SMODE2(1, 0, gscrtc->dpms), GS_REG_SMODE2);

	/* Enable the read circuit */
	outq(GS_SET_PMODE(0, 1, 0, 1, 0, 0x80), GS_REG_PMODE);

	gscrtc->started = true;
}

static void ps2gs_crtc_stop(struct ps2gs_crtc *gscrtc)
{
	FUNC_DEBUG();

	if (!gscrtc->started)
		return;

	gscrtc->gsplane->enabled = false;

	/* VESA DPMS OFF */
	outq(GS_SET_SMODE2(1, 0, gscrtc->dpms), GS_REG_SMODE2);

	/* Disable the read circuits */
	outq(GS_SET_PMODE(0, 0, 0, 1, 0, 0x80), GS_REG_PMODE);

	gscrtc->started = false;
}

void ps2gs_crtc_suspend(struct ps2gs_crtc *gscrtc)
{
	FUNC_DEBUG();

	ps2gs_crtc_stop(gscrtc);
}

void ps2gs_crtc_resume(struct ps2gs_crtc *gscrtc)
{
	FUNC_DEBUG();

	if (gscrtc->dpms != PS2GS_REG_SMODE2_DPMS_ON)
		return;

	ps2gs_crtc_start(gscrtc);
}

static void ps2gs_crtc_update_base(struct ps2gs_crtc *gscrtc)
{
	struct drm_crtc *crtc = &gscrtc->crtc;

	FUNC_DEBUG();

	ps2gs_plane_compute_base(gscrtc->gsplane, crtc->fb);
	ps2gs_plane_setup(gscrtc->gsplane);
}

static void ps2gs_crtc_dpms(struct drm_crtc *crtc, int mode)
{
	struct ps2gs_crtc *gscrtc = to_ps2gs_crtc(crtc);
	int gsmode;

	FUNC_DEBUG();

	switch(mode)
	{
	case DRM_MODE_DPMS_ON:		gsmode = PS2GS_REG_SMODE2_DPMS_ON;	break;
	case DRM_MODE_DPMS_STANDBY:	gsmode = PS2GS_REG_SMODE2_DPMS_STANDBY;	break;
	case DRM_MODE_DPMS_SUSPEND:	gsmode = PS2GS_REG_SMODE2_DPMS_SUSPEND;	break;
	case DRM_MODE_DPMS_OFF:		gsmode = PS2GS_REG_SMODE2_DPMS_OFF;	break;
	default: return;
	}

	if (gscrtc->dpms == gsmode)
		return;

	gscrtc->dpms = mode;

	if (gsmode == PS2GS_REG_SMODE2_DPMS_ON) {
		ps2gs_crtc_start(gscrtc);
	} else {
		ps2gs_crtc_stop(gscrtc);
	}
}

static bool ps2gs_crtc_mode_fixup(struct drm_crtc *crtc,
				    const struct drm_display_mode *mode,
				    struct drm_display_mode *adjusted_mode)
{
	FUNC_DEBUG();

	/* TODO Fixup modes */
	return true;
}

static void ps2gs_crtc_mode_prepare(struct drm_crtc *crtc)
{
	struct ps2gs_crtc *gscrtc = to_ps2gs_crtc(crtc);

	FUNC_DEBUG();

	gscrtc->dpms = PS2GS_REG_SMODE2_DPMS_OFF;
	ps2gs_crtc_stop(gscrtc);
}

static int ps2gs_crtc_mode_set(struct drm_crtc *crtc,
				 struct drm_display_mode *mode,
				 struct drm_display_mode *adjusted_mode,
				 int x, int y,
				 struct drm_framebuffer *old_fb)
{
	struct ps2gs_crtc *gscrtc = to_ps2gs_crtc(crtc);
	int crtc_x;
	int crtc_y;

	FUNC_DEBUG();

	crtc_x = mode->htotal - mode->hsync_start;
	crtc_y = mode->vtotal - mode->vsync_start;

	/*
	 * HACK: The CRTC is set to PAL mode by loader, but we are trying to set VGA.
	 * Center the 640x480 of VGA to the 720x576 of PAL
	 */
	crtc_x += (720 - 640) / 2;
	crtc_y += (576 - 480) / 2;

	return ps2gs_plane_mode_set(gscrtc->gsplane, crtc, crtc->fb,
			crtc_x, crtc_y, mode->hdisplay, mode->vdisplay,
			x, y, mode->hdisplay, mode->vdisplay);
}

static void ps2gs_crtc_mode_commit(struct drm_crtc *crtc)
{
	struct ps2gs_crtc *gscrtc = to_ps2gs_crtc(crtc);

	FUNC_DEBUG();

	gscrtc->dpms = PS2GS_REG_SMODE2_DPMS_ON;
	ps2gs_crtc_start(gscrtc);
}

static int ps2gs_crtc_mode_set_base(struct drm_crtc *crtc, int x, int y,
				      struct drm_framebuffer *old_fb)
{
	struct ps2gs_crtc *gscrtc = to_ps2gs_crtc(crtc);

	FUNC_DEBUG();

	gscrtc->gsplane->src_x = x;
	gscrtc->gsplane->src_y = y;

	ps2gs_crtc_update_base(to_ps2gs_crtc(crtc));

	return 0;
}

static void ps2gs_crtc_disable(struct drm_crtc *crtc)
{
	FUNC_DEBUG();

	ps2gs_crtc_dpms(crtc, DRM_MODE_DPMS_OFF);
}

static void ps2gs_crtc_load_lut(struct drm_crtc *crtc)
{
	//FUNC_DEBUG();
}

static const struct drm_crtc_helper_funcs crtc_helper_funcs = {
	.dpms		= ps2gs_crtc_dpms,
	.mode_fixup	= ps2gs_crtc_mode_fixup,
	.prepare	= ps2gs_crtc_mode_prepare,
	.commit		= ps2gs_crtc_mode_commit,
	.mode_set	= ps2gs_crtc_mode_set,
	.mode_set_base	= ps2gs_crtc_mode_set_base,
	.disable	= ps2gs_crtc_disable,
	.load_lut	= ps2gs_crtc_load_lut,
};

void ps2gs_crtc_cancel_page_flip(struct ps2gs_crtc *gscrtc,
				   struct drm_file *file)
{
	struct drm_pending_vblank_event *event;
	struct drm_device *dev = gscrtc->crtc.dev;
	unsigned long flags;

	FUNC_DEBUG();

	/* Destroy the pending vertical blanking event associated with the
	 * pending page flip, if any, and disable vertical blanking interrupts.
	 */
	spin_lock_irqsave(&dev->event_lock, flags);
	event = gscrtc->event;
	if (event && event->base.file_priv == file) {
		gscrtc->event = NULL;
		event->base.destroy(&event->base);
		drm_vblank_put(dev, 0);
	}
	spin_unlock_irqrestore(&dev->event_lock, flags);
}

static void ps2gs_crtc_finish_page_flip(struct ps2gs_crtc *gscrtc)
{
	struct drm_pending_vblank_event *event;
	struct drm_device *dev = gscrtc->crtc.dev;
	unsigned long flags;

	//FUNC_DEBUG();

	spin_lock_irqsave(&dev->event_lock, flags);
	event = gscrtc->event;
	gscrtc->event = NULL;
	spin_unlock_irqrestore(&dev->event_lock, flags);

	if (event == NULL)
		return;

	spin_lock_irqsave(&dev->event_lock, flags);
	drm_send_vblank_event(dev, 0, event);
	spin_unlock_irqrestore(&dev->event_lock, flags);

	drm_vblank_put(dev, 0);
}

static int ps2gs_crtc_page_flip(struct drm_crtc *crtc,
				  struct drm_framebuffer *fb,
				  struct drm_pending_vblank_event *event)
{
	struct ps2gs_crtc *gscrtc = to_ps2gs_crtc(crtc);
	struct drm_device *dev = gscrtc->crtc.dev;
	unsigned long flags;

	FUNC_DEBUG();

	spin_lock_irqsave(&dev->event_lock, flags);
	if (gscrtc->event != NULL) {
		spin_unlock_irqrestore(&dev->event_lock, flags);
		return -EBUSY;
	}
	spin_unlock_irqrestore(&dev->event_lock, flags);

	crtc->fb = fb;
	ps2gs_crtc_update_base(gscrtc);

	if (event) {
		event->pipe = 0;
		drm_vblank_get(dev, 0);
		spin_lock_irqsave(&dev->event_lock, flags);
		gscrtc->event = event;
		spin_unlock_irqrestore(&dev->event_lock, flags);
	}

	return 0;
}

static const struct drm_crtc_funcs crtc_funcs = {
	.destroy	= drm_crtc_cleanup,
	.set_config	= drm_crtc_helper_set_config,
	.page_flip	= ps2gs_crtc_page_flip,
};

static int ps2fbd(void *arg)
{
	struct ps2gs_crtc *gscrtc = arg;

	set_freezable();
	while (!kthread_should_stop()) {
		try_to_freeze();
		set_current_state(TASK_INTERRUPTIBLE);

		schedule();

		ps2gs_fb_redraw(gscrtc->crtc.fb);
	}
	return 0;
}

int ps2gs_crtc_create(struct ps2gs_device *gs)
{
	struct ps2gs_crtc *gscrtc = &gs->gscrtc;
	struct drm_crtc *crtc = &gscrtc->crtc;
	struct task_struct *task;
	int ret;

	FUNC_DEBUG();

	task = kthread_run(ps2fbd, gscrtc, "ps2gs redraw");
	if (IS_ERR(task)) {
		printk("ps2gs: kthread_run failed\n");
		return -EINVAL;
	}

	gscrtc->task = task;
	gscrtc->mmio_offset = 0;
	gscrtc->dpms = DRM_MODE_DPMS_ON;
	gscrtc->gsplane = &gs->gsplane;

	ret = drm_crtc_init(gs->ddev, crtc, &crtc_funcs);
	if (ret < 0)
		return ret;

	drm_crtc_helper_add(crtc, &crtc_helper_funcs);

	return 0;
}

void ps2gs_crtc_enable_vblank(struct ps2gs_crtc *gscrtc, bool enable)
{
	FUNC_DEBUG();

	if (enable) {
		// TODO: enable
	} else {
		// TODO: disable
	}
}

void ps2gs_crtc_irq(struct ps2gs_crtc *gscrtc)
{
	//FUNC_DEBUG();

	wake_up_process(gscrtc->task);

	drm_handle_vblank(gscrtc->crtc.dev, 0);
	ps2gs_crtc_finish_page_flip(gscrtc);
}
