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

#include "lat_bench.h"

static struct proc_dir_entry *lat_bench_dir;

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

int lat_bench_register(struct lat_bench *lb)
{
	if (!lat_bench_dir)
		return -ENOENT;

	lb->proc_entry = proc_create_single_data(lb->name, 0444,
						 lat_bench_dir,
						 lat_bench_show, lb);
	if (!lb->proc_entry)
		return -ENOMEM;

	return 0;
}
EXPORT_SYMBOL_GPL(lat_bench_register);

void lat_bench_unregister(struct lat_bench *lb)
{
	if (lb->proc_entry) {
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

	return 0;
}
core_initcall(lat_bench_init);
