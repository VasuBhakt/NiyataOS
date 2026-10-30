# Niyata: build plan

This document is the working plan for building Niyata. It is written for an AI coding agent and for the author. Follow it in order. Do not skip ahead.

## 0. Project summary

Niyata is a deterministic, preemptive RTOS kernel in C++20. It is built host-first (Linux, simulated time, `ucontext`) and then ported to ARM Cortex-M (STM32). The point of the project is a balanced resume artifact: scheduling and concurrency for software roles, bare-metal Cortex-M work for embedded/low-level roles.

## 1. Hard rules (never violate)

1. **The core never touches the platform.** Nothing in `src/` or `include/niyata/` (except `port.hpp` and port implementations) may include `<ucontext.h>`, `<signal.h>`, `<unistd.h>`, `<chrono>`-based timing for scheduling, or any OS/hardware header. All platform behaviour goes through `include/niyata/port.hpp`.
2. **`port/host/context_ucontext.cpp` is the only file that uses `ucontext`.**
3. **Time is simulated and tick-driven on host.** No `SIGALRM`, no wall-clock-driven scheduling. The clock is its own component so tests can advance ticks deterministically.
4. **Fixed-priority preemptive is the default policy**, with O(1) task selection. The scheduler sits behind a small interface (`pick_next()`) so other policies (e.g. EDF) can be swapped in later.
5. **No dynamic allocation in the scheduler hot path.** Task stacks and control blocks are provided or pooled up front.
6. **Report worst case.** Benchmarks must report p50, p99 and max, not only averages.
7. **Every feature ships with tests.** No feature is "done" without a passing test in `tests/`.
8. **Do not add placeholder files.** Create a file only when the current week needs it.
9. **Do not enable AddressSanitizer** on host builds; it produces false positives with `swapcontext`.
10. **Keep the build green.** CI builds with g++ and clang++ and runs `ctest`.

## 2. Current state

Already in the repo:

- `CMakeLists.txt`, `tests/CMakeLists.txt` (GoogleTest via FetchContent), CI workflow
- `include/niyata/port.hpp`: `context_create_main`, `context_create`, `context_switch`, `context_destroy`
- `port/host/context_ucontext.cpp`: ucontext implementation (pointer split into two 32-bit halves for `makecontext`)
- `examples/hello_tasks.cpp`, `tests/test_context.cpp`

Environment: Linux or WSL2 only. C++20. Build with:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## 3. Target layout

Create files only when needed.

```
include/niyata/  task.hpp scheduler.hpp clock.hpp mutex.hpp semaphore.hpp pool.hpp port.hpp
src/             task.cpp scheduler.cpp clock.cpp mutex.cpp semaphore.cpp pool.cpp
port/host/       context_ucontext.cpp
port/cortex_m/   context_switch.S startup.c linker.ld ...
tests/           test_*.cpp
bench/           bench_context_switch.cpp ...
examples/        hello_tasks.cpp priority_inversion_demo.cpp
docs/            plan.md design.md porting.md benchmarks.md
```

## 4. Week-by-week

### Week 1: Cooperative core (host)

**Goal:** tasks run, yield, sleep, and finish cleanly under a cooperative round-robin scheduler with a simulated clock.

Tasks:
- `task.hpp` / `task.cpp`: task control block (TCB) with at least: id, name, state (`Ready`, `Running`, `Blocked`, `Finished`), priority, `port::Context*`, stack pointer and size, entry function and argument, `wake_tick`.
- `scheduler.hpp` / `scheduler.cpp`: `create_task(...)`, `yield()`, `run()`, a ready queue (a circular list is fine now), and a `pick_next()` interface.
- Handle task exit: when a task function returns, the scheduler marks it `Finished`, removes it from scheduling and switches to the next ready task (use `on_exit` in `context_create`; the scheduler can exit through its own context). A finished task must never crash the scheduler.
- `clock.hpp` / `clock.cpp`: a tick counter with `tick()` and `now()`. Simulated only. Kept separate from the scheduler.
- `sleep(ticks)`: sets `wake_tick`, marks the task `Blocked`, and the scheduler wakes it when the clock reaches `wake_tick`.
- Handle the "no ready task" case (idle). Define behaviour explicitly and test it.

Tests (`tests/test_cooperative.cpp`, `tests/test_clock_sleep.cpp`):
- 3 tasks interleave in a deterministic order.
- A task that finishes does not break the others.
- `sleep(n)` resumes the task at exactly tick `now + n`, not before.
- Multiple sleepers wake in the correct order.
- All tasks finished: `run()` returns cleanly.

Done when: all tests pass locally and in CI, `hello_tasks` is updated or replaced to use the real scheduler.

### Week 2: Preemption and fixed priorities

**Goal:** a higher-priority ready task always runs instead of a lower-priority one, driven by the simulated tick.

**Priority Convention:**
- Priorities range from `0` to `31` (`kNumPriorities = 32`).
- **Lower number = higher priority** (`0` is the highest real-time priority, `31` is the lowest/idle priority). This directly mirrors the ARM Cortex-M hardware interrupt priority model.
- Task selection is strict $O(1)$: `ready_bitmap_` tracks non-empty priority queues, and `__builtin_ctz` finds the lowest set bit in a single CPU instruction.

