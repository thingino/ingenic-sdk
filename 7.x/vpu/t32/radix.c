#include <linux/module.h>
#include <linux/mm.h>
#include <linux/syscalls.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/of_irq.h>

#include "soc_vpu.h"
#include "hera.h"
#ifdef CONFIG_JZ_VPU_KERN_ENCODE
#include "../kernel_encode/include/ingenic_vpu.h"
struct completion *hera_done_for_kernEnc = NULL;
int hera_irq_for_kernEnc = 0;
int hera_irq_stat_for_kernEnc = 0;
#endif

//#define DUMP_HERA_REG
#define FPGA_VPU_CLK 100
static int vpu_clk = 0;
static int frame_cnt = 0;
static uint32_t t0, hw_time;
struct jz_vpu_radix {
	struct vpu          vpu;
	char                name[16];
	int                 irq;
	void __iomem        *iomem;
	void __iomem        *iomem_ivdc;
	struct clk          *clk_parent;
	struct clk          *clk;
	struct clk          *ahb1;
	struct clk          *clk_gate;
	struct clk          *ahb1_gate;
	struct clk          *ivdc_gate;
	enum jz_vpu_status  vpu_status;
	struct completion   done;
	spinlock_t          slock;
	struct mutex        mutex;
	pid_t               owner_pid;
	unsigned int        status;
	unsigned int        bslen;
	int                 is_ivdc;
	void*               cpm_pwc;
	unsigned int        cmpx;
};

