/**
 * @file test_shared_ring_buffer.cpp
 * @brief Unit tests for SharedRingBuffer
 * @brief SharedRingBuffer 的单元测试
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <gtest/gtest.h>

#include <atomic>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#include <oneplog/shared_ring_buffer.hpp>
#include <oneplog/log_entry.hpp>

namespace oneplog {
namespace test {

// ==============================================================================
// Helper class for managing test memory / 测试内存管理辅助类
// ==============================================================================

class TestMemory {
public:
    explicit TestMemory(size_t size) : m_size(size) {
        m_memory = std::aligned_alloc(kCacheLineSize, size);
        if (m_memory) {
            std::memset(m_memory, 0, size);
        }
    }
    
    ~TestMemory() {
        if (m_memory) {
            std::free(m_memory);
        }
    }
    
    void* Get() const { return m_memory; }
    size_t Size() const { return m_size; }
    
private:
    void* m_memory{nullptr};
    size_t m_size{0};
};

// ==============================================================================
// Unit Tests / 单元测试
// ==============================================================================

/**
 * @brief Test CalculateRequiredSize
 * @brief 测试 CalculateRequiredSize
 */
TEST(SharedRingBufferTest, CalculateRequiredSize) {
    size_t size4 = SharedRingBuffer<int>::CalculateRequiredSize(4);
    size_t size16 = SharedRingBuffer<int>::CalculateRequiredSize(16);
    
    EXPECT_GT(size4, 0);
    EXPECT_GT(size16, size4);
    
    // Size should include header, slot status array, and buffer
    // 大小应该包括头部、槽位状态数组和缓冲区
    size_t minSize = sizeof(RingBufferHeader) + sizeof(SlotStatus) * 4 + sizeof(int) * 4;
    EXPECT_GE(size4, minSize);
}

/**
 * @brief Test Create and basic operations
 * @brief 测试 Create 和基本操作
 */
TEST(SharedRingBufferTest, CreateAndBasicOps) {
    constexpr size_t capacity = 16;
    size_t memSize = SharedRingBuffer<int>::CalculateRequiredSize(capacity);
    TestMemory mem(memSize);
    
    auto* buffer = SharedRingBuffer<int>::Create(mem.Get(), mem.Size(), capacity);
    ASSERT_NE(buffer, nullptr);
    
    EXPECT_EQ(buffer->Capacity(), capacity);
    EXPECT_TRUE(buffer->IsEmpty());
    EXPECT_FALSE(buffer->IsFull());
    EXPECT_EQ(buffer->Size(), 0);
    EXPECT_TRUE(buffer->IsOwner());
    
    delete buffer;
}

/**
 * @brief Test Create with invalid parameters
 * @brief 测试使用无效参数的 Create
 */
TEST(SharedRingBufferTest, CreateInvalidParams) {
    // Null memory
    auto* buffer1 = SharedRingBuffer<int>::Create(nullptr, 1024, 16);
    EXPECT_EQ(buffer1, nullptr);
    
    // Zero capacity
    TestMemory mem(1024);
    auto* buffer2 = SharedRingBuffer<int>::Create(mem.Get(), mem.Size(), 0);
    EXPECT_EQ(buffer2, nullptr);
    
    // Insufficient memory
    TestMemory smallMem(64);
    auto* buffer3 = SharedRingBuffer<int>::Create(smallMem.Get(), smallMem.Size(), 1024);
    EXPECT_EQ(buffer3, nullptr);
}

/**
 * @brief Test basic push and pop
 * @brief 测试基本的推入和弹出
 */
TEST(SharedRingBufferTest, BasicPushPop) {
    constexpr size_t capacity = 16;
    size_t memSize = SharedRingBuffer<int>::CalculateRequiredSize(capacity);
    TestMemory mem(memSize);
    
    auto* buffer = SharedRingBuffer<int>::Create(mem.Get(), mem.Size(), capacity);
    ASSERT_NE(buffer, nullptr);
    
    // Push
    EXPECT_TRUE(buffer->TryPush(42));
    EXPECT_FALSE(buffer->IsEmpty());
    EXPECT_EQ(buffer->Size(), 1);
    
    // Pop
    int value = 0;
    EXPECT_TRUE(buffer->TryPop(value));
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(buffer->IsEmpty());
    EXPECT_EQ(buffer->Size(), 0);
    
    delete buffer;
}

