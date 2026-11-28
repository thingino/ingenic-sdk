/*
 * Ingenic TNPU driver
 *
 * Copyright (c) 2023 LiuTianyang
 *
 * This file is released under the GPLv2
 */

#include "tnpu_drv.h"
#include "tnpu_register.h"

#include "tnpu_boot_code.h"

#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/file.h>
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/dma-mapping.h>
#include <linux/console.h>
#include <linux/workqueue.h>
#include <asm/r4kcache.h>
#include <asm/mipsregs.h>

#ifdef CONFIG_TNPU_CORE_DEBUG
#define PRINT_TNPU_DEBUG(fmt, ...) \
	printk(KERN_DEBUG "<TNPU> " fmt, ##__VA_ARGS__)
#else
#define PRINT_TNPU_DEBUG(fmt, ...)
#endif

/* exported by arch/mips/mm/c-r4k.c, not declared in a header */
extern void (*r4k_blast_dcache)(void);

/*
 * The vendor polls the cores a fixed number of times, ~2.6 ms (sleep) and
 * ~330 us (bus idle) on silicon, which answers within the first polls. A
 * count is no bound on time: where each register read is slow (cores in
 * other QEMU processes) it expires before they sleep. Bound by the clock.
 */
#define TNPU_POLL_US              200000

/* console_drivers is not exported on 7.x: the console UART is a parameter */
#define TNPU_UART0_IOBASE         0x10030000
#define TNPU_UART_STRIDE          0x1000
static unsigned int tnpu_uart = 1;
module_param_named(uart, tnpu_uart, uint, 0444);
MODULE_PARM_DESC(uart, "UART (0-2) the TNPU cores print to, the console's (default 1)");

#define TNPU_R_RISCV_32           1
#define TNPU_R_RISCV_RELATIVE     3
#define TNPU_R_RISCV_JUMP_SLOT    5

//#define TNPU_ISOLATE_CLK

#ifndef TNPU_ISOLATE_CLK
#include <linux/clk.h>
#endif

struct tnpu_seqno {
	uint64_t can;
	uint64_t end;
};

struct tnpu_libref {
	struct list_head               node;
	unsigned int                   paddr;
	pid_t                          pid;
};

struct tnpu_sharelib {
	struct list_head               node;
	char                           name[64];
	int                            ref_cnt;
	unsigned int                   size;
	void                          *kva;
	unsigned int                   paddr;
	Elf32_Dyn                     *dyn;
	char                          *strtab;
	Elf32_Sym                     *symtab;
	unsigned int                   sym_num;
	unsigned int                   need_lib[4];
	unsigned int                   need_lib_num;
};

enum tnpu_task_status {
	TNPU_TASK_NONE = 0,
	TNPU_TASK_INIT = 1,
	TNPU_TASK_RUN  = 2,
};

struct tnpu_task {
	enum tnpu_task_status status;
	unsigned int device;
	unsigned int pc;
	unsigned int arg[TNPU_MAX_ARG];
	unsigned int start_time_l;
	unsigned int start_time_h;
	unsigned int stop_time_l;
	unsigned int stop_time_h;
};

struct tnpu_task_queue {
	uint64_t                       id;
	unsigned int                   nice;
	pid_t                          pid;
	struct tnpu_task               task[2];
	struct tnpu_task_queue        *prev;
	struct tnpu_task_queue        *next;
	unsigned int                   flush_cache;
	unsigned int                   oram_addr;
	unsigned int                   oram_size;
	/* Keep sched/run hot fields above wait state. */
	struct tnpu_seqno              submit_seqno;
	wait_queue_head_t              submit_wait;
};

struct tnpu_core_dev {
	struct device                  *dev;
	void __iomem                   *ccu_regs;
	void __iomem                   *dram_regs;
	int                             tnpu_irq;
	struct mutex                    load_mutex;
	struct list_head                libref_list;
	struct list_head                sharelib_list;
	int (*libread)(struct file *elf,
			unsigned int uvaddr, loff_t off,
			void *dst_vaddr, size_t size);
#ifndef TNPU_ISOLATE_CLK
	struct clk                     *div_tnpu;
	struct clk                     *gate_tnpu;
#endif
	int                             is_open;

	int                             sched_pool[TNPU_TASK_SCHED_POOL_SIZE];
	spinlock_t                      task_spin;
	unsigned int                    device;
	unsigned char                   send_idx[2];
	unsigned char                   send_max[2];
	unsigned char                   send_flag;
	unsigned long                   v1_sp_kvaddr;
	wait_queue_head_t               submit_queue_wait;
	struct tnpu_seqno               submit_queue_seqno;
	struct tnpu_task_queue          task_queue[TNPU_MAX_TASK];
	unsigned int                    oram_head;
	unsigned int                    oram_tail;
};

static struct tnpu_core_dev tnpu;

#ifdef CONFIG_TNPU_PERF_FILE
#define TNPU_PERF_LOG_MAX 8
struct tnpu_perf_task_info {
	unsigned int pc;
	unsigned int device;
	unsigned long long start_time;
	unsigned long long end_time;
};
struct tnpu_perf_job_info {
	uint64_t                       id;
	pid_t                          pid;
	unsigned int                   nice;
	struct tnpu_perf_task_info     task[2];
};
static struct tnpu_perf_job_info       tnpu_perf[TNPU_PERF_LOG_MAX];
static unsigned int                    tnpu_perf_index;
static spinlock_t                      tnpu_perf_spin;
static struct workqueue_struct        *perf_workqueue;
void perf_work_handler(struct work_struct *work);
static DECLARE_WORK(perf_work, perf_work_handler);

void perf_work_handler(struct work_struct *work)
{
	struct file *perf_file = NULL;
	loff_t pos = 0;
	char buffer[256];
	int len;
	int i, j;
	unsigned long task_irqflags;
	struct tnpu_perf_job_info perf_copy[TNPU_PERF_LOG_MAX];
	unsigned int count;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
	mm_segment_t old_fs;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
#endif

	for (;;) {
		spin_lock_irqsave(&tnpu_perf_spin, task_irqflags);
		count = tnpu_perf_index > TNPU_PERF_LOG_MAX ? TNPU_PERF_LOG_MAX : tnpu_perf_index;
		memcpy(perf_copy, tnpu_perf, count * sizeof(struct tnpu_perf_job_info));
		tnpu_perf_index = 0;
		spin_unlock_irqrestore(&tnpu_perf_spin, task_irqflags);

		if (count == 0)
			break;

		if (!perf_file) {
			perf_file = filp_open(CONFIG_TNPU_PERF_FILE,
					O_WRONLY | O_CREAT | O_APPEND, 0644);
			if (IS_ERR(perf_file)) {
				printk(KERN_ERR "Failed to open %s\n", CONFIG_TNPU_PERF_FILE);
				perf_file = NULL;
				break;
			}
		}

		for (i = 0; i < count; i++) {
			struct tnpu_perf_job_info *info = &perf_copy[i];

			len = snprintf(buffer, sizeof(buffer),
					"ID: %llu, PID: %d, NICE: %u\n",
					info->id, info->pid, info->nice);
			if (len > 0) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
				kernel_write(perf_file, buffer, len, pos);
#else
				kernel_write(perf_file, buffer, len, &pos);
#endif
			}

			for (j = 0; j < 2; j++) {
				struct tnpu_perf_task_info *task = &info->task[j];
				if (task->pc != 0) {
					len = snprintf(buffer, sizeof(buffer),
							"  Core%d: PC=0x%x, Device=0x%x, Start=%llu, End=%llu, Duratio=%llu\n",
							j, task->pc, task->device,
							task->start_time, task->end_time,
							task->end_time - task->start_time);
					if (len > 0) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
						kernel_write(perf_file, buffer, len, pos);
#else
						kernel_write(perf_file, buffer, len, &pos);
#endif
					}
				}
			}
			len = snprintf(buffer, sizeof(buffer), "---\n");
			if (len > 0) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
				kernel_write(perf_file, buffer, len, pos);
#else
				kernel_write(perf_file, buffer, len, &pos);
#endif
			}
		}
	}

	if (perf_file)
		filp_close(perf_file, NULL);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
	set_fs(old_fs);
#endif
}

static void tnpu_perf_set(struct tnpu_task_queue *queue)
{
	struct tnpu_perf_job_info *perf_info;

	if (!perf_workqueue)
		return;

	spin_lock(&tnpu_perf_spin);

	perf_info = &tnpu_perf[tnpu_perf_index % TNPU_PERF_LOG_MAX];
	perf_info->id = queue->id;
	perf_info->pid = queue->pid;
	perf_info->nice = queue->nice;
	perf_info->task[0].pc = 0;
	if (queue->task[0].pc) {
		perf_info->task[0].pc = queue->task[0].pc;
		perf_info->task[0].device = queue->task[0].device;
		perf_info->task[0].start_time =
				((unsigned long long)queue->task[0].start_time_h << 32) |
				queue->task[0].start_time_l;
		perf_info->task[0].end_time =
				((unsigned long long)queue->task[0].stop_time_h << 32) |
				queue->task[0].stop_time_l;
	}
	perf_info->task[1].pc = 0;
	if (queue->task[1].pc) {
		perf_info->task[1].pc = queue->task[1].pc;
		perf_info->task[1].device = queue->task[1].device;
		perf_info->task[1].start_time =
				((unsigned long long)queue->task[1].start_time_h << 32) |
				queue->task[1].start_time_l;
		perf_info->task[1].end_time =
				((unsigned long long)queue->task[1].stop_time_h << 32) |
				queue->task[1].stop_time_l;
	}
	tnpu_perf_index++;
	spin_unlock(&tnpu_perf_spin);
	queue_work(perf_workqueue, &perf_work);
}
#endif // CONFIG_TNPU_PERF_FILE

static void __iomem *tnpu_ioremap_regs(struct platform_device *pdev, int index)
{
	struct resource *res;
	void __iomem *map;

	res = platform_get_resource(pdev, IORESOURCE_MEM, index);
	map = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(map)) {
		PRINT_TNPU_ERROR("Failed to map registers: %ld\n", PTR_ERR(map));
		return map;
	}

	return map;
}

