/*
 * linux/include/asm-mips/ps2/sysconf.h
 *
 *	Copyright (C) 2000, 2001  Sony Computer Entertainment Inc.
 *	Copyright (C) 2010  Mega Man
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License Version 2. See the file "COPYING" in the main
 * directory of this archive for more details.
 *
 * $Id$
 */

#ifndef __ASM_PS2_SYSCONF_H
#define __ASM_PS2_SYSCONF_H

struct ps2_sysconf {
    short timezone;
    u_char aspect;
    u_char datenotation;
    u_char language;
    u_char spdif;
    u_char summertime;
    u_char timenotation;
    u_char video;
};

#define PS2SYSCONF_GETLINUXCONF		_IOR ('s', 0, struct ps2_sysconf)
#define PS2SYSCONF_SETLINUXCONF		_IOW ('s', 1, struct ps2_sysconf)

#ifdef __KERNEL__
extern struct ps2_sysconf *ps2_sysconf;
#endif

#endif /* __ASM_PS2_SYSCONF_H */