static void dump_hera_regs(struct device *dev)
{
#ifdef DUMP_HERA_REG
	int i = 0;
	int offset = 0;
	struct jz_vpu_radix *vpu = dev_get_drvdata(dev);

#define DUMP_HERA_BASE 0x13200000
#define DUMP_IVDC_BASE 0x13100000
	for (i = 0; i < 0xfc; i ++) {
		offset = RADIX_CFGC_BASE + i * 4;
		printk("CFGC:::0x%08x = 0x%08x\n", DUMP_HERA_BASE + offset, vpu_readl(vpu, offset));
	}

	for (i = 0; i < 0x0c; i ++) {
		offset = RADIX_VDMA_BASE + i * 4;
		printk("VDMA:::0x%08x = 0x%08x\n", DUMP_HERA_BASE + offset, vpu_readl(vpu, offset));
	}

	for (i = 0; i < 0x67; i ++) {
		offset = RADIX_ODMA_BASE + i * 4;
		printk("ODMA:::0x%08x = 0x%08x\n", DUMP_HERA_BASE + offset, vpu_readl(vpu, offset));
	}

	for (i = 0; i < 0x3a0; i ++) {
		offset = RADIX_EFE_BASE + i * 4;
		printk("EFE:::0x%08x = 0x%08x\n", DUMP_HERA_BASE + offset, vpu_readl(vpu, offset));
	}

	for (i = 0; i < 0x10c; i ++) {
		offset = RADIX_JRFD_BASE + i * 4;
		printk("JRFD:::0x%08x = 0x%08x\n", DUMP_HERA_BASE + offset, vpu_readl(vpu, offset));
	}

	for (i = 0; i < 0x7c; i ++) {
		offset = RADIX_MCE_BASE + i * 4;
		printk("MCE:::0x%08x = 0x%08x\n", DUMP_HERA_BASE + offset, vpu_readl(vpu, offset));
	}
	for (i = 0x100; i < 0x1bc; i ++) {
		offset = RADIX_MCE_BASE + i * 4;
		printk("MCE:::0x%08x = 0x%08x\n", DUMP_HERA_BASE + offset, vpu_readl(vpu, offset));
	}

	for (i = 0; i < 0x80; i ++) {
		offset = RADIX_TFM_BASE + i * 4;
		printk("TFM:::0x%08x = 0x%08x\n", DUMP_HERA_BASE + offset, vpu_readl(vpu, offset));
	}

	for (i = 0; i < 0x290; i ++) {
		offset = RADIX_MD_BASE + i * 4;
		printk("MD:::0x%08x = 0x%08x\n", DUMP_HERA_BASE + offset, vpu_readl(vpu, offset));
	}

	for (i = 0; i < 0x8; i ++) {
		offset = RADIX_DT_BASE + i * 4;
		printk("DT:::0x%08x = 0x%08x\n", DUMP_HERA_BASE + offset, vpu_readl(vpu, offset));
	}

	for (i = 0; i < 0x98; i ++) {
		offset = RADIX_DBLK_BASE + i * 4;
		printk("DBLK:::0x%08x = 0x%08x\n", DUMP_HERA_BASE + offset, vpu_readl(vpu, offset));
	}
	for (i = 0x240; i < 0x248; i ++) {
		offset = RADIX_DBLK_BASE + i * 4;
		printk("DBLK:::0x%08x = 0x%08x\n", DUMP_HERA_BASE + offset, vpu_readl(vpu, offset));
	}

	for (i = 0x20; i < 0x34; i ++) {
		offset = RADIX_SAO_BASE + i * 4;
		printk("SAO:::0x%08x = 0x%08x\n", DUMP_HERA_BASE + offset, vpu_readl(vpu, offset));
	}

	for (i = 0; i < 0x8; i ++) {
		offset = RADIX_BC_BASE + i * 4;
		printk("BC:::0x%08x = 0x%08x\n", DUMP_HERA_BASE + offset, vpu_readl(vpu, offset));
	}

	for (i = 0; i < 0x68; i ++) {
		offset = RADIX_SDE_BASE + i * 4;
		printk("SDE:::0x%08x = 0x%08x\n", DUMP_HERA_BASE + offset, vpu_readl(vpu, offset));
	}

	for (i = 0; i < 0x13c; i ++) {
		offset = RADIX_IPRED_BASE + i * 4;
		printk("IPRED:::0x%08x = 0x%08x\n", DUMP_HERA_BASE + offset, vpu_readl(vpu, offset));
	}

	for (i = 0; i < 0x4; i ++) {
		offset = RADIX_STC_BASE + i * 4;
		printk("STC:::0x%08x = 0x%08x\n", DUMP_HERA_BASE + offset, vpu_readl(vpu, offset));
	}
	for (i = 0x1000; i < 0x1004; i ++) {
		offset = RADIX_STC_BASE + i * 4;
		printk("STC:::0x%08x = 0x%08x\n", DUMP_HERA_BASE + offset, vpu_readl(vpu, offset));
	}

	for (i = 0; i < 0x1d4; i ++) {
		offset = RADIX_VMAU_BASE + i * 4;
		printk("VMAU:::0x%08x = 0x%08x\n", DUMP_HERA_BASE + offset, vpu_readl(vpu, offset));
	}

	for (i = 0; i < 0x68; i ++) {
		offset = RADIX_EMC_BASE + i * 4;
		printk("EMC:::0x%08x = 0x%08x\n", DUMP_HERA_BASE + offset, vpu_readl(vpu, offset));
	}

	for (i = 0; i < 0x200; i ++) {
		offset = i * 4;
		printk("IVDC:::0x%08x = 0x%08x\n", DUMP_IVDC_BASE + offset, readl(vpu->iomem_ivdc + offset));
	}
#endif
}

