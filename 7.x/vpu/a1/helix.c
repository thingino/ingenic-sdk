#include <linux/module.h>
#include <linux/mm.h>
#include <linux/syscalls.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>

#include "soc_vpu.h"
#include "helix.h"

//#define DUMP_HELIX_REG

struct jz_vpu_helix {
	struct vpu          vpu;
	char                name[16];
	int                 irq;
	void __iomem        *iomem;
	struct clk          *clk;
	struct clk          *clk_vc8000d;
	enum jz_vpu_status  vpu_status;
	struct completion   done;
	spinlock_t          slock;
	struct mutex        mutex;
	pid_t               owner_pid;
	unsigned int        status;
	unsigned int        bslen;
	void __iomem        *cpm;
	unsigned int        cmpx;
};

#define CPM_SRBC0	0xf0
#define CPM_OPCR	0x34
#define OPCR_IDLE	(0x1 << 31)

#define cpm_inl(vpu, off)		readl((vpu)->cpm + (off))
#define cpm_outl(vpu, val, off)		writel((val), (vpu)->cpm + (off))
#define cpm_set_bit(vpu, bit, off)	cpm_outl(vpu, cpm_inl(vpu, off) | BIT(bit), off)
#define cpm_clear_bit(vpu, bit, off)	cpm_outl(vpu, cpm_inl(vpu, off) & ~BIT(bit), off)

static long vpu_reset(struct device *dev)
{
	struct jz_vpu_helix *vpu = dev_get_drvdata(dev);
	int timeout = 0x7fff;
	unsigned int srbc = cpm_inl(vpu, CPM_SRBC0);

#ifndef CONFIG_FPGA_TEST
	cpm_set_bit(vpu, CPM_HELIX_STP, CPM_SRBC0);
	while (!(cpm_inl(vpu, CPM_SRBC0) & (1 << CPM_HELIX_ACK)) && --timeout) udelay(20);

	if (timeout == 0) {
		dev_warn(vpu->vpu.dev, "[%d:%d] wait stop ack timeout\n",
			 current->tgid, current->pid);
		cpm_outl(vpu, srbc, CPM_SRBC0);
		return -1;
	} else {
		cpm_outl(vpu, srbc | (1 << CPM_HELIX_SR), CPM_SRBC0);
		cpm_outl(vpu, srbc, CPM_SRBC0);
	}
#else
	cpm_outl(vpu, srbc | (1 << CPM_HELIX_SR), CPM_SRBC0);
	cpm_outl(vpu, srbc, CPM_SRBC0);
#endif
	return 0;
}

static long vpu_open(struct device *dev)
{
	struct jz_vpu_helix *vpu = dev_get_drvdata(dev);
	unsigned long slock_flag = 0;

	if (cpm_inl(vpu, CPM_OPCR) & OPCR_IDLE)
		return -EBUSY;

	spin_lock_irqsave(&vpu->slock, slock_flag);

	__asm__ __volatile__ (
			"mfc0  $2, $16,  7   \n\t"
			"ori   $2, $2, 0x340 \n\t"
			"andi  $2, $2, 0x3ff \n\t"
			"mtc0  $2, $16,  7  \n\t"
			"nop                  \n\t" : : : "$2");

	vpu_reset(dev);
	enable_irq(vpu->irq);
	vpu->vpu_status = VPU_STATUS_OPEN;
	spin_unlock_irqrestore(&vpu->slock, slock_flag);
	while(try_wait_for_completion(&vpu->done));

	dev_dbg(dev, "[%d:%d] open\n", current->tgid, current->pid);

	return 0;
}

static long vpu_release(struct device *dev)
{
	struct jz_vpu_helix *vpu = dev_get_drvdata(dev);
	unsigned long slock_flag = 0;

	spin_lock_irqsave(&vpu->slock, slock_flag);
	disable_irq_nosync(vpu->irq);

	__asm__ __volatile__ (
			"mfc0  $2, $16,  7   \n\t"
			"andi  $2, $2, 0xbf \n\t"
			"mtc0  $2, $16,  7  \n\t"
			"nop                  \n\t" : : : "$2");

	cpm_clear_bit(vpu, CPM_HELIX_SR, CPM_SRBC0);

	vpu->done.done = 0;
	vpu->vpu_status = VPU_STATUS_CLOSE;
	spin_unlock_irqrestore(&vpu->slock, slock_flag);

	dev_dbg(dev, "[%d:%d] close\n", current->tgid, current->pid);

	return 0;
}

