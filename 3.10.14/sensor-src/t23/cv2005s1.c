/*
 * cv2005s1.c
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Secondary CV2005 of a T23 dual-sensor pair (CONFIG_MULTI_SENSOR), the
 * frame sync slave; cv2005s0.c is the primary. The register tables,
 * timings and frame sync writes are those of the stock T23 driver
 * sensor_cv2005s1_t23 (H20250122a), for a 24 MHz MCLK. The register names
 * are the ones of the T32 cv2005.c, which runs a 27 MHz clock tree.
 *
 * @fsync FSync hardware connection: the primary (cv2005s0.c) drives VSYNC
 * out, this sensor runs as frame sync slave.
 * Settings:
 * sboot        resolution      fps       interface              mode
 *   0          1920*1080       15        mipi_2lane           linear
 */
#define DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/proc_fs.h>
#include <soc/gpio.h>

#include <tx-isp-common.h>
#include <sensor-common.h>
#include <txx-funcs.h>

#define SENSOR_I2C_ADDRESS 0x36

#define CV2005S1_CHIP_ID_H	(0x20)
#define CV2005S1_CHIP_ID_L	(0x05)
#define CV2005S1_REG_END		0xffff
#define CV2005S1_REG_DELAY	0xfffe
#define CV2005S1_HTS		1200	/* 0x3021/0x3020 in the init table */
#define CV2005S1_VTS		3000	/* 0x301d/0x301c in the init table */
#define SENSOR_OUTPUT_MAX_FPS 15
#define SENSOR_OUTPUT_MIN_FPS 5
#define SENSOR_VERSION	"H20250122a"

static int reset_gpio = -1;
module_param(reset_gpio, int, S_IRUGO);
MODULE_PARM_DESC(reset_gpio, "Reset GPIO NUM");

static int pwdn_gpio = -1;
module_param(pwdn_gpio, int, S_IRUGO);
MODULE_PARM_DESC(pwdn_gpio, "Power down GPIO NUM");

static int shvflip = 0;
module_param(shvflip, int, S_IRUGO);
MODULE_PARM_DESC(shvflip, "Sensor HV Flip Enable interface");

static int fsync_mode = TX_ISP_SENSOR_FSYNC_MODE_MS_REALTIME_MISPLACE;
module_param(fsync_mode, int, S_IRUGO);
MODULE_PARM_DESC(fsync_mode, "Sensor Indicates the frame synchronization mode");

struct regval_list {
	uint16_t reg_num;
	unsigned char value;
};

/*
 * the part of driver maybe modify about different sensor and different board.
 */
struct again_lut {
	unsigned int value;
	unsigned int gain;
};