static long vpu_reset(struct device *dev, struct channel_node * const cnode)
{
	struct jz_vpu_radix *vpu = dev_get_drvdata(dev);
	int timeout = 0x7ffff;
#if 0
	unsigned int srbc = cpm_inl(CPM_SRBC);
	cpm_set_bit(CPM_RADIX_STP(vpu->vpu.idx), CPM_SRBC);
	while (!(cpm_inl(CPM_SRBC) & (1 << CPM_RADIX_ACK(vpu->vpu.idx)) && --timeout)) {
		udelay(2);
	}

	if (timeout == 0) {
		dev_warn(vpu->vpu.dev, "[%d:%d] wait stop ack timeout\n",
				current->tgid, current->pid);
		cpm_outl(srbc, CPM_SRBC);
		return -1;
	} else {
		cpm_outl(srbc | (1 << CPM_RADIX_SR(vpu->vpu.idx)), CPM_SRBC);
		cpm_outl(srbc, CPM_SRBC);
	}
#else
	unsigned int cfgc_glb_ctrl = 1;

	vpu_writel(vpu, RADIX_CFGC_BASE+RADIX_REG_CFGC_GLB_CTRL, (0x1<<31) + 0xffff);//hera
	do {
		cfgc_glb_ctrl = vpu_readl(vpu,RADIX_CFGC_BASE+RADIX_REG_CFGC_GLB_CTRL);
		if (!cfgc_glb_ctrl) {
			break;
		}
		udelay(2);
	} while (--timeout);
	vpu_writel(vpu, RADIX_CFGC_BASE+RADIX_REG_CFGC_GLB_CTRL, 0xffff);//hera

	if (timeout == 0) {
		dev_warn(vpu->vpu.dev, "[%d:%d] wait stop ack timeout\n",
				current->tgid, current->pid);
		return -1;
	}
#endif
	return 0;
}

static long vpu_open(struct device *dev)
{
	struct jz_vpu_radix *vpu = dev_get_drvdata(dev);
	unsigned long slock_flag = 0;

	spin_lock_irqsave(&vpu->slock, slock_flag);
	enable_irq(vpu->irq);
	vpu->vpu_status = VPU_STATUS_OPEN;
	spin_unlock_irqrestore(&vpu->slock, slock_flag);

	clk_prepare_enable(vpu->ahb1_gate);
	clk_prepare_enable(vpu->ivdc_gate);
	clk_prepare_enable(vpu->clk_gate);
	clk_prepare_enable(vpu->clk);
	clk_prepare_enable(vpu->ahb1);
	dev_dbg(dev, "[%d:%d] open\n", current->tgid, current->pid);
#ifdef CONFIG_FPGA
	vpu_clk = FPGA_VPU_CLK;
#else
	vpu_clk = clk_get_rate(vpu->clk_gate) / 1000000;
#endif
	frame_cnt = 0;

	return 0;
}

static long vpu_release(struct device *dev)
{
	struct jz_vpu_radix *vpu = dev_get_drvdata(dev);
	unsigned long slock_flag = 0;

	clk_disable_unprepare(vpu->ahb1);
	clk_disable_unprepare(vpu->clk);
	clk_disable_unprepare(vpu->clk_gate);
	clk_disable_unprepare(vpu->ivdc_gate);
	clk_disable_unprepare(vpu->ahb1_gate);

	spin_lock_irqsave(&vpu->slock, slock_flag);
	disable_irq_nosync(vpu->irq);
	/* Clear completion use_count here to avoid a unhandled irq after vpu off */
	vpu->done.done = 0;
	vpu->vpu_status = VPU_STATUS_CLOSE;
	spin_unlock_irqrestore(&vpu->slock, slock_flag);

	dev_dbg(dev, "[%d:%d] close\n", current->tgid, current->pid);
	return 0;
}