#ifdef DUMP_HELIX_REG
void dump_helix_state(struct jz_vpu_helix *vpu, unsigned int base, unsigned int len)
{
	unsigned int i;
	for(i = 0; i <= len; i++) {
		printk("addr:0x%x, state:0x%x\n", base + (i << 2), vpu_readl(vpu, base + (i << 2)));
	}
}

void helix_show_internal_state(struct jz_vpu_helix *vpu)
{
	unsigned int cfgc_base = 0;
	unsigned int vdma_base = 0x10000;
	unsigned int efe_base = 0x40000;
	unsigned int vmau_base = 0x80000;
	unsigned int sde_base = 0x90000;

	printk("----------------helix_show_internal_state start------------\n");
	//CFGC
	printk("-----------CFGC_REG----------\n");
	dump_helix_state(vpu, cfgc_base, 32);

	//VDMA
	printk("-----------VDMA_REG-----------\n");
	dump_helix_state(vpu, vdma_base, 3);

	//EFE
	printk("-----------EFE_REG------------\n");
	dump_helix_state(vpu, efe_base, 15);

	//VMAU
	printk("-----------VMAU_REG------------\n");
	dump_helix_state(vpu, vmau_base, 31);

	//SDE
	printk("-----------SDE_REG-------------\n");
	dump_helix_state(vpu, sde_base, 7);
	printk("----------------helix_show_internal_state end------------\n");
}
#endif

static long vpu_start(struct device *dev, const struct channel_node * const cnode)
{
	struct jz_vpu_helix *vpu = dev_get_drvdata(dev);
	//struct channel_list *clist = list_entry(cnode->clist, struct channel_list, list);

#ifdef DUMP_HELIX_REG
	dev_info(vpu->vpu.dev, "------%s(%d)helix_show_internal_state start------\n", __func__, __LINE__);
	helix_show_internal_state(vpu);
	dev_info(vpu->vpu.dev, "------%s(%d)helix_show_internal_state end------\n", __func__, __LINE__);
#endif

    vpu_writel(vpu, REG_SCH_GLBC, SCH_GLBC_HIAXI | SCH_INTE_RESERR | SCH_INTE_ACFGERR
            | SCH_INTE_BSERR | SCH_INTE_ENDF);

/*	vpu_writel(vpu, REG_VDMA_TASKRG, VDMA_ACFG_DHA(cnode->dma_addr)
			| VDMA_ACFG_RUN);
*/
	vpu_writel(vpu, REG_VDMA_TASKRG_T21, VDMA_ACFG_DHA(cnode->dma_addr)
			| VDMA_ACFG_RUN);
	dev_dbg(vpu->vpu.dev, "[%d:%d] start vpu\n", current->tgid, current->pid);

	return 0;
}

static long vpu_wait_complete(struct device *dev, struct channel_node * const cnode)
{
	long ret = 0, bsreadcnt = 3;
	struct jz_vpu_helix *vpu = dev_get_drvdata(dev);

hard_vpu_wait_restart:
	ret = wait_for_completion_interruptible_timeout(&vpu->done, msecs_to_jiffies(cnode->mdelay));
	if ((ret > 0) && (cnode->codecdir == HWJPEGDEC)) {
		//vpu->bslen = vpu_readl(vpu, REG_JPGC_MCUS);
		vpu->cmpx = 0;
	}
	while ((bsreadcnt-- > 0) && (ret > 0) && (vpu->bslen == 0) && (cnode->codecdir == HWJPEGENC)) {
		if (cnode->codecdir == HWJPEGDEC) {
			//vpu->bslen = vpu_readl(vpu, REG_JPGC_MCUS);
			vpu->cmpx = 0;
		} else if (cnode->codecdir == HWJPEGENC) {
			vpu->bslen = vpu_readl(vpu, REG_JPGC_STAT) & 0xffffff;
			vpu->cmpx = 0;
		} else if (cnode->codecdir == HWH264ENC) {
			vpu->bslen = vpu_readl(vpu, REG_SDE_CFG9);
			vpu->cmpx = vpu_readl(vpu, REG_EFE_SSAD);
		}
		if (vpu->bslen == 0) {
			msleep(2);
			continue;
		}
	}
	if ((ret > 0) && (vpu->bslen || cnode->codecdir == HWJPEGDEC)) {
		ret = 0;
		dev_dbg(vpu->vpu.dev, "[%d:%d] wait complete finish\n", current->tgid, current->pid);
	} else if (ret == -ERESTARTSYS) {
		dev_dbg(vpu->vpu.dev, "[%d:%d]:fun:%s,line:%d vpu is interrupt\n", current->tgid, current->pid, __func__, __LINE__);
		goto hard_vpu_wait_restart;
	} else {
		dev_warn(dev, "[%d:%d] wait_for_completion timeout\n", current->tgid, current->pid);
		dev_warn(dev, "vpu_stat = %x\n", vpu_readl(vpu,REG_SCH_STAT));
		dev_warn(dev, "vdma_task = %x\n", vpu_readl(vpu,REG_VDMA_TASKST));
		dev_warn(dev, "ret = %ld\n", ret);
#ifdef DUMP_HELIX_REG
		dev_info(vpu->vpu.dev, "------%s(%d)helix_show_internal_state start------\n", __func__, __LINE__);
		helix_show_internal_state(vpu);
		dev_info(vpu->vpu.dev, "------%s(%d)helix_show_internal_state end------\n", __func__, __LINE__);
#endif
		if (vpu_reset(dev) < 0) {
			dev_warn(dev, "vpu reset failed\n");
		}
		ret = -1;
	}
	cnode->output_len = vpu->bslen;
	cnode->status = vpu->status;
	cnode->cmpx = vpu->cmpx;

	//dev_info(dev, "[file:%s,fun:%s,line:%d] ret = %ld, status = %x, bslen = %d, cnode->cmpx=%d\n", __FILE__, __func__, __LINE__, ret, cnode->status, cnode->output_len, cnode->cmpx);

	return ret;
}