/* called with the task spinlock held too: no sleeping */
static int tnpu_poll(volatile unsigned int *reg, unsigned int mask)
{
	ktime_t end = ktime_add_us(ktime_get(), TNPU_POLL_US);

	for (;;) {
		bool late = ktime_after(ktime_get(), end);

		if ((*reg & mask) == mask)
			return 0;
		if (late)
			return -ETIME;
		udelay(1);
	}
}

static int tnpu_hardware_sleep(void)
{
	if (tnpu_poll(TNPU_CCU_CCSR, TNPU_CCU_CCSR_SLEEP_MASK)) {
		PRINT_TNPU_ERROR("Hardware init timeout: CCSR=0x%x\n", *TNPU_CCU_CCSR);
		return -ETIME;
	}

	return 0;
}

static int tnpu_hardware_bus_idle(void)
{
	*TNPU_CCU_BECR |= TNPU_CCU_BECR_SET_MASK;

	if (tnpu_poll(TNPU_CCU_BECR, TNPU_CCU_BECR_ALL_IDLE_MASK)) {
		PRINT_TNPU_ERROR("Wait for bus idle failed: BECR=0x%x\n", *TNPU_CCU_BECR);
		return -ETIME;
	}

	return 0;
}

static inline void tnpu_init_dram_task_mem(void)
{
	memset((void *)(TNPU_DRAM_TASK), 0, TNPU_DRAM_TASK_TOTAL_SIZE);
}

static void tnpu_hardware_load(void)
{
	unsigned int v1_sp;
	unsigned int core_boot_addr = (unsigned int)TNPU_DRAM_BOOT_START & 0x1fffffff;
	/* the UART the cores print to: the vendor takes the console's, UART1 unless it is 0 or 2 */
	unsigned int uart_ioaddr = TNPU_UART0_IOBASE + tnpu_uart * TNPU_UART_STRIDE;

	tnpu_init_dram_task_mem();
	memcpy((void *)TNPU_DRAM_BOOT_START,
			(void *)tnpu_boot_code, sizeof(tnpu_boot_code));

	v1_sp = (unsigned int)virt_to_phys((void *)tnpu.v1_sp_kvaddr);
	if (!v1_sp) {
		PRINT_TNPU_ERROR("v1 stack alloc failed, size=0x%x\n", TNPU_V1_SP_SIZE);
		return;
	}
	v1_sp += 0x1000;

	*TNPU_DRAM_BOOT_KDATA_V0_SP = TNPU_V0_SP_TOP;
	*TNPU_DRAM_BOOT_KDATA_V1_SP = v1_sp | 0x80000000;
	*TNPU_DRAM_BOOT_KDATA_UART_BASE = uart_ioaddr;
	*TNPU_DRAM_BOOT_KDATA_VERSION_BASE = TNPU_DRIVER_VERSION;

	*TNPU_CCU_CRER(0) = core_boot_addr;
#ifndef TNPU_T32
	*TNPU_CCU_CRER(1) = core_boot_addr + 0x80;
#endif

	/* Configure PMA area to control the accessible range of v0
	 *
	 * default :
	 * PMA_ADDRESS0=0x0
	 * PMA_ADDRESS1=0x0
	 * PMA_ADDRESS2=0x0
	 * PMA_ADDRESS3=0x0
	 * PMA_ADDRESS4=0x0
	 * PMA_ADDRESS5=0x23ffffff // 0x8000_0000~0x9000_0000
	 * PMA_ADDRESS6=0x5ffffff  // 0x1000_0000~0x1FFFF_FFF
	 * PMA_ADDRESS7=0xfffffff  // 0x0000_0000~0x7FFFF_FFFF
	 * PMA_CONFIG0=0x7
	 * PMA_CONFIG1=0x7
	 * PMA_CONFIG2=0x7
	 * PMA_CONFIG3=0x7
	 * PMA_CONFIG4=0x7
	 * PMA_CONFIG5=0x3f
	 * PMA_CONFIG6=0x1f        // enable w/r/x
	 * PMA_CONFIG7=0x3f        // enable cach/w/r/x
	 */
	// *TNPU_CCU_PMA_ADDRESS(7) = 0x0bffffff; // 0x2000_0000~0x4000_0000
	// *TNPU_CCU_PMA_CONFIG(7) = 0x3f;
}

static int tnpu_hardware_reset(void)
{
	int i;

	*TNPU_CCU_SOFT_RESET = TNPU_CCU_SOFT_RESET_CMD_SET;
	*TNPU_CCU_BECR &= ~TNPU_CCU_BECR_SET_MASK;

	tnpu_init_dram_task_mem();
	for (i = 0; i < 2; i++) {
		tnpu.send_idx[i] = 0;
		tnpu.send_max[i] = TNPU_MAX_TASK_SEND;
	}

	*TNPU_CCU_TOHOST(0) = 0;
	*TNPU_CCU_TOHOST(1) = 0;
	*TNPU_CCU_LOCK(0) = 0;
	*TNPU_CCU_LOCK(1) = 0;
	*TNPU_CCU_LOCK(2) = 0;
	*TNPU_CCU_LOCK(3) = 0;
	//ndelay(10);

	/* Set CCU_CCSR intc enable, v0:bit1 v1:bit2 */
	*TNPU_CCU_CCSR |= 0b10;
	*TNPU_CCU_CCSR &= ~0b100;
	/* Set CCU_INTC_MASK_H to unmask, nna:bit9 cva:bit10, lut:bit11, dram:bit13 */
	*TNPU_CCU_INTC_MASK_H = 0x2E00;

	*TNPU_CCU_SOFT_RESET = TNPU_CCU_SOFT_RESET_CMD_RELEASE;
	//*TNPU_CCU_SOFT_RESET = TNPU_CCU_SOFT_RESET_CMD_RELEASE_V0;
	//*TNPU_CCU_SOFT_RESET = TNPU_CCU_SOFT_RESET_CMD_RELEASE_V1;

	return 0;
}

int tnpu_hardware_suspend(void)
{
	unsigned long irqflags;
	unsigned int init_cnt = 0, run_cnt = 0;
	int ret;
	int i, j;

	if (!tnpu.is_open) {
		return 0;
	}

	spin_lock_irqsave(&tnpu.task_spin, irqflags);
	for (i = 0; i < TNPU_MAX_TASK; i++) {
		for (j = 0; j < 2; j++) {
			if (tnpu.task_queue[i].task[j].status == TNPU_TASK_INIT)
				init_cnt++;
			else if (tnpu.task_queue[i].task[j].status == TNPU_TASK_RUN)
				run_cnt++;
		}
	}
	spin_unlock_irqrestore(&tnpu.task_spin, irqflags);

	if (init_cnt || run_cnt) {
		PRINT_TNPU_ERROR("PM suspend busy: init=%u, run=%u, seq=%llu/%llu\n",
				init_cnt, run_cnt,
				(unsigned long long)tnpu.submit_queue_seqno.can,
				(unsigned long long)tnpu.submit_queue_seqno.end);
		return -EBUSY;
	}

	ret = tnpu_hardware_bus_idle();
	if (ret)
		return ret;

	/* tnpu_hardware_sleep() is handled by tnpu_clk_close(). */
	return 0;
}

int tnpu_hardware_resume(void)
{
	int ret;
	unsigned long irqflags;
	int i, j;

	if (!tnpu.is_open) {
		PRINT_TNPU_ERROR("PM resume requested with TNPU clock closed\n");
		return -EIO;
	}

	*TNPU_CCU_AXIQOS |= 0x8800ff;

	tnpu_hardware_load();
	ret = tnpu_hardware_reset();
	if (ret)
		return ret;

	ret = tnpu_hardware_sleep();
	if (ret)
		return ret;

	spin_lock_irqsave(&tnpu.task_spin, irqflags);
	tnpu.send_flag = 1;
	tnpu.device = 0;
	tnpu.submit_queue_seqno.can = 0ll;
	tnpu.submit_queue_seqno.end = 0ll;
	for (i = 0; i < TNPU_TASK_SCHED_POOL_SIZE; i++) {
		tnpu.sched_pool[i] = 1 << i;
	}
	for (i = 0; i < TNPU_MAX_TASK; i++) {
		struct tnpu_task_queue *queue = &tnpu.task_queue[i];

		queue->nice = 0;
		queue->id = 0;
		queue->pid = 0;
		queue->task[0].status = TNPU_TASK_NONE;
		queue->task[1].status = TNPU_TASK_NONE;
		queue->task[0].device = 0;
		queue->task[1].device = 0;
		queue->task[0].pc = 0;
		queue->task[1].pc = 0;
		for (j = 0; j < TNPU_MAX_ARG; j++) {
			queue->task[0].arg[j] = 0;
			queue->task[1].arg[j] = 0;
		}
		queue->task[0].start_time_l = 0;
		queue->task[0].start_time_h = 0;
		queue->task[0].stop_time_l = 0;
		queue->task[0].stop_time_h = 0;
		queue->task[1].start_time_l = 0;
		queue->task[1].start_time_h = 0;
		queue->task[1].stop_time_l = 0;
		queue->task[1].stop_time_h = 0;
		queue->prev = NULL;
		queue->next = NULL;
		queue->flush_cache = 0;
		queue->oram_addr = 0;
		queue->oram_size = 0;
		queue->submit_seqno.can = 0ll;
		queue->submit_seqno.end = 0ll;
	}
	tnpu.oram_head = TNPU_ORAM_PADDR;
	tnpu.oram_tail = TNPU_ORAM_PADDR + TNPU_ORAM_SIZE;
	spin_unlock_irqrestore(&tnpu.task_spin, irqflags);

	return 0;
}

static unsigned int tnpu_oram_alloc(unsigned int size)
{
	if (size > (tnpu.oram_tail - tnpu.oram_head)) {
		return 0;
	}

	if (tnpu.oram_head == TNPU_ORAM_PADDR) {
		tnpu.oram_head += size;
		return TNPU_ORAM_PADDR;
	}

	tnpu.oram_tail -= size;
	return tnpu.oram_tail;
}

static unsigned int  tnpu_oram_free(unsigned int oram_addr)
{
	if (!oram_addr)
		return 0;

	if (oram_addr == TNPU_ORAM_PADDR) {
		tnpu.oram_head = TNPU_ORAM_PADDR;
	} else if (oram_addr == tnpu.oram_tail) {
		tnpu.oram_tail = TNPU_ORAM_PADDR + TNPU_ORAM_SIZE;
	}

	return 0;
}