/* 0x3118 code, gain in log2 .16 (about 65536 * log2(256 / (256 - code))) */
struct again_lut cv2005s1_again_lut[] = {
	{0x00, 0},
	{0x01, 370},
	{0x02, 741},
	{0x03, 1114},
	{0x04, 1488},
	{0x05, 1864},
	{0x06, 2242},
	{0x07, 2621},
	{0x08, 3001},
	{0x09, 3383},
	{0x0a, 3767},
	{0x0b, 4152},
	{0x0c, 4539},
	{0x0d, 4927},
	{0x0e, 5317},
	{0x0f, 5708},
	{0x10, 6102},
	{0x11, 6496},
	{0x12, 6893},
	{0x13, 7291},
	{0x14, 7691},
	{0x15, 8092},
	{0x16, 8495},
	{0x17, 8900},
	{0x18, 9307},
	{0x19, 9715},
	{0x1a, 10125},
	{0x1b, 10537},
	{0x1c, 10951},
	{0x1d, 11367},
	{0x1e, 11784},
	{0x1f, 12204},
	{0x20, 12625},
	{0x21, 13048},
	{0x22, 13473},
	{0x23, 13900},
	{0x24, 14328},
	{0x25, 14759},
	{0x26, 15192},
	{0x27, 15626},
	{0x28, 16063},
	{0x29, 16502},
	{0x2a, 16943},
	{0x2b, 17386},
	{0x2c, 17831},
	{0x2d, 18278},
	{0x2e, 18727},
	{0x2f, 19178},
	{0x30, 19631},
	{0x31, 20087},
	{0x32, 20545},
	{0x33, 21005},
	{0x34, 21467},
	{0x35, 21932},
	{0x36, 22399},
	{0x37, 22868},
	{0x38, 23340},
	{0x39, 23814},
	{0x3a, 24290},
	{0x3b, 24769},
	{0x3c, 25250},
	{0x3d, 25734},
	{0x3e, 26220},
	{0x3f, 26708},
	{0x40, 27199},
	{0x41, 27693},
	{0x42, 28189},
	{0x43, 28688},
	{0x44, 29190},
	{0x45, 29694},
	{0x46, 30201},
	{0x47, 30711},
	{0x48, 31223},
	{0x49, 31739},
	{0x4a, 32257},
	{0x4b, 32778},
	{0x4c, 33301},
	{0x4d, 33828},
	{0x4e, 34358},
	{0x4f, 34891},
	{0x50, 35426},
	{0x51, 35965},
	{0x52, 36507},
	{0x53, 37052},
	{0x54, 37600},
	{0x55, 38151},
	{0x56, 38706},
	{0x57, 39263},
	{0x58, 39825},
	{0x59, 40389},
	{0x5a, 40957},
	{0x5b, 41528},
	{0x5c, 42103},
	{0x5d, 42681},
	{0x5e, 43263},
	{0x5f, 43849},
	{0x60, 44438},
	{0x61, 45030},
	{0x62, 45627},
	{0x63, 46227},
	{0x64, 46831},
	{0x65, 47439},
	{0x66, 48051},
	{0x67, 48667},
	{0x68, 49287},
	{0x69, 49911},
	{0x6a, 50540},
	{0x6b, 51172},
	{0x6c, 51809},
	{0x6d, 52450},
	{0x6e, 53095},
	{0x6f, 53745},
	{0x70, 54399},
	{0x71, 55058},
	{0x72, 55722},
	{0x73, 56390},
	{0x74, 57063},
	{0x75, 57741},
	{0x76, 58423},
	{0x77, 59111},
	{0x78, 59804},
	{0x79, 60501},
	{0x7a, 61204},
	{0x7b, 61913},
	{0x7c, 62626},
	{0x7d, 63345},
	{0x7e, 64070},
	{0x7f, 64800},
	{0x80, 65536},
	{0x81, 66277},
	{0x82, 67024},
	{0x83, 67778},
	{0x84, 68537},
	{0x85, 69303},
	{0x86, 70075},
	{0x87, 70853},
	{0x88, 71638},
	{0x89, 72429},
	{0x8a, 73227},
	{0x8b, 74031},
	{0x8c, 74843},
	{0x8d, 75661},
	{0x8e, 76487},
	{0x8f, 77320},
	{0x90, 78161},
	{0x91, 79009},
	{0x92, 79864},
	{0x93, 80728},
	{0x94, 81599},
	{0x95, 82479},
	{0x96, 83367},
	{0x97, 84263},
	{0x98, 85167},
	{0x99, 86081},
	{0x9a, 87003},
	{0x9b, 87935},
	{0x9c, 88876},
	{0x9d, 89826},
	{0x9e, 90786},
	{0x9f, 91756},
	{0xa0, 92735},
	{0xa1, 93725},
	{0xa2, 94726},
	{0xa3, 95737},
	{0xa4, 96759},
	{0xa5, 97793},
	{0xa6, 98837},
	{0xa7, 99894},
	{0xa8, 100962},
	{0xa9, 102043},
	{0xaa, 103136},
	{0xab, 104242},
	{0xac, 105361},
	{0xad, 106493},
	{0xae, 107639},
	{0xaf, 108799},
	{0xb0, 109974},
	{0xb1, 111163},
	{0xb2, 112367},
	{0xb3, 113587},
	{0xb4, 114823},
	{0xb5, 116076},
	{0xb6, 117345},
	{0xb7, 118631},
	{0xb8, 119935},
	{0xb9, 121258},
	{0xba, 122599},
	{0xbb, 123959},
	{0xbc, 125340},
	{0xbd, 126740},
	{0xbe, 128162},
	{0xbf, 129606},
	{0xc0, 131072},
	{0xc1, 132560},
	{0xc2, 134073},
	{0xc3, 135611},
	{0xc4, 137174},
	{0xc5, 138763},
	{0xc6, 140379},
	{0xc7, 142023},
	{0xc8, 143697},
	{0xc9, 145400},
	{0xca, 147135},
	{0xcb, 148903},
	{0xcc, 150703},
	{0xcd, 152539},
	{0xce, 154412},
	{0xcf, 156322},
	{0xd0, 158271},
	{0xd1, 160262},
	{0xd2, 162295},
	{0xd3, 164373},
	{0xd4, 166498},
	{0xd5, 168672},
	{0xd6, 170897},
	{0xd7, 173175},
	{0xd8, 175510},
	{0xd9, 177903},
	{0xda, 180359},
	{0xdb, 182881},
	{0xdc, 185471},
	{0xdd, 188135},
	{0xde, 190876},
	{0xdf, 193698},
	{0xe0, 196608},
	{0xe1, 199609},
	{0xe2, 202710},
	{0xe3, 205915},
	{0xe4, 209233},
	{0xe5, 212671},
	{0xe6, 216239},
	{0xe7, 219948},
	{0xe8, 223807},
	{0xe9, 227831},
	{0xea, 232034},
	{0xeb, 236433},
	{0xec, 241046},
	{0xed, 245895},
	{0xee, 251007},
	{0xef, 256412},
	{0xf0, 262144},
	{0xf1, 268247},
	{0xf2, 274770},
	{0xf3, 281774},
	{0xf4, 289342},
	{0xf5, 297571},
	{0xf6, 306582},
	{0xf7, 316542},
	{0xf8, 327680},
};

