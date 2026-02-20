// SPDX-License-Identifier: GPL-2.0
/*
 * lat_bench - lightweight kernel function latency measurement via procfs
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>

#include "lat_bench.h"

static struct proc_dir_entry *lat_bench_dir;
atomic_t lat_bench_enabled = ATOMIC_INIT(0);
EXPORT_SYMBOL_GPL(lat_bench_enabled);

static DEFINE_SPINLOCK(lat_bench_lock);
static LIST_HEAD(lat_bench_list);

static int lat_bench_show(struct seq_file *m, void *v)
{
	struct lat_bench *lb = m->private;
	u64 total_ns = 0;
	u64 count = 0;
	u64 avg_ns = 0;
	int cpu;

	for_each_possible_cpu(cpu) {
		struct lat_bench_pcpu *p = per_cpu_ptr(lb->pcpu, cpu);

		total_ns += p->total_ns;
		count += p->count;
	}

	if (count)
		avg_ns = total_ns / count;

	seq_printf(m, "count: %llu\n", count);
	seq_printf(m, "total_ns: %llu\n", total_ns);
	seq_printf(m, "avg_ns: %llu\n", avg_ns);

	return 0;
}

static void lat_bench_clear_one(struct lat_bench *lb)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		struct lat_bench_pcpu *p = per_cpu_ptr(lb->pcpu, cpu);

		p->total_ns = 0;
		p->count = 0;
	}
}

static void lat_bench_clear_all(void)
{
	struct lat_bench *lb;

	spin_lock(&lat_bench_lock);
	list_for_each_entry(lb, &lat_bench_list, list)
		lat_bench_clear_one(lb);
	spin_unlock(&lat_bench_lock);
}

static int lat_bench_enable_show(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", atomic_read(&lat_bench_enabled));
	return 0;
}

static ssize_t lat_bench_enable_write(struct file *file,
				      const char __user *buf,
				      size_t count, loff_t *ppos)
{
	char kbuf[4];
	int val;

	if (count > sizeof(kbuf) - 1)
		return -EINVAL;
	if (copy_from_user(kbuf, buf, count))
		return -EFAULT;
	kbuf[count] = '\0';

	if (kstrtoint(kbuf, 10, &val))
		return -EINVAL;

	if (val == 0) {
		atomic_set(&lat_bench_enabled, 0);
		lat_bench_clear_all();
	} else if (val == 1) {
		atomic_set(&lat_bench_enabled, 1);
	} else {
		return -EINVAL;
	}

	return count;
}

static int lat_bench_enable_open(struct inode *inode, struct file *file)
{
	return single_open(file, lat_bench_enable_show, NULL);
}

static const struct proc_ops lat_bench_enable_ops = {
	.proc_open	= lat_bench_enable_open,
	.proc_read	= seq_read,
	.proc_write	= lat_bench_enable_write,
	.proc_lseek	= seq_lseek,
	.proc_release	= single_release,
};

int lat_bench_register(struct lat_bench *lb)
{
	if (!lat_bench_dir)
		return -ENOENT;

	lb->proc_entry = proc_create_single_data(lb->name, 0444,
						 lat_bench_dir,
						 lat_bench_show, lb);
	if (!lb->proc_entry)
		return -ENOMEM;

	spin_lock(&lat_bench_lock);
	list_add(&lb->list, &lat_bench_list);
	spin_unlock(&lat_bench_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(lat_bench_register);

void lat_bench_unregister(struct lat_bench *lb)
{
	if (lb->proc_entry) {
		spin_lock(&lat_bench_lock);
		list_del(&lb->list);
		spin_unlock(&lat_bench_lock);

		remove_proc_entry(lb->name, lat_bench_dir);
		lb->proc_entry = NULL;
	}
}
EXPORT_SYMBOL_GPL(lat_bench_unregister);

static int __init lat_bench_init(void)
{
	lat_bench_dir = proc_mkdir("lat_bench", NULL);
	if (!lat_bench_dir)
		return -ENOMEM;

	if (!proc_create("enable", 0644, lat_bench_dir,
			 &lat_bench_enable_ops)) {
		remove_proc_entry("lat_bench", NULL);
		return -ENOMEM;
	}

	return 0;
}
core_initcall(lat_bench_init);
