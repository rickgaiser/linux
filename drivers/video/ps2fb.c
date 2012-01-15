/* Copyright 2011 Mega Man */
/* TBD: Unfinished state. Rework code. */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/platform_device.h>

#include <asm/page.h>
#include <asm/cacheflush.h>

#include <asm/mach-ps2/ps2.h>
#include <asm/mach-ps2/gsfunc.h>
#include <asm/mach-ps2/eedev.h>
#include <asm/mach-ps2/dma.h>

/* TBD: Seems not to be needed, because this is not used by xorg. */
#define PIXMAP_SIZE (4 * 2048 * 32/8)

/** Bigger 1 bit color images will be devided into smaller images with the following maximum width. */
#define PATTERN_MAX_X 16
/** Bigger 1 bit color images will be devided into smaller images with the following maximum height. */
#define PATTERN_MAX_Y 8

#if 0
#define DPRINTK(args...) ps2_printf(args)
#else
#define DPRINTK(args...)
#endif

static int ps2fb_init(void);
static void ps2fb_redraw_timer_handler(unsigned long dev_addr);

/* TBD: Move functions declarations to header file. */
void ps2con_initinfo(struct ps2_screeninfo *info);
void ps2con_gsp_init(void);
u64 *ps2con_gsp_alloc(int request, int *avail);
void ps2con_gsp_send(int len, int flushall);

struct ps2fb_par
{
	u32 pseudo_palette[256];
	u32 opencnt;
	/* TBD: add members. */
};

static struct fb_fix_screeninfo ps2fb_fix /* TBD: __devinitdata */ = {
	.id =		"PS2 GS", 
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_TRUECOLOR, /* TBD: FB_VISUAL_PSEUDOCOLOR, */
	.xpanstep =	1,
	.ypanstep =	1,
	.ywrapstep =	1, 
	.accel =	FB_ACCEL_NONE,
};

static struct ps2_screeninfo defaultinfo;

static struct timer_list redraw_timer = {
    function: ps2fb_redraw_timer_handler
};

/**
 *	ps2fb_open - Optional function. Called when the framebuffer is
 *		     first accessed.
 *	@info: frame buffer structure that represents a single frame buffer
 *	@user: tell us if the userland (value=1) or the console is accessing
 *	       the framebuffer. 
 *
 *	This function is the first function called in the framebuffer api.
 *	Usually you don't need to provide this function. The case where it 
 *	is used is to change from a text mode hardware state to a graphics
 * 	mode state. 
 *
 *	Returns negative errno on error, or zero on success.
 */
static int ps2fb_open(struct fb_info *info, int user)
{
    struct ps2fb_par *par;

	DPRINTK("ps2fb_open: user %d\n", user);

    par = info->par;
	if (user) {
		if (par->opencnt == 0) {
			/* Start timer for redrawing screen, because application could use mmap. */
			redraw_timer.data = (unsigned long) info;
    		redraw_timer.expires = jiffies + HZ / 50;
    		add_timer(&redraw_timer);
		}
		par->opencnt++;
	}
    return 0;
}

/**
 *	ps2fb_release - Optional function. Called when the framebuffer 
 *			device is closed. 
 *	@info: frame buffer structure that represents a single frame buffer
 *	@user: tell us if the userland (value=1) or the console is accessing
 *	       the framebuffer. 
 *	
 *	Thus function is called when we close /dev/fb or the framebuffer 
 *	console system is released. Usually you don't need this function.
 *	The case where it is usually used is to go from a graphics state
 *	to a text mode state.
 *
 *	Returns negative errno on error, or zero on success.
 */
static int ps2fb_release(struct fb_info *info, int user)
{
    struct ps2fb_par *par;

	DPRINTK("ps2fb_release: user %d\n", user);

    par = info->par;
	if (user) {
		/* TBD: Check if mmap is also removed. */
		par->opencnt--;
		if (par->opencnt == 0) {
			/* Redrawing shouldn't be needed after closing by application. */
			del_timer(&redraw_timer);
		}
	}
    return 0;
}

/**
 * Paints a filled rectangle.
 *
 * Coordinate system:
 * 0-----> x++
 * |
 * |
 * |
 * \/
 * y++
 * @color Color format ABGR
 *
 */
static void ps2_paintrect(int sx, int sy, int width, int height, uint32_t color)
{
    u64 *gsp;
    int ctx = 0;

    if ((gsp = ps2con_gsp_alloc(ALIGN16(6 * 8), NULL)) == NULL) {
		return;
	}

    *gsp++ = PS2_GIFTAG_SET_TOPHALF(1, 1, 0, 0, PS2_GIFTAG_FLG_REGLIST, 4);
    *gsp++ = 0x5510;
	/* PRIM */
    *gsp++ = 0x006 + (ctx << 9);
	/* RGBAQ */
    *gsp++ = color;
	/* XYZ2 */
    *gsp++ = PACK32(sx * 16, sy * 16);
	/* XYZ2 */
    *gsp++ = PACK32((sx + width) * 16, (sy + height) * 16);

    ps2con_gsp_send(ALIGN16(6 * 8), 0);
}

