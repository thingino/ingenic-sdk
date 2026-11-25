#include <linux/module.h>
#include <linux/mm.h>
#include <linux/syscalls.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>
#include <linux/of_irq.h>

#include "soc_vpu.h"
#include "helix.h"

//#define DUMP_HELIX_REG
#define FPGA_VPU_CLK 100
static int vpu_clk = 0;
static int frame_cnt = 0;
static uint32_t t0, hw_time;

struct jz_vpu_helix {
	struct vpu          vpu;
	char                name[16];
	int                 irq;
	void __iomem        *iomem;
	void __iomem        *iomem_ivdc;
	struct clk          *clk_parent;
	struct clk          *clk;
	struct clk          *clk_gate;
	struct clk          *ahb1;
	struct clk          *ahb1_gate;
	struct clk          *ivdc_gate;
	enum jz_vpu_status  vpu_status;
	struct completion   done;
	spinlock_t          slock;
	struct mutex        mutex;
	pid_t               owner_pid;
	unsigned int        status;
	unsigned int        bslen;
	void*               cpm_pwc;
	unsigned int        cmpx;
	unsigned int        codecdir;
};

static long vpu_reset(struct device *dev, struct channel_node * const cnode)
{
	struct jz_vpu_helix *vpu = dev_get_drvdata(dev);
	int timeout = 0x7fff;

#if 0
	vpu->codecdir = cnode->codecdir;
	//To be able to normally stop JPEG, you need to disable the JPEG interrupt firstly.
	CLEAR_VPU_BIT(vpu, JPEG_CTRL, 1 << JPEG_FRAME_END_INTR_EN);
	unsigned int srbc = cpm_inl(CPM_SRBC);
	cpm_set_bit(CPM_HELIX_STP(vpu->vpu.idx), CPM_SRBC);
	while (!(cpm_inl(CPM_SRBC) & (1 << CPM_HELIX_ACK(vpu->vpu.idx))) && --timeout) udelay(2);

	if (timeout == 0) {
		dev_warn(vpu->vpu.dev, "[%d:%d] wait stop ack timeout\n",
				current->tgid, current->pid);
		cpm_outl(srbc, CPM_SRBC);
		return -1;
	} else {
		cpm_outl(srbc | (1 << CPM_HELIX_SR(vpu->vpu.idx)), CPM_SRBC);
		cpm_outl(srbc, CPM_SRBC);
	}
#else
	vpu->codecdir = cnode->codecdir;
	if (cnode->codecdir == HWJPEGDEC) {
		vpu_writel(vpu, JPEG_TRIG, 1<<JPEG_SEQ_RESET | 1 << 9);
	} else {
		vpu_writel(vpu, JPEG_TRIG, 1<<JPEG_SEQ_RESET | 0 << 9);
	}
	while (!(vpu_readl(vpu, JPEG_TRIG) & (1 << RESET_END)) && --timeout){
		udelay(2);
	}
	if (timeout == 0) {
		dev_warn(vpu->vpu.dev, "[%d:%d] wait reset end timeout\n", current->tgid, current->pid);
		return -1;
	}
#endif
	return 0;
}

static long vpu_open(struct device *dev)
{
	struct jz_vpu_helix *vpu = dev_get_drvdata(dev);
	unsigned long slock_flag = 0;

	clk_prepare_enable(vpu->ahb1_gate);
	clk_prepare_enable(vpu->ivdc_gate);
	clk_prepare_enable(vpu->clk_gate);
	clk_prepare_enable(vpu->clk);
	clk_prepare_enable(vpu->ahb1);
	spin_lock_irqsave(&vpu->slock, slock_flag);
	enable_irq(vpu->irq);
	vpu->vpu_status = VPU_STATUS_OPEN;
	spin_unlock_irqrestore(&vpu->slock, slock_flag);
	while(try_wait_for_completion(&vpu->done));
#ifdef CONFIG_FPGA
	vpu_clk = FPGA_VPU_CLK;
#else
	vpu_clk = clk_get_rate(vpu->clk_gate) / 1000000;
#endif
	frame_cnt = 0;
	t0 = 0;hw_time = 0;
	dev_dbg(dev, "[%d:%d] open\n", current->tgid, current->pid);

	return 0;
}

