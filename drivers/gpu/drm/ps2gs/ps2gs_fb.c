#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/scatterlist.h>

#include <linux/ps2/gs.h>

#include <asm/mach-ps2/ps2.h>
#include <asm/mach-ps2/gsfunc.h>
#include <asm/mach-ps2/eedev.h>
#include <asm/mach-ps2/dma.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_fb_cma_helper.h>

#include "ps2gs_dma.h"
#include "ps2gs_drv.h"


/** Alignment for width in Bytes. */
#define PS2_FBMEM_ALIGN 8

#define BUF_SIZE	(4 * 1024)
static unsigned char buf[BUF_SIZE] __attribute__ ((aligned(16)));

//#define USE_SGL

#ifdef USE_SGL
#define SGL_MAX 256
struct ps2fb_priv {
	/* Scatter gather list */
	struct scatterlist *sgl;
	int sgl_used;
};
static struct ps2fb_priv ps2fb;
#endif // USE_SGL

/*
 * Copied from drivers/gpu/drm/drm_fB_cma_helper.c
 */
struct drm_fb_cma {
	struct drm_framebuffer		fb;
	struct drm_gem_cma_object	*obj[4];
};

/*
 * Copied from drivers/gpu/drm/drm_fB_cma_helper.c
 */
static inline struct drm_fb_cma *to_fb_cma(struct drm_framebuffer *fb)
{
	return container_of(fb, struct drm_fb_cma, fb);
}

#ifdef USE_SGL
static void ps2fb_sgl_kick(void){
	FUNC_DEBUG();

	if (ps2fb.sgl_used > 0) {
		ps2gs_dma_send_sg(ps2fb.sgl, ps2fb.sgl_used);
		ps2fb.sgl_used = 0;
	}
}
#endif // USE_SGL

static void ps2fb_sgl_add_cont(const void *ptr, unsigned int len)
{
	//FUNC_DEBUG();
	//printk("ps2gs: %s(0x%p, %d)\n", __func__, ptr, len);

#ifdef USE_SGL
	sg_set_buf(&ps2fb.sgl[ps2fb.sgl_used], ptr, len);

	ps2fb.sgl_used++;
	if (ps2fb.sgl_used == SGL_MAX) {
		ps2fb_sgl_kick();
	}
#else
	ps2gs_dma_send(ptr, len);
#endif // USE_SGL
}

#if 0
static void ps2fb_sgl_add(const void *ptr, unsigned int len)
{
	unsigned int page;
	unsigned int start;
	unsigned int end;
	unsigned int s;
	unsigned int offset;
	void *cur_start;
	unsigned int cur_size;

	//FUNC_DEBUG();
	printk("ps2gs: %s(0x%p, %d)\n", __func__, ptr, len);

	start = (unsigned int) ptr;
	cur_start = 0;
	cur_size = 0;

	/* First align to page boundary */
	s = start & (PAGE_SIZE - 1);
	s = PAGE_SIZE - s;
	if (s < PAGE_SIZE) {
		if (s > len) {
			s = len;
		}
		offset = start & (PAGE_SIZE - 1);
		/* vmalloc_to_pfn is only working with redraw handler. */
		cur_start = phys_to_virt(PFN_PHYS(vmalloc_to_pfn((void *) start))) + offset;
		cur_size = ALIGN16(s);
		start += s;
		len -= s;
	}

	end = ALIGN16(start + len);

	for (page = start; page < end; page += PAGE_SIZE) {
		void *addr;
		unsigned int size;

		addr = phys_to_virt(PFN_PHYS(vmalloc_to_pfn((void *) page)));
		size = end - page;
		if (size > PAGE_SIZE) {
			size = PAGE_SIZE;
		}

		if (cur_size > 0) {
			if (addr == (cur_start + cur_size)) {
				/* contiguous physical memory. */
				cur_size += size;
			} else {
				ps2fb_sgl_add_cont(cur_start, ALIGN16(cur_size));

				cur_size = size;
				cur_start = addr;
			}
		} else {
			cur_start = addr;
			cur_size = size;
		}
	}

	if (cur_size > 0) {
		ps2fb_sgl_add_cont(cur_start, ALIGN16(cur_size));

		cur_size = 0;
		cur_start = 0;
	}
}
#endif

/**
 *		Copy data to framebuffer on GS side.
 *
 *		@param info PS2 screeninfo (16 bit and 32 bit PSM is supported)
 *		@param sx Destination X coordinate in framebuffer
 *		@param sy Destination Y coordinate in framebuffer
 *		@param width Width of frame
 *		@param height Height of frame
 *		@param data Source pointer which was allocated with rvmalloc()
 */
