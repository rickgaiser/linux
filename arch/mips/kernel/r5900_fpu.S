/* Copyright 2010-2011 Mega Man */
/* TBD: Unfinished state. Rework code. */
/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 98, 99, 2000, 01 Ralf Baechle
 *
 * Multi-arch abstraction and asm macros for easier reading:
 * Copyright (C) 1996 David S. Miller (dm@engr.sgi.com)
 *
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2000 MIPS Technologies, Inc.
 * Copyright (C) 1999, 2001 Silicon Graphics, Inc.
 */
#include <asm/asm.h>
#include <asm/errno.h>
#include <asm/fpregdef.h>
#include <asm/mipsregs.h>
#include <asm/asm-offsets.h>
#include <asm/regdef.h>

	.macro	EX insn, reg, src
	.set	push
	.set	nomacro
.ex\@:	\insn	\reg, \src
	.set	pop
	.section __ex_table,"a"
	PTR	.ex\@, fault
	.previous
	.endm

	.set	noreorder
	.set	mips3

LEAF(_save_fp_context)
	cfc1	t1, fcr31

// XXX: Check FPU handler

	/* Store the 32 32-bit registers */
	EX	swc1 $f0, SC_FPREGS+0(a0)
	EX	swc1 $f1, SC_FPREGS+4(a0)
	EX	swc1 $f2, SC_FPREGS+16(a0)
	EX	swc1 $f3, SC_FPREGS+20(a0)
	EX	swc1 $f4, SC_FPREGS+32(a0)
	EX	swc1 $f5, SC_FPREGS+36(a0)
	EX	swc1 $f6, SC_FPREGS+48(a0)
	EX	swc1 $f7, SC_FPREGS+52(a0)
	EX	swc1 $f8, SC_FPREGS+64(a0)
	EX	swc1 $f9, SC_FPREGS+68(a0)
	EX	swc1 $f10, SC_FPREGS+80(a0)
	EX	swc1 $f11, SC_FPREGS+84(a0)
	EX	swc1 $f12, SC_FPREGS+96(a0)
	EX	swc1 $f13, SC_FPREGS+100(a0)
	EX	swc1 $f14, SC_FPREGS+112(a0)
	EX	swc1 $f15, SC_FPREGS+116(a0)
	EX	swc1 $f16, SC_FPREGS+128(a0)
	EX	swc1 $f17, SC_FPREGS+132(a0)
	EX	swc1 $f18, SC_FPREGS+144(a0)
	EX	swc1 $f19, SC_FPREGS+148(a0)
	EX	swc1 $f20, SC_FPREGS+160(a0)
	EX	swc1 $f21, SC_FPREGS+164(a0)
	EX	swc1 $f22, SC_FPREGS+176(a0)
	EX	swc1 $f23, SC_FPREGS+180(a0)
	EX	swc1 $f24, SC_FPREGS+192(a0)
	EX	swc1 $f25, SC_FPREGS+196(a0)
	EX	swc1 $f26, SC_FPREGS+208(a0)
	EX	swc1 $f27, SC_FPREGS+212(a0)
	EX	swc1 $f28, SC_FPREGS+224(a0)
	EX	swc1 $f29, SC_FPREGS+228(a0)
	EX	swc1 $f30, SC_FPREGS+240(a0)
	EX	swc1 $f31, SC_FPREGS+244(a0)
	EX	sw t1, SC_FPC_CSR(a0)
	jr	ra
	 li	v0, 0					# success
	END(_save_fp_context)

#ifdef CONFIG_MIPS32_COMPAT
	/* Save 32-bit process floating point context */
LEAF(_save_fp_context32)
	cfc1	t1, fcr31

	EX	swc1 $f0, SC32_FPREGS+0(a0)
	EX	swc1 $f1, SC32_FPREGS+4(a0)
	EX	swc1 $f2, SC32_FPREGS+16(a0)
	EX	swc1 $f3, SC32_FPREGS+20(a0)
	EX	swc1 $f4, SC32_FPREGS+32(a0)
	EX	swc1 $f5, SC32_FPREGS+36(a0)
	EX	swc1 $f6, SC32_FPREGS+48(a0)
	EX	swc1 $f7, SC32_FPREGS+52(a0)
	EX	swc1 $f8, SC32_FPREGS+64(a0)
	EX	swc1 $f9, SC32_FPREGS+68(a0)
	EX	swc1 $f10, SC32_FPREGS+80(a0)
	EX	swc1 $f11, SC32_FPREGS+84(a0)
	EX	swc1 $f12, SC32_FPREGS+96(a0)
	EX	swc1 $f13, SC32_FPREGS+100(a0)
	EX	swc1 $f14, SC32_FPREGS+112(a0)
	EX	swc1 $f15, SC32_FPREGS+116(a0)
	EX	swc1 $f16, SC32_FPREGS+128(a0)
	EX	swc1 $f17, SC32_FPREGS+132(a0)
	EX	swc1 $f18, SC32_FPREGS+144(a0)
	EX	swc1 $f19, SC32_FPREGS+148(a0)
	EX	swc1 $f20, SC32_FPREGS+160(a0)
	EX	swc1 $f21, SC32_FPREGS+164(a0)
	EX	swc1 $f22, SC32_FPREGS+176(a0)
	EX	swc1 $f23, SC32_FPREGS+180(a0)
	EX	swc1 $f24, SC32_FPREGS+192(a0)
	EX	swc1 $f25, SC32_FPREGS+196(a0)
	EX	swc1 $f26, SC32_FPREGS+208(a0)
	EX	swc1 $f27, SC32_FPREGS+212(a0)
	EX	swc1 $f28, SC32_FPREGS+224(a0)
	EX	swc1 $f29, SC32_FPREGS+228(a0)
	EX	swc1 $f30, SC32_FPREGS+240(a0)
	EX	swc1 $f31, SC32_FPREGS+244(a0)
	EX	sw t1, SC32_FPC_CSR(a0)
	cfc1	t0, $0				# implementation/version
	EX	sw t0, SC32_FPC_EIR(a0)

	jr	ra
	 li	v0, 0					# success
	END(_save_fp_context32)
