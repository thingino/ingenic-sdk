/**
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/clk.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/sound.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <linux/dma-mapping.h>
#include <dt-bindings/dma/ingenic-pdma.h>
#include <linux/wait.h>
#include <linux/i2c.h>
#include <linux/of.h>
#include "audio_aic.h"
#include "../include/audio_common.h"
#include "../include/codec-common.h"
#include "../include/audio_dsp.h"
#include "../include/audio_debug.h"


#define AUDIO_DRIVER_VERSION "V30-20201030a"
#define DEFAULT_EXCODEC_ADDR 0xff

static int mic_mono_channel = 1;
module_param(mic_mono_channel, int, S_IRUGO);
MODULE_PARM_DESC(mic_mono_channel, "mic record channel select:1->left channel 2->right channel");

static int samplerate = 0;
module_param(samplerate, int, S_IRUGO);
MODULE_PARM_DESC(samplerate, "max samplerate you use");

static int i2c_bus = 0x01;
module_param(i2c_bus, int, S_IRUGO);
MODULE_PARM_DESC(i2c_bus, "the i2c bus of extern codec");

static int excodec_addr = DEFAULT_EXCODEC_ADDR;
module_param(excodec_addr, int, S_IRUGO);
MODULE_PARM_DESC(excodec_addr, "the i2c addr of extern codec");

static char excodec_name[I2C_NAME_SIZE];
module_param_string(excodec_name, excodec_name, sizeof(excodec_name), S_IRUGO);
MODULE_PARM_DESC(excodec_name, "the name of extern codec");

static int internal_codec = 1;
module_param(internal_codec, int, S_IRUGO);
MODULE_PARM_DESC(internal_codec, "1:internal codec,0:external codec");

static struct audio_aic_device *globe_aic = NULL;

struct codec_sign_configure aic2codec_signs[4] = {
	/* I2S LR MODE */
	{
		.data = DATA_I2S_MODE,
		.seq = LEFT_FIRST_MODE,
		.sync = LEFT_LOW_RIGHT_HIGH,
		.rate2mclk = 256,
	},
	/* I2S RL MODE */
	{
		.data = DATA_I2S_MODE,
		.seq = RIGHT_FIRST_MODE,
		.sync = LEFT_LOW_RIGHT_HIGH,
		.rate2mclk = 256,
	},
	/* MSB-justified LR MODE */
	{
		.data = DATA_LEFT_JUSTIFIED_MODE,
		.seq = LEFT_FIRST_MODE,
		.sync = LEFT_HIGH_RIGHT_LOW,
		.rate2mclk = 256,
	},
	/* MSB-justified RL MODE */
	{
		.data = DATA_LEFT_JUSTIFIED_MODE,
		.seq = RIGHT_FIRST_MODE,
		.sync = LEFT_HIGH_RIGHT_LOW,
		.rate2mclk = 256,
	},
};

/* Load an i2c sub-device. */
static struct codec_attributes *i2c_new_subdev_board(struct i2c_adapter *adapter, struct i2c_board_info *info,
		const unsigned short *probe_addrs)
{
	struct codec_attributes * attrs = NULL;
	struct i2c_client *client;
	request_module(I2C_MODULE_PREFIX "%s", info->type);
	/* Create the i2c client */

	if (info->addr == 0)
		return NULL;
	else
		client = i2c_new_client_device(adapter, info);

	/* Note: by loading the module first we are certain that c->driver
	   will be set if the driver was found. If the module was not loaded
	   first, then the i2c core tries to delay-load the module for us,
	   and then c->driver is still NULL until the module is finally
	   loaded. This delay-load mechanism doesn't work if other drivers
	   want to use the i2c device, so explicitly loading the module
	   is the best alternative. */
	if (IS_ERR_OR_NULL(client) || client->dev.driver == NULL)
		goto error;

	/* Lock the module so we can safely get the tx_isp_subdev pointer */
	if (!try_module_get(client->dev.driver->owner))
		goto error;
	attrs = i2c_get_clientdata(client);

	/* Decrease the module use count to match the first try_module_get. */
	module_put(client->dev.driver->owner);
	return attrs;

error:
	/* If we have a client but no subdev, then something went wrong and
	   we must unregister the client. */
	if (client && attrs == NULL)
		i2c_unregister_device(client);
	return attrs;
}

static int extern_codec_register(struct audio_aic_device *aic, int i2c_adapter, char *name, int i2c_addr)
{
	struct codec_attributes * attrs = NULL;
	int ret = AUDIO_SUCCESS;

	struct i2c_adapter *adapter;
	struct i2c_board_info board_info;

	adapter = i2c_get_adapter(i2c_adapter);
	if (!adapter) {
		printk("Failed to get I2C adapter %d, deferring probe\n",
				i2c_adapter);
		return -EINVAL;
	}
	memset(&board_info, 0 , sizeof(board_info));
	memcpy(&board_info.type, name, I2C_NAME_SIZE);
	board_info.addr = i2c_addr;
	printk("excodec addr = 0x%x, excodec_name = %s\n", board_info.addr, board_info.type);
	attrs = i2c_new_subdev_board(adapter, &board_info, NULL);
	if (IS_ERR_OR_NULL(attrs)) {
		i2c_put_adapter(adapter);
		printk("Failed to acquire subdev %s, deferring probe\n",
				name);
		return -EINVAL;
	}

	if(attrs->detect){
		ret = attrs->detect(attrs);
		if(ret != AUDIO_SUCCESS){
			struct i2c_client *client;
			struct i2c_adapter *adapter = client->adapter;
			if (adapter)
				i2c_put_adapter(adapter);
			i2c_unregister_device(client);
			return -EINVAL;
		}
	}

	aic->excodec = attrs;
	aic->livingcodec = attrs;
	printk("Registered extern codec %s\n", name);
	return AUDIO_SUCCESS;
}

static int extern_codec_release(struct audio_aic_device *aic)
{
	struct codec_attributes * attrs = aic->excodec;
	struct i2c_client *client = get_codec_devdata(attrs);
	struct i2c_adapter *adapter = client->adapter;

	if (adapter)
		i2c_put_adapter(adapter);
	i2c_unregister_device(client);

	if(globe_aic->livingcodec == attrs)
		globe_aic->livingcodec = NULL;
	return AUDIO_SUCCESS;
}

int inner_codec_register(struct codec_attributes *codec_attr)
{
	struct proc_dir_entry *codec_dir = NULL;
	struct proc_dir_entry *codec_file = NULL;

	if(globe_aic){
		globe_aic->incodec = codec_attr;
		if(!globe_aic->livingcodec)
			globe_aic->livingcodec = codec_attr;
		/* register codec debug proc */
		if (NULL != codec_attr->debug_ops){
			codec_dir = jz_proc_mkdir("codecs");
			if (!codec_dir)
				return -ENOMEM;
			codec_file = proc_create("codec", 0, codec_dir,
						codec_attr->debug_ops);
			if (!codec_file) {
				printk("%s,%d: codec proc file create error!\n", __func__, __LINE__);
				return -1;
			}
			globe_aic->codec_dir = codec_dir;
			globe_aic->codec_file = codec_file;
		}
		return AUDIO_SUCCESS;
	}
	audio_warn_print("aic hasn't been inited!\n");
	return -EPERM;
}

int inner_codec_release(struct codec_attributes *codec_attr)
{
	if(globe_aic){
		globe_aic->incodec = NULL;
		if(globe_aic->livingcodec == codec_attr)
			globe_aic->livingcodec = NULL;
	}
	return AUDIO_SUCCESS;
}

/* dump aic controller registers */
static void dump_aic_regs(void)
{
	printk("T31 AIC registers list:\n");
	printk("AICFR: %04x\n" ,readl(globe_aic->i2s_iomem + 0));
	printk("AICCR: %04x\n" ,readl(globe_aic->i2s_iomem + 0x4));
	printk("I2SCR: %04x\n" ,readl(globe_aic->i2s_iomem + 0x10));
	printk("AICSR: %04x\n" ,readl(globe_aic->i2s_iomem + 0x14));
	printk("I2SSR: %04x\n" ,readl(globe_aic->i2s_iomem + 0x1c));
	printk("I2SDIV: %04x\n" ,readl(globe_aic->i2s_iomem + 0x30));
	printk("AICDR: %04x\n" ,readl(globe_aic->i2s_iomem + 0x34));
	printk("AICLR: %04x\n" ,readl(globe_aic->i2s_iomem + 0x38));
	printk("AICTFLR: %04x\n" ,readl(globe_aic->i2s_iomem + 0x3c));
}

