// Day 1-2 throwaway: two stacks, manual switching, no scheduler yet.
// Expected output: A0 B0 A1 B1 A2 B2 done

#include <cstdio>
#include <vector>

#include "niyata/port.hpp"

using namespace niyata::port;

static Context* g_main;
static Context* g_a;
static Context* g_b;

static void task_a(void*) {
    for (int i = 0; i < 3; ++i) {
        std::printf("A%d ", i);
        context_switch(g_a, g_b);
    }
}

static void task_b(void*) {
    for (int i = 0; i < 3; ++i) {
        std::printf("B%d ", i);
        context_switch(g_b, g_a);
    }
}

int main() {
    constexpr std::size_t kStack = 64 * 1024;
    std::vector<char> stack_a(kStack), stack_b(kStack);

    g_main = context_create_main();
    g_a = context_create(stack_a.data(), kStack, task_a, nullptr, g_main);
    g_b = context_create(stack_b.data(), kStack, task_b, nullptr, g_main);

    context_switch(g_main, g_a);  // when task_a returns, we land back here
    std::printf("done\n");

    context_destroy(g_a);
    context_destroy(g_b);
    context_destroy(g_main);
    return 0;
}
