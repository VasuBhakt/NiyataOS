#include <gtest/gtest.h>

#include <string>

#include "niyata/scheduler.hpp"

using namespace niyata;

namespace {

    struct Ctx {
        Scheduler* s;
        std::string* log;
    };

} // namespace

TEST(Priority, HigherPriorityRunsFirst) {
    Scheduler s;
    std::string log;
    struct J {
        Scheduler* s;
        std::string* log;
        const char* tag;
    } low{&s, &log, "L "}, high{&s, &log, "H "};
    auto fn = [](void* p) {
        auto* j = static_cast<J*>(p);
        *j->log += j->tag;
    };
    
    s.create_task("low", fn, &low, 20);
    s.create_task("high", fn, &high, 5);
    s.run();

    EXPECT_EQ(log, "H L ");
}

TEST(Priority, YieldOnlyRotatesWithinSamePriority) {
    Scheduler s;
    std::string log;
    Ctx c{&s, &log};

    s.create_task("high", [](void* p) {
        auto* c = static_cast<Ctx*>(p);
        for (int i = 0; i < 3; i++) {
            *c->log += "H" + std::to_string(i) + " ";
            c->s->yield();
        }
    }, &c, 5);
    s.create_task("low", [](void* p) {
        *static_cast<Ctx*>(p)->log += "L "; 
    }, &c, 20);

    s.run();

    EXPECT_EQ(log, "H0 H1 H2 L ");
}

TEST(Preemption, WakingHighPriorityPreemptsOnThatExactTick) {
    Scheduler s;
    std::string log;
    Ctx c{&s, &log};

    s.create_task("low", [](void* p) {
        auto* c = static_cast<Ctx*>(p);
        for (int i = 0; i < 6; i++) {
            *c->log += "L" + std::to_string(i) + " ";
            c->s->work(1);
        }
    }, &c, 20);
    s.create_task("high", [](void* p) {
        auto* c = static_cast<Ctx*>(p);
        c->s->sleep(3);
        *c->log += "H" + std::to_string(c->s->now()) + " ";
    }, &c, 5);

    s.run();

    EXPECT_EQ(log, "L0 L1 L2 H3 L3 L4 L5 ");
}

TEST(Preemption, LowerPriorityRunsOnlyWhileHigherIsBlocked) {
    Scheduler s;
    std::string log;
    Ctx c{&s, &log};
    s.create_task("high", [](void* p) {
        auto* c = static_cast<Ctx*>(p);
        for (int i = 0; i < 3; i++) {
            *c->log += "H" + std::to_string(c->s->now()) + " ";
            c->s->sleep(2);
        }
    }, &c, 5);
    s.create_task("low", [](void* p) {
        auto* c = static_cast<Ctx*>(p);
        for (int i = 0; i < 8; i++) {
            *c->log += "L" + std::to_string(i) + " ";
            c->s->work(1);
        }
    }, &c, 20);

    s.run();

    EXPECT_EQ(log, "H0 L0 L1 H2 L2 L3 H4 L4 L5 L6 L7 ");
}

TEST(Preemption, EqualPriorityRoundRobinOnSliceExpiry) {
    Scheduler s(3);
    std::string log;
    struct J {
        Scheduler* s;
        std::string* log;
        char name;
    } a{&s, &log, 'A'}, b{&s, &log, 'B'};

    auto fn = [](void* p) {
        auto* j = static_cast<J*>(p);
        for (int i = 0; i < 4; i++) {
            *j->log += j->name + std::to_string(i) + " ";
            j->s->work(1);
        }
    };

    s.create_task("A", fn, &a, 10);
    s.create_task("B", fn, &b, 10);

    s.run();

    EXPECT_EQ(log, "A0 A1 A2 B0 B1 B2 A3 B3 ");
}

TEST(Preemption, PreemptedTaskKeepsItsTurn) {
    Scheduler s(100);
    std::string log;
    struct J {
        Scheduler* s;
        std::string* log;
        char name;
    } a{&s, &log, 'A'}, b{&s, &log, 'B'};
    Ctx c{&s, &log};

    auto fn = [](void* p) {
        auto* j = static_cast<J*>(p);
        for (int i = 0; i < 3; i++) {
            *j->log += j->name + std::to_string(i) + " ";
            j->s->work(1);
        }
    };

    s.create_task("high", [](void* p) {
        auto* c = static_cast<Ctx*>(p);
        c->s->sleep(1);
        *c->log += "H ";
    }, &c, 1);
    s.create_task("A", fn, &a, 5);
    s.create_task("B", fn, &b, 5);

    s.run();

    // A is preempted by H after A0, but resumes before B gets a turn.
    EXPECT_EQ(log, "A0 H A1 A2 B0 B1 B2 ");
}