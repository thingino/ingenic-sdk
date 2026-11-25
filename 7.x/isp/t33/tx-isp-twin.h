/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __TX_ISP_TWIN_H__
#define __TX_ISP_TWIN_H__

/*
 * Layout of the 4.4-shaped twins tx-isp-twin.c hands the firmware's fops
 * handlers (its own objects, not the kernel's): a struct file at the
 * offsets the 4.4.94 build reads, and the seq_file of a single_open() proc
 * file.
 */
#define FW(base, off, type)	(*(type *)((u8 *)(base) + (off)))

#define FILE44_SIZE		160
#define FILE44_REAL		0	/* the kernel's object (not a 4.4 member) */
#define FILE44_F_INODE		16
#define FILE44_F_FLAGS		32
#define FILE44_F_MODE		36
#define FILE44_F_POS		56
#define FILE44_PRIVATE_DATA	128

#define SEQ44_SIZE		80
#define SEQ44_REAL		0
#define SEQ44_PRIVATE		72

#endif
