#include <gtest/gtest.h>

#include <string>

#include "niyata/mutex.hpp"
#include "niyata/scheduler.hpp"

using namespace niyata;

namespace {

    struct Env {
        Scheduler* s;
        std::string *log;
        Mutex* m;
    };

    // Classic three-time inversion:
    // L (low) takes the mutex and works while holding it
    // H (high) wakes at t=1 and needs the mutex
    // M (medium) wakes at t=2, never needs mutex, just burns CPU 
    // Without inheritance M starves L, so H (the most important task)
    // ends up waiting for M.
    // With inheritance L runs at H's priority and M cannot interfere.
    std::string run_scenario(bool inheritance) {
        Scheduler s;
        std::string log;
        Mutex m(s, inheritance);
        Env e{&s, &log, &m};

        s.create_task("H", [](void* p) {
            auto* e = static_cast<Env*>(p);
            e->s->sleep(1);
            *e->log += "H? ";
            e->m->lock();
            *e->log += "H+ ";
            e->m->unlock();
        }, &e, 5);
        s.create_task("M", [](void* p) {
            auto* e = static_cast<Env*>(p);
            e->s->sleep(2);
            e->s->work(3);
            *e->log += "M-done ";
        }, &e, 10);
        s.create_task("L", [](void* p) {
            auto* e = static_cast<Env*>(p);
            e->m->lock();
            *e->log += "L+ ";
            e->s->work(4);
            *e->log += "L- ";
            e->m->unlock();
        }, &e, 20);

        s.run();
        return log;
    } 
} // namespace

TEST(PriorityInversion, WithoutInheritanceMediumTaskStarvesTheHighOne) {
    // M finishes BEFORE H gets the lock, even though H is higher priority.
    EXPECT_EQ(run_scenario(false), "L+ H? M-done L- H+ ");
}

TEST(PriorityInversion, WithInheritanceHighTaskIsNotDelayedByMedium) {
    // L is boosted to H's priority, finishes its critical section, H goes next.
    EXPECT_EQ(run_scenario(true), "L+ H? L- H+ M-done ");
} 