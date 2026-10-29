#pragma once

#include <cstddef>
#include <memory>
#include <vector>

#include "niyata/port.hpp"
#include "niyata/task.hpp"
#include "niyata/clock.hpp"

namespace niyata {

    class Scheduler {

        public:

            static constexpr std::size_t kDefaultStack = 64 * 1024;

            Scheduler();
            ~Scheduler();
            Scheduler(const Scheduler&) = delete; // An OS Scheduler cannot be copied!
            Scheduler& operator = (const Scheduler&) = delete;

            // Create a task in the Ready state. Valid before or during run().
            Task* create_task(const char* name, TaskFn fn, void* arg = nullptr, std::size_t stack_size = kDefaultStack);

            // Dispatcher loop. Returns when no Ready task is left.
            void run();

            // Called from inside a task: give the CPU back to the dispatcher.
            void yield();

            // Called from inside a task: block for `ticks` ticks. sleep(0) == yield().
            void sleep(Clock::Tick ticks);

            // Called from outside tasks (tests / tick source): advance time by one
            // tick and make every sleeper whose time has come Ready.
            void tick();

            Clock::Tick now() const {
                return clock_.now();
            }

            bool all_finished() const;

            Task* current() const {
                return current_;
            }
        
        private:
            
            Task* pick_next();

            Clock clock_;
            std::vector<std::unique_ptr<Task>> tasks_;
            port::Context* sched_ctx_ = nullptr; // Dispatcher's own context
            Task* current_ = nullptr;
            std::size_t cursor_ = 0; // where round-robin scanning resumes
            
    };

} // namespace niyata