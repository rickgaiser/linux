/*
 * Conversion between 64-bit and 32-bit native system calls.
 * Convert syscalls from MIPS ABI o64 to o32.
 *
 * Copyright (C) 2012 Mega Man
 */

#include <asm/stackframe.h>

.text
.global syso64_lseek

syso64_lseek:
	sd	ra, PT_R2(sp)		/* misuse PT_R2 to save return address. */
	jal	sysn32_lseek

	ld	ra, PT_R2(sp)		/* Restore return address. */
	/* o32 returns 64 bit value in v0 and v1. Convert it to a 64
	 * bit value in register v0.
	 */
	pextlw	v0, v1, v0
	jr	ra
