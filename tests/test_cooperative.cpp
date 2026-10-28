#include <gtest/gtest.h>

#include <string>

#include "niyata/scheduler.hpp"

using namespace niyata;

namespace {

    struct Job {
        Scheduler* scheduler;
        std::string* log;
        char name;
        int iters;
    };

    void job(void* p) {
        auto* j = static_cast<Job*>(p);
        for (int i = 0; i < j->iters; i++) {
            *j->log += j->name + std::to_string(i) + " ";
            j->scheduler->yield();
        }
    }
} // namespace

TEST(Cooperative, ThreeTasksInterleaveRoundRobin) {
    Scheduler s;
    std::string log;
    Job a{&s, &log, 'A', 3}, b{&s, &log, 'B', 3}, c{&s, &log, 'C', 3};
    s.create_task("A", job, &a);
    s.create_task("B", job, &b);
    s.create_task("C", job, &c);

    s.run();

    EXPECT_EQ(log, "A0 B0 C0 A1 B1 C1 A2 B2 C2 ");
}

TEST(Cooperative, FinishedTaskDoesNotBreakOthers) {
    Scheduler s;
    std::string log;
    Job a{&s, &log, 'A', 1}, b{&s, &log, 'B', 3};
    Task* ta = s.create_task("A", job, &a);
    Task* tb = s.create_task("B", job, &b);

    s.run();

    EXPECT_EQ(log, "A0 B0 B1 B2 ");
    EXPECT_EQ(ta->state, TaskState::Finished);
    EXPECT_EQ(tb->state, TaskState::Finished);
}

TEST(Cooperative, RunReturnsImmediatelyWithNoTasks) {
    Scheduler s;
    s.run();
    SUCCEED();
}

TEST(Cooperative, TaskThatNeverYieldsRunsToCompletion) {
    Scheduler s;
    std::string log;
    Job a{&s, &log, 'A', 0}, b{&s, &log, 'B', 2};
    Task* ta = s.create_task("A", job, &a);
    s.create_task("B", job, &b);

    s.run();

    EXPECT_EQ(log, "B0 B1 ");
    EXPECT_EQ(ta->state, TaskState::Finished);
}    
