#include <linux/cdev.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/fcntl.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kfifo.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/signal.h>
#include <linux/slab.h>
#include <linux/stddef.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include <linux/resource.h>
#include <linux/clk.h>
#include <linux/string.h>
#include <dt-bindings/clock/ingenic,t40-cgu.h>

#include "avpu_ioctl.h"
#include "avpu_alloc_ioctl.h"
#include "avpu_ip.h"
#include "rmem_cache.h"

#define DEV_NAME "avpu"

#define AVPU_DRIVER_VERSION "H20220825a"

int avpu_codec_major;
int avpu_codec_nr_devs = AVPU_NR_DEVS;
module_param(avpu_codec_nr_devs, int, S_IRUGO);
/*
 * MPLL, as the vendor default: the sensor drivers retune VPLL (1080 MHz for a
 * 27 MHz MCLK) after this module loads, which would move the encoder clock
 * underneath a running encoder.
 */
static char *clk_name = "mpll";
module_param(clk_name, charp, S_IRUGO);
MODULE_PARM_DESC(clk_name, "parent of the EL150 clock: mpll, vpll, epll or sclka");
static int avpu_clk = 550000000;
module_param(avpu_clk, int, S_IRUGO);
MODULE_PARM_DESC(avpu_clk, "avpu clock freq");
static bool flush_trace;
module_param(flush_trace, bool, 0644);
MODULE_PARM_DESC(flush_trace, "log every cache flush ioctl");
static struct class *module_class;

struct flush_cache_info {
	unsigned int	addr;
	unsigned int	len;
#define WBACK		DMA_TO_DEVICE
#define INV		DMA_FROM_DEVICE
#define WBACK_INV	DMA_BIDIRECTIONAL
	unsigned int	dir;
};

static struct platform_device *avpu_twin;

static int channel_is_ready(struct avpu_codec_chan *chan)
{
	unsigned long flags;
	int ret = chan->unblock;

	spin_lock_irqsave(&chan->codec->i_lock, flags);
	ret = ret || !list_empty(&chan->codec->irq_masks);
	spin_unlock_irqrestore(&chan->codec->i_lock, flags);
	return ret;
}

static int avpu_codec_open(struct inode *inode, struct file *filp)
{
	struct avpu_codec_chan *chan;
	int ret;

	chan = kzalloc(sizeof(struct avpu_codec_chan), GFP_KERNEL);
	if (!chan) {
		ret = -ENOMEM;
		goto fail;
	}

	INIT_LIST_HEAD(&chan->mem);
	spin_lock_init(&chan->lock);
	chan->num_bufs = 0;

	filp->private_data = chan;

	/* irq */
	init_waitqueue_head(&chan->irq_queue);

	ret = avpu_codec_bind_channel(chan, inode);
	if (ret)
		goto fail_codec_binding;
//	printk("--------------%s(%d)-----------\n", __func__, __LINE__);
	return 0;

fail_codec_binding:
//	printk("--------------%s(%d)-----------\n", __func__, __LINE__);
	kfree_sensitive(chan);
fail:
//	printk("--------------%s(%d)-----------\n", __func__, __LINE__);
	return ret;
}

static int avpu_codec_release(struct inode *inode, struct file *filp)
{
	struct avpu_codec_chan *chan = filp->private_data;
	struct avpu_dma_buf_mmap *tmp;
	struct list_head *pos, *n;
//	printk("--------------%s(%d)-----------\n", __func__, __LINE__);
	avpu_codec_unbind_channel(chan);
	list_for_each_safe(pos, n, &chan->mem){
		tmp = list_entry(pos, struct avpu_dma_buf_mmap, list);
		list_del(pos);
		kfree(tmp);
	}

	kfree(chan);

	return 0;
}

static long avpu_codec_compat_ioctl(struct file *file, unsigned int cmd,
				    unsigned long arg)
{
	long ret = -ENOIOCTLCMD;

	if (file->f_op->unlocked_ioctl)
		ret = file->f_op->unlocked_ioctl(file, cmd, arg);

	return ret;
}

static struct avpu_dma_buffer *find_buf_by_id(struct avpu_codec_chan *chan, int desc_id)
{
	struct avpu_dma_buf_mmap *cur_buf_mmap;

	list_for_each_entry(cur_buf_mmap, &chan->mem, list){
		if (cur_buf_mmap->buf_id == desc_id)
			return cur_buf_mmap->buf;
	}

