/*
 *  PlayStation 2 IOP Debugging using printk
 *
 *  Copyright (C) 2015-2015 Rick Gaiser
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/init.h>
#include <linux/module.h>

#include <asm/mach-ps2/sifdefs.h>
#include <asm/mach-ps2/sbios.h>


typedef struct {
       struct t_SifCmdHeader sifcmd;
       char text[80];
} iop_text_data_t;


static void iopdebug_printk(iop_text_data_t *data, void *arg)
{
	printk("IOP: %s", data->text);
}

int __init iopdebug_init(void)
{
	struct sb_sifaddcmdhandler_arg addcmdhandlerparam;

	/* Add SIF command handler for incoming debug messages */
	addcmdhandlerparam.fid = 0x10;
	addcmdhandlerparam.func = iopdebug_printk;
	addcmdhandlerparam.data = NULL;
	if (sbios(SB_SIFADDCMDHANDLER, &addcmdhandlerparam) < 0) {
		printk("Failed to initialize IOP debug handler.\n");
		return -1;
	}

	return 0;
}
arch_initcall(iopdebug_init);
