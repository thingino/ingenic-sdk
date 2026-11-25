// SPDX-License-Identifier: GPL-2.0
/*
 * tx-isp-twin.c - 4.4-shaped twins of the kernel objects the T40 firmware reads
 *
 * The firmware's file_operations handlers read struct file (private_data at
 * 136, f_flags at 32), the /proc show handlers read struct seq_file (private
 * at 76), and its IQ loader reads the struct file and inode it opened
 * (f_pos 56, f_inode 16, i_size 48). All of those are inlined loads at
 * 4.4.94 offsets. The firmware only ever receives these objects as
 * arguments of its own handlers, so each handler is registered through a
 * wrapper here that builds a 4.4-shaped twin on the stack, fills in the
 * fields the firmware reads (checked on every load through the argument in
 * the disassembly) and calls the firmware's code unmodified. Table entries
 * that take such an object (single_open_size, filp_open and friends) map the
 * twin back to the kernel's object through the pointer stored at offset 0.
 *
 * thread_info needs no twin: the firmware's inlined access_ok() (addr_limit
 * at 24($gp), which 7.x replaced with the syscall number) is satisfied by
 * fw_user_call() around the user-facing handlers.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/poll.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/dma-mapping.h>
#include <linux/mutex.h>
#include <linux/thread_info.h>

#include "tx-isp-abi.h"
#include "tx-isp-twin.h"

/* slots of the 4.4 file_operations (27 members, 108 bytes) */
enum {
	F44_OWNER, F44_LLSEEK, F44_READ, F44_WRITE, F44_READ_ITER,
	F44_WRITE_ITER, F44_ITERATE, F44_POLL, F44_UNLOCKED_IOCTL,
	F44_COMPAT_IOCTL, F44_MMAP, F44_OPEN, F44_FLUSH, F44_RELEASE,
	F44_FSYNC, F44_AIO_FSYNC, F44_FASYNC, F44_LOCK, F44_SENDPAGE,
	F44_GET_UNMAPPED_AREA, F44_CHECK_FLAGS, F44_FLOCK,
	F44_SPLICE_WRITE, F44_SPLICE_READ, F44_SETLEASE, F44_FALLOCATE,
	F44_SHOW_FDINFO, F44_COUNT
};

#define F44(f, i)	(((void * const *)(f))[i])

/* ---- the twins ------------------------------------------------------------ */

struct fw_file {
	u8 b[FILE44_SIZE] __aligned(8);
};

struct fw_seq {
	u8 b[SEQ44_SIZE] __aligned(8);
};

static void *file_twin(struct fw_file *t, struct file *file, void *private)
{
	memset(t, 0, sizeof(*t));
	FW(t->b, FILE44_REAL, struct file *) = file;
	FW(t->b, FILE44_F_FLAGS, unsigned int) = file->f_flags;
	FW(t->b, FILE44_F_MODE, unsigned int) = file->f_mode;
	FW(t->b, FILE44_PRIVATE_DATA, void *) = private;
	return t->b;
}

static struct file *file_real(void *twin)
{
	return FW(twin, FILE44_REAL, struct file *);
}

/*
 * The firmware's inlined access_ok() loads addr_limit from 24($gp), where
 * 4.4.94's thread_info kept it; 7.x keeps the syscall number there. Around
 * every user-facing handler call that word holds 4.4's USER_DS, so the
 * firmware's checks mean what they did, and it is put back afterwards.
 */
#define FW_USER_DS	0x80000000UL

#define fw_user_call(expr)						\
({									\
	long __sv = current_thread_info()->syscall;			\
	typeof(expr) __r;						\
									\
	current_thread_info()->syscall = FW_USER_DS;			\
	__r = (expr);							\
	current_thread_info()->syscall = __sv;				\
	__r;								\
})

/* ---- misc and cdev fops ---------------------------------------------------- */

static struct fw_fops *fw_of(struct file *file)
{
	return container_of(file->f_op, struct fw_fops, fops);
}

static int fw_open(struct inode *inode, struct file *file)
{
	struct fw_fops *t = fw_of(file);
	int (*open)(void *, void *) = t->orig[F44_OPEN];
	struct fw_file tw;
	void *priv, *tp;
	int ret;

	/* misc_open() left the registered twin there; 4.4 left the firmware's own */
	if (t->blob)
		file->private_data = t->blob;
	if (!open)
		return 0;
	priv = file->private_data;
	tp = file_twin(&tw, file, priv);
	ret = open(inode, tp);
	if (!ret && FW(tp, FILE44_PRIVATE_DATA, void *) != priv)
		file->private_data = FW(tp, FILE44_PRIVATE_DATA, void *);
	return ret;
}

