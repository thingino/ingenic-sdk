#include <linux/init.h>
#include <linux/math64.h>
#include <linux/proc_fs.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/suspend.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>
#include <soc/base.h>

#include "lib_base.h"
#include "lib_base_sensor.h"
#include "tx-isp-device.h"
#include "tx-isp-videodev.h"
#include "txx-funcs.h"
#define ISP_PROC_SHOW_BUF_SIZE (16 * 1024)
#define VIDEOIN_CMD_SENSOR_NUM SENSORNUM
#define VIDEOIN_CMD_BUF_SIZE 128
#define TX_ISP_BIN_MEM_SIZE (200 * 1024)

static char video_input_cmd_buf[VIDEOIN_CMD_SENSOR_NUM][VIDEOIN_CMD_BUF_SIZE];

#ifdef CONFIG_KERNEL_5_15
#define ISP_PROC_FOPS struct proc_ops
#define ISP_PROC_OWNER
#define ISP_PROC_OPEN(fn) .proc_open = fn,
#define ISP_PROC_READ(fn) .proc_read = fn,
#define ISP_PROC_WRITE(fn) .proc_write = fn,
#define ISP_PROC_LSEEK(fn) .proc_lseek = fn,
#define ISP_PROC_RELEASE(fn) .proc_release = fn,
#else
#define ISP_PROC_FOPS struct file_operations
#ifndef CONFIG_ZERATUL
#define ISP_PROC_OWNER .owner = THIS_MODULE,
#else
#define ISP_PROC_OWNER .owner = NULL,
#endif
#define ISP_PROC_OPEN(fn) .open = fn,
#define ISP_PROC_READ(fn) .read = fn,
#define ISP_PROC_WRITE(fn) .write = fn,
#define ISP_PROC_LSEEK(fn) .llseek = fn,
#define ISP_PROC_RELEASE(fn) .release = fn,
#endif

static struct tx_isp_device *globe_ispdev = NULL;

tx_isp_ops_type_t tx_isp_get_device_opstype(void)
{
	if (globe_ispdev == NULL) {
		return TISP_OPS_KERNEL;
	}

	return globe_ispdev->module.ops_type;
}

extern struct platform_device tx_isp_platform_device;

extern unsigned int direct_mode;
extern unsigned int ivdc_mem_line;
extern unsigned int ivdc_threshold_line;
extern unsigned int ivdc_mem_stride;
extern unsigned int ivdc_mem_paddr;

enum tx_isp_iomem_index {
	TX_ISP_IOMEM_CORE = 0,
	TX_ISP_IOMEM_CSI0 = 1,
	TX_ISP_IOMEM_CSI1 = 2,
	TX_ISP_IOMEM_IVDC = 3,
	TX_ISP_IOMEM_CSI_PHY = 4,
};

static int tx_isp_module_cpm_reset(unsigned int addr, unsigned int bit)
{
	int timeout = 500;
	unsigned int value = 0;

	value = *(volatile unsigned int *)(addr);
	value |= 1 << (bit - 1);
	*(volatile unsigned int *)(addr) = value;

	while (timeout && ((*(volatile unsigned int *)(addr)) & (1 << (bit - 2))) == 0) {
		private_msleep(2);
		timeout--;
	}
	if (timeout == 0)
		return -1;

	value = *(volatile unsigned int *)(addr);
	value |= 1 << bit;
	*(volatile unsigned int *)(addr) = value;
	private_msleep(2);
	value = *(volatile unsigned int *)(addr);
	value &= ~(1 << bit);
	*(volatile unsigned int *)(addr) = value;
	private_msleep(2);
	value = *(volatile unsigned int *)(addr);
	value &= ~(1 << (bit - 1));
	*(volatile unsigned int *)(addr) = value;
	return 0;
}

static int tx_isp_cpm_reset(void)
{
	unsigned int addr = 0;
	unsigned int bit = 0;
	addr = 0xb00000c4;
	bit = 22;
	return tx_isp_module_cpm_reset(addr, bit);
}

void system_reg_write(unsigned int reg, unsigned int value)
{
	tx_isp_writel(globe_ispdev->base[TX_ISP_IOMEM_CORE], reg, value);
}

unsigned int system_reg_read(unsigned int reg)
{
	return tx_isp_readl(globe_ispdev->base[TX_ISP_IOMEM_CORE], reg);
}

void system_reg_csi_write(unsigned int reg, unsigned int value)
{
	unsigned int index = (reg >> 16) & 0x1;
	unsigned int offset = reg & 0xFFFF;
	unsigned int base_index = index ? TX_ISP_IOMEM_CSI1 : TX_ISP_IOMEM_CSI0;

	if (!globe_ispdev || index > 1 || !globe_ispdev->base[base_index]) {
		ISP_ERROR("[ %s:%d ] invalid CSI host index %u\n", __func__, __LINE__, index);
		return;
	}

	tx_isp_writel(globe_ispdev->base[base_index], offset, value);
}

unsigned int system_reg_csi_read(unsigned int reg)
{
	unsigned int index = (reg >> 16) & 0x1;
	unsigned int offset = reg & 0xFFFF;
	unsigned int base_index = index ? TX_ISP_IOMEM_CSI1 : TX_ISP_IOMEM_CSI0;

	if (!globe_ispdev || index > 1 || !globe_ispdev->base[base_index]) {
		ISP_ERROR("[ %s:%d ] invalid CSI host index %u\n", __func__, __LINE__, index);
		return 0;
	}

	return tx_isp_readl(globe_ispdev->base[base_index], offset);
}

void system_reg_csi_phy_write(unsigned int reg, unsigned int value)
{
	tx_isp_writel(globe_ispdev->base[TX_ISP_IOMEM_CSI_PHY], reg, value);
}

unsigned int system_reg_csi_phy_read(unsigned int reg)
{
	return tx_isp_readl(globe_ispdev->base[TX_ISP_IOMEM_CSI_PHY], reg);
}

void system_reg_ivdc_write(unsigned int reg, unsigned int value)
{
	tx_isp_writel(globe_ispdev->base[TX_ISP_IOMEM_IVDC], reg, value);
}

unsigned int system_reg_ivdc_read(unsigned int reg)
{
	return tx_isp_readl(globe_ispdev->base[TX_ISP_IOMEM_IVDC], reg);
}

uint64_t private_tx_isp_div64_u64(uint64_t dividend, uint64_t divisor)
{
	if (divisor == 0) {
		ISP_ERROR("[ %s:%d ] divisor is zero\n", __func__, __LINE__);
		return 0;
	}

	return div64_u64(dividend, divisor);
}

static int tx_isp_module_init(struct platform_device *pdev, struct tx_isp_module *module)
{
	int ret = 0;

	if (!module) {
		ISP_ERROR("[ %s:%d ] the parameters are invalid!\n", __func__, __LINE__);
		return -EINVAL;
	}

	memset(module->name, 0x0, sizeof(module->name));
	snprintf(module->name, sizeof(module->name), "%s", pdev && pdev->name ? pdev->name : "tx-isp");
	module->ops_type = TISP_OPS_SDK;
	module->mode = TISP_MODE_NORMAL;
	module->moduleState = TX_ISP_MODULE_INFO_SLAKE;
	// ISP_PRINTK("tx_isp_module_init module is %p, name is %s\n", module, pdev->name);

	return ret;
}

static void ispcore_set_clk_parent(tx_isp_device_t *p_dev, const char *clk_name, const char *pclk_name)
{
	int ret = 0;
	struct clk *tclk = NULL;
	struct clk *aclk = NULL;

	if (!p_dev || !p_dev->dev || !clk_name || !pclk_name) {
		ISP_WARNING("invalid clock-parent parameters.\n");
		return;
	}

	tclk = devm_clk_get(p_dev->dev, pclk_name);
	if (IS_ERR(tclk)) {
		ISP_WARNING("get %s failed!\n", pclk_name);
		return;
	}

	aclk = clk_get(NULL, clk_name);
	if (IS_ERR(aclk)) {
		ISP_WARNING("get parent %s failed!\n", clk_name);
		devm_clk_put(p_dev->dev, tclk);
		return;
	}

	ret = clk_set_parent(tclk, aclk);
	clk_put(aclk);
	devm_clk_put(p_dev->dev, tclk);

	if (ret)
		ISP_WARNING("Failed to modify the parent clock.\n");
}

static int tx_isp_update_clk_param(tx_isp_device_t *p_dev)
{
	int32_t i = 0;
	int32_t ret = ISP_SUCCESS;

	uint32_t num = ARRAY_SIZE(tx_isp_clk_info);
	char *clk_name = NULL;
	char *clkv_name = NULL;
	char *clka_name = NULL;
	char *clkl_name = NULL;
	int clkm_freq = 0;
	int clka_freq = 0;
	int clkv_freq = 0;
	int clkl_freq = 0;

	clk_name = get_clk_name();
	clka_name = get_clka_name();
	clkv_name = get_clkv_name();
	clkl_name = get_clkl_name();
	clkm_freq = get_isp_clk();
	clka_freq = get_isp_clka();
	clkv_freq = get_isp_clkv();
	clkl_freq = get_isp_clkl();

	ispcore_set_clk_parent(p_dev, clk_name, CDR_ISP_NAME);
	ispcore_set_clk_parent(p_dev, clka_name, CDR_ISPA_NAME);
	ispcore_set_clk_parent(p_dev, clkv_name, CDR_VPU_NAME);
	ispcore_set_clk_parent(p_dev, clkl_name, CDR_LDC_NAME);

	for (i = 0; i < num; i++) {
		if (p_dev->clks[i] == NULL || IS_ERR(p_dev->clks[i])) {
			if (!(strcmp(tx_isp_clk_info[i].name, "div_isp"))) {
				tx_isp_clk_info[i].rate = clkm_freq;
			} else if (!(strcmp(tx_isp_clk_info[i].name, "div_ispa"))) {
				tx_isp_clk_info[i].rate = clka_freq;
			} else if (!(strcmp(tx_isp_clk_info[i].name, "div_vpu"))) {
				tx_isp_clk_info[i].rate = clkv_freq;
			} else if (!(strcmp(tx_isp_clk_info[i].name, "div_ldc"))) {
				tx_isp_clk_info[i].rate = clkl_freq;
			}
		}
	}

	return ret;
}

static int tx_isp_set_clk(tx_isp_device_t *p_dev)
{
	int32_t i = 0;
	int32_t ret = ISP_SUCCESS;

	uint32_t num = ARRAY_SIZE(tx_isp_clk_info);

	tx_isp_update_clk_param(p_dev);
	for (i = 0; i < num; i++) {
		// ISP_PRINTK("tx_isp_clk_info[%d].name = %s\n", i, tx_isp_clk_info[i].name);
		if (p_dev->clks[i] == NULL || IS_ERR(p_dev->clks[i])) {
			p_dev->clks[i] = devm_clk_get(p_dev->dev, tx_isp_clk_info[i].name);
			if (IS_ERR(p_dev->clks[i])) {
				ISP_ERROR("[ %s:%d ] i %d Failed to get %s clock %ld\n", __func__, __LINE__, i, tx_isp_clk_info[i].name, PTR_ERR(p_dev->clks[i]));
				ret = PTR_ERR(p_dev->clks[i]);
				goto failed_to_get_clk;
			}
		}

		if (tx_isp_clk_info[i].rate != DUMMY_CLOCK_RATE) {
			ret = clk_set_rate(p_dev->clks[i], tx_isp_clk_info[i].rate);
			if (ret) {
				ISP_ERROR("[ %s:%d ] Failed to set %s clock rate(%ld)\n", __func__, __LINE__, tx_isp_clk_info[i].name, tx_isp_clk_info[i].rate);
				goto failed_to_get_clk;
			}
		}
#ifdef CONFIG_KERNEL_3_10
		clk_enable(p_dev->clks[i]);
#else
		clk_prepare_enable(p_dev->clks[i]);
#endif
	}

failed_to_get_clk:
	return ret;
}