static void *ps2_addpattern1(void *gsp, const unsigned char *data, int width, int height, uint32_t bgcolor, uint32_t fgcolor, int lineoffset)
{
	int y;
	int x;
	int offset;
    u32 *p32;

	offset = 0;
	p32 = (u32 *) gsp;
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			u32 v;

			v = data[(offset + x) / 8];
			v >>= 7 - ((offset + x) & 7);
			v &= 1;
			if (v) {
				*p32++ = fgcolor;
			} else {
				*p32++ = bgcolor;
			}
		}
		offset += lineoffset;
	}
	return (void *)p32;
}

/* Paint image from data with 1 bit per pixel. */
static void ps2_paintsimg1(int sx, int sy, int width, int height, uint32_t bgcolor, uint32_t fgcolor, const unsigned char *data, int lineoffset)
{
    u64 *gsp;
    void *gsp_h;
    int gspsz; /* Available DMA packet size. */
	int fbw = (defaultinfo.w + 63) / 64;
	unsigned int packetlen;

	if ((gsp = ps2con_gsp_alloc(ALIGN16(12 * 8 + sizeof(fgcolor) * width * height), &gspsz)) == NULL) {
		DPRINTK("Failed ps2con_gsp_alloc with w %d h %d size %lu\n", width, height, ALIGN16(12 * 8 + sizeof(fgcolor) * width * height));
	    return;
	}
	gsp_h = gsp;

	*gsp++ = PS2_GIFTAG_SET_TOPHALF(4, 0, 0, 0, PS2_GIFTAG_FLG_PACKED, 1);
	*gsp++ = 0x0e;		/* A+D */
	*gsp++ = (u64)0 | ((u64)defaultinfo.fbp << 32) |
	    ((u64)fbw << 48) | ((u64)defaultinfo.psm << 56);
	*gsp++ = PS2_GS_BITBLTBUF;
	*gsp++ = PACK64(0, PACK32(sx, sy));
	*gsp++ = PS2_GS_TRXPOS;
	*gsp++ = PACK64(width, height);
	*gsp++ = PS2_GS_TRXREG;
	*gsp++ = 0;		/* host to local */
	*gsp++ = PS2_GS_TRXDIR;

	*gsp++ = PS2_GIFTAG_SET_TOPHALF(ALIGN16(sizeof(fgcolor) * width * height) / 16, 1, 0, 0, PS2_GIFTAG_FLG_IMAGE, 0);
	*gsp++ = 0;

	gsp = ps2_addpattern1(gsp, data, width, height, bgcolor, fgcolor, lineoffset);
	packetlen = ((void *) ALIGN16(gsp)) - gsp_h;
	ps2con_gsp_send(packetlen, 0);
}

/* Paint image from data with 8 bit per pixel. */
static void *ps2_addpattern8(void *gsp, const unsigned char *data, int width, int height, uint32_t *palette, int lineoffset)
{
	int y;
	int x;
	int offset;
    u32 *p32;

	offset = 0;
	p32 = (u32 *) gsp;
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			u32 v;

			v = data[offset + x];
			*p32++ = palette[v];
		}
		offset += lineoffset;
	}
	return (void *)p32;
}

static void ps2_paintsimg8(int sx, int sy, int width, int height, uint32_t *palette, const unsigned char *data, int lineoffset)
{
    u64 *gsp;
    void *gsp_h;
    int gspsz; /* Available size. */
	int fbw = (defaultinfo.w + 63) / 64;
#if 0
	void *buffer;
#endif
	unsigned int packetlen;

	if ((gsp = ps2con_gsp_alloc(ALIGN16(12 * 8 + sizeof(palette[0]) * width * height), &gspsz)) == NULL) {
		DPRINTK("Failed ps2con_gsp_alloc with w %d h %d size %lu\n", width, height, ALIGN16(12 * 8 + sizeof(palette[0]) * width * height));
	    return;
	}
	gsp_h = gsp;

	*gsp++ = PS2_GIFTAG_SET_TOPHALF(4, 0, 0, 0, PS2_GIFTAG_FLG_PACKED, 1);
	/* A+D */
	*gsp++ = 0x0e;
	*gsp++ = (u64)0 | ((u64)defaultinfo.fbp << 32) |
	    ((u64)fbw << 48) | ((u64)defaultinfo.psm << 56);
	*gsp++ = PS2_GS_BITBLTBUF;
	*gsp++ = PACK64(0, PACK32(sx, sy));
	*gsp++ = PS2_GS_TRXPOS;
	*gsp++ = PACK64(width, height);
	*gsp++ = PS2_GS_TRXREG;
	/* host to local */
	*gsp++ = 0;
	*gsp++ = PS2_GS_TRXDIR;

	*gsp++ = PS2_GIFTAG_SET_TOPHALF(ALIGN16(sizeof(palette[0]) * width * height) / 16, 1, 0, 0, PS2_GIFTAG_FLG_IMAGE, 0);
	*gsp++ = 0;

#if 1
	/* TBD: Optimize, don't copy pixel data, use DMA instead. */
	gsp = ps2_addpattern8(gsp, data, width, height, palette, lineoffset);
	packetlen = ((void *) ALIGN16(gsp)) - gsp_h;
	ps2con_gsp_send(packetlen, 0);
#else
	ps2con_gsp_send(((void *) gsp) - gsp_h, 0);
	packetlen = ALIGN16(4 * width * height);

	buffer = kmalloc(packetlen, GFP_DMA32);
	if (buffer != NULL) {
		/* TBD: Still slow copy. */
		/* Convert data. */
		ps2_addpattern8(buffer, data, width, height, palette, lineoffset);
		dma_cache_wback((unsigned long) buffer, packetlen);
		ps2sdma_send(DMA_GIF, buffer, packetlen, 0);
		kfree(buffer);
	} else {
		/* TBD: Fast, but clut missing and 8 bit format. */
		dma_cache_wback((unsigned long) data, packetlen);
		ps2sdma_send(DMA_GIF, data, packetlen, 0);
	}
#endif
}

