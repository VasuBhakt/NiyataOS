#pragma once
// Port interface: the ONLY thing the scheduler core knows about the platform.
// Host build implements this with ucontext; the Cortex-M build will implement
// it with a PendSV handler in assembly. The core never includes <ucontext.h>.

#include <cstddef>

namespace niyata::port {
    
    using EntryFn = void (*)(void*);

    // Opaque, defined by each port
    struct Context;

    // Context representing "whoever is running right now" (e.g. main()).
    // context_switch(from, to) saves the current state into `from`.
    Context* context_create_main();

    // Create a context that will run fn(arg) on the given stack the first time it
    // is switched to. If fn returns, control goes to `on_exit` (must not be null).
    Context* context_create(void* stack, std::size_t stack_size, EntryFn fn, void* arg, Context* on_exit);

    // Destroy a stack-based context (frees memory if port allocated it).
    void context_destroy(Context* ctx);

    // Save current state into `from`, resume `to`.
    void context_switch(Context* from, Context* to);

} // namespace niyata::port