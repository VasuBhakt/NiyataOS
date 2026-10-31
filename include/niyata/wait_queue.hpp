#pragma once

#include <cstdint>

#include "niyata/task.hpp"

namespace niyata {

    // Tasks blocked on something, ordered by priority (highest first)
    // FIFO among equals.
    // Intrusive: reuses Task::next, so a task can only be in a ready queue or a wait queue, never both.
    class WaitQueue {
        public:
            bool empty() const {
                return head_ == nullptr;
            }

            void insert(Task* t) {
                if (!head_ || t->priority < head_->priority) {
                    t->next = head_;
                    head_ = t;
                    return;
                }
                Task* link = head_;
                while (link->next && link->next->priority <= t->priority) link = link->next;
                t->next = link->next;
                link->next = t;
            }

            Task* pop_front() {
                Task* t = head_;
                if (t) {
                    head_ = t->next;
                    t->next = nullptr;
                }
                return t;
            }

            bool remove(Task* t) {
                if (t == head_) {
                    head_ = t->next;
                    t->next = nullptr;
                    return true;
                }
                for (Task* link = head_; link->next; link = link->next) {
                    if (link->next == t) {
                        link->next = t->next;
                        t->next = nullptr;
                        return true; 
                    }
                }
                return false;
            }

            // Priority of best waiter, or 255 if nobody waits.
            std::uint8_t top_priority() const {
                return head_ ? head_->priority : 255;
            }

        private:
            Task* head_ = nullptr;
    };
} // namespace niyata