static long vpu_suspend(struct device *dev)
{
	return 0;
}

static long vpu_resume(struct device *dev)
{
	return 0;
}

/* called under the channel locks: the clock is prepared at probe, only the gate toggles */
static long vc8kd_clk_on(struct device *dev)
{
	struct jz_vpu_helix *vpu = dev_get_drvdata(dev);
	clk_enable(vpu->clk_vc8000d);
	return 0;
}

static long vc8kd_clk_off(struct device *dev)
{
	struct jz_vpu_helix *vpu = dev_get_drvdata(dev);
	clk_disable(vpu->clk_vc8000d);
	return 0;
}

static struct vpu_ops vpu_ops = {
	.owner		= THIS_MODULE,
	.open		= vpu_open,
	.release	= vpu_release,
	.start_vpu	= vpu_start,
	.wait_complete	= vpu_wait_complete,
	.reset		= vpu_reset,
	.suspend	= vpu_suspend,
	.resume		= vpu_resume,
	.vc8kd_clk_on = vc8kd_clk_on,
	.vc8kd_clk_off = vc8kd_clk_off,
};

static irqreturn_t vpu_interrupt(int irq, void *dev)
{
	struct jz_vpu_helix *vpu = dev;
	unsigned int vpu_stat;

	vpu_stat = vpu_readl(vpu,REG_SCH_STAT);
	spin_lock(&vpu->slock);
	CLEAR_VPU_BIT(vpu, REG_SCH_GLBC, SCH_INTE_MASK);
	if(vpu_stat) {
		if(vpu_stat & SCH_STAT_ENDFLAG) {
			if(vpu_stat & SCH_STAT_JPGEND) {
				dev_dbg(vpu->vpu.dev, "JPG successfully done!\n");
				vpu->bslen = vpu_readl(vpu, REG_JPGC_STAT) & 0xffffff;
				vpu->cmpx = 0;
				CLEAR_VPU_BIT(vpu,REG_JPGC_STAT,JPGC_STAT_ENDF);
			} else {
				dev_dbg(vpu->vpu.dev, "SCH successfully done!\n");
				vpu->bslen = vpu_readl(vpu, REG_SDE_CFG9);
				vpu->cmpx = vpu_readl(vpu, REG_EFE_SSAD);
				CLEAR_VPU_BIT(vpu,REG_SDE_STAT,SDE_STAT_BSEND);
				CLEAR_VPU_BIT(vpu,REG_DBLK_GSTA,DBLK_STAT_DOEND);
			}
			vpu->status = vpu_stat;
			complete(&vpu->done);
		} else {
			check_vpu_status(SCH_STAT_ORESERR, "out fo resolution error!\n");
			check_vpu_status(SCH_STAT_BSERR, "BS error!\n");
			check_vpu_status(SCH_STAT_ACFGERR, "ACFG error!\n");
			vpu->bslen = 0;
			vpu->status = vpu_stat;
			vpu->cmpx = 0;
			complete(&vpu->done);
		}
	}

	spin_unlock(&vpu->slock);

	return IRQ_HANDLED;
}