	return NULL;
}

static int avpu_dma_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct avpu_codec_chan *chan = filp->private_data;
	unsigned long start = vma->vm_start;
	unsigned long vsize = vma->vm_end - start;
	/* offset if already in page */
	int desc_id = vma->vm_pgoff;
	int ret = 0;
	struct avpu_dma_buffer *buf = find_buf_by_id(chan, desc_id);

	if (!buf)
		return -EINVAL;

	vma->vm_pgoff = 0;

	ret = dma_mmap_coherent(chan->codec->device, vma, buf->cpu_handle,
				buf->dma_handle, vsize);
	if (ret < 0) {
		pr_err("Remapping memory failed, error: %d\n", ret);
		return ret;
	}

	vm_flags_set(vma, VM_DONTEXPAND | VM_DONTDUMP);

	return 0;
}

static int unblock_channel(struct avpu_codec_chan *chan)
{
	chan->unblock = 1;
//	printk("--------------%s(%d)-----------\n", __func__, __LINE__);
	wake_up_interruptible(&chan->irq_queue);
	return 0;
}

static int wait_irq(struct avpu_codec_chan *chan, unsigned long arg)
{
	struct avpu_codec_desc *codec = chan->codec;
	int callback;
	struct r_irq *i_callback;
	unsigned long flags;
	int ret;

//	printk("--------------%s(%d)-----------\n", __func__, __LINE__);
	ret = wait_event_interruptible(chan->irq_queue,
				       channel_is_ready(chan));
//	printk("--------------%s(%d)-----------\n", __func__, __LINE__);
	if (ret == -ERESTARTSYS)
		return ret;
	if (chan->unblock) {
		avpu_dbg("Unblocking channel\n");
		return -EINTR;
	}

//	printk("--------------%s(%d)-----------\n", __func__, __LINE__);
	spin_lock_irqsave(&codec->i_lock, flags);
	i_callback = list_first_entry(&chan->codec->irq_masks,
				      struct r_irq, list);
	callback = i_callback->bitfield;
	list_del(&i_callback->list);
	kmem_cache_free(codec->cache, i_callback);
	spin_unlock_irqrestore(&codec->i_lock, flags);
//	printk("--------------%s(%d)-----------\n", __func__, __LINE__);

	if (copy_to_user((void *)arg, &callback, sizeof(__u32)))
		return -EFAULT;

	return ret;
}

static int read_reg(struct avpu_codec_chan *chan, unsigned long arg)
{
	struct avpu_reg reg;
	struct avpu_codec_desc *codec = chan->codec;
	int err;

	if (copy_from_user(&reg, (struct avpu_reg *)arg, sizeof(struct avpu_reg)))
		return -EFAULT;

	if (reg.id % 4) {
		avpu_err("Unaligned register access: 0x%.4X\n",
			reg.id);
		return -EINVAL;
	}

#if 1 /* T40 */
	if (reg.id < 0x8000 || reg.id > chan->codec->regs_size) {
		avpu_err("Out-of-range register read: 0x%.4X\n",
			reg.id);
		return -EINVAL;
	}
#elif 0
	if (reg.id < 0x0000 || reg.id > chan->codec->regs_size) {
		avpu_err("Out-of-range register read: 0x%.4X\n",
			reg.id);
		return -EINVAL;
	}
#endif

	err = avpu_codec_read_register(chan, &reg);
	if (err)
		return err;

	if (copy_to_user((struct avpu_reg *)arg, &reg, sizeof(struct avpu_reg)))
		return -EFAULT;

	avpu_dbg("Reg read: 0x%.4X: 0x%.8x\n", reg.id, reg.value);

	return 0;
}

