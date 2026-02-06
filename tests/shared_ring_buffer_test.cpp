/**
 * @file shared_ring_buffer_test.cpp
 * @brief Unit tests for SharedRingBuffer
 * @brief SharedRingBuffer 单元测试
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <gtest/gtest.h>
#include "oneplog/shared_ring_buffer.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <cstring>

using namespace oneplog::internal;

// Test fixture for SharedRingBuffer tests
// SharedRingBuffer 测试夹具
class SharedRingBufferTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up any leftover shared memory from previous tests
        // 清理之前测试遗留的共享内存
#if !defined(_WIN32)
        shm_unlink("/test_srb");
        shm_unlink("/test_srb_connect");
        shm_unlink("/test_srb_multi");
#endif
    }

    void TearDown() override {
        // Clean up shared memory
        // 清理共享内存
#if !defined(_WIN32)
        shm_unlink("/test_srb");
        shm_unlink("/test_srb_connect");
        shm_unlink("/test_srb_multi");
#endif
    }
};

// ============================================================================
// Basic Creation Tests / 基本创建测试
// ============================================================================

TEST_F(SharedRingBufferTest, CreateReturnsValidPointer) {
    auto buffer = DefaultSharedRingBuffer::Create("/test_srb");
    ASSERT_NE(buffer, nullptr);
    EXPECT_TRUE(buffer->IsEmpty());
    EXPECT_EQ(buffer->Size(), 0);
}

TEST_F(SharedRingBufferTest, CreateWithEmptyNameReturnsNull) {
    auto buffer = DefaultSharedRingBuffer::Create("");
    EXPECT_EQ(buffer, nullptr);
}

TEST_F(SharedRingBufferTest, CreateInitializesWithCorrectPolicy) {
    auto buffer = DefaultSharedRingBuffer::Create("/test_srb", QueueFullPolicy::Block);
    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->GetFullPolicy(), QueueFullPolicy::Block);
}

TEST_F(SharedRingBufferTest, CreateInitializesWithDropNewestPolicy) {
    auto buffer = DefaultSharedRingBuffer::Create("/test_srb", QueueFullPolicy::DropNewest);
    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->GetFullPolicy(), QueueFullPolicy::DropNewest);
}


// ============================================================================
// Connect Tests / 连接测试
// ============================================================================

TEST_F(SharedRingBufferTest, ConnectToExistingBuffer) {
    // Create buffer first
    // 首先创建缓冲区
    auto creator = DefaultSharedRingBuffer::Create("/test_srb_connect");
    ASSERT_NE(creator, nullptr);
    
    // Write some data
    // 写入一些数据
    const char* testData = "Hello, SharedRingBuffer!";
    EXPECT_TRUE(creator->TryPush(testData, strlen(testData) + 1));
    
    // Connect to existing buffer
    // 连接到已存在的缓冲区
    auto connector = DefaultSharedRingBuffer::Connect("/test_srb_connect");
    ASSERT_NE(connector, nullptr);
    
    // Verify data is visible
    // 验证数据可见
    EXPECT_EQ(connector->Size(), 1);
    EXPECT_FALSE(connector->IsEmpty());
    
    // Read data from connected buffer
    // 从连接的缓冲区读取数据
    char readBuffer[256];
    size_t readSize = sizeof(readBuffer);
    EXPECT_TRUE(connector->TryPop(readBuffer, readSize));
    EXPECT_STREQ(readBuffer, testData);
}

TEST_F(SharedRingBufferTest, ConnectWithEmptyNameReturnsNull) {
    auto buffer = DefaultSharedRingBuffer::Connect("");
    EXPECT_EQ(buffer, nullptr);
}

TEST_F(SharedRingBufferTest, ConnectToNonExistentBufferReturnsNull) {
    auto buffer = DefaultSharedRingBuffer::Connect("/nonexistent_buffer_12345");
    EXPECT_EQ(buffer, nullptr);
}

// ============================================================================
// Memory Size Tests / 内存大小测试
// ============================================================================

TEST_F(SharedRingBufferTest, GetRequiredMemorySizeIsCorrect) {
    size_t expectedSize = sizeof(DefaultSharedRingBuffer::ShmMetadata) + 
                          sizeof(DefaultSharedRingBuffer);
    EXPECT_EQ(DefaultSharedRingBuffer::GetRequiredMemorySize(), expectedSize);
}

TEST_F(SharedRingBufferTest, GetTotalMemorySizeIsCorrect) {
    EXPECT_EQ(DefaultSharedRingBuffer::GetTotalMemorySize(), sizeof(DefaultSharedRingBuffer));
}

TEST_F(SharedRingBufferTest, SmallBufferHasSmallerSize) {
    EXPECT_LT(SmallSharedRingBuffer::GetTotalMemorySize(), 
              DefaultSharedRingBuffer::GetTotalMemorySize());
}

TEST_F(SharedRingBufferTest, LargeBufferHasLargerSize) {
    EXPECT_GT(LargeSharedRingBuffer::GetTotalMemorySize(), 
              DefaultSharedRingBuffer::GetTotalMemorySize());
}

// ============================================================================
// In-Place Creation Tests / 就地创建测试
// ============================================================================

TEST_F(SharedRingBufferTest, CreateInPlaceWithValidMemory) {
    // Allocate aligned memory
    // 分配对齐的内存
    void* memory = nullptr;
#if defined(_MSC_VER)
    memory = _aligned_malloc(DefaultSharedRingBuffer::GetTotalMemorySize(), kCacheLineSize);
#else
    posix_memalign(&memory, kCacheLineSize, DefaultSharedRingBuffer::GetTotalMemorySize());
#endif
    ASSERT_NE(memory, nullptr);
    
    auto* buffer = DefaultSharedRingBuffer::CreateInPlace(memory);
    ASSERT_NE(buffer, nullptr);
    EXPECT_TRUE(buffer->IsEmpty());
    
    // Clean up
    // 清理
    buffer->~SharedRingBuffer();
#if defined(_MSC_VER)
    _aligned_free(memory);
#else
    free(memory);
#endif
}

TEST_F(SharedRingBufferTest, CreateInPlaceWithNullReturnsNull) {
    auto* buffer = DefaultSharedRingBuffer::CreateInPlace(nullptr);
    EXPECT_EQ(buffer, nullptr);
}

TEST_F(SharedRingBufferTest, ConnectInPlaceWithValidMemory) {
    // Allocate and initialize
    // 分配并初始化
    void* memory = nullptr;
#if defined(_MSC_VER)
    memory = _aligned_malloc(DefaultSharedRingBuffer::GetTotalMemorySize(), kCacheLineSize);
#else
    posix_memalign(&memory, kCacheLineSize, DefaultSharedRingBuffer::GetTotalMemorySize());
#endif
    ASSERT_NE(memory, nullptr);
    
    auto* creator = DefaultSharedRingBuffer::CreateInPlace(memory);
    ASSERT_NE(creator, nullptr);
    
    // Write data
    // 写入数据
    const char* testData = "Test";
    creator->TryPush(testData, strlen(testData) + 1);
    
    // Connect in place
    // 就地连接
    auto* connector = DefaultSharedRingBuffer::ConnectInPlace(memory);
    ASSERT_NE(connector, nullptr);
    EXPECT_EQ(connector->Size(), 1);
    
    // Clean up
    // 清理
    creator->~SharedRingBuffer();
#if defined(_MSC_VER)
    _aligned_free(memory);
#else
    free(memory);
#endif
}

TEST_F(SharedRingBufferTest, ConnectInPlaceWithNullReturnsNull) {
    auto* buffer = DefaultSharedRingBuffer::ConnectInPlace(nullptr);
    EXPECT_EQ(buffer, nullptr);
}


// ============================================================================
// Push/Pop Tests (Inherited from RingBuffer) / 推送/弹出测试（继承自 RingBuffer）
// ============================================================================

TEST_F(SharedRingBufferTest, TryPushAndPop) {
    auto buffer = DefaultSharedRingBuffer::Create("/test_srb");
    ASSERT_NE(buffer, nullptr);
    
    const char* testData = "Test message";
    EXPECT_TRUE(buffer->TryPush(testData, strlen(testData) + 1));
    EXPECT_EQ(buffer->Size(), 1);
    
    char readBuffer[256];
    size_t readSize = sizeof(readBuffer);
    EXPECT_TRUE(buffer->TryPop(readBuffer, readSize));
    EXPECT_STREQ(readBuffer, testData);
    EXPECT_TRUE(buffer->IsEmpty());
}

TEST_F(SharedRingBufferTest, TryPushExReturnsValidIndex) {
    auto buffer = DefaultSharedRingBuffer::Create("/test_srb");
    ASSERT_NE(buffer, nullptr);
    
    const char* testData = "Test";
    int64_t index = buffer->TryPushEx(testData, strlen(testData) + 1);
    EXPECT_GE(index, 0);
}

TEST_F(SharedRingBufferTest, PopFromEmptyBufferReturnsFalse) {
    auto buffer = DefaultSharedRingBuffer::Create("/test_srb");
    ASSERT_NE(buffer, nullptr);
    
    char readBuffer[256];
    size_t readSize = sizeof(readBuffer);
    EXPECT_FALSE(buffer->TryPop(readBuffer, readSize));
}

TEST_F(SharedRingBufferTest, PushNullDataReturnsFalse) {
    auto buffer = DefaultSharedRingBuffer::Create("/test_srb");
    ASSERT_NE(buffer, nullptr);
    
    EXPECT_FALSE(buffer->TryPush(nullptr, 10));
}

TEST_F(SharedRingBufferTest, PushZeroSizeReturnsFalse) {
    auto buffer = DefaultSharedRingBuffer::Create("/test_srb");
    ASSERT_NE(buffer, nullptr);
    
    const char* testData = "Test";
    EXPECT_FALSE(buffer->TryPush(testData, 0));
}

// ============================================================================
// WFC Tests (Inherited from RingBuffer) / WFC 测试（继承自 RingBuffer）
// ============================================================================

TEST_F(SharedRingBufferTest, WaitForCompletionBasic) {
    auto buffer = DefaultSharedRingBuffer::Create("/test_srb", QueueFullPolicy::Block);
    ASSERT_NE(buffer, nullptr);
    
    const char* testData = "WFC Test";
    int64_t slotIndex = buffer->TryPushEx(testData, strlen(testData) + 1);
    ASSERT_GE(slotIndex, 0);
    
    // Start consumer thread
    // 启动消费者线程
    std::thread consumer([&buffer]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        char readBuffer[256];
        size_t readSize = sizeof(readBuffer);
        buffer->TryPop(readBuffer, readSize);
    });
    
    // Wait for completion
    // 等待完成
    bool completed = buffer->WaitForCompletion(slotIndex, std::chrono::milliseconds(1000));
    EXPECT_TRUE(completed);
    
    consumer.join();
}

TEST_F(SharedRingBufferTest, WaitForCompletionTimeout) {
    auto buffer = DefaultSharedRingBuffer::Create("/test_srb");
    ASSERT_NE(buffer, nullptr);
    
    const char* testData = "Timeout Test";
    int64_t slotIndex = buffer->TryPushEx(testData, strlen(testData) + 1);
    ASSERT_GE(slotIndex, 0);
    
    // No consumer, should timeout
    // 没有消费者，应该超时
    bool completed = buffer->WaitForCompletion(slotIndex, std::chrono::milliseconds(50));
    EXPECT_FALSE(completed);
}

TEST_F(SharedRingBufferTest, FlushWaitsForAllMessages) {
    auto buffer = DefaultSharedRingBuffer::Create("/test_srb", QueueFullPolicy::Block);
    ASSERT_NE(buffer, nullptr);
    
    // Push multiple messages
    // 推送多条消息
    for (int i = 0; i < 5; ++i) {
        std::string msg = "Message " + std::to_string(i);
        buffer->TryPush(msg.c_str(), msg.size() + 1);
    }
    
    EXPECT_EQ(buffer->Size(), 5);
    
    // Start consumer thread
    // 启动消费者线程
    std::thread consumer([&buffer]() {
        for (int i = 0; i < 5; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            char readBuffer[256];
            size_t readSize = sizeof(readBuffer);
            buffer->TryPop(readBuffer, readSize);
        }
    });
    
    // Flush should wait for all messages
    // Flush 应该等待所有消息
    bool flushed = buffer->Flush(std::chrono::milliseconds(1000));
    EXPECT_TRUE(flushed);
    EXPECT_TRUE(buffer->IsEmpty());
    
    consumer.join();
}


// ============================================================================
// Multi-Process Simulation Tests / 多进程模拟测试
// ============================================================================

TEST_F(SharedRingBufferTest, MultipleConnectorsCanReadSameData) {
    auto creator = DefaultSharedRingBuffer::Create("/test_srb_multi");
    ASSERT_NE(creator, nullptr);
    
    // Write data
    // 写入数据
    const char* testData = "Shared data";
    creator->TryPush(testData, strlen(testData) + 1);
    
    // Multiple connectors
    // 多个连接器
    auto connector1 = DefaultSharedRingBuffer::Connect("/test_srb_multi");
    auto connector2 = DefaultSharedRingBuffer::Connect("/test_srb_multi");
    
    ASSERT_NE(connector1, nullptr);
    ASSERT_NE(connector2, nullptr);
    
    // Both should see the same size
    // 两者应该看到相同的大小
    EXPECT_EQ(connector1->Size(), 1);
    EXPECT_EQ(connector2->Size(), 1);
}

TEST_F(SharedRingBufferTest, ConnectorSeesCreatorWrites) {
    auto creator = DefaultSharedRingBuffer::Create("/test_srb_multi");
    ASSERT_NE(creator, nullptr);
    
    auto connector = DefaultSharedRingBuffer::Connect("/test_srb_multi");
    ASSERT_NE(connector, nullptr);
    
    // Initially empty
    // 初始为空
    EXPECT_TRUE(connector->IsEmpty());
    
    // Creator writes
    // 创建者写入
    const char* testData = "New data";
    creator->TryPush(testData, strlen(testData) + 1);
    
    // Connector should see the write
    // 连接器应该看到写入
    EXPECT_EQ(connector->Size(), 1);
    
    // Connector can read
    // 连接器可以读取
    char readBuffer[256];
    size_t readSize = sizeof(readBuffer);
    EXPECT_TRUE(connector->TryPop(readBuffer, readSize));
    EXPECT_STREQ(readBuffer, testData);
}

// ============================================================================
// Concurrent Access Tests / 并发访问测试
// ============================================================================

TEST_F(SharedRingBufferTest, ConcurrentPushFromMultipleThreads) {
    auto buffer = DefaultSharedRingBuffer::Create("/test_srb", QueueFullPolicy::DropNewest);
    ASSERT_NE(buffer, nullptr);
    
    constexpr int kNumThreads = 4;
    constexpr int kMessagesPerThread = 100;
    std::atomic<int> successCount{0};
    
    std::vector<std::thread> producers;
    for (int t = 0; t < kNumThreads; ++t) {
        producers.emplace_back([&buffer, &successCount, t]() {
            for (int i = 0; i < kMessagesPerThread; ++i) {
                std::string msg = "T" + std::to_string(t) + "_M" + std::to_string(i);
                if (buffer->TryPush(msg.c_str(), msg.size() + 1)) {
                    successCount.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    
    // Consumer thread
    // 消费者线程
    std::atomic<bool> stopConsumer{false};
    std::atomic<int> consumedCount{0};
    std::thread consumer([&buffer, &stopConsumer, &consumedCount]() {
        while (!stopConsumer.load(std::memory_order_relaxed) || !buffer->IsEmpty()) {
            char readBuffer[256];
            size_t readSize = sizeof(readBuffer);
            if (buffer->TryPop(readBuffer, readSize)) {
                consumedCount.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    // Wait for producers
    // 等待生产者
    for (auto& t : producers) {
        t.join();
    }
    
    // Stop consumer
    // 停止消费者
    stopConsumer.store(true, std::memory_order_relaxed);
    consumer.join();
    
    // Verify counts
    // 验证计数
    EXPECT_EQ(successCount.load(), consumedCount.load() + buffer->GetDroppedCount());
}

// ============================================================================
// Type Alias Tests / 类型别名测试
// ============================================================================

TEST_F(SharedRingBufferTest, DefaultSharedRingBufferHasCorrectParameters) {
    constexpr size_t expected = (RingBuffer<512, 1024>::kMaxDataSize);
    EXPECT_EQ(DefaultSharedRingBuffer::kMaxDataSize, expected);
}

TEST_F(SharedRingBufferTest, SmallSharedRingBufferHasCorrectParameters) {
    constexpr size_t expected = (RingBuffer<256, 256>::kMaxDataSize);
    EXPECT_EQ(SmallSharedRingBuffer::kMaxDataSize, expected);
}

TEST_F(SharedRingBufferTest, LargeSharedRingBufferHasCorrectParameters) {
    constexpr size_t expected = (RingBuffer<1024, 4096>::kMaxDataSize);
    EXPECT_EQ(LargeSharedRingBuffer::kMaxDataSize, expected);
}

// ============================================================================
// Capacity Tests / 容量测试
// ============================================================================

TEST_F(SharedRingBufferTest, CapacityMatchesSlotCount) {
    auto buffer = DefaultSharedRingBuffer::Create("/test_srb");
    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->Capacity(), 1024);
}

TEST_F(SharedRingBufferTest, SmallBufferHasCorrectCapacity) {
    auto buffer = SmallSharedRingBuffer::Create("/test_srb");
    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->Capacity(), 256);
}

// ============================================================================
// Cleanup Tests / 清理测试
// ============================================================================

TEST_F(SharedRingBufferTest, DestructorCleansUpSharedMemory) {
    {
        auto buffer = DefaultSharedRingBuffer::Create("/test_srb");
        ASSERT_NE(buffer, nullptr);
        // Buffer goes out of scope, destructor should clean up
        // 缓冲区超出作用域，析构函数应该清理
    }
    
    // After cleanup, connect should fail (on POSIX systems)
    // 清理后，连接应该失败（在 POSIX 系统上）
#if !defined(_WIN32)
    auto buffer = DefaultSharedRingBuffer::Connect("/test_srb");
    EXPECT_EQ(buffer, nullptr);
#endif
}

TEST_F(SharedRingBufferTest, ConnectorDoesNotUnlinkOnDestruction) {
    auto creator = DefaultSharedRingBuffer::Create("/test_srb_multi");
    ASSERT_NE(creator, nullptr);
    
    {
        auto connector = DefaultSharedRingBuffer::Connect("/test_srb_multi");
        ASSERT_NE(connector, nullptr);
        // Connector goes out of scope
        // 连接器超出作用域
    }
    
    // Creator should still work
    // 创建者应该仍然工作
    const char* testData = "Still works";
    EXPECT_TRUE(creator->TryPush(testData, strlen(testData) + 1));
    EXPECT_EQ(creator->Size(), 1);
}
