#include "niyata/scheduler.hpp"

#include <cassert>

namespace niyata {

    namespace {
        // Every task starts execution here.
        // 1. Calls the user function t->entry(t->arg).
        // 2. If the user function yields, execution freezes inside yield() and 
        //    NEVER reaches end of trampoline.
        void task_trampoline(void* p) {
            auto* t = static_cast<Task*>(p);
            t->entry(t->arg);
            // 3. Only when the task function genuinely returns does it reach ,
            //    marking itself Finished. Returning from this function automatically 
            //    triggers uc_link (sched_ctx_), safely returning control to the dispatcher.
            t->state = TaskState::Finished;
        }
    } // namespace

    Scheduler::Scheduler(Clock::Tick time_slice) : sched_ctx_(port::context_create_main()), time_slice_(time_slice) {}

    // Clean up all task contexts first, then the dispatcher's own context.
    Scheduler::~Scheduler() {
        for (auto& t: tasks_) port::context_destroy(t->ctx);
        port::context_destroy(sched_ctx_);
    }

    Task* Scheduler::create_task(const char* name, TaskFn fn, void* arg, std::uint8_t priority, std::size_t stack_size) {
        assert(priority < kNumPriorities);

        auto t = std::make_unique<Task>();
        t->id = static_cast<std::uint32_t>(tasks_.size());
        t->name = name;
        t->entry = fn;
        t->arg = arg;
        t->stack.resize(stack_size);
        t->priority = priority;
        // Configure context:
        // - Stack: uses t->stack buffer
        // - Entry point: task_trampoline
        // - on_exit: sched_ctx_ (if the task exits, return cleanly to dispatcher)
        t->ctx = port::context_create(t->stack.data(), t->stack.size(), task_trampoline, t.get(), sched_ctx_);

        Task* raw = t.get();
        tasks_.push_back(std::move(t));
        make_ready(raw);
        return raw;
    }

    void Scheduler::make_ready(Task* t, bool front) {
        t->state = TaskState::Ready;
        if (front) ready_[t->priority].push_front(t);
        else ready_[t->priority].push_back(t);
        ready_bitmap_ |= (1u << t->priority);
    }

    int Scheduler::highest_ready() const {
        if (ready_bitmap_ == 0) return -1;
        return __builtin_ctz(ready_bitmap_); // lowest sset bit = highest priority
    }

    Task* Scheduler::pick_next() {
        int p = highest_ready();
        if (p < 0) return nullptr;
        Task* t = ready_[p].pop_front();
        if (ready_[p].empty()) ready_bitmap_ &= ~(1u << p);
        return t;
    }

    void Scheduler::run() {
         // Main dispatcher loop: keeps running as long as any task is Ready.
        while (Task* next = pick_next()) {
            current_ = next;
            next->state = TaskState::Running;
            slice_left_ = time_slice_;
            // Save dispatcher state into sched_ctx_ and jump to the task
            port::context_switch(sched_ctx_, next->ctx);
            // Back in the dispatcher: the task yielded, slept, was preempted,
            // or finished. It already put itself wherever it belongs.
            current_ = nullptr;
        }
    }

    void Scheduler::yield() {
        Task* self = current_;
        make_ready(self); // back of its own priority queue
        port::context_switch(self->ctx, sched_ctx_);
        // When this task is scheduled again later, execution resumes right here!
    }

    void Scheduler::sleep(Clock::Tick ticks) {
        if (ticks == 0) {
            yield();
            return;
        }

        Task* self = current_;
        self->wake_tick = clock_.now() + ticks;
        self->state = TaskState::Blocked;
        port::context_switch(self->ctx, sched_ctx_);
    }

    void Scheduler::tick() {
        clock_.advance();
        for (auto& t: tasks_) {
            if (t->state == TaskState::Blocked && t->wake_tick <= clock_.now()) {
                t->wake_tick = Task::kNoWake;
                make_ready(t.get());
            }
        }
        if (current_ && slice_left_ > 0) --slice_left_;
    }

    // Called after every tick while a task is running (the "return from timer
    // interrupt" point). Decides whether the running task must give up the CPU.
    void Scheduler::preempt_if_needed() {
        Task* self = current_;
        if (!self) return ;

        int top = highest_ready();
        if (top >= 0 && top < static_cast<int>(self->priority)) {
            // Strictly higher priority is ready: preempt.
            // We did not use up our turn, so go to the FRONT of our queue.
            make_ready(self, true);
            port::context_switch(self->ctx, sched_ctx_);
        } else if (slice_left_ == 0) {
            if (top == static_cast<int>(self->priority)) {
                make_ready(self);
                port::context_switch(self->ctx, sched_ctx_);
            } else {
                slice_left_ = time_slice_; // only job is top priority, no other higher or equal priority job
            }
        }
    }

    void Scheduler::work(Clock::Tick ticks) {
        for (Clock::Tick i = 0; i < ticks; i++) {
            tick();
            preempt_if_needed();
        }
    }

    bool Scheduler::all_finished() const {
        for (const auto& t: tasks_) {
            if (t->state != TaskState::Finished) return false;
        }
        return true;
    }

} // namespace niyata