static int tnpu_device_conflict(struct tnpu_task_queue *queue, int sel)
{
	int i;
	for (i = 0; i < 2; i++) {
		if (sel & 1) {
			if (tnpu.send_max[i] == 0) {
				return 1;
			}
			if (tnpu.device & queue->task[i].device) {
				return 1;
			}
		}
		sel = sel >> 1;
	}

	if (queue->oram_size && !queue->oram_addr) {
		queue->oram_addr = tnpu_oram_alloc(queue->oram_size);
		if (queue->oram_addr == 0) {
			return 1;
		}
	}

	return 0;
}

static int tnpu_job_sched(void)
{
	int i, idx, have_low_priority = 0;
	int task_idx = -1, last_nice = -1, last_low_nice = TNPU_TASK_NICE_MAX;
	uint64_t last_id = 0;
	struct tnpu_task_queue *queue;

	for (i = 0; i < TNPU_MAX_TASK; i++) {
		enum tnpu_task_status status0, status1;
		unsigned int nice;
		uint64_t id;

		queue = tnpu.task_queue + i;
		status0 = queue->task[0].status;
		status1 = queue->task[1].status;
		if ((status0 == TNPU_TASK_INIT) ||
			(status1 == TNPU_TASK_INIT)) {

			if (queue->prev) {
				// Not the head of the chain
				continue;
			}

			nice = queue->nice;
			id = queue->id;
			if (tnpu.sched_pool[nice]) {
				if ((int)nice > last_nice) {
					task_idx = i;
					last_id = id;
					last_nice = nice;
				} else if (nice == last_nice) {
					if (id <= last_id) {
						task_idx = i;
						last_id = id;
					}
				}
			}
			if ((int)nice < last_low_nice)
				last_low_nice = nice;
		}
	}
	if ((task_idx >= 0) && (last_nice != TNPU_TASK_NICE_MAX)) {
		have_low_priority = last_nice != last_low_nice;
		idx = last_nice;

		if (have_low_priority && tnpu.sched_pool[idx]) {
			tnpu.sched_pool[idx]--;
		}

		for (i = idx + 1; i < TNPU_TASK_NICE_MAX; i++) {
			tnpu.sched_pool[i] = 1 << i;
		}
	}

	return task_idx;
}

static void tnpu_job_run(struct tnpu_task_queue *queue, int idx, int sel)
{
	int i, send_idx = 0;
	struct tnpu_task *task = NULL;

	for (i = 0; i < 2; i++) {
		if (sel & 1) {
			queue->task[i].status = TNPU_TASK_RUN;
			queue->nice = TNPU_TASK_NICE_MAX;
			tnpu.device |= queue->task[i].device;

			task = queue->task + i;
			send_idx = tnpu.send_idx[i] & (TNPU_MAX_TASK_SEND - 1);
			PRINT_TNPU_DEBUG("run send_idx=%d, i=%d, pc=0x%x, queue=0x%x, pid=%d\n",
					send_idx, i, task->pc, (unsigned int)queue, queue->pid);

			*TNPU_DRAM_TASK_ID        (i, send_idx) = idx;
			*TNPU_DRAM_TASK_DEVICE    (i, send_idx) = task->device;
			*TNPU_DRAM_TASK_PC        (i, send_idx) = task->pc;
			*TNPU_DRAM_TASK_ARG0      (i, send_idx) = task->arg[0];
			*TNPU_DRAM_TASK_ARG1      (i, send_idx) = task->arg[1];
			*TNPU_DRAM_TASK_ARG2      (i, send_idx) = task->arg[2];
			*TNPU_DRAM_TASK_ARG3      (i, send_idx) = task->arg[3];
			*TNPU_DRAM_TASK_ORAM_ADDR (i, send_idx) = queue->oram_addr;

			tnpu.send_idx[i]++;
			tnpu.send_max[i]--;

			*TNPU_CCU_FROMHOST(i) = 1;
		}
		sel = sel >> 1;
	}
}

static void tnpu_job_remove(struct tnpu_task_queue *queue, bool remove_all)
{
	int send_idx, i;
	int is_remove = 0;

	for (i = 0; i < 2; i++) {
		if (queue->task[i].status == TNPU_TASK_RUN) {
			is_remove |= (1 << i);
			tnpu.send_idx[i]--;
			send_idx = tnpu.send_idx[i] & (TNPU_MAX_TASK_SEND - 1);
			PRINT_TNPU_DEBUG("remove send_idx=%d, i=%d, pid=%d, pc=0x%x\n",
					send_idx, i, queue->pid, queue->task[i].pc);
			//*TNPU_DRAM_TASK_ID        (i, send_idx) = 0;
			//*TNPU_DRAM_TASK_DEVICE    (i, send_idx) = 0;
			//*TNPU_DRAM_TASK_PC        (i, send_idx) = 0;
			//*TNPU_DRAM_TASK_ORAM_ADDR (i, send_idx) = 0;

			queue->task[i].status = TNPU_TASK_NONE;
			tnpu.device &= ~queue->task[i].device;
			//tnpu.send_max[i]++;  // reset
		} else if (remove_all && queue->task[i].status != TNPU_TASK_NONE) {
			is_remove |= (1 << i);
			queue->task[i].status = TNPU_TASK_NONE;
		}
	}

	if (is_remove) {
		queue->pid = 0;
		tnpu_oram_free(queue->oram_addr);
		queue->oram_addr = 0;
		queue->oram_size = 0;
		if (queue->prev) {
			queue->prev->next = queue->next;
		}
		if (queue->next) {
			queue->next->prev = queue->prev;
		}
		queue->prev = NULL;
		queue->next = NULL;
		queue->flush_cache = 0;
		queue->submit_seqno.end++;
		tnpu.submit_queue_seqno.end++;
		wake_up_all(&tnpu.submit_queue_wait);
	}

}

/*
 * The vendor blasts L1 when CP0 ErrCtl bit 29 says L1 and L2 are connected,
 * and otherwise syncs the uncached DRAM window, which writes nothing back.
 * Writing back L1 always covers both.
 */
static void tnpu_job_sync(void)
{
	r4k_blast_dcache();
}

static void tnpu_job_send(void)
{
	int task_idx;
	struct tnpu_task_queue *queue;
	int sel;


	while (tnpu.send_flag) {
		task_idx = tnpu_job_sched();
		if (task_idx < 0) {
			break;
		}
		queue = tnpu.task_queue + task_idx;

		sel = queue->task[0].status;
		sel |= (queue->task[1].status << 1);

		if (tnpu_device_conflict(queue, sel)) {
			break;
		}

		if (queue->flush_cache) {
			tnpu_job_sync();
		}

		tnpu_job_run(queue, task_idx, sel);
	}
}

static void tnpu_task_finish(unsigned int core, unsigned int tohost)
{
	struct tnpu_task_queue *queue;
	unsigned int task_id = (tohost & 0xff) - 1;

	queue = tnpu.task_queue + task_id;

	if (tohost >> 16) {
		unsigned int send_idx = (tnpu.send_idx[core] - 1) & (TNPU_MAX_TASK_SEND - 1);
		unsigned int flag = (tohost >> 16) & 0xf;
		unsigned int pending = *TNPU_CCU_INTC_PENDING;

		PRINT_TNPU_ERROR("FUNC RETURN: core=%d, tohost=0x%x, idx=0x%x\n", core, tohost, send_idx);

		switch (flag) {
		case TNPU_TOHOST_RET_STACK:
			PRINT_TNPU_ERROR("Stack overflow, please reduce stack size in tnpu func.\n");
			break;
		case TNPU_TOHOST_RET_INTC:
			PRINT_TNPU_ERROR("Hardware exception: INTC_PENDING=0x%x\n", pending);
			if (pending & 0x200) {
				PRINT_TNPU_ERROR("NNA(bit9,mask=0x200) ERROR!\n");
			}
			if (pending & 0x400) {
				PRINT_TNPU_ERROR("CVA(bit10,mask=0x400) ERROR!\n");
			}
			if (pending & 0x800) {
				PRINT_TNPU_ERROR("LUT(bit11,mask=0x800) ERROR!\n");
			}
			if (pending & 0x2000) {
				PRINT_TNPU_ERROR("DRAM(bit13,mask=0x2000) ERROR!\n");
			}
			PRINT_TNPU_ERROR("Reboot to reload\n");
			break;
		default:
			PRINT_TNPU_ERROR("Error PC=0x%x, please check lib!\n", queue->task[core].pc);
			PRINT_TNPU_ERROR("mepc=0x%x,mtval=0x%x,type=0x%x\n",
				*TNPU_DRAM_TASK_ARG0(core, send_idx),
				*TNPU_DRAM_TASK_ARG1(core, send_idx),
				*TNPU_DRAM_TASK_ARG2(core, send_idx));
		}
	}

#ifdef CONFIG_TNPU_PERF_FILE
	if  (perf_workqueue) {
		unsigned int send_idx = (tnpu.send_idx[core] - 1) & (TNPU_MAX_TASK_SEND - 1);
		queue->task[core].start_time_l = *TNPU_DRAM_TASK_START_TIME_L(core, send_idx);
		queue->task[core].start_time_h = *TNPU_DRAM_TASK_START_TIME_H(core, send_idx);
		queue->task[core].stop_time_l = *TNPU_DRAM_TASK_STOP_TIME_L(core, send_idx);
		queue->task[core].stop_time_h = *TNPU_DRAM_TASK_STOP_TIME_H(core, send_idx);
	}
#endif


	queue->task[core].status = TNPU_TASK_NONE;
	if (queue->task[!core].status == TNPU_TASK_NONE) {
#ifdef CONFIG_TNPU_PERF_FILE
		tnpu_perf_set(queue);
#endif
		if (queue->next) {
			queue->next->prev = NULL;
			queue->next = NULL;
		}

		queue->pid = 0;
		tnpu_oram_free(queue->oram_addr);
		queue->oram_addr = 0;

		queue->submit_seqno.end++;
		wake_up_all(&queue->submit_wait);

		tnpu.submit_queue_seqno.end++;
		wake_up_all(&tnpu.submit_queue_wait);
	}

	tnpu.device &= ~queue->task[core].device;
	tnpu.send_max[core]++;

#ifdef CONFIG_TNPU_PERF_FILE
	queue->task[core].pc = 0;
#endif
}


