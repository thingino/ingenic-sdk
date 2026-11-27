
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/completion.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/wait.h>

#include "ingenic_aip.h"
#define AIP_DRIVER_VERSION "H20230213a"
/* #define DEBUG_ENABLE */
#ifdef DEBUG_ENABLE
#define my_printk(fmt, x...) \
	do {	\
		printk("[%s:%d] "fmt"",__func__,__LINE__,##x); \
	}while(0)
#else
#define my_printk(fmt, x...)
#endif

#define REG_AIP_IRQ_START       (0x04)

#define JZ_AIP_NUM      	3

#define CCU_MSCR		0x60
#define CCU_MSCR_AIP_CLK	BIT(25)

/* the vendor T41 driver returns this in place of a kernel address */
#define AIP_T41_CHAINBUF_TAG	0x20240103

struct jz_aip_info {
	bool cpm;			/* A1: CGU gate + CPM SRBC0 reset */
	unsigned int chainbuf_size[JZ_AIP_NUM];
	bool chainbuf_tag;		/* T41: MALLOC reports the tag as vaddr */
	bool any_irq;			/* T41: every interrupt ends the wait */
};

struct jz_aip {
	char				name[16];
	int				idx;
	struct device			*dev;
	void __iomem			*iomem;
	void __iomem			*cpm;
	void __iomem			*arb;

	int				irq;
	struct completion		done;
	spinlock_t			slock;
	unsigned int			status;
	pid_t				owner_pid;
	struct miscdevice		mdev;
	struct mutex			mlock;
	int				opencnt;
	struct jz_aip_chainbuf 		*chainbuf;
	const struct jz_aip_info	*info;
	struct file			*lock_owner;
	wait_queue_head_t		lock_wait;
};

#define jz_aip_readl(aip, offset)		__raw_readl((aip)->iomem + offset)
#define jz_aip_writel(aip, offset, value)	__raw_writel((value), (aip)->iomem + offset)

#define CLEAR_RADIX_BIT(aip,offset,bm)				\
	do {							\
		unsigned int stat;				\
		stat = jz_aip_readl(aip,offset);			\
		jz_aip_writel(aip,offset,stat & ~(bm));		\
	} while(0)

static irqreturn_t jz_aip_interrupt_t(int irq, void *data)
{
	struct jz_aip *aip = (struct jz_aip *)data;

	/* disable_irq_nosync(irq); */

	/* printk("############irq status = 0x%08x\n",aip->status); */
	aip->status = jz_aip_readl(aip, REG_AIP_IRQ_START);
	jz_aip_writel(aip, REG_AIP_IRQ_START, aip->status);
	my_printk("irq status = 0x%08x",aip->status);

	if (aip->info->any_irq) {
		if (aip->status & BIT(2))
			dev_err(aip->mdev.this_device, "irq %d status 0x%08x: timeout\n", irq, aip->status);
		complete(&aip->done);
		return IRQ_HANDLED;
	}

	if(aip->idx == 0) {
		if(aip->status & 0x1<<3) {
			dev_dbg(aip->mdev.this_device, "%s : irq = %d, status = 0x%08x\n",__func__, irq, aip->status);
			complete(&aip->done);
		}

		if(aip->status & 0x1<<2) {
			dev_err(aip->mdev.this_device, "%s : irq = %d, status = 0x%08x\n, timeout!!!",__func__, irq, aip->status);
		}

		if(aip->status & 0x1<<1) {
			dev_dbg(aip->mdev.this_device, "%s : irq = %d, status = 0x%08x\n",__func__, irq, aip->status);
			complete(&aip->done);
		}

		if(aip->status & 0x1) {
			dev_dbg(aip->mdev.this_device, "%s : irq = %d, status = 0x%08x\n",__func__, irq, aip->status);
		}
	} else if(aip->idx == 1 || aip->idx == 2) {
		if(aip->status & 0x1<<2) {
			dev_err(aip->mdev.this_device, "%s : irq = %d, status = 0x%08x, timeout!!!\n",__func__, irq, aip->status);
		}

		if(aip->status & 0x1<<1) {
			dev_dbg(aip->mdev.this_device, "%s : irq = %d, status = 0x%08x\n",__func__, irq, aip->status);
			complete(&aip->done);
		}

		if(aip->status & 0x1) {
			dev_dbg(aip->mdev.this_device, "%s : irq = %d, status = 0x%08x\n",__func__, irq, aip->status);
			complete(&aip->done);
		}
	}

	/* enable_irq(irq); */

	return IRQ_HANDLED;
}

static int jz_aip_open(struct inode *inode, struct file *file)
{
	struct miscdevice *mdev = file->private_data;
	struct jz_aip *aip = list_entry(mdev, struct jz_aip, mdev);

	mutex_lock(&aip->mlock);
	/* vendor: a sync, then 0 to the register at phys 0x12200000, then a sync */
	wmb();
	writel(0, aip->arb);
	wmb();
	if (!aip->opencnt) {
		aip->done.done = 0;
		enable_irq(aip->irq);
	}
	aip->opencnt++;
	mutex_unlock(&aip->mlock);

	my_printk("open successful, opencnt=%d!\n", aip->opencnt);
	return 0;
}

static int jz_aip_release(struct inode *inode, struct file *file)
{
	struct miscdevice *mdev = file->private_data;
	struct jz_aip *aip = list_entry(mdev, struct jz_aip, mdev);

	mutex_lock(&aip->mlock);
	if (aip->lock_owner == file) {
		aip->lock_owner = NULL;
		wake_up(&aip->lock_wait);
	}
	if (aip->opencnt == 1) {
		disable_irq(aip->irq);
		aip->done.done = 0;
	}
	if (aip->opencnt > 0)
		aip->opencnt--;
	mutex_unlock(&aip->mlock);
	my_printk("close successful, opencnt=%d!\n", aip->opencnt);

	return 0;
}

static ssize_t jz_aip_read(struct file *file, char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}

static ssize_t jz_aip_write(struct file *file, const char __user * buffer, size_t count, loff_t * ppos)
{
	return 0;
}

static long jz_aip_wait_irq(struct jz_aip *aip, unsigned long arg)
{
	long ret = 0;
	my_printk("aip index=%d wait done....\n", aip->idx);

	ret = wait_for_completion_timeout(&aip->done, msecs_to_jiffies(10*1000));
	if (aip->info->any_irq) {
		/*
		 * The T41 vendor driver drops completions that arrived while
		 * nobody waited, and reports the last status even after a
		 * timeout; libaip is written against that.
		 */
		reinit_completion(&aip->done);
		if (!ret)
			dev_err(aip->mdev.this_device, "aip index=%d wait done timeout!\n", aip->idx);
	} else if (ret <= 0) {
		dev_err(aip->mdev.this_device, "aip index=%d wait done timeout!\n", aip->idx);
		return -EFAULT;
	}

	if (copy_to_user((void __user *)arg, &aip->status, sizeof(unsigned int))) {
		dev_err(aip->mdev.this_device, "copy_to_user aip index=%d, stat=0x%x failed!\n", aip->idx, aip->status);
		return -EFAULT;
	}

	return 0;
}

static long jz_aip_alloc_chainbuf(struct jz_aip *aip, unsigned long arg)
{
	long ret = 0;
	/*void *page = NULL;*/
	/*void *endpage = NULL;*/
	struct jz_aip_chainbuf *buf = aip->chainbuf;
	struct jz_aip_chainbuf tagged;

	if (aip->info->chainbuf_tag) {
		tagged = *buf;
		tagged.vaddr = (void *)AIP_T41_CHAINBUF_TAG;
		buf = &tagged;
	}

#if 0
	if(copy_from_user(buf, (void __user *)arg, sizeof(struct jz_aip_chainbuf))) {
		dev_err(aip->mdev.this_device, "copy_from_user error!!!\n");
		ret = -EFAULT;
		goto err_copy_from_user;
	}
	buf->size = PAGE_ALIGN(buf->size);
	buf->vaddr = dma_alloc_coherent(aip->mdev.this_device, buf->size, (dma_addr_t *)&buf->paddr, GFP_KERNEL);
	if(buf->vaddr == NULL) {
		dev_err(aip->mdev.this_device, "[%s:%d]:dma_alloc_coherent failed!!!\n",__func__,__LINE__);
		ret = -ENOMEM;
		goto err_dma_alloc_coherent;
	}
	endpage = buf->vaddr + buf->size;
	for(page = buf->vaddr; page < endpage; page += PAGE_SIZE) {
		SetPageReserved(virt_to_page(page));
	}
#endif
	/* printk("alloc: buf.size = %d, buf.paddr = 0x%08x, buf.vaddr = 0x%08x\n",buf->size, buf->paddr, buf->vaddr); */
	if(copy_to_user((void __user *)arg, buf, sizeof(struct jz_aip_chainbuf))) {
		dev_err(aip->mdev.this_device, "copy_to_user failed!!! %s:%d\n",__func__,__LINE__);
		ret = -ENOMEM;
		goto err_copy_to_user;
	}

	return 0;

err_copy_to_user:
	/*dma_free_coherent(aip->mdev.this_device, buf->size, buf->vaddr, (dma_addr_t)buf->paddr);*/
/*err_dma_alloc_coherent:*/
/*err_copy_from_user:*/
	return ret;
}
static long jz_aip_free_chainbuf(struct jz_aip *aip, unsigned long arg)
{
	long ret = 0;
	/*struct jz_aip_chainbuf *buf = aip->chainbuf;*/

	/* printk("free: buf.size = %d, buf.paddr = 0x%08x, buf.vaddr = 0x%08x\n",buf->size, buf->paddr, buf->vaddr); */
	/*dma_free_coherent(aip->mdev.this_device, buf->size, buf->vaddr, (dma_addr_t)buf->paddr);*/

	return ret;
}

/*
 * The vendor T41 driver holds a mutex across the return to user space; a
 * lock owned by the open file does the same without that.
 */
static long jz_aip_lock(struct jz_aip *aip, struct file *file)
{
	int ret;

	mutex_lock(&aip->mlock);
	while (aip->lock_owner && aip->lock_owner != file) {
		mutex_unlock(&aip->mlock);
		ret = wait_event_interruptible(aip->lock_wait, !READ_ONCE(aip->lock_owner));
		if (ret)
			return ret;
		mutex_lock(&aip->mlock);
	}
	aip->lock_owner = file;
	mutex_unlock(&aip->mlock);
	return 0;
}

static long jz_aip_unlock(struct jz_aip *aip, struct file *file)
{
	long ret = -EPERM;

	mutex_lock(&aip->mlock);
	if (aip->lock_owner == file) {
		aip->lock_owner = NULL;
		wake_up(&aip->lock_wait);
		ret = 0;
	}
	mutex_unlock(&aip->mlock);
	return ret;
}

static long jz_aip_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *mdev = file->private_data;
	struct jz_aip *aip = list_entry(mdev, struct jz_aip, mdev);

	switch(cmd) {
	case IOCTL_AIP_IRQ_WAIT_CMP:
		return jz_aip_wait_irq(aip, arg);
	case IOCTL_AIP_MALLOC:
		return jz_aip_alloc_chainbuf(aip, arg);
	case IOCTL_AIP_FREE:
		return jz_aip_free_chainbuf(aip, arg);
	case IOCTL_AIP_LOCK:
		return jz_aip_lock(aip, file);
	case IOCTL_AIP_UNLOCK:
		return jz_aip_unlock(aip, file);
	default:
		dev_err(mdev->this_device, "%s:invalid cmd %u\n", __func__, cmd);
		return -1;
	}

	return 0;
}