static void tx_isp_disable_clk(tx_isp_device_t *p_dev)
{
	int32_t i = 0;
	uint32_t num = ARRAY_SIZE(tx_isp_clk_info);

	if (p_dev == NULL) {
		return;
	}

	for (i = (int32_t)num - 1; i >= 0; i--) {
		if (p_dev->clks[i] == NULL || IS_ERR(p_dev->clks[i])) {
			continue;
		}
#ifdef CONFIG_KERNEL_3_10
		clk_disable(p_dev->clks[i]);
#else
		clk_disable_unprepare(p_dev->clks[i]);
#endif
	}
}

static int tx_isp_open_config(struct tx_isp_device *ispdev, struct tx_isp_module *module, int cpm_reset_enable)
{
	int ret = 0;

	ISP_PRINTK("tx_isp_open\n");
	if (module->moduleState >= TX_ISP_MODULE_INFO_ACTIVE) {
		return 0;
	}
	ret = tx_isp_set_clk(ispdev);

	if (ret == -ENOIOCTLCMD)
		ret = 0;

	if (cpm_reset_enable) {
		ret = tx_isp_cpm_reset();
		if (ret) {
			ISP_ERROR("tx_isp_cpm_reset failed\n");
		}
	}

	return ret;
}

static void tx_isp_memory_pool_free(struct tx_isp_device *ispdev)
{
	int i;

	for (i = 0; i < TX_ISP_MEMORY_MAX_NUM; i++) {
		if (ispdev->mem[i].vaddr != 0) {
			kfree((void *)ispdev->mem[i].vaddr);
			memset(&ispdev->mem[i], 0x0, sizeof(struct tx_isp_mem));
		}
		ispdev->mem_used[i] = 0;
	}
}

static int tx_isp_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = NULL;
	struct tx_isp_module *module = NULL;
	struct tx_isp_device *ispdev = NULL;
	int ret = 0;

	dev = file->private_data;
	module = miscdev_to_module(dev);
	ispdev = module_to_ispdev(module);

	mutex_lock(&ispdev->mlock);

	if (ispdev->refcnt) {
		ispdev->refcnt++;
		mutex_unlock(&ispdev->mlock);
		return 0;
	}

	module->ops_type = TISP_OPS_SDK;
	ispdev->module.ops_type = TISP_OPS_SDK;
	lib_tx_isp_fw_device_sync_opstype(TISP_OPS_SDK);
	if (module->moduleState >= TX_ISP_MODULE_INFO_ACTIVE) {
		ispdev->refcnt = 1;
		goto unlock;
	}

	ret = tx_isp_open_config(ispdev, module, 1);
	if (ret)
		goto unlock;

	ret = tx_isp_fw_device_open(ispdev->module.ops_type, ispdev->module.mode, NULL);
	if (ret) {
		tx_isp_disable_clk(ispdev);
		goto unlock;
	}

	module->moduleState = TX_ISP_MODULE_INFO_ACTIVE;
	ispdev->refcnt = 1;

unlock:
	mutex_unlock(&ispdev->mlock);

	return ret;
}

static int tx_isp_open_fast(int cpm_reset_enable)
{
	struct tx_isp_module *module = NULL;
	struct tx_isp_device *ispdev = NULL;
	int ret = 0;

	ispdev = globe_ispdev;
	module = &ispdev->module;

	mutex_lock(&ispdev->mlock);

	ret = tx_isp_open_config(ispdev, module, cpm_reset_enable);

	ret = tx_isp_fw_device_open(ispdev->module.ops_type, ispdev->module.mode, NULL);

	module->moduleState = TX_ISP_MODULE_INFO_ACTIVE;

	mutex_unlock(&ispdev->mlock);

	return ret;
}

static int __maybe_unused tx_isp_video_link_destroy(struct tx_isp_module *module, struct tx_isp_cmd_argdata *init)
{
	return 0;
}

static int tx_isp_release(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct tx_isp_module *module = miscdev_to_module(dev);
	struct tx_isp_device *ispdev = module_to_ispdev(module);
	int ret = 0;
	int i;

	mutex_lock(&ispdev->mlock);
	if (ispdev->refcnt <= 0) {
		mutex_unlock(&ispdev->mlock);
		return 0;
	}

	ispdev->refcnt--;
	if (ispdev->refcnt > 0) {
		mutex_unlock(&ispdev->mlock);
		return 0;
	}

	if (module->moduleState <= TX_ISP_MODULE_INFO_SLAKE) {
		mutex_unlock(&ispdev->mlock);
		return 0;
	}

	ISP_PRINTK("tx_isp_release\n");

	ispdev->module.mode = TISP_MODE_NORMAL;
	ret = tx_isp_fw_device_release(TISP_OPS_SDK, ispdev->module.mode, NULL);

	module->moduleState = TX_ISP_MODULE_INFO_SLAKE;
	module->mode = (tx_isp_mode_t)TISP_NORMAL_MODE;
	ispdev->pm_mode = TISP_NORMAL_MODE;

	tx_isp_disable_clk(ispdev);
	for (i = 0; i < TX_ISP_MEMORY_MAX_NUM; i++) {
		ispdev->mem_used[i] = 0;
	}

	mutex_unlock(&ispdev->mlock);
	return ret;
}

static int tx_isp_get_pm_control(struct tx_isp_module *module, unsigned long arg)
{
	struct tx_isp_device *ispdev = module_to_ispdev(module);
	struct tx_isp_cmd_argdata init;

	if (copy_from_user(&init, (void __user *)arg, sizeof(init))) {
		ISP_ERROR("[ %s:%d ] copy from user error\n", __func__, __LINE__);
		return -EFAULT;
	}

	init.enable = (ispdev->pm_mode == TISP_AOV_MODE) ? 1 : 0;
	if (copy_to_user((void __user *)arg, &init, sizeof(init))) {
		ISP_ERROR("[ %s:%d ] copy to user error\n", __func__, __LINE__);
		return -EFAULT;
	}

	return 0;
}

static int tx_isp_set_pm_control(struct tx_isp_module *module, unsigned long arg)
{
	struct tx_isp_device *ispdev = module_to_ispdev(module);
	struct tx_isp_cmd_argdata init;
	int ret = 0;

	if (copy_from_user(&init, (void __user *)arg, sizeof(init))) {
		ISP_ERROR("[ %s:%d ] copy from user error\n", __func__, __LINE__);
		return -EFAULT;
	}

	if (init.enable == 0) {
		if (ispdev->pm_mode == TISP_NORMAL_MODE) {
			return 0;
		}

		ret = lib_tx_isp_pm_stream_on(&init);
		if (ret) {
			return ret;
		}
		ispdev->pm_mode = TISP_NORMAL_MODE;
		module->mode = (tx_isp_mode_t)TISP_NORMAL_MODE;
		return 0;
	}

	if (init.enable == 1) {
		if (ispdev->pm_mode == TISP_AOV_MODE) {
			return 0;
		}

		tx_isp_fw_completion_init(&ispdev->pm_completion);
		ispdev->pm_mode = TISP_AOV_MODE;
		module->mode = (tx_isp_mode_t)TISP_AOV_MODE;
		return 0;
	}

	return -EINVAL;
}

static int tx_isp_memory_malloc(struct tx_isp_module *module, unsigned long arg)
{
	struct tx_isp_device *ispdev = module_to_ispdev(module);
	struct tx_isp_mem mem;
	int ret = 0;
	int i = 0;

	if (copy_from_user(&mem, (void __user *)arg, sizeof(struct tx_isp_mem))) {
		ISP_ERROR("[ %s:%d ] copy from user error\n", __func__, __LINE__);
		return -EFAULT;
	}

	for (i = 0; i < TX_ISP_MEMORY_MAX_NUM; i++) {
		if (!ispdev->mem_used[i]) {
			break;
		}
	}
	if (i == TX_ISP_MEMORY_MAX_NUM) {
		ISP_WARNING("[ %s:%d ] Unable to apply for manageable memory\n", __func__, __LINE__);
		return -ENOMEM;
	}

	if (ispdev->mem[i].vaddr == 0) {
		ispdev->mem[i].size = TX_ISP_BIN_MEM_SIZE;
		ispdev->mem[i].vaddr = (uint32_t)kmalloc(ispdev->mem[i].size, GFP_KERNEL);
		if (!(ispdev->mem[i].vaddr)) {
			ISP_WARNING("[ %s:%d ] Failed to allocate isp bin memory\n", __func__, __LINE__);
			return -ENOMEM;
		}
		ispdev->mem[i].paddr = (uint32_t)virt_to_phys((void *)ispdev->mem[i].vaddr);
	}

	mem.vaddr = ispdev->mem[i].vaddr;
	mem.paddr = ispdev->mem[i].paddr;
	ispdev->mem_used[i] = 1;

	if (copy_to_user((void __user *)arg, &mem, sizeof(mem))) {
		ispdev->mem_used[i] = 0;
		ISP_ERROR("[ %s:%d ] copy to user error\n", __func__, __LINE__);
		return -EFAULT;
	}

	return ret;
}

static int tx_isp_memory_free(struct tx_isp_module *module, unsigned long arg)
{
	struct tx_isp_device *ispdev = module_to_ispdev(module);
	struct tx_isp_mem mem;
	int ret = 0;
	int i = 0;

	if (copy_from_user(&mem, (void __user *)arg, sizeof(struct tx_isp_mem))) {
		ISP_ERROR("[ %s:%d ] copy from user error\n", __func__, __LINE__);
		return -EFAULT;
	}

	if (!(mem.vaddr)) {
		ISP_ERROR("[ %s:%d ] The memory cannot be empty\n", __func__, __LINE__);
		return -ENOMEM;
	}

	for (i = 0; i < TX_ISP_MEMORY_MAX_NUM; i++) {
		if (ispdev->mem[i].vaddr == mem.vaddr) {
			break;
		}
	}
	if (i == TX_ISP_MEMORY_MAX_NUM) {
		ISP_WARNING("[ %s:%d ] Unmanageable memory\n", __func__, __LINE__);
		return -ENOMEM;
	}

	ispdev->mem_used[i] = 0;

	return ret;
}

