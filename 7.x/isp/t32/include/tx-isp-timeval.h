/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __TX_ISP_TIMEVAL_H__
#define __TX_ISP_TIMEVAL_H__

/* the 5.15 build's struct timeval: two longs, gone from 7.x */
struct timeval {
	long tv_sec;
	long tv_usec;
};

#endif