static int jz_aip_mmap(struct file *file, struct vm_area_struct *vma)
{
	vm_flags_set(vma, VM_IO);
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);

	if (io_remap_pfn_range(vma,vma->vm_start,
				vma->vm_pgoff,
				vma->vm_end - vma->vm_start,
				vma->vm_page_prot))
		return -EAGAIN;

	return 0;
}

const struct file_operations jz_aip_fops = {
	.owner = THIS_MODULE,
	.open = jz_aip_open,
	.release = jz_aip_release,
	.read = jz_aip_read,
	.write = jz_aip_write,
	.unlocked_ioctl = jz_aip_ioctl,
	.mmap = jz_aip_mmap,
};

/* the three engines (T, F, P) share the node's window: +0, +0x200, +0x300 */
static const unsigned int aip_off[JZ_AIP_NUM] = { 0, 0x200, 0x300 };
static const char * const aip_name[JZ_AIP_NUM] = { "jzaip_t", "jzaip_f", "jzaip_p" };

struct aip_dev {
	struct jz_aip *aip[JZ_AIP_NUM];
};

static void jz_aip_free(struct jz_aip *aip)
{
	struct jz_aip_chainbuf *buf = aip->chainbuf;

	if (aip->mdev.this_device)
		misc_deregister(&aip->mdev);
	if (buf) {
		if (buf->vaddr)
			dma_free_coherent(aip->dev, buf->size, buf->vaddr, (dma_addr_t)buf->paddr);
		kfree(buf);
	}
	free_irq(aip->irq, aip);
	iounmap(aip->iomem);
	kfree(aip);
}