static int tx_isp_ldc_online_probe(struct tx_isp_module *module,
								   tx_isp_ops_type_t ops, unsigned long arg)
{
	struct tx_isp_device *ispdev = module_to_ispdev(module);
	struct ldc_online_init ldc_attr;
	struct tx_isp_mem mem;
	int ret;
	int i;
	int buf0_idx = -1;
	int buf1_idx = -1;

	ret = lib_tx_isp_ldc_online_probe(ops, arg);
	if (ret != 0) {
		return ret;
	}

	if (copy_from_user(&ldc_attr, (void __user *)arg, sizeof(ldc_attr))) {
		ISP_ERROR("[ %s:%d ] copy from user error\n", __func__, __LINE__);
		return -EFAULT;
	}

	if ((ldc_attr.func != LDC_ISP_HLDC_MODE && ldc_attr.func != LDC_ISP_LDC_MODE) ||
		ldc_attr.lut_size == 0) {
		return 0;
	}

	mem.size = ldc_attr.lut_size;
	mem.vaddr = 0;
	mem.paddr = 0;
	for (i = 0; i < TX_ISP_MEMORY_MAX_NUM; i++) {
		if (ispdev->mem[i].vaddr == 0) {
			break;
		}
	}
	if (i == TX_ISP_MEMORY_MAX_NUM) {
		ISP_WARNING("[ %s:%d ] Unable to apply for manageable memory\n", __func__, __LINE__);
		return -ENOMEM;
	}

	mem.vaddr = (uint32_t)kmalloc(mem.size, GFP_KERNEL);
	if (!(mem.vaddr)) {
		ISP_WARNING("[ %s:%d ] Failed to allocate memory\n", __func__, __LINE__);
		return -ENOMEM;
	}
	mem.paddr = (uint32_t)virt_to_phys((int *)mem.vaddr);
	memcpy(&ispdev->mem[i], &mem, sizeof(struct tx_isp_mem));
	ispdev->mem_used[i] = 1;
	ldc_attr.lut_buf0 = mem.paddr;
	buf0_idx = i;

	if (ldc_attr.lut_single_flag != 1) {
		mem.size = ldc_attr.lut_size;
		mem.vaddr = 0;
		mem.paddr = 0;
		for (i = 0; i < TX_ISP_MEMORY_MAX_NUM; i++) {
			if (ispdev->mem[i].vaddr == 0) {
				break;
			}
		}
		if (i == TX_ISP_MEMORY_MAX_NUM) {
			ISP_WARNING("[ %s:%d ] Unable to apply for manageable memory\n", __func__, __LINE__);
			kfree((void *)ispdev->mem[buf0_idx].vaddr);
			memset(&ispdev->mem[buf0_idx], 0x0, sizeof(struct tx_isp_mem));
			ispdev->mem_used[buf0_idx] = 0;
			return -ENOMEM;
		}

		mem.vaddr = (uint32_t)kmalloc(mem.size, GFP_KERNEL);
		if (!(mem.vaddr)) {
			ISP_WARNING("[ %s:%d ] Failed to allocate memory\n", __func__, __LINE__);
			kfree((void *)ispdev->mem[buf0_idx].vaddr);
			memset(&ispdev->mem[buf0_idx], 0x0, sizeof(struct tx_isp_mem));
			ispdev->mem_used[buf0_idx] = 0;
			return -ENOMEM;
		}
		mem.paddr = (uint32_t)virt_to_phys((int *)mem.vaddr);
		memcpy(&ispdev->mem[i], &mem, sizeof(struct tx_isp_mem));
		ispdev->mem_used[i] = 1;
		ldc_attr.lut_buf1 = mem.paddr;
		buf1_idx = i;
	}

	if (copy_to_user((void __user *)arg, &ldc_attr, sizeof(ldc_attr))) {
		ISP_ERROR("[ %s:%d ] copy to user error\n", __func__, __LINE__);
		if (buf1_idx >= 0) {
			kfree((void *)ispdev->mem[buf1_idx].vaddr);
			memset(&ispdev->mem[buf1_idx], 0x0, sizeof(struct tx_isp_mem));
			ispdev->mem_used[buf1_idx] = 0;
		}
		kfree((void *)ispdev->mem[buf0_idx].vaddr);
		memset(&ispdev->mem[buf0_idx], 0x0, sizeof(struct tx_isp_mem));
		ispdev->mem_used[buf0_idx] = 0;
		return -EFAULT;
	}

	return 0;
}

typedef struct {
	uint8_t vinum;
	struct {
		int enable;
		uint32_t value;
	} attr;
} isp_device_ae_start_attr;

static int tx_isp_ae_start_ev_ioctl(enum tx_isp_priv_ioctl_direction dir, unsigned long arg)
{
	isp_device_ae_start_attr ae_start_attr;
	unsigned long attr_arg = arg + (unsigned long)&((isp_device_ae_start_attr *)0)->attr;

	if (copy_from_user(&ae_start_attr, (void __user *)arg, sizeof(ae_start_attr))) {
		ISP_ERROR("[ %s:%d ] copy from user error\n", __func__, __LINE__);
		return -EFAULT;
	}

	return lib_tx_isp_core_ae_start_ev_ioctl(ae_start_attr.vinum, dir, attr_arg);
}

static int tx_isp_oneshot_control(struct tx_isp_device *ispdev, unsigned long arg)
{
	int ret = ISP_SUCCESS;
	enum oneshot_control_value ops = arg;

	ret = lib_tx_isp_oneshot_control(ops);
	if (ops == TISP_ONESHOT_ENABLE) {
		ispdev->module.mode = TISP_MODE_ONESHOT;
	} else if (ops == TISP_ONESHOT_DISABLE) {
		ispdev->module.mode = TISP_MODE_NORMAL;
	} else {
		// ISP_ERROR("ops type error !!! ops is %d\n", ops);
	}

	return ret;
}

static long tx_isp_unlocked_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = NULL;
	struct tx_isp_module *module = NULL;
	struct tx_isp_device *ispdev = NULL;
	struct isp_buf_info ispIpc;
	struct tx_isp_dbg_register senreg;
	int ret = 0;
	uint32_t ops = 0;
	//      int index = -1;

	if (filp != NULL) {
		dev = filp->private_data;
		module = miscdev_to_module(dev);
		ops = TISP_OPS_SDK;
	} else {
		module = globe_ispdev ? &globe_ispdev->module : NULL;
		if (!module)
			return -ENODEV;
		ops = TISP_OPS_KERNEL;
	}

	ispdev = globe_ispdev;

	switch (cmd) {
		case TISP_VIDIOC_DRIVER_VERSION:
			// ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			ret = lib_tx_isp_driver_version(arg);
			break;
		case TISP_VIDIOC_BYPASS_SENSOR:
			ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			// ret = tx_isp_bypass_all(module, arg);
			break;
		case TISP_VIDIOC_ENUMINPUT:
			// ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			ret = lib_tx_isp_sensor_enum_input(ops, arg);
			break;
		case TISP_VIDIOC_G_INPUT:
			ret = lib_tx_isp_sensor_get_input(module, arg);
			break;
		case TISP_VIDIOC_S_INPUT:
			ret = lib_tx_isp_sensor_set_input(ops, arg);
			break;
		case TISP_VIDIOC_REGISTER_SENSOR:
			ret = lib_tx_isp_sensor_register_sensor(ops, arg);
			break;
		case TISP_VIDIOC_RELEASE_SENSOR:
			ret = lib_tx_isp_sensor_release_sensor(ops, arg);
			break;
		case TISP_VIDIOC_STREAMON:
			ret = lib_tx_isp_video_s_stream(module, arg);
			break;
		case TISP_VIDIOC_STREAMOFF:
			ret = lib_tx_isp_video_s_stream(module, arg);
			break;
		case TISP_VIDIOC_CREATE_SUBDEV_LINKS:
			// ret = tx_isp_video_link_setup(module, arg);
			// ISP_PRINTK("API 接口废弃,确定没用!!! \n");
			ret = 0;
			break;
		case TISP_VIDIOC_DESTROY_SUBDEV_LINKS:
			// ret = tx_isp_video_link_destroy(module, &init);
			ret = 0;
			break;
		case TISP_VIDIOC_LINKS_STREAMON:
			// ret = tx_isp_video_link_stream(module, arg);
			// ISP_PRINTK("API 接口废弃,确定没用!!! \n");
			ret = 0;
			break;
		case TISP_VIDIOC_LINKS_STREAMOFF:
			// ret = tx_isp_video_link_stream(module, arg);
			// ISP_PRINTK("API 接口废弃,确定没用!!! \n");
			ret = 0;
			break;
		case TISP_VIDIOC_SET_SENSOR_REGISTER:
			ret = copy_from_user(&senreg, (void __user *)arg, sizeof(senreg));
			if (ret) {
				ISP_ERROR("[ %s:%d ] copy from user error\n", __func__, __LINE__);
				return -EFAULT;
			}
			ret = tx_isp_write_sensor_register(&senreg);
			break;
		case TISP_VIDIOC_GET_SENSOR_REGISTER:
			ret = copy_from_user(&senreg, (void __user *)arg, sizeof(senreg));
			if (ret) {
				ISP_ERROR("[ %s:%d ] copy from user error\n", __func__, __LINE__);
				return -EFAULT;
			}
			ret = tx_isp_read_sensor_register(&senreg);
			if (ret == 0) {
				ret = copy_to_user((void __user *)arg, &senreg, sizeof(senreg));
				if (ret) {
					ISP_ERROR("[ %s:%d ] copy to user error\n", __func__, __LINE__);
					return -EFAULT;
				}
			}
			break;
		case TISP_VIDIOC_SET_ISP_MEM_INFO:
			ret = lib_tisp_set_isp_rmem_info(ops, arg);
			break;
		case TISP_VIDIOC_GET_ISP_MEM_INFO:
			ret = lib_tisp_get_isp_rmem_info(ops, arg);
			break;
		case TISP_VIDIOC_SET_WDR_BUF_INFO:
			ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			// ret = tx_isp_wdr_set_buf(module, arg);
			break;
		case TISP_VIDIOC_GET_WDR_BUF_INFO:
			ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			// ret = tx_isp_wdr_get_buf(module, arg);
			break;
		case TISP_VIDIOC_SET_MULTISENSOR_MODE:
			ret = lib_tx_isp_set_multi_mode(ops, arg);
			break;
			/* case TISP_VIDIOC_SET_MULTISENSOR_SELECT: */
			/*         ret = tx_isp_mutlisensor_select(module, arg); */
			/*         break; */
		case TISP_VIDIOC_GET_AE_ALGO_HANDLE:
			ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			// ret = tx_isp_get_ae_algo_handle(module, arg);
			break;
		case TISP_VIDIOC_SET_AE_ALGO_HANDLE:
			ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			// ret = tx_isp_set_ae_algo_handle(module, arg);
			break;
		case TISP_VIDIOC_SET_AE_ALGO_OPEN:
			ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			// ret = tx_isp_set_ae_algo_open(module, arg);
			break;
		case TISP_VIDIOC_SET_AE_ALGO_CLOSE:
			ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			// ret = tx_isp_set_ae_algo_close(module, arg);
			break;
		case TISP_VIDIOC_SET_AE_ALGO_CTRL:
			ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			// ret = tx_isp_set_ae_algo_ctrl(module, arg);
			break;
		case TISP_VIDIOC_GET_AWB_ALGO_HANDLE:
			ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			// ret = tx_isp_get_awb_algo_handle(module, arg);
			break;
		case TISP_VIDIOC_SET_AWB_ALGO_HANDLE:
			ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			// ret = tx_isp_set_awb_algo_handle(module, arg);
			break;
		case TISP_VIDIOC_SET_AWB_ALGO_OPEN:
			// ret = tx_isp_set_awb_algo_open(module, arg);
			ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			break;
		case TISP_VIDIOC_SET_AWB_ALGO_CLOSE:
			ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			// ret = tx_isp_set_awb_algo_close(module, arg);
			break;
		case TISP_VIDIOC_SET_AWB_ALGO_CTRL:
			ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			// ret = tx_isp_set_awb_algo_ctrl(module, arg);
			break;
		case TISP_VIDIOC_SET_DEFAULT_BIN_PATH:
			/*ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);*/
			// ISP_PRINTK("API 接口废弃,确定没用!!! \n");
			ret = 0;
			break;
		case TISP_VIDIOC_GET_DEFAULT_BIN_PATH:
			ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			// ret = tx_isp_get_default_bin_path(module, arg);
			break;
		case TISP_VIDIOC_SET_FRAME_DROP:
			ret = lib_tx_isp_set_frame_drop(ops, arg);
			break;
		case TISP_VIDIOC_GET_FRAME_DROP:
			ret = lib_tx_isp_get_frame_drop(ops, arg);
			break;
		case TISP_VIDIOC_SET_FRAME_DROP_ONCE:
			ret = lib_tx_isp_set_frame_drop_once(ops, arg);
			break;
		case TISP_VIDIOC_GET_FRAME_DROP_ONCE:
			ret = lib_tx_isp_get_frame_drop_once(ops, arg);
			break;
		case TISP_VIDIOC_START_NIGHT_MODE:
			ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			// ret = tx_isp_start_night_mode(module, arg);
			break;
		case TISP_VIDIOC_SET_PM_CONTROL:
			ret = tx_isp_set_pm_control(module, arg);
			break;
		case TISP_VIDIOC_GET_PM_CONTROL:
			ret = tx_isp_get_pm_control(module, arg);
			break;
		case TISP_VIDIOC_WAIT_PM_ALL_DONE:
			ret = lib_tx_isp_wait_pm_all_done(ops, arg);
			break;
		case TISP_VIDIOC_GET_RAW_CONTROL:
			ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			// ret = tx_isp_get_raw_control(module, arg);
			break;
		case TISP_VIDIOC_SET_AE_START_EV:
			ret = tx_isp_ae_start_ev_ioctl(TX_ISP_PRIVATE_IOCTL_SET, arg);
			break;
		case TISP_VIDIOC_GET_AE_START_EV:
			ret = tx_isp_ae_start_ev_ioctl(TX_ISP_PRIVATE_IOCTL_GET, arg);
			break;
		case TISP_VIDIOC_ONESHOT_CONTROL:
			ret = tx_isp_oneshot_control(ispdev, arg);
			break;
		case TISP_VIDIOC_SNAP_RAW:
			// ISP_PRINTK("API 接口废弃,确定没用!!! \n");
			ret = 0;
			break;
		case TISP_VIDIOC_CLOSE_MSCA:
			// ISP_PRINTK("API 接口废弃,确定没用!!! \n");
			ret = 0;
			break;
		case TISP_VIDIOC_SET_ISP_IPC_MEM_INFO:
			// ISP_PRINTK("API 接口废弃,确定没用!!! \n");
			ret = 0;
			break;
		case TISP_VIDIOC_GET_ISP_IPC_MEM_INFO:
			if (copy_from_user(&ispIpc, (void __user *)arg, sizeof(struct isp_buf_info))) {
				ret = -EFAULT;
				break;
			}
			ispIpc.size = 0;
			ret = copy_to_user((void __user *)arg, &ispIpc, sizeof(struct isp_buf_info)) ? -EFAULT : 0;
			break;
		case TISP_VIDIOC_GPIO_STATE_SWITCH:
			ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			// ret = tx_isp_gpio_state_switch(module, arg);
			break;
		case TISP_VIDIOC_GPIO_STATE_SWITCH_TRIGGER:
			ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			// ret = tx_isp_gpio_state_switch_trige(module, arg);
			break;
		case TISP_VIDIOC_SET_TIZIANO_PAR:
			ret = lib_tx_isp_set_tiziano_par(ops, arg);
			break;
		case TISP_VIDIOC_DEFAULT_BIN:
			// ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			ret = lib_tisp_defaule_bin_info((void __user *)arg);
			break;
		case TISP_VIDIOC_SET_WDR_ENABLE:
			ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			// ret = tx_isp_set_wdr_enable(module, arg);
			break;
		case TISP_VIDIOC_GET_RUNNING_MODE:
			ret = copy_to_user((void __user *)arg, &ispdev->module.mode, sizeof(int));
			if (ret)
				ISP_ERROR("Get isp driver running mode failed\n");
			break;
		case TISP_VIDIOC_MEMORY_MALLOC:
			// ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			ret = tx_isp_memory_malloc(module, arg);
			break;
		case TISP_VIDIOC_MEMORY_FREE:
			/*ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);*/
			ret = tx_isp_memory_free(module, arg);
			break;
		case TISP_VIDIOC_LDC_SET_MODE:
			ret = lib_tx_isp_ldc_online_set_mode(ops, arg);
			break;
		case TISP_VIDIOC_LDC_ONLINE_PROBE:
			ret = tx_isp_ldc_online_probe(module, ops, arg);
			break;
		case TISP_VIDIOC_LDC_ONLINE_INIT:
			ret = lib_tx_isp_ldc_online_init(ops, arg);
			break;
		case TISP_VIDIOC_GET_LDC_PAR:
			ret = lib_tx_isp_ldc_online_get_param(ops, arg);
			break;
		case TISP_VIDIOC_SET_HLDC_BYPASS_STATE:
			ret = lib_tx_isp_ldc_online_set_hldc_bypass(ops, arg);
			break;
		case TISP_VIDIOC_LDC_ONLINE_REFRESH_LUT:
			ret = lib_tx_isp_ldc_online_refresh_lut(ops, arg);
			break;
		case TISP_VIDIOC_KERNEL_FAST_OPEN: {
			int cpm_reset_enable = 0;

			if (arg) {
				if (ops == TISP_OPS_KERNEL) {
					cpm_reset_enable = *(int *)(unsigned long)arg;
				} else if (copy_from_user(&cpm_reset_enable, (void __user *)arg, sizeof(cpm_reset_enable))) {
					ret = -EFAULT;
					break;
				}
			}
			ret = tx_isp_open_fast(cpm_reset_enable);
			break;
		}
		case TISP_VIDIOC_SET_RISCV_INFO:
			ret = lib_tx_isp_set_riscv_info(ops, arg);
			break;
		case TISP_VIDIOC_SET_ZERATUL_DEBUG_ATTR:
			ret = lib_tx_isp_set_zeratul_debug_attr(ops, arg);
			break;
		default:
			ISP_PRINTK("Unknown ioctl cmd(0x%08x)\n", cmd);
			break;
	}

	return ret;
}

