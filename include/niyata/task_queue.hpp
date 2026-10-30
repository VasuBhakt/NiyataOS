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

        private:
            Task* head_ = nullptr;
            Task* tail_ = nullptr;
    };
} // namespace niyata