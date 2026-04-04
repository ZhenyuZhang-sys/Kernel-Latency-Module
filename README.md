# lat_bench — Kernel Latency Measurement

A minimal library for measuring latency of Linux kernel code paths via `/proc/lat_bench/`. Includes optional per-probe histograms with fine-grained linear-within-log2 bucketing.

## Integration

Copy this directory into your kernel tree and add to the parent `Kbuild`:

```makefile
obj-y += lat_bench/
```

The module is initialized via `core_initcall`, so it is available early in boot.

## Instrumenting code

```c
#include "../lat_bench/lat_bench.h"

DEFINE_LAT_BENCH(lb_foo, "foo");   /* creates /proc/lat_bench/foo */

void my_func(void)
{
    ktime_t start = lat_bench_start();
    /* ... code to measure ... */
    lat_bench_end(&lb_foo, start);
}

static int __init my_init(void)
{
    return lat_bench_register(&lb_foo);
}
```

Call `lat_bench_unregister(&lb_foo)` in the exit path for modules.

Measurement points use per-CPU counters, so recording is lock-free on the hot path.

## Runtime usage

```bash
# Enable measurement (clears all counters first)
echo 1 | sudo tee /proc/lat_bench/enable

# Run your workload, then read results
cat /proc/lat_bench/foo

# Disable and clear all counters
echo 0 | sudo tee /proc/lat_bench/enable
```

### Histogram

An optional latency histogram can be enabled independently:

```bash
# Enable histogram collection (clears previous histogram data)
echo 1 | sudo tee /proc/lat_bench/hist_enable

# Disable histogram (data is preserved for reading)
echo 0 | sudo tee /proc/lat_bench/hist_enable
```

The histogram uses 4 sub-buckets per power-of-2 (linear-within-log2), covering 1 ns to ~16 ms (2^24 ns) in 97 buckets. Bucket layout:

| Buckets | Range |
|---------|-------|
| 0-3     | [1, 2) ns |
| 4-7     | [2, 4) ns |
| 8-11    | [4, 8) ns |
| ...     | ... |
| 92-95   | [2^23, 2^24) ns |
| 96      | >= 2^24 ns (overflow) |

## Output format

```
count: 142381            # total calls across all CPUs
total_ns: 568743200      # cumulative nanoseconds
avg_ns: 3995             # mean latency per call
hist_shift: 2            # log2(sub-buckets per power-of-2)
hist_buckets: 97         # total number of histogram buckets
hist: 0 0 12 53 ...      # space-separated bucket counts
```

## Files

| File | Description |
|------|-------------|
| `lat_bench.h` | Public API: macros, inline start/end functions, histogram bucket mapping |
| `lat_bench.c` | procfs setup, registration, enable/hist_enable controls |
| `Makefile`    | Kbuild makefile |

## License

GPL-2.0