void private_enable_irq(unsigned int irq_id)
{
	if (globe_ispdev && irq_id < TISP_MAX_IRQ) {
		if (globe_ispdev->irqdev[irq_id].irq_refCont == 0) {
			enable_irq(globe_ispdev->irqdev[irq_id].irq);
			globe_ispdev->irqdev[irq_id].irq_enabled = 1;
		}
		globe_ispdev->irqdev[irq_id].irq_refCont++;
	}
}

void private_disable_irq(unsigned int irq_id)
{
	if (globe_ispdev && irq_id < TISP_MAX_IRQ) {
		globe_ispdev->irqdev[irq_id].irq_refCont = globe_ispdev->irqdev[irq_id].irq_refCont > 0 ? globe_ispdev->irqdev[irq_id].irq_refCont - 1 : 0;
		if (globe_ispdev->irqdev[irq_id].irq_refCont == 0) {
			disable_irq(globe_ispdev->irqdev[irq_id].irq);
			globe_ispdev->irqdev[irq_id].irq_enabled = 0;
		}
	}
}

static irqreturn_t tisp_irq_handle(int this_irq, void *dev)
{
	struct tx_isp_irq_device *irqdev = dev;
	if (irqdev && irqdev->index < TISP_MAX_IRQ)
		tx_isp_irq_handle(irqdev->index, NULL);
	return IRQ_HANDLED;
}

long tx_isp_unlocked_ioctl_first(unsigned int cmd, unsigned long arg)
{
	return tx_isp_unlocked_ioctl(NULL, cmd, arg);
}

static int tx_isp_request_irq(struct platform_device *pdev, struct tx_isp_irq_device *irqdevs, int max_irq, int *irq_num)
{
	int irq;
	int num = 0;
	int ret = 0;

	if (!pdev || !irqdevs || max_irq <= 0) {
		ISP_ERROR("%s[%d] the parameters are invalid!\n", __func__, __LINE__);
		ret = -EINVAL;
		goto exit;
	}

	/* num = pdev->num_resources; */
	while (1) {
		irq = platform_get_irq_optional(pdev, num);
		if (irq < 0) {
			/* ISP_INFO("[ %s:%d ] get irq error!! num is %d, irq is %d\n", __func__, __LINE__, pdev->num_resources - num, irq); */
			break;
		}
		if (num >= max_irq) {
			ISP_ERROR("%s[%d] too many irqs for %s (max=%d)\n", __func__, __LINE__, pdev->name, max_irq);
			ret = -EINVAL;
			goto err_req_irq;
		}

		/* ISP_WARNING("[%s %d] %s %d %d 0x%08x\n", __func__, __LINE__, pdev->name, num, irq, irqdev); */
		// ISP_PRINTK("name is %s, num is %d, irq is %d\n", pdev->name, num, irq);
		ret = request_irq(irq, tisp_irq_handle, 0, pdev->name, &irqdevs[num]);
		if (ret) {
			ISP_ERROR("%s[%d] Failed to request irq(%d).\n", __func__, __LINE__, irq);
			ret = -EINTR;
			goto err_req_irq;
		}

		spin_lock_init(&irqdevs[num].slock);
		irqdevs[num].irq = irq;
		irqdevs[num].index = num;
		irqdevs[num].irq_enabled = 0;
		irqdevs[num].irq_refCont = 0;
		disable_irq(irq);
		num++;
	}
	if (irq_num)
		*irq_num = num;

exit:
	return ret;

err_req_irq:
	while (num > 0) {
		num--;
		free_irq(irqdevs[num].irq, &irqdevs[num]);
		irqdevs[num].irq = 0;
		irqdevs[num].irq_enabled = 0;
		irqdevs[num].irq_refCont = 0;
	}
	if (irq_num)
		*irq_num = 0;
	goto exit;
}

static void tx_isp_free_irqs(struct tx_isp_device *ispdev)
{
	int index = 0;

	if (!ispdev)
		return;

	for (index = 0; index < ispdev->irq_num; index++) {
		free_irq(ispdev->irqdev[index].irq, &ispdev->irqdev[index]);
		ispdev->irqdev[index].irq = 0;
		ispdev->irqdev[index].irq_enabled = 0;
		ispdev->irqdev[index].irq_refCont = 0;
	}
	ispdev->irq_num = 0;
}

static void tx_isp_release_iomem(struct tx_isp_device *ispdev)
{
	int index = 0;

	if (!ispdev)
		return;

	for (index = 0; index < ARRAY_SIZE(ispdev->base); index++) {
		if (ispdev->base[index]) {
			iounmap(ispdev->base[index]);
			ispdev->base[index] = NULL;
		}
		if (ispdev->res[index]) {
			release_mem_region(ispdev->res[index]->start,
							   ispdev->res[index]->end - ispdev->res[index]->start + 1);
			ispdev->res[index] = NULL;
		}
	}
}

static void tx_isp_remove_proc_entries(struct tx_isp_device *ispdev)
{
	if (!ispdev)
		return;

	if (ispdev->proc_w00) {
		proc_remove(ispdev->proc_w00);
		ispdev->proc_w00 = NULL;
	}
	if (ispdev->proc_w01) {
		proc_remove(ispdev->proc_w01);
		ispdev->proc_w01 = NULL;
	}
	if (ispdev->proc_w02) {
		proc_remove(ispdev->proc_w02);
		ispdev->proc_w02 = NULL;
	}
	if (ispdev->proc_m0) {
		proc_remove(ispdev->proc_m0);
		ispdev->proc_m0 = NULL;
	}
	if (ispdev->proc_tuning) {
		proc_remove(ispdev->proc_tuning);
		ispdev->proc_tuning = NULL;
	}
	if (ispdev->proc_msca) {
		proc_remove(ispdev->proc_msca);
		ispdev->proc_msca = NULL;
	}
	if (ispdev->proc_ldc) {
		proc_remove(ispdev->proc_ldc);
		ispdev->proc_ldc = NULL;
	}
	if (ispdev->proc_ivdc) {
		proc_remove(ispdev->proc_ivdc);
		ispdev->proc_ivdc = NULL;
	}
	if (ispdev->proc_fs) {
		proc_remove(ispdev->proc_fs);
		ispdev->proc_fs = NULL;
	}
	if (ispdev->proc) {
		proc_remove(ispdev->proc);
		ispdev->proc = NULL;
	}
}

static struct file_operations tx_isp_fops = {
	.open = tx_isp_open,
	.release = tx_isp_release,
	.unlocked_ioctl = tx_isp_unlocked_ioctl,
};

/* misc-ivdc file operations */
static int ivdc_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct tx_isp_device *ispdev = container_of(dev, struct tx_isp_device, ivdc_miscdev);

	ISP_PRINTK("misc-ivdc open\n");
	(void)ispdev;
	return 0;
}