static irqreturn_t tnpu_irq_handler(int irq, void *data)
{
	unsigned int v0_tohost = *TNPU_CCU_TOHOST(0);
	unsigned int v1_tohost = *TNPU_CCU_TOHOST(1);
	irqreturn_t ret = IRQ_WAKE_THREAD;

	PRINT_TNPU_DEBUG("IRQ: V0=0x%x, V1=0x%x\n", v0_tohost, v1_tohost);

	if (!v0_tohost && !v1_tohost) {
		return IRQ_HANDLED;
	}

	spin_lock(&tnpu.task_spin);

	if (v0_tohost) {
		*TNPU_CCU_TOHOST(0) = 0;
		tnpu_task_finish(0, v0_tohost);
	}

	if (v1_tohost) {
		*TNPU_CCU_TOHOST(1) = 0;
		tnpu_task_finish(1, v1_tohost);
	}
	if (tnpu.submit_queue_seqno.can == tnpu.submit_queue_seqno.end) {
		ret = IRQ_HANDLED;
	}
	spin_unlock(&tnpu.task_spin);

	return ret;
}

static irqreturn_t tnpu_irq_thread(int irq, void *dev)
{
	spin_lock(&tnpu.task_spin);

	tnpu_job_send();

	spin_unlock(&tnpu.task_spin);

	return IRQ_HANDLED;
}

void tnpu_timer_open(void)
{
	*TNPU_CCU_CCSR |= 1 << 5;
	*TNPU_CCU_TIME_L = 0;
	*TNPU_CCU_TIME_H = 0;
}

void tnpu_timer_close(void)
{
	*TNPU_CCU_CCSR &= ~(1 << 5);
}

unsigned long long tnpu_timer_get(void)
{
	return ((unsigned long long)(*TNPU_CCU_TIME_H) << 32) | *TNPU_CCU_TIME_L;
}

int tnpu_clk_open(void)
{
#ifndef TNPU_ISOLATE_CLK
	int ret;
#endif

	mutex_lock(&tnpu.load_mutex);
	if (tnpu.is_open) {
		mutex_unlock(&tnpu.load_mutex);
		return 0;
	}

	tnpu.is_open = 1;

#ifndef TNPU_ISOLATE_CLK
	ret = clk_prepare_enable(tnpu.div_tnpu);
	if (ret) {
		PRINT_TNPU_ERROR("Failed to open div_tnpu, %d\n", ret);
		goto err_unlock;
	}
	ret = clk_prepare_enable(tnpu.gate_tnpu);
	if (ret) {
		PRINT_TNPU_ERROR("Failed to open gate_tnpu, %d\n", ret);
		clk_disable_unprepare(tnpu.div_tnpu);
		goto err_unlock;
	}
#endif
	mutex_unlock(&tnpu.load_mutex);

	return 0;

#ifndef TNPU_ISOLATE_CLK
err_unlock:
	tnpu.is_open = 0;
	mutex_unlock(&tnpu.load_mutex);
	return ret;
#endif
}

void tnpu_clk_close(void)
{
	mutex_lock(&tnpu.load_mutex);
	if (tnpu.is_open) {
		int ret;

		ret = tnpu_hardware_sleep();
		if (ret)
			PRINT_TNPU_ERROR("Clock close before sleep failed, ret=%d\n", ret);

#ifndef TNPU_ISOLATE_CLK
		clk_disable_unprepare(tnpu.gate_tnpu);
		clk_disable_unprepare(tnpu.div_tnpu);
#endif
		tnpu.is_open = 0;
	}
	mutex_unlock(&tnpu.load_mutex);
}

#ifndef TNPU_ISOLATE_CLK
#define TNPU_PARENT_CLK_TARGET_RATE    1000000000UL
#define TNPU_PARENT_CLK_MAX_RATE       1088000000UL

static const char * const tnpu_parent_clk_names[] = {
	"sclka",
	"mpll",
	"vpll",
};

static unsigned long tnpu_clk_rate_diff(unsigned long rate,
		unsigned long target_rate)
{
	if (rate > target_rate)
		return rate - target_rate;

	return target_rate - rate;
}

static int tnpu_parent_clk_get_rate(const char *clk_name,
		unsigned long *clk_rate)
{
	struct clk *clk;

	clk = clk_get(tnpu.dev, clk_name);
	if (IS_ERR(clk)) {
		PRINT_TNPU_ERROR("Failed to get parent clk %s\n", clk_name);
		return PTR_ERR(clk);
	}

	*clk_rate = clk_get_rate(clk);
	clk_put(clk);

	return 0;
}

static const char *tnpu_find_nearest_1000m_parent_clk(void)
{
	int i;
	int ret;
	const char *best_name = NULL;
	unsigned long best_diff = ~0UL;
	unsigned long rate;
	unsigned long diff;

	for (i = 0; i < ARRAY_SIZE(tnpu_parent_clk_names); i++) {
		ret = tnpu_parent_clk_get_rate(tnpu_parent_clk_names[i], &rate);
		if (ret)
			continue;

		/* Ignore parent rates above the validated TNPU clock limit. */
		if (rate > TNPU_PARENT_CLK_MAX_RATE)
			continue;

		/* Among valid parents, prefer the one nearest to 1GHz. */
		diff = tnpu_clk_rate_diff(rate, TNPU_PARENT_CLK_TARGET_RATE);
		if (!best_name || diff < best_diff) {
			best_name = tnpu_parent_clk_names[i];
			best_diff = diff;
		}
	}

	if (!best_name) {
		PRINT_TNPU_ERROR("No valid parent clk <= %luMHz in sclka/mpll/vpll\n",
				TNPU_PARENT_CLK_MAX_RATE / 1000000);
		return NULL;
	}

	return best_name;
}
#endif /* TNPU_ISOLATE_CLK */

static int tnpu_clk_init(
		const char *parent_clk_name,
		unsigned long tnpu_clk_rate)
{
#ifndef TNPU_ISOLATE_CLK
	int ret;
	unsigned long rate;
	unsigned long parent_rate;
	const char *selected_parent_clk_name;
	struct clk *parent_clk;
	struct clk *clk;

	selected_parent_clk_name = parent_clk_name;
	if (!parent_clk_name || !*parent_clk_name) {
		selected_parent_clk_name = tnpu_find_nearest_1000m_parent_clk();
		if (!selected_parent_clk_name)
			return -EINVAL;
	}

	/*
	 * The CGU's TNPU clock is the vendor's mux_tnpu, div_tnpu and gate_tnpu
	 * in one; its parents are the node's other clocks, by the vendor names
	 */
	clk = devm_clk_get(tnpu.dev, "tnpu");
	if (IS_ERR(clk)) {
		PRINT_TNPU_ERROR("Failed to get the tnpu clock\n");
		return PTR_ERR(clk);
	}
	parent_clk = devm_clk_get(tnpu.dev, selected_parent_clk_name);
	if (IS_ERR(parent_clk)) {
		PRINT_TNPU_ERROR("Failed to get parent clock %s\n", selected_parent_clk_name);
		return PTR_ERR(parent_clk);
	}
	tnpu.div_tnpu = clk;
	tnpu.gate_tnpu = clk;

	parent_rate = clk_get_rate(parent_clk);
	rate = tnpu_clk_rate;
	/* Divider /1 is unsupported; cap v0 at half the parent rate. */
	if (parent_rate <= rate)
		rate = parent_rate / 2;
	PRINT_TNPU_INFO("config: parent=%s(%luMHz), target=%luMHz, v0_rate=%luMHz\n",
			selected_parent_clk_name, parent_rate / 1000000,
			tnpu_clk_rate / 1000000, rate / 1000000);

	ret = clk_set_parent(clk, parent_clk);
	if (ret) {
		PRINT_TNPU_ERROR("Failed to set parent clock, %d\n", ret);
		return ret;
	}

	/* div_tnpu and gate_tnpu are the same clock: enabled twice, as tnpu_clk_close() drops both */
	ret = clk_prepare_enable(clk);
	if (ret) {
		PRINT_TNPU_ERROR("Failed to open the tnpu clock, %d\n", ret);
		return ret;
	}
	ret = clk_prepare_enable(clk);
	if (ret) {
		PRINT_TNPU_ERROR("Failed to open the tnpu clock, %d\n", ret);
		goto err_disable;
	}

	ret = clk_set_rate(clk, rate);
	if (ret) {
		PRINT_TNPU_ERROR("Failed to set tnpu clock rate, %d\n", ret);
		goto err_disable2;
	}
	rate = clk_get_rate(clk);
	parent_rate = clk_get_rate(parent_clk);
	ret = *TNPU_CCU_UMPID;
	if ((ret & 0xffff) < 0x150) {
		PRINT_TNPU_ERROR("Device mismatch, UMPID=0x%x\n", ret);
		ret = -EINVAL;
		goto err_disable2;
	}
	PRINT_TNPU_INFO("ready: parent=%s(%luMHz), v0_rate=%luMHz\n",
			selected_parent_clk_name, parent_rate / 1000000,
			rate / 1000000);
#endif /* TNPU_ISOLATE_CLK */

	return 0;

#ifndef TNPU_ISOLATE_CLK
err_disable2:
	clk_disable_unprepare(clk);
err_disable:
	clk_disable_unprepare(clk);
	return ret;
#endif /* TNPU_ISOLATE_CLK */
}

