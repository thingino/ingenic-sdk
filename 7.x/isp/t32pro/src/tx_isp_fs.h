/*
 * Header: tx_isp_fs.h
 * Role  : Frame source 和 frame channel 私有定义。
 * Scope : tx_isp_fs_device、frame channel misc device 和快速 open/register 接口。
 * Notes : 仅供 tx-isp driver 内部 frame channel 路径使用。
 */

#include "lib_base.h"
#include "tx-isp-device.h"
#include "tx-isp-videodev.h"
#include "txx-funcs.h"

struct tx_isp_fs_device {
	/* the common parameters */
	struct tx_isp_subdev sd;
	spinlock_t slock;
};

#define miscdev_to_frame_chan_misc(mdev) (container_of((mdev), frame_chan_misc_t, misc))

typedef struct frame_chan_misc {
    struct miscdevice misc;
	char name[32];
	void *data; /* 存放底层传过来的framechan */
} frame_chan_misc_t;

int frame_channel_misc_register(int chn, char *name, void *data);
void frame_channel_misc_deregister(int chn, char *name);
int fs_channel_open_fast(uint32_t chn_num);
int frame_channel_open_fast(uint32_t chn_num);