struct tx_isp_sensor_attribute cv2005s1_attr;

unsigned int cv2005s1_alloc_again(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_again)
{
	struct again_lut *lut = cv2005s1_again_lut;
	while(lut->gain <= cv2005s1_attr.max_again) {
		if(isp_gain == 0) {
			*sensor_again = lut[0].value;
			return 0;
		} else if(isp_gain < lut->gain) {
			*sensor_again = (lut - 1)->value;
			return (lut - 1)->gain;
		} else {
			if((lut->gain == cv2005s1_attr.max_again) && (isp_gain >= lut->gain)) {
				*sensor_again = lut->value;
				return lut->gain;
			}
		}

		lut++;
	}

	return isp_gain;
}

unsigned int cv2005s1_alloc_dgain(unsigned int isp_gain, unsigned char shift, unsigned int *sensor_dgain)
{
	return 0;
}

struct tx_isp_sensor_attribute cv2005s1_attr={
	.name = "cv2005s1",
	.chip_id = 0x2005,
	.cbus_type = TX_SENSOR_CONTROL_INTERFACE_I2C,
	.cbus_mask = V4L2_SBUS_MASK_SAMPLE_8BITS | V4L2_SBUS_MASK_ADDR_16BITS,
	.cbus_device = SENSOR_I2C_ADDRESS,
	.dbus_type = TX_SENSOR_DATA_INTERFACE_MIPI,
	.mipi = {
		.mode = SENSOR_MIPI_OTHER_MODE,
		.clk = 1080,
		.lans = 2,
		.settle_time_apative_en = 0,
		.mipi_sc.sensor_csi_fmt = TX_SENSOR_RAW10,
		.mipi_sc.hcrop_diff_en = 0,
		.mipi_sc.mipi_vcomp_en = 0,
		.mipi_sc.mipi_hcomp_en = 0,
		.mipi_sc.line_sync_mode = 0,
		.mipi_sc.work_start_flag = 0,
		.image_twidth = 1920,
		.image_theight = 1080,
		.mipi_sc.mipi_crop_start0x = 0,
		.mipi_sc.mipi_crop_start0y = 0,
		.mipi_sc.mipi_crop_start1x = 0,
		.mipi_sc.mipi_crop_start1y = 0,
		.mipi_sc.mipi_crop_start2x = 0,
		.mipi_sc.mipi_crop_start2y = 0,
		.mipi_sc.mipi_crop_start3x = 0,
		.mipi_sc.mipi_crop_start3y = 0,
		.mipi_sc.data_type_en = 0,
		.mipi_sc.data_type_value = RAW10,
		.mipi_sc.del_start = 0,
		.mipi_sc.sensor_frame_mode = TX_SENSOR_DEFAULT_FRAME_MODE,
		.mipi_sc.sensor_fid_mode = 0,
		.mipi_sc.sensor_mode = TX_SENSOR_DEFAULT_MODE,
	},
	.data_type = TX_SENSOR_DATA_TYPE_LINEAR,
	.max_again = 327680,
	.max_dgain = 0,
	.min_integration_time = 1,
	.min_integration_time_native = 1,
	.max_integration_time_native = CV2005S1_VTS - 10,
	.integration_time_limit = CV2005S1_VTS - 10,
	.total_width = CV2005S1_HTS,
	.total_height = CV2005S1_VTS,
	.max_integration_time = CV2005S1_VTS - 10,
	.one_line_expr_in_us = 18,
	.integration_time_apply_delay = 2,
	.again = 0,
	.integration_time = 0x0,
	.again_apply_delay = 2,
	.dgain_apply_delay = 0,
	.sensor_ctrl.alloc_again = cv2005s1_alloc_again,
	.sensor_ctrl.alloc_dgain = cv2005s1_alloc_dgain,
	.fsync_attr = {
		.mode = TX_ISP_SENSOR_FSYNC_MODE_MS_REALTIME_MISPLACE,
		.call_times = 1,
		.sdelay = 500,
	}
};

