// Priority inversion, with and without priority inheritance.
// L (low) holds a mutex; H (high) wants it; M (medium) just burns CPU.

#include <cstdio>

#include "niyata/mutex.hpp"
#include "niyata/scheduler.hpp"

using namespace niyata;

struct Env {
    Scheduler* s;
    Mutex* m;
};

static void say(Env* e, const char* who, const char* what) {
    std::printf(" t=%llu %-1s %s\n", static_cast<unsigned long long>(e->s->now()), who, what);
}

static void run(bool inheritance) {
    std::printf("\n== priority inheritance %s ==\n", inheritance ? "ON" : "OFF");
    Scheduler s;
    Mutex m(s,inheritance);
    Env e{&s, &m};

    s.create_task("H", [](void* p) {
        auto* e = static_cast<Env*>(p);
        e->s->sleep(1);
        say(e, "H", "WANTS the lock");
        e->m->lock();
        say(e, "H", "GOT the lock");
        e->m->unlock();
    }, &e, 5);
    s.create_task("M", [](void* p) {
        auto* e = static_cast<Env*>(p);
        e->s->sleep(2);
        say(e, "M", "starts burning CPU");
        e->s->work(3);
        say(e, "M", "done");
    }, &e, 10);
    s.create_task("L", [](void* p) {
        auto* e = static_cast<Env*>(p);
        e->m->lock();
        say(e, "L", "TOOK the lock");
        e->s->work(4);
        say(e, "L", "RELEASING the lock");
        e->m->unlock();
    }, &e, 20);

    s.run();
}

int main() {
    run(true);
    run(false);
}