static long vpu_start(struct device *dev, const struct channel_node * const cnode)
{
	struct jz_vpu_radix *vpu = dev_get_drvdata(dev);

	vpu_writel(vpu, RADIX_CFGC_BASE  + RADIX_REG_CFGC_GLB_CTRL, (0<<22) | (((1<<16) - 1) & 0xffff));
	vpu_writel(vpu, RADIX_CFGC_BASE  + RADIX_REG_CFGC_INTR_EN, RADIX_CFGC_INTE_IVDCRST | RADIX_CFGC_INTE_AMCERR | RADIX_CFGC_INTE_BSFULL | RADIX_CFGC_INTE_ENDF | RADIX_CFGC_INTE_TIMEOUT);
	vpu_writel(vpu, RADIX_CFGC_BASE + RADIX_REG_CFGC_VTYPE, cnode->vtype);
	/* enable timeout ctrl and set vpu timeout thrd to 1s */
	vpu_writel(vpu, RADIX_CFGC_BASE + RADIX_REG_CFGC_TIMEOUT_CTRL, 0x1);
	vpu_writel(vpu, RADIX_CFGC_BASE + RADIX_REG_CFGC_TIMEOUT_THRD, vpu_clk * 1000000 * 5);

	//ivdc add
	if (cnode->frame_type == FRAME_TYPE_IVDC) {
		vpu->is_ivdc = 1;
		writel(0x1, vpu->iomem_ivdc+0x78);//write runfifo
		writel(cnode->sensor_id << 8 | 0x1, vpu->iomem_ivdc+0x70);//write v0 start
	} else {
		vpu->is_ivdc = 0;
	}

#if defined(CONFIG_SOC_PRJ008) || defined(CONFIG_SOC_PRJ009) || defined(CONFIG_SOC_PRJ010)
	vpu_writel(vpu, RADIX_CFGC_BASE+ RADIX_REG_CFGC_EARB, (61 & 0xFF));
#endif

	// 0x90068
	vpu_writel(vpu, RADIX_EMC_BASE + RADIX_REG_EMC_ADDR_CHN, RADIX_VDMA_ACFG_DHA(cnode->dma_addr));
	vpu_writel(vpu,RADIX_CFGC_BASE+RADIX_REG_CFGC_ACM_CTRL, RADIX_VDMA_ACFG_RUN);
	dev_dbg(vpu->vpu.dev, "[%d:%d] start vpu ... \n", current->tgid, current->pid);
	return 0;
}

static long vpu_wait_complete(struct device *dev, struct channel_node * const cnode)
{
	long ret = 0;
	unsigned int v0_status = 0;
	struct jz_vpu_radix *vpu = dev_get_drvdata(dev);
hard_vpu_wait_restart:
	ret = wait_for_completion_interruptible_timeout(&vpu->done, msecs_to_jiffies(cnode->mdelay));
	if (ret > 0) {
		dev_dbg(vpu->vpu.dev, "[%d:%d] wait complete finish\n", current->tgid, current->pid);
		if (vpu->bslen > 0) {
			ret = 0;
		} else {
			ret = VPU_RET_OTHERS;
		}
		if (cnode->frame_type == FRAME_TYPE_IVDC) {
			v0_status = readl(vpu->iomem_ivdc+0x7c);
			if (v0_status & IVDC_CHECK) {
				//printk("overflow:0x%08x\n", v0_status);
				ret = VPU_RET_IVDC_CHECKSUM;
			} else if (((v0_status >> 4) & 0x7) != cnode->sensor_id) {
				//printk("need%d is not %d\n", cnode->sensor_id, ((v0_status >> 4) & 0x7));
				ret = VPU_RET_IVDC_SIDNOTATTACH;
			}
		}
	} else if (ret == -ERESTARTSYS) {
		dev_dbg(vpu->vpu.dev, "[%d:%d]:fun:%s,line:%d vpu is interrupt\n", current->tgid, current->pid, __func__, __LINE__);
		goto hard_vpu_wait_restart;
	} else {
		dev_warn(dev, "[%d:%d] wait_for_completion timeout\n", current->tgid, current->pid);
		dev_warn(dev, "vpu_stat = %x\n", vpu_readl(vpu, RADIX_CFGC_BASE + RADIX_REG_CFGC_STAT));
		dev_warn(dev, "ret = %ld, sensor id = %d\n", ret, cnode->sensor_id);
		dump_hera_regs(dev);
		dump_hera_regs(dev);
		ret = -1;
	}
	cnode->output_len = vpu->bslen;
	cnode->status = vpu->status;
	cnode->hw_time = hw_time;

	return ret;
}