#if 0
/* Paint image from data with 8 bit per pixel. */
static void *ps2_addpattern32(void *gsp, const uint32_t *data, int width, int height, int lineoffset)
{
	int y;
	int x;
	int offset;
    u32 *p32;

	offset = 0;
	p32 = (u32 *) gsp;
	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x++) {
			*p32++ = data[offset + x];
		}
		offset += lineoffset / (32/8);
	}
	return (void *)p32;
}
#endif

void ps2fb_dma_send(const void *data, unsigned long len)
{
	unsigned long page;
	unsigned long start;
	unsigned long end;
	unsigned long s;
	unsigned long offset;
	void *cur_start;
	unsigned long cur_size;

	/* TBD: Cache is flushed by ps2con_gsp_send, but this may change. */

	start = (unsigned long) data;
	cur_start = 0;
	cur_size = 0;

	s = start & (PAGE_SIZE - 1);
	s = PAGE_SIZE - s;
	if (s < PAGE_SIZE) {
		if (s > len) {
			s = len;
		}
		offset = start & (PAGE_SIZE - 1);
		// TBD: vmalloc_to_pfn is only working with redraw handler.
		cur_start = phys_to_virt(PFN_PHYS(vmalloc_to_pfn((void *) start))) + offset;
		cur_size = ALIGN16(s);
		start += s;
		len -= s;
	}

	end = ALIGN16(start + len);

	for (page = start; page < end; page += PAGE_SIZE) {
		void *addr;
		unsigned long size;

		addr = phys_to_virt(PFN_PHYS(vmalloc_to_pfn((void *) page)));
		size = end - page;
		if (size > PAGE_SIZE) {
			size = PAGE_SIZE;
		}

		if (cur_size > 0) {
			if (addr == (cur_start + cur_size)) {
				/* contigous physical memory. */
				cur_size += size;
			} else {
				/* Start DMA transfer for current data. */
				ps2sdma_send(DMA_GIF, cur_start, ALIGN16(cur_size), 0);

				cur_size = size;
				cur_start = addr;
			}
		} else {
			cur_start = addr;
			cur_size = size;
		}
	}
	if (cur_size > 0) {
		/* Start DMA transfer for current data. */
		ps2sdma_send(DMA_GIF, cur_start, ALIGN16(cur_size), 0);

		cur_size = 0;
		cur_start = 0;
	}
}


static void ps2_paintsimg32(int sx, int sy, int width, int height, const uint32_t *data)
{
    u64 *gsp;
    void *gsp_h;
    int gspsz; /* Available size. */
	int fbw = (defaultinfo.w + 63) / 64;

	if ((gsp = ps2con_gsp_alloc(ALIGN16(12 * 8), &gspsz)) == NULL) {
		DPRINTK("Failed ps2con_gsp_alloc with w %d h %d size %lu\n", width, height, ALIGN16(12 * 8 + sizeof(data[0]) * width * height));
	    return;
	}
	gsp_h = gsp;

	*gsp++ = PS2_GIFTAG_SET_TOPHALF(4, 0, 0, 0, PS2_GIFTAG_FLG_PACKED, 1);
	/* A+D */
	*gsp++ = 0x0e;
	*gsp++ = (u64)0 | ((u64)defaultinfo.fbp << 32) |
	    ((u64)fbw << 48) | ((u64)defaultinfo.psm << 56);
	*gsp++ = PS2_GS_BITBLTBUF;
	*gsp++ = PACK64(0, PACK32(sx, sy));
	*gsp++ = PS2_GS_TRXPOS;
	*gsp++ = PACK64(width, height);
	*gsp++ = PS2_GS_TRXREG;
	/* host to local */
	*gsp++ = 0;
	*gsp++ = PS2_GS_TRXDIR;

	*gsp++ = PS2_GIFTAG_SET_TOPHALF(ALIGN16(sizeof(data[0]) * width * height) / 16, 1, 0, 0, PS2_GIFTAG_FLG_IMAGE, 0);
	*gsp++ = 0;
	ps2con_gsp_send(((unsigned long) gsp) - ((unsigned long) gsp_h), 1);

	ps2fb_dma_send(data, ALIGN16(4 * width * height));
}

