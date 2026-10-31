#include "niyata/semaphore.hpp"

#include <cassert>

namespace niyata {

    void Semaphore::take() {
        Task* self = sched_.current();
        assert(self && "take() must be called from a task");

        if (count_ > 0) {
            --count_;
            return ;
        }

        waiters_.insert(self);
        sched_.block_current();
        // give() handed us the token directly.
    }

    void Semaphore::give() {
        if (Task* t = waiters_.pop_front()) {
            sched_.wake(t); // token goes to a waiting task
            // If the woken task t has a higher priority than the task currently calling give(), preemption happens
            sched_.preempt_if_needed();
        } else {
            ++count_;
        }
    }
} // namespace niyata 