static long vpu_wait_bs_complete(struct device *dev, struct channel_node * const cnode)
{
	int ret = 0;
	unsigned int vpu_stat;
	unsigned int v0_status = 0;
	struct jz_vpu_radix *vpu = dev_get_drvdata(dev);

hard_vpu_wait_bs_restart:
	ret = wait_for_completion_interruptible_timeout(&vpu->done, msecs_to_jiffies(cnode->mdelay));
	vpu_stat = vpu->status;
	if (ret > 0) {
		dev_dbg(vpu->vpu.dev, "[%d:%d] wait complete finish\n", current->tgid, current->pid);
		if (vpu->bslen > 0) {
			ret = 0;
			if (vpu_stat & RADIX_CFGC_INTE_BSFULL) {
				ret = VPU_RET_HERA_BSFULL;
			}
		} else {
			ret = VPU_RET_OTHERS;
		}
		if ((cnode->frame_type == FRAME_TYPE_IVDC) && (vpu_stat & RADIX_CFGC_INTE_ENDF)) {
		//if ((cnode->frame_type == FRAME_TYPE_IVDC)) {
			v0_status = readl(vpu->iomem_ivdc+0x7c);
			if (v0_status & IVDC_CHECK) {
				//printk("overflow:0x%08x\n", v0_status);
				ret = VPU_RET_IVDC_CHECKSUM;
			} else if (((v0_status >> 4) & 0x7) != cnode->sensor_id) {
				//printk("need%d is not %d\n", cnode->sensor_id, ((v0_status >> 4) & 0x7));
				ret = VPU_RET_IVDC_SIDNOTATTACH;
			}
		}
	} else if (ret == -ERESTARTSYS) {
		dev_dbg(vpu->vpu.dev, "[%d:%d]:fun:%s,line:%d vpu is interrupt\n", current->tgid, current->pid, __func__, __LINE__);
		goto hard_vpu_wait_bs_restart;
	} else {
		dev_warn(dev, "[%d:%d] wait_for_bs_completion timeout\n", current->tgid, current->pid);
		dev_warn(dev, "vpu_stat = %x\n", vpu_readl(vpu, RADIX_CFGC_BASE + RADIX_REG_CFGC_STAT));
		dev_warn(dev, "ret = %ld, sensor id = %d\n", ret, cnode->sensor_id);
		dump_hera_regs(dev);
		dump_hera_regs(dev);
		ret = -1;
	}
	cnode->output_len = vpu->bslen;
	cnode->status = vpu->status;
	cnode->hw_time = hw_time;

	return ret;
}

static long vpu_set_bsfull_reg(struct device *dev, struct channel_node * const cnode)
{
	unsigned int paddr;
	struct jz_vpu_radix *vpu = dev_get_drvdata(dev);

	vpu_writel(vpu, RADIX_EMC_BASE + 0x60, 1);

	return 0;
}

static long vpu_suspend(struct device *dev)
{
	return 0;
}

static long vpu_resume(struct device *dev)
{
	return 0;
}

static struct vpu_ops vpu_ops = {
	.owner		= THIS_MODULE,
	.open		= vpu_open,
	.release	= vpu_release,
	.start_vpu	= vpu_start,
	.wait_complete	= vpu_wait_complete,
	.wait_bs_complete = vpu_wait_bs_complete,
	.set_bsfull_reg = vpu_set_bsfull_reg,
	.reset		= vpu_reset,
	.suspend	= vpu_suspend,
	.resume		= vpu_resume,
};

