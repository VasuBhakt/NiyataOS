#include "niyata/scheduler.hpp"

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

    Scheduler::Scheduler() : sched_ctx_(port::context_create_main()) {}

    // Clean up all task contexts first, then the dispatcher's own context.
    Scheduler::~Scheduler() {
        for (auto& t: tasks_) port::context_destroy(t->ctx);
        port::context_destroy(sched_ctx_);
    }

    Task* Scheduler::create_task(const char* name, TaskFn fn, void* arg, std::size_t stack_size) {
        auto t = std::make_unique<Task>();
        t->id = static_cast<std::uint32_t>(tasks_.size());
        t->name = name;
        t->entry = fn;
        t->arg = arg;
        t->stack.resize(stack_size);
        // Configure context:
        // - Stack: uses t->stack buffer
        // - Entry point: task_trampoline
        // - on_exit: sched_ctx_ (if the task exits, return cleanly to dispatcher)
        t->ctx = port::context_create(t->stack.data(), t->stack.size(), task_trampoline, t.get(), sched_ctx_);

        Task* raw = t.get();
        tasks_.push_back(std::move(t));
        return raw;
    }

    Task* Scheduler::pick_next() {
        // Cooperative Round-Robin:
        // Scan circularly from cursor_ to give each ready task a fair turn.
        const std::size_t n = tasks_.size();
        for (std::size_t i = 0; i < n; i++) {
            std::size_t idx = (cursor_ + i) % n;
            if (tasks_[idx]->state == TaskState::Ready) {
                cursor_ = idx + 1;
                return tasks_[idx].get();
            }
        }
        return nullptr; // No ready tasks left (all Finished or Blocked)
    }

    void Scheduler::run() {
         // Main dispatcher loop: keeps running as long as any task is Ready.
        while (Task* next = pick_next()) {
            current_ = next;
            next->state = TaskState::Running;
            // Save dispatcher state into sched_ctx_ and jump to the task
            port::context_switch(sched_ctx_, next->ctx);
            // Control returns here either because:
            // 1. The task called yield(): state is still Running -> set to Ready so it runs again.
            // 2. The task finished: state is already Finished -> leave it alone so it's not picked again.
            if (next->state == TaskState::Running) next->state = TaskState::Ready;
            current_ = nullptr;
        }
    }

    void Scheduler::yield() {
        // Cooperative yield: voluntarily give up the remainder of our turn.
        // Freezes the running task here and switches CPU control back to the dispatcher loop.
        Task* self = current_;
        port::context_switch(self->ctx, sched_ctx_);
        // When this task is scheduled again later, execution resumes right here!
    }

} // namespace niyata