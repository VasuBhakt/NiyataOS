#pragma once

#include <cstdint>

namespace niyata {
    // Simulated tick counter. On host, nothing advances it except explicit calls,
    // so tests control time exactly. On Cortex-M, SysTick will call advance().
    class Clock {
        public:
            using Tick = std::uint64_t;
            Tick now() const {
                return now_;
            }
            void advance() {
                now_++;
            }
        
        private:
            Tick now_ = 0;
    };
} // namespace niyata