static int fw_release(struct inode *inode, struct file *file)
{
	struct fw_fops *t = fw_of(file);
	int (*release)(void *, void *) = t->orig[F44_RELEASE];
	struct fw_file tw;

	if (!release)
		return 0;
	return release(inode, file_twin(&tw, file, file->private_data));
}

static long fw_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long (*ioctl)(void *, unsigned int, unsigned long) =
		fw_of(file)->orig[F44_UNLOCKED_IOCTL];
	struct fw_file tw;

	return fw_user_call(ioctl(file_twin(&tw, file, file->private_data), cmd, arg));
}

static ssize_t fw_read(struct file *file, char __user *buf, size_t count, loff_t *ppos)
{
	ssize_t (*read)(void *, char __user *, size_t, loff_t *) =
		fw_of(file)->orig[F44_READ];
	struct fw_file tw;

	return fw_user_call(read(file_twin(&tw, file, file->private_data), buf, count, ppos));
}

static ssize_t fw_write(struct file *file, const char __user *buf, size_t count,
			loff_t *ppos)
{
	ssize_t (*write)(void *, const char __user *, size_t, loff_t *) =
		fw_of(file)->orig[F44_WRITE];
	struct fw_file tw;

	return fw_user_call(write(file_twin(&tw, file, file->private_data), buf, count, ppos));
}

/* poll_wait() hands the file to the kernel, so the poll handler gets the real one */
static __poll_t fw_poll(struct file *file, struct poll_table_struct *wait)
{
	__poll_t (*poll)(void *, void *) = fw_of(file)->orig[F44_POLL];

	return poll(file, wait);
}

void fw_fops_init(struct fw_fops *t, const void *f44, void *blob)
{
	int i;

	memset(t, 0, sizeof(*t));
	t->blob = blob;
	for (i = 0; i < F44_COUNT; i++)
		t->orig[i] = F44(f44, i);
	t->fops.owner = (struct module *)t->orig[F44_OWNER];
	t->fops.open = fw_open;
	t->fops.release = fw_release;
	if (t->orig[F44_UNLOCKED_IOCTL])
		t->fops.unlocked_ioctl = fw_ioctl;
	if (t->orig[F44_READ])
		t->fops.read = fw_read;
	if (t->orig[F44_WRITE])
		t->fops.write = fw_write;
	if (t->orig[F44_POLL])
		t->fops.poll = fw_poll;
	for (i = 0; i < F44_COUNT; i++) {
		if (!t->orig[i])
			continue;
		switch (i) {
		case F44_OWNER: case F44_OPEN: case F44_RELEASE:
		case F44_UNLOCKED_IOCTL: case F44_READ: case F44_WRITE:
		case F44_POLL:
			break;
		default:
			pr_warn("t40-isp: fops member %d not carried over\n", i);
		}
	}
}

/* ---- /proc ------------------------------------------------------------------ */

struct fw_seq_ctx {
	int (*show)(void *seq, void *v);
	void *data;
};

struct fw_proc {
	struct proc_ops ops;
	const void *orig[F44_COUNT];
	void *data;
	struct proc_dir_entry *de;
};

static struct fw_proc *fw_proc_of(struct file *file)
{
	return pde_data(file_inode(file));
}

static void *seq_twin(struct fw_seq *t, struct seq_file *seq)
{
	struct fw_seq_ctx *ctx = seq->private;

	memset(t, 0, sizeof(*t));
	FW(t->b, SEQ44_REAL, struct seq_file *) = seq;
	FW(t->b, SEQ44_PRIVATE, void *) = ctx->data;
	return t->b;
}

struct seq_file *t40_seq_real(void *twin)
{
	return FW(twin, SEQ44_REAL, struct seq_file *);
}

static int fw_single_show(struct seq_file *seq, void *v)
{
	struct fw_seq_ctx *ctx = seq->private;
	struct fw_seq st;

	return ctx->show(seq_twin(&st, seq), v);
}

int t40_single_open_size(struct file *twin, int (*show)(struct seq_file *, void *),
			 void *data, size_t size)
{
	struct fw_seq_ctx *ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	int ret;

	if (!ctx)
		return -ENOMEM;
	ctx->show = (void *)show;
	ctx->data = data;
	ret = single_open_size(file_real(twin), fw_single_show, ctx, size);
	if (ret)
		kfree(ctx);
	return ret;
}

static int fw_single_release_real(struct inode *inode, struct file *file)
{
	struct seq_file *seq = file->private_data;
	struct fw_seq_ctx *ctx = seq ? seq->private : NULL;
	int ret = single_release(inode, file);

	kfree(ctx);
	return ret;
}