static struct regval_list cv2005s1_init_regs_1920_1080_15fps_mipi[] = {
	{0x3031, 0x00},	/* OB off */
	{0x3204, 0x40},	/* black level */
	{0x359d, 0x01},
	{0x35b0, 0x50},
	{0x35b1, 0x66},
	{0x3158, 0xff},
	{0x389d, 0x0a},
	{0x389c, 0x6a},
	{0x38a0, 0x2b},
	{0x3878, 0x01},
	{0x3879, 0x15},
	{0x356f, 0x02},
	{0x36d8, 0x0c},
	{0x36d9, 0x0c},
	{0x3274, 0x00},
	{0x3275, 0x01},
	{0x3510, 0x24},
	{0x3512, 0x80},
	{0x3513, 0x01},
	{0x3109, 0x01},	/* split gain enable */
	{0x3420, 0x3f},	/* MIPI THS-PREPARE */
	{0x3422, 0xc7},	/* MIPI THS-ZERO */
	{0x3424, 0x5f},	/* MIPI THS-TRAIL */
	{0x3426, 0x87},	/* MIPI THS-EXIT */
	{0x3428, 0x47},	/* MIPI TLPX */
	{0x3538, 0x01},	/* 15 fps low power settings, to 0x3513 */
	{0x3628, 0x66},	/* M HBLK */
	{0x3629, 0x7e},
	{0x3510, 0x7d},
	{0x3512, 0x80},
	{0x3513, 0x01},
	{0x3021, 0x04},	/* HMAX = 1200 */
	{0x3020, 0xb0},
	{0x3808, 0x5a},	/* PLL; the 27 MHz T32 table has 0x64 (1.2 GHz) */
	{0x380a, 0x02},
	{0x301d, 0x0b},	/* VMAX = 3000 */
	{0x301c, 0xb8},
	{0x3834, 0x00},	/* LDO on */
	{0x3001, 0x01},	/* frame sync slave setup, to 0x3944 */
	{0x3076, 0x02},
	{0x306a, 0x0f},
	{0x3944, 0xf0},
	{0x3000, 0x00},	/* streaming */
	{CV2005S1_REG_END, 0x00},	/* END MARKER */
};

static struct tx_isp_sensor_win_setting cv2005s1_win_sizes[] = {
	{
		.width		= 1920,
		.height		= 1080,
		.fps		= 15 << 16 | 1,
		.mbus_code	= V4L2_MBUS_FMT_SRGGB10_1X10,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= cv2005s1_init_regs_1920_1080_15fps_mipi,
	},
};
struct tx_isp_sensor_win_setting *wsize = &cv2005s1_win_sizes[0];

static struct regval_list cv2005s1_stream_on_mipi[] = {
	{0x3000, 0x00},
	{CV2005S1_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list cv2005s1_stream_off_mipi[] = {
	{0x3000, 0x01},
	{CV2005S1_REG_END, 0x00},	/* END MARKER */
};

int cv2005s1_read(struct tx_isp_subdev *sd, uint16_t reg, unsigned char *value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned char buf[2] = {reg >> 8, reg & 0xff};
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= buf,
		},
		[1] = {
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= value,
		}
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}

int cv2005s1_write(struct tx_isp_subdev *sd, uint16_t reg, unsigned char value)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	uint8_t buf[3] = {(reg >> 8) & 0xff, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 3,
		.buf	= buf,
	};
	int ret;
	ret = private_i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}

static int cv2005s1_write_array(struct tx_isp_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != CV2005S1_REG_END) {
		if (vals->reg_num == CV2005S1_REG_DELAY) {
			private_msleep(vals->value);
		} else {
			ret = cv2005s1_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		vals++;
	}

	return 0;
}