static long vpu_release(struct device *dev)
{
	struct jz_vpu_helix *vpu = dev_get_drvdata(dev);
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
	int i = 0;
	struct jz_vpu_helix *vpu = dev_get_drvdata(dev);
	struct jpeg_reg_info *chn = (struct jpeg_reg_info *)phys_to_virt((unsigned long)cnode->dma_addr);

	//jpeg v1.2
	if (cnode->codecdir == HWJPEGENC) {
		for (i = 0; i < QMEM_LEN; i ++) {
			vpu_writel(vpu, JPEG_QT_BASE_TAB + 4 * i, chn->qmem[i]);
		}
		vpu_writel(vpu, JPEG_CTRL, chn->jpeg_ctrl);
		vpu_writel(vpu, JPEG_NMCU, chn->nmcu);
		vpu_writel(vpu, JPEG_FRM_INFO, chn->frm_info);
		vpu_writel(vpu, JPEG_LINE_STRD, chn->stride);
		vpu_writel(vpu, JPEG_RAWY_ADDR, chn->y_addr);
		vpu_writel(vpu, JPEG_RAWC_ADDR, chn->c_addr);
		vpu_writel(vpu, JPEG_BS_ADDR, chn->bs_addr);
#if defined(CONFIG_SOC_PRJ008) || defined(CONFIG_SOC_PRJ009) || defined(CONFIG_SOC_PRJ010)
		if (chn->jpeg_ctrl & (1 << CTRL_BSFULL_INTR_EN)) {
			vpu_writel(vpu, JPEG_BS1_ADDR, chn->bs_addr + (chn->bsfull_jpeg_intr_size * 1024));
			vpu_writel(vpu, JPEG_BSFULL_EN, 0x1);
			vpu_writel(vpu, JPEG_BSFULL_SIZE, chn->bsfull_jpeg_intr_size);
		}
#endif
		vpu_writel(vpu, JPEG_CODE_PIX_TOTAL, chn->pixel_num);
		vpu_writel(vpu, JPEG_MARKERS, chn->marker_en << MARKERS_ENABLE | (chn->marker_num - 1) << MARKERS_NUMBER);
		vpu_writel(vpu, JPEG_LIMIT_BS_LEN_VAL, chn->limit_bs_len);
		//ivdc add
		if (cnode->frame_type == FRAME_TYPE_IVDC) {
			writel(0x1, vpu->iomem_ivdc+0x90);
			writel(cnode->sensor_id << 8 | 0x1, vpu->iomem_ivdc+0x88);
		}
		vpu_writel(vpu, JPEG_TRIG, vpu_readl(vpu, JPEG_TRIG) | (1 << JPEG_START));
	} else if (cnode->codecdir == HWJPEGDEC){
		for (i = 0; i < QMEM_LEN; i++) {
			vpu_writel(vpu, JPEG_QT_BASE_TAB + 4 * i, chn->qmem[i]);
		}
		vpu_writel(vpu, JPEG_CTRL, chn->jpeg_ctrl);
		vpu_writel(vpu, JPEG_NMCU, chn->nmcu);
		vpu_writel(vpu, JPEG_FRM_INFO, chn->frm_info);
		vpu_writel(vpu, JPEG_LINE_STRD, chn->stride);
		vpu_writel(vpu, JPEG_RAWY_ADDR, chn->y_addr);
		vpu_writel(vpu, JPEG_BS_ADDR, chn->bs_addr);
		vpu_writel(vpu, JPEG_CODE_PIX_TOTAL, chn->pixel_num);
		vpu_writel(vpu, JPEG_DECODE_MODE, chn->decode_mode);
		vpu_writel(vpu, JPEG_MARKERS, chn->marker_en << MARKERS_ENABLE | chn->marker_num << MARKERS_NUMBER);
		vpu_writel(vpu, JPEG_TRIG, vpu_readl(vpu, JPEG_TRIG) | (1 << 9) | (1 << JPEG_START));
	}
	dev_dbg(vpu->vpu.dev, "[%d:%d] start vpu\n", current->tgid, current->pid);

	return 0;
}

static long vpu_wait_complete(struct device *dev, struct channel_node * const cnode)
{
	long ret = 0;
	unsigned int v1_status = 0;
	struct jz_vpu_helix *vpu = dev_get_drvdata(dev);

hard_vpu_wait_restart:
	ret = wait_for_completion_interruptible_timeout(&vpu->done, msecs_to_jiffies(cnode->mdelay));
	if (ret > 0) {
		dev_dbg(vpu->vpu.dev, "[%d:%d] wait complete finish\n", current->tgid, current->pid);
		if (vpu->bslen > 0) {
			ret = 0;
		} else {
			ret = VPU_RET_OTHERS;
		}
		//In direct mode. must read done fifo no matter what error occurs.
		if (cnode->frame_type == FRAME_TYPE_IVDC) {
			v1_status = readl(vpu->iomem_ivdc+0x94);
			if (v1_status & IVDC_CHECK) {
				//printk("overflow:0x%08x\n", v1_status);
				ret = VPU_RET_IVDC_CHECKSUM;
			} else if (((v1_status >> 4) & 0x7) != cnode->sensor_id) {
				//printk("need%d is not %d\n", cnode->sensor_id, ((v1_status >> 4) & 0x7));
				ret = VPU_RET_IVDC_SIDNOTATTACH;
			}
		}
		if (vpu->status & (0x1 << JPEG_FRAME_END_BSLESS_INTR)) {
			ret = VPU_RET_JPEG_BSLESS;
		}
	} else if (ret == -ERESTARTSYS) {
		dev_dbg(vpu->vpu.dev, "[%d:%d]:fun:%s,line:%d vpu is interrupt\n", current->tgid, current->pid, __func__, __LINE__);
		goto hard_vpu_wait_restart;
	} else {
		dev_warn(dev, "[%d:%d] wait_for_completion timeout\n", current->tgid, current->pid);
		dev_warn(dev, "ret = %ld, sensor id = %d\n", ret, cnode->sensor_id);
		ret = -1;
	}
	cnode->output_len = vpu->bslen;
	cnode->status = vpu->status;
	cnode->cmpx = vpu->cmpx;
	cnode->hw_time = hw_time;

	return ret;
}