static int ivdc_release(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct tx_isp_device *ispdev = container_of(dev, struct tx_isp_device, ivdc_miscdev);

	ISP_PRINTK("misc-ivdc release\n");
	(void)ispdev;
	return 0;
}

static long ivdc_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = file->private_data;
	struct tx_isp_device *ispdev = container_of(dev, struct tx_isp_device, ivdc_miscdev);
	int ret = 0;

	ISP_PRINTK("misc-ivdc ioctl: cmd=0x%x, arg=0x%lx\n", cmd, arg);
	(void)ispdev;
	(void)cmd;
	(void)arg;
	if (file != NULL) {
		dev = file->private_data;
		ispdev = container_of(dev, struct tx_isp_device, ivdc_miscdev);
	} else {
		ispdev = globe_ispdev;
	}

	/* TODO: 添加具体的 ioctl 处理逻辑 */
	switch (cmd) {
		case TISP_VIDIOC_SET_JPEG:
			// set vpu0 jpeg arid
			// tx_isp_ivdc_writel(ivdc, IVDC_VPU_FRAME_ID, 0x3<<12 | 0x7<<8 | 0x8<<4 | 0x8);
			break;
		case TISP_VIDIOC_SET_H264_H265:
			// set vpu0 h264/h265 arid
			// tx_isp_ivdc_writel(ivdc, IVDC_VPU_FRAME_ID, 0x3<<12 | 0x2<<8 | 0x8<<4 | 0x8);
			break;
		case TISP_VIDIOC_MEM_LINE:
			// get ivdc_mem_line
			ret = copy_to_user((void __user *)arg, &ivdc_mem_line, sizeof(int));
			if (ret)
				ISP_ERROR("Get ivdc_mem_line err! Failed to copy to user\n");
			break;
		case TISP_VIDIOC_MAX_STRIDE:
			// get ivdc_mem_stride
			ret = copy_to_user((void __user *)arg, &ivdc_mem_stride, sizeof(int));
			if (ret)
				ISP_ERROR("Get ivdc_mem_stride err! Failed to copy to user\n");
			break;
		case TISP_VIDIOC_GET_DIRECT:
			// get direct_mode
			ret = copy_to_user((void __user *)arg, &direct_mode, sizeof(int));
			if (ret)
				ISP_ERROR("Get direct_mode err! Failed to copy to user\n");
			break;
		case TISP_VIDIOC_SET_DIRECT:
			// set direct_mode
			ret = copy_from_user(&direct_mode, (void __user *)arg, sizeof(int));
			if (ret)
				ISP_ERROR("Set direct_mode err! Failed to copy form user\n");
			else
				lib_tx_isp_ivdc_set_direct_mode(direct_mode);
			break;
		case TISP_VIDIOC_SET_MEM_LINE:
			// set ivdc_mem_line
			ret = copy_from_user(&ivdc_mem_line, (void __user *)arg, sizeof(int));
			if (ret)
				ISP_ERROR("Set ivdc_mem_line err! Failed to copy form user\n");
			else
				lib_tx_isp_ivdc_set_mem_line(ivdc_mem_line);
			break;
		case TISP_VIDIOC_SET_PADDR:
			// set ivdc paddr
			ret = copy_from_user(&ivdc_mem_paddr, (void __user *)arg, sizeof(unsigned int));
			if (ret)
				ISP_ERROR("Set ivdc_mem_paddr err! Failed to copy from user\n");
			else
				lib_tx_isp_ivdc_set_mem_paddr(ivdc_mem_paddr);
			break;
		default:
			ret = -1;
			ISP_ERROR("unsupport ivdc ioclt cmd: 0x%08x\n", cmd);
	}

	return ret;
}

long ivdc_misc_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return ivdc_unlocked_ioctl(NULL, cmd, arg);
}

static struct file_operations ivdc_fops = {
	.open = ivdc_open,
	.release = ivdc_release,
	.unlocked_ioctl = ivdc_unlocked_ioctl,
};

/* offline_ldc file operations */
static int ldc_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct tx_isp_device *ispdev = container_of(dev, struct tx_isp_device, ldc_miscdev);
	int ret;

	ISP_PRINTK("offline_ldc open\n");
	(void)ispdev;
	ret = lib_tx_isp_ldc_offline_open();
	if (ret == 0)
		private_enable_irq(TISP_LDC_IRQ);

	return ret;
}

static int ldc_release(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct tx_isp_device *ispdev = container_of(dev, struct tx_isp_device, ldc_miscdev);
	int ret;

	ISP_PRINTK("offline_ldc release\n");
	(void)ispdev;
	ret = lib_tx_isp_ldc_offline_release();
	private_disable_irq(TISP_LDC_IRQ);

	return ret;
}

static long ldc_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = file->private_data;
	struct tx_isp_device *ispdev = container_of(dev, struct tx_isp_device, ldc_miscdev);

	(void)ispdev;
	switch (cmd) {
		case LDC_OFFLINE_CMD_CREATE_JOB:
			return lib_tx_isp_ldc_offline_create_job(arg);
		case LDC_OFFLINE_CMD_DESTROY_JOB:
			return lib_tx_isp_ldc_offline_destroy_job(arg);
		case LDC_OFFLINE_CMD_SUBMIT_TASK:
			return lib_tx_isp_ldc_offline_submit_task(arg);
		case LDC_OFFLINE_CMD_SUBMIT_TASK_EXT:
			return lib_tx_isp_ldc_offline_submit_task_ext(arg);
		default:
			ISP_ERROR("unsupport ldc ioctl cmd: 0x%08x\n", cmd);
			return -ENOIOCTLCMD;
	}
}

static struct file_operations ldc_fops = {
	.open = ldc_open,
	.release = ldc_release,
	.unlocked_ioctl = ldc_unlocked_ioctl,
};

/* isp-m0 api file operations (for /dev/isp-m0) */
static int tx_isp_api_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct tx_isp_device *ispdev = container_of(dev, struct tx_isp_device, isp_m0_miscdev);

	ISP_PRINTK("isp-m0 api open\n");
	(void)ispdev;
	return 0;
}

static int tx_isp_api_release(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct tx_isp_device *ispdev = container_of(dev, struct tx_isp_device, isp_m0_miscdev);

	ISP_PRINTK("isp-m0 api release\n");
	(void)ispdev;
	return 0;
}

static const char *tx_isp_tuning_node_name(struct file *file)
{
	struct miscdevice *dev = file ? file->private_data : NULL;

	if (!dev || !dev->name)
		return "unknown";

	return dev->name;
}

static struct tx_isp_device *tx_isp_tuning_node_ispdev(struct file *file)
{
	struct miscdevice *dev = file ? file->private_data : NULL;

	if (!dev || !dev->name)
		return NULL;

	if (!strcmp(dev->name, "isp-m0"))
		return container_of(dev, struct tx_isp_device, isp_m0_miscdev);
	if (!strcmp(dev->name, "isp-tuning"))
		return container_of(dev, struct tx_isp_device, isp_tuning_miscdev);

	return NULL;
}

static int tx_isp_tuning_in_use(struct tx_isp_device *ispdev)
{
	int in_use;

	if (!ispdev)
		return 0;

	mutex_lock(&ispdev->tuning_mlock);
	in_use = ispdev->tuning_opened_times > 0;
	mutex_unlock(&ispdev->tuning_mlock);

	return in_use;
}

static void tx_isp_tuning_free_par_buf(struct tx_isp_device *ispdev)
{
	if (!ispdev || !ispdev->tuning_par_buf)
		return;

	vfree(ispdev->tuning_par_buf);
	ispdev->tuning_par_buf = NULL;
	ispdev->tuning_par_buf_size = 0;
	ispdev->tuning_opened_times = 0;
}

static long tx_isp_tuning_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct tx_isp_device *ispdev = tx_isp_tuning_node_ispdev(file);
	const char *node_name = tx_isp_tuning_node_name(file);
	int is_isp_tuning = !strcmp(node_name, "isp-tuning");
	int ret = 0;

	/* ISP_PRINTK("%s ioctl: cmd=0x%x, arg=0x%lx\n", node_name, cmd, arg); */
	struct tisp_control control;

	/* Temporarily add, filtering for MASK/OSD/LINE API.
	   The return value 1 represents MASK/OSD/LINE */
	ret = lib_tx_isp_core_tuning_filter_draw(arg);

	if (((!is_isp_tuning && !ret) && tx_isp_tuning_in_use(ispdev))) {
		/* ISP_WARNING("ignore isp-m0 ioctl(0x%x), isp-tuning is active\n", cmd); */
		return -EBUSY;
	}

	switch (cmd) {
		case TISP_VIDIOC_S_CTRL:
			if (copy_from_user(&control, (void __user *)arg, sizeof(control)))
				return -EFAULT;
			/*ISP_WARNING("##### %s %d S_CTRL cmd=0x%08x #####\n", __func__,__LINE__, control.id);*/
			//              ret = tx_isp_core_ops_s_ctrl(ctrl.vinum, tuning, &control);
			break;
		case TISP_VIDIOC_G_CTRL:
			if (copy_from_user(&control, (void __user *)arg, sizeof(control)))
				return -EFAULT;
			/*ISP_WARNING("##### %s %d G_CTRL cmd=0x%08x #####\n", __func__,__LINE__, control.id);*/
			//             ret = tx_isp_core_ops_g_ctrl(ctrl.vinum, tuning, &control);
			if (ret && ret != -ENOIOCTLCMD)
				goto done;
			if (copy_to_user((void __user *)arg, &control, sizeof(control)))
				ret = -EFAULT;
			break;
		default:
			if (is_isp_tuning) {
				ret = lib_tx_isp_core_tunning_default_ioctl_ex(cmd, arg,
															   ispdev ? ispdev->tuning_par_buf : NULL,
															   ispdev ? ispdev->tuning_par_buf_size : 0);
			} else {
				ret = lib_tx_isp_core_tunning_default_ioctl(cmd, arg);
			}
			break;
	}

	ret = (ret == -ENOIOCTLCMD) ? 0 : ret;

done:
	return ret;
}

long isp_core_tunning_unlocked_ioctl_fast(int vinum, unsigned int get_or_set, unsigned int cmd, unsigned long arg)
{
	struct isp_image_tuning_default_ctrl ctrl = {
		.vinum = vinum,
		.dir = get_or_set,
		.control = {
			.id = cmd,
			.value = arg,
		}};

	return lib_tx_isp_core_tunning_default_ioctl(TISP_VIDIOC_DEFAULT_CMD_ISP_TUNING, (unsigned long)&ctrl);
}

static int tx_isp_tuning_open(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct tx_isp_device *ispdev = container_of(dev, struct tx_isp_device, isp_tuning_miscdev);
	int ret = 0;

	mutex_lock(&ispdev->tuning_mlock);
	if (ispdev->tuning_par_buf == NULL) {
		ispdev->tuning_par_buf = vmalloc(TX_ISP_TISP_IOCTL_SCRATCH_SIZE);
		if (!ispdev->tuning_par_buf) {
			ret = -ENOMEM;
			goto out;
		}
		ispdev->tuning_par_buf_size = TX_ISP_TISP_IOCTL_SCRATCH_SIZE;
	}
	ispdev->tuning_opened_times++;
out:
	mutex_unlock(&ispdev->tuning_mlock);

	if (ret)
		return ret;

	ISP_PRINTK("isp-tuning open\n");
	(void)ispdev;
	return ret;
}

static int tx_isp_tuning_release(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct tx_isp_device *ispdev = container_of(dev, struct tx_isp_device, isp_tuning_miscdev);

	mutex_lock(&ispdev->tuning_mlock);
	if (ispdev->tuning_opened_times > 0)
		ispdev->tuning_opened_times--;
	if (ispdev->tuning_opened_times == 0)
		tx_isp_tuning_free_par_buf(ispdev);
	mutex_unlock(&ispdev->tuning_mlock);

	ISP_PRINTK("isp-tuning release\n");
	(void)ispdev;
	return 0;
}

