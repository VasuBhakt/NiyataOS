#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <limits>

#include "niyata/port.hpp"

namespace niyata {

    enum class TaskState {
        Ready,
        Running,
        Blocked,
        Finished
    };

    using TaskFn = void (*)(void*);

    struct Task {
        std::uint32_t id = 0;
        const char* name = "";
        TaskState state = TaskState::Ready;
        std::uint8_t priority = 0;

        static constexpr std::uint64_t kNoWake = std::numeric_limits<std::uint64_t>::max();
        std::uint64_t wake_tick = kNoWake; // meaningful only while sleeping

        TaskFn entry = nullptr;
        void* arg = nullptr;

        std::vector<char> stack;
        port::Context* ctx = nullptr;
    };

} // namespace niyata