static int write_reg(struct avpu_codec_chan *chan, unsigned long arg)
{
	struct avpu_reg reg;
	struct avpu_codec_desc *codec = chan->codec;

	if (copy_from_user(&reg, (struct avpu_reg *)arg, sizeof(struct avpu_reg)))
		return -EFAULT;

	avpu_dbg("Reg write: 0x%.4X: 0x%.8x\n", reg.id, reg.value);
	if (reg.id == 0x8084 || reg.id == 0x8094)
		avpu_dbg("Reg write: 0x%.4X: 0x%.8x\n", reg.id, reg.value);

	if (reg.id % 4) {
		avpu_err("Unaligned register access: 0x%.4X\n",
			reg.id);
		return -EINVAL;
	}

#if 1 /* T40 */
	if (reg.id < 0x8000 || reg.id > chan->codec->regs_size) {
		avpu_dbg("Out-of-range register write: 0x%.4X\n", reg.id);
		return -EINVAL;
	}
#elif 0
	if (reg.id < 0x0000 || reg.id > chan->codec->regs_size) {
		avpu_dbg("Out-of-range register write: 0x%.4X\n", reg.id);
		return -EINVAL;
	}
#endif

	avpu_codec_write_register(chan, &reg);

	if (copy_to_user((struct avpu_reg *)arg, &reg, sizeof(struct avpu_reg)))
		return -EFAULT;

	return 0;
}
static long jz_cmd_flush_cache(long arg)
{
	struct flush_cache_info info;

	if (copy_from_user(&info, (void __user *)arg, sizeof(info)))
		return -EFAULT;
	if (flush_trace)
		pr_info("avpu: flush %08x+%x dir %u\n", info.addr, info.len, info.dir);
	return rmem_cache_sync_user(info.addr, info.len, info.dir);
}

static long avpu_codec_ioctl(struct file *filp, unsigned int cmd,
			     unsigned long arg)
{
	struct avpu_codec_chan *chan = filp->private_data;
	struct avpu_codec_desc *codec = chan->codec;

	switch (cmd) {
	case GET_DMA_MMAP:
		return avpu_ioctl_get_dma_mmap(codec->device, chan, arg);
	case GET_DMA_FD:
		return avpu_ioctl_get_dma_fd(codec->device, arg);
	case GET_DMA_PHY:
		return avpu_ioctl_get_dmabuf_dma_addr(codec->device, arg);
	case AL_CMD_UNBLOCK_CHANNEL:
		return unblock_channel(chan);
	case AL_CMD_IP_WAIT_IRQ:
		return wait_irq(chan, arg);
	case AL_CMD_IP_READ_REG:
		return read_reg(chan, arg);
	case AL_CMD_IP_WRITE_REG:
		return write_reg(chan, arg);
	case JZ_CMD_FLUSH_CACHE:
		return jz_cmd_flush_cache(arg);
	default:
		avpu_err("Unknown ioctl: 0x%.8X\n", cmd);
		return -EINVAL;
	}
}

const struct file_operations avpu_codec_fops = {
	.owner		= THIS_MODULE,
	.open		= avpu_codec_open,
	.release	= avpu_codec_release,
	.unlocked_ioctl = avpu_codec_ioctl,
	.compat_ioctl	= avpu_codec_compat_ioctl,
	.mmap		= avpu_dma_mmap,
};

static void clean_up_avpu_codec_cdev(struct avpu_codec_desc *dev)
{
	cdev_del(&dev->cdev);
}

static int setup_chrdev_region(void)
{
	dev_t dev = 0;
	int err;

	if (avpu_codec_major == 0) {
		err = alloc_chrdev_region(&dev, 0, avpu_codec_nr_devs, "avpu");
		avpu_codec_major = MAJOR(dev);

		if (err) {
			pr_alert("Allegro codec: can't get major %d\n",
				 avpu_codec_major);
			return err;
		}
	}
	return 0;
}

static int avpu_setup_codec_cdev(struct avpu_codec_desc *codec, int minor,
			  const char *device_name)
{
	struct device *device;
	int err, devno =
		MKDEV(avpu_codec_major, minor);

	cdev_init(&codec->cdev, &avpu_codec_fops);
	codec->cdev.owner = THIS_MODULE;
	err = cdev_add(&codec->cdev, devno, 1);
	if (err) {
		avpu_err("Error %d adding avpu device number %d", err, minor);
		return err;
	}

	if (device_name != NULL) {
		device = device_create(module_class, NULL, devno, NULL,
				       device_name);
		if (IS_ERR(device)) {
			pr_err("device not created\n");
			clean_up_avpu_codec_cdev(codec);
			return PTR_ERR(device);
		}
	}

	return 0;
}