static u64 *ps2fb_copyframe(u64 *gsp, struct ps2_screeninfo *info, int sx, int sy, int width, int height, const void *data)
{
	void *gsp_h;
	int fbw = (info->w + 63) / 64;
	int bpp;

	//FUNC_DEBUG();
	//printk("ps2gs: %s(%d, %d, %d, %d, 0x%p)\n", __func__, sx, sy, width, height, data);

	/* Calculate number of bytes per pixel. */
	switch (info->psm) {
	case PS2_GS_PSMCT32:
	case PS2_GS_PSMZ32:
		bpp = 4;
		break;
	case PS2_GS_PSMCT24:
	case PS2_GS_PSMZ24:
		bpp = 3;
		break;
	case PS2_GS_PSMCT16:
	case PS2_GS_PSMCT16S:
	case PS2_GS_PSMZ16:
	case PS2_GS_PSMZ16S:
		bpp = 2;
		break;
	case PS2_GS_PSMT8:
	case PS2_GS_PSMT8H:
	default:
		bpp = 1;
		break;
	}
	gsp_h = gsp;

	*gsp++ = PS2_GIFTAG_SET_TOPHALF(4, 0, 0, 0, PS2_GIFTAG_FLG_PACKED, 1);
	/* A+D */
	*gsp++ = 0x0e;

	*gsp++ = (u64)0 | ((u64)info->fbp << 32) |
	    ((u64)fbw << 48) | ((u64)info->psm << 56);
	*gsp++ = PS2_GS_BITBLTBUF;

	*gsp++ = PACK64(0, PACK32(sx, sy));
	*gsp++ = PS2_GS_TRXPOS;

	*gsp++ = PACK64(width, height);
	*gsp++ = PS2_GS_TRXREG;

	/* host to local */
	*gsp++ = 0;
	*gsp++ = PS2_GS_TRXDIR;

	*gsp++ = PS2_GIFTAG_SET_TOPHALF(ALIGN16(bpp * width * height) / 16, 1, 0, 0, PS2_GIFTAG_FLG_IMAGE, 0);
	*gsp++ = 0;

	ps2fb_sgl_add_cont(gsp_h, ((unsigned long) gsp) - ((unsigned long) gsp_h));
#if 0
	ps2fb_sgl_add(data, ALIGN16(bpp * width * height));
#else
	ps2fb_sgl_add_cont(data, ALIGN16(bpp * width * height));
#endif

	return gsp;
}

void ps2gs_fb_redraw(struct drm_framebuffer *fb)
{
	struct drm_fb_cma *fb_cma = to_fb_cma(fb);
	struct drm_gem_cma_object *obj = fb_cma->obj[0];
	struct ps2_screeninfo ps2_info;
	u64 *gsp = (u64 *)buf;
	unsigned long offset;
	unsigned long y;
	unsigned long maxheight;

	/*
	 * Consistent DMA mappings are allocated in uncached memory area. But the
	 * DMA engine expects scatter gather lists from cached memory. So
	 * convert to cached memory.
	 */
	if ((unsigned long)obj->vaddr >= UNCAC_BASE)
		obj->vaddr = CAC_ADDR(obj->vaddr);

	//FUNC_DEBUG();
	//printk("ps2gs: %s: %dx%dx%dbpp (vaddr=0x%p, paddr=0x%p)\n", __func__, fb->width, fb->height, fb->bits_per_pixel, obj->vaddr, (void *)obj->paddr);

	switch (fb->width) {
		case 640:
			maxheight = 64;
			break;
		case 720:
			maxheight = 56;
			break;
		case 800:
			maxheight = 50;
			break;
		case 1024:
			maxheight = 40;
			break;
		case 1280:
			maxheight = 32;
			break;
		case 1920:
			maxheight = 20;
			break;
		default:
			maxheight = 20;
			break;
	}

	/* HACK the needed info in here */
	ps2_info.w   = fb->width;
	ps2_info.psm = PS2_GS_PSMCT24;
	ps2_info.fbp = 0;

	offset = ((fb->bits_per_pixel/8) * fb->width + PS2_FBMEM_ALIGN - 1) & ~(PS2_FBMEM_ALIGN - 1);
	for (y = 0; y < fb->height; y += maxheight) {
		int h;

		h = fb->height - y;
		if (h > maxheight) {
			h = maxheight;
		}
		gsp = ps2fb_copyframe(gsp, &ps2_info, 0, y, fb->width, h, obj->vaddr + (y * offset));
	}
#ifdef USE_SGL
	ps2fb_sgl_kick();
#endif // USE_SGL
}

int
ps2gs_fb_init(void)
{
	FUNC_DEBUG();

#ifdef USE_SGL
	ps2fb.sgl = vzalloc(SGL_MAX * sizeof(struct scatterlist));
	if (!ps2fb.sgl) {
		return -ENOMEM;
	}
	sg_init_table(ps2fb.sgl, SGL_MAX);
	ps2fb.sgl_used = 0;
#endif // USE_SGL

	return 0;
}