/* debug audio aic info */
static int audio_aic_show(struct seq_file *m, void *v)
{
	int len = 0;
	struct codec_info *info = NULL;
	struct audio_aic_device *aic = (struct audio_aic_device*)(m->private);

	if (NULL == aic) {
		audio_warn_print("error, aic is null\n");
		return len;
	}

	seq_printf(m ,"\nThe version of audio driver is : %s\n", AUDIO_DRIVER_VERSION);

	info = aic->codec_mic_info;
	if (NULL == info) {
		audio_warn_print("error, codec mic info is null\n");
		return len;
	}
	seq_printf(m, "Record attr list : \n");
	seq_printf(m, "The living rate of record : %lu\n", info->rate);
	seq_printf(m, "The living channel of record : %d\n", info->channel);
	seq_printf(m, "The living format of record : %d\n", info->format);
	seq_printf(m, "The living datatype of record : %u\n", info->data_type.frame_vsize);
	seq_printf(m, "The living left again of record : %d\n", info->lagain);
	seq_printf(m, "The living right again of record : %d\n", info->ragain);
	seq_printf(m, "The living left dgain of record : %d\n", info->ldgain);
	seq_printf(m, "The living right dgain of record : %d\n", info->rdgain);
	seq_printf(m, "The living left mute of record : %s\n", info->lchannel_mute_en==0?"No":"Yes");
	seq_printf(m, "The living right mute of record : %s\n", info->rchannel_mute_en==0?"No":"Yes");
	seq_printf(m, "The living left alc max gain of record : %d\n", info->lalc_maxgain);
	seq_printf(m, "The living left alc min gain of record : %d\n", info->lalc_mingain);
	seq_printf(m, "The living right alc max gain of record : %d\n", info->ralc_maxgain);
	seq_printf(m, "The living right alc min gain of record : %d\n", info->ralc_mingain);

	info = aic->codec_spk_info;
	if (NULL == info) {
		audio_warn_print("error, codec spk info is null\n");
		return len;
	}
	seq_printf(m, "Replay attr list : \n");
	seq_printf(m, "The living rate of replay : %lu\n", info->rate);
	seq_printf(m, "The living channel of replay : %d\n", info->channel);
	seq_printf(m, "The living format of replay : %d\n", info->format);
	seq_printf(m, "The living datatype of replay : %u\n", info->data_type.frame_vsize);
	seq_printf(m, "The living left again of replay : %d\n", info->lagain);
	seq_printf(m, "The living right again of replay : %d\n", info->ragain);
	seq_printf(m, "The living left dgain of replay : %d\n", info->ldgain);
	seq_printf(m, "The living right dgain of replay : %d\n", info->rdgain);
	seq_printf(m, "The living left mute of record : %s\n", info->lchannel_mute_en==0?"No":"Yes");
	seq_printf(m, "The living right mute of record : %s\n", info->rchannel_mute_en==0?"No":"Yes");

	dump_aic_regs();

	return len;
}

static int dump_audio_aic_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, audio_aic_show, pde_data(inode), 2048);
}

