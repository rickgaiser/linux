/*
 * linux/include/asm-mips/ps2/speed.h
 *
 *	Copyright (C) 2001  Sony Computer Entertainment Inc.
 *	Copyright (C) 2010  Mega Man
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * $Id$
 */

#ifndef __ASM_PS2_SPEED_H
#define __ASM_PS2_SPEED_H

#define DEV9M_BASE		0x14000000

#define SPD_R_REV		(DEV9M_BASE + 0x00)
#define SPD_R_REV_1		(DEV9M_BASE + 0x00)
#define SPD_R_REV_3		(DEV9M_BASE + 0x04)

#define SPD_R_INTR_STAT		(DEV9M_BASE + 0x28)
#define SPD_R_INTR_ENA		(DEV9M_BASE + 0x2a)
#define SPD_R_XFR_CTRL		(DEV9M_BASE + 0x32)
#define SPD_R_IF_CTRL		(DEV9M_BASE + 0x64)

#endif /* __ASM_PS2_SPEED_H */