static int vpu_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *regs;
	struct device_node *cgu;
	struct jz_vpu_helix *vpu;

	vpu = kzalloc(sizeof(struct jz_vpu_helix), GFP_KERNEL);
	if (!vpu)
		return -ENOMEM;

	vpu->vpu.vpu_id = VPU_HELIX_ID | 1;
	vpu->vpu.idx = 0;
	snprintf(vpu->name, sizeof(vpu->name), "helix%d", vpu->vpu.idx);

	vpu->irq = platform_get_irq(pdev, 0);
	if (vpu->irq < 0) {
		dev_err(&pdev->dev, "get irq failed\n");
		ret = vpu->irq;
		goto err_free;
	}

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		dev_err(&pdev->dev, "No iomem resource\n");
		ret = -ENXIO;
		goto err_free;
	}

	vpu->iomem = ioremap(regs->start, resource_size(regs));
	if (!vpu->iomem) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENXIO;
		goto err_free;
	}

	/* SRBC0 (reset) and OPCR (idle) are CPM registers, the CGU node's window */
	cgu = of_find_compatible_node(NULL, NULL, "ingenic,a1-cgu");
	vpu->cpm = cgu ? of_iomap(cgu, 0) : NULL;
	of_node_put(cgu);
	if (!vpu->cpm) {
		dev_err(&pdev->dev, "no CPM registers\n");
		ret = -ENXIO;
		goto err_iomem;
	}

	vpu->clk_vc8000d = devm_clk_get(&pdev->dev, "gate_vc8000d");
	if (IS_ERR(vpu->clk_vc8000d)) {
		ret = PTR_ERR(vpu->clk_vc8000d);
		goto err_cpm;
	}
	vpu->clk = devm_clk_get(&pdev->dev, "gate_jpeg");
	if (IS_ERR(vpu->clk)) {
		ret = PTR_ERR(vpu->clk);
		goto err_cpm;
	}
	ret = clk_prepare_enable(vpu->clk_vc8000d);
	if (ret)
		goto err_cpm;
	ret = clk_prepare_enable(vpu->clk);
	if (ret)
		goto err_vc8kd;

	spin_lock_init(&vpu->slock);
	mutex_init(&vpu->mutex);
	init_completion(&vpu->done);

	ret = request_irq(vpu->irq, vpu_interrupt, 0, vpu->name, vpu);
	if (ret < 0) {
		dev_err(&pdev->dev, "request_irq failed\n");
		goto err_clk;
	}
	disable_irq_nosync(vpu->irq);

	vpu->vpu_status = VPU_STATUS_CLOSE;
	vpu->vpu.dev = &pdev->dev;
	vpu->vpu.ops = &vpu_ops;
	platform_set_drvdata(pdev, vpu);

	ret = vpu_register(&vpu->vpu.vlist);
	if (ret < 0) {
		dev_err(&pdev->dev, "vpu_register failed\n");
		goto err_irq;
	}

	return 0;

err_irq:
	free_irq(vpu->irq, vpu);
err_clk:
	clk_disable_unprepare(vpu->clk);
err_vc8kd:
	clk_disable_unprepare(vpu->clk_vc8000d);
err_cpm:
	iounmap(vpu->cpm);
err_iomem:
	iounmap(vpu->iomem);
err_free:
	kfree(vpu);
	return ret;
}

static void vpu_remove(struct platform_device *dev)
{
	struct jz_vpu_helix *vpu = platform_get_drvdata(dev);

	vpu_unregister(&vpu->vpu.vlist);
	free_irq(vpu->irq, vpu);
	clk_disable_unprepare(vpu->clk);
	clk_disable_unprepare(vpu->clk_vc8000d);
	iounmap(vpu->cpm);
	iounmap(vpu->iomem);
	kfree(vpu);
}

static const struct of_device_id helix_of_match[] = {
	{ .compatible = "ingenic,a1-helix" },
	{ }
};
MODULE_DEVICE_TABLE(of, helix_of_match);

static struct platform_driver jz_vpu_driver = {
	.probe		= vpu_probe,
	.remove		= vpu_remove,
	.driver		= {
		.name		= "helix",
		.of_match_table	= helix_of_match,
	},
};

int soc_channel_init(void);
void soc_channel_exit(void);

/* soc_channel_init() needs one registered vpu, so the platform driver goes first */
static int __init vpu_init(void)
{
	int ret = platform_driver_register(&jz_vpu_driver);

	if (ret)
		return ret;
	ret = soc_channel_init();
	if (ret)
		platform_driver_unregister(&jz_vpu_driver);
	return ret;
}

static void __exit vpu_exit(void)
{
	soc_channel_exit();
	platform_driver_unregister(&jz_vpu_driver);
}

module_init(vpu_init);
module_exit(vpu_exit);

MODULE_DESCRIPTION("A1 VPU helix (JPEG) driver");
MODULE_AUTHOR("Justin <ptkang@ingenic.cn>");
MODULE_LICENSE("GPL v2");
