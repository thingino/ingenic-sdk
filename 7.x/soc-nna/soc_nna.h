/* SPDX-License-Identifier: GPL-2.0 */
/*
 * /dev/soc-nna ABI as the Ingenic NNA libraries (libdrivers, libvenus) use it,
 * from the vendor SDK's drivers/soc-nna/soc_nna.h (T40 1.3.1, T41 1.2.6,
 * A1 1.7.0). MIPS32 only: the pointers in it are 32-bit.
 */
#ifndef SOC_NNA_H
#define SOC_NNA_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define SOC_NNA_MAGIC			'c'
#define IOCTL_SOC_NNA_MALLOC		_IOWR(SOC_NNA_MAGIC, 0, int)
#define IOCTL_SOC_NNA_FREE		_IOWR(SOC_NNA_MAGIC, 1, int)
#define IOCTL_SOC_NNA_FLUSHCACHE	_IOWR(SOC_NNA_MAGIC, 2, int)
#define IOCTL_SOC_NNA_SETUP_DES		_IOWR(SOC_NNA_MAGIC, 3, int)
#define IOCTL_SOC_NNA_RDCH_START	_IOWR(SOC_NNA_MAGIC, 4, int)
#define IOCTL_SOC_NNA_WRCH_START	_IOWR(SOC_NNA_MAGIC, 5, int)
#define IOCTL_SOC_NNA_VERSION		_IOWR(SOC_NNA_MAGIC, 6, int)

/* dir: 1 to the device, 2 from the device, anything else both (libdrivers: 3 = none) */
struct soc_nna_flush_cache_info {
	__u32 addr;
	__u32 len;
	__u32 dir;
};

/* vaddr is the kernel's address of the buffer: user space only echoes it */
struct soc_nna_buf {
	__u32 vaddr;
	__u32 paddr;
	__s32 size;
};

/* the caller's view of nmem is ignored here: the kernel's nmem= region wins */
struct soc_nna_version {
	__u32 version;
	__u32 nmem_extension;
	__u32 nmem_paddr;
	__u32 nmem_size;
};

#endif