static const struct proc_ops audio_aic_debug_fops = {
	.proc_read = seq_read,
	.proc_open = dump_audio_aic_open,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

/* The vendor flow leaves every AIC interrupt masked; this only runs if one is enabled. */
static irqreturn_t aic_irq_handler(int irq, void *dev_id)
{
	struct audio_aic_device *aic = dev_id;
	u32 sr = readl(aic->i2s_iomem + AICSR);

	if (!(sr & (I2S_TUR_MASK | I2S_ROR_MASK)))
		return IRQ_NONE;
	if (sr & I2S_TUR_MASK) {
		aic->underruns++;
		__i2s_clear_tur(aic);
	}
	if (sr & I2S_ROR_MASK) {
		aic->overruns++;
		__i2s_clear_ror(aic);
	}
	return IRQ_HANDLED;
}

static int init_pipe(struct audio_pipe *pipe, enum dma_data_direction direction, unsigned long iobase, int dma_type)
{
	if (NULL == pipe || (direction != DMA_FROM_DEVICE && direction != DMA_TO_DEVICE))
		return -1;

	pipe->dma_type = dma_type;
	pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;

	if (DMA_FROM_DEVICE == direction) {
		pipe->dma_config.direction = DMA_DEV_TO_MEM;
		pipe->dma_config.src_maxburst = AIC_RX_FIFO_DEPTH / 2;
		pipe->dma_config.dst_maxburst = AIC_RX_FIFO_DEPTH / 2;
		if (INGENIC_DMA_REQ_I2S0_TX == dma_type || INGENIC_DMA_REQ_I2S0_RX == dma_type)
			pipe->dma_config.src_addr = iobase + AICDR;
		else if (INGENIC_DMA_REQ_AEC == dma_type)
			pipe->dma_config.src_addr = iobase + AICLR;
		pipe->dma_config.dst_addr = 0;
	} else {
		pipe->dma_config.direction = DMA_MEM_TO_DEV;
		pipe->dma_config.src_maxburst = AIC_TX_FIFO_DEPTH / 2;
		pipe->dma_config.dst_maxburst = AIC_TX_FIFO_DEPTH / 2;
		pipe->dma_config.dst_addr = iobase + AICDR;
		pipe->dma_config.src_addr = 0;
	}

	spin_lock_init(&pipe->pipe_lock);

	return 0;
}

static int pipe_request_dma(struct audio_pipe *pipe, struct device *dev)
{
	int ret = 0;
	unsigned long lock_flags;
	const char *dma_name;
	enum dma_data_direction ddir =
		(pipe->dma_config.direction == DMA_DEV_TO_MEM) ?
		DMA_FROM_DEVICE : DMA_TO_DEVICE;

	switch (pipe->dma_type) {
	case INGENIC_DMA_REQ_I2S0_TX:
		dma_name = "tx";
		break;
	case INGENIC_DMA_REQ_I2S0_RX:
		dma_name = "rx";
		break;
	case INGENIC_DMA_REQ_AEC:
		dma_name = "aec";
		break;
	default:
		audio_err_print("unknown dma_type 0x%x\n", pipe->dma_type);
		return -EINVAL;
	}

	if (samplerate == 0)
		pipe->reservesize = PAGE_ALIGN(SND_DSP_DMA_BUFFER_SIZE);
	else
		pipe->reservesize = PAGE_ALIGN(samplerate/100*MAX_SAMPLE_BYTES*MAX_SAMPLE_CHANNEL*64);
	pipe->vaddr = dma_alloc_noncoherent(dev, pipe->reservesize, &pipe->paddr, ddir, GFP_KERNEL);
	if (NULL == pipe->vaddr) {
		audio_err_print("failed to alloc dma buffer.\n");
		return -ENOMEM;
	}

	spin_lock_irqsave(&pipe->pipe_lock, lock_flags);
	pipe->is_trans = false;
	spin_unlock_irqrestore(&pipe->pipe_lock, lock_flags);

	pipe->dma_chan = dma_request_chan(dev, dma_name);
	if (IS_ERR(pipe->dma_chan)) {
		ret = PTR_ERR(pipe->dma_chan);
		pipe->dma_chan = NULL;
		audio_err_print("dma channel request '%s' failed: %d\n", dma_name, ret);
		goto failed_request_channel;
	}
	dmaengine_slave_config(pipe->dma_chan, &pipe->dma_config);

	audio_info_print("init dev = %p, size = %d, pipe->vaddr = %p, pipe->paddr = 0x%08x.\n", dev, pipe->reservesize, pipe->vaddr, pipe->paddr);

	return 0;

failed_request_channel:
	dma_free_noncoherent(dev, pipe->reservesize, pipe->vaddr, pipe->paddr, ddir);
	pipe->vaddr = NULL;

	return ret;
}

static void pipe_release_one(struct audio_pipe *pipe, struct device *dev,
			      enum dma_data_direction ddir)
{
	unsigned long lock_flags;

	if (!pipe || !pipe->dma_chan)
		return;

	spin_lock_irqsave(&pipe->pipe_lock, lock_flags);
	if (pipe->is_trans) {
		dmaengine_terminate_all(pipe->dma_chan);
		pipe->is_trans = false;
	}
	spin_unlock_irqrestore(&pipe->pipe_lock, lock_flags);

	if (pipe->vaddr)
		dma_free_noncoherent(dev, pipe->reservesize, pipe->vaddr, pipe->paddr, ddir);
	pipe->vaddr = NULL;
	dma_release_channel(pipe->dma_chan);
	pipe->dma_chan = NULL;
}

static void pipe_release_dma(struct audio_aic_device *aic, struct device *dev)
{
	pipe_release_one(aic->mic_pipe, dev, DMA_FROM_DEVICE);
	pipe_release_one(aic->spk_pipe, dev, DMA_TO_DEVICE);
	pipe_release_one(aic->aec_pipe, dev, DMA_FROM_DEVICE);
}

static void aic_free_dma(struct audio_aic_device *aic)
{
	pipe_release_dma(aic, aic->dev);
	kfree(aic->mic_pipe);
	kfree(aic->spk_pipe);
	kfree(aic->aec_pipe);
	kfree(aic->codec_mic_info);
	kfree(aic->codec_spk_info);
	kfree(aic->codec_aec_info);
	aic->mic_pipe = aic->spk_pipe = aic->aec_pipe = NULL;
	aic->codec_mic_info = aic->codec_spk_info = aic->codec_aec_info = NULL;
}

/* mic operations */
static int mic_init(void *pipe)
{
	struct audio_aic_device *aic = NULL;
	struct codec_info *cur_codec = NULL;
	struct audio_route *route = NULL;

	if (pipe == NULL) {
		audio_err_print("route is NULL.\n");
		return -1;
	}

	route = (struct audio_route*)pipe;
	aic = (struct audio_aic_device*)route->pipe->priv;
	cur_codec = aic->codec_mic_info;

	if (NULL == aic->livingcodec) {
		audio_err_print("livingcodec is NULL.\n");
		return -1;
	}

	//init parameters
	cur_codec->rate = DEFAULT_SAMPLERATE;
	/*
	 * The value of format reference are:
	 * AFMT_U8     0x00000001
	 * AFMT_S8     0x00000002
	 * AFMT_S16LE  0x00000010
	 * AFMT_S16BE  0x00000011
	 * */
	cur_codec->format = AFMT_0;
	cur_codec->channel = DEFAULT_CHANNEL;
	cur_codec->transfer_info.data = DATA_I2S_MODE;
	cur_codec->transfer_info.seq = LEFT_FIRST_MODE;
	cur_codec->transfer_info.sync = LEFT_LOW_RIGHT_HIGH;
	cur_codec->transfer_info.rate2mclk = MCLK_DIV_TO_SAMPLE;
	cur_codec->lagain = DEFAULT_AGAIN;
	cur_codec->ragain = DEFAULT_AGAIN;
	cur_codec->ldgain = DEFAULT_DGAIN;
	cur_codec->rdgain = DEFAULT_DGAIN;
	cur_codec->alc_en_l = DEFAULT_ALC_EN;
	cur_codec->alc_en_r = DEFAULT_ALC_EN;
	cur_codec->lalc_maxgain = DEFAULT_ALC_MAXGAIN;
	cur_codec->lalc_mingain = DEFAULT_ALC_MINGAIN;
	cur_codec->ralc_maxgain = DEFAULT_ALC_MAXGAIN;
	cur_codec->ralc_mingain = DEFAULT_ALC_MINGAIN;
	cur_codec->lchannel_mute_en = MUTE_DISEN;
	cur_codec->rchannel_mute_en = MUTE_DISEN;
	cur_codec->trigger = DEFAULT_RECORD_TRIGGER;
	cur_codec->data_type.frame_size = DEFAULT_FRAME_SIZE;
	cur_codec->data_type.frame_vsize = DEFAULT_VFRAME_SIZE;
	cur_codec->data_type.sample_rate = DEFAULT_SAMPLERATE;
	cur_codec->data_type.sample_channel = DEFAULT_CHANNEL;

	return 0;
}

static int set_codec_mic_datatype(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct audio_parameter *param = (struct audio_parameter*)data;
	struct audio_data_type data_type;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	data_type.sample_rate = param->rate;
	if(param->channel == 2){
		data_type.sample_channel = STEREO;
	}else{
		if(1 == mic_mono_channel)
			data_type.sample_channel = MONO_LEFT;
		else if (2 == mic_mono_channel)
			data_type.sample_channel = MONO_RIGHT;
	}
	data_type.frame_vsize = param->format;
	data_type.frame_size = 32;

	if (data_type.sample_channel != MONO_LEFT && data_type.sample_channel != MONO_RIGHT &&
		data_type.sample_channel != STEREO) {
		audio_err_print("channel error.\n");
		return -EINVAL;
	}
	//set samplerate
	if (CODEC_IS_0_LINES == aic->livingcodec->pins || CODEC_IS_6_LINES == aic->livingcodec->pins) {
		clk_set_rate(aic->mic_clock, data_type.sample_rate*256);
	} else if (CODEC_IS_4_LINES == aic->livingcodec->pins) {
		clk_set_rate(aic->spk_clock, data_type.sample_rate*256);
	}
	aic->codec_mic_info->rate = data_type.sample_rate;
	audio_info_print("audio mic clock rate: %d\n", data_type.sample_rate*256);

	//set channel
	if (MONO_RIGHT == data_type.sample_channel || MONO_LEFT == data_type.sample_channel) {
		if (mic_mono_channel == 2)
			__i2s_enable_monoctr_right(aic);
		else if (mic_mono_channel == 1)
			__i2s_enable_monoctr_left(aic);
	}else{
		__i2s_enable_stereo(aic);
	}

	aic->codec_mic_info->channel = data_type.sample_channel;

	//set format
	if (VALID_8BIT == data_type.frame_vsize) {
		__i2s_set_iss_sample_size(aic, 0);
		aic->mic_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
		aic->mic_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	} else if (VALID_16BIT == data_type.frame_vsize) {
		__i2s_set_iss_sample_size(aic, 1);
		aic->mic_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
		aic->mic_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	} else if (VALID_20BIT == data_type.frame_vsize) {
		__i2s_set_iss_sample_size(aic, 3);
		aic->mic_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		aic->mic_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	} else if (VALID_24BIT == data_type.frame_vsize){
		__i2s_set_iss_sample_size(aic, 4);
		aic->mic_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		aic->mic_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		aic->aec_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		aic->aec_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	}
	__i2s_disable_signadj(aic);
	//set trigger? default value is 8
	//__i2s_set_receive_trigger(aic, trigger_value);
	aic->codec_mic_info->format = data_type.frame_vsize;

	//codec set data_type
	ret = aic->livingcodec->record->set_datatype(&data_type);

	aic->codec_mic_info->data_type = data_type;

	return ret;
}

static int __maybe_unused set_codec_mic_protocol(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct codec_sign_configure *protocol = (struct codec_sign_configure*)data;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	if (DATA_I2S_MODE == protocol->data)
		__i2s_select_i2s(aic);
	else if (DATA_RIGHT_JUSTIFIED_MODE == protocol->data || DATA_LEFT_JUSTIFIED_MODE == protocol->data)
		__i2s_select_msbjustified(aic);

	ret = aic->livingcodec->set_sign(protocol);

	aic->codec_mic_info->transfer_info = *protocol;

	return ret;
}

static int set_codec_mic_hpf_enable(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	int hp_en = *(int*)data;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	if (NULL != aic->livingcodec->record->set_mic_hpf_en)
		ret = aic->livingcodec->record->set_mic_hpf_en(hp_en);

	return ret;
}

static int set_codec_mic_again(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct volume again = *(struct volume*)data;
	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}
	if(again.channel == 2){
		again.channel = STEREO;
	}else{
		if(1 == mic_mono_channel)
			again.channel = MONO_LEFT;
		else if (2 == mic_mono_channel)
			again.channel = MONO_RIGHT;
	}

	aic->livingcodec->record->set_again(again.channel,again.gain[0]);

	if (again.channel == MONO_LEFT){
		aic->codec_mic_info->alc_en_l = 0;
		aic->codec_mic_info->lagain = again.gain[0];
	}else if (again.channel == MONO_RIGHT){
		aic->codec_mic_info->alc_en_r = 0;
		aic->codec_mic_info->ragain = again.gain[1];
	}else {
		aic->codec_mic_info->alc_en_l = 0;
		aic->codec_mic_info->lagain = again.gain[0];
		aic->codec_mic_info->alc_en_r = 0;
		aic->codec_mic_info->ragain = again.gain[1];
	}

	return ret;
}

static int set_codec_mic_dgain(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct volume dgain = *(struct volume*)data;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	ret = aic->livingcodec->record->set_dgain(dgain.channel,dgain.gain[0]);

	if (dgain.channel == MONO_LEFT)
		aic->codec_mic_info->ldgain = dgain.gain[0];
	else if (dgain.channel == MONO_RIGHT)
		aic->codec_mic_info->rdgain = dgain.gain[1];

	return ret;
}

static int set_codec_mic_alc_gain(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct alc_gain alc = *(struct alc_gain*)data;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	if (alc.channel != MONO_LEFT && alc.channel != MONO_RIGHT) {
		audio_err_print("mute channel param is invalid %ld.\n", alc.channel);
		return -1;
	}

	if (aic->livingcodec->record->set_alc_threshold)
		aic->livingcodec->record->set_alc_threshold(alc.channel,alc.max_gain[0],alc.min_gain[0]);

	if (alc.channel == MONO_LEFT) {
		aic->codec_mic_info->alc_en_l = 1;
		aic->codec_mic_info->lalc_maxgain = alc.max_gain[0];
		aic->codec_mic_info->lalc_mingain = alc.min_gain[0];
	} else if (alc.channel == MONO_RIGHT) {
		aic->codec_mic_info->alc_en_r = 1;
		aic->codec_mic_info->ralc_maxgain = alc.max_gain[1];
		aic->codec_mic_info->ralc_mingain = alc.min_gain[1];
	}

	return ret;
}