static int jz_aip_setup(struct platform_device *pdev, struct resource *res,
			const struct jz_aip_info *info, void __iomem *cpm,
			void __iomem *arb, int idx, struct jz_aip **out)
{
	struct jz_aip *aip;
	struct jz_aip_chainbuf *buf;
	unsigned int value;
	int ret;

	aip = kzalloc(sizeof(*aip), GFP_KERNEL);
	if (!aip)
		return -ENOMEM;

	aip->dev = &pdev->dev;
	aip->idx = idx;
	aip->cpm = cpm;
	aip->arb = arb;
	aip->info = info;
	init_waitqueue_head(&aip->lock_wait);
	strscpy(aip->name, aip_name[idx], sizeof(aip->name));

	aip->iomem = ioremap(res->start + aip_off[idx], 0x100);
	if (!aip->iomem) {
		ret = -ENXIO;
		goto err_free;
	}

	aip->irq = platform_get_irq(pdev, idx);
	if (aip->irq < 0) {
		ret = aip->irq;
		goto err_iomem;
	}
	ret = request_irq(aip->irq, jz_aip_interrupt_t, 0, aip->name, aip);
	if (ret < 0) {
		dev_err(&pdev->dev, "request_irq(%d) failed\n", aip->irq);
		goto err_iomem;
	}
	disable_irq_nosync(aip->irq);

	init_completion(&aip->done);
	spin_lock_init(&aip->slock);
	mutex_init(&aip->mlock);

	if (cpm) {
		/* pulse the engine's reset, SRBC0 bit 28 */
		value = readl(cpm);
		writel(value | BIT(28), cpm);
		msleep(10);
		writel(readl(cpm) & ~BIT(28), cpm);
	}

	aip->mdev.minor = MISC_DYNAMIC_MINOR;
	aip->mdev.fops = &jz_aip_fops;
	aip->mdev.name = aip->name;

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf) {
		ret = -ENOMEM;
		goto err_irq;
	}
	aip->chainbuf = buf;
	buf->size = info->chainbuf_size[idx];
	buf->vaddr = dma_alloc_coherent(&pdev->dev, buf->size, (dma_addr_t *)&buf->paddr, GFP_KERNEL);
	if (!buf->vaddr) {
		ret = -ENOMEM;
		goto err_buf;
	}

	ret = misc_register(&aip->mdev);
	if (ret < 0) {
		dev_err(&pdev->dev, "request aip misc device failed!\n");
		goto err_dma;
	}

	*out = aip;
	return 0;