#if defined(CONFIG_SOC_PRJ008) || defined(CONFIG_SOC_PRJ009) || defined(CONFIG_SOC_PRJ010)
static long vpu_wait_bs_complete(struct device *dev, struct channel_node * const cnode)
{
	unsigned int vpu_status;
	long ret = 0;
	unsigned int v1_status = 0;
	struct jz_vpu_helix *vpu = dev_get_drvdata(dev);

hard_vpu_wait_bs_restart:
	ret = wait_for_completion_interruptible_timeout(&vpu->done, msecs_to_jiffies(cnode->mdelay));
	vpu_status = vpu->status;
	if (ret > 0) {
		dev_dbg(vpu->vpu.dev, "[%d:%d] wait bs complete finish\n", current->tgid, current->pid);
		if (vpu->bslen > 0) {
			ret = 0;
			if (vpu_status & (0x1 << JPEG_BSFULL_INTR_STAT)) {
				ret = VPU_RET_HERA_BSFULL;
			}
		} else {
			ret = VPU_RET_OTHERS;
		}

		if (cnode->frame_type == FRAME_TYPE_IVDC && (vpu_status & (1 << JPEG_FRAME_END_INTR))) {
			v1_status = readl(vpu->iomem_ivdc+0x94);
			if (v1_status & IVDC_CHECK) {
				//printk("overflow:0x%08x\n", v1_status);
				ret = VPU_RET_IVDC_CHECKSUM;
			} else if (((v1_status >> 4) & 0x7) != cnode->sensor_id) {
				//printk("need%d is not %d\n", cnode->sensor_id, ((v1_status >> 4) & 0x7));
				ret = VPU_RET_IVDC_SIDNOTATTACH;
			}
		}
	} else if (ret == -ERESTARTSYS) {
		dev_dbg(vpu->vpu.dev, "[%d:%d]:fun:%s,line:%d vpu is interrupt\n", current->tgid, current->pid, __func__, __LINE__);
		goto hard_vpu_wait_bs_restart;
	} else {
		dev_warn(dev, "[%d:%d] wait_for_completion timeout\n", current->tgid, current->pid);
		dev_warn(dev, "ret = %ld, sensor id = %d\n", ret, cnode->sensor_id);
		ret = -1;
	}
	cnode->output_len = vpu->bslen;
	cnode->status = vpu->status;
	cnode->cmpx = vpu->cmpx;
	cnode->hw_time = hw_time;

	return ret;
}

static long vpu_set_bsfull_reg(struct device *dev, struct channel_node * const cnode)
{
	struct jz_vpu_helix *vpu = dev_get_drvdata(dev);

	vpu_writel(vpu, JPEG_BSFULL_STATE, 0x1);
	return 0;
}
#endif

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
#if defined(CONFIG_SOC_PRJ008) || defined(CONFIG_SOC_PRJ009) || defined(CONFIG_SOC_PRJ010)
	.wait_bs_complete = vpu_wait_bs_complete,
	.set_bsfull_reg = vpu_set_bsfull_reg,
#endif
	.reset		= vpu_reset,
	.suspend	= vpu_suspend,
	.resume		= vpu_resume,
};

