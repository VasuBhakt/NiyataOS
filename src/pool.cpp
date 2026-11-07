#include "niyata/pool.hpp"

#include <cassert>
#include <cstdint>

namespace niyata {
    Pool::Pool(void* storage, std::size_t block_size, std::size_t block_count)
        : base_(static_cast<unsigned char*>(storage)),
        block_size_(block_size),
        capacity_(block_count),
        free_count_(block_count) {
            assert(storage != nullptr);
            assert(block_size >= sizeof(FreeNode));
            assert(block_size % alignof(FreeNode) == 0);
            assert(reinterpret_cast<std::uintptr_t>(storage) % alignof(FreeNode) == 0);

            // Thread every block onto the free list, lowest address first out.
            for (std::size_t i = block_count; i-- > 0;) {
                auto* node = reinterpret_cast<FreeNode*>(base_ + i * block_size_);
                node->next = free_head_;
                free_head_ = node;
            }
        }
    
    void* Pool::alloc() {
        FreeNode* node = free_head_;
        if (!node) return nullptr;
        free_head_ = node->next;
        --free_count_;
        return node;
    }

    void Pool::free(void* p) {
        [[maybe_unused]] auto* bp = static_cast<unsigned char*>(p);
        assert((bp >= base_) && (bp < base_ + (capacity_ * block_size_)) && "not from this pool");
        assert(((bp - base_) % block_size_ == 0) && "pointer is not a block boundary");
        assert((free_count_ < capacity_) && "more frees than allocs");

        auto* node = reinterpret_cast<FreeNode*>(p);
        node->next = free_head_;
        free_head_ = node;
        ++free_count_;
    }
} // namespace niyata