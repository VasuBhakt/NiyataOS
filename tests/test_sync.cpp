#include <gtest/gtest.h>

#include <string>

#include "niyata/mutex.hpp"
#include "niyata/scheduler.hpp"
#include "niyata/semaphore.hpp"

using namespace niyata;

namespace {

    struct Env {
        Scheduler* s = nullptr;
        std::string* log = nullptr;
        Mutex* m1 = nullptr;
        Mutex* m2 = nullptr;
        Semaphore* sem = nullptr;
    };
} // namespace

// MUTEX

TEST(Mutex, ProvidesMutualExclusion) {
    Scheduler s(1); // slice expires constantly: tries hard to interleave
    Mutex m(s);
    int inside = 0, max_inside = 0;
    struct W {
        Scheduler* s;
        Mutex* m;
        int* inside;
        int* max_inside;
    } w{&s, &m, &inside, &max_inside};

    auto fn = [](void* p) {
        auto* w = static_cast<W*>(p);
        for (int i = 0; i < 3; i++) {
            w->m->lock();
            (*w->inside)++;
            if (*w->inside > *w->max_inside) {
                *w->max_inside = *w->inside;
            }
            w->s->work(2);
            (*w->inside)--;
            w->m->unlock();
            w->s->work(1);
        }
    };

    Task* a = s.create_task("A", fn, &w, 10);
    Task* b = s.create_task("B", fn, &w, 10);

    s.run();

    EXPECT_EQ(max_inside, 1);
    EXPECT_EQ(a->state, TaskState::Finished);
    EXPECT_EQ(b->state, TaskState::Finished);
}

TEST(Mutex, HandsOffToHighestPriorityWaiter) {
    Scheduler s;
    std::string log;
    Mutex m(s);
    Env e{&s, &log, &m};

    // Owner grabs the lock and sleeps on it.
    s.create_task("owner", [](void* p) {
        auto* e = static_cast<Env*>(p);
        e->m1->lock();
        e->s->sleep(5);
        e->m1->unlock();
    }, &e, 25);

    // Three waiters;
    struct Waiter {
        Env* e;
        const char* tag;
    } w15{&e, "15 "}, w10{&e, "10 "}, w5{&e, "5 "};

    auto waiter = [](void* p) {
        auto* w = static_cast<Waiter*>(p);
        w->e->s->sleep(1);
        w->e->m1->lock();
        *w->e->log += w->tag;
        w->e->m1->unlock();
    };
    s.create_task("w15", waiter, &w15, 15);
    s.create_task("w5", waiter, &w5, 5);
    s.create_task("w10", waiter, &w10, 10);

    s.run();

    while(!s.all_finished()) {
        s.tick();
        s.run();
    }

    EXPECT_EQ(log, "5 10 15 ");
}

TEST(Mutex, RestoresBasePriorityIncludingNestedLocks) {
    Scheduler s;
    std::string log;
    Mutex m1(s), m2(s);
    Env e{&s, &log, &m1, &m2};
    static int rec_after_m1, rec_mid, rec_after_m2;
    rec_after_m1 = rec_mid = rec_after_m2 = -1;

    s.create_task("L", [](void* p) {
        auto* e = static_cast<Env*>(p);
        Task* self = e->s->current();
        e->m1->lock();
        e->m2->lock();
        e->s->sleep(3);
        rec_mid = self->priority; // boosted by H1(5) via m1 and H2(10) via m2
        e->m1->unlock(); // still holds m2, whose waiter is H2(10)
        rec_after_m1 = self->priority;
        e->m2->unlock();
        rec_after_m2 = self->priority;
    }, &e, 20);
    s.create_task("H1", [](void* p){
        auto* e = static_cast<Env*>(p);
        e->s->sleep(1);
        e->m1->lock();
        e->m1->unlock();
    }, &e, 5);
    s.create_task("H2", [](void* p){
        auto* e = static_cast<Env*>(p);
        e->s->sleep(1);
        e->m2->lock();
        e->m2->unlock();
    }, &e, 10);

    s.run();
    while(!s.all_finished()) {
        s.tick();
        s.run();
    }

    EXPECT_EQ(rec_mid, 5);
    EXPECT_EQ(rec_after_m1, 10);
    EXPECT_EQ(rec_after_m2, 20);
}

