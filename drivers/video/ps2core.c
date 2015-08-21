
/* Copyright 2010 Mega Man */

/* TBD: Unfinished state. Rework code. */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/semaphore.h>
#include <linux/hardirq.h>
#include <linux/dma-mapping.h>

#include <linux/ps2/ee.h>
#include <linux/ps2/gs.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include <asm/mach-ps2/dma.h>
#include <asm/mach-ps2/eedev.h>
#include <asm/mach-ps2/ps2con.h>

#ifdef CONFIG_T10000
static int defaultmode = PS2_GS_VESA, defaultres = PS2_GS_640x480;
static int default_w = 640, default_h = 480;
#else
static int defaultmode = PS2_GS_NTSC, defaultres = PS2_GS_NOINTERLACE;
static int default_w = 640, default_h = 448;
#endif

void ps2con_initinfo(struct ps2_screeninfo *info)
{
	info->fbp = 0;
	info->psm = PS2_GS_PSMCT32;
	info->mode = defaultmode;
	info->res = defaultres;
	info->w = default_w;
	info->h = default_h;

	if (info->w * info->h > 1024 * 1024)
		info->psm = PS2_GS_PSMCT16;
}

const static struct {
	int w, h;
} reslist[4][4] = {
	{
		{640, 480},
		{800, 600},
		{1024, 768},
		{1280, 1024},
	}, {
		{720, 480},
		{1920, 1080},
		{1280, 720},
		{-1, -1},
	},
	{
		{640, 224},
		{640, 448},
		{-1, -1},
		{-1, -1},
	},
	{
		{640, 240},
		{640, 480},
		{-1, -1},
		{-1, -1},
	},
};

static int __init crtmode_setup(char *options)
{
	int maxres;

	int rrate, w, h;

	if (!options || !*options)
		return 0;

	if (strnicmp(options, "vesa", 4) == 0) {
		options += 4;
		defaultmode = PS2_GS_VESA;
		maxres = 4;
	} else if (strnicmp(options, "dtv", 3) == 0) {
		options += 3;
		defaultmode = PS2_GS_DTV;
		maxres = 3;
	} else if (strnicmp(options, "ntsc", 4) == 0) {
		options += 4;
		defaultmode = PS2_GS_NTSC;
		maxres = 2;
	} else if (strnicmp(options, "pal", 3) == 0) {
		options += 3;
		defaultmode = PS2_GS_PAL;
		maxres = 2;
	} else
		return 0;

	defaultres = 0;
	if (*options >= '0' && *options <= '9') {
		defaultres = *options - '0';
		options++;
		if (defaultres >= maxres)
			defaultres = 0;
	}

	if (defaultmode == PS2_GS_VESA && *options == ',') {
		rrate = simple_strtoul(options + 1, &options, 0);
		if (rrate == 60)
			defaultres |= PS2_GS_60Hz;
		else if (rrate == 75)
			defaultres |= PS2_GS_75Hz;
	}

	w = default_w = reslist[defaultmode][defaultres & 0xff].w;
	h = default_h = reslist[defaultmode][defaultres & 0xff].h;

	if (*options == ':') {
		w = simple_strtoul(options + 1, &options, 0);
		if (*options == ',' || tolower(*options) == 'x') {
			h = simple_strtoul(options + 1, &options, 0);
		}
		if (w > 0)
			default_w = w;
		if (h > 0)
			default_h = h;
	}

	return 1;
}

/* Convert resolution to ps2 mode format. */
int ps2con_get_resolution(int mode, int w, int h, int rate)
{
	int res;

	for (res = 0; res < 4; res++) {
		if (reslist[mode][res & 0xff].w == w) {
			if (reslist[mode][res & 0xff].h == h) {
				if (mode == PS2_GS_VESA) {
					if (rate >= 75) {
						res |= PS2_GS_75Hz;
					} else if (rate >= 60) {
						res |= PS2_GS_60Hz;
					}
				}
				return res;
			}
		}
	}
	return -1;
}

__setup("crtmode=", crtmode_setup);

MODULE_LICENSE("GPL");