int tnpu_core_init(
		struct platform_device *pdev,
		const char *parent_clk_name,
		unsigned long tnpu_clk_rate)
{
	int ret;
	int i, j;

	tnpu.dev = &pdev->dev;
	init_waitqueue_head(&tnpu.submit_queue_wait);
	tnpu.submit_queue_seqno.can = 0ll;
	tnpu.submit_queue_seqno.end = 0ll;
	spin_lock_init(&tnpu.task_spin);
	mutex_init(&tnpu.load_mutex);
	INIT_LIST_HEAD(&tnpu.libref_list);
	INIT_LIST_HEAD(&tnpu.sharelib_list);

	tnpu.ccu_regs = tnpu_ioremap_regs(pdev, 0);
	if (IS_ERR(tnpu.ccu_regs))
		return PTR_ERR(tnpu.ccu_regs);

	tnpu.dram_regs = tnpu_ioremap_regs(pdev, 1);
	if (IS_ERR(tnpu.dram_regs))
		return PTR_ERR(tnpu.dram_regs);

	tnpu.tnpu_irq = platform_get_irq(pdev, 0);
	if (tnpu.tnpu_irq < 0)
		return tnpu.tnpu_irq;

	//ret = devm_request_irq(&pdev->dev, tnpu.tnpu_irq,
	//		tnpu_irq_handler, IRQF_SHARED, "tnpu-irq", &tnpu);
	ret = devm_request_threaded_irq(&pdev->dev, tnpu.tnpu_irq,
			tnpu_irq_handler,
			tnpu_irq_thread,
			IRQF_ONESHOT, "tnpu-irq", &tnpu);
	if (ret) {
		PRINT_TNPU_ERROR("request_irq fail\n");
		return ret;
	}

	ret = tnpu_clk_init(parent_clk_name, tnpu_clk_rate);
	if (ret) {
		return ret;
	}


	for (i = 0; i < TNPU_MAX_TASK; i++) {
		tnpu.task_queue[i].nice = 0;
		tnpu.task_queue[i].id = 0;
		tnpu.task_queue[i].pid = 0;
		for (j = 0; j < 2; j++) {
			tnpu.task_queue[i].task[j].status = TNPU_TASK_NONE;
			tnpu.task_queue[i].task[j].device = 0;
			tnpu.task_queue[i].task[j].pc = 0;
		}
		tnpu.task_queue[i].oram_addr = 0;
		tnpu.task_queue[i].oram_size = 0;
		tnpu.task_queue[i].submit_seqno.can = 0ll;
		tnpu.task_queue[i].submit_seqno.end = 0ll;
		init_waitqueue_head(&tnpu.task_queue[i].submit_wait);
	}

	for (i = 0; i < TNPU_TASK_SCHED_POOL_SIZE; i++) {
		tnpu.sched_pool[i] = 1 << i;
	}


	tnpu.send_flag = 1;
	tnpu.device = 0;

	tnpu.v1_sp_kvaddr = __get_free_page(GFP_KERNEL);
	if (!tnpu.v1_sp_kvaddr) {
		PRINT_TNPU_ERROR("Failed to alloc v1 sp\n");
		return -ENOMEM;
	}

	tnpu.oram_head = TNPU_ORAM_PADDR;
	tnpu.oram_tail = TNPU_ORAM_PADDR + TNPU_ORAM_SIZE;

	// Configure the AXI priority of v0 as the highest
	*TNPU_CCU_AXIQOS |= 0x8800ff;

	tnpu_hardware_load();
	tnpu_hardware_reset();
	tnpu.is_open = 1;
	tnpu_clk_close();

#ifdef CONFIG_TNPU_PERF_FILE
	perf_workqueue = create_singlethread_workqueue("perf_workqueue");
	if (!perf_workqueue) {
		printk(KERN_ERR "Failed to create workqueue\n");
		return 0;
	}

	spin_lock_init(&tnpu_perf_spin);
	tnpu_perf_index = 0;
#endif

	return 0;
}

void tnpu_core_remove(void)
{
	struct tnpu_sharelib *lib, *tmp_lib;
	struct tnpu_libref *libref, *tmp_libref;

	#ifdef CONFIG_TNPU_PERF_FILE
	if (perf_workqueue) {
		flush_workqueue(perf_workqueue);
		destroy_workqueue(perf_workqueue);
		perf_workqueue = NULL;
	}
	#endif

	mutex_lock(&tnpu.load_mutex);
	list_for_each_entry_safe(libref, tmp_libref, &tnpu.libref_list, node) {
		list_del(&libref->node);
		kfree(libref);
	}
	list_for_each_entry_safe(lib, tmp_lib, &tnpu.sharelib_list, node) {
		list_del(&lib->node);
		if (lib->kva)
			tnpu_cma_free(lib->size, lib->kva, lib->paddr);
		kfree(lib);
	}
	mutex_unlock(&tnpu.load_mutex);

	tnpu_clk_close();

	free_page(tnpu.v1_sp_kvaddr);
	tnpu.v1_sp_kvaddr = 0;
}

static int tnpu_wait_submit_queue(
		struct tnpu_seqno *seqno,
		uint64_t wait_seqno)
{
	return (seqno->can - seqno->end) < (TNPU_MAX_TASK - (wait_seqno - 1));
}

static int tnpu_wait_submit_seqno(
		struct tnpu_seqno *seqno,
		uint64_t wait_seqno)
{
	if (!wait_seqno) wait_seqno = seqno->can;
	PRINT_TNPU_DEBUG("wait: end=%lld >= can=%lld\n", seqno->end, seqno->can);
	return (seqno->end >= wait_seqno);
}

static int tnpu_wait(uint64_t timeout_us,
		bool interruptible,
		bool restartsys,
		uint64_t wait_seqno,
		struct tnpu_seqno *seqno,
		wait_queue_head_t *wait_queue,
		int (*comp)(struct tnpu_seqno *seqno, uint64_t wait_seqno))
{
	int ret = 0;
	unsigned long timeout_expire;
	unsigned long timeout_jiffies;
	DEFINE_WAIT(wait);

	if (comp(seqno, wait_seqno))
		return 0;

	if (timeout_us == 0)
		return -ETIME;

	timeout_jiffies = usecs_to_jiffies(timeout_us);
	if (timeout_jiffies < 2)
		timeout_jiffies = 2;
	timeout_expire = jiffies + timeout_jiffies;

	for (;;) {
		prepare_to_wait(wait_queue, &wait,
				interruptible ? TASK_INTERRUPTIBLE :
				TASK_UNINTERRUPTIBLE);
		if (interruptible && signal_pending(current)) {
			if (__fatal_signal_pending(current)) {
				PRINT_TNPU_INFO("EINTR:Waiting for forced termination.\n");
				ret = -EINTR;
			} else if (restartsys) {
				PRINT_TNPU_INFO("ERESTARTSY:Waiting for forced restartsys.\n");
				ret = -ERESTARTSYS;
			} else {
				PRINT_TNPU_INFO("SIGNAL:Waiting for signal.\n");
			}
			break;
		}
		if (comp(seqno, wait_seqno))
			break;

		if (timeout_us != ~0ull) {
			if (time_after_eq(jiffies, timeout_expire)) {
				ret = -ETIME;
				break;
			}
			schedule_timeout(timeout_jiffies);
		} else {
			schedule();
		}
	}

	finish_wait(wait_queue, &wait);

	return ret;
}


int ioctl_tnpu_job_wait(unsigned long arg)
{
	int ret;
	tnpu_ioctl_wait_t user_wait;
	int idx;
	uint64_t wait_seqno;

	ret = copy_from_user(&user_wait,
			(tnpu_ioctl_wait_t __user *)arg, sizeof(user_wait));
	if (ret) {
		PRINT_TNPU_ERROR("Failed to copy op from user\n");
		return -EFAULT;
	}

	idx = user_wait.id;
	if (idx < 0 || idx >= TNPU_MAX_TASK)
		return -EINVAL;
	wait_seqno = user_wait.seqno;

	ret = tnpu_wait(user_wait.timeout_us, true, false,
			wait_seqno,
			&tnpu.task_queue[idx].submit_seqno,
			&tnpu.task_queue[idx].submit_wait,
			tnpu_wait_submit_seqno);

	return ret;
}

static int tnpu_read_from_file(struct file *elf, unsigned int uvaddr,
		loff_t off, void *dst_vaddr, size_t size)
{
	int ret;

#if LINUX_VERSION_CODE > KERNEL_VERSION(4, 13, 0)
	ret = kernel_read(elf, dst_vaddr, size, &off);
#else
	ret = kernel_read(elf, off, dst_vaddr, size);
#endif
	if (ret < 0)
		return ret;
	if (ret != size)
		return -EIO;
	return 0;
}

static int tnpu_read_from_mem(struct file *elf, unsigned int uvaddr,
		loff_t off, void *dst_vaddr, size_t size)
{
	if (copy_from_user(dst_vaddr, (void *)((char *)uvaddr + off), size))
		return -EFAULT;
	return 0;
}

static void tnpu_free_lib(unsigned int paddr)
{
	struct tnpu_sharelib *lib, *tmp_lib;

	list_for_each_entry_safe_reverse(lib, tmp_lib, &tnpu.sharelib_list, node) {
		if (lib->paddr == paddr) {
			lib->ref_cnt--;
			if (lib->ref_cnt == 0) {
				int i;

				list_del(&lib->node);
				for (i = 0; i < lib->need_lib_num; i++) {
					tnpu_free_lib(lib->need_lib[i]);
				}
				PRINT_TNPU_DEBUG("%s has been uninstalled.\n", lib->name);
				tnpu_cma_free(lib->size, lib->kva, lib->paddr);
				kfree(lib);
				break;
			}
		}
	}
}

static int tnpu_is_jump_var(const Elf_Sym *sym, const char *sym_name)
{
	if (ELF32_ST_TYPE(sym->st_info) == STT_OBJECT &&
			ELF32_ST_BIND(sym->st_info) == STB_GLOBAL) {
		PRINT_TNPU_ERROR("[SHLIB] unsupported symbol reference: symbol='%s'\n",
				sym_name);
		PRINT_TNPU_ERROR("[SHLIB] Global object jumps are not allowed.\n");
		return -1;
	}

	if (ELF32_ST_TYPE(sym->st_info) != STT_FUNC && sym->st_value != 0) {
		PRINT_TNPU_ERROR("[SHLIB] unsupported symbol type: symbol='%s', type=%d, bind=%d\n",
				sym_name, ELF32_ST_TYPE(sym->st_info), ELF32_ST_BIND(sym->st_info));
		PRINT_TNPU_ERROR("[SHLIB] Export function symbols only.\n");
		return -2;
	}

	return 0;
}

