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
 *
 * Histogram: enable via /proc/lat_bench/hist_enable.
 * Uses 4 sub-buckets per power-of-2 (linear-within-log2) for fine
 * granularity.  Covers 1 ns to ~16 ms (2^24 ns) in 97 buckets.
 */
#ifndef _LAT_BENCH_H
#define _LAT_BENCH_H

#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/ktime.h>
#include <linux/list.h>
#include <linux/percpu.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/types.h>

extern atomic_t lat_bench_enabled;
extern atomic_t lat_bench_hist_enabled;

/*
 * Histogram parameters:
 *   LAT_BENCH_HIST_SHIFT  - log2(sub-buckets per power-of-2) = 2 → 4 sub-buckets
 *   LAT_BENCH_HIST_BITS   - powers-of-2 covered (0..23 → up to 2^24 = ~16 ms)
 *   LAT_BENCH_HIST_BUCKETS - HIST_BITS * (1 << HIST_SHIFT) + 1 overflow
 */
#define LAT_BENCH_HIST_SHIFT	2
#define LAT_BENCH_HIST_SUBS	(1 << LAT_BENCH_HIST_SHIFT)  /* 4 */
#define LAT_BENCH_HIST_BITS	24
#define LAT_BENCH_HIST_BUCKETS	(LAT_BENCH_HIST_BITS * LAT_BENCH_HIST_SUBS + 1) /* 97 */

struct lat_bench_pcpu {
	u64 total_ns;
	u64 count;
	u64 hist[LAT_BENCH_HIST_BUCKETS];
};

struct lat_bench {
	const char *name;
	struct lat_bench_pcpu __percpu *pcpu;
	struct proc_dir_entry *proc_entry;
	struct list_head list;
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

/*
 * lat_bench_bucket - map a latency delta (ns) to a histogram bucket index.
 *
 * Bucket layout (4 sub-buckets per power-of-2):
 *   bucket 0..3:   [1,2)   split into 4 sub-ranges  (but [1,2) has no sub-range, all map to sub 0)
 *   bucket 4..7:   [2,4)   → [2,3), [3,4) effectively
 *   bucket 8..11:  [4,8)   → [4,5), [5,6), [6,7), [7,8)
 *   ...
 *   bucket 92..95: [2^23, 2^24)
 *   bucket 96:     overflow (>= 2^24)
 *
 * For delta in [2^k, 2^(k+1)):
 *   bucket = k * 4 + ((delta >> (k - 2)) & 3)   when k >= 2
 *   bucket = k * 4                                when k < 2
 */
static inline int lat_bench_bucket(s64 delta_ns)
{
	int msb, sub;

	if (delta_ns <= 0)
		return 0;

	msb = fls64((u64)delta_ns);  /* 1-based: fls64(1)=1, fls64(2..3)=2, fls64(4..7)=3 */
	if (msb > LAT_BENCH_HIST_BITS)
		return LAT_BENCH_HIST_BUCKETS - 1;  /* overflow bucket */

	/* msb is 1-based, so power-of-2 index k = msb - 1.
	 * For k >= HIST_SHIFT, extract sub-bucket from the next HIST_SHIFT bits
	 * below the MSB.
	 */
	if (msb - 1 >= LAT_BENCH_HIST_SHIFT)
		sub = ((u64)delta_ns >> (msb - 1 - LAT_BENCH_HIST_SHIFT)) &
		      (LAT_BENCH_HIST_SUBS - 1);
	else
		sub = 0;

	return (msb - 1) * LAT_BENCH_HIST_SUBS + sub;
}

static inline void lat_bench_end(struct lat_bench *lb, ktime_t start)
{
	s64 delta;
	struct lat_bench_pcpu *p;

	if (!atomic_read(&lat_bench_enabled))
		return;

	delta = ktime_to_ns(ktime_sub(ktime_get(), start));
	p = this_cpu_ptr(lb->pcpu);

	p->total_ns += delta;
	p->count++;

	if (atomic_read(&lat_bench_hist_enabled))
		p->hist[lat_bench_bucket(delta)]++;
}

int lat_bench_register(struct lat_bench *lb);
void lat_bench_unregister(struct lat_bench *lb);

#endif /* _LAT_BENCH_H */
