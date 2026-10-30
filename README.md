# Niyata

**A deterministic, preemptive real-time operating system kernel in C++, built host-first and ported to ARM Cortex-M.**

> *niyata* (Sanskrit, नियत): fixed, regulated, determined.

Niyata is a small RTOS kernel written to understand, and demonstrate, how real-time schedulers actually work: context switching, priority-based preemption, synchronization primitives, and the classic priority inversion problem. It is developed in two stages: a fully testable simulation on Linux, then a port to real hardware (STM32, Cortex-M).

The design goal is **determinism**: bounded, predictable worst-case latency, rather than best average throughput.

## Status

Early development. Track progress in the roadmap below.

## Features

| Feature | Status |
| --- | --- |
| Portable port layer (platform-independent core) | Done |
| Task control blocks and context switching | Done |
| Cooperative round-robin scheduling | Done |
| Simulated, tick-driven clock and `sleep` | Done |
| Fixed-priority preemptive scheduling (O(1) task selection) | Done |
| Mutex and counting semaphore | Planned |
| Priority inheritance | Planned |
| Fixed-block pool allocator | Planned |
| Latency and jitter benchmarks (p50 / p99 / max) | Planned |
| Cortex-M port (SysTick, PendSV) | Planned |

## Design principles

- **Deterministic first.** Worst-case behaviour is measured and reported, not just averages.
- **Portable core.** The scheduler never touches platform headers. Everything platform-specific lives behind a small port interface, so the same core runs on a Linux simulation and on Cortex-M.
- **Testable.** On the host, time is simulated and tick-driven, so scheduling behaviour is reproducible and covered by unit tests.
- **Small and readable.** The kernel is meant to be read end to end.

## Architecture

```
          +-----------------------------+
          |       Application tasks     |
          +--------------+--------------+
                         |
          +--------------v--------------+
          |  Niyata core (src/)         |
          |  scheduler | clock | sync   |
          +--------------+--------------+
                         |   port.hpp
          +--------------v--------------+
          |  Port layer                 |
          |  host: ucontext             |
          |  cortex-m: PendSV + asm     |
          +-----------------------------+
```

## Repository layout

```
include/niyata/   public headers
src/              platform-independent kernel core
port/host/        host port (POSIX ucontext)
port/cortex_m/    Cortex-M port (assembly context switch)
tests/            GoogleTest unit tests
bench/            latency and jitter benchmarks
examples/         small demos
docs/             design notes and build plan
```

## Building

The host build requires Linux or WSL2 (the simulation uses `ucontext`, which needs glibc).

```bash
sudo apt install -y build-essential cmake git gdb clang

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/hello_tasks
```

Expected output of the demo:

```
A0 B0 A1 B1 A2 B2 done
```

## Roadmap

- [x] Cooperative core: task control block, context switch, round-robin, simulated clock, sleep
- [x] Preemption and fixed-priority scheduling
- [ ] Mutex, semaphore, priority inversion demo and priority inheritance fix
- [ ] Pool allocator, benchmarks, design write-up
- [ ] STM32 port (SysTick, PendSV, MSP/PSP)
- [ ] On-hardware latency measurements and demo

## Benchmarks

Results will be published in [`docs/benchmarks.md`](docs/benchmarks.md) once the host benchmarks are in.

## Maintainer

[Swastik Bose](https://github.com/VasuBhakt), a.k.a VasuBhakt

## License

[MIT](LICENSE)