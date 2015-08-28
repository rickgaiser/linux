/*
 * ps2gs_drv.c  --  Playstation2 GS DRM driver
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

#include <linux/io.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/slab.h>

#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_fb_cma_helper.h>
#include <drm/drm_gem_cma_helper.h>

#include "ps2gs_crtc.h"
#include "ps2gs_drv.h"
#include "ps2gs_kms.h"
#include "ps2gs_fbdev.h"
#include "ps2gs_regs.h"

/* -----------------------------------------------------------------------------
 * DRM operations
 */

static int ps2gs_unload(struct drm_device *dev)
{
	struct ps2gs_device *gs = dev->dev_private;

	FUNC_DEBUG();

	ps2gs_fbdev_fini(gs);
	drm_kms_helper_poll_fini(dev);
	drm_mode_config_cleanup(dev);
	drm_vblank_cleanup(dev);
	drm_irq_uninstall(dev);

	dev->dev_private = NULL;

	return 0;
}

static int ps2gs_load(struct drm_device *dev, unsigned long flags)
{
	struct platform_device *pdev = dev->platformdev;
	struct ps2gs_device *gs;
	//struct resource *ioarea;
	//struct resource *mem;
	int ret;

	FUNC_DEBUG();

	gs = devm_kzalloc(&pdev->dev, sizeof(*gs), GFP_KERNEL);
	if (gs == NULL) {
		dev_err(dev->dev, "failed to allocate private data\n");
		return -ENOMEM;
	}

	gs->dev = &pdev->dev;
	gs->ddev = dev;
	dev->dev_private = gs;
#if 0
	/* I/O resources */
	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (mem == NULL) {
		dev_err(&pdev->dev, "failed to get memory resource\n");
		return -EINVAL;
	}

	ioarea = devm_request_mem_region(&pdev->dev, mem->start,
					 resource_size(mem), pdev->name);
	if (ioarea == NULL) {
		dev_err(&pdev->dev, "failed to request memory region\n");
		return -EBUSY;
	}

	gs->mmio = devm_ioremap_nocache(&pdev->dev, ioarea->start,
					  resource_size(ioarea));
	if (gs->mmio == NULL) {
		dev_err(&pdev->dev, "failed to remap memory resource\n");
		return -ENOMEM;
	}
#endif
	/* DRM/KMS objects */
	ret = ps2gs_modeset_init(gs);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to initialize DRM/KMS\n");
		goto done;
	}

	/* IRQ and vblank handling */
	ret = drm_vblank_init(dev, 1);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to initialize vblank\n");
		goto done;
	}

	ret = drm_irq_install(dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to install IRQ handler\n");
		goto done;
	}

	platform_set_drvdata(pdev, gs);

done:
	if (ret)
		ps2gs_unload(dev);

	return ret;
}

static void ps2gs_preclose(struct drm_device *dev, struct drm_file *file)
{
	struct ps2gs_device *gs = dev->dev_private;

	FUNC_DEBUG();

	ps2gs_crtc_cancel_page_flip(&gs->gscrtc, file);
}

static irqreturn_t ps2gs_irq(int irq, void *arg)
{
	struct drm_device *dev = arg;
	struct ps2gs_device *gs = dev->dev_private;

	//FUNC_DEBUG();

	ps2gs_crtc_irq(&gs->gscrtc);

	return IRQ_HANDLED;
}

static void ps2gs_lastclose(struct drm_device *dev)
{
	struct ps2gs_device *gs = dev->dev_private;

	FUNC_DEBUG();

	drm_fbdev_cma_restore_mode(gs->fbdev);
}

static int ps2gs_enable_vblank(struct drm_device *dev, int crtc)
{
	struct ps2gs_device *gs = dev->dev_private;

	FUNC_DEBUG();

	ps2gs_crtc_enable_vblank(&gs->gscrtc, true);

	return 0;
}

static void ps2gs_disable_vblank(struct drm_device *dev, int crtc)
{
	struct ps2gs_device *gs = dev->dev_private;

	FUNC_DEBUG();

	ps2gs_crtc_enable_vblank(&gs->gscrtc, false);
}

static const struct file_operations ps2gs_fops = {
	.owner		= THIS_MODULE,
	.open		= drm_open,
	.release	= drm_release,
	.unlocked_ioctl	= drm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= drm_compat_ioctl,
#endif
	.poll		= drm_poll,
	.read		= drm_read,
	.fasync		= drm_fasync,
	.llseek		= no_llseek,
	.mmap		= drm_gem_cma_mmap,
};

static struct drm_driver ps2gs_driver = {
	.driver_features	= DRIVER_HAVE_IRQ | DRIVER_GEM | DRIVER_MODESET,
	.load			= ps2gs_load,
	.unload			= ps2gs_unload,
	.preclose		= ps2gs_preclose,
	.lastclose		= ps2gs_lastclose,
	.irq_handler		= ps2gs_irq,
	.get_vblank_counter	= drm_vblank_count,
	.enable_vblank		= ps2gs_enable_vblank,
	.disable_vblank		= ps2gs_disable_vblank,
	.gem_free_object	= drm_gem_cma_free_object,
	.gem_vm_ops		= &drm_gem_cma_vm_ops,
	.dumb_create		= ps2gs_dumb_create,
	.dumb_map_offset	= drm_gem_cma_dumb_map_offset,
	.dumb_destroy		= drm_gem_cma_dumb_destroy,
	.fops			= &ps2gs_fops,
	.name			= "ps2gs",
	.desc			= "Playstation2 GS",
	.date			= "20150101",
	.major			= 1,
	.minor			= 0,
};

/* -----------------------------------------------------------------------------
 * Power management
 */

#if CONFIG_PM_SLEEP
static int ps2gs_pm_suspend(struct device *dev)
{
	struct ps2gs_device *gs = dev_get_drvdata(dev);

	FUNC_DEBUG();

	drm_kms_helper_poll_disable(gs->ddev);
	/* TODO Suspend the CRTC */

	return 0;
}

static int ps2gs_pm_resume(struct device *dev)
{
	struct ps2gs_device *gs = dev_get_drvdata(dev);

	FUNC_DEBUG();

	/* TODO Resume the CRTC */

	drm_kms_helper_poll_enable(gs->ddev);
	return 0;
}
#endif

static const struct dev_pm_ops ps2gs_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(ps2gs_pm_suspend, ps2gs_pm_resume)
};

/* -----------------------------------------------------------------------------
 * Platform driver
 */

static int ps2gs_probe(struct platform_device *pdev)
{
	FUNC_DEBUG();

	return drm_platform_init(&ps2gs_driver, pdev);
}

static int ps2gs_remove(struct platform_device *pdev)
{
	FUNC_DEBUG();

	drm_platform_exit(&ps2gs_driver, pdev);

	return 0;
}

static struct platform_driver ps2gs_platform_driver = {
	.probe		= ps2gs_probe,
	.remove		= ps2gs_remove,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "ps2gs",
		.pm	= &ps2gs_pm_ops,
	},
};

module_platform_driver(ps2gs_platform_driver);

MODULE_AUTHOR("Rick Gaiser <rgaiser@gmail.com>");
MODULE_DESCRIPTION("Playstation2 DRM Driver");
MODULE_LICENSE("GPL");
