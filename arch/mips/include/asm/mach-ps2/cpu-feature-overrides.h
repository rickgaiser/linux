/* Copyright 2010 Mega Man */
/* TBD: Unfinished state. Rework code. */
#ifndef __ASM_MACH_PS2_CPU_FEATURE_OVERRIDES_H
#define __ASM_MACH_PS2_CPU_FEATURE_OVERRIDES_H

#define cpu_has_llsc			0
#define cpu_has_4k_cache		1 /* TBD: Verify cache functions. */
#define cpu_has_divec			1 /* TBD: Check also other bits near definition of MIPS_CPU_DIVEC. */
#define cpu_has_4kex			1
#define cpu_has_counter			1
#define cpu_has_cache_cdex_p	0
#define cpu_has_cache_cdex_s	0
#define cpu_has_mcheck			0
#define cpu_has_nofpuex			1
#define cpu_has_mipsmt			0
#define cpu_has_vce				0
#define cpu_has_dsp				1
#define cpu_has_userlocal		0
#define cpu_has_64bit_addresses	0
#define cpu_has_64bit_gp_regs	0	/* TBD: Change to 1 if 64-bit support is enabled in exception handler. */
#define cpu_has_64bit_zero_reg	0	/* TBD: Change to 1 if 64-bit support is enabled in exception handler. */
#define cpu_vmbits				31
#define cpu_has_clo_clz			0
#define cpu_has_ejtag			0
/* TBD: Check define for cpu_has_dc_aliases */
#define cpu_has_ic_fills_f_dc	0

#endif
