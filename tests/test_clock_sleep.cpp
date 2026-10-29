#include <gtest/gtest.h>

#include <string>

#include "niyata/scheduler.hpp"

using namespace niyata;

namespace {

    struct Napper {
        Scheduler* sched;
        std::string* log;
        char name;
        Clock::Tick nap;
    };

    void napper(void* p) {
        auto* n = static_cast<Napper*>(p);
        Clock::Tick before = n->sched->now();
        n->sched->sleep(n->nap);
        *n->log += n->name + std::to_string(n->sched->now() - before) + " ";
    }

    struct Counter {
        Scheduler* sched;
        std::string* log;
        int iters;
    };

    void counter(void* p) {
        auto* c = static_cast<Counter*>(p);
        for (int i = 0; i < c->iters; i++) {
            *c->log += "B" + std::to_string(i) + " ";
            c->sched->yield();
        }
    }

    // Drive time until every task is done (with a guard against hangs).
    void drive(Scheduler& s) {
        s.run();
        for (int guard = 0; !s.all_finished() && guard < 1000; guard++) {
            s.tick();
            s.run();
        }
        ASSERT_TRUE(s.all_finished());
    }

} // namespace

TEST(Sleep, ResumesAtExactlyNowPlusN) {
    Scheduler s;
    std::string log;
    Napper a{&s, &log, 'A', 3};
    s.create_task("A", napper, &a);

    s.run();
    EXPECT_EQ(log, ""); // asleep
    s.tick();
    s.run();
    s.tick();
    s.run();
    EXPECT_EQ(log, ""); // 2 ticks; still asleep, not early
    s.tick();
    s.run();
    EXPECT_EQ(log, "A3 "); // woke up
}

TEST(Sleep, SleepersWakeInWakeTickOrder) {
    Scheduler s;
    std::string log;
    Napper a{&s, &log, 'A', 3}, b{&s, &log, 'B', 1}, c{&s, &log, 'C', 2};
    s.create_task("A", napper, &a);
    s.create_task("B", napper, &b);
    s.create_task("C", napper, &c);

    drive(s);

    EXPECT_EQ(log, "B1 C2 A3 ");
}

TEST(Sleep, SleepingTaskDoesNotBlockOthers) {
    Scheduler s;
    std::string log;
    Napper a{&s, &log, 'A', 5};
    Counter b{&s, &log, 3};

    s.create_task("A", napper, &a);
    s.create_task("B", counter, &b);

    s.run(); // no ticks yet: B finishes all its work while A sleeps

    EXPECT_EQ(log, "B0 B1 B2 ");
    drive(s);
    EXPECT_EQ(log, "B0 B1 B2 A5 ");
}

TEST(Sleep, SleepZeroBehavesLikeYield) {
    Scheduler s;
    std::string log;
    Napper a{&s, &log, 'A', 0};
    Counter b{&s, &log, 2};
    s.create_task("A", napper, &a);
    s.create_task("B", counter, &b);

    s.run();

    // A yields at sleep(0), B runs one step, A finishes, B finishes.
    EXPECT_EQ(log, "B0 A0 B1 ");
    EXPECT_TRUE(s.all_finished());
}

TEST(Sleep, RunReturnsWhenEveryTaskIsAsleep) {
    Scheduler s;
    std::string log;
    Napper a{&s, &log, 'A', 2}, b{&s, &log, 'B', 4};
    Task* ta = s.create_task("A", napper, &a);
    Task* tb = s.create_task("B", napper, &b);

    s.run(); // must return, not hang

    EXPECT_EQ(ta->state, TaskState::Blocked);
    EXPECT_EQ(tb->state, TaskState::Blocked);
    EXPECT_FALSE(s.all_finished());
}