static void ps2fb_redraw(struct fb_info *info)
{
	int offset;
	int y;

	offset = (32/8) * defaultinfo.w;
	for (y = 0; y < defaultinfo.h; y += 64) {
		int h;

		h = defaultinfo.h - y;
		if (h > 64) {
			h = 64;
		}
		ps2_paintsimg32(0, y, 640, h, (void *) (info->fix.smem_start + y * offset));
	}
}

static void ps2fb_redraw_timer_handler(unsigned long data)
{
	ps2fb_redraw((void *) data);
	/* Redraw every 20 ms. */
	redraw_timer.expires = jiffies + HZ / 50;
	add_timer(&redraw_timer);
}

/**
 *      ps2fb_check_var - Optional function. Validates a var passed in. 
 *      @var: frame buffer variable screen structure
 *      @info: frame buffer structure that represents a single frame buffer 
 *
 *	Checks to see if the hardware supports the state requested by
 *	var passed in. This function does not alter the hardware state!!! 
 *	This means the data stored in struct fb_info and struct ps2fb_par do 
 *      not change. This includes the var inside of struct fb_info. 
 *	Do NOT change these. This function can be called on its own if we
 *	intent to only test a mode and not actually set it. The stuff in 
 *	modedb.c is a example of this. If the var passed in is slightly 
 *	off by what the hardware can support then we alter the var PASSED in
 *	to what we can do.
 *
 *      For values that are off, this function must round them _up_ to the
 *      next value that is supported by the hardware.  If the value is
 *      greater than the highest value supported by the hardware, then this
 *      function must return -EINVAL.
 *
 *      Exception to the above rule:  Some drivers have a fixed mode, ie,
 *      the hardware is already set at boot up, and cannot be changed.  In
 *      this case, it is more acceptable that this function just return
 *      a copy of the currently working var (info->var). Better is to not
 *      implement this function, as the upper layer will do the copying
 *      of the current var for you.
 *
 *      Note:  This is the only function where the contents of var can be
 *      freely adjusted after the driver has been registered. If you find
 *      that you have code outside of this function that alters the content
 *      of var, then you are doing something wrong.  Note also that the
 *      contents of info->var must be left untouched at all times after
 *      driver registration.
 *
 *	Returns negative errno on error, or zero on success.
 */
static int ps2fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	DPRINTK("ps2fb: %d %s() force 32 bit color\n", __LINE__, __FUNCTION__);
    /* TBD: Implement ps2fb_check_var() */
	var->bits_per_pixel = 32;
	var->red.offset = 0;
	var->red.length = 8;
	var->green.offset = 8;
	var->green.length = 8;
	var->blue.offset = 16;
	var->blue.length = 8;
	var->transp.offset = 24; /* TBD: Check, seems to be disabled. Not needed? */
	var->transp.length = 8;

	var->red.msb_right = 0;
	var->green.msb_right = 0;
	var->blue.msb_right = 0;
	var->transp.msb_right = 0;

	if (var->rotate) {
		return -EINVAL;
	}
    return 0;	   	
}

#if 0 /* TBD: Implement function. */
/**
 *      ps2fb_set_par - Optional function. Alters the hardware state.
 *      @info: frame buffer structure that represents a single frame buffer
 *
 *	Using the fb_var_screeninfo in fb_info we set the resolution of the
 *	this particular framebuffer. This function alters the par AND the
 *	fb_fix_screeninfo stored in fb_info. It doesn't not alter var in 
 *	fb_info since we are using that data. This means we depend on the
 *	data in var inside fb_info to be supported by the hardware. 
 *
 *      This function is also used to recover/restore the hardware to a
 *      known working state.
 *
 *	ps2fb_check_var is always called before ps2fb_set_par to ensure that
 *      the contents of var is always valid.
 *
 *	Again if you can't change the resolution you don't need this function.
 *
 *      However, even if your hardware does not support mode changing,
 *      a set_par might be needed to at least initialize the hardware to
 *      a known working state, especially if it came back from another
 *      process that also modifies the same hardware, such as X.
 *
 *      If this is the case, a combination such as the following should work:
 *
 *      static int ps2fb_check_var(struct fb_var_screeninfo *var,
 *                                struct fb_info *info)
 *      {
 *              *var = info->var;
 *              return 0;
 *      }
 *
 *      static int ps2fb_set_par(struct fb_info *info)
 *      {
 *              init your hardware here
 *      }
 *
 *	Returns negative errno on error, or zero on success.
 */
static int ps2fb_set_par(struct fb_info *info)
{
    struct ps2fb_par *par;

	DPRINTK("ps2fb: %d %s()\n", __LINE__, __FUNCTION__);

    par = info->par;
    /* TBD: Set hardware state. */
    return 0;	
}
#endif

