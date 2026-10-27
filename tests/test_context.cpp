#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "niyata/port.hpp"

using namespace niyata::port;

namespace {

    struct Env {
        Context* main_ctx = nullptr;
        Context* a = nullptr;
        Context* b = nullptr;
        std::string log;
    };

    Env* g_env;

    void task_a(void* ) {
        for (int i = 0; i < 3; i++) {
            g_env->log += "A" + std::to_string(i)+ " ";
            context_switch(g_env->a, g_env->b);
        }
    }

    void task_b(void* ) {
        for (int i = 0; i < 3; i++) {
            g_env->log += "B" + std::to_string(i)+ " ";
            context_switch(g_env->b, g_env->a);
        }
    }

} // namespace

TEST(Context, TwoTasksInterleaveDeterministically) {
    constexpr std::size_t kStack = 64 * 1024;
    std::vector<char> sa(kStack), sb(kStack);
    Env env;
    g_env = &env;
    
    env.main_ctx = context_create_main();
    env.a = context_create(sa.data(), kStack, task_a, nullptr, env.main_ctx);
    env.b = context_create(sb.data(), kStack, task_b, nullptr, env.main_ctx);
    
    context_switch(env.main_ctx, env.a);

    EXPECT_EQ(env.log, "A0 B0 A1 B1 A2 B2 ");

    context_destroy(env.a);
    context_destroy(env.b);
    context_destroy(env.main_ctx);
}

TEST(Context, ArgumentIsPassedToEntry) {
    constexpr std::size_t kStack = 64 * 1024;
    std::vector<char> s(kStack);
    static int value;
    value = 0;

    Context* main_ctx = context_create_main();
    Context* task_ctx = context_create(s.data(), kStack, [](void* arg){ *static_cast<int*>(arg) = 31; }, &value, main_ctx);

    context_switch(main_ctx, task_ctx);
    EXPECT_EQ(value, 31);

    context_destroy(task_ctx);
    context_destroy(main_ctx);
}