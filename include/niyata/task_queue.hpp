#pragma once

#include "niyata/task.hpp"

namespace niyata {

    // Intrusive FIFO of tasks. No allocation: it just threads Task::next.
    class TaskQueue {
        public:
            bool empty() const {
                return head_ == nullptr;
            }

            void push_back(Task* t) {
                t->next = nullptr;
                if (tail_) tail_->next = t;
                else head_ = t;
                tail_ = t;
            }

            void push_front(Task* t) {
                t->next = head_;
                head_ = t;
                if (!tail_) tail_ = t;
            }

            Task* pop_front() {
                Task* t = head_;
                if (!t) return nullptr;
                head_ = t->next;
                if (!head_) tail_ = nullptr;
                t->next = nullptr;
                return t;
            }

            // Unlink t if present. O(n), only used on rare priority changes.
            bool remove(Task* t) {
                Task* prev = nullptr;
                for (Task* c = head_; c; prev = c, c = c->next) {
                    if (c==t) {
                        if (prev) prev->next = c->next;
                        else head_ = c->next;
                        if (tail_ == c) tail_ = prev;
                        c->next = nullptr;
                        return true;
                    }
                }
                return false;
            }

        private:
            Task* head_ = nullptr;
            Task* tail_ = nullptr;
    };
} // namespace niyata