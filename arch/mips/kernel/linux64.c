/*
 * Conversion between 64-bit and 32-bit native system calls.
 * Convert syscalls from MIPS ABI n32 to o32.
 *
 * Copyright (C) 2012 Mega Man
 */

#include <linux/syscalls.h>
#include <linux/file.h>

#include <asm/sim.h>
#include <asm/ptrace.h>

asmlinkage int sysn32_pread64(nabi_no_regargs struct pt_regs regs)
{
	return sys_pread64(MIPS_READ_REG(regs.regs[4]),
		(void *) MIPS_READ_REG_L(regs.regs[5]),
		MIPS_READ_REG(regs.regs[6]),
		MIPS_READ_REG(regs.regs[7]));
}

asmlinkage int sysn32_pwrite64(nabi_no_regargs struct pt_regs regs)
{
	return sys_pwrite64(MIPS_READ_REG(regs.regs[4]),
		(void *) MIPS_READ_REG_L(regs.regs[5]),
		MIPS_READ_REG(regs.regs[6]),
		MIPS_READ_REG(regs.regs[7]));
}

asmlinkage int sysn32_lookup_dcookie(nabi_no_regargs struct pt_regs regs)
{
	return sys_lookup_dcookie(MIPS_READ_REG(regs.regs[4]),
		(void *) MIPS_READ_REG_L(regs.regs[5]),
		MIPS_READ_REG(regs.regs[6]));
}

asmlinkage int sysn32_sync_file_range(nabi_no_regargs struct pt_regs regs)
{
	return sys_sync_file_range(MIPS_READ_REG(regs.regs[4]),
		MIPS_READ_REG(regs.regs[5]),
		MIPS_READ_REG(regs.regs[6]),
		MIPS_READ_REG(regs.regs[7]));
}

asmlinkage int sysn32_fallocate(nabi_no_regargs struct pt_regs regs)
{
	return sys_fallocate(MIPS_READ_REG(regs.regs[4]),
		MIPS_READ_REG(regs.regs[5]),
		MIPS_READ_REG(regs.regs[6]),
		MIPS_READ_REG(regs.regs[7]));
}

asmlinkage int sysn32_fadvise64_64(nabi_no_regargs struct pt_regs regs)
{
	return sys_fadvise64_64(MIPS_READ_REG(regs.regs[4]),
		MIPS_READ_REG(regs.regs[5]),
		MIPS_READ_REG(regs.regs[6]),
		MIPS_READ_REG(regs.regs[7]));
}

asmlinkage int sysn32_readahead(nabi_no_regargs struct pt_regs regs)
{
	return sys_readahead(MIPS_READ_REG(regs.regs[4]),
		MIPS_READ_REG(regs.regs[5]),
		MIPS_READ_REG(regs.regs[6]));
}

static long long n32_lseek(unsigned int fd, loff_t offset, unsigned int origin)
{
	loff_t retval;
	struct file * file;
	int fput_needed;

	retval = -EBADF;
	file = fget_light(fd, &fput_needed);
	if (!file)
		goto bad;

	retval = -EINVAL;
	if (origin <= SEEK_MAX) {
		loff_t res = vfs_llseek(file, offset, origin);
		retval = res;
	}
	fput_light(file, fput_needed);
bad:
	return retval;
}

/*
 * WARNING: v0 is misused for storing return address in syso64_lseek().
 * Don't expect a sensible value here.
 */
asmlinkage int sysn32_lseek(nabi_no_regargs struct pt_regs regs)
{
	return n32_lseek(MIPS_READ_REG(regs.regs[4]), MIPS_READ_REG(regs.regs[5]), MIPS_READ_REG(regs.regs[6]));
}

asmlinkage int sysn32_mips_mmap(nabi_no_regargs struct pt_regs regs)
{
	unsigned long result;

	result = -EINVAL;
	if (MIPS_READ_REG(regs.regs[9]) & ~PAGE_MASK)
		goto out;

	result = sys_mmap_pgoff(MIPS_READ_REG(regs.regs[4]), MIPS_READ_REG(regs.regs[5]), MIPS_READ_REG(regs.regs[6]), MIPS_READ_REG(regs.regs[7]), MIPS_READ_REG(regs.regs[8]), MIPS_READ_REG(regs.regs[9]) >> PAGE_SHIFT);

out:
	return result;
}