static int tnpu_find_func_addr(const char *rela_name, unsigned int *func_addr)
{
	struct tnpu_sharelib *need_lib;
	unsigned int need_off;
	char *need_name;
	int j;

	if (!func_addr)
		return -EINVAL;

	*func_addr = 0;

	/* Search libraries in reverse order (newest first) */
	list_for_each_entry_reverse(need_lib, &tnpu.sharelib_list, node) {
		for (j = 1; j < need_lib->sym_num; j++) {
			need_off = need_lib->symtab[j].st_value;
			if (!need_off)
				continue;

			need_name = need_lib->strtab + need_lib->symtab[j].st_name;
			if (!strcmp(need_name, rela_name)) {
				if (tnpu_is_jump_var(&need_lib->symtab[j], need_name)) {
					PRINT_TNPU_ERROR("[SHLIB] dependency symbol is not callable: lib='%s', symbol='%s'\n",
							need_lib->name, need_name);
					PRINT_TNPU_ERROR("[SHLIB] Check TNPU shared-library exports.\n");
					return -EINVAL;
				}
				PRINT_TNPU_DEBUG("  Linked func: %s(from %s),addr:0x%x,offset:0x%x\n",
						need_name, need_lib->name, need_lib->paddr, need_off);
				*func_addr = need_lib->paddr + need_off;
				return 0;
			}
		}
	}

	return -ENOENT;
}

static int tnpu_process_relocations(struct tnpu_sharelib *lib, Elf32_Rela *rela, int entry_id)
{
	unsigned int *rela_pos;
	int rela_type, rela_idx, link_off, ret;
	char *rela_name;

	rela_pos = (unsigned int *)((char *)lib->kva + rela[entry_id].r_offset);
	rela_type = ELF_R_TYPE(rela[entry_id].r_info);
	rela_idx = ELF_R_SYM(rela[entry_id].r_info);
	rela_name = lib->strtab + lib->symtab[rela_idx].st_name;

	PRINT_TNPU_DEBUG("  Link pos=%p,type=%d,idx=%d,name=%s\n",
			rela_pos, rela_type, rela_idx, rela_name);

	switch (rela_type) {
	case TNPU_R_RISCV_JUMP_SLOT:
	case TNPU_R_RISCV_32:
			link_off = lib->symtab[rela_idx].st_value;
			if (link_off) {
				if (tnpu_is_jump_var(&lib->symtab[rela_idx], rela_name)) {
					PRINT_TNPU_ERROR("[SHLIB] local symbol is not callable: lib='%s', symbol='%s'\n",
							lib->name, rela_name);
					PRINT_TNPU_ERROR("[SHLIB] Check TNPU shared-library code.\n");
					return -1;
				}
				*rela_pos = lib->paddr + link_off;
			} else {
				ret = tnpu_find_func_addr(rela_name, rela_pos);
				if (ret == -ENOENT) {
					PRINT_TNPU_ERROR("[SHLIB] unresolved symbol: lib='%s', symbol='%s'\n",
							lib->name, rela_name);
					PRINT_TNPU_ERROR("[SHLIB] Check dependency libraries and exported symbols.\n");
				}
				if (ret) {
					return -1;
				}
			}
		break;
	case TNPU_R_RISCV_RELATIVE:
			*rela_pos = lib->paddr + rela[entry_id].r_addend;
			break;
		default:
			PRINT_TNPU_ERROR("[SHLIB] unsupported relocation: lib='%s', type=%d\n",
					lib->name, rela_type);
			PRINT_TNPU_ERROR("[SHLIB] Check toolchain and linker settings.\n");
			return -1;
		}

	return 0;
}

static unsigned int tnpu_get_lib(const char *name, unsigned int vaddr)
{
	struct tnpu_sharelib *lib;
	struct file *elf;
	int ret, i;
	size_t name_len;
	unsigned int load_end = 0;
	unsigned int dyn_off = 0;
	unsigned int hash_off = 0;
	unsigned int dynsym_off = 0;
	unsigned int sym_num = 0;
	struct elfhdr ehdr;
	struct elf_phdr phdr;
	struct elf_shdr shdr;
	Elf32_Dyn *dyn;

	name_len = strnlen(name, TNPU_NAME_MAX - 1);

	/* Check if library already loaded */
	list_for_each_entry(lib, &tnpu.sharelib_list, node) {
		if (!strcmp(name, lib->name)) {
			PRINT_TNPU_DEBUG("%s lib already loaded.\n", name);
			lib->ref_cnt++;
			return lib->paddr;
		}
	}
	lib = NULL;

	/* Setup read method */
	if (vaddr == 0) {
		elf = filp_open(name, O_RDONLY, 0);
		if (IS_ERR(elf)) {
			PRINT_TNPU_ERROR("[SHLIB] open failed: lib='%s'\n", name);
			PRINT_TNPU_ERROR("[SHLIB] Check path, file existence, and permissions.\n");
			return 0;
		}
		tnpu.libread = tnpu_read_from_file;
	} else {
		elf = NULL;
		tnpu.libread = tnpu_read_from_mem;
	}

	/* Read ELF header */
	ret = tnpu.libread(elf, vaddr, 0, (char *)&ehdr, sizeof(struct elfhdr));
	if (ret < 0) {
		PRINT_TNPU_ERROR("[SHLIB] failed to read ELF header: lib='%s'\n", name);
		PRINT_TNPU_ERROR("[SHLIB] Check file completeness and TNPU shared-library format.\n");
		goto get_lib_error;
	}
	if (memcmp(ehdr.e_ident, ELFMAG, SELFMAG) != 0 ||
			ehdr.e_ident[EI_CLASS] != ELFCLASS32 ||
			ehdr.e_ident[EI_DATA] != ELFDATA2LSB ||
			ehdr.e_type != ET_DYN) {
		PRINT_TNPU_ERROR("[SHLIB] invalid ELF header: lib='%s'\n", name);
		PRINT_TNPU_ERROR("[SHLIB] Check TNPU shared-library format.\n");
		goto get_lib_error;
	}

	/* Parse program headers */
	for (i = 0; i < ehdr.e_phnum; i++) {
		ret = tnpu.libread(elf, vaddr,
				ehdr.e_phoff + (i * sizeof(struct elf_phdr)),
				(char *)(&phdr), sizeof(struct elf_phdr));
		if (ret < 0) {
			PRINT_TNPU_ERROR("[SHLIB] failed to read program headers: lib='%s'\n", name);
			PRINT_TNPU_ERROR("[SHLIB] Check file completeness and ELF format.\n");
			goto get_lib_error;
		}
		if (phdr.p_type == PT_LOAD) {
			unsigned int end = phdr.p_offset + phdr.p_filesz;

			if (load_end < end)
				load_end = end;
		} else if (phdr.p_type == PT_DYNAMIC) {
			dyn_off = phdr.p_offset;
		}
	}
	if (!dyn_off || dyn_off >= load_end) {
		PRINT_TNPU_ERROR("[SHLIB] missing PT_DYNAMIC: lib='%s'\n", name);
		PRINT_TNPU_ERROR("[SHLIB] Check linker output and library type.\n");
		goto get_lib_error;
	}

	/* Allocate library structure */
	lib = kzalloc(sizeof(*lib), GFP_KERNEL);
	if (!lib) {
		PRINT_TNPU_ERROR("[SHLIB] metadata allocation failed: lib='%s'\n", name);
		PRINT_TNPU_ERROR("[SHLIB] Check kernel memory usage.\n");
		goto get_lib_error;
	}
	memcpy(lib->name, name, name_len);
	lib->name[name_len] = '\0';
	lib->ref_cnt = 1;
	lib->size = round_up(load_end, PAGE_SIZE);
	lib->kva = tnpu_cma_alloc(lib->size, &lib->paddr);
	if (!lib->kva) {
		PRINT_TNPU_ERROR("[SHLIB] out of contiguous memory: lib='%s', size=0x%x\n",
				name, lib->size);
		PRINT_TNPU_ERROR("[SHLIB] Check CMA size, pool usage, and loaded shared libraries.\n");
		goto get_lib_error;
	}
	lib->dyn = (Elf32_Dyn *)((char *)lib->kva + dyn_off);

	/* Load segments */
	ret = tnpu.libread(elf, vaddr, 0, (char *)lib->kva, load_end);
	if (ret < 0) {
		PRINT_TNPU_ERROR("[SHLIB] failed to load segments: lib='%s'\n", name);
		PRINT_TNPU_ERROR("[SHLIB] Check file completeness and source buffer content.\n");
		goto get_lib_error;
	}
	//printk(KERN_NOTICE "@@@@ lib: name=%s, kv=0x%x, pa=0x%x, dyn=0x%x\n",
	//		name,(unsigned int)lib->kva, (unsigned int)lib->paddr, (unsigned int)lib->dyn);

	/* Process dynamic entries */
	if (lib->dyn->d_tag == DT_HASH)
		hash_off = lib->dyn->d_un.d_val;

	for (dyn = lib->dyn + 1; dyn->d_tag != DT_NULL; dyn++) {
		switch (dyn->d_tag) {
		case DT_STRTAB:
			lib->strtab = (char *)lib->kva + dyn->d_un.d_val;
			break;
		case DT_SYMTAB:
			dynsym_off = dyn->d_un.d_val;
			lib->symtab = (Elf_Sym *)((char *)lib->kva + dyn->d_un.d_val);
			break;
		case DT_RELA:
			dyn_off = dyn->d_un.d_val;
			break;
		}
	}
	if (!lib->strtab || !lib->symtab) {
		PRINT_TNPU_ERROR("[SHLIB] missing STRTAB or SYMTAB: lib='%s'\n", name);
		PRINT_TNPU_ERROR("[SHLIB] Check linker output.\n");
		goto get_lib_error;
	}

	if (hash_off && hash_off <= load_end - 2 * sizeof(unsigned int))
		sym_num = ((unsigned int *)((char *)lib->kva + hash_off))[1];

	if (!sym_num && dynsym_off && ehdr.e_shoff && ehdr.e_shnum) {
		for (i = 0; i < ehdr.e_shnum; i++) {
			ret = tnpu.libread(elf, vaddr,
					ehdr.e_shoff + (i * sizeof(struct elf_shdr)),
					(char *)&shdr, sizeof(struct elf_shdr));
			if (ret < 0) {
				PRINT_TNPU_ERROR("[SHLIB] failed to read section headers: lib='%s'\n", name);
				PRINT_TNPU_ERROR("[SHLIB] Check file completeness and ELF format.\n");
				goto get_lib_error;
			}
			if (shdr.sh_type == SHT_DYNSYM && shdr.sh_addr == dynsym_off) {
				if (!shdr.sh_entsize || (shdr.sh_size % shdr.sh_entsize)) {
					PRINT_TNPU_ERROR("[SHLIB] invalid DYNSYM section: lib='%s'\n", name);
					PRINT_TNPU_ERROR("[SHLIB] Check linker output.\n");
					goto get_lib_error;
				}
				sym_num = shdr.sh_size / shdr.sh_entsize;
				break;
			}
		}
	}

	if (!sym_num) {
		PRINT_TNPU_ERROR("[SHLIB] failed to get symbol count: lib='%s'\n", name);
		PRINT_TNPU_ERROR("[SHLIB] Check DT_HASH and DYNSYM section headers.\n");
		goto get_lib_error;
	}

	lib->sym_num = sym_num;

	if (vaddr == 0) {
		filp_close(elf, NULL);
		elf = NULL;
	}

	/* Handle dependencies and relocations */
	lib->need_lib_num = 0;
	if (dyn_off) {
		unsigned int relasz = 0;
		unsigned int relaend = 0;
		Elf32_Rela *rela = (Elf_Rela *)((char *)lib->kva + dyn_off);

		for (dyn = lib->dyn; dyn->d_tag != DT_NULL; dyn++) {
			switch (dyn->d_tag) {
				case DT_NEEDED:
					if (lib->need_lib_num >= ARRAY_SIZE(lib->need_lib)) {
						PRINT_TNPU_ERROR("[SHLIB] dependency limit exceeded: lib='%s', max=%u\n",
								lib->name, (unsigned int)ARRAY_SIZE(lib->need_lib));
						PRINT_TNPU_ERROR("[SHLIB] Reduce DT_NEEDED count or update the driver limit.\n");
						goto get_lib_error;
					}
					PRINT_TNPU_DEBUG("%s depends on %s\n",
							lib->name, lib->strtab + dyn->d_un.d_val);
					lib->need_lib[lib->need_lib_num] =
						tnpu_get_lib(lib->strtab + dyn->d_un.d_val, 0);
				if (!lib->need_lib[lib->need_lib_num]) {
					PRINT_TNPU_ERROR("[SHLIB] dependency load failed: parent='%s', need='%s'\n",
							lib->name, lib->strtab + dyn->d_un.d_val);
					PRINT_TNPU_ERROR("[SHLIB] Check dependent TNPU libraries in the current directory.\n");
					goto get_lib_error;
				}
				lib->need_lib_num++;
				break;
			case DT_RELASZ:
				relasz = dyn->d_un.d_val;
				break;
			case DT_RELAENT:
				relaend = dyn->d_un.d_val;
				break;
			}
		}

		if (relaend) {
			unsigned int rela_num = relasz / relaend;
			for (i = 0; i < rela_num; i++) {
				ret = tnpu_process_relocations(lib, rela, i);
				if (ret) {
					goto get_lib_error;
				}
			}
		}
	}

	/* Add to library list */
	list_add_tail(&lib->node, &tnpu.sharelib_list);

	PRINT_TNPU_DEBUG("Loaded %s, pa=0x%x, size=%d\n",
			lib->name, lib->paddr, lib->size);
	return lib->paddr;

get_lib_error:
	if (lib) {
		for (i = 0; i < lib->need_lib_num; i++) {
			if (lib->need_lib[i]) {
				tnpu_free_lib(lib->need_lib[i]);
			}
		}
		if (lib->kva) {
			tnpu_cma_free(lib->size, lib->kva, lib->paddr);
		}
		kfree(lib);
	}
	if (elf) {
		filp_close(elf, NULL);
	}
	return 0;
}

