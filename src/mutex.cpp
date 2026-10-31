#include "niyata/mutex.hpp"

#include <cassert>

namespace niyata {

    void Mutex::add_held(Task* t) {
        next_held_ = t->held_head;
        t->held_head = this;
    }

    void Mutex::remove_held(Task* t) {
        Mutex** link = &t->held_head;
        while (*link && *link != this) link = &(*link)->next_held_;
        if (*link) *link = next_held_; // *link here is "this's" previous next_held_ pointer!
        next_held_ = nullptr; 
    }

    // Base priority, raised to the best waiter of any mutex the task still holds
    std::uint8_t Mutex::effective_priority(const Task* t) {
        std::uint8_t p = t->base_priority;
        for(Mutex* m = t->held_head; m; m = m->next_held_) {
            if (m->inherit_ && m->waiters_.top_priority() < p) p = m->waiters_.top_priority();
        }
        return p;
    }

    // Raise the owner of this mutex to `prio`. If that owner is itself blocked on
    // another mutex, keep going down the chain (transitive inheritance).
    void Mutex::boost_owner_chain(std::uint8_t prio) {
        Mutex* m = this;
        while (m && m->inherit_) {
            Task* owner = m->owner_;
            if (!owner || owner->priority <= prio) return; // high enough

            Mutex* next = owner->waiting_on;
            if (next) {
                // Owner is parked in another wait-queue: re-sort it there.
                next->waiters_.remove(owner);
                owner->priority = prio;
                next->waiters_.insert(owner);
            } else {
                sched_.set_priority(owner, prio);
            }
            m = next;
        }
    }

    void Mutex::lock() {
        Task* self = sched_.current();
        assert(self && "lock() must be called from a task");
        assert(owner_ != self && "mutex is not recursive");

        if (!owner_) {
            owner_ = self;
            add_held(self);
            return;
        }

        waiters_.insert(self);
        self->waiting_on = this;
        if (inherit_) boost_owner_chain(self->priority);
        sched_.block_current();
        // unlock() made us the owner before waking us.
    }

    void Mutex::unlock() {
        Task* self = sched_.current();
        assert(self && owner_ == self && "unlock() by a non-owner");

        remove_held(self);
        Task* next = waiters_.pop_front();
        if (next) {
            next->waiting_on = nullptr;
            owner_ = next; 
            add_held(next);
            next->priority = effective_priority(next); // may inherit from remaining waiters
        } else {
            owner_ = nullptr;
        }

        self->priority = effective_priority(self); // our priority may have been raised by boost_owner_chain
        if (next) sched_.wake(next);
        sched_.preempt_if_needed();
    }
} // namespace niyata