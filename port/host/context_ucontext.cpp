// Host port: context switching via POSIX ucontext (Linux/glibc).
// This is the only file which touches <ucontext.h>

#include "niyata/port.hpp"

#include <ucontext.h>

#include <cstdint>
#include <cstdlib>

namespace niyata::port {

    struct Context {
        ucontext_t uc;
        EntryFn fn = nullptr;
        void* arg = nullptr;
    };

    namespace {

        // makecontext only passes int arguments, so split the pointer into two halves.
        void split_pointer(unsigned int hi, unsigned int lo) {
            auto addr = (static_cast<std::uintptr_t>(hi) << 32) | static_cast<std::uintptr_t>(lo);
            auto* ctx = reinterpret_cast<Context*>(addr);
            ctx->fn(ctx->arg);
            // Returning here follows uc_link (the on_exit context).
        }
        
    } // namespace

    Context* context_create_main() {
        return new Context{}; // filled in by the first swapcontext
    }

    Context* context_create(void* stack, std::size_t stack_size, EntryFn fn, void* arg, Context* on_exit) {
        auto* ctx = new Context;
        ctx->fn = fn;
        ctx->arg = arg;
        
        if (getcontext(&ctx->uc) != 0) std::abort();
        ctx->uc.uc_stack.ss_sp = stack;
        ctx->uc.uc_stack.ss_size = stack_size;
        ctx->uc.uc_link = &on_exit->uc;

        auto addr = reinterpret_cast<std::uintptr_t>(ctx);
        makecontext(&ctx->uc, reinterpret_cast<void (*)()>(split_pointer), 2, static_cast<unsigned int>(addr >> 32), static_cast<unsigned int>(addr & 0xffffffffu));
        return ctx;
    }

    void context_destroy(Context* ctx) {
        delete ctx;
    }

    void context_switch(Context* from, Context* to)  {
        if (swapcontext(&from->uc, &to->uc) != 0) std::abort();
    }

} // namespace niyata::port