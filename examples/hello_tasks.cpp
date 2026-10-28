// Example tests to get users onboard with API
// Expected output : A0 B0 A1 B1 A2 B2 done

#include <cstdio>

#include "niyata/scheduler.hpp"

using namespace niyata;

struct TaskArg {
    Scheduler* sched;
    char name;
    int iters;
};

void task_fn(void* p) {
    auto* arg = static_cast<TaskArg*>(p);
    for (int i = 0; i < arg->iters; i++) {
        std::printf("%c%d ", arg->name, i);
        arg->sched->yield();
    } 
}

int main() {
    Scheduler s;
    TaskArg a{&s, 'A', 3};
    TaskArg b{&s, 'B', 3};

    s.create_task("A", task_fn, &a);
    s.create_task("B", task_fn, &b);

    s.run();
    std::printf("done\n");
    return 0;
}