static int init_codec_desc(struct avpu_codec_desc *codec)
{
	INIT_LIST_HEAD(&codec->irq_masks);
	spin_lock_init(&codec->i_lock);
	/* make chan requirement explicit */
	codec->chan = NULL;
	codec->cache =  kmem_cache_create("avpu_codec_ram",
					  sizeof(struct r_irq),
					  0, SLAB_HWCACHE_ALIGN, NULL);
	if (!codec->cache)
		return -ENOMEM;

	return 0;
}

static void deinit_codec_desc(struct avpu_codec_desc *codec)
{
	kmem_cache_destroy(codec->cache);
}

static int avpu_set_parent_rate(struct device *dev, struct clk *clk)
{
	static const struct {
		const char *name;
		u32 index;
	} parents[] = {
		{ "sclk_a", T40_CLK_SCLKA },
		{ "sclka", T40_CLK_SCLKA },
		{ "mpll", T40_CLK_MPLL },
		{ "vpll", T40_CLK_VPLL },
		{ "epll", T40_CLK_EPLL },
	};
	struct of_phandle_args args = { .args_count = 1 };
	struct clk *parent;
	long rate;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(parents); i++)
		if (!strcmp(clk_name, parents[i].name))
			break;
	if (i == ARRAY_SIZE(parents)) {
		dev_err(dev, "unknown clk_name %s\n", clk_name);
		return -EINVAL;
	}

	args.np = of_find_compatible_node(NULL, NULL, "ingenic,t40-cgu");
	if (!args.np)
		return -ENODEV;
	args.args[0] = parents[i].index;
	parent = of_clk_get_from_provider(&args);
	of_node_put(args.np);
	if (IS_ERR(parent))
		return PTR_ERR(parent);

	ret = clk_set_parent(clk, parent);
	clk_put(parent);
	if (ret)
		return ret;

	/* the CGU divider only makes exact ratios; 3.10 took the nearest */
	rate = clk_round_rate(clk, avpu_clk);
	return clk_set_rate(clk, rate > 0 ? rate : avpu_clk);
}

static int avpu_codec_probe(struct platform_device *pdev)
{
	int err, irq;
	static int current_minor;
	struct resource *res;
	struct avpu_codec_desc *codec
		= devm_kzalloc(&pdev->dev, sizeof(*codec), GFP_KERNEL);

	if (!codec)
		return -ENOMEM;

	codec->device = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		avpu_err("Can't get resource\n");
		return -ENODEV;
	}

	codec->regs = devm_ioremap(&pdev->dev, res->start, resource_size(res));
	if (!codec->regs) {
		avpu_err("Can't map registers\n");
		return -ENOMEM;
	}
	codec->regs_size = res->end - res->start;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		avpu_err("No irq\n");
		return irq;
	}

	codec->ahb1_gate = devm_clk_get(&pdev->dev, "ahb1");
	if (IS_ERR(codec->ahb1_gate))
		return dev_err_probe(&pdev->dev, PTR_ERR(codec->ahb1_gate),
				     "ahb1 clock\n");
	codec->clk = devm_clk_get(&pdev->dev, "avpu");
	if (IS_ERR(codec->clk))
		return dev_err_probe(&pdev->dev, PTR_ERR(codec->clk),
				     "avpu clock\n");

	err = avpu_set_parent_rate(&pdev->dev, codec->clk);
	if (err)
		avpu_err("parent/rate setup failed: %d\n", err);

	err = clk_prepare_enable(codec->ahb1_gate);
	if (err)
		return err;
	err = clk_prepare_enable(codec->clk);
	if (err)
		goto out_ahb1;

	err = init_codec_desc(codec);
	if (err)
		goto out_clk;

	/* clocks are up: a pending AVPU interrupt reads registers */
	err = request_irq(irq, avpu_hardirq_handler, IRQF_SHARED,
			  dev_name(&pdev->dev), codec);
	if (err) {
		avpu_err("Failed to request IRQ #%d -> :%d\n", irq, err);
		goto out_desc;
	}
	codec->irq = irq;

	platform_set_drvdata(pdev, codec);

	err = avpu_setup_codec_cdev(codec, current_minor, DEV_NAME);
	if (err)
		goto out_irq;

	codec->minor = current_minor;
	++current_minor;
	avpu_info("@@@@ avpu driver ok(version %s) @@@@@, irq %d, clk %lu Hz\n",
		  AVPU_DRIVER_VERSION, irq, clk_get_rate(codec->clk));

	return 0;

out_irq:
	free_irq(irq, codec);