int ioctl_tnpu_sharelib_load(unsigned long arg)
{
	tnpu_ioctl_sharelib_t load_lib;
	struct tnpu_libref *libref;

	if (copy_from_user(&load_lib, (void __user *)arg, sizeof(load_lib))) {
		PRINT_TNPU_ERROR("[SHLIB] load request copy failed.\n");
		PRINT_TNPU_ERROR("[SHLIB] Check caller buffer and access permissions.\n");
		return -EFAULT;
	}
	load_lib.name[TNPU_NAME_MAX - 1] = '\0';

	libref = kmalloc(sizeof(*libref), GFP_KERNEL);
	if (!libref) {
		PRINT_TNPU_ERROR("[SHLIB] load request allocation failed: lib='%s'\n",
				load_lib.name);
		PRINT_TNPU_ERROR("[SHLIB] Check kernel memory usage.\n");
		return -ENOMEM;
	}

	mutex_lock(&tnpu.load_mutex);

	libref->paddr = tnpu_get_lib(load_lib.name, load_lib.uvaddr);
	if (libref->paddr == 0) {
		PRINT_TNPU_ERROR("[SHLIB] load failed: lib='%s'\n", load_lib.name);
		PRINT_TNPU_ERROR("[SHLIB] Check previous [SHLIB] errors for details.\n");
		mutex_unlock(&tnpu.load_mutex);
		kfree(libref);
		return -EFAULT;
	}
	libref->pid = task_tgid_nr(current);

	list_add_tail(&libref->node, &tnpu.libref_list);

	load_lib.paddr = libref->paddr;

	if (copy_to_user((void __user *)arg, &load_lib, sizeof(load_lib))) {
		PRINT_TNPU_ERROR("[SHLIB] load result copy failed: lib='%s'\n",
				load_lib.name);
		PRINT_TNPU_ERROR("[SHLIB] Check user buffer and caller memory.\n");
		tnpu_free_lib(libref->paddr);
		list_del(&libref->node);
		mutex_unlock(&tnpu.load_mutex);
		kfree(libref);
		return -EFAULT;
	}
	mutex_unlock(&tnpu.load_mutex);
	return 0;
}

int ioctl_tnpu_sharelib_unload(unsigned long arg)
{
	unsigned int lib_paddr;
	unsigned int pid = task_tgid_nr(current);
	struct tnpu_libref *libref;

	if (copy_from_user(&lib_paddr, (void __user *)arg, sizeof(unsigned int))) {
		PRINT_TNPU_ERROR("Failed to copy from user\n");
		return -EFAULT;
	}

	mutex_lock(&tnpu.load_mutex);
	list_for_each_entry_reverse(libref, &tnpu.libref_list, node) {
		if (libref->paddr == lib_paddr && libref->pid == pid) {
			tnpu_free_lib(lib_paddr);
			list_del(&libref->node);
			kfree(libref);
			break;
		}
	}
	mutex_unlock(&tnpu.load_mutex);

	return 0;
}

int tnpu_sharelib_busy(void)
{
	int busy;

	mutex_lock(&tnpu.load_mutex);
	busy = !list_empty(&tnpu.sharelib_list);
	mutex_unlock(&tnpu.load_mutex);

	return busy;
}

void tnpu_show_sharelib(void)
{
	struct tnpu_sharelib *lib;
	unsigned int total_size = 0;
	unsigned int lib_count = 0;

	PRINT_TNPU_INFO("TNPU Shared Libraries:\n");

	mutex_lock(&tnpu.load_mutex);
	if (list_empty(&tnpu.sharelib_list)) {
		PRINT_TNPU_INFO("  (No shared libraries)\n");
	} else {
		PRINT_TNPU_INFO("%-12s %-12s %-10s %s\n", "KVA", "PAddr", "Size", "Name");
		list_for_each_entry(lib, &tnpu.sharelib_list, node) {
			PRINT_TNPU_INFO("%-12p 0x%-10x %-10x %s\n",
					lib->kva, lib->paddr, lib->size, lib->name);
			total_size += lib->size;
			lib_count++;
		}
		PRINT_TNPU_INFO("Total: %u libraries, %u bytes\n", lib_count, total_size);
	}
	mutex_unlock(&tnpu.load_mutex);
}


static int tnpu_validate_submit(tnpu_ioctl_submit_t *submit, int job_idx)
{
	if (submit->nice < 0 || submit->oram < 0 ||
			submit->oram > TNPU_ORAM_SIZE) {
		if (job_idx >= 0) {
			PRINT_TNPU_ERROR("Invalid nice/oram in job[%d]\n", job_idx);
		} else {
			PRINT_TNPU_ERROR("Invalid nice/oram\n");
		}
		return -EINVAL;
	}

	if (submit->device[0] & submit->device[1]) {
		if (job_idx >= 0) {
			PRINT_TNPU_ERROR("Device conflict in job[%d]\n", job_idx);
		} else {
			PRINT_TNPU_ERROR("Device conflict\n");
		}
		return -EFAULT;
	}

	if (submit->pc[0] == 0 && submit->pc[1] == 0) {
		if (job_idx >= 0) {
			PRINT_TNPU_ERROR("Invalid task pc in job[%d]\n", job_idx);
		} else {
			PRINT_TNPU_ERROR("Invalid task pc\n");
		}
		return -EINVAL;
	}

	return 0;
}

static void tnpu_set_queue(
		struct tnpu_task_queue *queue,
		tnpu_ioctl_submit_t *submit,
		int idx,
		pid_t curr_pid)
{
	struct tnpu_task *task = queue->task;

	if (submit->nice > TNPU_TASK_NICE_HIGH)
		submit->nice = TNPU_TASK_NICE_HIGH;

	queue->nice = submit->nice;
	queue->pid = curr_pid;
	queue->id = tnpu.submit_queue_seqno.can;
	queue->oram_size = submit->oram;

	if (submit->pc[0]) {
		task[0].status = TNPU_TASK_INIT;
		task[0].device = submit->device[0] | TNPU_TASK_DEVICE_V0;
		task[0].pc     = submit->pc[0];
		task[0].arg[0] = submit->arg[0][0];
		task[0].arg[1] = submit->arg[0][1];
		task[0].arg[2] = submit->arg[0][2];
		task[0].arg[3] = submit->arg[0][3];
	}

	if (submit->pc[1]) {
		task[1].status = TNPU_TASK_INIT;
		task[1].device = submit->device[1] | TNPU_TASK_DEVICE_V1;
		task[1].pc     = submit->pc[1];
		task[1].arg[0] = submit->arg[1][0];
		task[1].arg[1] = submit->arg[1][1];
		task[1].arg[2] = submit->arg[1][2];
		task[1].arg[3] = submit->arg[1][3];
	}

	queue->flush_cache = !(submit->ctl & TNPU_JOB_KEEP_CACHE);

	queue->submit_seqno.can++;
	tnpu.submit_queue_seqno.can++;


	submit->wait.id = idx;
	submit->wait.seqno = queue->submit_seqno.can;
	submit->wait.timeout_us = -1;
}