static struct file_operations tx_isp_api_fops = {
	.open = tx_isp_api_open,
	.release = tx_isp_api_release,
	.unlocked_ioctl = tx_isp_tuning_ioctl,
};

static struct file_operations tx_isp_tuning_fops = {
	.open = tx_isp_tuning_open,
	.release = tx_isp_tuning_release,
	.unlocked_ioctl = tx_isp_tuning_ioctl,
};

/* Proc node types */
enum isp_proc_node_type {
	ISP_PROC_W00 = 0,
	ISP_PROC_W01,
	ISP_PROC_W02,
	ISP_PROC_M0,
	ISP_PROC_TUNING,
	ISP_PROC_MSCA,
	ISP_PROC_LDC,
	ISP_PROC_IVDC,
	ISP_PROC_FS,
	ISP_PROC_MAX
};

/* Generic function: get proc node info into buffer
 * @type: node type (w00, w01, w02, m0, msca, ldc)
 * @buf: output buffer
 * @buf_size: buffer size
 * Returns the number of bytes written to buf
 */
static int isp_proc_get_info(enum isp_proc_node_type type, char *buf, size_t buf_size)
{
	int len = 0;
	int i;

	switch (type) {
		case ISP_PROC_W00: {
			for (i = 0; i < VIDEOIN_CMD_SENSOR_NUM; i++) {
				if (video_input_cmd_buf[i][0]) {
					len += snprintf(buf + len, buf_size - len,
									"sensor[%d]: %s\n", i,
									video_input_cmd_buf[i]);
					memset(video_input_cmd_buf[i], 0, VIDEOIN_CMD_BUF_SIZE);
				}
			}
			if (len == 0)
				len += lib_tx_isp_vin_debug_info(buf, buf_size);
			break;
		}
		case ISP_PROC_W01:
			len += lib_tx_isp_csi_debug_info(buf, buf_size);
			break;

		case ISP_PROC_W02:
			len += lib_tx_isp_vic_debug_info(buf, buf_size);
			break;

		case ISP_PROC_M0:
			len += lib_tx_isp_code_debug_info(buf, buf_size);
			break;

		case ISP_PROC_TUNING:
			len += lib_tx_isp_tuning_debug_info(buf, buf_size);
			break;

		case ISP_PROC_MSCA:
			len += lib_tx_isp_msca_debug_info(buf, buf_size);
			break;

		case ISP_PROC_LDC:
			len += lib_tx_isp_ldc_debug_info(buf, buf_size);
			break;

		case ISP_PROC_IVDC:
			len += lib_tx_isp_ivdc_debug_info(buf, buf_size);
			break;

		case ISP_PROC_FS:
			len += lib_tx_isp_frame_channel_debug_info(buf, buf_size);
			break;

		default:
			len += snprintf(buf + len, buf_size - len, "Unknown proc node type\n");
			break;
	}

	len += snprintf(buf + len, buf_size - len, "\n");
	return len;
}

static int isp_proc_show_common(struct seq_file *m, enum isp_proc_node_type type)
{
	struct tx_isp_device *ispdev = m ? m->private : NULL;
	char *buf;
	int len;
	size_t out_len;

	if (!ispdev || !ispdev->proc_buf) {
		ISP_ERROR("proc buffer is not initialized\n");
		return -ENOMEM;
	}

	buf = ispdev->proc_buf;
	mutex_lock(&ispdev->mlock);
	len = isp_proc_get_info(type, buf, ISP_PROC_SHOW_BUF_SIZE);
	if (len < 0) {
		mutex_unlock(&ispdev->mlock);
		return len;
	}

	out_len = (len >= ISP_PROC_SHOW_BUF_SIZE) ? (ISP_PROC_SHOW_BUF_SIZE - 1) : len;
	buf[out_len] = '\0';
	seq_write(m, buf, out_len);
	mutex_unlock(&ispdev->mlock);

	return 0;
}

/* Helper macro to generate proc show function using isp_proc_get_info */
#define DEFINE_ISP_PROC_SHOW(name, type)                     \
	static int name##_proc_show(struct seq_file *m, void *v) \
	{                                                        \
		return isp_proc_show_common(m, type);                \
	}

/* Generate show functions for each node */
DEFINE_ISP_PROC_SHOW(isp_w00, ISP_PROC_W00)
DEFINE_ISP_PROC_SHOW(isp_w01, ISP_PROC_W01)
DEFINE_ISP_PROC_SHOW(isp_w02, ISP_PROC_W02)
DEFINE_ISP_PROC_SHOW(isp_m0, ISP_PROC_M0)
DEFINE_ISP_PROC_SHOW(isp_tuning, ISP_PROC_TUNING)
DEFINE_ISP_PROC_SHOW(isp_msca, ISP_PROC_MSCA)
DEFINE_ISP_PROC_SHOW(isp_ldc, ISP_PROC_LDC)
DEFINE_ISP_PROC_SHOW(isp_ivdc, ISP_PROC_IVDC)
DEFINE_ISP_PROC_SHOW(isp_fs, ISP_PROC_FS)

static int isp_w00_get_sensor_index(const char **cmd)
{
	const char *ptr = *cmd;
	int index = 0;
	int max_index = VIDEOIN_CMD_SENSOR_NUM - 1;

	if (*ptr >= '0' && *ptr <= ('0' + max_index))
		index = *ptr++ - '0';
	while (*ptr == ' ')
		ptr++;
	*cmd = ptr;

	return index;
}

static int isp_w00_parse_u64(const char *cmd, unsigned long long *value, const char **endp)
{
	char token[32];
	const char *ptr = cmd;
	size_t len = 0;
	int ret;

	while (*ptr == ' ')
		ptr++;
	while (ptr[len] && ptr[len] != ' ' && ptr[len] != '\n' && ptr[len] != '\t')
		len++;
	if (!len || len >= sizeof(token))
		return -EINVAL;

	memcpy(token, ptr, len);
	token[len] = '\0';
	ret = kstrtoull(token, 0, value);
	if (ret)
		return ret;

	ptr += len;
	while (*ptr == ' ')
		ptr++;
	if (endp)
		*endp = ptr;

	return 0;
}
#if 0
static int isp_w00_proc_show(struct seq_file *m, void *v)
{
	struct tx_isp_device *ispdev = m ? m->private : NULL;
	int i;

	if (!ispdev)
		return -ENODEV;

	mutex_lock(&ispdev->mlock);
	for (i = 0; i < VIDEOIN_CMD_SENSOR_NUM; i++) {
		if (!video_input_cmd_buf[i][0])
			snprintf(video_input_cmd_buf[i], VIDEOIN_CMD_BUF_SIZE, "%s", "null");
		seq_printf(m, "------------>> sensor id: %d <<------------\n", i);
		seq_printf(m, "%s\n", video_input_cmd_buf[i]);
	}
	mutex_unlock(&ispdev->mlock);

	return 0;
}
#endif
/* isp_w00 proc file operations - Manual implementation */
static int isp_w00_proc_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, isp_w00_proc_show, pde_data(inode), 8192);
}

static ssize_t isp_w00_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	struct seq_file *m = file->private_data;
	struct tx_isp_device *ispdev = m ? m->private : NULL;
	struct tx_isp_dbg_register sensor_reg;
	char tmp[VIDEOIN_CMD_BUF_SIZE];
	const char *cmd = NULL;
	const char *next = NULL;
	unsigned long long reg = 0;
	unsigned long long val = 0;
	size_t len = count;
	int index = 0;
	int ret = 0;

	if (!ispdev)
		return -ENODEV;

	if (len >= sizeof(tmp))
		len = sizeof(tmp) - 1;
	if (copy_from_user(tmp, buf, len))
		return -EFAULT;
	tmp[len] = '\0';
	if (len && tmp[len - 1] == '\n')
		tmp[len - 1] = '\0';

	memset(&sensor_reg, 0, sizeof(sensor_reg));
	if (!strncmp(tmp, "r sen_reg", strlen("r sen_reg"))) {
		cmd = tmp + strlen("r sen_reg");
		index = isp_w00_get_sensor_index(&cmd);
		ret = isp_w00_parse_u64(cmd, &reg, NULL);
		if (ret)
			return ret;
		sensor_reg.sensor_id = index;
		sensor_reg.reg = reg;
		ret = tx_isp_read_sensor_register(&sensor_reg);

		mutex_lock(&ispdev->mlock);
		if (!ret)
			snprintf(video_input_cmd_buf[index], VIDEOIN_CMD_BUF_SIZE,
					 "reg:0x%llx, value:0x%llx, state:read ok",
					 sensor_reg.reg, sensor_reg.val);
		else
			snprintf(video_input_cmd_buf[index], VIDEOIN_CMD_BUF_SIZE,
					 "reg:0x%llx, value:null, state:read failed", sensor_reg.reg);
		mutex_unlock(&ispdev->mlock);
	} else if (!strncmp(tmp, "w sen_reg", strlen("w sen_reg"))) {
		cmd = tmp + strlen("w sen_reg");
		index = isp_w00_get_sensor_index(&cmd);
		ret = isp_w00_parse_u64(cmd, &reg, &next);
		if (ret)
			return ret;
		if (!*next)
			return -EINVAL;
		ret = isp_w00_parse_u64(next, &val, NULL);
		if (ret)
			return ret;

		sensor_reg.sensor_id = index;
		sensor_reg.reg = reg;
		sensor_reg.val = val;
		ret = tx_isp_write_sensor_register(&sensor_reg);

		mutex_lock(&ispdev->mlock);
		if (!ret)
			snprintf(video_input_cmd_buf[index], VIDEOIN_CMD_BUF_SIZE,
					 "reg:0x%llx, value:0x%llx, state:write ok",
					 sensor_reg.reg, sensor_reg.val);
		else
			snprintf(video_input_cmd_buf[index], VIDEOIN_CMD_BUF_SIZE,
					 "reg:0x%llx, value:null, state:write failed", sensor_reg.reg);
		mutex_unlock(&ispdev->mlock);
	} else {
		return count;
	}

	if (ret)
		ISP_ERROR("[ %s:%d ] sensor register access failed, cmd=%s ret=%d\n",
				  __func__, __LINE__, tmp, ret);

	return count;
}

static const ISP_PROC_FOPS isp_w00_proc_fops = {
	ISP_PROC_OWNER
		ISP_PROC_OPEN(isp_w00_proc_open)
			ISP_PROC_READ(seq_read)
				ISP_PROC_WRITE(isp_w00_proc_write)
					ISP_PROC_LSEEK(seq_lseek)
						ISP_PROC_RELEASE(single_release)};

/* isp_w01 proc file operations - Manual implementation */
static int isp_w01_proc_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, isp_w01_proc_show, pde_data(inode), 8192);
}

static ssize_t isp_w01_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	char tmp[32];
	if (count >= sizeof(tmp))
		count = sizeof(tmp) - 1;
	if (copy_from_user(tmp, buf, count))
		return -EFAULT;
	tmp[count] = '\0';
	ISP_PRINTK("isp_w01 write: %s\n", tmp);
	return count;
}

static const ISP_PROC_FOPS isp_w01_proc_fops = {
	ISP_PROC_OWNER
		ISP_PROC_OPEN(isp_w01_proc_open)
			ISP_PROC_READ(seq_read)
				ISP_PROC_WRITE(isp_w01_proc_write)
					ISP_PROC_LSEEK(seq_lseek)
						ISP_PROC_RELEASE(single_release)};

/* isp_w02 proc file operations - Manual implementation */
static int isp_w02_proc_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, isp_w02_proc_show, pde_data(inode), 8192);
}

static ssize_t isp_w02_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	char tmp[32];
	if (count >= sizeof(tmp))
		count = sizeof(tmp) - 1;
	if (copy_from_user(tmp, buf, count))
		return -EFAULT;
	tmp[count] = '\0';
	ISP_PRINTK("isp_w02 write: %s\n", tmp);
	return count;
}