err_dma:
	dma_free_coherent(&pdev->dev, buf->size, buf->vaddr, (dma_addr_t)buf->paddr);
err_buf:
	kfree(buf);
err_irq:
	free_irq(aip->irq, aip);
err_iomem:
	iounmap(aip->iomem);
err_free:
	kfree(aip);
	return ret;
}

static int jz_aip_probe(struct platform_device *pdev)
{
	struct resource *res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	const struct jz_aip_info *info = of_device_get_match_data(&pdev->dev);
	void __iomem *cpm = NULL, *arb;
	struct device_node *np;
	struct aip_dev *ad;
	struct clk *clk;
	int i, ret;

	if (!res)
		return -ENXIO;

	ad = devm_kzalloc(&pdev->dev, sizeof(*ad), GFP_KERNEL);
	if (!ad)
		return -ENOMEM;

	/* the CCU: CSCR at +0 (written on open), MSCR */
	arb = ioremap(0x12200000, 0x100);
	if (!arb)
		return -ENXIO;

	if (info->cpm) {
		clk = devm_clk_get_enabled(&pdev->dev, "gate_aip");
		if (IS_ERR(clk)) {
			ret = PTR_ERR(clk);
			goto err_arb;
		}

		/* SRBC0 is a CPM register, in the CGU node's window */
		np = of_find_compatible_node(NULL, NULL, "ingenic,a1-cgu");
		cpm = np ? of_iomap(np, 0) : NULL;
		of_node_put(np);
		if (!cpm) {
			ret = -ENXIO;
			goto err_arb;
		}
		cpm += 0xf0;
	} else {
		/* T41: the AIP's clock is CCU MSCR bit 25 */
		writel(readl(arb + CCU_MSCR) | CCU_MSCR_AIP_CLK, arb + CCU_MSCR);
	}

	for (i = 0; i < JZ_AIP_NUM; i++) {
		ret = jz_aip_setup(pdev, res, info, cpm, arb, i, &ad->aip[i]);
		if (ret)
			goto err_engines;
	}

	platform_set_drvdata(pdev, ad);
	dev_info(&pdev->dev, "AIP driver %s\n", AIP_DRIVER_VERSION);
	return 0;

err_engines:
	while (--i >= 0)
		jz_aip_free(ad->aip[i]);
	if (cpm)
		iounmap(cpm - 0xf0);
err_arb:
	iounmap(arb);
	return ret;
}