/**
 *  	ps2fb_setcolreg - Optional function. Sets a color register.
 *      @regno: Which register in the CLUT we are programming 
 *      @red: The red value which can be up to 16 bits wide 
 *      @green: The green value which can be up to 16 bits wide 
 *      @blue:  The blue value which can be up to 16 bits wide.
 *      @transp: If supported, the alpha value which can be up to 16 bits wide.
 *      @info: frame buffer info structure
 * 
 *  	Set a single color register. The values supplied have a 16 bit
 *  	magnitude which needs to be scaled in this function for the hardware. 
 *	Things to take into consideration are how many color registers, if
 *	any, are supported with the current color visual. With truecolor mode
 *	no color palettes are supported. Here a pseudo palette is created
 *	which we store the value in pseudo_palette in struct fb_info. For
 *	pseudocolor mode we have a limited color palette. To deal with this
 *	we can program what color is displayed for a particular pixel value.
 *	DirectColor is similar in that we can program each color field. If
 *	we have a static colormap we don't need to implement this function. 
 * 
 *	Returns negative errno on error, or zero on success.
 */
static int ps2fb_setcolreg(unsigned regno, unsigned red, unsigned green,
			   unsigned blue, unsigned transp,
			   struct fb_info *info)
{
	// TBD: DPRINTK("ps2fb: %d %s()\n", __LINE__, __FUNCTION__);
    if (regno >= 256)  /* no. of hw registers */
       return -EINVAL;

	((u32 *) (info->pseudo_palette))[regno] =
		(red & 0xFF) << 0 |
		(green & 0xFF) << 8 |
		(blue & 0xFF) << 16 | 0x80000000; /* TBD: transp */
	// TBD: DPRINTK("ps2fb: %d %s() reg %d = 0x%08x r 0x%02x g 0x%02x b 0x%02x\n", __LINE__, __FUNCTION__, regno, ((u32 *) (info->pseudo_palette))[regno], red, green, blue);
    return 0;
}

#if 0 /* TBD: Implement functions. */
ssize_t ps2fb_read(struct fb_info *info, char __user *buf,
			   size_t count, loff_t *ppos)
{
	DPRINTK("ps2fb: %d %s()\n", __LINE__, __FUNCTION__);
	/* TBD: implement reading of framebuffer. */
	return count;
}

ssize_t ps2fb_write(struct fb_info *info, const char __user *buf,
			    size_t count, loff_t *ppos)
{
	DPRINTK("ps2fb: %d %s()\n", __LINE__, __FUNCTION__);
	/* TBD: implement writing of framebuffer. */
	return count;
}

/**
 *      ps2fb_blank - NOT a required function. Blanks the display.
 *      @blank_mode: the blank mode we want. 
 *      @info: frame buffer structure that represents a single frame buffer
 *
 *      Blank the screen if blank_mode != FB_BLANK_UNBLANK, else unblank.
 *      Return 0 if blanking succeeded, != 0 if un-/blanking failed due to
 *      e.g. a video mode which doesn't support it.
 *
 *      Implements VESA suspend and powerdown modes on hardware that supports
 *      disabling hsync/vsync:
 *
 *      FB_BLANK_NORMAL = display is blanked, syncs are on.
 *      FB_BLANK_HSYNC_SUSPEND = hsync off
 *      FB_BLANK_VSYNC_SUSPEND = vsync off
 *      FB_BLANK_POWERDOWN =  hsync and vsync off
 *
 *      If implementing this function, at least support FB_BLANK_UNBLANK.
 *      Return !0 for any modes that are unimplemented.
 *
 */
static int ps2fb_blank(int blank_mode, struct fb_info *info)
{
	DPRINTK("ps2fb: %d %s()\n", __LINE__, __FUNCTION__);
    /* ... */
    return 0;
}

/**
 *	ps2fb_sync - NOT a required function. Normally the accel engine 
 *		     for a graphics card take a specific amount of time.
 *		     Often we have to wait for the accelerator to finish
 *		     its operation before we can write to the framebuffer
 *		     so we can have consistent display output. 
 *
 *      @info: frame buffer structure that represents a single frame buffer
 *
 *      If the driver has implemented its own hardware-based drawing function,
 *      implementing this function is highly recommended.
 */
int ps2fb_sync(struct fb_info *info)
{
	DPRINTK("ps2fb: %d %s()\n", __LINE__, __FUNCTION__);
	return 0;
}
#endif

/**
 *      ps2fb_fillrect - REQUIRED function. Can use generic routines if 
 *		 	 non acclerated hardware and packed pixel based.
 *			 Draws a rectangle on the screen.		
 *
 *      @info: frame buffer structure that represents a single frame buffer
 *    @region: The structure representing the rectangular region we 
 *		 wish to draw to.
 *
 *	This drawing operation places/removes a retangle on the screen 
 *	depending on the rastering operation with the value of color which
 *	is in the current color depth format.
 */
