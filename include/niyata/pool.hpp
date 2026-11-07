#pragma once

#include <cstddef>

namespace niyata {
    // Fixed-block pool allocator over caller-provided storage.
    // alloc() and free() are O(1) with no loops, no system calls, and no fragmentation.
    //
    // Free blocks form an intrusive singly linked list threaded directly through
    // the unused blocks themselves (the FreeNode trick). The allocator needs ZERO 
    // extra memory beyond the storage buffer provided to it.
    //
    // NOTE: NOT thread-safe on its own. In an RTOS task context, wrap alloc()/free()
    // in a critical section or mutex if shared across preempting tasks.
    class Pool {
        public:
            Pool(void* storage, std::size_t block_size, std::size_t block_count);

            void* alloc();       // Returns a free block, or nullptr when exhausted (O(1))
            void free(void* p);  // Returns block back to pool (O(1)). p must come from alloc()

            std::size_t capacity() const {
                return capacity_;
            }
            std::size_t free_count() const {
                return free_count_;
            }
            std::size_t block_size() const {
                return block_size_;
            }
        
        private:
            // The Intrusive Free-List Node:
            // When a block is empty/unused, we reuse its first 8 bytes to store
            // a pointer to the next empty block. When the block is allocated,
            // the user overwrites this space with their own data.
            // Result: Zero tracking memory overhead!
            struct FreeNode {
                FreeNode* next;
            };

            unsigned char* base_;
            std::size_t block_size_;
            std::size_t capacity_;
            std::size_t free_count_;
            FreeNode* free_head_ = nullptr; // Points to the first available empty block
    };

    // StaticPool: An all-in-one pool that owns its own storage buffer inline.
    // - No heap (malloc/new) needed: memory is allocated at compile time.
    // - Template parameters:
    //     BlockSize: Size of each individual element in bytes.
    //     Count: Maximum number of blocks available in the pool.
    template <std::size_t BlockSize, std::size_t Count>
    class StaticPool {
        // Alignment:
        // CPUs require data to start on clean grid boundaries (typically 8 or 16 bytes).
        // Unaligned access degrades performance on x86 and causes hardware crashes on ARM.
        static constexpr std::size_t kAlign = alignof(std::max_align_t);

        // Alignment Math:
        // Rounds BlockSize UP to the nearest multiple of kAlign.
        // Example with kAlign = 8:
        //   BlockSize = 10 -> ((10 + 7) / 8) * 8 = (17 / 8) * 8 = 2 * 8 = 16 bytes.
        static constexpr std::size_t kBlock = ((BlockSize + kAlign - 1) / kAlign) * kAlign;
        
        public:
            StaticPool() : pool_(storage_, kBlock, Count) {}
            StaticPool(const StaticPool&) = delete;
            StaticPool& operator=(const StaticPool&) = delete;

            void* alloc() {
                return pool_.alloc();
            }
            void free(void* p) {
                pool_.free(p);
            }

            std::size_t capacity() const {
                return pool_.capacity();
            }
            std::size_t free_count() const {
                return pool_.free_count();
            }
        
        private:
            // alignas forces the buffer to start on a strict CPU-aligned memory address.
            alignas(kAlign) unsigned char storage_[kBlock * Count];
            Pool pool_;
    };
} // namespace niyata