static int set_codec_mic_mute(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	int channel = *(int *)data;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	if (channel != MONO_LEFT && channel != MONO_RIGHT) {
		audio_err_print("mute channel param is invalid %ld.\n", channel);
		return -1;
	}
	if(channel == MONO_LEFT){
		if(aic->codec_mic_info->lchannel_mute_en){
			ret = aic->livingcodec->record->set_dgain(channel,aic->codec_mic_info->ldgain);
			aic->codec_mic_info->lchannel_mute_en = 0;
		}else{
			if (aic->livingcodec->record->set_mute)
				ret = aic->livingcodec->record->set_mute(channel);
			aic->codec_mic_info->lchannel_mute_en = 1;
		}
	}else{
		if(aic->codec_mic_info->rchannel_mute_en){
			ret = aic->livingcodec->record->set_dgain(channel,aic->codec_mic_info->rdgain);
			aic->codec_mic_info->rchannel_mute_en = 0;
		}else{
			if (aic->livingcodec->record->set_mute)
				ret = aic->livingcodec->record->set_mute(channel);
			aic->codec_mic_info->rchannel_mute_en = 1;
		}
	}

	return ret;
}

#ifndef CPU_MODE_DEBUG
static int set_codec_mic_start(struct audio_aic_device *aic, void *data)
{
	int ret = 0, val;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	__i2s_enable(aic);
	ret = aic->livingcodec->record->set_endpoint(CODEC_MIC_START);

	{
		__i2s_flush_rfifo(aic);
		val = __i2s_read_rfifo(aic);
		__i2s_enable_receive_dma(aic);
		__i2s_enable_record(aic);
		__i2s_enable_tloop(aic);
		__i2s_enable_etfl(aic);
	}

	return ret;
}
#else
#define FILE_NAME "/tmp/record.pcm"
int frm_cnt = 16*1024*10;
static int set_codec_mic_start(struct audio_aic_device *aic, void *data)
{
	int ret = 0, val;
	struct file *fp = NULL;
	mm_segment_t old_fs;
	loff_t *pos;
	int cnt = 0;
	short *pdata = kmalloc(frm_cnt * 2,GFP_KERNEL);
	if(!pdata)
		return -1;

	fp = filp_open(FILE_NAME, O_CREAT | O_WRONLY | O_TRUNC, 00766);

	if (fp == NULL){
		printk("file: %s open failed...\n",FILE_NAME);
	}

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	__i2s_enable(aic);
	ret = aic->livingcodec->record->set_endpoint(CODEC_MIC_START);

	{
		__i2s_flush_rfifo(aic);
		val = __i2s_read_rfifo(aic);
		//__i2s_enable_receive_dma(aic);
		__i2s_enable_record(aic);
		__i2s_enable_tloop(aic);
		__i2s_enable_etfl(aic);
	}

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	pos = &(fp->f_pos);
	while(1){
		if(__i2s_test_rfl(aic) == 0)
			continue;
		pdata[cnt] = __i2s_read_rfifo(aic);
		cnt++;
		if(cnt >= frm_cnt)
			break;
	}

	vfs_write(fp, pdata, frm_cnt * 2, pos);
	filp_close(fp, NULL);
	kfree(pdata);
	return -1;
}

#endif

static int set_codec_mic_stop(struct audio_aic_device *aic, void *data)
{
	int ret = 0;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	//disable aic record bit
	__i2s_disable_record(aic);
	__i2s_disable_receive_dma(aic);
	__i2s_disable_tloop(aic);
	__i2s_disable_etfl(aic);
	__i2s_enable_stereo(aic);

	//mic stop
	ret = aic->livingcodec->record->set_endpoint(CODEC_MIC_STOP);

	return ret;
}

static int mic_ioctl(void *pipe, unsigned int cmd, void *data)
{
	struct audio_aic_device *aic = NULL;
	struct codec_info *cur_codec = NULL;
	struct audio_pipe *mic_pipe = NULL;
	int ret = 0;

	if (pipe == NULL) {
		audio_err_print("pipe is NULL.\n");
		return -1;
	}

	mic_pipe = (struct audio_pipe *)pipe;
	aic = (struct audio_aic_device*)mic_pipe->priv;;
	cur_codec = aic->codec_mic_info;

	if (NULL == aic || NULL == cur_codec) {
		audio_err_print("aic is null or cur_codec is null.\n");
		return -1;
	}

	switch(cmd) {
		case AUDIO_CMD_CONFIG_PARAM:
			ret = set_codec_mic_datatype(aic, data);
			break;

		case AUDIO_CMD_GET_GAIN:
			{
				struct volume *vol= (struct volume*)data;
				if (vol->channel == MONO_LEFT)
					vol->gain[0] = cur_codec->lagain;
				else if (vol->channel == MONO_RIGHT)
					vol->gain[1]= cur_codec->ragain;
				break;
			}

		case AUDIO_CMD_GET_VOLUME:
			{
				struct volume *vol= (struct volume*)data;
				if (vol->channel == MONO_LEFT)
					vol->gain[0] = cur_codec->ldgain;
				else if (vol->channel == MONO_RIGHT)
					vol->gain[1] = cur_codec->rdgain;
				break;
			}

		case AUDIO_CMD_GET_ALC_GAIN:
			{
				struct alc_gain *vol= (struct alc_gain*)data;
				if (vol->channel == MONO_LEFT) {
					vol->max_gain[0] = cur_codec->lalc_maxgain;
					vol->min_gain[0] = cur_codec->lalc_mingain;
				} else if (vol->channel == MONO_RIGHT) {
					vol->max_gain[1] = cur_codec->ralc_maxgain;
					vol->min_gain[1] = cur_codec->ralc_mingain;
				}
				break;
			}

		case AUDIO_CMD_SET_MIC_HPF_EN:
			ret = set_codec_mic_hpf_enable(aic, data);
			break;

		case AUDIO_CMD_SET_GAIN:
			ret = set_codec_mic_again(aic, data);
			break;

		case AUDIO_CMD_SET_VOLUME:
			ret = set_codec_mic_dgain(aic, data);
			break;

		case AUDIO_CMD_SET_ALC_GAIN:
			ret = set_codec_mic_alc_gain(aic, data);
			break;

		case AUDIO_CMD_SET_MUTE:
			ret = set_codec_mic_mute(aic, data);
			break;

		case AUDIO_CMD_ENABLE_STREAM:
			ret = set_codec_mic_start(aic, data);
			break;

		case AUDIO_CMD_DISABLE_STREAM:
			ret = set_codec_mic_stop(aic, data);
			break;
		default:
			break;
	}


	return ret;
}

static int mic_deinit(void *pipe)
{
	struct audio_aic_device *aic = NULL;
	struct codec_info *cur_codec = NULL;
	struct audio_route *route = NULL;

	if (pipe == NULL) {
		audio_err_print("pipe is NULL.\n");
		return -1;
	}

	route = (struct audio_route*)pipe;
	aic = (struct audio_aic_device*)route->pipe->priv;
	cur_codec = aic->codec_mic_info;

	if (NULL == aic->livingcodec) {
		audio_err_print("error, livingcodec is NULL.\n");
		return -1;
	}
	if (NULL == cur_codec) {
		audio_err_print("error, codec mic info is null\n");
		return -1;
	}
	cur_codec->rate = DEFAULT_SAMPLERATE;
	cur_codec->format = AFMT_0;
	cur_codec->channel = DEFAULT_CHANNEL;
	cur_codec->transfer_info.data = DATA_I2S_MODE;
	cur_codec->transfer_info.seq = LEFT_FIRST_MODE;
	cur_codec->transfer_info.sync = LEFT_LOW_RIGHT_HIGH;
	cur_codec->transfer_info.rate2mclk = MCLK_DIV_TO_SAMPLE;
	cur_codec->lagain = DEFAULT_AGAIN;
	cur_codec->ragain = DEFAULT_AGAIN;
	cur_codec->ldgain = DEFAULT_DGAIN;
	cur_codec->rdgain = DEFAULT_DGAIN;
	cur_codec->alc_en_l = DEFAULT_ALC_EN;
	cur_codec->alc_en_r = DEFAULT_ALC_EN;
	cur_codec->lalc_maxgain = DEFAULT_ALC_MAXGAIN;
	cur_codec->lalc_mingain = DEFAULT_ALC_MINGAIN;
	cur_codec->ralc_maxgain = DEFAULT_ALC_MAXGAIN;
	cur_codec->ralc_mingain = DEFAULT_ALC_MINGAIN;
	cur_codec->lchannel_mute_en = MUTE_DISEN;
	cur_codec->rchannel_mute_en = MUTE_DISEN;
	cur_codec->trigger = DEFAULT_RECORD_TRIGGER;
	cur_codec->data_type.frame_size = DEFAULT_FRAME_SIZE;
	cur_codec->data_type.frame_vsize = DEFAULT_VFRAME_SIZE;
	cur_codec->data_type.sample_rate = DEFAULT_SAMPLERATE;
	cur_codec->data_type.sample_channel = DEFAULT_CHANNEL;

	return 0;
}

