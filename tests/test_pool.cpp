#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>
#include <set>
#include <vector>

#include "niyata/pool.hpp"

using namespace niyata;

TEST(Pool, StartsFull) {
    StaticPool<32, 8> p;
    EXPECT_EQ(p.capacity(), 8u);
    EXPECT_EQ(p.free_count(), 8u);
}

TEST(Pool, ReturnsNullWhenExhausted) {
    StaticPool<32, 4> p;
    for (int i = 0; i < 4; i++) {
        EXPECT_NE(p.alloc(), nullptr);
    }
    EXPECT_EQ(p.free_count(), 0u);
    EXPECT_EQ(p.alloc(), nullptr);
}

TEST(Pool, BlockAreDistinctAlignedAndNonOverlapping) {
    StaticPool<22, 16> p; // 22 gets rounded up to max alignment
    // if 8: rounded to 24
    // if 16: rounded to 32
    // hence minimum heck till 24 
    std::set<void*> seen;
    std::vector<unsigned char*> blocks;
    for (int i = 0; i < 16; i++) {
        auto* b = static_cast<unsigned char*>(p.alloc());
        ASSERT_NE(b, nullptr);
        EXPECT_EQ((reinterpret_cast<std::uintptr_t>(b) % alignof(std::max_align_t)), 0u);
        EXPECT_TRUE(seen.insert(b).second); // never handed out twice
        std::memset(b, i+1, 24); // fill the whole requested size
        blocks.push_back(b);
    }
    // No block was clobbered by a neighbour's memset.
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 24; j++) {
            EXPECT_EQ(blocks[i][j], i+1);
        }
    }
}

TEST(Pool, FreedBlockIsReusedFirst) {
    StaticPool<32, 4> p;
    void* a = p.alloc();
    void* b = p.alloc();
    p.free(a);
    EXPECT_EQ(p.alloc(), a); // LIFO: hot block comes back first
    p.free(b);
    EXPECT_EQ(p.alloc(), b);
}

TEST(Pool, NoFragmentationAfterOutOfOrderFrees) {
    constexpr int N = 16;
    StaticPool<32, N> p;
    void* ptrs[N];
    for (int i = 0; i < N; i++) {
        ptrs[i] = p.alloc();
    }
    for (int i = 0; i < N; i++) {
        p.free(ptrs[(i * 7) % N]); // scrambled order
    }
    EXPECT_EQ(p.free_count(), static_cast<std::size_t>(N));

    std::set<void*> again;
    for (int i = 0; i < N; i++) {
        void* b = p.alloc();
        ASSERT_NE(b, nullptr); // every block is usable again
        again.insert(b); 
    }
    EXPECT_EQ(again.size(), static_cast<std::size_t>(N));
    EXPECT_EQ(p.alloc(), nullptr);
}