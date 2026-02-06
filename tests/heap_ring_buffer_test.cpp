/**
 * @file heap_ring_buffer_test.cpp
 * @brief Unit tests for HeapRingBuffer
 * @文件 heap_ring_buffer_test.cpp
 * @简述 HeapRingBuffer 的单元测试
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include "oneplog/heap_ring_buffer.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>

using namespace oneplog;
using namespace oneplog::internal;

// ==============================================================================
// Factory Method Tests / 工厂方法测试
// ==============================================================================

TEST(HeapRingBufferTest, CreateWithDefaultPolicy) {
    auto buffer = HeapRingBuffer<512, 64>::Create();
    
    ASSERT_NE(buffer, nullptr);
    EXPECT_TRUE(buffer->IsEmpty());
    EXPECT_FALSE(buffer->IsFull());
    EXPECT_EQ(buffer->Size(), 0);
    EXPECT_EQ(buffer->Capacity(), 64);
    EXPECT_EQ(buffer->GetDroppedCount(), 0);
    EXPECT_EQ(buffer->GetFullPolicy(), QueueFullPolicy::DropNewest);
}

TEST(HeapRingBufferTest, CreateWithBlockPolicy) {
    auto buffer = HeapRingBuffer<512, 64>::Create(QueueFullPolicy::Block);
    
    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->GetFullPolicy(), QueueFullPolicy::Block);
}

TEST(HeapRingBufferTest, CreateWithDropOldestPolicy) {
    auto buffer = HeapRingBuffer<512, 64>::Create(QueueFullPolicy::DropOldest);
    
    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->GetFullPolicy(), QueueFullPolicy::DropOldest);
}

TEST(HeapRingBufferTest, MemoryAlignment) {
    auto buffer = HeapRingBuffer<512, 64>::Create();
    
    ASSERT_NE(buffer, nullptr);
    // Verify cache line alignment
    uintptr_t address = reinterpret_cast<uintptr_t>(buffer.get());
    EXPECT_EQ(address % kCacheLineSize, 0);
}


// ==============================================================================
// Inheritance Tests / 继承测试
// ==============================================================================

TEST(HeapRingBufferTest, InheritsFromRingBuffer) {
    auto buffer = HeapRingBuffer<512, 64>::Create();
    ASSERT_NE(buffer, nullptr);
    
    // HeapRingBuffer should be usable as RingBuffer
    // HeapRingBuffer 应该可以作为 RingBuffer 使用
    RingBuffer<512, 64>* basePtr = buffer.get();
    EXPECT_NE(basePtr, nullptr);
    EXPECT_EQ(basePtr->Capacity(), 64);
}

TEST(HeapRingBufferTest, GetTotalMemorySize) {
    using TestBuffer = HeapRingBuffer<512, 64>;
    using TestRingBuffer = RingBuffer<512, 64>;
    
    size_t expectedSize = sizeof(TestBuffer);
    size_t actualSize = TestBuffer::GetTotalMemorySize();
    EXPECT_EQ(actualSize, expectedSize);
    
    // Should be same as base RingBuffer size
    size_t ringBufferSize = sizeof(TestRingBuffer);
    EXPECT_EQ(expectedSize, ringBufferSize);
}

// ==============================================================================
// Push and Pop Tests / 推送和弹出测试
// ==============================================================================

TEST(HeapRingBufferTest, TryPushAndTryPop) {
    auto buffer = HeapRingBuffer<512, 64>::Create();
    ASSERT_NE(buffer, nullptr);
    
    // Push data
    const char* testData = "Test message";
    size_t dataSize = strlen(testData) + 1;
    EXPECT_TRUE(buffer->TryPush(testData, dataSize));
    
    EXPECT_FALSE(buffer->IsEmpty());
    EXPECT_EQ(buffer->Size(), 1);
    
    // Pop data
    char readBuffer[100];
    size_t readSize = sizeof(readBuffer);
    EXPECT_TRUE(buffer->TryPop(readBuffer, readSize));
    EXPECT_EQ(readSize, dataSize);
    EXPECT_STREQ(readBuffer, testData);
    
    EXPECT_TRUE(buffer->IsEmpty());
    EXPECT_EQ(buffer->Size(), 0);
}

TEST(HeapRingBufferTest, TryPopEmpty) {
    auto buffer = HeapRingBuffer<512, 64>::Create();
    ASSERT_NE(buffer, nullptr);
    
    // Try to pop from empty buffer
    char readBuffer[100];
    size_t readSize = sizeof(readBuffer);
    EXPECT_FALSE(buffer->TryPop(readBuffer, readSize));
}

TEST(HeapRingBufferTest, MultiplePushPop) {
    auto buffer = HeapRingBuffer<512, 64>::Create();
    ASSERT_NE(buffer, nullptr);
    
    // Push multiple items
    for (int i = 0; i < 10; ++i) {
        std::string data = "Message " + std::to_string(i);
        EXPECT_TRUE(buffer->TryPush(data.c_str(), data.size() + 1));
    }
    
    EXPECT_EQ(buffer->Size(), 10);
    
    // Pop and verify
    for (int i = 0; i < 10; ++i) {
        char readBuffer[100];
        size_t readSize = sizeof(readBuffer);
        EXPECT_TRUE(buffer->TryPop(readBuffer, readSize));
        
        std::string expected = "Message " + std::to_string(i);
        EXPECT_STREQ(readBuffer, expected.c_str());
    }
    
    EXPECT_TRUE(buffer->IsEmpty());
}


// ==============================================================================
// Queue Full Policy Tests / 队列满策略测试
// ==============================================================================

TEST(HeapRingBufferTest, DropNewestPolicy) {
    auto buffer = HeapRingBuffer<512, 64>::Create(QueueFullPolicy::DropNewest);
    ASSERT_NE(buffer, nullptr);
    
    // Fill the buffer
    for (int i = 0; i < 64; ++i) {
        std::string data = "Message " + std::to_string(i);
        EXPECT_TRUE(buffer->TryPush(data.c_str(), data.size() + 1));
    }
    
    EXPECT_TRUE(buffer->IsFull());
    EXPECT_EQ(buffer->GetDroppedCount(), 0);
    
    // Try to push when full - should drop newest
    std::string newData = "Message 64";
    EXPECT_FALSE(buffer->TryPush(newData.c_str(), newData.size() + 1));
    EXPECT_EQ(buffer->GetDroppedCount(), 1);
    
    // Verify old messages are still there
    for (int i = 0; i < 64; ++i) {
        char readBuffer[100];
        size_t readSize = sizeof(readBuffer);
        EXPECT_TRUE(buffer->TryPop(readBuffer, readSize));
        
        std::string expected = "Message " + std::to_string(i);
        EXPECT_STREQ(readBuffer, expected.c_str());
    }
}

TEST(HeapRingBufferTest, BlockPolicy) {
    auto buffer = HeapRingBuffer<512, 64>::Create(QueueFullPolicy::Block);
    ASSERT_NE(buffer, nullptr);
    
    // Fill the buffer
    for (int i = 0; i < 64; ++i) {
        std::string data = "Message " + std::to_string(i);
        EXPECT_TRUE(buffer->TryPush(data.c_str(), data.size() + 1));
    }
    
    EXPECT_TRUE(buffer->IsFull());
    
    // Try to push when full in a separate thread
    std::atomic<bool> pushCompleted{false};
    std::thread producer([&]() {
        std::string data = "Message 64";
        buffer->TryPush(data.c_str(), data.size() + 1);
        pushCompleted = true;
    });
    
    // Give producer time to block
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(pushCompleted);
    
    // Pop one item to make space
    char readBuffer[100];
    size_t readSize = sizeof(readBuffer);
    EXPECT_TRUE(buffer->TryPop(readBuffer, readSize));
    
    // Wait for producer to complete
    producer.join();
    EXPECT_TRUE(pushCompleted);
    
    // Verify the new message was pushed
    EXPECT_EQ(buffer->Size(), 64);
}

// ==============================================================================
// WFC Tests / WFC 测试
// ==============================================================================

TEST(HeapRingBufferTest, TryPushExReturnsSlotIndex) {
    auto buffer = HeapRingBuffer<512, 64>::Create();
    ASSERT_NE(buffer, nullptr);
    
    // Push with TryPushEx returns slot index
    const char* testData = "WFC message";
    size_t dataSize = strlen(testData) + 1;
    int64_t slotIndex = buffer->TryPushEx(testData, dataSize);
    
    EXPECT_GE(slotIndex, 0);
    EXPECT_EQ(buffer->Size(), 1);
}

TEST(HeapRingBufferTest, WaitForCompletionWithConsumer) {
    auto buffer = HeapRingBuffer<512, 64>::Create();
    ASSERT_NE(buffer, nullptr);
    
    // Push with TryPushEx
    const char* testData = "WFC message";
    size_t dataSize = strlen(testData) + 1;
    int64_t slotIndex = buffer->TryPushEx(testData, dataSize);
    EXPECT_GE(slotIndex, 0);
    
    // Start consumer thread
    std::thread consumer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        char readBuffer[100];
        size_t readSize = sizeof(readBuffer);
        buffer->TryPop(readBuffer, readSize);
    });
    
    // Wait for completion (should succeed after consumer processes)
    EXPECT_TRUE(buffer->WaitForCompletion(slotIndex, std::chrono::milliseconds(500)));
    
    consumer.join();
}

TEST(HeapRingBufferTest, WaitForCompletionTimeout) {
    auto buffer = HeapRingBuffer<512, 64>::Create();
    ASSERT_NE(buffer, nullptr);
    
    // Push but don't consume
    const char* testData = "WFC message";
    size_t dataSize = strlen(testData) + 1;
    int64_t slotIndex = buffer->TryPushEx(testData, dataSize);
    EXPECT_GE(slotIndex, 0);
    
    // Wait for completion (should timeout since no consumer)
    EXPECT_FALSE(buffer->WaitForCompletion(slotIndex, std::chrono::milliseconds(50)));
}


TEST(HeapRingBufferTest, FlushWaitsForAllMessages) {
    auto buffer = HeapRingBuffer<512, 64>::Create();
    ASSERT_NE(buffer, nullptr);
    
    // Push multiple messages
    for (int i = 0; i < 5; ++i) {
        std::string data = "Message " + std::to_string(i);
        buffer->TryPush(data.c_str(), data.size() + 1);
    }
    
    // Start consumer thread
    std::thread consumer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        for (int i = 0; i < 5; ++i) {
            char readBuffer[100];
            size_t readSize = sizeof(readBuffer);
            buffer->TryPop(readBuffer, readSize);
        }
    });
    
    // Flush should wait for all messages to be processed
    EXPECT_TRUE(buffer->Flush(std::chrono::milliseconds(500)));
    
    consumer.join();
}

TEST(HeapRingBufferTest, ProcessedTailUpdatedOnPop) {
    auto buffer = HeapRingBuffer<512, 64>::Create();
    ASSERT_NE(buffer, nullptr);
    
    // Initial processed tail should be 0
    EXPECT_EQ(buffer->GetProcessedTail(), 0);
    
    // Push and pop
    const char* testData = "Test";
    buffer->TryPush(testData, 5);
    
    char readBuffer[100];
    size_t readSize = sizeof(readBuffer);
    buffer->TryPop(readBuffer, readSize);
    
    // Processed tail should be 1
    EXPECT_EQ(buffer->GetProcessedTail(), 1);
}

// ==============================================================================
// Edge Cases / 边界情况测试
// ==============================================================================

TEST(HeapRingBufferTest, PushNullData) {
    auto buffer = HeapRingBuffer<512, 64>::Create();
    ASSERT_NE(buffer, nullptr);
    
    EXPECT_FALSE(buffer->TryPush(nullptr, 10));
}

TEST(HeapRingBufferTest, PushZeroSize) {
    auto buffer = HeapRingBuffer<512, 64>::Create();
    ASSERT_NE(buffer, nullptr);
    
    const char* data = "test";
    EXPECT_FALSE(buffer->TryPush(data, 0));
}

TEST(HeapRingBufferTest, PushTooLarge) {
    auto buffer = HeapRingBuffer<512, 64>::Create();
    ASSERT_NE(buffer, nullptr);
    
    std::vector<uint8_t> largeData(buffer->kMaxDataSize + 1, 0xFF);
    EXPECT_FALSE(buffer->TryPush(largeData.data(), largeData.size()));
}

TEST(HeapRingBufferTest, PopNullBuffer) {
    auto buffer = HeapRingBuffer<512, 64>::Create();
    ASSERT_NE(buffer, nullptr);
    
    // Push data first
    const char* data = "test";
    buffer->TryPush(data, strlen(data) + 1);
    
    // Try to pop with null buffer
    size_t size = 100;
    EXPECT_FALSE(buffer->TryPop(nullptr, size));
}

TEST(HeapRingBufferTest, PopZeroSize) {
    auto buffer = HeapRingBuffer<512, 64>::Create();
    ASSERT_NE(buffer, nullptr);
    
    // Push data first
    const char* data = "test";
    buffer->TryPush(data, strlen(data) + 1);
    
    // Try to pop with zero size
    char readBuffer[100];
    size_t size = 0;
    EXPECT_FALSE(buffer->TryPop(readBuffer, size));
}

TEST(HeapRingBufferTest, WrapAround) {
    auto buffer = HeapRingBuffer<512, 64>::Create();
    ASSERT_NE(buffer, nullptr);
    
    // Fill and empty multiple times to test wrap-around
    for (int cycle = 0; cycle < 3; ++cycle) {
        // Fill
        for (int i = 0; i < 64; ++i) {
            std::string data = "Cycle " + std::to_string(cycle) + " Msg " + std::to_string(i);
            EXPECT_TRUE(buffer->TryPush(data.c_str(), data.size() + 1));
        }
        
        // Empty
        for (int i = 0; i < 64; ++i) {
            char readBuffer[100];
            size_t readSize = sizeof(readBuffer);
            EXPECT_TRUE(buffer->TryPop(readBuffer, readSize));
            
            std::string expected = "Cycle " + std::to_string(cycle) + " Msg " + std::to_string(i);
            EXPECT_STREQ(readBuffer, expected.c_str());
        }
        
        EXPECT_TRUE(buffer->IsEmpty());
    }
}

// ==============================================================================
// Type Alias Tests / 类型别名测试
// ==============================================================================

TEST(HeapRingBufferTest, DefaultHeapRingBufferAlias) {
    auto buffer = DefaultHeapRingBuffer::Create();
    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->Capacity(), 1024);
}

TEST(HeapRingBufferTest, SmallHeapRingBufferAlias) {
    auto buffer = SmallHeapRingBuffer::Create();
    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->Capacity(), 256);
}

TEST(HeapRingBufferTest, LargeHeapRingBufferAlias) {
    auto buffer = LargeHeapRingBuffer::Create();
    ASSERT_NE(buffer, nullptr);
    EXPECT_EQ(buffer->Capacity(), 4096);
}
