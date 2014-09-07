/*
 *  Playstation 2 IOP loadfile
 *
 *  Copyright (C) 2014 Juergen Urban
 */
#include <linux/init.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/semaphore.h>
#include <linux/sched.h>

#include <asm/cacheflush.h>

#include <asm/mach-ps2/ps2.h>
#include <asm/mach-ps2/sifdefs.h>
#include <asm/mach-ps2/sbios.h>
#include <asm/mach-ps2/dma.h>

#include "loadfile.h"

#define LOADFILE_SID 0x80000006
#define LF_PATH_MAX 252
#define LF_ARG_MAX 252
// TBD: #define USE_DMA 1

enum _lf_functions {
	LF_F_MOD_LOAD = 0,
	LF_F_ELF_LOAD,

	LF_F_SET_ADDR,
	LF_F_GET_ADDR,

	LF_F_MG_MOD_LOAD,
	LF_F_MG_ELF_LOAD,

	LF_F_MOD_BUF_LOAD,

	LF_F_MOD_STOP,
	LF_F_MOD_UNLOAD,

	LF_F_SEARCH_MOD_BY_NAME,
	LF_F_SEARCH_MOD_BY_ADDRESS,
};

struct _lf_module_load_arg {                                                    
	union {                                                                 
		int32_t arg_len;                                                
		int32_t result;                                                 
	} p;                                                                    
	int32_t     modres;                                                         
	char path[LF_PATH_MAX];                                              
	char args[LF_ARG_MAX];                                               
};

struct _lf_module_buffer_load_arg {                                             
	union {                                                                 
		uint32_t ptr;                                                   
		int32_t result;                                                 
	} p;                                                                    
	union {                                                                 
		int32_t arg_len;                                                
		int32_t modres;                                                 
	} q;                                                                    
	char unused[LF_PATH_MAX];                                            
	char args[LF_ARG_MAX];                                               
};

static ps2sif_clientdata_t cd_loadfile_rpc;
static int rpc_initialized;

DECLARE_MUTEX(loadfile_rpc_sema);

static void loadfile_rpcend_notify(void *arg)
{
	complete((struct completion *)arg);
	return;
}

static int __init loadfile_bind(void)
{
	int loop;
	struct completion compl;
	int rv;
	volatile int j;

	init_completion(&compl);

	/* bind poweroff.irx module */
	for (loop = 100; loop; loop--) {
		rv = ps2sif_bindrpc(&cd_loadfile_rpc, LOADFILE_SID,
			SIF_RPCM_NOWAIT, loadfile_rpcend_notify, (void *)&compl);
		if (rv < 0) {
			printk("loadfile: bind rv = %d.\n", rv);
			break;
		}
		wait_for_completion(&compl);
		if (cd_loadfile_rpc.serve != 0)
			break;
		j = 0x010000;
		while (j--) ;
	}
	if (cd_loadfile_rpc.serve == 0) {
		printk("loadfile bind error 1, loading of IRX modules will not work.\n");
		return -1;
	}
	rpc_initialized = -1;
	return 0;
}

static int load_module(const char *path, int path_len, const char *args, int arg_len, int *modres, int fno)
{
	struct _lf_module_load_arg *arg;
	unsigned char buffer[sizeof(*arg) + DMA_TRUNIT];
	struct completion compl;
	int rv;

	if (arg_len > LF_ARG_MAX) {
		return -EINVAL;
	}
	if (path_len > (LF_PATH_MAX - 1)) {
		return -EINVAL;
	}

	init_completion(&compl);
	arg = (void *) buffer;
	arg = DMA_ALIGN(arg);
	memset(arg, 0, sizeof(arg));
	strncpy(arg->path, path, path_len);
	arg->path[path_len] = 0;
	if ((args != NULL) && (arg_len > 0)) {
		arg->p.arg_len = arg_len;
		memcpy(arg->args, args, arg->p.arg_len);
	} else {
		arg->p.arg_len = 0;
	}

	down(&loadfile_rpc_sema);

	if (!rpc_initialized) {
		up(&loadfile_rpc_sema);
		return -ENODEV;
	}
	do {
		rv = ps2sif_callrpc(&cd_loadfile_rpc, fno,
			SIF_RPCM_NOWAIT,
			arg, sizeof(*arg),
			arg, 8,
			loadfile_rpcend_notify,
			&compl);
	} while (rv == -E_SIF_PKT_ALLOC);
	if (rv == 0) {
		wait_for_completion(&compl);
	}
	up(&loadfile_rpc_sema);

	if (rv == 0) {
		rv = arg->p.result;
		if (modres != NULL) {
			*modres = arg->modres;
		}
	}
	if (rv < 0) {
		switch (rv) {
			case -SIF_RPCE_GETP:
			case -E_SIF_PKT_ALLOC:
			case -E_IOP_NO_MEMORY:
				rv = -ENOMEM;
				break;
			case -SIF_RPCE_SENDP:
				rv = -EIO;
				break;
			case -E_LF_FILE_NOT_FOUND:
				rv = -ENOENT;
				break;
			case -E_LF_FILE_IO_ERROR:
				rv = -EIO;
				break;
			case -E_LF_NOT_IRX:
				rv = -ENOEXEC;
				break;
			default:
				rv = -EIO;
				break;
		}
	}
	return rv;
}