/**
 * @brief Test multiple push and pop
 * @brief 测试多次推入和弹出
 */
TEST(SharedRingBufferTest, MultiplePushPop) {
    constexpr size_t capacity = 16;
    size_t memSize = SharedRingBuffer<int>::CalculateRequiredSize(capacity);
    TestMemory mem(memSize);
    
    auto* buffer = SharedRingBuffer<int>::Create(mem.Get(), mem.Size(), capacity);
    ASSERT_NE(buffer, nullptr);
    
    // Push multiple items
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(buffer->TryPush(i));
    }
    EXPECT_EQ(buffer->Size(), 10);
    
    // Pop and verify order (FIFO)
    for (int i = 0; i < 10; ++i) {
        int value = -1;
        EXPECT_TRUE(buffer->TryPop(value));
        EXPECT_EQ(value, i);
    }
    EXPECT_TRUE(buffer->IsEmpty());
    
    delete buffer;
}

/**
 * @brief Test queue full with DropNewest policy
 * @brief 测试队列满（DropNewest 策略）
 */
TEST(SharedRingBufferTest, QueueFullDropNewest) {
    constexpr size_t capacity = 4;
    size_t memSize = SharedRingBuffer<int>::CalculateRequiredSize(capacity);
    TestMemory mem(memSize);
    
    auto* buffer = SharedRingBuffer<int>::Create(mem.Get(), mem.Size(), capacity, 
                                                  QueueFullPolicy::DropNewest);
    ASSERT_NE(buffer, nullptr);
    
    // Fill the queue
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(buffer->TryPush(i));
    }
    EXPECT_TRUE(buffer->IsFull());
    
    // Try to push when full - should fail
    EXPECT_FALSE(buffer->TryPush(100));
    EXPECT_EQ(buffer->GetDroppedCount(), 1);
    
    delete buffer;
}

/**
 * @brief Test queue full with DropOldest policy
 * @brief 测试队列满（DropOldest 策略）
 */
TEST(SharedRingBufferTest, QueueFullDropOldest) {
    constexpr size_t capacity = 4;
    size_t memSize = SharedRingBuffer<int>::CalculateRequiredSize(capacity);
    TestMemory mem(memSize);
    
    auto* buffer = SharedRingBuffer<int>::Create(mem.Get(), mem.Size(), capacity,
                                                  QueueFullPolicy::DropOldest);
    ASSERT_NE(buffer, nullptr);
    
    // Fill the queue with 0, 1, 2, 3
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(buffer->TryPush(i));
    }
    EXPECT_TRUE(buffer->IsFull());
    
    // Push when full - should drop oldest and succeed
    EXPECT_TRUE(buffer->TryPush(100));
    EXPECT_EQ(buffer->GetDroppedCount(), 1);
    
    // Verify oldest was dropped (0 should be gone)
    std::vector<int> values;
    int value;
    while (buffer->TryPop(value)) {
        values.push_back(value);
    }
    
    EXPECT_EQ(values.size(), 4);
    EXPECT_EQ(values[0], 1);  // 0 was dropped
    EXPECT_EQ(values[3], 100);
    
    delete buffer;
}

/**
 * @brief Test Connect to existing buffer
 * @brief 测试连接到已存在的缓冲区
 */
TEST(SharedRingBufferTest, Connect) {
    constexpr size_t capacity = 16;
    size_t memSize = SharedRingBuffer<int>::CalculateRequiredSize(capacity);
    TestMemory mem(memSize);
    
    // Create buffer
    auto* owner = SharedRingBuffer<int>::Create(mem.Get(), mem.Size(), capacity);
    ASSERT_NE(owner, nullptr);
    EXPECT_TRUE(owner->IsOwner());
    
    // Push some data
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(owner->TryPush(i));
    }
    
    // Connect to the same memory
    auto* client = SharedRingBuffer<int>::Connect(mem.Get(), mem.Size());
    ASSERT_NE(client, nullptr);
    EXPECT_FALSE(client->IsOwner());
    
    // Client should see the same data
    EXPECT_EQ(client->Capacity(), capacity);
    EXPECT_EQ(client->Size(), 5);
    
    // Pop from client
    int value;
    EXPECT_TRUE(client->TryPop(value));
    EXPECT_EQ(value, 0);
    
    // Owner should see the change
    EXPECT_EQ(owner->Size(), 4);
    
    delete client;
    delete owner;
}

