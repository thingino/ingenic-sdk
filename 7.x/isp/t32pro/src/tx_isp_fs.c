#include "tx_isp_fs.h"

#define FRAME_CHAN_MISC_COUNT 20

frame_chan_misc_t *g_frame_chan_misc = NULL;
static uint32_t frame_chan_ops = TISP_OPS_SDK;

int fs_channel_open(struct inode *inode, struct file *file)
{
	void *data = NULL;
	struct miscdevice *mdev = NULL;

	if (!file || !file->private_data)
		return -ENODEV;

	mdev = file->private_data;
	data = miscdev_to_frame_chan_misc(mdev)->data;
	if (!data)
		return -ENODEV;

	frame_chan_ops = TISP_OPS_SDK;
	lib_tx_isp_fs_sync_opstype(TISP_OPS_SDK);
	lib_tx_isp_frame_channel_open(data);

	return 0;
}

int fs_channel_release(struct inode *inode, struct file *file)
{
	void *data = NULL;
	struct miscdevice *mdev = NULL;

	if (!file || !file->private_data)
		return -ENODEV;

	mdev = file->private_data;
	data = miscdev_to_frame_chan_misc(mdev)->data;
	if (!data)
		return -ENODEV;

	lib_tx_isp_frame_channel_release(data);

	return 0;
}

static long fs_channel_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	uint32_t ops = TISP_OPS_SDK;
	int chan_index = -1;
	void *data = NULL;
	struct miscdevice *mdev = NULL;

	ops = frame_chan_ops;
	if(ops == TISP_OPS_SDK){
		if (!file || !file->private_data)
			return -ENODEV;

		mdev = file->private_data;
		data = miscdev_to_frame_chan_misc(mdev)->data;
		if (!data)
			return -ENODEV;
	} else if (ops == TISP_OPS_KERNEL){
		chan_index = (int)file;
		if (chan_index < 0 || chan_index >= FRAME_CHAN_MISC_COUNT) {
			printk("chan_index = %d, value is error !\n", chan_index);
			return -1;
		} else {
			if (!g_frame_chan_misc)
				return -ENODEV;
			data = g_frame_chan_misc[chan_index].data;
			if (!data)
				return -ENODEV;
		}
	} else {
		return -EINVAL;
	}

	switch (cmd) {
	case TISP_VIDIOC_SET_FRAME_FORMAT:
		ret = lib_tx_isp_frame_channel_vidioc_set_fmt(ops, data, arg);
		break;
	case TISP_VIDIOC_GET_FRAME_FORMAT:
		ret = lib_tx_isp_frame_channel_vidioc_get_fmt(ops, data, arg);
		break;
	case TISP_VIDIOC_REQBUFS:
		ret = lib_tx_isp_frame_channel_vb2_reqbufs(ops, data, arg);
		break;
	case TISP_VIDIOC_QUERYBUF:
		ret = lib_tx_isp_frame_channel_vb2_querybuf(ops, data, arg);
		break;
	case TISP_VIDIOC_QBUF:
		ret = lib_tx_isp_frame_channel_vb2_qbuf(ops, data, arg);
		break;
	case TISP_VIDIOC_DQBUF:
		ret = lib_tx_isp_frame_channel_vb2_dqbuf(ops, data, arg);
		break;
	case TISP_VIDIOC_FRAME_STREAMON:
		ret = lib_tx_isp_frame_channel_vb2_streamon(ops, data, arg);
		break;
	case TISP_VIDIOC_FRAME_STREAMOFF:
		ret = lib_tx_isp_frame_channel_vb2_streamoff(ops, data, arg);
		break;
	case TISP_VIDIOC_DEFAULT_CMD_SET_BANKS:
		ret = lib_tx_isp_frame_channel_set_channel_banks(ops, data, arg);
		break;
	case TISP_VIDIOC_DEFAULT_CMD_LISTEN_BUF:
		ret = lib_tx_isp_frame_channel_listen_buffer(ops, data, arg);
		break;
	case TISP_VIDIOC_SET_RAW_SHARE_LINE:
		ret = lib_tx_isp_set_raw_share_line(ops, data, arg);
		break;

	default:
		ISP_ERROR("[ %s:%d ] the frame ioctl cmd(%d) couldn't be handled!\n",
			  __func__, __LINE__, cmd);
		ret = -ENOIOCTLCMD;
		break;
	}

	return (long)ret;
}