Tasks:
- [x] Priorities (lower number = higher priority, documented above).
- [x] Per-priority ready lists (`TaskQueue ready_[32]`) plus a 32-bit priority bitmap (`ready_bitmap_`) for $O(1)$ task selection (`__builtin_ctz`).
- [x] Tick handler: advances the clock, wakes sleepers, and triggers a reschedule if a higher-priority task became ready.
- [x] Round-robin among equal-priority tasks with a configurable time slice (`time_slice_` and `slice_left_`).
- [x] Preemption on host is simulated: the tick handler is called explicitly by the test harness or a driver loop, never by a real signal.

Tests (`tests/test_preemption.cpp`):
- [x] A high-priority task waking from sleep preempts a running low-priority task on that same tick.
- [x] Equal-priority tasks round-robin on time slice expiry.
- [x] Lower-priority tasks make progress only when higher-priority tasks are blocked.
- [x] Preempted tasks keep their turn (pushed to front of their priority queue).

Done when: preemption is deterministic and fully covered by tests. (Completed)

### Week 3: Synchronization and priority inheritance

**Goal:** mutex and semaphore, with a reproduced priority inversion and its fix.

Tasks:
- Counting semaphore (`take`/`give`, with blocking, and a timeout in ticks).
- Mutex with an owner, blocking, and wake-up of the highest-priority waiter.
- First implement the mutex **without** priority inheritance and write a test that reproduces classic priority inversion (low holds the lock, high blocks on it, medium starves low).
- Add priority inheritance: the owner temporarily inherits the highest waiting priority and restores its base priority on release. Handle nested locks correctly.
- `examples/priority_inversion_demo.cpp`: prints the sequence with and without inheritance.

Tests (`tests/test_priority_inversion.cpp`, `tests/test_sync.cpp`):
- The inversion test fails or shows the inversion with inheritance off, and passes with it on.
- Mutex wakes the highest-priority waiter first.
- Base priority is restored after release, including with nested locks.
- Semaphore blocking, timeout, and ordering.

Done when: the inversion test demonstrates both behaviours. This is the headline demo of the project.

### Week 4: Allocator, benchmarks, write-up (host complete)

**Goal:** host version is finished and presentable even if the hardware port never happens.

Tasks:
- `pool.hpp` / `pool.cpp`: fixed-block pool allocator, O(1) alloc and free, no fragmentation, deterministic.
- `bench/bench_context_switch.cpp`: measure context-switch latency and scheduling overhead. Report p50, p99, max, and jitter. Use a high-resolution monotonic clock for measuring only (never for scheduling). Warm up, run many iterations, pin to a core where possible.
- `docs/design.md`: design decisions and tradeoffs (why fixed-priority, why tick-driven, why the port layer, priority inheritance vs ceiling).
- `docs/benchmarks.md`: results table with methodology. Never invent numbers; only record measured results.
- Optional, only if time remains: EDF as a pluggable policy behind `pick_next()`.
- Read FreeRTOS `tasks.c` and the Cortex-M `port.c` and xv6 `swtch.S` before week 5.

Done when: README roadmap is updated and the benchmarks doc has real numbers.

### Week 5: STM32 port

**Goal:** real context switching on Cortex-M.

Board: TBD (assume Cortex-M3/M4). Confirm the exact board and toolchain before starting. Toolchain: `arm-none-eabi-gcc` with a CMake toolchain file; flashing and debugging via OpenOCD and GDB (or STM32CubeProgrammer from Windows if using WSL).

Tasks:
- `port/cortex_m/`: startup code, vector table, linker script, minimal clock setup. Avoid relying on HAL for the kernel core.
- SysTick as the tick source.
- PendSV as the context switch handler, set to the lowest interrupt priority; SVC to start the first task.
- Use MSP for the kernel/exceptions and PSP for tasks.
- Build the initial stack frame for a new task: hardware-stacked registers (xPSR with the Thumb bit set = `0x01000000`, PC = entry, LR = task-exit handler, R12, R3-R0 with R0 = argument), plus the software-saved R4-R11. Use `EXC_RETURN` `0xFFFFFFFD` to return to thread mode on PSP.
- Critical sections via interrupt masking (PRIMASK, or BASEPRI to leave high-priority interrupts alive).
- Implement the same `port.hpp` contract, or extend it where the Cortex-M model genuinely differs, without leaking platform details into the core.
- Demo: two or three tasks at different priorities toggling GPIOs / writing UART.

Done when: preemptive multitasking runs on hardware with the same scheduler core.

### Week 6: Hardware measurements and demo

Tasks:
- Measure real context-switch latency with a GPIO toggle plus a scope or logic analyzer (and/or the DWT cycle counter).
- Demonstrate priority inversion and its fix on hardware.
- Update `docs/benchmarks.md` with measured hardware numbers and method.
- Write the blog post and record a short demo video.
- Update the README with results and links.

## 5. Benchmark methodology

- Measure, do not estimate. Record the machine, compiler, flags, and iteration counts.
- Report min, p50, p99, max, and jitter (max minus min, or standard deviation).
- Separate cold (first run) from warm results.
- Host numbers are for comparison between design choices, not claims about real-time guarantees. Hardware numbers are the real ones.

## 6. Scope control

If time runs short, cut in this order (cut from the top first):
1. EDF or any alternative scheduling policy
2. Pool allocator
3. Hardware measurement polish (keep at least one real measurement)
4. Semaphore timeouts

Never cut: preemptive fixed-priority scheduling, mutex with priority inheritance, the priority-inversion demo and test, the Cortex-M context switch.

## 7. Definition of done (per task)

- Compiles cleanly with `-Wall -Wextra -Wpedantic` on g++ and clang++.
- Tests added and passing locally and in CI.
- No platform headers leaked into the core.
- Docs updated if a design decision changed.
- Small, focused commits with clear messages.