/**
 * @brief Test Connect with invalid memory
 * @brief 测试使用无效内存的 Connect
 */
TEST(SharedRingBufferTest, ConnectInvalid) {
    // Null memory
    auto* buffer1 = SharedRingBuffer<int>::Connect(nullptr, 1024);
    EXPECT_EQ(buffer1, nullptr);
    
    // Uninitialized memory (invalid magic)
    TestMemory mem(1024);
    auto* buffer2 = SharedRingBuffer<int>::Connect(mem.Get(), mem.Size());
    EXPECT_EQ(buffer2, nullptr);
}

/**
 * @brief Test WFC support
 * @brief 测试 WFC 支持
 */
TEST(SharedRingBufferTest, WFCSupport) {
    constexpr size_t capacity = 16;
    size_t memSize = SharedRingBuffer<int>::CalculateRequiredSize(capacity);
    TestMemory mem(memSize);
    
    auto* buffer = SharedRingBuffer<int>::Create(mem.Get(), mem.Size(), capacity);
    ASSERT_NE(buffer, nullptr);
    
    // Push with WFC
    int64_t slot = buffer->TryPushWFC(42);
    EXPECT_NE(slot, SharedRingBuffer<int>::kInvalidSlot);
    
    // Check WFC state
    EXPECT_EQ(buffer->GetWFCState(slot), kWFCEnabled);
    
    // Pop (this should complete WFC)
    int value = 0;
    EXPECT_TRUE(buffer->TryPop(value));
    EXPECT_EQ(value, 42);
    
    // WFC should be completed
    EXPECT_EQ(buffer->GetWFCState(slot), kWFCCompleted);
    
    delete buffer;
}

/**
 * @brief Test WFC never dropped
 * @brief 测试 WFC 永不丢弃
 */
TEST(SharedRingBufferTest, WFCNeverDropped) {
    constexpr size_t capacity = 4;
    size_t memSize = SharedRingBuffer<int>::CalculateRequiredSize(capacity);
    TestMemory mem(memSize);
    
    auto* buffer = SharedRingBuffer<int>::Create(mem.Get(), mem.Size(), capacity,
                                                  QueueFullPolicy::DropNewest);
    ASSERT_NE(buffer, nullptr);
    
    // Fill the queue
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(buffer->TryPush(i));
    }
    EXPECT_TRUE(buffer->IsFull());
    
    std::atomic<bool> wfcCompleted{false};
    std::atomic<int64_t> wfcSlot{-1};
    
    // Start a thread that will push WFC (should block, not drop)
    std::thread producer([&]() {
        int64_t slot = buffer->TryPushWFC(100);
        wfcSlot.store(slot, std::memory_order_release);
        wfcCompleted.store(true, std::memory_order_release);
    });
    
    // Give producer time to start blocking
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_FALSE(wfcCompleted.load(std::memory_order_acquire));
    
    // Pop one item to unblock producer
    int value;
    EXPECT_TRUE(buffer->TryPop(value));
    
    // Wait for producer to complete
    producer.join();
    EXPECT_TRUE(wfcCompleted.load(std::memory_order_acquire));
    
    // WFC slot should be valid
    EXPECT_NE(wfcSlot.load(std::memory_order_acquire), SharedRingBuffer<int>::kInvalidSlot);
    
    // No logs should be dropped (WFC never dropped)
    EXPECT_EQ(buffer->GetDroppedCount(), 0);
    
    delete buffer;
}

/**
 * @brief Test TryPopBatch
 * @brief 测试 TryPopBatch
 */
TEST(SharedRingBufferTest, TryPopBatch) {
    constexpr size_t capacity = 16;
    size_t memSize = SharedRingBuffer<int>::CalculateRequiredSize(capacity);
    TestMemory mem(memSize);
    
    auto* buffer = SharedRingBuffer<int>::Create(mem.Get(), mem.Size(), capacity);
    ASSERT_NE(buffer, nullptr);
    
    // Push multiple items
    for (int i = 0; i < 10; ++i) {
        buffer->TryPush(i);
    }
    
    // Pop batch
    std::vector<int> items;
    size_t count = buffer->TryPopBatch(items, 5);
    EXPECT_EQ(count, 5);
    EXPECT_EQ(items.size(), 5);
    
    // Verify order
    for (size_t i = 0; i < items.size(); ++i) {
        EXPECT_EQ(items[i], static_cast<int>(i));
    }
    
    // Remaining items
    EXPECT_EQ(buffer->Size(), 5);
    
    delete buffer;
}