static int cv2005s1_reset(struct tx_isp_subdev *sd, int val)
{
	return 0;
}

static int cv2005s1_detect(struct tx_isp_subdev *sd, unsigned int *ident)
{
	int ret;
	unsigned char v;

	ret = cv2005s1_read(sd, 0x3003, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != CV2005S1_CHIP_ID_H)
		return -ENODEV;
	*ident = v;

	ret = cv2005s1_read(sd, 0x3002, &v);
	ISP_WARNING("-----%s: %d ret = %d, v = 0x%02x\n", __func__, __LINE__, ret,v);
	if (ret < 0)
		return ret;
	if (v != CV2005S1_CHIP_ID_L)
		return -ENODEV;
	*ident = (*ident << 8) | v;

	return 0;
}

static int cv2005s1_set_expo(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;
	int it = value & 0xffff;
	int again = (value & 0xffff0000) >> 16;
	int shr = sensor->video.attr->total_height - it;

	/* the shutter registers hold VMAX minus the integration time */
	ret += cv2005s1_write(sd, 0x3049, (unsigned char)((shr >> 8) & 0xff));
	ret += cv2005s1_write(sd, 0x3048, (unsigned char)(shr & 0xff));
	/* gain-dependent analog setting of the stock T23 driver */
	if (again < 0x51) {
		ret += cv2005s1_write(sd, 0x3666, 0x01);
		ret += cv2005s1_write(sd, 0x3667, 0x1d);
	} else {
		ret += cv2005s1_write(sd, 0x3666, 0x7f);
		ret += cv2005s1_write(sd, 0x3667, 0x7f);
	}
	ret += cv2005s1_write(sd, 0x3118, (unsigned char)(again & 0xff));
	if (ret < 0)
		return ret;

	return 0;
}

static int cv2005s1_set_logic(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int cv2005s1_set_digital_gain(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int cv2005s1_get_black_pedestal(struct tx_isp_subdev *sd, int value)
{
	return 0;
}

static int cv2005s1_init(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if(!enable)
		return ISP_SUCCESS;

	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;

	ret = cv2005s1_write_array(sd, wsize->regs);

	if (ret)
		return ret;
	ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	sensor->priv = wsize;

	return 0;
}

static int cv2005s1_s_stream(struct tx_isp_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = cv2005s1_write_array(sd, cv2005s1_stream_on_mipi);
		ISP_WARNING("cv2005s1 stream on\n");

	} else {
		ret = cv2005s1_write_array(sd, cv2005s1_stream_off_mipi);
		ISP_WARNING("cv2005s1 stream off\n");
	}

	return ret;
}

/*
 * The frame rate stays the 15 fps of the init table, as in the stock T23
 * driver: a rate in range is accepted and nothing is written.
 */
static int cv2005s1_set_fps(struct tx_isp_subdev *sd, int fps)
{
	unsigned int newformat = 0; //the format is 24.8

	newformat = (((fps >> 16) / (fps & 0xffff)) << 8) + ((((fps >> 16) % (fps & 0xffff)) << 8) / (fps & 0xffff));
	if(newformat > (SENSOR_OUTPUT_MAX_FPS << 8) || newformat < (SENSOR_OUTPUT_MIN_FPS << 8)) {
		ISP_ERROR("warn: fps(0x%x) no in range\n", fps);
		return -1;
	}

	return 0;
}

static int cv2005s1_set_mode(struct tx_isp_subdev *sd, int value)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = ISP_SUCCESS;

	if(wsize){
		sensor->video.mbus.width = wsize->width;
		sensor->video.mbus.height = wsize->height;
		sensor->video.mbus.code = wsize->mbus_code;
		sensor->video.mbus.field = V4L2_FIELD_NONE;
		sensor->video.mbus.colorspace = wsize->colorspace;
		sensor->video.fps = wsize->fps;
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);
	}

	return ret;
}