out_desc:
	deinit_codec_desc(codec);
out_clk:
	clk_disable_unprepare(codec->clk);
out_ahb1:
	clk_disable_unprepare(codec->ahb1_gate);
	return err;
}

static void avpu_codec_remove(struct platform_device *pdev)
{
	struct avpu_codec_desc *codec = platform_get_drvdata(pdev);
	dev_t dev = MKDEV(avpu_codec_major, codec->minor);

	device_destroy(module_class, dev);
	clean_up_avpu_codec_cdev(codec);
	free_irq(codec->irq, codec);
	deinit_codec_desc(codec);
	clk_disable_unprepare(codec->clk);
	clk_disable_unprepare(codec->ahb1_gate);
}

static const struct of_device_id avpu_codec_of_match[] = {
	{ .compatible = "ingenic,t40-avpu" },
	{ /* sentinel */ },
};

MODULE_DEVICE_TABLE(of, avpu_codec_of_match);

static struct platform_driver avpu_platform_driver = {
	.probe			= avpu_codec_probe,
	.remove			= avpu_codec_remove,
	.driver			=       {
		.name		= "avpu",
		.of_match_table = avpu_codec_of_match,
	},
};

static int create_module_class(void)
{
	module_class = class_create("avpu_class");
	if (IS_ERR(module_class))
		return PTR_ERR(module_class);

	return 0;
}

static void destroy_module_class(void)
{
	class_destroy(module_class);
}

/*
 * The DT node is disabled on this kernel, so no platform device exists for
 * it. If it is ever enabled the OF core creates the device and the driver
 * binds natively; otherwise a device is made here from the same node, with
 * the node attached so clocks and the interrupt come from the DT.
 */
static int avpu_create_twin(struct device_node *np)
{
	struct resource res;
	int ret;

	ret = of_address_to_resource(np, 0, &res);
	if (ret)
		return ret;

	avpu_twin = platform_device_alloc("avpu", 0);
	if (!avpu_twin)
		return -ENOMEM;

	ret = platform_device_add_resources(avpu_twin, &res, 1);
	if (ret)
		goto err;

	device_set_node(&avpu_twin->dev, of_fwnode_handle(of_node_get(np)));
	avpu_twin->dev.coherent_dma_mask = DMA_BIT_MASK(32);
	avpu_twin->dev.dma_mask = &avpu_twin->dev.coherent_dma_mask;

	ret = platform_device_add(avpu_twin);
	if (ret)
		goto err;
	return 0;
err:
	platform_device_put(avpu_twin);
	avpu_twin = NULL;
	return ret;
}

static int avpu_module_init(void)
{
	struct device_node *np;
	int ret;

	np = of_find_compatible_node(NULL, NULL, "ingenic,t40-avpu");
	if (!np) {
		pr_err("avpu: no ingenic,t40-avpu node in the device tree\n");
		return -ENODEV;
	}

	if (!of_device_is_available(np)) {
		ret = avpu_create_twin(np);
		if (ret) {
			of_node_put(np);
			return ret;
		}
	}
	of_node_put(np);

	ret = platform_driver_register(&avpu_platform_driver);
	if (ret && avpu_twin) {
		platform_device_unregister(avpu_twin);
		avpu_twin = NULL;
	}

	return ret;
}

static void avpu_module_deinit(void)
{
	platform_driver_unregister(&avpu_platform_driver);
	if (avpu_twin)
		platform_device_unregister(avpu_twin);
}

static int __init avpu_codec_init(void)
{
	dev_t devno;
	int err = setup_chrdev_region();

	if (err)
		return err;

	err = create_module_class();
	if (err)
		goto fail;

	err = avpu_module_init();
	if (err)
		goto fail;

	return 0;

fail:
	devno = MKDEV(avpu_codec_major, 0);
	unregister_chrdev_region(devno, avpu_codec_nr_devs);

	return err;
}

static void __exit avpu_codec_exit(void)
{
	dev_t devno = MKDEV(avpu_codec_major, 0);

	avpu_module_deinit();
	destroy_module_class();
	unregister_chrdev_region(devno, avpu_codec_nr_devs);
}

module_init(avpu_codec_init);
module_exit(avpu_codec_exit);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Kevin Grandemange");
MODULE_AUTHOR("Sebastien Alaiwan");
MODULE_DESCRIPTION("VPU Driver");