int fs_channel_open_fast(uint32_t chn_num)
{
	void *data = NULL;

	if (!g_frame_chan_misc || chn_num >= FRAME_CHAN_MISC_COUNT) {
		ISP_ERROR("[ %s:%d ] channel %u is invalid!\n",
			  __func__, __LINE__, chn_num);
		return -EINVAL;
	}

	data = g_frame_chan_misc[chn_num].data;
	if (!data) {
		ISP_ERROR("[ %s:%d ] channel %u data is NULL!\n",
			  __func__, __LINE__, chn_num);
		return -EINVAL;
	}

	frame_chan_ops = TISP_OPS_KERNEL;
	lib_tx_isp_fs_sync_opstype(TISP_OPS_KERNEL);

	return lib_tx_isp_frame_channel_open(data);
}

int frame_channel_open_fast(uint32_t chn_num)
{
	return fs_channel_open_fast(chn_num);
}

long frame_channel_unlocked_ioctl_first(int chn, unsigned int cmd, unsigned long arg)
{
	return fs_channel_unlocked_ioctl((struct file *)(unsigned long)chn, cmd, arg);
}

static struct file_operations fs_channel_ops = {
	.open = fs_channel_open,
	.release = fs_channel_release,
	.unlocked_ioctl = fs_channel_unlocked_ioctl,
};

int frame_channel_misc_register(int chn, char *name, void *data)
{
	if (chn >= FRAME_CHAN_MISC_COUNT)
		return -EINVAL;

	/* 初始化指针 仅调用一次 */
	if (!g_frame_chan_misc) {
		g_frame_chan_misc = (void *)private_vmalloc(sizeof(frame_chan_misc_t) * FRAME_CHAN_MISC_COUNT);
		if (!g_frame_chan_misc) {
			return -ENOMEM;
		}
		memset(g_frame_chan_misc, 0, sizeof(frame_chan_misc_t) * FRAME_CHAN_MISC_COUNT);
	}

	memset(&g_frame_chan_misc[chn], 0, sizeof(frame_chan_misc_t));
	g_frame_chan_misc[chn].misc.name = g_frame_chan_misc[chn].name;

	if (chn >= 12) {
		sprintf((char *)g_frame_chan_misc[chn].misc.name, "framechan-orig%d", chn - 12);
	} else {
		sprintf((char *)g_frame_chan_misc[chn].misc.name, "framechan%d", chn);
	}
#ifdef CONFIG_ZERATUL
	if(!strcmp(g_frame_chan_misc[chn].name, "framechan0")){
		g_frame_chan_misc[chn].misc.minor = 60;
	}else if(!strcmp(g_frame_chan_misc[chn].name, "framechan1")){
		g_frame_chan_misc[chn].misc.minor = 59;
	}else if(!strcmp(g_frame_chan_misc[chn].name, "framechan2")){
		g_frame_chan_misc[chn].misc.minor = 58;
	}else if(!strcmp(g_frame_chan_misc[chn].name, "framechan3")){
		g_frame_chan_misc[chn].misc.minor = 44;
	}else if(!strcmp(g_frame_chan_misc[chn].name, "framechan4")){
		g_frame_chan_misc[chn].misc.minor = 43;
	}else if(!strcmp(g_frame_chan_misc[chn].name, "framechan5")){
		g_frame_chan_misc[chn].misc.minor = 42;
	}else if(!strcmp(g_frame_chan_misc[chn].name, "framechan6")){
		g_frame_chan_misc[chn].misc.minor = 41;
	}else if(!strcmp(g_frame_chan_misc[chn].name, "framechan7")){
		g_frame_chan_misc[chn].misc.minor = 40;
	}else if(!strcmp(g_frame_chan_misc[chn].name, "framechan8")){
		g_frame_chan_misc[chn].misc.minor = 39;
	}else{
		g_frame_chan_misc[chn].misc.minor = MISC_DYNAMIC_MINOR;
	}
#else
	g_frame_chan_misc[chn].misc.minor = MISC_DYNAMIC_MINOR;
#endif /* CONFIG_ZERATUL */
	g_frame_chan_misc[chn].misc.fops = &fs_channel_ops;

	if (misc_register(&g_frame_chan_misc[chn].misc)) {
		return -1;
	}

	g_frame_chan_misc[chn].data = data;

	return 0;
}

void frame_channel_misc_unregister(int chn)
{
	if (chn >= FRAME_CHAN_MISC_COUNT || !g_frame_chan_misc)
		return;

	misc_deregister(&g_frame_chan_misc[chn].misc);
}

void frame_channel_misc_deregister(int chn, char *name)
{
	frame_channel_misc_unregister(chn);
}
