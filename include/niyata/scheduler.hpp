#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "niyata/port.hpp"
#include "niyata/task.hpp"
#include "niyata/clock.hpp"
#include "niyata/task_queue.hpp"

namespace niyata {

    class Scheduler {

        public:

            static constexpr std::size_t kDefaultStack = 64 * 1024;
            static constexpr unsigned kNumPriorities = 32; // 0 = highest
            static constexpr std::uint8_t kDefaultPriority = 16;

            explicit Scheduler(Clock::Tick time_slice = 4);
            ~Scheduler();
            Scheduler(const Scheduler&) = delete; // An OS Scheduler cannot be copied!
            Scheduler& operator = (const Scheduler&) = delete;

            // Create a task in the Ready state. Valid before or during run().
            Task* create_task(const char* name, TaskFn fn, void* arg = nullptr, std::uint8_t priority = kDefaultPriority, std::size_t stack_size = kDefaultStack);

            // Dispatcher loop. Returns when no Ready task is left.
            void run();

            // Called from inside a task: give the CPU back to the dispatcher.
            void yield();

            // Called from inside a task: block for `ticks` ticks. sleep(0) == yield().
            void sleep(Clock::Tick ticks);

            // Burn CPU for `ticks` ticks (simulated)
            void work(Clock::Tick ticks);

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
            void make_ready(Task* t, bool front = false);
            Task* pick_next();
            int highest_ready() const; // -1 if nothing is ready
            void preempt_if_needed();

            Clock clock_;
            std::vector<std::unique_ptr<Task>> tasks_;
            port::Context* sched_ctx_ = nullptr; // Dispatcher's own context
            Task* current_ = nullptr;

            TaskQueue ready_[kNumPriorities];
            std::uint32_t ready_bitmap_ = 0; // bit p set <=> ready_[p] non-empty

            Clock::Tick time_slice_;
            Clock::Tick slice_left_ = 0;
            
    };

} // namespace niyata