#endif

/*
 * Restore FPU state:
 *  - fp gp registers
 *  - cp1 status/control register
 */
LEAF(_restore_fp_context)
	EX	lw t0, SC_FPC_CSR(a0)
	EX	lwc1 $f0, SC_FPREGS+0(a0)
	EX	lwc1 $f1, SC_FPREGS+4(a0)
	EX	lwc1 $f2, SC_FPREGS+16(a0)
	EX	lwc1 $f3, SC_FPREGS+20(a0)
	EX	lwc1 $f4, SC_FPREGS+32(a0)
	EX	lwc1 $f5, SC_FPREGS+36(a0)
	EX	lwc1 $f6, SC_FPREGS+48(a0)
	EX	lwc1 $f7, SC_FPREGS+52(a0)
	EX	lwc1 $f8, SC_FPREGS+64(a0)
	EX	lwc1 $f9, SC_FPREGS+68(a0)
	EX	lwc1 $f10, SC_FPREGS+80(a0)
	EX	lwc1 $f11, SC_FPREGS+84(a0)
	EX	lwc1 $f12, SC_FPREGS+96(a0)
	EX	lwc1 $f13, SC_FPREGS+100(a0)
	EX	lwc1 $f14, SC_FPREGS+112(a0)
	EX	lwc1 $f15, SC_FPREGS+116(a0)
	EX	lwc1 $f16, SC_FPREGS+128(a0)
	EX	lwc1 $f17, SC_FPREGS+132(a0)
	EX	lwc1 $f18, SC_FPREGS+144(a0)
	EX	lwc1 $f19, SC_FPREGS+148(a0)
	EX	lwc1 $f20, SC_FPREGS+160(a0)
	EX	lwc1 $f21, SC_FPREGS+164(a0)
	EX	lwc1 $f22, SC_FPREGS+176(a0)
	EX	lwc1 $f23, SC_FPREGS+180(a0)
	EX	lwc1 $f24, SC_FPREGS+192(a0)
	EX	lwc1 $f25, SC_FPREGS+196(a0)
	EX	lwc1 $f26, SC_FPREGS+208(a0)
	EX	lwc1 $f27, SC_FPREGS+212(a0)
	EX	lwc1 $f28, SC_FPREGS+224(a0)
	EX	lwc1 $f29, SC_FPREGS+228(a0)
	EX	lwc1 $f30, SC_FPREGS+240(a0)
	EX	lwc1 $f31, SC_FPREGS+244(a0)
	ctc1	t0, fcr31
	jr	ra
	 li	v0, 0					# success
	END(_restore_fp_context)

#ifdef CONFIG_MIPS32_COMPAT
LEAF(_restore_fp_context32)
	/* Restore an o32 sigcontext.  */
	EX	lw t0, SC32_FPC_CSR(a0)
	EX	lwc1 $f0, SC32_FPREGS+0(a0)
	EX	lwc1 $f1, SC32_FPREGS+4(a0)
	EX	lwc1 $f2, SC32_FPREGS+16(a0)
	EX	lwc1 $f3, SC32_FPREGS+20(a0)
	EX	lwc1 $f4, SC32_FPREGS+32(a0)
	EX	lwc1 $f5, SC32_FPREGS+36(a0)
	EX	lwc1 $f6, SC32_FPREGS+48(a0)
	EX	lwc1 $f7, SC32_FPREGS+52(a0)
	EX	lwc1 $f8, SC32_FPREGS+64(a0)
	EX	lwc1 $f9, SC32_FPREGS+68(a0)
	EX	lwc1 $f10, SC32_FPREGS+80(a0)
	EX	lwc1 $f11, SC32_FPREGS+84(a0)
	EX	lwc1 $f12, SC32_FPREGS+96(a0)
	EX	lwc1 $f13, SC32_FPREGS+100(a0)
	EX	lwc1 $f14, SC32_FPREGS+112(a0)
	EX	lwc1 $f15, SC32_FPREGS+116(a0)
	EX	lwc1 $f16, SC32_FPREGS+128(a0)
	EX	lwc1 $f17, SC32_FPREGS+132(a0)
	EX	lwc1 $f18, SC32_FPREGS+144(a0)
	EX	lwc1 $f19, SC32_FPREGS+148(a0)
	EX	lwc1 $f20, SC32_FPREGS+160(a0)
	EX	lwc1 $f21, SC32_FPREGS+164(a0)
	EX	lwc1 $f22, SC32_FPREGS+176(a0)
	EX	lwc1 $f23, SC32_FPREGS+180(a0)
	EX	lwc1 $f24, SC32_FPREGS+192(a0)
	EX	lwc1 $f25, SC32_FPREGS+196(a0)
	EX	lwc1 $f26, SC32_FPREGS+208(a0)
	EX	lwc1 $f27, SC32_FPREGS+212(a0)
	EX	lwc1 $f28, SC32_FPREGS+224(a0)
	EX	lwc1 $f29, SC32_FPREGS+228(a0)
	EX	lwc1 $f30, SC32_FPREGS+240(a0)
	EX	lwc1 $f31, SC32_FPREGS+244(a0)
	ctc1	t0, fcr31
	jr	ra
	 li	v0, 0					# success
	END(_restore_fp_context32)
#endif

	.set	reorder

	.type	fault@function
	.ent	fault
fault:	li	v0, -EFAULT				# failure
	jr	ra
	.end	fault