void ps2fb_fillrect(struct fb_info *p, const struct fb_fillrect *region)
{
	uint32_t color;
#if 0
	DPRINTK("ps2fb: %d %s() dx %d dy %d w %d h %d col 0x%08x\n", __LINE__, __FUNCTION__,
		region->dx,
		region->dy,
		region->width,
		region->height,
		region->color);
#endif

	color = region->color & 0x00FFFFFF;
	color |= 0x80000000;
	/* TBD: handle region->rop ROP_COPY or ROP_XOR */
	ps2_paintrect(region->dx, region->dy, region->width, region->height, color);
/*	Meaning of struct fb_fillrect
 *
 *	@dx: The x and y corrdinates of the upper left hand corner of the 
 *	@dy: area we want to draw to. 
 *	@width: How wide the rectangle is we want to draw.
 *	@height: How tall the rectangle is we want to draw.
 *	@color:	The color to fill in the rectangle with. 
 *	@rop: The raster operation. We can draw the rectangle with a COPY
 *	      of XOR which provides erasing effect. 
 */
}

/**
 *      ps2fb_copyarea - REQUIRED function. Can use generic routines if
 *                       non acclerated hardware and packed pixel based.
 *                       Copies one area of the screen to another area.
 *
 *      @info: frame buffer structure that represents a single frame buffer
 *      @area: Structure providing the data to copy the framebuffer contents
 *	       from one region to another.
 *
 *      This drawing operation copies a rectangular area from one area of the
 *	screen to another area.
 */
void ps2fb_copyarea(struct fb_info *p, const struct fb_copyarea *area) 
{
	DPRINTK("ps2fb: %d %s()\n", __LINE__, __FUNCTION__);
/*
 *      @dx: The x and y coordinates of the upper left hand corner of the
 *      @dy: destination area on the screen.
 *      @width: How wide the rectangle is we want to copy.
 *      @height: How tall the rectangle is we want to copy.
 *      @sx: The x and y coordinates of the upper left hand corner of the
 *      @sy: source area on the screen.
 */
}

/**
 *      ps2fb_imageblit - REQUIRED function. Can use generic routines if
 *                        non acclerated hardware and packed pixel based.
 *                        Copies a image from system memory to the screen. 
 *
 *      @info: frame buffer structure that represents a single frame buffer
 *	@image:	structure defining the image.
 *
 *      This drawing operation draws a image on the screen. It can be a 
 *	mono image (needed for font handling) or a color image (needed for
 *	tux). 
 */
void ps2fb_imageblit(struct fb_info *p, const struct fb_image *image) 
{
	uint32_t fgcolor;
	uint32_t bgcolor;

	if (image->depth != 1) {
		DPRINTK("ps2fb: blit depth %d dx %d dy %d w %d h %d 0x%08x\n",
		image->depth,
		image->dx,
		image->dy,
		image->width,
		image->height,
		(unsigned int)image->data);
	}

	if (image->depth == 1) {
		int x;
		int y;
		int offset;

		fgcolor = ((u32*)(p->pseudo_palette))[image->fg_color];
		bgcolor = ((u32*)(p->pseudo_palette))[image->bg_color];
		offset = (image->width + 7) & ~7;
		for (x = 0; x < image->width; x += PATTERN_MAX_X)
		{
			int w;

			w = image->width - x;
			if (w > PATTERN_MAX_X) {
				w = PATTERN_MAX_X;
			}
			for (y = 0; y < image->height; y += PATTERN_MAX_Y)
			{
				int h;

				h = image->height - y;
				if (h > PATTERN_MAX_Y) {
					h = PATTERN_MAX_Y;
				}
				ps2_paintsimg1(image->dx + x, image->dy + y, w, h,
					bgcolor, fgcolor, image->data + (x + y * offset) / 8,
					offset);
			}
		}
	} else if (image->depth == 8) {
		int x;
		int y;
		int offset;

		offset = image->width;
		for (x = 0; x < image->width; x += PATTERN_MAX_X)
		{
			int w;

			w = image->width - x;
			if (w > PATTERN_MAX_X) {
				w = PATTERN_MAX_X;
			}
			for (y = 0; y < image->height; y += PATTERN_MAX_Y)
			{
				int h;

				h = image->height - y;
				if (h > PATTERN_MAX_Y) {
					h = PATTERN_MAX_Y;
				}
				ps2_paintsimg8(image->dx + x, image->dy + y, w, h,
					p->pseudo_palette, image->data + x + y * offset,
					offset);
			}
		}
	}
}

static int ps2fb_mmap(struct fb_info *info,
		    struct vm_area_struct *vma)
{
	unsigned long start = vma->vm_start;
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long page, pos;

	if (((unsigned long)info->fix.smem_start) == 0) {
		return -ENOMEM;
	}

	if (offset + size > info->fix.smem_len) {
		return -EINVAL;
	}

	pos = (unsigned long)info->fix.smem_start + offset;
	/* Framebuffer can't be mapped. Map normal memory instead
	 * and copy every 20ms the data from memory to the
	 * framebuffer. Currently there is no other way beside
	 * mapping to access the framebuffer from applications
	 * like xorg.
	 */

