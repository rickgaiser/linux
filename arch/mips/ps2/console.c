/*
 *  linux/drivers/video/ps2con.c -- A PS2 console driver
 *
 *  To be used if there's no other console driver (e.g. for plain VGA text)
 *  available, usually until fbcon takes console over.
 *
 * Copyright 2010 Mega Man
 */

#include <linux/types.h>
#include <linux/kdev_t.h>
#include <linux/console.h>
#include <linux/vt_kern.h>
#include <linux/screen_info.h>
#include <linux/init.h>
#include <linux/module.h>

#include <asm/mach-ps2/ps2.h>

/*
 * Sony Playstation console driver
 * TBD: prom_putchar() is used. No graphic support.
 */

#define PS2CON_COLUMNS	80
#define PS2CON_ROWS	25

static const char *ps2con_startup(void)
{
	return "PS2 console device";
}

static void ps2con_init(struct vc_data *vc, int init)
{
    vc->vc_can_do_color = 1;
	if (init) {
		vc->vc_cols = PS2CON_COLUMNS;
		vc->vc_rows = PS2CON_ROWS;
	} else
	vc_resize(vc, PS2CON_COLUMNS, PS2CON_ROWS);
}

static int ps2con_dummy(void)
{
	return 0;
}

#define DUMMY	(void *)ps2con_dummy

void ps2_putc(struct vc_data *vc, int c, int ypos, int xpos)
{
	prom_putchar(c);
}

void ps2_putcs(struct vc_data *vc, const unsigned short *s, int length, int ypos, int xpos)
{
	int i;

	for (i = 0; i < length; i++) {
		prom_putchar(*s);
		s++;
	}
	prom_putchar('\n');
}

/*
 *  The console `switch' structure for the PS2 console
 *
 *  Most of the operations are dummies.
 */

const struct consw ps2_con = {
	.owner = THIS_MODULE,
	.con_startup = ps2con_startup,
	.con_init = ps2con_init,
	.con_deinit = DUMMY,
	.con_clear = DUMMY,
	.con_putc = ps2_putc,
	.con_putcs = ps2_putcs,
	.con_cursor = DUMMY,
	.con_scroll = DUMMY,
	.con_bmove = DUMMY,
	.con_switch = DUMMY,
	.con_blank = DUMMY,
	.con_font_set = DUMMY,
	.con_font_get = DUMMY,
	.con_font_default = DUMMY,
	.con_font_copy = DUMMY,
	.con_set_palette = DUMMY,
	.con_scrolldelta = DUMMY,
};