static irqreturn_t vpu_interrupt(int irq, void *dev)
{
	struct jz_vpu_radix *vpu = dev;
	unsigned int vpu_stat;
	unsigned long vflag = 0;

	spin_lock_irqsave(&vpu->slock, vflag);
	/* read intr stat */
	vpu_stat = vpu_readl(vpu, RADIX_CFGC_BASE + RADIX_REG_CFGC_INTR_STAT);
#ifdef CONFIG_JZ_VPU_KERN_ENCODE
	if(vpu_stat != 0){
		hera_irq_stat_for_kernEnc = vpu_stat;
	}
#endif
	if (!(vpu_stat & RADIX_CFGC_INTE_BSFULL) && (vpu_stat)) {
		/* disable all intr */
		vpu_writel(vpu, RADIX_CFGC_BASE  + RADIX_REG_CFGC_INTR_EN, 0x0);
		/* clear intr stat */
		SET_VPU_BIT(vpu, RADIX_CFGC_BASE + RADIX_REG_CFGC_INTR_CLR, 0x7ff);
	}

	if(vpu_stat & RADIX_CFGC_INTE_ENDF) {
		vpu->bslen = vpu_readl(vpu, RADIX_REG_CFGC_BSLEN);
		vpu->status = vpu_stat;
		complete(&vpu->done);
	} else if (vpu_stat & RADIX_CFGC_INTE_BSFULL) {
		vpu_writel(vpu, RADIX_EMC_BASE + RADIX_REG_EMC_BSFULL_STAT, 2);
		//vpu->bslen = (vpu_readl(vpu, RADIX_EMC_BASE + 0x64) & 0xffff) * 1024; //bsfull size reg
		vpu->bslen = vpu_readl(vpu, RADIX_REG_CFGC_BSLEN);
		vpu->status = vpu_stat;
		SET_VPU_BIT(vpu, RADIX_CFGC_BASE + RADIX_REG_CFGC_INTR_CLR, 0x7ff);
		complete(&vpu->done);
	} else if ((vpu_stat & RADIX_CFGC_INTE_TIMEOUT) && vpu->is_ivdc) {
		//1. If encoding timeout occurs during the encoding process, IVDC will become unrecoverable, and subsequent transmissions will continue to experience issues.
		//2. If ISP fails to output frames, resulting in an encoding timeout, the encoding can resume normal after soft-resetting IVDC.
		vpu->bslen = 0;
		vpu->status = vpu_stat;
#ifndef DUMP_HERA_REG
		writel(0x3, vpu->iomem_ivdc+0x0); //ivdc software reset
		complete(&vpu->done);
#endif
		//printk(KERN_DEBUG"[%s][%d] status=%d\n", __func__, __LINE__, vpu_stat);
	} else if (vpu_stat == 0) {
		//TODO put vpu debug info into /proc/jz/vpu/vpu_info
		//printk(KERN_DEBUG"[%s][%d] status=%d\n", __func__, __LINE__, vpu_stat);
	} else {
		vpu->bslen = 0;
		vpu->status = vpu_stat;
		complete(&vpu->done);
	}

	/* debug hw time */
	t0 = vpu_readl(vpu, 0xa4);
	hw_time = t0 * 1024 / (vpu_clk * 1000);
	hw_time = hw_time == 0 ? 1 : hw_time;
	//printk(KERN_DEBUG"%d: hw_time = %d ms, vpu clk=%d\n", frame_cnt, hw_time, vpu_clk);
	frame_cnt++;
	spin_unlock_irqrestore(&vpu->slock, vflag);

	return IRQ_HANDLED;
}

