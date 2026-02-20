# lat_bench — Kernel Latency Measurement

A minimal library for measuring latency of Linux kernel code paths via `/proc/lat_bench/`.

## Integration

Copy this directory into your kernel tree and add to the parent `Kbuild`:

```makefile
obj-y += lat_bench/
```

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

## Runtime usage

```bash
# Enable measurement
echo 1 | sudo tee /proc/lat_bench/enable

# Run your workload, then read results
cat /proc/lat_bench/foo

# Disable and clear all counters
echo 0 | sudo tee /proc/lat_bench/enable
```

## Output

```
count: 142381       # total calls across all CPUs
total_ns: 568743200 # cumulative nanoseconds
avg_ns: 3995        # mean latency per call
```