static irqreturn_t vpu_interrupt(int irq, void *dev)
{
	struct jz_vpu_helix *vpu = dev;
	unsigned int vpu_stat;

	vpu_stat = vpu_readl(vpu,JPEG_INTR_STA);
	spin_lock(&vpu->slock);
	SET_VPU_BIT(vpu,JPEG_INTR_CLR,0xf);
	//CLEAR_VPU_BIT(vpu, REG_SCH_GLBC, SCH_INTE_MASK);
	if(vpu_stat) {
		if (vpu_stat & (1 << JPEG_FRAME_END_BSLESS_INTR)) {
			vpu->bslen = 0;
			vpu->status = vpu_stat;
			complete(&vpu->done);
		} else if (vpu_stat & (1 << IVDC_STOP_REQ_INTR)) {
			//printk("recieve irq IVDC_STOP_REQ_INTR!\n");
			vpu->bslen = 0;
			complete(&vpu->done);
		} else if (vpu_stat & (1 << JPEG_TIMEOUT_INTR)) {
			//printk("recieve irq JPEG_TIMEOUT_INTR!\n");
			vpu->bslen = 0;
			complete(&vpu->done);
		}
#if defined(CONFIG_SOC_PRJ008) || defined(CONFIG_SOC_PRJ009) || defined(CONFIG_SOC_PRJ010)
		else if(vpu_stat & (1 << JPEG_BSFULL_INTR_STAT)) {
			vpu->bslen = vpu_readl(vpu, JPEG_BSBI_BS_LEN);
			vpu->cmpx = 0;
			vpu->status = vpu_stat;
			complete(&vpu->done);
			SET_VPU_BIT(vpu, JPEG_BSFULL_STATE, 0x2);
			SET_VPU_BIT(vpu, JPEG_INTR_CLR, 0x1F);
		}
#endif
		else if (vpu_stat & (1 << JPEG_FRAME_END_INTR)) {
			dev_dbg(vpu->vpu.dev, "JPG successfully done!\n");
			if (vpu->codecdir == HWJPEGENC) {
				vpu->bslen = vpu_readl(vpu, JPEG_GLB_BS_LEN);
			} else if (vpu->codecdir == HWJPEGDEC) {
				vpu->bslen = vpu_readl(vpu, 0x16c);
			}
			vpu->cmpx = 0;
			vpu->status = vpu_stat;
			complete(&vpu->done);
		} else if (vpu_stat == 0) {
			//TODO put vpu debug info into /proc/jz/vpu/vpu_info
			//printk(KERN_DEBUG"[%s][%d] status=%d\n", __func__, __LINE__, vpu_stat);
		} else {
			vpu->bslen = 0;
			vpu->status = vpu_stat;
			vpu->cmpx = 0;
			complete(&vpu->done);
		}
	}
	t0 = vpu_readl(vpu, 0x244);
	hw_time = t0 / (vpu_clk * 1000);
	hw_time = hw_time == 0 ? 1 : hw_time;
	//printk(KERN_DEBUG"%d: hw_time = %d ms\n", frame_cnt, hw_time);
	frame_cnt++;
	spin_unlock(&vpu->slock);

	return IRQ_HANDLED;
}

static int vpu_probe(struct platform_device *pdev)
{
	int ret;
	struct resource	*regs;
	struct jz_vpu_helix *vpu;
	long rate;

	pdev->id = 0;
	vpu = kzalloc(sizeof(struct jz_vpu_helix), GFP_KERNEL);
	if (!vpu) {
		dev_err(&pdev->dev, "kzalloc vpu space failed\n");
		ret = -ENOMEM;
		goto err_kzalloc_vpu;
	}

	vpu->vpu.vpu_id = VPU_HELIX_ID | (1 << pdev->id);
	vpu->vpu.idx = pdev->id;
	snprintf(vpu->name, sizeof(vpu->name), "helix%d", pdev->id);

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
	printk("%s success\n", vpu->name);
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
	struct jz_vpu_helix *vpu = platform_get_drvdata(dev);

	vpu_unregister(&vpu->vpu.vlist);
	free_irq(vpu->irq, vpu);
	iounmap(vpu->iomem);
	iounmap(vpu->iomem_ivdc);
	kfree(vpu);
}

static const struct of_device_id jz_helix_dt_match[] = {
	{ .compatible = "ingenic,t32-helix" },
	{ .compatible = "ingenic,t33-helix" },
	{ }
};
MODULE_DEVICE_TABLE(of, jz_helix_dt_match);

static struct platform_driver jz_vpu_driver = {
	.probe		= vpu_probe,
	.remove		= vpu_remove,
	.driver		= {
		.name	= "jz_helix",
		.of_match_table = jz_helix_dt_match,
	},
};

int radix_init(void);
void radix_exit(void);

static int __init vpu_init(void)
{
	int ret = platform_driver_register(&jz_vpu_driver);

	if (ret)
		return ret;
	ret = radix_init();
	if (ret)
		goto err_helix;
	/* the channel device needs both VPUs registered first */
	ret = soc_channel_init();
	if (ret)
		goto err_radix;
	return 0;
err_radix:
	radix_exit();
err_helix:
	platform_driver_unregister(&jz_vpu_driver);
	return ret;
}

static void __exit vpu_exit(void)
{
	soc_channel_exit();
	radix_exit();
	platform_driver_unregister(&jz_vpu_driver);
}

module_init(vpu_init);
module_exit(vpu_exit);

MODULE_DESCRIPTION("T32 and T33 VPU helix driver");
MODULE_AUTHOR("Justin <ptkang@ingenic.cn>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("20171010");