/* spk operations */
static int spk_init(void *pipe)
{
	struct audio_aic_device *aic = NULL;
	struct codec_info *cur_codec = NULL;
	struct audio_route *route = NULL;

	if (pipe == NULL) {
		audio_err_print("pipe is NULL.\n");
		return -1;
	}

	route = (struct audio_route*)pipe;
	aic = (struct audio_aic_device*)route->pipe->priv;
	cur_codec = aic->codec_spk_info;

	if (NULL == aic->livingcodec) {
		audio_err_print("livingcodec is NULL.\n");
		return -1;
	}

	//init parameters
	cur_codec->rate = DEFAULT_SAMPLERATE;
	/*
	 * The value of format reference are:
	 * AFMT_U8     0x00000001
	 * AFMT_S8     0x00000002
	 * AFMT_S16LE  0x00000010
	 * AFMT_S16BE  0x00000011
	 * */
	cur_codec->format = AFMT_0;
	cur_codec->channel = DEFAULT_CHANNEL;
	cur_codec->transfer_info.data = DATA_I2S_MODE;
	cur_codec->transfer_info.seq = LEFT_FIRST_MODE;
	cur_codec->transfer_info.sync = LEFT_LOW_RIGHT_HIGH;
	cur_codec->transfer_info.rate2mclk = MCLK_DIV_TO_SAMPLE;
	cur_codec->lagain = DEFAULT_AGAIN;
	cur_codec->ragain = DEFAULT_AGAIN;
	cur_codec->ldgain = DEFAULT_DGAIN;
	cur_codec->rdgain = DEFAULT_DGAIN;
	cur_codec->lchannel_mute_en = MUTE_DISEN;
	cur_codec->rchannel_mute_en = MUTE_DISEN;
	cur_codec->trigger = DEFAULT_REPLAY_TRIGGER;
	cur_codec->data_type.frame_size = DEFAULT_FRAME_SIZE;
	cur_codec->data_type.frame_vsize = DEFAULT_VFRAME_SIZE;
	cur_codec->data_type.sample_rate = DEFAULT_SAMPLERATE;
	cur_codec->data_type.sample_channel = DEFAULT_CHANNEL;

	return 0;
}

static int set_codec_spk_datatype(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct audio_parameter *param = (struct audio_parameter*)data;
	struct audio_data_type data_type;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}
	data_type.sample_rate = param->rate;
	if(param->channel == 2){
		data_type.sample_channel = STEREO;
	}else{
		data_type.sample_channel = MONO_LEFT;
	}
	data_type.frame_vsize = param->format;
	data_type.frame_size = 32;
	if (data_type.sample_channel != MONO_LEFT && data_type.sample_channel != STEREO) {
		audio_err_print("spk only support stereo or mono_left channel error.\n");
		return -EINVAL;
	}
	//set samplerate
	clk_set_rate(aic->spk_clock, data_type.sample_rate*256);
	clk_enable(aic->spk_clock);

	aic->codec_spk_info->rate = data_type.sample_rate;
	audio_info_print("audio spk clock rate: %d\n", data_type.sample_rate*256);

	//set channel
	if(STEREO == data_type.sample_channel){
		__i2s_out_channel_select(aic, 0x01);//two channel
	}else{
		__i2s_out_channel_select(aic, 0x00);//one channel,only left
		__i2s_enable_mono2stereo(aic);
	}
	aic->codec_spk_info->channel = data_type.sample_channel;

	//set format
	if (VALID_8BIT == data_type.frame_vsize) {
		__i2s_set_oss_sample_size(aic, 0);
		aic->spk_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
		aic->spk_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	} else if (VALID_16BIT == data_type.frame_vsize) {
		__i2s_set_oss_sample_size(aic, 1);
		aic->spk_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
		aic->spk_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	} else if (VALID_20BIT == data_type.frame_vsize) {
		__i2s_set_oss_sample_size(aic, 3);
		aic->spk_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		aic->spk_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	} else if (VALID_24BIT == data_type.frame_vsize){
		__i2s_set_oss_sample_size(aic, 4);
		aic->spk_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		aic->spk_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	}

	__i2s_disable_signadj(aic);
	__i2s_disable_byteswap(aic);
	//set trigger? default value is 8
	//__i2s_set_receive_trigger(aic, trigger_value);
	aic->codec_spk_info->format = data_type.frame_vsize;

	//codec set data_type
	ret = aic->livingcodec->playback->set_datatype(&data_type);

	aic->codec_spk_info->data_type = data_type;

	return ret;
}

static int __maybe_unused set_codec_spk_protocol(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct codec_sign_configure *protocol = (struct codec_sign_configure*)data;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	if (DATA_I2S_MODE == protocol->data)
		__i2s_select_i2s(aic);
	else if (DATA_RIGHT_JUSTIFIED_MODE == protocol->data || DATA_LEFT_JUSTIFIED_MODE == protocol->data)
		__i2s_select_msbjustified(aic);

	ret = aic->livingcodec->set_sign(protocol);

	aic->codec_spk_info->transfer_info = *protocol;

	return ret;
}

static int set_codec_spk_again(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct volume again = *(struct volume*)data;
	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}
	if(again.channel == 2){
		again.channel = STEREO;
	}else{
		if(1 == mic_mono_channel)
			again.channel = MONO_LEFT;
		else if (2 == mic_mono_channel)
			again.channel = MONO_RIGHT;
	}
	ret = aic->livingcodec->playback->set_again(again.channel,again.gain[0]);

	if (again.channel == MONO_LEFT)
		aic->codec_spk_info->lagain = again.gain[0];
	else if (again.channel == MONO_RIGHT)
		aic->codec_spk_info->ragain = again.gain[1];
	else {
		aic->codec_spk_info->lagain = again.gain[0];
		aic->codec_spk_info->ragain = again.gain[1];
	}
	return ret;
}

static int set_codec_spk_dgain(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct volume dgain = *(struct volume*)data;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	ret = aic->livingcodec->playback->set_dgain(dgain.channel,dgain.gain[0]);

	if (dgain.channel == MONO_LEFT)
		aic->codec_spk_info->ldgain = dgain.gain[0];
	else if (dgain.channel == MONO_RIGHT)
		aic->codec_spk_info->rdgain = dgain.gain[1];

	return ret;
}

static int set_codec_spk_mute(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	int channel = *(int *)data;
	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	if (channel != MONO_LEFT && channel != MONO_RIGHT) {
		audio_err_print("mute channel param is invalid %ld.\n", channel);
		return -1;
	}
	if(channel == MONO_LEFT){
		if(aic->codec_spk_info->lchannel_mute_en){
			ret = aic->livingcodec->playback->set_dgain(channel,aic->codec_spk_info->ldgain);
			aic->codec_spk_info->lchannel_mute_en = 0;
		}else{
			if (aic->livingcodec->playback->set_mute)
				ret = aic->livingcodec->playback->set_mute(channel);
			aic->codec_spk_info->lchannel_mute_en = 1;
		}
	}else{
		if(aic->codec_spk_info->rchannel_mute_en){
			ret = aic->livingcodec->playback->set_dgain(channel,aic->codec_spk_info->rdgain);
			aic->codec_spk_info->rchannel_mute_en = 0;
		}else{
			if (aic->livingcodec->playback->set_mute)
				ret = aic->livingcodec->playback->set_mute(channel);
			aic->codec_spk_info->rchannel_mute_en = 1;
		}
	}

	return ret;
}

static int set_codec_spk_start(struct audio_aic_device *aic, void *data)
{
	int ret = 0;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	__i2s_enable(aic);
	ret = aic->livingcodec->playback->set_endpoint(CODEC_SPK_START);
	{
		__i2s_flush_tfifo(aic);
		__i2s_enable_transmit_dma(aic);
		__i2s_enable_replay(aic);
	}

	return ret;
}


static int set_codec_spk_stop(struct audio_aic_device *aic, void *data)
{
	int ret = 0;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	__i2s_disable_transmit_dma(aic);
	__i2s_disable_replay(aic);

	{
		//avoid pop
		__i2s_write_tfifo(aic, 0);
		__i2s_write_tfifo(aic, 0);
		__i2s_write_tfifo(aic, 0);
		__i2s_write_tfifo(aic, 0);
		__i2s_enable_replay(aic);
		msleep(2);
		__i2s_disable_replay(aic);
	}

	ret = aic->livingcodec->playback->set_endpoint(CODEC_SPK_STOP);

	return ret;
}

