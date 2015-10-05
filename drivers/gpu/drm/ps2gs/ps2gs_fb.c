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

#include <asm/mach-ps2/gsfunc.h>
#include <asm/mach-ps2/eedev.h>

#include <drm/drmP.h>
#include <drm/drm_crtc.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_crtc_helper.h>
#include <drm/drm_gem_cma_helper.h>
#include <drm/drm_fb_cma_helper.h>

#include "ps2gs_dma.h"
#include "ps2gs_drv.h"


#define BUF_SIZE	(4 * 1024)
static unsigned char buf[BUF_SIZE] __attribute__ ((aligned(16)));

#define SGL_MAX 256
struct ps2fb_priv {
	/* Scatter gather list */
	struct scatterlist *sgl;
	int sgl_used;
};
static struct ps2fb_priv ps2fb;

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

static void ps2fb_sgl_kick(void){
	//FUNC_DEBUG();

	if (ps2fb.sgl_used > 0) {
		ps2gs_dma_send_sg(ps2fb.sgl, ps2fb.sgl_used);
		ps2fb.sgl_used = 0;
	}
}

static void ps2fb_sgl_add_cont(const void *ptr, unsigned int len)
{
	//FUNC_DEBUG();
	//printk("ps2gs: %s(0x%p, %d)\n", __func__, ptr, len);

	sg_set_buf(&ps2fb.sgl[ps2fb.sgl_used], ptr, len);

	ps2fb.sgl_used++;
	if (ps2fb.sgl_used == SGL_MAX) {
		ps2fb_sgl_kick();
	}
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

/* QWC   field is 16 bits, so 2^16-1 quad words MAX */
#define PS2_MAX_DMA_SIZE ((64*1024-1)*16)
/* NLOOP field is 15 bits, so 2^15-1 quad words MAX */
#define PS2_MAX_GIF_SIZE ((32*1024-1)*16)
void ps2gs_fb_redraw(struct drm_framebuffer *fb)
{
	struct drm_fb_cma *fb_cma = to_fb_cma(fb);
	struct drm_gem_cma_object *obj = fb_cma->obj[0];
	u64 *gsp = (u64 *)buf;
	void *gsp_h;
	unsigned long stride;
	unsigned long y;
	unsigned long maxheight;
	unsigned long fbw, fbp, psm, bpp;
	void * fb_vaddr;

	//FUNC_DEBUG();
	//printk("ps2gs: %s: %dx%dx%dbpp (vaddr=0x%p, paddr=0x%p)\n", __func__, fb->width, fb->height, fb->bits_per_pixel, obj->vaddr, (void *)obj->paddr);

	/*
	 * Consistent DMA mappings are allocated in uncached memory area. But the
	 * DMA engine expects scatter gather lists from cached memory. So
	 * convert to cached memory.
	 */
	fb_vaddr = obj->vaddr;
	if ((unsigned long)fb_vaddr >= UNCAC_BASE)
		fb_vaddr = CAC_ADDR(fb_vaddr);

	bpp = fb->bits_per_pixel / 8;
	fbw = (fb->width + 63) / 64;
	fbp = 0; 			/* FIXME: this is the address in GS */
	psm = PS2_GS_PSMCT24;		/* FIXME */

	stride = bpp * fb->width;
	maxheight = PS2_MAX_GIF_SIZE / stride;

	gsp_h = gsp;

	*gsp++ = PS2_GIFTAG_SET_TOPHALF(4, 0, 0, 0, PS2_GIFTAG_FLG_PACKED, 1);
	/* A+D */
	*gsp++ = 0x0e;

	*gsp++ = (u64)0 | ((u64)fbp << 32) | ((u64)fbw << 48) | ((u64)psm << 56);
	*gsp++ = PS2_GS_BITBLTBUF;

	*gsp++ = PACK64(0, PACK32(0, 0));
	*gsp++ = PS2_GS_TRXPOS;

	*gsp++ = PACK64(fb->width, fb->height);
	*gsp++ = PS2_GS_TRXREG;

	/* host to local */
	*gsp++ = 0;
	*gsp++ = PS2_GS_TRXDIR;

	for (y = 0; y < fb->height; y += maxheight) {
		int h;
		int size;

		h = fb->height - y;
		if (h > maxheight) {
			h = maxheight;
		}

		size = stride * h;

		*gsp++ = PS2_GIFTAG_SET_TOPHALF(size / 16, 1, 0, 0, PS2_GIFTAG_FLG_IMAGE, 0);
		*gsp++ = 0;

		ps2fb_sgl_add_cont(gsp_h, ((unsigned long) gsp) - ((unsigned long) gsp_h));
		gsp_h = gsp;
#if 0
		ps2fb_sgl_add(data, ALIGN16(bpp * width * height));
#else
		ps2fb_sgl_add_cont(fb_vaddr + (y * stride), size);
#endif
	}
	ps2fb_sgl_kick();
}

int
ps2gs_fb_init(void)
{
	FUNC_DEBUG();

	ps2fb.sgl = vzalloc(SGL_MAX * sizeof(struct scatterlist));
	if (!ps2fb.sgl) {
		return -ENOMEM;
	}
	sg_init_table(ps2fb.sgl, SGL_MAX);
	ps2fb.sgl_used = 0;

	return 0;
}