TEST(Mutex, InheritanceIsTransitiveAlongTheChain) {
    Scheduler s;
    std::string log;
    Mutex m1(s), m2(s);
    Env e{&s, &log, &m1, &m2};
    static Task *tl, *tm;
    static int seen_l, seen_m;
    seen_l = seen_m = -1;

    // L(20) owns m1. M(10) owns m2 then blocks on m1. H(5) blocks on m2.
    tl = s.create_task("L", [](void* p){
        auto* e = static_cast<Env*>(p);
        e->m1->lock();
        e->s->sleep(10);
        e->m1->unlock();
    }, &e, 20);

    tm = s.create_task("M", [](void* p){
        auto* e = static_cast<Env*>(p);
        e->s->sleep(1);
        e->m2->lock();
        e->m1->lock();
        e->m1->unlock();
        e->m2->unlock();
    }, &e, 10);

    s.create_task("H", [](void* p) {
        auto* e = static_cast<Env*>(p);
        e->s->sleep(2);
        e->m2->lock();
        e->m2->unlock();
    }, &e, 5);

    s.create_task("observer", [](void* p) {
        auto* e = static_cast<Env*>(p);
        e->s->sleep(3);
        seen_l = tl->priority;
        seen_m = tm->priority;
    }, &e, 1);

    s.run();
    while (!s.all_finished()) {
        s.tick();
        s.run();
    }

    EXPECT_EQ(seen_m, 5);
    EXPECT_EQ(seen_l, 5);
}

// SEMAPHORE

TEST(Semaphore, GiveWakesWaiterAndItPreemptsTheGiver) {
    Scheduler s;
    std::string log;
    Semaphore sem(s, 0);
    Env e{&s, &log, nullptr, nullptr, &sem};

    s.create_task("consumer", [](void* p) {
        auto* e = static_cast<Env*>(p);
        *e->log += "C-wait ";
        e->sem->take();
        *e->log += "C-got ";
    }, &e, 5);
    s.create_task("producer", [](void* p) {
        auto* e = static_cast<Env*>(p);
        e->s->sleep(2);
        *e->log += "P-give ";
        e->sem->give();
        *e->log += "P-after ";
    }, &e, 10);

    s.run();
    while (!s.all_finished()) {
        s.tick();
        s.run();
    }

    EXPECT_EQ(log, "C-wait P-give C-got P-after ");
}

TEST(Semaphore, InitialCountLetsTakesThroughWithoutBlocking) {
    Scheduler s;
    std::string log;
    Semaphore sem(s, 2);
    Env e{&s, &log, nullptr, nullptr, &sem};

    s.create_task("T", [](void* p) {
        auto* e = static_cast<Env*>(p);
        e->sem->take();
        *e->log += "1 ";
        e->sem->take();
        *e->log += "2 ";
        e->sem->take(); // count is 0; blocks here
        *e->log += "3 ";
    }, &e, 10);

    s.run();
    EXPECT_EQ(log, "1 2 ");
    EXPECT_EQ(s.all_finished(), false);

    sem.give(); // from any outside task
    s.run();
    EXPECT_EQ(log, "1 2 3 ");
    EXPECT_TRUE(s.all_finished());
}

TEST(Semaphore, WakesWaitersInPriorityOrder) {
    Scheduler s;
    std::string log;
    Semaphore sem(s, 0);
    Env e{&s, &log, nullptr, nullptr, &sem};

    struct Waiter {
        Env* e;
        const char* tag;
    } w15{&e, "15 "}, w5{&e, "5 "}, w10{&e, "10 "};

    auto waiter = [](void* p) {
        auto* w = static_cast<Waiter*>(p);
        w->e->sem->take();
        *w->e->log += w->tag;
    };

    s.create_task("w15", waiter, &w15, 15);
    s.create_task("w5", waiter, &w5, 5);
    s.create_task("w10", waiter, &w10, 10);

    s.run(); // all three block
    sem.give();
    sem.give();
    sem.give();
    s.run();

    EXPECT_EQ(log, "5 10 15 ");
}