int ps2_load_module(const char *path, int path_len, const char *args, int arg_len, int *mod_res)
{
	return load_module(path, path_len, args, arg_len, mod_res, LF_F_MOD_LOAD);
}

static int load_module_buffer(dma_addr_t ptr, const char *args, int arg_len, int *modres)
{
	struct _lf_module_buffer_load_arg *arg;
	unsigned char buffer[sizeof(*arg) + DMA_TRUNIT];
	struct completion compl;
	int rv;

	if (arg_len > LF_ARG_MAX) {
		return -EINVAL;
	}

	init_completion(&compl);
	arg = (void *) buffer;
	arg = DMA_ALIGN(arg);
	memset(arg, 0, sizeof(arg));
	arg->p.ptr = ptr;
	if ((args != NULL) && (arg_len > 0)) {
		arg->q.arg_len = arg_len;
		memcpy(arg->args, args, arg->q.arg_len);
	} else {
		arg->q.arg_len = 0;
	}

	down(&loadfile_rpc_sema);

	if (!rpc_initialized) {
		up(&loadfile_rpc_sema);
		return -ENODEV;
	}
	do {
		rv = ps2sif_callrpc(&cd_loadfile_rpc,
			LF_F_MOD_BUF_LOAD,
			SIF_RPCM_NOWAIT,
			arg, sizeof(*arg),
			arg, 8,
			loadfile_rpcend_notify,
			&compl);
	} while (rv == -E_SIF_PKT_ALLOC);
	if (rv == 0) {
		wait_for_completion(&compl);
	}
	up(&loadfile_rpc_sema);

	if (rv == 0) {
		rv = arg->p.result;
		if (modres != NULL) {
			*modres = arg->q.modres;
		}
	}
	if (rv < 0) {
		switch (rv) {
			case -SIF_RPCE_GETP:
			case -E_SIF_PKT_ALLOC:
			case -E_IOP_NO_MEMORY:
				rv = -ENOMEM;
				break;
			case -SIF_RPCE_SENDP:
				rv = -EIO;
				break;
			case -E_LF_FILE_NOT_FOUND:
				rv = -ENOENT;
				break;
			case -E_LF_FILE_IO_ERROR:
				rv = -EIO;
				break;
			case -E_LF_NOT_IRX:
				rv = -ENOEXEC;
				break;
			default:
				rv = -EIO;
				break;
		}
	}
	return rv;
}

static unsigned int iop_cmp(dma_addr_t addr, const unsigned char *buf, unsigned int size)
{
	volatile unsigned char *p;
	int i;

	p = phys_to_virt(ps2sif_bustophys(addr));
	for (i = 0; i < size; i++) {
		if (buf[i] != p[i]) {
			printk(KERN_ERR "DMA copy failed at offset 0x%08x 0x%02x 0x%02x\n", i, buf[i], p[i]);
		}
	}

	return size;
}

#ifdef USE_DMA
/** Write iop memory address. */
static unsigned int iop_write(dma_addr_t addr, const void *buf, unsigned int size)
{
	ps2sif_dmadata_t sdd;
	unsigned int qid;
	int rv = size;

	size = (size + DMA_TRUNIT - 1) & ~(DMA_TRUNIT - 1);

	ps2sif_writebackdcache(buf, size);

	/* Copy module to IOP memory using DMA. */
	sdd.data = (uint32_t) buf;
	sdd.addr = addr;
	sdd.size = size;
	sdd.mode = 0;

	qid = ps2sif_setdma_wait_interruptible(&sdd, 1);
	if (qid != 0) {
		if (ps2sif_dmastat_wait_interruptible(qid) >= 0) {
			rv = -ERESTARTSYS;
		}
	} else {
		rv = -ERESTART;
	}
	return rv;
}
#else
/** Write iop memory address. */
static unsigned int iop_write(dma_addr_t addr, const uint8_t *buf, unsigned int size)
{
	volatile uint8_t *p;
	int i;

	p = phys_to_virt(ps2sif_bustophys(addr));
	for (i = 0; i < size; i++) {
		p[i] = buf[i];
		__asm__ __volatile__("sync.l");
	}

	return size;
}
#endif

int ps2_load_module_buffer(const void *ptr, int size, const char *args, int arg_len, int *mod_res)
{
	dma_addr_t iop_addr;
	int rv = -ENOMEM;
	
	iop_addr = ps2sif_allociopheap(size);
	if (iop_addr != 0) {
		rv = iop_write(iop_addr, ptr, size);
		if (rv >= 0) {
			/* TBD: Fix DMA transfer so that the following will not
			 * fail.
			 */
			iop_cmp(iop_addr, ptr, size);

			/* Load module. */
			rv = load_module_buffer(iop_addr, args, arg_len, mod_res);
		}

		/* Clean up */
		ps2sif_freeiopheap(iop_addr);
	}
	return rv;
}

int __init ps2_loadfile_init(void)
{
	int rv;

	down(&loadfile_rpc_sema);
	if (rpc_initialized) {
		rv = 0;
	} else {
		rv = loadfile_bind();
	}
	up(&loadfile_rpc_sema);

	return rv;
}