static int spk_ioctl(void *pipe, unsigned int cmd, void *data)
{
	struct audio_aic_device *aic = NULL;
	struct codec_info *cur_codec = NULL;
	struct audio_pipe *spk_pipe = NULL;
	int ret = 0;

	if (pipe == NULL) {
		audio_err_print("pipe is NULL.\n");
		return -1;
	}

	spk_pipe = (struct audio_pipe *)pipe;
	aic = (struct audio_aic_device*)spk_pipe->priv;;
	cur_codec = aic->codec_spk_info;

	if (NULL == aic || NULL == cur_codec) {
		audio_err_print("aic is null or cur_codec is null.\n");
		return -1;
	}

	/*set parameters*/
	switch(cmd) {
		case AUDIO_CMD_CONFIG_PARAM:
			ret = set_codec_spk_datatype(aic, data);
			break;

		case AUDIO_CMD_GET_GAIN:
			{
				struct volume *vol= (struct volume*)data;
				if (vol->channel == MONO_LEFT)
					vol->gain[0] = cur_codec->lagain;
				else if (vol->channel == MONO_RIGHT)
					vol->gain[1] = cur_codec->ragain;
				break;
			}

		case AUDIO_CMD_GET_VOLUME:
			{
				struct volume *vol= (struct volume*)data;
				if (vol->channel == MONO_LEFT)
					vol->gain[0] = cur_codec->ldgain;
				else if (vol->channel == MONO_RIGHT)
					vol->gain[1] = cur_codec->rdgain;
				break;
			}

		case AUDIO_CMD_SET_GAIN:
			ret = set_codec_spk_again(aic, data);
			break;

		case AUDIO_CMD_SET_VOLUME:
			ret = set_codec_spk_dgain(aic, data);
			break;

		case AUDIO_CMD_SET_MUTE:
			ret = set_codec_spk_mute(aic, data);
			break;

		case AUDIO_CMD_ENABLE_STREAM:
			ret = set_codec_spk_start(aic, data);
			break;

		case AUDIO_CMD_DISABLE_STREAM:
			ret = set_codec_spk_stop(aic, data);
			break;


	default:
		break;
	}

	return ret;
}

static int spk_deinit(void *pipe)
{
	struct audio_aic_device *aic = NULL;
	struct codec_info *cur_codec = NULL;
	struct audio_route *route = NULL;

	if (pipe == NULL) {
		audio_err_print("pipe is NULL.\n");
		return -1;
	}

	route = (struct audio_route*)pipe;
	aic = (struct audio_aic_device*)route->pipe->priv;
	cur_codec = aic->codec_spk_info;

	if (NULL == aic->livingcodec) {
		audio_err_print("livingcodec is NULL.\n");
		return -1;
	}
	if (NULL == cur_codec) {
		audio_err_print("error, codec mic info is null\n");
		return -1;
	}
	cur_codec->rate = DEFAULT_SAMPLERATE;
	cur_codec->format = AFMT_0;
	cur_codec->channel = DEFAULT_CHANNEL;
	cur_codec->transfer_info.data = DATA_I2S_MODE;
	cur_codec->transfer_info.seq = LEFT_FIRST_MODE;
	cur_codec->transfer_info.sync = LEFT_LOW_RIGHT_HIGH;
	cur_codec->transfer_info.rate2mclk = MCLK_DIV_TO_SAMPLE;
	cur_codec->lagain = DEFAULT_AGAIN;
	cur_codec->ragain = DEFAULT_AGAIN;
	cur_codec->ldgain = DEFAULT_DGAIN;
	cur_codec->rdgain = DEFAULT_DGAIN;
	cur_codec->lchannel_mute_en = MUTE_DISEN;
	cur_codec->rchannel_mute_en = MUTE_DISEN;
	cur_codec->trigger = DEFAULT_RECORD_TRIGGER;
	cur_codec->data_type.frame_size = DEFAULT_FRAME_SIZE;
	cur_codec->data_type.frame_vsize = DEFAULT_VFRAME_SIZE;
	cur_codec->data_type.sample_rate = DEFAULT_SAMPLERATE;
	cur_codec->data_type.sample_channel = DEFAULT_CHANNEL;

	return 0;
}

/* aec operations */
static int aec_init(void *pipe)
{
	struct audio_aic_device *aic = NULL;
	struct codec_info *cur_codec = NULL;
	struct audio_route *route = NULL;

	if (pipe == NULL) {
		audio_err_print("pipe is NULL.\n");
		return -1;
	}

	route = (struct audio_route*)pipe;
	aic = (struct audio_aic_device*)route->pipe->priv;
	cur_codec = aic->codec_aec_info;

	if (NULL == aic->livingcodec) {
		audio_err_print("livingcodec is NULL.\n");
		return -1;
	}

	//init parameters
	cur_codec->rate = DEFAULT_SAMPLERATE;
	/*
	 * The value of format reference are:
	 * AFMT_U8     0x00000001
	 * AFMT_S8     0x00000002
	 * AFMT_S16LE  0x00000010
	 * AFMT_S16BE  0x00000011
	 * */
	cur_codec->format = AFMT_0;
	cur_codec->channel = DEFAULT_CHANNEL;
	cur_codec->transfer_info.data = DATA_I2S_MODE;
	cur_codec->transfer_info.seq = LEFT_FIRST_MODE;
	cur_codec->transfer_info.sync = LEFT_LOW_RIGHT_HIGH;
	cur_codec->transfer_info.rate2mclk = MCLK_DIV_TO_SAMPLE;
	cur_codec->lagain = DEFAULT_AGAIN;
	cur_codec->ragain = DEFAULT_AGAIN;
	cur_codec->ldgain = DEFAULT_DGAIN;
	cur_codec->rdgain = DEFAULT_DGAIN;
	cur_codec->trigger = DEFAULT_AEC_TRIGGER;
	cur_codec->data_type.frame_size = DEFAULT_FRAME_SIZE;
	cur_codec->data_type.frame_vsize = DEFAULT_VFRAME_SIZE;
	cur_codec->data_type.sample_rate = DEFAULT_SAMPLERATE;
	cur_codec->data_type.sample_channel = DEFAULT_CHANNEL;

	return 0;
}

static int set_codec_aec_datatype(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct audio_parameter *param = (struct audio_parameter*)data;
	struct audio_data_type data_type;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

	data_type.sample_rate = param->rate;
	data_type.sample_channel = param->channel;
	data_type.frame_vsize = param->format;
	data_type.frame_size = 32;
	if (data_type.sample_channel != MONO_LEFT) {
		audio_info_print("We only support replay mono channel now.\n");
		//return -EINVAL;
	}

	//set samplerate
/*	clk_set_rate(aic->spk_clock, data_type.sample_rate*256);
	clk_enable(aic->spk_clock);
	aic->codec_spk_info->rate = data_type.sample_rate;

	//set channel
	__i2s_out_channel_select(aic, MONO_LEFT-1);
	__i2s_enable_mono2stereo(aic);
	aic->codec_spk_info->channel = data_type.sample_channel;

	//set format
	if (VALID_8BIT == data_type.frame_vsize) {
		__i2s_set_oss_sample_size(aic, 0);
		aic->spk_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
		aic->spk_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_1_BYTE;
	} else if (VALID_16BIT == data_type.frame_vsize) {
		__i2s_set_oss_sample_size(aic, 1);
		aic->spk_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
		aic->spk_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	} else if (VALID_20BIT == data_type.frame_vsize) {
		__i2s_set_oss_sample_size(aic, 3);
		aic->spk_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		aic->spk_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	} else if (VALID_24BIT == data_type.frame_vsize){
		__i2s_set_oss_sample_size(aic, 4);
		aic->spk_pipe->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		aic->spk_pipe->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	}
	__i2s_disable_byteswap(aic);*/
	//set trigger? default value is 8
	//__i2s_set_receive_trigger(aic, trigger_value);
	aic->codec_aec_info->format = data_type.frame_vsize;

	//codec set data_type
//	ret = aic->livingcodec->aec.set_datatype(&data_type);

	aic->codec_aec_info->data_type = data_type;

	return ret;
}

static int __maybe_unused set_codec_aec_protocol(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct codec_sign_configure *protocol = (struct codec_sign_configure*)data;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

/*	if (DATA_I2S_MODE == protocol->data)
//		__aic_select_i2s(aic);
		__i2s_select_i2s(aic);
	else if (DATA_RIGHT_JUSTIFIED_MODE == protocol->data || DATA_LEFT_JUSTIFIED_MODE == protocol->data)
		__i2s_select_msbjustified(aic);

	ret = aic->livingcodec->set_sign(protocol);
*/
	aic->codec_spk_info->transfer_info = *protocol;

	return ret;
}

static int set_codec_aec_again(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct volume gain = *(struct volume*)data;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

//	ret = aic->livingcodec->playback.set_again(&again);

	aic->codec_aec_info->lagain = gain.gain[0];

	return ret;
}

static int set_codec_aec_dgain(struct audio_aic_device *aic, void *data)
{
	int ret = 0;
	struct volume dgain = *(struct volume*)data;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

//	ret = aic->livingcodec->playback.set_dgain(dgain);

	aic->codec_aec_info->ldgain = dgain.gain[0];

	return ret;
}

