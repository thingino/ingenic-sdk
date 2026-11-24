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
#include <linux/of_irq.h>
#include <linux/timekeeping.h>

#include "soc_vpu.h"
#include "helix.h"

//#define DUMP_HELIX_REG

static int timeoffset = 50;
static int data_threshold = 500;

struct jz_vpu_helix {
	struct vpu          vpu;
	char                name[16];
	int                 irq;
	void __iomem        *iomem;
	void __iomem        *iomem_ivdc;
	struct clk          *clk;
	struct clk          *clk_gate;
	struct clk          *ivdc_gate;
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

#define CPM_SRBC	0xc4
#define CPM_OPCR	0x24
#define OPCR_IDLE	(0x1 << 31)

#define cpm_inl(vpu, off)		readl((vpu)->cpm + (off))
#define cpm_outl(vpu, val, off)		writel((val), (vpu)->cpm + (off))
#define cpm_set_bit(vpu, bit, off)	cpm_outl(vpu, cpm_inl(vpu, off) | BIT(bit), off)
#define cpm_clear_bit(vpu, bit, off)	cpm_outl(vpu, cpm_inl(vpu, off) & ~BIT(bit), off)

static long vpu_reset(struct device *dev)
{
	struct jz_vpu_helix *vpu = dev_get_drvdata(dev);
	int timeout = 0x7fff;
	unsigned int srbc = cpm_inl(vpu, CPM_SRBC);

	cpm_set_bit(vpu, CPM_HELIX_STP(vpu->vpu.idx), CPM_SRBC);
	while (!(cpm_inl(vpu, CPM_SRBC) & (1 << CPM_HELIX_ACK(vpu->vpu.idx))) && --timeout) udelay(20);

	if (timeout == 0) {
		dev_warn(vpu->vpu.dev, "[%d:%d] wait stop ack timeout\n",
			 current->tgid, current->pid);
		cpm_outl(vpu, srbc, CPM_SRBC);
		return -1;
	} else {
		cpm_outl(vpu, srbc | (1 << CPM_HELIX_SR(vpu->vpu.idx)), CPM_SRBC);
		cpm_outl(vpu, srbc, CPM_SRBC);
	}

	return 0;
}

static long vpu_open(struct device *dev)
{
	struct jz_vpu_helix *vpu = dev_get_drvdata(dev);
	unsigned long slock_flag = 0;
	int ret;

	/* clk_prepare_enable() may sleep, so the clocks go on before the lock */
	if (cpm_inl(vpu, CPM_OPCR) & OPCR_IDLE)
		return -EBUSY;
	ret = clk_prepare_enable(vpu->clk);
	if (ret)
		return ret;
	ret = clk_prepare_enable(vpu->ivdc_gate);
	if (ret)
		goto err_ivdc;
	ret = clk_prepare_enable(vpu->clk_gate);
	if (ret)
		goto err_gate;

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
	vpu->bslen = 0;
	spin_unlock_irqrestore(&vpu->slock, slock_flag);
	while (try_wait_for_completion(&vpu->done))
		;

	dev_dbg(dev, "[%d:%d] open\n", current->tgid, current->pid);

	return 0;

err_gate:
	clk_disable_unprepare(vpu->ivdc_gate);
err_ivdc:
	clk_disable_unprepare(vpu->clk);
	return ret;
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

	cpm_clear_bit(vpu, CPM_HELIX_SR(vpu->vpu.idx), CPM_SRBC);

	/* Clear completion use_count here to avoid a unhandled irq after vpu off */
	vpu->done.done = 0;
	vpu->vpu_status = VPU_STATUS_CLOSE;
	spin_unlock_irqrestore(&vpu->slock, slock_flag);

	clk_disable_unprepare(vpu->clk);
	clk_disable_unprepare(vpu->clk_gate);
	clk_disable_unprepare(vpu->ivdc_gate);

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
	unsigned int isp_y_ddr_line_cnt = 0;
	unsigned int v0_ddr_y_grp_line = 0;
	unsigned int valid_data_line = 0;
	unsigned int overflow_cnt = 0;
	struct jz_vpu_helix *vpu = dev_get_drvdata(dev);
	struct timespec64 ts;
	uint64_t time = 0;


#ifdef DUMP_HELIX_REG
	dev_info(vpu->vpu.dev, "------%s(%d)helix_show_internal_state start------\n", __func__, __LINE__);
	helix_show_internal_state(vpu);
	dev_info(vpu->vpu.dev, "------%s(%d)helix_show_internal_state end------\n", __func__, __LINE__);
#endif

	unsigned long slock_flag = 0;
	spin_lock_irqsave(&vpu->slock, slock_flag);
	vpu_writel(vpu, REG_SCH_GLBC, SCH_GLBC_HIAXI | SCH_INTE_RESERR | SCH_INTE_ACFGERR
			| SCH_INTE_BSERR | SCH_INTE_ENDF | SCH_INTE_BSF);

	if (cnode->frame_type == FRAME_TYPE_IVDC) {
		overflow_cnt = readl(vpu->iomem_ivdc+0x2c);
		if (overflow_cnt > 0) {
			if (overflow_cnt > cnode->overflow_cnt) {
				spin_unlock_irqrestore(&vpu->slock, slock_flag);
				printk("overflow_cnt = %d, cnode_overflow_cnt = %d\n",overflow_cnt,cnode->overflow_cnt);
				goto enc_cancel;
			}
		}

		ktime_get_raw_ts64(&ts);
		time =  ts.tv_sec*1000 + ts.tv_nsec / 1000 / 1000;
		if (cnode->time > 0) {
			if ((time - cnode->time) > timeoffset) {
				spin_unlock_irqrestore(&vpu->slock, slock_flag);
				printk("time = %lld, timeoffset = %d\n",time - cnode->time, timeoffset);
				goto enc_cancel;
			}
		}

		isp_y_ddr_line_cnt = readl(vpu->iomem_ivdc+0x208);
		v0_ddr_y_grp_line = readl(vpu->iomem_ivdc+0x3a8);
		if (((isp_y_ddr_line_cnt >> 16) & (1 << 15)) == (v0_ddr_y_grp_line & (1 << 15))) {
			valid_data_line = ((isp_y_ddr_line_cnt >> 16)&0x7fff) - (v0_ddr_y_grp_line & 0x7fff);
		} else {
			valid_data_line = ((isp_y_ddr_line_cnt >> 16)&0x7fff) + cnode->ivdc_mem_line - (v0_ddr_y_grp_line & 0x7fff);
		}
		if (valid_data_line > data_threshold) {
			spin_unlock_irqrestore(&vpu->slock, slock_flag);
			printk("valid_data_line = %d,data_threshold = %d\n",valid_data_line, data_threshold);
			goto enc_cancel;
		}

		writel(1, vpu->iomem_ivdc+0x78);
		writel(1, vpu->iomem_ivdc+0x70);
	}

	vpu_writel(vpu, REG_VDMA_TASKRG_T21, VDMA_ACFG_DHA(cnode->dma_addr)
			| VDMA_ACFG_RUN);

	spin_unlock_irqrestore(&vpu->slock, slock_flag);
	dev_dbg(vpu->vpu.dev, "[%d:%d] start vpu\n", current->tgid, current->pid);

	return 0;
enc_cancel:
	return 0x2;
}

static long vpu_wait_complete(struct device *dev, struct channel_node * const cnode)
{
	long ret = 0, bsreadcnt = 3;
	struct jz_vpu_helix *vpu = dev_get_drvdata(dev);

hard_vpu_wait_restart:
	ret = wait_for_completion_interruptible_timeout(&vpu->done, msecs_to_jiffies(cnode->mdelay));
	if ((ret > 0) && (cnode->codecdir == HWJPEGDEC)) {
		vpu->bslen = vpu_readl(vpu, REG_JPGC_MCUS);
		vpu->cmpx = 0;
	}
	while ((bsreadcnt-- > 0) && (ret > 0) && (vpu->bslen == 0)) {
		if (cnode->codecdir == HWJPEGDEC) {
			vpu->bslen = vpu_readl(vpu, REG_JPGC_MCUS);
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
	if ((ret > 0) && vpu->bslen) {
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
	cnode->max_bs_act = 0;
	if (cnode->codecdir == HWJPEGENC) {
		if ((vpu_readl(vpu, REG_JPGC_MAX_BS)) & (1 << 31))
			cnode->max_bs_act = vpu_readl(vpu, REG_JPGC_ACT_BS);
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

static long vpu_set_bsfull_paddr(struct device *dev, struct channel_node *cnode)
{
	unsigned int paddr;
	struct jz_vpu_helix *vpu = dev_get_drvdata(dev);

	paddr = vpu_readl(vpu, REG_EMC_BS_ADDR);
	vpu_writel(vpu, REG_EMC_BS_ADDR, paddr);
	vpu_writel(vpu, REG_EMC_BS_STAT, 1);
	vpu_writel(vpu, REG_SCH_GLBC, SCH_GLBC_HIAXI | SCH_INTE_RESERR | SCH_INTE_ACFGERR | SCH_INTE_BSERR | SCH_INTE_ENDF | SCH_INTE_BSF);

	return 0;
}

static int bs_cnt = 0;
static long vpu_wait_bs_complete(struct device *dev, struct channel_node *cnode)
{
	int ret = 0, bsreadcnt = 3, bslen_bs = 0;
	unsigned int vpu_stat;
	unsigned int bs_paddr;
	struct jz_vpu_helix *vpu = dev_get_drvdata(dev);

hard_vpu_wait_bs_restart:
	ret = wait_for_completion_interruptible_timeout(&vpu->done, msecs_to_jiffies(cnode->mdelay));
	vpu_stat = vpu->status;
	bs_paddr = vpu_readl(vpu, REG_EMC_BS_ADDR);

	if (vpu_stat & SCH_STAT_ENDFLAG) {
		bs_cnt = 0;
		if ((ret > 0) && (cnode->codecdir == HWJPEGDEC)) {
			vpu->bslen = vpu_readl(vpu, REG_JPGC_MCUS);
			vpu->cmpx = 0;
		}
		while ((bsreadcnt-- > 0) && (ret > 0) && (vpu->bslen == 0)) {
			if (cnode->codecdir == HWJPEGDEC) {
				vpu->bslen = vpu_readl(vpu, REG_JPGC_MCUS);
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
		if ((ret > 0) && vpu->bslen) {
			ret = 0;
			dev_dbg(vpu->vpu.dev, "[%d:%d] wait complete finish\n", current->tgid, current->pid);
		} else if (ret == -ERESTARTSYS) {
			dev_dbg(vpu->vpu.dev, "[%d:%d]:fun:%s,line:%d vpu is interrupt\n", current->tgid, current->pid, __func__, __LINE__);
		} else {
			dev_warn(dev, "[%d:%d] wait_for_completion timeout\n", current->tgid, current->pid);
			//dev_warn(dev, "vpu_stat = %x\n", vpu_readl(vpu,REG_SCH_STAT));
			//dev_warn(dev, "vdma_task = %x\n", vpu_readl(vpu,REG_VDMA_TASKST));
			//dev_warn(dev, "ret = %ld\n", ret);
#ifdef DUMP_HELIX_REG
			dev_info(vpu->vpu.dev, "------%s(%d)helix_show_internal_state start------\n", __func__, __LINE__);
			helix_show_internal_state(vpu);
			dev_info(vpu->vpu.dev, "------%s(%d)helix_show_internal_state end------\n", __func__, __LINE__);
#endif
			if (vpu_reset(dev) < 0) {
				dev_warn(dev, "vpu reset failed\n");
			}
			goto hard_vpu_wait_bs_restart;
		}
		cnode->output_len = vpu->bslen;
		cnode->status = vpu->status;
		cnode->cmpx = vpu->cmpx;
		ret = 0x3;
	} else if (vpu_stat & SCH_STAT_BSFULL) {

		if (vpu_stat & (1 << 9)) {
			vpu->bslen = vpu_readl(vpu, REG_SDE_CFG9);
			bslen_bs = vpu_readl(vpu, REG_EMC_BS_SIZE);

			if ((vpu->bslen - (bslen_bs*1024*bs_cnt)) <= (bslen_bs*1024)) {
				cnode->output_len = vpu->bslen;
				ret = 0x3;
				bs_cnt = 0;
				return ret;
			}
		}

		bs_cnt++;
		vpu->bslen = vpu_readl(vpu, REG_EMC_BS_SIZE);
		if ((ret > 0) && (cnode->codecdir == HWJPEGDEC)) {
			vpu->bslen = vpu_readl(vpu, REG_EMC_BS_SIZE);
			vpu->cmpx = 0;
		}
		while ((bsreadcnt-- > 0) && (ret > 0) && (vpu->bslen == 0)) {
			if (cnode->codecdir == HWJPEGDEC) {
				vpu->bslen = vpu_readl(vpu, REG_EMC_BS_SIZE);
				vpu->cmpx = 0;
			} else if (cnode->codecdir == HWJPEGENC) {
				vpu->bslen = vpu_readl(vpu, REG_EMC_BS_SIZE) & 0xffffff;
				vpu->cmpx = 0;
			} else if (cnode->codecdir == HWH264ENC) {
				vpu->bslen = vpu_readl(vpu, REG_EMC_BS_SIZE);
				vpu->cmpx = vpu_readl(vpu, REG_EFE_SSAD);
			}
			if (vpu->bslen == 0) {
				msleep(2);
				continue;
			}
		}
		if ((ret > 0) && vpu->bslen) {
			ret = 0;
			dev_dbg(vpu->vpu.dev, "[%d:%d] wait bs_complete finish\n", current->tgid, current->pid);
		} else if (ret == -ERESTARTSYS) {
			dev_dbg(vpu->vpu.dev, "[%d:%d]:fun:%s,line:%d vpu is interrupt\n", current->tgid, current->pid, __func__, __LINE__);
		} else {
			dev_warn(dev, "[%d:%d] wait_for_bs_completion timeout\n", current->tgid, current->pid);
#ifdef DUMP_HELIX_REG
			dev_info(vpu->vpu.dev, "------%s(%d)helix_show_internal_state start------\n", __func__, __LINE__);
			helix_show_internal_state(vpu);
			dev_info(vpu->vpu.dev, "------%s(%d)helix_show_internal_state end------\n", __func__, __LINE__);
#endif
			if (vpu_reset(dev) < 0) {
				dev_warn(dev, "vpu reset failed\n");
			}
			goto hard_vpu_wait_bs_restart;
		}
		cnode->output_len = vpu->bslen;
		cnode->status = vpu->status;
		cnode->cmpx = vpu->cmpx;
		ret = 0x4;
	}

	return ret;
}

static struct vpu_ops vpu_ops = {
	.owner		= THIS_MODULE,
	.open		= vpu_open,
	.release	= vpu_release,
	.start_vpu	= vpu_start,
	.wait_complete	= vpu_wait_complete,
	.wait_bs_complete	= vpu_wait_bs_complete,
	.set_bsfull_paddr	= vpu_set_bsfull_paddr,
	.reset		= vpu_reset,
	.suspend	= vpu_suspend,
	.resume		= vpu_resume,
};

static irqreturn_t vpu_interrupt(int irq, void *dev)
{
	struct jz_vpu_helix *vpu = dev;
	unsigned int vpu_stat;
	unsigned long vflag = 0;

	vpu_stat = vpu_readl(vpu,REG_SCH_STAT);

	spin_lock_irqsave(&vpu->slock, vflag);
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
			if (vpu_stat & SCH_STAT_BSFULL) {
				vpu_writel(vpu, REG_EMC_BS_STAT, 0x2);
				vpu_writel(vpu, REG_SCH_GLBC, SCH_GLBC_HIAXI | SCH_INTE_RESERR | SCH_INTE_ACFGERR | SCH_INTE_BSERR | SCH_INTE_ENDF | SCH_INTE_BSF);
			}
			vpu->status = vpu_stat;
			complete(&vpu->done);
		} else if (vpu_stat & SCH_STAT_BSFULL) {
			vpu_writel(vpu, REG_EMC_BS_STAT, 0x2);
			vpu->status = vpu_stat;
			complete(&vpu->done);
		} else {
			check_vpu_status(SCH_STAT_ORESERR, "out fo resolution error!\n");
			check_vpu_status(SCH_STAT_BSERR, "BS error!\n");
			check_vpu_status(SCH_STAT_ACFGERR, "ACFG error!\n");
			vpu->bslen = 0;
			vpu->cmpx = 0;
		}
	}

	spin_unlock_irqrestore(&vpu->slock, vflag);

	return IRQ_HANDLED;
}

static int vpu_probe(struct platform_device *pdev)
{
	int ret;
	struct resource	*regs;
	struct jz_vpu_helix *vpu;
	struct device_node *cgu;
	long rate;

	vpu = kzalloc(sizeof(struct jz_vpu_helix), GFP_KERNEL);
	if (!vpu) {
		dev_err(&pdev->dev, "kzalloc vpu space failed\n");
		ret = -ENOMEM;
		goto err_kzalloc_vpu;
	}

	vpu->vpu.idx = 0;
	vpu->vpu.vpu_id = VPU_HELIX_ID | (1 << vpu->vpu.idx);
	snprintf(vpu->name, sizeof(vpu->name), "helix%d", vpu->vpu.idx);

	vpu->irq = platform_get_irq(pdev, 0);
	if (vpu->irq < 0) {
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
		dev_err(&pdev->dev, "ioremap ivdc failed\n");
		ret = -ENXIO;
		goto err_ivdc_iomem;
	}

	/* SRBC (reset) and OPCR (idle) live in the CPM, the CGU node's registers */
	cgu = of_find_compatible_node(NULL, NULL, "ingenic,t23-cgu");
	vpu->cpm = cgu ? of_iomap(cgu, 0) : NULL;
	of_node_put(cgu);
	if (!vpu->cpm) {
		dev_err(&pdev->dev, "no CPM registers\n");
		ret = -ENXIO;
		goto err_cpm;
	}

	vpu->ivdc_gate = devm_clk_get(&pdev->dev, "ivdc");
	if (IS_ERR(vpu->ivdc_gate)) {
		dev_err(&pdev->dev, "ivdc_gate get failed\n");
		ret = PTR_ERR(vpu->ivdc_gate);
		goto err_clk;
	}

	vpu->clk_gate = devm_clk_get(&pdev->dev, "gate");
	if (IS_ERR(vpu->clk_gate)) {
		dev_err(&pdev->dev, "clk_gate get failed\n");
		ret = PTR_ERR(vpu->clk_gate);
		goto err_clk;
	}

	vpu->clk = devm_clk_get(&pdev->dev, "vpu");
	if (IS_ERR(vpu->clk)) {
		dev_err(&pdev->dev, "clk get failed\n");
		ret = PTR_ERR(vpu->clk);
		goto err_clk;
	}
	/* the 7.x CGU only takes rates it can make exactly; 3.10 rounded */
	rate = clk_round_rate(vpu->clk, 600000000);
	clk_set_rate(vpu->clk, rate > 0 ? rate : 600000000);

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

	if ((ret = vpu_register(&vpu->vpu.vlist)) < 0) {
		dev_err(&pdev->dev, "vpu_register failed\n");
		goto err_vpu_register;
	}

	return 0;

err_vpu_register:
	free_irq(vpu->irq, vpu);
err_clk:
	iounmap(vpu->cpm);
err_cpm:
	iounmap(vpu->iomem_ivdc);
err_ivdc_iomem:
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
	iounmap(vpu->cpm);
	iounmap(vpu->iomem_ivdc);
	iounmap(vpu->iomem);
	kfree(vpu);
}

static const struct of_device_id helix_of_match[] = {
	{ .compatible = "ingenic,t23-helix" },
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

/*
 * soc_channel_init() insists on at least one registered vpu, which the
 * platform driver's probe provides, so the driver goes first.
 */
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

MODULE_DESCRIPTION("T23 VPU helix driver");
MODULE_AUTHOR("Justin <ptkang@ingenic.cn>");
MODULE_LICENSE("GPL v2");