/**
 * @brief Test wrap-around behavior
 * @brief 测试环绕行为
 */
TEST(SharedRingBufferTest, WrapAround) {
    constexpr size_t capacity = 4;
    size_t memSize = SharedRingBuffer<int>::CalculateRequiredSize(capacity);
    TestMemory mem(memSize);
    
    auto* buffer = SharedRingBuffer<int>::Create(mem.Get(), mem.Size(), capacity);
    ASSERT_NE(buffer, nullptr);
    
    // Fill and empty multiple times to test wrap-around
    for (int round = 0; round < 3; ++round) {
        // Fill
        for (int i = 0; i < 4; ++i) {
            EXPECT_TRUE(buffer->TryPush(round * 10 + i));
        }
        EXPECT_TRUE(buffer->IsFull());
        
        // Empty
        for (int i = 0; i < 4; ++i) {
            int value;
            EXPECT_TRUE(buffer->TryPop(value));
            EXPECT_EQ(value, round * 10 + i);
        }
        EXPECT_TRUE(buffer->IsEmpty());
    }
    
    delete buffer;
}

/**
 * @brief Test with LogEntry type
 * @brief 测试 LogEntry 类型
 */
TEST(SharedRingBufferTest, WithLogEntry) {
    constexpr size_t capacity = 16;
    size_t memSize = SharedRingBuffer<LogEntry>::CalculateRequiredSize(capacity);
    TestMemory mem(memSize);
    
    auto* buffer = SharedRingBuffer<LogEntry>::Create(mem.Get(), mem.Size(), capacity);
    ASSERT_NE(buffer, nullptr);
    
    // Create and push LogEntry
    LogEntry entry;
    entry.timestamp = 12345;
    entry.level = Level::Info;
    entry.threadId = 1;
    entry.processId = 100;
    
    EXPECT_TRUE(buffer->TryPush(std::move(entry)));
    EXPECT_EQ(buffer->Size(), 1);
    
    // Pop and verify
    LogEntry popped;
    EXPECT_TRUE(buffer->TryPop(popped));
    EXPECT_EQ(popped.timestamp, 12345);
    EXPECT_EQ(popped.level, Level::Info);
    EXPECT_EQ(popped.threadId, 1);
    EXPECT_EQ(popped.processId, 100);
    
    delete buffer;
}

/**
 * @brief Test consumer state tracking
 * @brief 测试消费者状态跟踪
 */
TEST(SharedRingBufferTest, ConsumerStateTracking) {
    constexpr size_t capacity = 16;
    size_t memSize = SharedRingBuffer<int>::CalculateRequiredSize(capacity);
    TestMemory mem(memSize);
    
    auto* buffer = SharedRingBuffer<int>::Create(mem.Get(), mem.Size(), capacity);
    ASSERT_NE(buffer, nullptr);
    
    // Initially consumer should be Active
    EXPECT_EQ(buffer->GetConsumerState(), ConsumerState::Active);
    
    delete buffer;
}

/**
 * @brief Test dropped count tracking
 * @brief 测试丢弃计数跟踪
 */
TEST(SharedRingBufferTest, DroppedCountTracking) {
    constexpr size_t capacity = 4;
    size_t memSize = SharedRingBuffer<int>::CalculateRequiredSize(capacity);
    TestMemory mem(memSize);
    
    auto* buffer = SharedRingBuffer<int>::Create(mem.Get(), mem.Size(), capacity,
                                                  QueueFullPolicy::DropNewest);
    ASSERT_NE(buffer, nullptr);
    
    // Fill the queue
    for (int i = 0; i < 4; ++i) {
        buffer->TryPush(i);
    }
    
    // Try to push more (should be dropped)
    for (int i = 0; i < 10; ++i) {
        buffer->TryPush(100 + i);
    }
    
    EXPECT_EQ(buffer->GetDroppedCount(), 10);
    
    // Reset dropped count
    buffer->ResetDroppedCount();
    EXPECT_EQ(buffer->GetDroppedCount(), 0);
    
    delete buffer;
}

}  // namespace test
}  // namespace oneplog
