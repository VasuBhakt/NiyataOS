// Host benchmarks: context-switch latency and scheduling overhead.
// Run a Release build, ideally with nothing else busy:
//      ./build/bench/bench_context_switch

// These numbers describe the HOST simulation (glibc swapcontext,
// which makes a signal-mask syscall on every switch). They are
// for comparing design choices, not claims about real-time behaviour.

#include <sched.h>
#include <time.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

#include "niyata/port.hpp"
#include "niyata/scheduler.hpp"

using namespace niyata;

static inline std::uint64_t now_ns() {
    timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<std::uint64_t>(ts.tv_sec) * 1000000000ull + ts.tv_nsec;
}

constexpr std::size_t kWarmup = 10000;
constexpr std::size_t kSamples = 200000;

static void report(const char* name, std::vector<std::uint64_t>& v) {
    v.erase(v.begin(), v.begin() + kWarmup); // drop warm-up
    std::sort(v.begin(), v.end());
    auto pct = [&](double p) {
        return v[static_cast<std::size_t>(p * (v.size() - 1) + 0.5)];
    };
    std::printf("%-30s %7llu %7llu %7llu %8llu %9llu\n", name,
        (unsigned long long)v.front(), (unsigned long long)pct(0.50),
        (unsigned long long)pct(0.99), (unsigned long long)pct(0.999),
        (unsigned long long)v.back()
    );
}

// 0. Cost of reading the clock itself
static void bench_clock() {
    std::vector<std::uint64_t> v(kWarmup + kSamples);
    for (auto& x : v) {
        std::uint64_t a = now_ns();
        x = now_ns() - a;
    }
    report("clock read (baseline)", v);
}

// 1. Raw one-way context switch through the port layer
namespace {
    port::Context *g_main, *g_a, *g_b;
    std::uint64_t g_t0;
    std::vector<std::uint64_t>* g_samples;
    std::size_t g_idx;

    void pp_a(void*) {
        for (std::size_t i = 0; i < kWarmup + kSamples; i++) {
            g_t0 = now_ns();
            port::context_switch(g_a, g_b); // B timestamps on arrival
        }
    }

    void pp_b(void*) {
        for(;;) {
            std::uint64_t t1 = now_ns();
            (*g_samples)[g_idx++] = t1 - g_t0;
            port::context_switch(g_b, g_a);
        }
    }

} // namespace

static void bench_raw_switch() {
    std::vector<std::uint64_t> v(kWarmup + kSamples);
    g_samples = &v;
    g_idx = 0;
    std::vector<char> sa(64 * 1024), sb(64 * 1024);
    g_main = port::context_create_main();
    g_a = port::context_create(sa.data(), sa.size(), pp_a, nullptr, g_main);
    g_b = port::context_create(sb.data(), sb.size(), pp_b, nullptr, g_main);
    port::context_switch(g_main, g_a);
    report("raw context switch (1-way)", v);
    port::context_destroy(g_a);
    port::context_destroy(g_b);
    port::context_destroy(g_main);
}

// 2. yield() round trip: task -> dispatcher -> pick_next -> task
static void bench_yield() {
    std::vector<std::uint64_t> v(kWarmup + kSamples);
    Scheduler s;
    struct A {
        Scheduler* s;
        std::vector<std::uint64_t>* v;
    } a{&s, &v};
    s.create_task("bench", [](void* p) {
        auto* a = static_cast<A*>(p);
        for (auto& x : *a->v) {
            std::uint64_t t0 = now_ns();
            a->s->yield();
            x = now_ns() - t0;
        }
    }, &a);
    s.run();
    report("yield() round trip", v);
}

int main() {
    #ifndef NDEBUG
        std::printf("WARNING: not an optimized build. Use -DCMAKE_BUILD_TYPE=Release.\n");
    #endif
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(0, &set);
        if (sched_setaffinity(0, sizeof(set), &set) != 0) {
            std::printf("note: could not pin to CPU 0\n");
        }

        std::printf("%zu samples after %zu warm-up. All times in ns.\n\n", kSamples, kWarmup);
        std::printf("%-30s %7s %7s %7s %8s %9s\n",
            "benchmark", "min", "p50", "p99", "p99.9", "max");
        bench_clock();
        bench_raw_switch();
        bench_yield();
}