/*
 * Conversion between 64-bit and 32-bit native system calls.
 * Convert syscalls from MIPS ABI n32 to o32.
 *
 * Copyright (C) 2012 Mega Man
 */

#include <linux/syscalls.h>

#include <asm/sim.h>
#include <asm/ptrace.h>

asmlinkage int sysn32_pread64(nabi_no_regargs struct pt_regs regs)
{
	return sys_pread64(MIPS_READ_REG_L(regs.regs[4]),
		(void *) MIPS_READ_REG_L(regs.regs[5]),
		MIPS_READ_REG_L(regs.regs[6]),
		MIPS_READ_REG_L(regs.regs[7]));
}

asmlinkage int sysn32_pwrite64(nabi_no_regargs struct pt_regs regs)
{
	return sys_pwrite64(MIPS_READ_REG_L(regs.regs[4]),
		(void *) MIPS_READ_REG_L(regs.regs[5]),
		MIPS_READ_REG_L(regs.regs[6]),
		MIPS_READ_REG_L(regs.regs[7]));
}

asmlinkage int sysn32_lookup_dcookie(nabi_no_regargs struct pt_regs regs)
{
	return sys_lookup_dcookie(MIPS_READ_REG_L(regs.regs[4]),
		(void *) MIPS_READ_REG_L(regs.regs[5]),
		MIPS_READ_REG_L(regs.regs[6]));
}

asmlinkage int sysn32_sync_file_range(nabi_no_regargs struct pt_regs regs)
{
	return sys_sync_file_range(MIPS_READ_REG_L(regs.regs[4]),
		MIPS_READ_REG_L(regs.regs[5]),
		MIPS_READ_REG_L(regs.regs[6]),
		MIPS_READ_REG_L(regs.regs[7]));
}

asmlinkage int sysn32_fallocate(nabi_no_regargs struct pt_regs regs)
{
	return sys_fallocate(MIPS_READ_REG_L(regs.regs[4]),
		MIPS_READ_REG_L(regs.regs[5]),
		MIPS_READ_REG_L(regs.regs[6]),
		MIPS_READ_REG_L(regs.regs[7]));
}

asmlinkage int sysn32_fadvise64_64(nabi_no_regargs struct pt_regs regs)
{
	return sys_fadvise64_64(MIPS_READ_REG_L(regs.regs[4]),
		MIPS_READ_REG_L(regs.regs[5]),
		MIPS_READ_REG_L(regs.regs[6]),
		MIPS_READ_REG_L(regs.regs[7]));
}

asmlinkage int sysn32_readahead(nabi_no_regargs struct pt_regs regs)
{
	return sys_readahead(MIPS_READ_REG_L(regs.regs[4]),
		MIPS_READ_REG_L(regs.regs[5]),
		MIPS_READ_REG_L(regs.regs[6]));
}
