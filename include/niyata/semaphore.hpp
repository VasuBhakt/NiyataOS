#pragma once

#include "niyata/scheduler.hpp"
#include "niyata/wait_queue.hpp"

namespace niyata {

    class Semaphore {

        public:
            explicit Semaphore(Scheduler& s, unsigned initial = 0): sched_(s), count_(initial) {}

            void take(); // from a task; blocks while the count is 0
            void give(); // may be called from a task or from outside

            unsigned count() const {
                return count_;
            }
        
        private:
            Scheduler &sched_;
            unsigned count_;
            WaitQueue waiters_;
    };
} // namespace niyata