static int set_codec_aec_start(struct audio_aic_device *aic, void *data)
{
	int ret = 0;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

/*	//enable aic global bit
	__i2s_enable(aic);
//	ret = aic->livingcodec->playback.set_endpoint(CODEC_SPK_START);

	{
		__i2s_flush_rfifo(aic);
		__i2s_read_rfifo(aic);
//		__i2s_enable_monoctr_right(aic);
		__i2s_enable_receive_dma(aic);
		__i2s_enable_record(aic);
		__i2s_enable_tloop(aic);
		__i2s_enable_etfl(aic);
	}*/

	return ret;
}

static int set_codec_aec_stop(struct audio_aic_device *aic, void *data)
{
	int ret = 0;

	if (NULL == aic || NULL == aic->livingcodec) {
		audio_err_print("aic is null or livingcodec is null.\n");
		return -1;
	}

/*	//disable aic record bit
	__i2s_disable_record(aic);
	__i2s_disable_receive_dma(aic);
	__i2s_disable_tloop(aic);
	__i2s_disable_etfl(aic);
	__i2s_enable_stereo(aic);
//	__i2s_disable_monoctr_right(aic);
*/
	//spk stop
//	ret = aic->livingcodec->playback.set_endpoint(CODEC_SPK_STOP);

	return ret;
}

static int aec_ioctl(void *pipe, unsigned int cmd, void *data)
{
	struct audio_aic_device *aic = NULL;
	struct codec_info *cur_codec = NULL;
	struct audio_pipe *aec_pipe = NULL;
	int ret = 0;

	if (pipe == NULL) {
		audio_err_print("pipe is NULL.\n");
		return -1;
	}

	aec_pipe = (struct audio_pipe *)pipe;
	aic = (struct audio_aic_device*)aec_pipe->priv;;
	cur_codec = aic->codec_spk_info;

	if (NULL == aic || NULL == cur_codec) {
		audio_err_print("aic is null or cur_codec is null.\n");
		return -1;
	}

	/*set parameters*/
	switch(cmd) {
		case AUDIO_CMD_CONFIG_PARAM:
			ret = set_codec_aec_datatype(aic, data);
			break;

		case AUDIO_CMD_GET_GAIN:
			//*(unsigned long*)data = cur_codec->again;
			break;

		case AUDIO_CMD_SET_GAIN:
			ret = set_codec_aec_again(aic, data);
			break;
		case AUDIO_CMD_SET_VOLUME:
			ret = set_codec_aec_dgain(aic, data);
			break;

		case AUDIO_CMD_ENABLE_STREAM:
			ret = set_codec_aec_start(aic, data);
			break;

		case AUDIO_CMD_DISABLE_STREAM:
			ret = set_codec_aec_stop(aic, data);
			break;


		default:
			break;
	}

	return ret;
}

static int aec_deinit(void *pipe)
{
	struct audio_aic_device *aic = NULL;
	struct codec_info *cur_codec = NULL;
	struct audio_route *route = NULL;

	if (pipe == NULL) {
		audio_err_print("pipe is NULL.\n");
		return -1;
	}

	route = (struct audio_route*)pipe;
	aic = (struct audio_aic_device*)route->pipe->priv;
	cur_codec = aic->codec_aec_info;

	if (NULL == aic->livingcodec) {
		audio_err_print("livingcodec is NULL.\n");
		return -1;
	}
	if (NULL == cur_codec) {
		audio_err_print("error, codec mic info is null\n");
		return -1;
	}
	cur_codec->rate = DEFAULT_SAMPLERATE;
	cur_codec->format = AFMT_0;
	cur_codec->channel = DEFAULT_CHANNEL;
	cur_codec->transfer_info.data = DATA_I2S_MODE;
	cur_codec->transfer_info.seq = LEFT_FIRST_MODE;
	cur_codec->transfer_info.sync = LEFT_LOW_RIGHT_HIGH;
	cur_codec->transfer_info.rate2mclk = MCLK_DIV_TO_SAMPLE;
	cur_codec->lagain = DEFAULT_AGAIN;
	cur_codec->ragain = DEFAULT_AGAIN;
	cur_codec->ldgain = DEFAULT_DGAIN;
	cur_codec->rdgain = DEFAULT_DGAIN;
	cur_codec->trigger = DEFAULT_RECORD_TRIGGER;
	cur_codec->data_type.frame_size = DEFAULT_FRAME_SIZE;
	cur_codec->data_type.frame_vsize = DEFAULT_VFRAME_SIZE;
	cur_codec->data_type.sample_rate = DEFAULT_SAMPLERATE;
	cur_codec->data_type.sample_channel = DEFAULT_CHANNEL;

	//codec need power down or deinit procedure?
//	aic->livingcodec->set_power(CODEC_POWER_DOWN);

	return 0;
}

static int init_dma_config(struct audio_aic_device *aic)
{
	struct audio_pipe *mic_pipe, *spk_pipe, *aec_pipe;
	int ret;

	aic->mic_pipe = mic_pipe = kzalloc(sizeof(*mic_pipe), GFP_KERNEL);
	aic->spk_pipe = spk_pipe = kzalloc(sizeof(*spk_pipe), GFP_KERNEL);
	aic->aec_pipe = aec_pipe = kzalloc(sizeof(*aec_pipe), GFP_KERNEL);
	aic->codec_mic_info = kzalloc(sizeof(*aic->codec_mic_info), GFP_KERNEL);
	aic->codec_spk_info = kzalloc(sizeof(*aic->codec_spk_info), GFP_KERNEL);
	aic->codec_aec_info = kzalloc(sizeof(*aic->codec_aec_info), GFP_KERNEL);
	if (!mic_pipe || !spk_pipe || !aec_pipe || !aic->codec_mic_info ||
	    !aic->codec_spk_info || !aic->codec_aec_info) {
		ret = -ENOMEM;
		goto fail;
	}

	if (init_pipe(mic_pipe, DMA_FROM_DEVICE, aic->res->start, INGENIC_DMA_REQ_I2S0_RX) ||
	    init_pipe(spk_pipe, DMA_TO_DEVICE, aic->res->start, INGENIC_DMA_REQ_I2S0_TX) ||
	    init_pipe(aec_pipe, DMA_FROM_DEVICE, aic->res->start, INGENIC_DMA_REQ_AEC)) {
		ret = -EINVAL;
		goto fail;
	}

	ret = pipe_request_dma(mic_pipe, aic->dev);
	if (!ret)
		ret = pipe_request_dma(spk_pipe, aic->dev);
	if (!ret)
		ret = pipe_request_dma(aec_pipe, aic->dev);
	if (ret)
		goto fail;

	mic_pipe->init = mic_init;
	mic_pipe->ioctl = mic_ioctl;
	mic_pipe->deinit = mic_deinit;

	spk_pipe->init = spk_init;
	spk_pipe->ioctl = spk_ioctl;
	spk_pipe->deinit = spk_deinit;

	aec_pipe->init = aec_init;
	aec_pipe->ioctl = aec_ioctl;
	aec_pipe->deinit = aec_deinit;

	mic_pipe->priv = aic;
	spk_pipe->priv = aic;
	aec_pipe->priv = aic;
	return 0;
fail:
	aic_free_dma(aic);
	return ret;
}

extern struct platform_driver audio_codec_driver;

/* Power the codec down (speaker amp off) and drop it; the codec's proc entries go with it. */
static void aic_codec_down(struct audio_aic_device *aic)
{
	if (aic->livingcodec)
		aic->livingcodec->set_power(CODEC_POWER_DOWN);
	if (aic->excodec) {
		extern_codec_release(aic);
		return;
	}
	if (aic->codec_file)
		proc_remove(aic->codec_file);
	if (aic->codec_dir)
		proc_remove(aic->codec_dir);
	aic->codec_file = aic->codec_dir = NULL;
	inner_codec_release(aic->livingcodec);
	platform_driver_unregister(&audio_codec_driver);
}