static void tnpu_reset_queue(struct tnpu_task_queue *queue)
{
	memset(queue->task, 0, sizeof(queue->task));
	queue->nice = 0;
	queue->id = 0;
	queue->pid = 0;
	queue->oram_addr = 0;
	queue->oram_size = 0;
	queue->prev = NULL;
	queue->next = NULL;
	queue->flush_cache = 0;
	queue->submit_seqno.can--;
	tnpu.submit_queue_seqno.can--;
}

int ioctl_tnpu_job_submit(unsigned long arg)
{
	int ret, idx;
	tnpu_ioctl_submit_t submit;
	tnpu_ioctl_submit_t __user *user_submit;
	pid_t curr_pid = task_tgid_nr(current);
	unsigned long task_irqflags;
	struct tnpu_task_queue *queue;

	user_submit = (tnpu_ioctl_submit_t __user *)arg;
	ret = copy_from_user(&submit, user_submit, sizeof(submit));
	if (ret) {
		PRINT_TNPU_ERROR("Copy from user failed\n");
		return -EFAULT;
	}

	ret = tnpu_validate_submit(&submit, -1);
	if (ret) {
		return ret;
	}

retry:
	spin_lock_irqsave(&tnpu.task_spin, task_irqflags);
	for (idx = 0; idx < TNPU_MAX_TASK; idx++) {
		queue = &tnpu.task_queue[idx];
		if (queue->task[0].status == TNPU_TASK_NONE &&
			queue->task[1].status == TNPU_TASK_NONE) {
			tnpu_set_queue(queue, &submit, idx, curr_pid);
			ret = 1;
			break;
		}
	}

	if (ret != 1) {
		spin_unlock_irqrestore(&tnpu.task_spin, task_irqflags);
		ret = tnpu_wait(-1, true, false,
				1ll, // need_queue_num
				&tnpu.submit_queue_seqno,
				&tnpu.submit_queue_wait,
				tnpu_wait_submit_queue);
		if (ret) {
			return ret;
		}
		goto retry;
	}

	tnpu_job_send();

	spin_unlock_irqrestore(&tnpu.task_spin, task_irqflags);


	ret = copy_to_user(&user_submit->wait, &submit.wait, sizeof(tnpu_ioctl_wait_t));
	if (ret) {
		PRINT_TNPU_ERROR("Copy to user failed\n");
		return -EFAULT;
	}

	if (!(submit.ctl & TNPU_JOB_BACKGROUND)) {
		ret = tnpu_wait(-1, true, false, 0ll,
				&tnpu.task_queue[idx].submit_seqno,
				&tnpu.task_queue[idx].submit_wait,
				tnpu_wait_submit_seqno);
	}

	return ret;
}

int ioctl_tnpu_job_submit_chain(unsigned long arg)
{
	int ret, i, idx;
	tnpu_ioctl_submit_chain_t chain;
	unsigned int jobs_size;
	tnpu_ioctl_submit_t *submit;
	unsigned long task_irqflags;
	struct tnpu_task_queue *queue;
	struct tnpu_task_queue *prev_queue = NULL;
	struct tnpu_task_queue *queued[TNPU_MAX_TASK];
	pid_t curr_pid = task_tgid_nr(current);
	unsigned int wait_background;
	unsigned int queued_num = 0;

	ret = copy_from_user(&chain,
		(tnpu_ioctl_submit_chain_t __user *)arg,
		sizeof(tnpu_ioctl_submit_chain_t));
	if (ret) {
		PRINT_TNPU_ERROR("Copy chain from user failed, ret=%d\n", ret);
		return -EFAULT;
	}

	if (chain.job_num > 15 || chain.job_num <= 0) {
		PRINT_TNPU_ERROR("Invalid job num, %d\n", chain.job_num);
		return -EINVAL;
	}
	jobs_size = chain.job_num * sizeof(tnpu_ioctl_submit_t);
	submit = kmalloc_array(chain.job_num, sizeof(tnpu_ioctl_submit_t), GFP_KERNEL);
	if (!submit) {
		PRINT_TNPU_ERROR("Alloc jobs submit failed, ret=%d\n", ret);
		return -ENOMEM;
	}

	ret = copy_from_user(submit, chain.job, chain.job_num * sizeof(tnpu_ioctl_submit_t));
	if (ret) {
		kfree(submit);
		PRINT_TNPU_ERROR("Copy jobs from user failed, ret=%d\n", ret);
		return -EFAULT;
	}

	for (i = 0; i < chain.job_num; i++) {
		ret = tnpu_validate_submit(&submit[i], i);
		if (ret) {
			kfree(submit);
			return ret;
		}
	}
	wait_background = submit[chain.job_num - 1].ctl & TNPU_JOB_BACKGROUND;

	spin_lock_irqsave(&tnpu.task_spin, task_irqflags);
	ret = tnpu_wait_submit_queue(&tnpu.submit_queue_seqno, chain.job_num);
	while (!ret) {
		spin_unlock_irqrestore(&tnpu.task_spin, task_irqflags);
		PRINT_TNPU_DEBUG("Lack of available queue, num=%d\n", chain.job_num);
		ret = tnpu_wait(-1, true, false,
				chain.job_num, // need_queue_num
				&tnpu.submit_queue_seqno,
				&tnpu.submit_queue_wait,
				tnpu_wait_submit_queue
		);
		if (ret) {
			kfree(submit);
			return ret;
		}
		spin_lock_irqsave(&tnpu.task_spin, task_irqflags);
		ret = tnpu_wait_submit_queue(&tnpu.submit_queue_seqno, chain.job_num);
	}

	for (i = 0; i < chain.job_num; i++) {
		ret = 0;
		for (idx = 0; idx < TNPU_MAX_TASK; idx++) {
			queue = &tnpu.task_queue[idx];
			if (queue->task[0].status == TNPU_TASK_NONE &&
				queue->task[1].status == TNPU_TASK_NONE) {
				tnpu_set_queue(queue, &submit[i], idx, curr_pid);
				queued[queued_num++] = queue;
				if (prev_queue) {
					prev_queue->next = queue;
					queue->prev = prev_queue;
				}
				prev_queue = queue;
				ret = 1;
				break;
			}
		}

		if (ret != 1) {
			while (queued_num > 0) {
				tnpu_reset_queue(queued[--queued_num]);
			}
			spin_unlock_irqrestore(&tnpu.task_spin, task_irqflags);
			kfree(submit);
			PRINT_TNPU_ERROR("No free queue in job[%d]\n", i);
			return -EFAULT;
		}
	}

	tnpu_job_send();

	spin_unlock_irqrestore(&tnpu.task_spin, task_irqflags);


	ret = copy_to_user(chain.job, submit, jobs_size);
	if (ret) {
		kfree(submit);
		PRINT_TNPU_ERROR("Failed to copy job[%d] to user (ret=%d)\n", i, ret);
		return -EFAULT;
	}
	kfree(submit);

	if (!wait_background) {
		ret = tnpu_wait(-1, true, false, 0ll,
				&queue->submit_seqno,
				&queue->submit_wait,
				tnpu_wait_submit_seqno);
	}

	return ret;
}

static int tnpu_scan_work_status(pid_t pid)
{
	int i;
	int run_v0 = 0;
	int run_v1 = 0;
	struct tnpu_task_queue *queue;
	static const int status_table[3][3] = {
		// v1=0 v1=1 v1=2
		{  0,   -1,  0 }, // v0=0
		{  -1,  -1,  1 }, // v0=1
		{  0,   1,   0 }  // v0=2
	};

	for (i = 0; i < TNPU_MAX_TASK; i++) {
		queue = &tnpu.task_queue[i];
		if (queue->pid) {
			if (queue->pid == pid) {
				if (queue->task[0].status == TNPU_TASK_RUN) {
					run_v0 = 1;
				}
				if (queue->task[1].status == TNPU_TASK_RUN) {
					run_v1 = 1;
				}
			} else {
				if (queue->task[0].status == TNPU_TASK_RUN) {
					run_v0 = 2;
				}
				if (queue->task[1].status == TNPU_TASK_RUN) {
					run_v1 = 2;
				}
			}
		}
	}

	// -1:Reset 0:Non 1:Wait
	return status_table[run_v0][run_v1];
}

void tnpu_kill_job(pid_t pid)
{
	unsigned long irqflags;
	int reset;
	int i;
	int cycles = 3000;
	struct tnpu_libref *libref, *tmp_libref;

	// kill job
	tnpu.send_flag = 0; // Prohibit sending tasks
	wmb();              // asm volatile (":::memery");

	for (i = 0; i < cycles; i++) {
		reset = tnpu_scan_work_status(pid);
		if (reset <= 0) {
			// No other PID tasks
			break;
		}

		msleep(2);
		if (cycles == i + 1) {
			// Force reset of other PID tasks
			PRINT_TNPU_ERROR("Timeout waiting, resetting\n");
		}
	}

	spin_lock_irqsave(&tnpu.task_spin, irqflags);

	if (reset) {
		struct tnpu_task_queue *queue;
		PRINT_TNPU_DEBUG("Need kill tnpu task.\n");
		for (i = 0; i < TNPU_MAX_TASK; i++) {
			queue = &tnpu.task_queue[i];
			if (queue->task[0].status == TNPU_TASK_RUN
					|| queue->task[1].status == TNPU_TASK_RUN) {
				tnpu_job_remove(queue, false);
			}
		}
		tnpu_hardware_bus_idle();
		tnpu_hardware_reset();
		tnpu_hardware_sleep();
	}
	for (i = 0; i < TNPU_MAX_TASK; i++) {
		struct tnpu_task_queue *queue = &tnpu.task_queue[i];

		if (queue->pid == pid) {
			tnpu_job_remove(queue, true);
		}
	}

	tnpu.send_flag = 1;
	tnpu_job_send();

	spin_unlock_irqrestore(&tnpu.task_spin, irqflags);


	// kill lib
	mutex_lock(&tnpu.load_mutex);
	list_for_each_entry_safe_reverse(libref, tmp_libref, &tnpu.libref_list, node) {
		if (libref->pid == pid) {
			tnpu_free_lib(libref->paddr);
			list_del(&libref->node);
			kfree(libref);
		}
	}
	mutex_unlock(&tnpu.load_mutex);
}