static int vpu_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *regs;
	struct jz_vpu_radix *vpu;
	long rate;
	pdev->id = 0;
	vpu = kzalloc(sizeof(struct jz_vpu_radix), GFP_KERNEL);
	if (!vpu) {
		dev_err(&pdev->dev, "kzalloc vpu space failed\n");
		ret = -ENOMEM;
		goto err_kzalloc_vpu;
	}

	vpu->vpu.vpu_id = VPU_RADIX_ID | (1 << pdev->id);
	vpu->vpu.idx = 0;

	snprintf(vpu->name, sizeof(vpu->name), "radix%d", pdev->id);

	vpu->irq = platform_get_irq(pdev, 0);
	if(vpu->irq < 0) {
		dev_err(&pdev->dev, "get irq failed\n");
		ret = vpu->irq;
		goto err_get_vpu_irq;
	}

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		dev_err(&pdev->dev, "No iomem resource\n");
		ret = -ENXIO;
		goto err_get_vpu_resource;
	}

	vpu->iomem = ioremap(regs->start, resource_size(regs));
	if (!vpu->iomem) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENXIO;
		goto err_get_vpu_iomem;
	}

	vpu->iomem_ivdc = ioremap(IVDC_BASE_ADDR, 0x1000);
	if (!vpu->iomem_ivdc) {
		dev_err(&pdev->dev, "ioremap failed\n");
		ret = -ENXIO;
		goto err_get_vpu_iomem_ivdc;
	}

	vpu->ivdc_gate = devm_clk_get(&pdev->dev, "ivdc");
	if (IS_ERR(vpu->ivdc_gate)) {
		dev_err(&pdev->dev, "ivdc_gate get failed\n");
		ret = PTR_ERR(vpu->ivdc_gate);
		goto err_clk;
	}

	vpu->ahb1_gate = devm_clk_get(&pdev->dev, "ahb1");
	if (IS_ERR(vpu->ahb1_gate)) {
		dev_err(&pdev->dev, "ahb1_gate get failed\n");
		ret = PTR_ERR(vpu->ahb1_gate);
		goto err_clk;
	}

	/* the 7.x CGU's vpu clock is the mux, divider and gate in one */
	vpu->clk = devm_clk_get(&pdev->dev, "vpu");
	if (IS_ERR(vpu->clk)) {
		dev_err(&pdev->dev, "clk get failed\n");
		ret = PTR_ERR(vpu->clk);
		goto err_clk;
	}
	vpu->clk_gate = vpu->clk;

	vpu->ahb1 = devm_clk_get(&pdev->dev, "ispa");
	if (IS_ERR(vpu->ahb1)) {
		dev_err(&pdev->dev, "ahb1 get failed\n");
		ret = PTR_ERR(vpu->ahb1);
		goto err_clk;
	}

	/* the 7.x CGU only takes rates it can make exactly; the vendor kernel rounded */
	rate = clk_round_rate(vpu->clk, 500000000);
	clk_set_rate(vpu->clk, rate > 0 ? rate : 500000000);
	rate = clk_round_rate(vpu->ahb1, 500000000);
	clk_set_rate(vpu->ahb1, rate > 0 ? rate : 500000000);

	spin_lock_init(&vpu->slock);
	mutex_init(&vpu->mutex);
	init_completion(&vpu->done);

	ret = request_irq(vpu->irq, vpu_interrupt, IRQF_SHARED, vpu->name, vpu);
	if (ret < 0) {
		dev_err(&pdev->dev, "request_irq failed\n");
		goto err_vpu_request_irq;
	}
	disable_irq_nosync(vpu->irq);

	vpu->vpu_status = VPU_STATUS_CLOSE;
	vpu->vpu.dev = &pdev->dev;
	vpu->vpu.ops = &vpu_ops;

	if ((ret = vpu_register(&vpu->vpu.vlist)) < 0) {
		dev_err(&pdev->dev, "vpu_register failed\n");
		goto err_vpu_register;
	}
	platform_set_drvdata(pdev, vpu);

#ifdef CONFIG_JZ_VPU_KERN_ENCODE
	hera_done_for_kernEnc = &vpu->done;
	hera_irq_for_kernEnc = vpu->irq;
	//VPU_Kern_Init();
#endif
	printk("%s probe success\n", vpu->name);
	return 0;

err_vpu_register:
	free_irq(vpu->irq, vpu);
err_vpu_request_irq:
err_clk:
	iounmap(vpu->iomem_ivdc);
err_get_vpu_iomem_ivdc:
	iounmap(vpu->iomem);
err_get_vpu_iomem:
err_get_vpu_resource:
err_get_vpu_irq:
	kfree(vpu);
err_kzalloc_vpu:
	return ret;
}

static void vpu_remove(struct platform_device *dev)
{
	struct jz_vpu_radix *vpu = platform_get_drvdata(dev);

	vpu_unregister(&vpu->vpu.vlist);
	free_irq(vpu->irq, vpu);
	iounmap(vpu->iomem);
	iounmap(vpu->iomem_ivdc);
	kfree(vpu);
}

static const struct of_device_id jz_radix_dt_match[] = {
	{ .compatible = "ingenic,t32-radix" },
	{ }
};
MODULE_DEVICE_TABLE(of, jz_radix_dt_match);

static struct platform_driver jz_vpu_driver = {
	.probe		= vpu_probe,
	.remove		= vpu_remove,
	.driver		= {
		.name	= "jz_radix",
		.of_match_table	= jz_radix_dt_match,
	},
};

int radix_init(void)
{
	return platform_driver_register(&jz_vpu_driver);
}

void radix_exit(void)
{
	platform_driver_unregister(&jz_vpu_driver);
}