static const ISP_PROC_FOPS isp_w02_proc_fops = {
	ISP_PROC_OWNER
		ISP_PROC_OPEN(isp_w02_proc_open)
			ISP_PROC_READ(seq_read)
				ISP_PROC_WRITE(isp_w02_proc_write)
					ISP_PROC_LSEEK(seq_lseek)
						ISP_PROC_RELEASE(single_release)};

/* isp_m0 proc file operations - Manual implementation */
static int isp_m0_proc_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, isp_m0_proc_show, pde_data(inode), 8192);
}

static ssize_t isp_m0_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	char tmp[32];
	if (count >= sizeof(tmp))
		count = sizeof(tmp) - 1;
	if (copy_from_user(tmp, buf, count))
		return -EFAULT;
	tmp[count] = '\0';
	ISP_PRINTK("isp_m0 write: %s\n", tmp);
	return count;
}

static const ISP_PROC_FOPS isp_m0_proc_fops = {
	ISP_PROC_OWNER
		ISP_PROC_OPEN(isp_m0_proc_open)
			ISP_PROC_READ(seq_read)
				ISP_PROC_WRITE(isp_m0_proc_write)
					ISP_PROC_LSEEK(seq_lseek)
						ISP_PROC_RELEASE(single_release)};

static int isp_tuning_proc_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, isp_tuning_proc_show, pde_data(inode), ISP_PROC_SHOW_BUF_SIZE);
}

static const ISP_PROC_FOPS isp_tuning_proc_fops = {
	ISP_PROC_OWNER
		ISP_PROC_OPEN(isp_tuning_proc_open)
			ISP_PROC_READ(seq_read)
				ISP_PROC_LSEEK(seq_lseek)
					ISP_PROC_RELEASE(single_release)};

/* isp_msca proc file operations - Manual implementation */
static int isp_msca_proc_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, isp_msca_proc_show, pde_data(inode), 8192);
}

static ssize_t isp_msca_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	char tmp[32];
	if (count >= sizeof(tmp))
		count = sizeof(tmp) - 1;
	if (copy_from_user(tmp, buf, count))
		return -EFAULT;
	tmp[count] = '\0';
	ISP_PRINTK("isp_msca write: %s\n", tmp);
	return count;
}

static const ISP_PROC_FOPS isp_msca_proc_fops = {
	ISP_PROC_OWNER
		ISP_PROC_OPEN(isp_msca_proc_open)
			ISP_PROC_READ(seq_read)
				ISP_PROC_WRITE(isp_msca_proc_write)
					ISP_PROC_LSEEK(seq_lseek)
						ISP_PROC_RELEASE(single_release)};

/* isp_ldc proc file operations - Manual implementation */
static int isp_ldc_proc_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, isp_ldc_proc_show, pde_data(inode), 8192);
}

/* isp_ivdc proc file operations - Manual implementation */
static int isp_ivdc_proc_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, isp_ivdc_proc_show, pde_data(inode), 8192);
}

/* isp-fs proc file operations - Manual implementation */
static int isp_fs_proc_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, isp_fs_proc_show, pde_data(inode), 8192);
}

static ssize_t isp_ldc_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	char tmp[32];
	if (count >= sizeof(tmp))
		count = sizeof(tmp) - 1;
	if (copy_from_user(tmp, buf, count))
		return -EFAULT;
	tmp[count] = '\0';
	ISP_PRINTK("isp_ldc write: %s\n", tmp);
	return count;
}

static ssize_t isp_ivdc_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	char tmp[32];
	if (count >= sizeof(tmp))
		count = sizeof(tmp) - 1;
	if (copy_from_user(tmp, buf, count))
		return -EFAULT;
	tmp[count] = '\0';
	ISP_PRINTK("isp_ivdc write: %s\n", tmp);
	return count;
}

static ssize_t isp_fs_proc_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
	char tmp[32];
	if (count >= sizeof(tmp))
		count = sizeof(tmp) - 1;
	if (copy_from_user(tmp, buf, count))
		return -EFAULT;
	tmp[count] = '\0';
	ISP_PRINTK("isp_fs write: %s\n", tmp);
	return count;
}

static const ISP_PROC_FOPS isp_ldc_proc_fops = {
	ISP_PROC_OWNER
		ISP_PROC_OPEN(isp_ldc_proc_open)
			ISP_PROC_READ(seq_read)
				ISP_PROC_WRITE(isp_ldc_proc_write)
					ISP_PROC_LSEEK(seq_lseek)
						ISP_PROC_RELEASE(single_release)};

static const ISP_PROC_FOPS isp_ivdc_proc_fops = {
	ISP_PROC_OWNER
		ISP_PROC_OPEN(isp_ivdc_proc_open)
			ISP_PROC_READ(seq_read)
				ISP_PROC_WRITE(isp_ivdc_proc_write)
					ISP_PROC_LSEEK(seq_lseek)
						ISP_PROC_RELEASE(single_release)};

static const ISP_PROC_FOPS isp_fs_proc_fops = {
	ISP_PROC_OWNER
		ISP_PROC_OPEN(isp_fs_proc_open)
			ISP_PROC_READ(seq_read)
				ISP_PROC_WRITE(isp_fs_proc_write)
					ISP_PROC_LSEEK(seq_lseek)
						ISP_PROC_RELEASE(single_release)};

static int tx_isp_probe(struct platform_device *pdev)
{
	struct tx_isp_device *ispdev = NULL;
	struct tx_isp_module *module = NULL;
	void *fwDev = NULL;
	struct resource *res = NULL;
	int ret = ISP_SUCCESS;
	uint8_t index = 0;

	ispdev = (struct tx_isp_device *)kmalloc(sizeof(*ispdev), GFP_KERNEL);
	if (!ispdev) {
		ISP_ERROR("[ %s:%d ] Failed to allocate camera device\n", __func__, __LINE__);
		ret = -ENOMEM;
		goto exit;
	}
	memset(ispdev, 0, sizeof(*ispdev));

	/* init self */
	spin_lock_init(&ispdev->slock);
	mutex_init(&ispdev->mlock);
	mutex_init(&ispdev->tuning_mlock);
	tx_isp_fw_completion_init(&ispdev->pm_completion);
	ispdev->pm_mode = TISP_NORMAL_MODE;
	ispdev->proc_buf = (char *)kmalloc(ISP_PROC_SHOW_BUF_SIZE, GFP_KERNEL);
	if (!ispdev->proc_buf) {
		ISP_ERROR("Failed to allocate proc buffer\n");
		ret = -ENOMEM;
		goto failed_proc_buf_alloc;
	}

	module = &ispdev->module;
	ret = tx_isp_module_init(pdev, module);
	if (ret) {
		ISP_ERROR("[ %s:%d ] Failed to init isp module(%s)\n", __func__, __LINE__, pdev->name);
		ret = -ENOMEM;
		goto failed_module_init;
	}
	ispdev->dev = &(pdev->dev);

	tx_isp_set_module_nodeops(module, &tx_isp_fops);
	/*tx_isp_set_module_debugops(&ispdev->module, &tx_isp_debug_fops);*/
	// if (zeratul_open) {
	// 	static int ingenic_static_tx_isp_minor = 71;
	// 	module->miscdev.minor = ingenic_static_tx_isp_minor;
	// } else

	/* Register misc-ivdc device */
#ifdef CONFIG_ZERATUL
	ispdev->ivdc_miscdev.minor = 70;
#else
	ispdev->ivdc_miscdev.minor = MISC_DYNAMIC_MINOR;
#endif /* CONFIG_ZERATUL */
	ispdev->ivdc_miscdev.name = "misc-ivdc";
	ispdev->ivdc_miscdev.fops = &ivdc_fops;
	ret = misc_register(&ispdev->ivdc_miscdev);
	if (ret < 0) {
		ret = -ENOENT;
		ISP_ERROR("[ %s:%d ] Failed to register misc-ivdc miscdev!\n", __func__, __LINE__);
		goto failed_ivdc_misc_register;
	}

#ifdef CONFIG_ZERATUL
	ispdev->ldc_miscdev.minor = 73;
#else
	ispdev->ldc_miscdev.minor = MISC_DYNAMIC_MINOR;
#endif /* CONFIG_ZERATUL */
	ispdev->ldc_miscdev.name = "offline_ldc";
	ispdev->ldc_miscdev.fops = &ldc_fops;
	ret = misc_register(&ispdev->ldc_miscdev);
	if (ret < 0) {
		ret = -ENOENT;
		ISP_ERROR("[ %s:%d ] Failed to register offline_ldc miscdev!\n", __func__, __LINE__);
		goto failed_ldc_misc_register;
	}

#ifdef CONFIG_ZERATUL
	ispdev->miscdev.minor = 71;
#else
	ispdev->miscdev.minor = MISC_DYNAMIC_MINOR;
#endif /* CONFIG_ZERATUL */
	ispdev->miscdev.name = "tx-isp";
	ispdev->miscdev.fops = &tx_isp_fops;

	ret = misc_register(&ispdev->miscdev);
	if (ret < 0) {
		ret = -ENOENT;
		ISP_ERROR("[ %s:%d ] Failed to register tx-isp miscdev!\n", __func__, __LINE__);
		goto failed_misc_register;
	}

	/* Register isp-m0 device (tuning interface) */
	ispdev->isp_m0_miscdev.name = "isp-m0";
#ifdef CONFIG_ZERATUL
	ispdev->isp_m0_miscdev.minor = 61;
#else
	ispdev->isp_m0_miscdev.minor = MISC_DYNAMIC_MINOR;
#endif /* CONFIG_ZERATUL */
	ispdev->isp_m0_miscdev.fops = &tx_isp_api_fops;
	ret = misc_register(&ispdev->isp_m0_miscdev);
	if (ret < 0) {
		ret = -ENOENT;
		ISP_ERROR("[ %s:%d ] Failed to register isp-m0 miscdev!\n", __func__, __LINE__);
		goto failed_isp_m0_misc_register;
	}
	/* Register isp-tuning device (poll/read interface) */
#ifdef CONFIG_ZERATUL
	ispdev->isp_tuning_miscdev.minor = 72;
#else
	ispdev->isp_tuning_miscdev.minor = MISC_DYNAMIC_MINOR;
#endif /* CONFIG_ZERATUL */
	ispdev->isp_tuning_miscdev.name = "isp-tuning";
	ispdev->isp_tuning_miscdev.fops = &tx_isp_tuning_fops;
	ret = misc_register(&ispdev->isp_tuning_miscdev);
	if (ret < 0) {
		ret = -ENOENT;
		ISP_ERROR("[ %s:%d ] Failed to register isp-tuning miscdev!\n", __func__, __LINE__);
		goto failed_isp_tuning_misc_register;
	}
	ispdev->proc = jz_proc_mkdir("isp");
	if (!ispdev->proc) {
		ispdev->proc = NULL;
		ISP_ERROR("[ %s:%d ] Failed to create debug directory of tx-isp!\n", __func__, __LINE__);
		goto failed_to_proc;
	}

	/* Create proc entries with private data */
	ispdev->proc_w00 = proc_create_data("isp-w00", S_IRUGO | S_IWUGO, ispdev->proc, &isp_w00_proc_fops, ispdev);
	if (!ispdev->proc_w00) {
		ISP_ERROR("[ %s:%d ] Failed to create isp-w00 proc entry!\n", __func__, __LINE__);
	}

	ispdev->proc_w01 = proc_create_data("isp-w01", S_IRUGO | S_IWUGO, ispdev->proc, &isp_w01_proc_fops, ispdev);
	if (!ispdev->proc_w01) {
		ISP_ERROR("[ %s:%d ] Failed to create isp-w01 proc entry!\n", __func__, __LINE__);
	}

	ispdev->proc_w02 = proc_create_data("isp-w02", S_IRUGO | S_IWUGO, ispdev->proc, &isp_w02_proc_fops, ispdev);
	if (!ispdev->proc_w02) {
		ISP_ERROR("[ %s:%d ] Failed to create isp-w02 proc entry!\n", __func__, __LINE__);
	}

	ispdev->proc_m0 = proc_create_data("isp-m0", S_IRUGO | S_IWUGO, ispdev->proc, &isp_m0_proc_fops, ispdev);
	if (!ispdev->proc_m0) {
		ISP_ERROR("[ %s:%d ] Failed to create isp-m0 proc entry!\n", __func__, __LINE__);
	}

	ispdev->proc_tuning = proc_create_data("isp-tuning", S_IRUGO, ispdev->proc, &isp_tuning_proc_fops, ispdev);
	if (!ispdev->proc_tuning) {
		ISP_ERROR("[ %s:%d ] Failed to create isp-tuning proc entry!\n", __func__, __LINE__);
	}

	ispdev->proc_msca = proc_create_data("isp-msca", S_IRUGO | S_IWUGO, ispdev->proc, &isp_msca_proc_fops, ispdev);
	if (!ispdev->proc_msca) {
		ISP_ERROR("[ %s:%d ] Failed to create isp-msca proc entry!\n", __func__, __LINE__);
	}

	/* Create isp_ldc proc entry */
	ispdev->proc_ldc = proc_create_data("isp-ldc", S_IRUGO | S_IWUGO, ispdev->proc, &isp_ldc_proc_fops, ispdev);
	if (!ispdev->proc_ldc) {
		ISP_ERROR("[ %s:%d ] Failed to create isp-ldc proc entry!\n", __func__, __LINE__);
	}

	ispdev->proc_ivdc = proc_create_data("isp-ivdc", S_IRUGO | S_IWUGO, ispdev->proc, &isp_ivdc_proc_fops, ispdev);
	if (!ispdev->proc_ivdc) {
		ISP_ERROR("[ %s:%d ] Failed to create isp-ivdc proc entry!\n", __func__, __LINE__);
	}

	ispdev->proc_fs = proc_create_data("isp-fs", S_IRUGO | S_IWUGO, ispdev->proc, &isp_fs_proc_fops, ispdev);
	if (!ispdev->proc_fs) {
		ISP_ERROR("[ %s:%d ] Failed to create isp-fs proc entry!\n", __func__, __LINE__);
	}

	platform_set_drvdata(pdev, ispdev);

	ret = tx_isp_request_irq(pdev, ispdev->irqdev, ARRAY_SIZE(ispdev->irqdev), &ispdev->irq_num);
	if (ret) {
		ISP_ERROR("[ %s:%d ] Failed to request irq!\n", __func__, __LINE__);
		goto failed_irq;
	}

	for (index = 0; index < pdev->num_resources; index++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, index);
		if (res && !strcmp(res->name, TX_ISP_DEV_NAME)) {
			// ISP_PRINTK("index is %d, name is %s\n", index, pdev->resource[index].name);
			res = request_mem_region(res->start, res->end - res->start + 1, dev_name(&pdev->dev));
			if (!res) {
				ISP_ERROR("[ %s:%d ] Not enough memory for resources\n", __func__, __LINE__);
				ret = -EBUSY;
				goto failed_req;
			}

			ispdev->res[index] = res;
			ispdev->base[index] = ioremap(res->start, res->end - res->start + 1);
			// ISP_PRINTK("ioremap addr: %p, size: 0x%08x\n", ispdev->base[index], res->end - res->start + 1);
			if (!ispdev->base[index]) {
				ISP_ERROR("[ %s:%d ] Unable to ioremap registers\n", __func__, __LINE__);
				ret = -ENXIO;
				goto err_ioremap;
			}
		} else {
			res = NULL;
		}
	}

	ispdev->module.ops_type = TISP_OPS_KERNEL;
	globe_ispdev = ispdev;

	fwDev = tx_isp_fw_device_probe(ispdev);
	ispdev->fwDev = fwDev;

	ispdev->version = TX_ISP_FIRMWARE_VERSION;

	ISP_WARNING("@@@@ tx-isp-probe ok (fw:%s drv:%s) @@@@\n",
				ispdev->version, TX_ISP_DRIVER_VERSION);

	return ISP_SUCCESS;