	while (size > 0) {
		page = vmalloc_to_pfn((void *)pos);
		if (remap_pfn_range(vma, start, page, PAGE_SIZE, PAGE_SHARED)) {
			return -EAGAIN;
		}
		start += PAGE_SIZE;
		pos += PAGE_SIZE;
		if (size > PAGE_SIZE)
			size -= PAGE_SIZE;
		else
			size = 0;
	}

	vma->vm_flags |= VM_RESERVED;	/* avoid to swap out this VMA */
	return 0;

}

static struct fb_ops ps2fb_ops = {
	.owner		= THIS_MODULE,
	.fb_open = ps2fb_open,
	.fb_release = ps2fb_release,
#if 0 /* TBD: Implement functions. */
	.fb_read = ps2fb_read,
	.fb_write = ps2fb_write,
	.fb_blank = ps2fb_blank,
	.fb_sync = ps2fb_sync,
	/** TBD: .fb_ioctl = ps2fb_ioctl, */
#endif
	.fb_check_var = ps2fb_check_var,
#if 0 /* TBD: Implement function. */
	.fb_set_par = ps2fb_set_par,
#endif
	.fb_setcolreg = ps2fb_setcolreg,
	.fb_fillrect = ps2fb_fillrect,
	.fb_copyarea = ps2fb_copyarea,
	.fb_imageblit = ps2fb_imageblit,
	.fb_mmap = ps2fb_mmap,
};

static void *rvmalloc(unsigned long size)
{
	void *mem;
	unsigned long adr;

	size = PAGE_ALIGN(size);
	mem = vmalloc_32(size);
	if (!mem)
		return NULL;

	memset(mem, 0, size); /* Clear the ram out, no junk to the user */
	adr = (unsigned long) mem;
	while (size > 0) {
		SetPageReserved(vmalloc_to_page((void *)adr));
		adr += PAGE_SIZE;
		size -= PAGE_SIZE;
	}

	return mem;
}

static int __devinit ps2fb_probe(struct platform_device *pdev)
{
    struct fb_info *info;
    struct ps2fb_par *par;
    struct device *device = &pdev->dev; /* or &pdev->dev */
    int cmap_len, retval;	
	char *mode_option;
   
	DPRINTK("ps2fb: %d %s()\n", __LINE__, __FUNCTION__);
    /*
     * Dynamically allocate info and par
     */
    info = framebuffer_alloc(sizeof(struct ps2fb_par), device);

    if (!info) {
	    /* TBD: goto error path */
		return -ENOMEM;
    }
    ps2con_initinfo(&defaultinfo);
    ps2gs_screeninfo(&defaultinfo, NULL);

	ps2con_gsp_init();

	/* Clear screen (black). */
	ps2_paintrect(0, 0, defaultinfo.w, defaultinfo.h, 0x80000000);

    par = info->par;
	info->pseudo_palette = par->pseudo_palette;
	par->opencnt = 0;

    /* 
     * Here we set the screen_base to the virtual memory address
     * for the framebuffer. Usually we obtain the resource address
     * from the bus layer and then translate it to virtual memory
     * space via ioremap. Consult ioport.h. 
     */
    info->screen_base = NULL; /* TBD: framebuffer_virtual_memory; */
    info->fbops = &ps2fb_ops;

	ps2fb_fix.smem_len = 32/8 * defaultinfo.w * defaultinfo.h; /* TBD: Increase memory. */
	ps2fb_fix.smem_start = (unsigned long) rvmalloc(ps2fb_fix.smem_len);
	ps2fb_fix.line_length = 32/8 * defaultinfo.w;
	printk("ps2fb: smem_start 0x%08lx\n", ps2fb_fix.smem_start);
    info->fix = ps2fb_fix;

    /*
     * Set up flags to indicate what sort of acceleration your
     * driver can provide (pan/wrap/copyarea/etc.) and whether it
     * is a module -- see FBINFO_* in include/linux/fb.h
     *
     * If your hardware can support any of the hardware accelerated functions
     * fbcon performance will improve if info->flags is set properly.
     *
     * FBINFO_HWACCEL_COPYAREA - hardware moves
     * FBINFO_HWACCEL_FILLRECT - hardware fills
     * FBINFO_HWACCEL_IMAGEBLIT - hardware mono->color expansion
     * FBINFO_HWACCEL_YPAN - hardware can pan display in y-axis
     * FBINFO_HWACCEL_YWRAP - hardware can wrap display in y-axis
     * FBINFO_HWACCEL_DISABLED - supports hardware accels, but disabled
     * FBINFO_READS_FAST - if set, prefer moves over mono->color expansion
     * FBINFO_MISC_TILEBLITTING - hardware can do tile blits
     *
     * NOTE: These are for fbcon use only.
     */
    info->flags = FBINFO_DEFAULT
		| FBINFO_HWACCEL_COPYAREA
		| FBINFO_HWACCEL_FILLRECT
		| FBINFO_HWACCEL_IMAGEBLIT;

#if 1 // TBD: Check
/********************* This stage is optional ******************************/
     /*
     * The struct pixmap is a scratch pad for the drawing functions. This
     * is where the monochrome bitmap is constructed by the higher layers
     * and then passed to the accelerator.  For drivers that uses
     * cfb_imageblit, you can skip this part.  For those that have a more
     * rigorous requirement, this stage is needed
     */

    /* PIXMAP_SIZE should be small enough to optimize drawing, but not
     * large enough that memory is wasted.  A safe size is
     * (max_xres * max_font_height/8). max_xres is driver dependent,
     * max_font_height is 32.
     */
    info->pixmap.addr = kmalloc(PIXMAP_SIZE, GFP_KERNEL);
    if (!info->pixmap.addr) {
	    /* TBD: goto error handler */
		return -ENOMEM;
    }

    info->pixmap.size = PIXMAP_SIZE;

    /*
     * FB_PIXMAP_SYSTEM - memory is in system ram
     * FB_PIXMAP_IO     - memory is iomapped
     * FB_PIXMAP_SYNC   - if set, will call fb_sync() per access to pixmap,
     *                    usually if FB_PIXMAP_IO is set.
     *
     * Currently, FB_PIXMAP_IO is unimplemented.
     */
	/* TBD: Set FB_PIXMAP_SYNC for DMA? */
    info->pixmap.flags = FB_PIXMAP_SYSTEM;

    /*
     * scan_align is the number of padding for each scanline.  It is in bytes.
     * Thus for accelerators that need padding to the next u32, put 4 here.
     */
    info->pixmap.scan_align = 1;

    /*
     * buf_align is the amount to be padded for the buffer. For example,
     * the i810fb needs a scan_align of 2 but expects it to be fed with
     * dwords, so a buf_align = 4 is required.
     */
    info->pixmap.buf_align = 16;

    /* access_align is how many bits can be accessed from the framebuffer
     * ie. some epson cards allow 16-bit access only.  Most drivers will
     * be safe with u32 here.
     *
     * NOTE: This field is currently unused.
     */
    info->pixmap.access_align = 8;
/***************************** End optional stage ***************************/
#endif

    /*
     * This should give a reasonable default video mode. The following is
     * done when we can set a video mode. 
     */
	mode_option = "640x480@60";	 	

    retval = fb_find_mode(&info->var, info, mode_option, NULL, 0, NULL, 8);
  
    if (!retval || retval == 4)
	return -EINVAL;			

    /* This has to be done! */
	cmap_len = 256; /* TBD: check. */
    if (fb_alloc_cmap(&info->cmap, cmap_len, 0))
	return -ENOMEM;

	if (register_framebuffer(info) < 0) {
		fb_dealloc_cmap(&info->cmap);
		return -EINVAL;
	}
	DPRINTK(KERN_INFO "fb%d: %s frame buffer device\n", info->node,
		info->fix.id);
	platform_set_drvdata(pdev, info);

    init_timer(&redraw_timer);

    return 0;
}