static int audio_aic_probe(struct platform_device *pdev)
{
	struct audio_aic_device *aic;
	int ret;
#define INNER_CODEC  0
#define EXTERNAL_CODEC 1
	int codec_type = INNER_CODEC;

	/* DT-registered platform devices have no preset dma_mask. */
	ret = dma_coerce_mask_and_coherent(&pdev->dev, DMA_BIT_MASK(32));
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "dma mask\n");

	aic = devm_kzalloc(&pdev->dev, sizeof(*aic), GFP_KERNEL);
	if (!aic)
		return -ENOMEM;
	aic->dev = &pdev->dev;
	platform_set_drvdata(pdev, aic);

	aic->i2s_iomem = devm_platform_get_and_ioremap_resource(pdev, 0, &aic->res);
	if (IS_ERR(aic->i2s_iomem))
		return PTR_ERR(aic->i2s_iomem);

	spin_lock_init(&aic->i2s_lock);

	aic->ce_rclock = devm_clk_get(&pdev->dev, "ce_i2sr");
	if (IS_ERR(aic->ce_rclock))
		return dev_err_probe(&pdev->dev, PTR_ERR(aic->ce_rclock), "ce_i2sr\n");
	aic->ce_tclock = devm_clk_get(&pdev->dev, "ce_i2st");
	if (IS_ERR(aic->ce_tclock))
		return dev_err_probe(&pdev->dev, PTR_ERR(aic->ce_tclock), "ce_i2st\n");
	aic->spk_clock = devm_clk_get(&pdev->dev, "div_i2st");
	if (IS_ERR(aic->spk_clock))
		return dev_err_probe(&pdev->dev, PTR_ERR(aic->spk_clock), "div_i2st\n");
	aic->mic_clock = devm_clk_get(&pdev->dev, "div_i2sr");
	if (IS_ERR(aic->mic_clock))
		return dev_err_probe(&pdev->dev, PTR_ERR(aic->mic_clock), "div_i2sr\n");
	aic->aic_clock = devm_clk_get(&pdev->dev, "gate_aic");
	if (IS_ERR(aic->aic_clock))
		return dev_err_probe(&pdev->dev, PTR_ERR(aic->aic_clock), "gate_aic\n");

	aic->aic_irq = platform_get_irq_optional(pdev, 0);
	if (aic->aic_irq > 0) {
		ret = devm_request_irq(&pdev->dev, aic->aic_irq, aic_irq_handler, 0,
				       dev_name(&pdev->dev), aic);
		if (ret)
			return dev_err_probe(&pdev->dev, ret, "irq %d\n", aic->aic_irq);
	} else if (aic->aic_irq == -EPROBE_DEFER) {
		return aic->aic_irq;
	}

	ret = init_dma_config(aic);
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "dma channels\n");

	globe_aic = aic;
	ret = clk_prepare_enable(aic->ce_rclock);
	if (ret)
		goto err_dma;
	ret = clk_prepare_enable(aic->ce_tclock);
	if (ret)
		goto err_ce_r;
	ret = clk_prepare_enable(aic->aic_clock);
	if (ret)
		goto err_ce_t;

	if (excodec_addr != DEFAULT_EXCODEC_ADDR) {
		audio_info_print("excodec name=%s, excodec_addr=%p\n", excodec_name, excodec_addr);
		ret = extern_codec_register(aic, i2c_bus, excodec_name, excodec_addr);
		codec_type = EXTERNAL_CODEC;
	} else {
		ret = platform_driver_register(&audio_codec_driver);
		if (!ret && !aic->livingcodec) {
			platform_driver_unregister(&audio_codec_driver);
			ret = -ENODEV;
			dev_err(&pdev->dev, "no enabled ingenic,t41-codec node\n");
		}
	}
	if (ret)
		goto err_aic_clk;

	clk_set_rate(aic->spk_clock, DEFAULT_REPLAY_CLK);
	clk_prepare_enable(aic->spk_clock);
	clk_set_rate(aic->mic_clock, DEFAULT_REPLAY_CLK);
	clk_prepare_enable(aic->mic_clock);

	__i2s_disable(aic);
	schedule_timeout(5);
	__i2s_disable(aic);

	__i2s_master_clkset(aic);
	__i2s_reset(aic);
	while((i2s_read_reg(aic, AICFR) & I2S_FIFO_RESET_STAT) != 0)
		msleep(1);
	__aic_select_i2s(aic);
	__i2s_select_i2s(aic);

	//set aic transfer protocol:I2S mode
	if (EXTERNAL_CODEC == codec_type) {
		//---------------------------------
		//FPGA test --wjxu
		if(internal_codec){
			__i2s_internal_codec(aic);
		}else{
			__i2s_external_codec(aic);
		}

		//__i2s_select_share_clk(aic);
		//---------------------------------
		//have excodec. we configure master/slave mode, line mode
		if (CODEC_IS_MASTER_MODE == aic->livingcodec->mode) {
			//set codec master mode, AIC slave mode
			__i2s_slave_clkset(aic);
		} else if (CODEC_IS_SLAVE_MODE == aic->livingcodec->mode) {
			__i2s_master_clkset(aic);
		}

		if (CODEC_IS_4_LINES == aic->livingcodec->pins)
			__i2s_select_share_clk(aic);
		else if (CODEC_IS_6_LINES == aic->livingcodec->pins||CODEC_IS_0_LINES == aic->livingcodec->pins) {
			__i2s_select_spilt_clk(aic);
		}

		if (DATA_RIGHT_JUSTIFIED_MODE == aic->livingcodec->transfer_protocol ||
				DATA_LEFT_JUSTIFIED_MODE == aic->livingcodec->transfer_protocol)
			__i2s_select_msbjustified(aic);

		printk("@@@@@ excodec codec power up@@@@@@\n");
		aic->livingcodec->set_power(CODEC_POWER_UP);
	} else {
		//aic: slave mode, codec master mode
		__i2s_internal_codec(aic);
		__i2s_slave_clkset(aic);
		//aic: i2s transfer mode, codec i2s transfer mode
		__i2s_select_i2s(aic);
		//aic->livingcodec.set_sign();
		printk("@@@@@ inner codec power up@@@@@@\n");
		aic->livingcodec->set_power(CODEC_POWER_UP);
	}


	__i2s_disable_receive_dma(aic);
	__i2s_disable_transmit_dma(aic);
	__i2s_disable_tloop(aic);
	__i2s_disable_record(aic);
	__i2s_disable_replay(aic);
	__i2s_disable_loopback(aic);
	__i2s_disable_etfl(aic);
	__i2s_clear_ror(aic);
	__i2s_clear_tur(aic);
	__i2s_set_receive_trigger(aic, DEFAULT_RECORD_TRIGGER);
	__i2s_set_transmit_trigger(aic, DEFAULT_REPLAY_TRIGGER);
	__i2s_write_tfifo_loop(aic, DEFAULT_AEC_TRIGGER);
	__i2s_disable_overrun_intr(aic);
	__i2s_disable_underrun_intr(aic);
	__i2s_disable_transmit_intr(aic);
	__i2s_disable_receive_intr(aic);
	__i2s_send_rfirst(aic);

	__i2s_play_lastsample(aic);

	__i2s_enable(aic);

	//call funtion:registe_aic_to_dsp in dsp module to init dsp route
	if (AUDIO_SUCCESS != register_audio_pipe(aic->mic_pipe, AUDIO_ROUTE_AMIC_ID)) {
		audio_err_print("failed to register amic pipe.\n");
		ret = -EBUSY;
		goto err_codec;
	}
	if (AUDIO_SUCCESS != register_audio_pipe(aic->spk_pipe, AUDIO_ROUTE_SPK_ID)) {
		audio_err_print("failed to register spk pipe.\n");
		ret = -EBUSY;
		goto err_pipes;
	}
	if (AUDIO_SUCCESS != register_audio_pipe(aic->aec_pipe, AUDIO_ROUTE_AEC_ID)) {
		audio_err_print("failed to register aec pipe.\n");
		ret = -EBUSY;
		goto err_pipes;
	}

	if (AUDIO_SUCCESS != register_audio_debug_ops("audio_aic_info", &audio_aic_debug_fops, aic))
		audio_err_print("failed to register audio aic debug proc file!\n");
	audio_info_print("aic init success.\n");

	return 0;

err_pipes:
	release_audio_pipe(aic->mic_pipe);
	release_audio_pipe(aic->spk_pipe);
	release_audio_pipe(aic->aec_pipe);
err_codec:
	aic_codec_down(aic);
	__i2s_disable(aic);
	clk_disable_unprepare(aic->mic_clock);
	clk_disable_unprepare(aic->spk_clock);
err_aic_clk:
	clk_disable_unprepare(aic->aic_clock);
err_ce_t:
	clk_disable_unprepare(aic->ce_tclock);
err_ce_r:
	clk_disable_unprepare(aic->ce_rclock);
err_dma:
	globe_aic = NULL;
	aic_free_dma(aic);
	return ret;
}

static void audio_aic_remove(struct platform_device *pdev)
{
	struct audio_aic_device *aic = platform_get_drvdata(pdev);

	release_audio_pipe(aic->mic_pipe);
	release_audio_pipe(aic->spk_pipe);
	release_audio_pipe(aic->aec_pipe);

	aic_codec_down(aic);

	__i2s_disable(aic);
	clk_disable_unprepare(aic->mic_clock);
	clk_disable_unprepare(aic->spk_clock);
	clk_disable_unprepare(aic->aic_clock);
	clk_disable_unprepare(aic->ce_tclock);
	clk_disable_unprepare(aic->ce_rclock);

	aic_free_dma(aic);
	globe_aic = NULL;
}

static const struct of_device_id audio_aic_of_match[] = {
	{ .compatible = "ingenic,t41-aic" },
	{ }
};
MODULE_DEVICE_TABLE(of, audio_aic_of_match);

struct platform_driver audio_aic_driver = {
	.probe = audio_aic_probe,
	.remove = audio_aic_remove,
	.driver = {
		.name = "jz-aic",
		.owner = THIS_MODULE,
		.of_match_table = audio_aic_of_match,
	},
};