static void jz_aip_remove(struct platform_device *pdev)
{
	struct aip_dev *ad = platform_get_drvdata(pdev);
	void __iomem *cpm = ad->aip[0]->cpm, *arb = ad->aip[0]->arb;
	int i;

	for (i = JZ_AIP_NUM - 1; i >= 0; i--)
		jz_aip_free(ad->aip[i]);
	iounmap(arb);
	if (cpm)
		iounmap(cpm - 0xf0);
}

static const struct jz_aip_info jz_aip_a1 = {
	.cpm = true,
	.chainbuf_size = { 8192, 8192, 8192 },
};

/* the vendor T41 driver's chain buffers: 1024 T, 1025 F and 1024 P entries */
static const struct jz_aip_info jz_aip_t41 = {
	.chainbuf_size = { 1024, 1025 * 104, 1024 * 216 },
	.chainbuf_tag = true,
	.any_irq = true,
};

static const struct of_device_id jz_aip_of_match[] = {
	{ .compatible = "ingenic,a1-aip", .data = &jz_aip_a1 },
	{ .compatible = "ingenic,t41-aip", .data = &jz_aip_t41 },
	{ }
};
MODULE_DEVICE_TABLE(of, jz_aip_of_match);

static struct platform_driver jz_aip_driver = {
	.probe		= jz_aip_probe,
	.remove		= jz_aip_remove,
	.driver		= {
		.name		= "jz-aip",
		.of_match_table	= jz_aip_of_match,
	},
};
module_platform_driver(jz_aip_driver);

MODULE_DESCRIPTION("JZ AIP Driver");
MODULE_AUTHOR("Danny<yiwen.han@ingenic.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("20210520");