static int cv2005s1_g_chip_ident(struct tx_isp_subdev *sd,
			       struct tx_isp_chip_ident *chip)
{
	struct i2c_client *client = tx_isp_get_subdevdata(sd);
	unsigned int ident = 0;
	int ret = ISP_SUCCESS;
	if(reset_gpio != -1){
		ret = private_gpio_request(reset_gpio,"cv2005s1_reset");
		if(!ret){
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(50);
			private_gpio_direction_output(reset_gpio, 0);
			private_msleep(35);
			private_gpio_direction_output(reset_gpio, 1);
			private_msleep(35);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",reset_gpio);
		}
	}
	if(pwdn_gpio != -1){
		ret = private_gpio_request(pwdn_gpio,"cv2005s1_pwdn");
		if(!ret){
			private_gpio_direction_output(pwdn_gpio, 1);
			private_msleep(150);
			private_gpio_direction_output(pwdn_gpio, 0);
			private_msleep(10);
		}else{
			ISP_ERROR("gpio requrest fail %d\n",pwdn_gpio);
		}
	}
	ret = cv2005s1_detect(sd, &ident);
	if (ret) {
		ISP_ERROR("chip found @ 0x%x (%s) is not an cv2005s1 chip.\n",
			  client->addr, client->adapter->name);
		return ret;
	}
	ISP_WARNING("cv2005s1 chip found @ 0x%02x (%s)\n", client->addr, client->adapter->name);
	ISP_WARNING("sensor driver version %s\n",SENSOR_VERSION);
	if(chip){
		memcpy(chip->name, "cv2005s1", sizeof("cv2005s1"));
		chip->ident = ident;
		chip->revision = SENSOR_VERSION;
	}
	return 0;
}

/*
 * As the stock T23 driver: a non-zero value programs what the T32 cv2005.c
 * calls normal (0x3028 = 0x03), zero what it calls mirror + flip
 * (0x3028 = 0x00). The Bayer order reported to the ISP does not change.
 */
static int cv2005s1_set_vflip(struct tx_isp_subdev *sd, int enable)
{
	struct tx_isp_sensor *sensor = sd_to_sensor_device(sd);
	int ret = 0;

	if (enable) {
		ret = cv2005s1_write(sd, 0x3028, 0x03);
		ret += cv2005s1_write(sd, 0x3038, 0x05);
		ret += cv2005s1_write(sd, 0x3034, 0x05);
	} else {
		ret = cv2005s1_write(sd, 0x3028, 0x00);
		ret += cv2005s1_write(sd, 0x3038, 0x04);
		ret += cv2005s1_write(sd, 0x3034, 0x04);
	}
	sensor->video.mbus_change = 1;
	if(!ret)
		ret = tx_isp_call_subdev_notify(sd, TX_ISP_EVENT_SYNC_SENSOR_ATTR, &sensor->video);

	return ret;
}

static int cv2005s1_fsync(struct tx_isp_subdev *sd, struct tx_isp_sensor_fsync *fsync)
{
	if (fsync->place != TX_ISP_SENSOR_FSYNC_PLACE_STREAMON_AFTER)
		return 0;

	switch (fsync->call_index) {
	case 0:
		switch (fsync_mode) {
		case TX_ISP_SENSOR_FSYNC_MODE_MS_REALTIME_MISPLACE:
			/* frame sync slave, as the stock secondary driver */
			cv2005s1_write(sd, 0x3076, 0x02);
			cv2005s1_write(sd, 0x3001, 0x01);
			cv2005s1_write(sd, 0x306a, 0x0f);
			cv2005s1_write(sd, 0x3000, 0x00);
			break;
		}
		break;
	}

	return 0;
}