err_ioremap:
failed_req:
	tx_isp_release_iomem(ispdev);
	tx_isp_free_irqs(ispdev);
failed_irq:
	platform_set_drvdata(pdev, NULL);
	tx_isp_remove_proc_entries(ispdev);
failed_to_proc:
	if (ispdev->fwDev) {
		kfree(ispdev->fwDev);
		ispdev->fwDev = NULL;
	}
	misc_deregister(&ispdev->isp_tuning_miscdev);
failed_isp_tuning_misc_register:
	misc_deregister(&ispdev->isp_m0_miscdev);
failed_isp_m0_misc_register:
	misc_deregister(&ispdev->miscdev);
failed_misc_register:
	misc_deregister(&ispdev->ldc_miscdev);
failed_ldc_misc_register:
	misc_deregister(&ispdev->ivdc_miscdev);
failed_ivdc_misc_register:
failed_module_init:
	tx_isp_tuning_free_par_buf(ispdev);
	if (ispdev->proc_buf) {
		kfree(ispdev->proc_buf);
		ispdev->proc_buf = NULL;
	}
	tx_isp_fw_completion_deinit(&ispdev->pm_completion);
failed_proc_buf_alloc:
	if (globe_ispdev == ispdev)
		globe_ispdev = NULL;
	kfree(ispdev);
exit:
	return ret;
}

static void tx_isp_remove(struct platform_device *pdev)
{
	struct tx_isp_device *ispdev = platform_get_drvdata(pdev);
	struct tx_isp_module *module = NULL;

	if (!ispdev)
		return;
	module = &ispdev->module;

	/* Remove proc entries first (must be before removing parent dir) */
	tx_isp_remove_proc_entries(ispdev);
	if (ispdev->proc_buf) {
		kfree(ispdev->proc_buf);
		ispdev->proc_buf = NULL;
	}
	tx_isp_fw_completion_deinit(&ispdev->pm_completion);
	misc_deregister(&ispdev->isp_tuning_miscdev);
	misc_deregister(&ispdev->isp_m0_miscdev);
	misc_deregister(&ispdev->miscdev);
	misc_deregister(&ispdev->ldc_miscdev);
	misc_deregister(&ispdev->ivdc_miscdev);
	tx_isp_free_irqs(ispdev);
	tx_isp_release_iomem(ispdev);
	tx_isp_memory_pool_free(ispdev);
	if (ispdev->fwDev) {
		kfree(ispdev->fwDev);
		ispdev->fwDev = NULL;
	}
	tx_isp_tuning_free_par_buf(ispdev);
	platform_set_drvdata(pdev, NULL);
	if (globe_ispdev == ispdev)
		globe_ispdev = NULL;
	kfree(ispdev);
}

static int tx_isp_resume_early(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tx_isp_device *ispdev = platform_get_drvdata(pdev);
	struct tx_isp_cmd_argdata init;
	int ret = ISP_SUCCESS;

	if ((ispdev == NULL) || (ispdev->module.mode != TISP_MODE_ONESHOT)) {
		ISP_ERROR("tx_isp_prepare error !!!, ispdev is %p, mode is %d\n", ispdev, ispdev->module.mode);
		return -1;
	}

	ret = tx_isp_set_clk(ispdev);
	if (ret) {
		return ret;
	}

	init.vinum = 0;
	init.enable = 1;

	return lib_tx_isp_pm_resume_early(init.vinum, ispdev->module.mode, &init);
}

static void tx_isp_complete(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tx_isp_device *ispdev = platform_get_drvdata(pdev);
	struct tx_isp_cmd_argdata init;

	if ((ispdev == NULL) || (ispdev->module.mode != TISP_MODE_ONESHOT)) {
		ISP_ERROR("tx_isp_prepare error !!!, ispdev is %p, mode is %d\n", ispdev, ispdev->module.mode);
	}

	init.vinum = 0;
	init.enable = 1;
	lib_tx_isp_pm_complete(init.vinum, ispdev->module.mode, &init);
}

static int tx_isp_prepare(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tx_isp_device *ispdev = platform_get_drvdata(pdev);
	struct tx_isp_cmd_argdata init;

	ISP_PRINTK("tx_isp_prepare\n");
	if ((ispdev == NULL) || (ispdev->module.mode != TISP_MODE_ONESHOT)) {
		ISP_ERROR("tx_isp_prepare error !!!, ispdev is %p, mode is %d\n", ispdev, ispdev->module.mode);
		return -1;
	}

	init.vinum = 0;
	init.enable = 1;

	ISP_PRINTK("tx_isp_prepare\n");
	return lib_tx_isp_pm_prepare(init.vinum, ispdev->module.mode, &init);
}

static int tx_isp_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tx_isp_device *ispdev = platform_get_drvdata(pdev);
	struct tx_isp_cmd_argdata init;
	int ret = 0;

	ISP_PRINTK("tx_isp_suspend, ispdev is %p, mode is %d\n", ispdev, ispdev->module.mode);
	if ((ispdev == NULL) || (ispdev->module.mode != TISP_MODE_ONESHOT)) {
		ISP_ERROR("tx_isp_suspend error !!!, ispdev is %p, mode is %d\n", ispdev, ispdev->module.mode);
		return -1;
	}

	init.vinum = 0;
	init.enable = 1;

	ret = lib_tx_isp_pm_suspend(init.vinum, ispdev->module.mode, &init);
	if (ret) {
		return ret;
	}

	tx_isp_disable_clk(ispdev);
	ISP_PRINTK("tx_isp_suspend end\n");
	return 0;
}

static int tx_isp_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tx_isp_device *ispdev = platform_get_drvdata(pdev);
	struct tx_isp_cmd_argdata init;
	int ret = 0;

	ISP_PRINTK("tx_isp_resume, ispdev is %p, mode is %d\n", ispdev, ispdev->module.mode);
	if ((ispdev == NULL) || (ispdev->module.mode != TISP_MODE_ONESHOT)) {
		ISP_ERROR("tx_isp_resume error !!!, ispdev is %p, mode is %d\n", ispdev, ispdev->module.mode);
		return 0;
	}

	init.vinum = 0;
	init.enable = 1;
	ret = lib_tx_isp_pm_resume(init.vinum, ispdev->module.mode, &init);
	ISP_PRINTK("tx_isp_resume end\n");
	return ret;
}

static struct dev_pm_ops tx_isp_pm_ops = {
	.resume_early = tx_isp_resume_early,
	.suspend = tx_isp_suspend,
	.resume = tx_isp_resume,
	.prepare = tx_isp_prepare,
	.complete = tx_isp_complete,
};

static struct platform_driver tx_isp_driver = {
	.probe = tx_isp_probe,
	.remove = tx_isp_remove,
	.driver =
		{
			.name = "tx-isp",
			.owner = NULL,
			.pm = &tx_isp_pm_ops,
		},
};

int tx_isp_init(void)
{
	int32_t ret = 0;

	ret = platform_device_register(&tx_isp_platform_device);

	if (ret) {
		ISP_WARNING("[ %s:%d ] Failed to insmod isp driver!!!\n", __func__, __LINE__);
		return ret;
	}

#ifndef CONFIG_ZERATUL
	tx_isp_driver.driver.owner = THIS_MODULE;
#else
	tx_isp_driver.driver.owner = NULL;
#endif

	ret = platform_driver_register(&tx_isp_driver);
	if (ret) {
		platform_device_unregister(&tx_isp_platform_device);
		return ret;
	}

	return 0;
}

void tx_isp_exit(void)
{
	platform_driver_unregister(&tx_isp_driver);
	platform_device_unregister(&tx_isp_platform_device);
}