int t40_single_release(struct inode *inode, struct file *twin)
{
	return fw_single_release_real(inode, file_real(twin));
}

ssize_t t40_seq_read(struct file *twin, char __user *buf, size_t size, loff_t *ppos)
{
	return seq_read(file_real(twin), buf, size, ppos);
}

loff_t t40_seq_lseek(struct file *twin, loff_t offset, int whence)
{
	return seq_lseek(file_real(twin), offset, whence);
}

/* the file twin of a single_open() proc file carries the seq twin as private_data */
static void *proc_file_twin(struct fw_file *tf, struct fw_seq *ts, struct file *file)
{
	struct seq_file *seq = file->private_data;

	return file_twin(tf, file, seq ? seq_twin(ts, seq) : NULL);
}

static int fw_proc_open(struct inode *inode, struct file *file)
{
	struct fw_proc *p = pde_data(inode);
	int (*open)(void *, void *) = p->orig[F44_OPEN];
	struct fw_file tf;

	return open(inode, file_twin(&tf, file, NULL));
}

static ssize_t fw_proc_write(struct file *file, const char __user *buf, size_t count,
			     loff_t *ppos)
{
	ssize_t (*write)(void *, const char __user *, size_t, loff_t *) =
		fw_proc_of(file)->orig[F44_WRITE];
	struct fw_file tf;
	struct fw_seq ts;

	return fw_user_call(write(proc_file_twin(&tf, &ts, file), buf, count, ppos));
}

static long fw_proc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long (*ioctl)(void *, unsigned int, unsigned long) =
		fw_proc_of(file)->orig[F44_UNLOCKED_IOCTL];
	struct fw_file tf;
	struct fw_seq ts;

	return fw_user_call(ioctl(proc_file_twin(&tf, &ts, file), cmd, arg));
}

static int fw_proc_release(struct inode *inode, struct file *file)
{
	int (*release)(void *, void *) = fw_proc_of(file)->orig[F44_RELEASE];
	struct fw_file tf;
	struct fw_seq ts;

	return release(inode, proc_file_twin(&tf, &ts, file));
}

static struct fw_proc *fw_procs[64];
static DEFINE_MUTEX(fw_proc_lock);

/* the firmware's table-driven seq helpers, recognized by address */
extern char private_seq_read[], private_seq_lseek[], private_single_release[];

struct proc_dir_entry *t40_proc_create_data(const char *name, umode_t mode,
					    struct proc_dir_entry *parent,
					    const void *f44, void *data)
{
	struct fw_proc *p = kzalloc(sizeof(*p), GFP_KERNEL);
	int i;

	if (!p)
		return NULL;
	for (i = 0; i < F44_COUNT; i++)
		p->orig[i] = F44(f44, i);
	p->data = data;
	if (p->orig[F44_OPEN])
		p->ops.proc_open = fw_proc_open;
	if (p->orig[F44_READ] == private_seq_read)
		p->ops.proc_read = seq_read;
	if (p->orig[F44_LLSEEK] == private_seq_lseek)
		p->ops.proc_lseek = seq_lseek;
	if (p->orig[F44_RELEASE] == private_single_release)
		p->ops.proc_release = fw_single_release_real;
	else if (p->orig[F44_RELEASE])
		p->ops.proc_release = fw_proc_release;
	if (p->orig[F44_WRITE])
		p->ops.proc_write = fw_proc_write;
	if (p->orig[F44_UNLOCKED_IOCTL])
		p->ops.proc_ioctl = fw_proc_ioctl;
	if (p->orig[F44_MMAP])
		pr_debug("t40-isp: proc %s: mmap not carried over\n", name);
	p->de = proc_create_data(name, mode, parent, &p->ops, p);
	if (!p->de)
		goto err;
	mutex_lock(&fw_proc_lock);
	for (i = 0; i < ARRAY_SIZE(fw_procs); i++) {
		if (!fw_procs[i]) {
			fw_procs[i] = p;
			break;
		}
	}
	mutex_unlock(&fw_proc_lock);
	if (i == ARRAY_SIZE(fw_procs)) {
		proc_remove(p->de);
		goto err;
	}
	return p->de;
err:
	kfree(p);
	return NULL;
}

void *t40_proc_data(const struct inode *inode)
{
	struct fw_proc *p = pde_data(inode);

	return p->data;
}

/* proc_remove() on a directory takes its entries with it: free their ops after */
void t40_proc_remove(struct proc_dir_entry *de)
{
	int i;

	proc_remove(de);
	mutex_lock(&fw_proc_lock);
	for (i = 0; i < ARRAY_SIZE(fw_procs); i++) {
		if (fw_procs[i] && fw_procs[i]->de == de) {
			kfree(fw_procs[i]);
			fw_procs[i] = NULL;
		}
	}
	mutex_unlock(&fw_proc_lock);
}