static int __devexit ps2fb_remove(struct platform_device *pdev)
{
	struct fb_info *info = platform_get_drvdata(pdev);

	DPRINTK("ps2fb: %d %s()\n", __LINE__, __FUNCTION__);
	if (info) {
		unregister_framebuffer(info);
		fb_dealloc_cmap(&info->cmap);
		/* ... */
		framebuffer_release(info);
	}
	return 0;
}

static struct platform_driver ps2fb_driver = {
	.probe = ps2fb_probe,
	.remove = __devexit_p(ps2fb_remove),
	.driver = {
		.name = "ps2fb",
	},
};

#ifndef MODULE
/*
 * Only necessary if your driver takes special options,
 * otherwise we fall back on the generic fb_setup().
 */
int __init ps2fb_setup(char *options)
{
	DPRINTK("ps2fb: %d %s()\n", __LINE__, __FUNCTION__);
    /* Parse user speficied options (`video=ps2fb:') */
	return 0;
}
#endif /* MODULE */

static int __init ps2fb_init(void)
{
	int ret;
	/*
	 *  For kernel boot options (in 'video=ps2fb:<options>' format)
	 */
#ifndef MODULE
	char *option = NULL;

	DPRINTK("ps2fb: %d %s()\n", __LINE__, __FUNCTION__);

	if (fb_get_options("ps2fb", &option))
		return -ENODEV;
	ps2fb_setup(option);
#else
	DPRINTK("ps2fb: %d %s()\n", __LINE__, __FUNCTION__);
#endif

	ret = platform_driver_register(&ps2fb_driver);
	DPRINTK("ps2fb: %d %s() ret = %d\n", __LINE__, __FUNCTION__, ret);

	return ret;
}

static void __exit ps2fb_exit(void)
{
	DPRINTK("ps2fb: %d %s()\n", __LINE__, __FUNCTION__);
	platform_driver_unregister(&ps2fb_driver);
}

/* ------------------------------------------------------------------------- */

module_init(ps2fb_init);
module_exit(ps2fb_exit);

MODULE_LICENSE("GPL");
