#pragma once

#include <cstdint>

#include "niyata/scheduler.hpp"
#include "niyata/wait_queue.hpp"

namespace niyata {

    // Non-recursive mutex with optional priority inheritance.
    class Mutex {

        public:
            explicit Mutex(Scheduler &s, bool priority_inheritance = true) : sched_(s), inherit_(priority_inheritance) {}

            void lock(); // from a task; blocks if owned
            void unlock(); // must be called by the owner

            Task* owner() const {
                return owner_;
            }
        
        private:
            void add_held(Task* t);
            void remove_held(Task* t);
            void boost_owner_chain(std::uint8_t prio);
            static std::uint8_t effective_priority(const Task* t);

            Scheduler& sched_;
            bool inherit_;
            Task* owner_ = nullptr;
            WaitQueue waiters_;
            Mutex* next_held_ = nullptr; // next mutex held by this task
    };
} // namespace niyata