/* ---- vfs: the IQ file -------------------------------------------------------- */

struct fw_vfile {
	struct fw_file f;
	u8 inode[64] __aligned(8);
	struct file *real;
};


struct file *t40_filp_open(const char *path, int flags, umode_t mode)
{
	struct file *real = filp_open(path, flags, mode);
	struct fw_vfile *h;

	if (IS_ERR(real))
		return real;
	h = kzalloc(sizeof(*h), GFP_KERNEL);
	if (!h) {
		filp_close(real, NULL);
		return ERR_PTR(-ENOMEM);
	}
	h->real = real;
	FW(h->f.b, FILE44_REAL, struct file *) = real;
	FW(h->f.b, FILE44_F_FLAGS, unsigned int) = real->f_flags;
	FW(h->f.b, FILE44_F_INODE, void *) = h->inode;
	FW(h->inode, INODE44_I_SIZE, loff_t) = i_size_read(file_inode(real));
	return (struct file *)h->f.b;
}

int t40_filp_close(struct file *handle, void *id)
{
	struct fw_vfile *h = container_of((void *)handle, struct fw_vfile, f);
	int ret;

	if (IS_ERR_OR_NULL(handle))
		return -EBADF;
	ret = filp_close(h->real, id);
	kfree(h);
	return ret;
}

ssize_t t40_vfs_read(struct file *handle, char __user *buf, size_t count, loff_t *pos)
{
	struct fw_vfile *h = container_of((void *)handle, struct fw_vfile, f);

	return kernel_read(h->real, (void __force *)buf, count, pos);
}

ssize_t t40_vfs_write(struct file *handle, const char __user *buf, size_t count,
		      loff_t *pos)
{
	struct fw_vfile *h = container_of((void *)handle, struct fw_vfile, f);

	return kernel_write(h->real, (const void __force *)buf, count, pos);
}

loff_t t40_vfs_llseek(struct file *handle, loff_t offset, int whence)
{
	struct fw_vfile *h = container_of((void *)handle, struct fw_vfile, f);
	loff_t pos = vfs_llseek(h->real, offset, whence);

	if (pos >= 0)
		FW(h->f.b, FILE44_F_POS, loff_t) = pos;
	return pos;
}

/* ---- IVDC: dma_alloc_coherent() inlined against 4.4's mips_dma_map_ops ------- */

static void *t40_ivdc_alloc(struct device *dev, size_t size, dma_addr_t *handle,
			    gfp_t gfp, unsigned long attrs)
{
	struct device *d = t40_any_dev();

	return d ? dma_alloc_coherent(d, size, handle, GFP_KERNEL) : NULL;
}

static void t40_ivdc_free(struct device *dev, size_t size, void *vaddr,
			  dma_addr_t handle, unsigned long attrs)
{
	struct device *d = t40_any_dev();

	if (d)
		dma_free_coherent(d, size, vaddr, handle);
}

/*
 * 4.4's MIPS keeps a pointer to its struct dma_map_ops in mips_dma_map_ops
 * and the firmware calls through it. The members it uses sit at the 4.4
 * offsets: alloc 0, free 4, sync_single_for_cpu 32, sync_single_for_device 36.
 */
static void t40_ops_sync_cpu(struct device *dev, dma_addr_t addr, size_t size,
			     enum dma_data_direction dir)
{
	t40_dma_sync_single_for_cpu(dev, addr, size, dir);
}

static void t40_ops_sync_device(struct device *dev, dma_addr_t addr, size_t size,
				enum dma_data_direction dir)
{
	t40_dma_sync_single_for_device(dev, addr, size, dir);
}

static void *t40_dma_ops_tbl[10] = {
	[0] = t40_ivdc_alloc,
	[1] = t40_ivdc_free,
	[8] = t40_ops_sync_cpu,
	[9] = t40_ops_sync_device,
};
void *t40_dma_map_ops = t40_dma_ops_tbl;

int t40_dma_alloc_from_coherent_44(struct device *dev, ssize_t size,
				   dma_addr_t *handle, void **ret)
{
	return 0;
}

int t40_dma_release_from_coherent_44(struct device *dev, int order, void *vaddr)
{
	return 0;
}

/* ---- clocks and warnings the firmware calls directly ------------------------- */

void t40_warn_slowpath_null_44(const char *file, int line)
{
	pr_warn("t40-isp: firmware warning at %s:%d\n", file, line);
}
