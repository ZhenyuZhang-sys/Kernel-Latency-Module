/* SPDX-License-Identifier: GPL-2.0 */
/*
 * lat_bench - lightweight kernel function latency measurement
 *
 * Portable: copy this directory to any kernel tree and add
 *   obj-y += lat_bench/
 * to the top-level Kbuild file.
 *
 * Usage:
 *   #include "../lat_bench/lat_bench.h"
 *
 *   DEFINE_LAT_BENCH(lb_foo, "foo");
 *
 *   void some_func(void) {
 *       ktime_t start = lat_bench_start();
 *       // ... code to measure ...
 *       lat_bench_end(&lb_foo, start);
 *   }
 *
 *   // in an __init function:
 *   lat_bench_register(&lb_foo);
 *
 * Then read /proc/lat_bench/foo for results.
 */
#ifndef _LAT_BENCH_H
#define _LAT_BENCH_H

#include <linux/ktime.h>
#include <linux/percpu.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/types.h>

struct lat_bench_pcpu {
	u64 total_ns;
	u64 count;
};

struct lat_bench {
	const char *name;
	struct lat_bench_pcpu __percpu *pcpu;
	struct proc_dir_entry *proc_entry;
};

/*
 * DEFINE_LAT_BENCH - declare a latency measurement point
 * @var_name: C variable name for the lat_bench struct
 * @str_name: string name shown in /proc/lat_bench/<str_name>
 */
#define DEFINE_LAT_BENCH(var_name, str_name)				\
	static DEFINE_PER_CPU(struct lat_bench_pcpu, var_name##_pcpu);	\
	static struct lat_bench var_name = {				\
		.name = str_name,					\
		.pcpu = &var_name##_pcpu,				\
	}

static inline ktime_t lat_bench_start(void)
{
	return ktime_get();
}

static inline void lat_bench_end(struct lat_bench *lb, ktime_t start)
{
	s64 delta = ktime_to_ns(ktime_sub(ktime_get(), start));
	struct lat_bench_pcpu *p = this_cpu_ptr(lb->pcpu);

	p->total_ns += delta;
	p->count++;
}

int lat_bench_register(struct lat_bench *lb);
void lat_bench_unregister(struct lat_bench *lb);

#endif /* _LAT_BENCH_H */