static int cv2005s1_sensor_ops_ioctl(struct tx_isp_subdev *sd, unsigned int cmd, void *arg)
{
	long ret = 0;

	if(IS_ERR_OR_NULL(sd)){
		ISP_ERROR("[%d]The pointer is invalid!\n", __LINE__);
		return -EINVAL;
	}
	switch(cmd){
	case TX_ISP_EVENT_SENSOR_EXPO:
		if(arg)
			ret = cv2005s1_set_expo(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_INT_TIME:
	case TX_ISP_EVENT_SENSOR_AGAIN:
		/* both come with TX_ISP_EVENT_SENSOR_EXPO */
		break;
	case TX_ISP_EVENT_SENSOR_DGAIN:
		if(arg)
			ret = cv2005s1_set_digital_gain(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_BLACK_LEVEL:
		if(arg)
			ret = cv2005s1_get_black_pedestal(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_RESIZE:
		if(arg)
			ret = cv2005s1_set_mode(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_PREPARE_CHANGE:
		ret = cv2005s1_write_array(sd, cv2005s1_stream_off_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FINISH_CHANGE:
		ret = cv2005s1_write_array(sd, cv2005s1_stream_on_mipi);
		break;
	case TX_ISP_EVENT_SENSOR_FPS:
		if(arg)
			ret = cv2005s1_set_fps(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_VFLIP:
		if(arg)
			ret = cv2005s1_set_vflip(sd, *(int*)arg);
		break;
	case TX_ISP_EVENT_SENSOR_LOGIC:
		if(arg)
			ret = cv2005s1_set_logic(sd, *(int*)arg);
		break;
	default:
		break;
	}

	return ret;
}

static int cv2005s1_g_register(struct tx_isp_subdev *sd, struct tx_isp_dbg_register *reg)
{
	unsigned char val = 0;
	int len = 0;
	int ret = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = cv2005s1_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;

	return ret;
}

static int cv2005s1_s_register(struct tx_isp_subdev *sd, const struct tx_isp_dbg_register *reg)
{
	int len = 0;

	len = strlen(sd->chip.name);
	if(len && strncmp(sd->chip.name, reg->name, len)){
		return -EINVAL;
	}
	if (!private_capable(CAP_SYS_ADMIN))
		return -EPERM;
	cv2005s1_write(sd, reg->reg & 0xffff, reg->val & 0xff);

	return 0;
}

static struct tx_isp_subdev_core_ops cv2005s1_core_ops = {
	.g_chip_ident = cv2005s1_g_chip_ident,
	.reset = cv2005s1_reset,
	.init = cv2005s1_init,
	.g_register = cv2005s1_g_register,
	.s_register = cv2005s1_s_register,
};

static struct tx_isp_subdev_video_ops cv2005s1_video_ops = {
	.s_stream = cv2005s1_s_stream,
};

static struct tx_isp_subdev_sensor_ops	cv2005s1_sensor_ops = {
	.ioctl	= cv2005s1_sensor_ops_ioctl,
	.fsync	= cv2005s1_fsync,
};

static struct tx_isp_subdev_ops cv2005s1_ops = {
	.core = &cv2005s1_core_ops,
	.video = &cv2005s1_video_ops,
	.sensor = &cv2005s1_sensor_ops,
};

/* It's the sensor device */
static u64 tx_isp_module_dma_mask = ~(u64)0;
struct platform_device sensor_platform_device = {
	.name = "cv2005s1",
	.id = -1,
	.dev = {
		.dma_mask = &tx_isp_module_dma_mask,
		.coherent_dma_mask = 0xffffffff,
		.platform_data = NULL,
	},
	.num_resources = 0,
};

static int sensor_mclk_config(struct tx_isp_sensor *sensor, unsigned long want_rate)
{
        unsigned long rate = 0;
        struct clk *pll = NULL;
        char *plls[] = {"mpll", "sclka"};
        int psize = sizeof(plls) / sizeof(char *);
        char *ppll = plls[psize - 1];
        int ret = 0, i = 0;

        pll = clk_get_parent(sensor->mclk);
        rate = clk_get_rate(pll);
        if (rate % want_rate) {
                for (i = 0; i < psize; i++) {
                        pll = clk_get(NULL, plls[i]);
                        rate = clk_get_rate(pll);
                        if (!(rate % want_rate)) {
                                ret = clk_set_parent(sensor->mclk, pll);
                                if (ret) {
                                        ISP_WARNING("[%s %d] %s mounted node switchover failed !!!\n",
                                                    __func__, __LINE__, plls[i]);
                                        continue;
                                } else {
                                        break;
                                }
                        }
                }
                if (i == psize) {
                        if (!ret) {
                                pll = clk_get(NULL, ppll);
                                rate = clk_get_rate(pll);
                                if(want_rate == 37125000){
                                        if((rate >= 1188000000)) {
                                                rate = 1188000000;
                                        } else if (rate >= 891000000) {
                                                rate = 891000000;
                                        } else {
                                                ISP_ERROR("[%s %d] The %s clock setting failed !!!\n",
                                                          __func__, __LINE__, ppll);
                                                ret = -1;
                                                goto error;
                                        }
                                } else if (want_rate == 24000000 || want_rate == 27000000) {
                                        rate -= rate % want_rate;
                                } else {
                                        ret = -1;
                                        goto error;
                                }
                                ret = private_clk_set_rate(pll, rate);
                                if (ret) {
                                        ISP_WARNING("[%s %d] Failed to set %s !!!\n",
                                                    __func__, __LINE__, ppll);
                                        goto error;
                                } else {
                                        ISP_WARNING("[%s %d] !!!!!!!!!!! The %s frequency has been changed to %ld !!!\n",
                                                    __func__, __LINE__, ppll, rate);
                                }
                                ret = clk_set_parent(sensor->mclk, pll);
                                if (ret) {
                                        ISP_WARNING("[%s %d] %s mounted node switchover failed !!!\n",
                                                    __func__, __LINE__, ppll);
                                        goto error;
                                }
                        } else {
                                goto error;
                        }
                }
        }
        private_clk_set_rate(sensor->mclk, want_rate);
        private_clk_enable(sensor->mclk);

        rate = clk_get_rate(sensor->mclk);
        if (rate % want_rate) {
                ret = -1;
                goto error;
        }

        return ret;

error:
        ISP_ERROR("[%s %d] Unable to allocate the required MCLK %ld !!!\n",
                  __func__, __LINE__, want_rate);
        return ret;
}

static int cv2005s1_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct tx_isp_subdev *sd;
	struct tx_isp_sensor *sensor;

	sensor = (struct tx_isp_sensor *)kzalloc(sizeof(*sensor), GFP_KERNEL);
	if(!sensor){
		ISP_ERROR("Failed to allocate sensor subdev.\n");
		return -ENOMEM;
	}
	memset(sensor, 0 ,sizeof(*sensor));

#ifdef CONFIG_KERNEL_4_4_94
	sensor->mclk = clk_get(NULL, "div_cim");
#else
	sensor->mclk = clk_get(NULL, "cgu_cim");
#endif
	if (IS_ERR(sensor->mclk)) {
		ISP_ERROR("Cannot get sensor input clock cgu_cim\n");
		goto err_get_mclk;
	}
	sensor_mclk_config(sensor, 24000000);
	cv2005s1_attr.fsync_attr.mode = fsync_mode;
	cv2005s1_attr.max_integration_time_native = CV2005S1_VTS - 10;
	cv2005s1_attr.integration_time_limit = CV2005S1_VTS - 10;
	cv2005s1_attr.max_integration_time = CV2005S1_VTS - 10;

	sd = &sensor->sd;
	sensor->video.shvflip = shvflip;
	sensor->video.attr = &cv2005s1_attr;
	sensor->video.vi_max_width = wsize->width;
	sensor->video.vi_max_height = wsize->height;
	sensor->video.mbus.width = wsize->width;
	sensor->video.mbus.height = wsize->height;
	sensor->video.mbus.code = wsize->mbus_code;
	sensor->video.mbus.field = V4L2_FIELD_NONE;
	sensor->video.mbus.colorspace = wsize->colorspace;
	sensor->video.fps = wsize->fps;
	tx_isp_subdev_init(&sensor_platform_device, sd, &cv2005s1_ops);
	tx_isp_set_subdevdata(sd, client);
	tx_isp_set_subdev_hostdata(sd, sensor);
	private_i2c_set_clientdata(client, sd);

	ISP_WARNING("probe ok ------->cv2005s1\n");

	return 0;
err_get_mclk:
	kfree(sensor);

	return -1;
}

static int cv2005s1_remove(struct i2c_client *client)
{
	struct tx_isp_subdev *sd = private_i2c_get_clientdata(client);
	struct tx_isp_sensor *sensor = tx_isp_get_subdev_hostdata(sd);

	if(reset_gpio != -1)
		private_gpio_free(reset_gpio);
	if(pwdn_gpio != -1)
		private_gpio_free(pwdn_gpio);

	private_clk_disable(sensor->mclk);
	private_clk_put(sensor->mclk);
	tx_isp_subdev_deinit(sd);
	kfree(sensor);

	return 0;
}

static const struct i2c_device_id cv2005s1_id[] = {
	{ "cv2005s1", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cv2005s1_id);

static struct i2c_driver cv2005s1_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "cv2005s1",
	},
	.probe		= cv2005s1_probe,
	.remove		= cv2005s1_remove,
	.id_table	= cv2005s1_id,
};

static __init int init_cv2005s1(void)
{
	int ret = 0;
	ret = private_driver_get_interface();
	if(ret){
		ISP_ERROR("Failed to init cv2005s1 dirver.\n");
		return -1;
	}
	return private_i2c_add_driver(&cv2005s1_driver);
}

static __exit void exit_cv2005s1(void)
{
	private_i2c_del_driver(&cv2005s1_driver);
}

module_init(init_cv2005s1);
module_exit(exit_cv2005s1);

MODULE_DESCRIPTION("A low-level driver for cv2005s1 sensor");
MODULE_LICENSE("GPL");
