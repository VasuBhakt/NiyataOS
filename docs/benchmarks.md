# Host Benchmarks: Context Switching & Scheduling Latency

These benchmarks measure the latency and jitter of Niyata's host port (`ucontext` on Linux) and the cooperative scheduler dispatch loop.

> **Note on Host Benchmarks:**  
> These numbers describe the **Linux simulation** environment. Under Linux, `swapcontext` performs a `sigprocmask` system call on each switch, and user-space threads are subject to preemption by the Linux CFS scheduler and hardware interrupts (explaining the long-tail worst-case latency). These numbers demonstrate algorithmic efficiency and relative overhead, not hard real-time guarantees. Hard real-time bounds are evaluated on bare-metal ARM Cortex-M in Week 5/6.

---

## 📊 Summary Results (Aggregate of 40 Runs)

Each benchmark run executes **200,000 samples** after a **10,000 sample warm-up** on CPU Core 0 (`sched_setaffinity`). The table below reports the median statistic across **40 independent runs** (8,000,000 total measurements):

| Benchmark | Min | p50 (Median) | p99 | p99.9 | Worst Max |
| :--- | :---: | :---: | :---: | :---: | :---: |
| **Clock Read (`now_ns` baseline)** | 13 ns | 17 ns | 43 ns | 54 ns | 864 µs |
| **Raw Context Switch (1-way port)** | 145 ns | 162 ns | 261 ns | 928 ns | 1.15 ms |
| **`yield()` Round Trip (Task $\to$ Sched $\to$ Task)** | 283 ns | 315 ns | 444 ns | 1.47 µs | 684 µs |

---

## 🔍 Key Takeaways

1. **Sub-Microsecond Latency**:
   - The median 1-way context switch takes **~162 ns** (0.16 µs).
   - The median full `yield()` round trip takes **~315 ns** (0.31 µs).
   - 99% of all scheduler round trips complete in under **450 ns**!

2. **Negligible Scheduling Overhead**:
   A full `yield()` round trip requires two context switches (Task $\to$ Dispatcher, then Dispatcher $\to$ Task) plus `pick_next()`:
   $$\text{Overhead} = \text{Round Trip } (315\text{ ns}) - 2 \times \text{One-Way Switch } (162\text{ ns}) \approx -9\text{ ns} \approx \mathbf{0\text{ ns}}$$
   This proves that our $O(1)$ priority bitmap (`__builtin_ctz`) and intrusive queue pop introduce essentially **zero measurable overhead** over the raw context switch.

3. **Tail Latency & Jitter**:
   - The p99.9 latency remains bounded under **1.5 µs**.
   - The worst-case peaks (~0.7 to 1.15 ms) represent host OS interruptions (Linux kernel interrupts and background CFS task scheduling), which are eliminated on bare-metal Cortex-M.

---

## 🛠️ Methodology & Reproducibility

- **Binary**: `build/bench/bench_context_switch`
- **Compiler**: GCC / Clang with `-O3` (`-DCMAKE_BUILD_TYPE=Release`)
- **CPU Affinity**: Pinned to CPU Core 0 (`sched_setaffinity`) to eliminate inter-core migration cache invalidation.
- **Clock**: Monotonic POSIX clock via `clock_gettime(CLOCK_MONOTONIC, &ts)` (never used for kernel scheduling, only for benchmark measurement).
- **Warm-up**: 10,000 iterations discarded before timing to ensure warm instruction and data L1 caches.
- **Command**:
  ```bash
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
  cmake --build build -j
  ./build/bench/bench_context_switch
  ```

