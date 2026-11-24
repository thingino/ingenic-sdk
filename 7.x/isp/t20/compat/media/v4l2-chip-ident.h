/* SPDX-License-Identifier: GPL-2.0 */
#ifndef __COMPAT_V4L2_CHIP_IDENT_H__
#define __COMPAT_V4L2_CHIP_IDENT_H__

/* removed from mainline; the vendor sensor drivers still call these */
#include <linux/i2c.h>
#include <linux/videodev2.h>

struct v4l2_dbg_chip_ident {
	struct v4l2_dbg_match match;
	__u32 ident;
	__u32 revision;
};

static inline int v4l2_chip_match_i2c_client(struct i2c_client *c,
					     const struct v4l2_dbg_match *match)
{
	if (match->type == V4L2_CHIP_MATCH_I2C_ADDR)
		return c->addr == match->addr;
	return 0;
}

/* the sensors' ident op is now an ioctl, which passes no chip */
static inline int v4l2_chip_ident_i2c_client(struct i2c_client *c,
		struct v4l2_dbg_chip_ident *chip, u32 ident, u32 revision)
{
	if (!chip || !v4l2_chip_match_i2c_client(c, &chip->match))
		return 0;
	chip->ident = ident;
	chip->revision = revision;
	return 0;
}

#endif
