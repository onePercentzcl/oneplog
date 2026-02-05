/**
 * @file ring_buffer_test.cpp
 * @brief Unit tests for RingBuffer
 * @文件 ring_buffer_test.cpp
 * @简述 RingBuffer 的单元测试
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include "oneplog/ring_buffer.hpp"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <atomic>

using namespace oneplog;
using namespace oneplog::internal;

// ==============================================================================
// SlotState Tests / SlotState 测试
// ==============================================================================

TEST(SlotStateTest, EnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(SlotState::Empty), 0);
    EXPECT_EQ(static_cast<uint8_t>(SlotState::Writing), 1);
    EXPECT_EQ(static_cast<uint8_t>(SlotState::Ready), 2);
    EXPECT_EQ(static_cast<uint8_t>(SlotState::Reading), 3);
}

// ==============================================================================
// WFCState Tests / WFCState 测试
// ==============================================================================

TEST(WFCStateTest, EnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(WFCState::None), 0);
    EXPECT_EQ(static_cast<uint8_t>(WFCState::Enabled), 1);
    EXPECT_EQ(static_cast<uint8_t>(WFCState::Completed), 2);
}

// ==============================================================================
// QueueFullPolicy Tests / QueueFullPolicy 测试
// ==============================================================================

TEST(QueueFullPolicyTest, EnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(QueueFullPolicy::Block), 0);
    EXPECT_EQ(static_cast<uint8_t>(QueueFullPolicy::DropNewest), 1);
    EXPECT_EQ(static_cast<uint8_t>(QueueFullPolicy::DropOldest), 2);
}

// ==============================================================================
// Slot Tests / Slot 测试
// ==============================================================================

TEST(SlotTest, DefaultConstructor) {
    Slot<512> slot;
    EXPECT_EQ(slot.GetState(), SlotState::Empty);
    EXPECT_EQ(slot.GetWFC(), WFCState::None);
}

TEST(SlotTest, StateOperations) {
    Slot<512> slot;
    
    // Test SetState and GetState
    slot.SetState(SlotState::Writing);
    EXPECT_EQ(slot.GetState(), SlotState::Writing);
    
    slot.SetState(SlotState::Ready);
    EXPECT_EQ(slot.GetState(), SlotState::Ready);
    
    slot.SetState(SlotState::Reading);
    EXPECT_EQ(slot.GetState(), SlotState::Reading);
    
    slot.SetState(SlotState::Empty);
    EXPECT_EQ(slot.GetState(), SlotState::Empty);
}

TEST(SlotTest, CompareExchangeState) {
    Slot<512> slot;
    slot.SetState(SlotState::Empty);
    
    // Successful exchange
    SlotState expected = SlotState::Empty;
    EXPECT_TRUE(slot.CompareExchangeState(expected, SlotState::Writing));
    EXPECT_EQ(slot.GetState(), SlotState::Writing);
    
    // Failed exchange
    expected = SlotState::Empty;
    EXPECT_FALSE(slot.CompareExchangeState(expected, SlotState::Ready));
    EXPECT_EQ(slot.GetState(), SlotState::Writing);  // State unchanged
    EXPECT_EQ(expected, SlotState::Writing);  // Expected updated to actual value
}

TEST(SlotTest, WFCOperations) {
    Slot<512> slot;
    
    // Test SetWFC and GetWFC
    slot.SetWFC(WFCState::Enabled);
    EXPECT_EQ(slot.GetWFC(), WFCState::Enabled);
    
    slot.SetWFC(WFCState::Completed);
    EXPECT_EQ(slot.GetWFC(), WFCState::Completed);
    
    slot.SetWFC(WFCState::None);
    EXPECT_EQ(slot.GetWFC(), WFCState::None);
}

TEST(SlotTest, WriteAndReadData) {
    Slot<512> slot;
    
    // Write data
    const char* testData = "Hello, World!";
    size_t dataSize = strlen(testData) + 1;
    EXPECT_TRUE(slot.WriteData(testData, dataSize));
    
    // Read data
    char buffer[100];
    EXPECT_TRUE(slot.ReadData(buffer, dataSize));
    EXPECT_STREQ(buffer, testData);
}

TEST(SlotTest, WriteDataTooLarge) {
    Slot<512> slot;
    
    // Try to write data larger than slot capacity
    std::vector<uint8_t> largeData(slot.DataSize() + 1, 0xFF);
    EXPECT_FALSE(slot.WriteData(largeData.data(), largeData.size()));
}

TEST(SlotTest, DataSizeConstant) {
    Slot<512> slot;
    
    // DataSize should be SlotSize minus overhead
    size_t expectedSize = 512 - sizeof(std::atomic<SlotState>) 
                              - sizeof(std::atomic<WFCState>) - 6;
    EXPECT_EQ(slot.DataSize(), expectedSize);
    EXPECT_EQ(Slot<512>::DataSize(), expectedSize);
}

TEST(SlotTest, CacheLineAlignment) {
    // Verify slot is cache-line aligned
    Slot<512> slot;
    uintptr_t address = reinterpret_cast<uintptr_t>(&slot);
    EXPECT_EQ(address % kCacheLineSize, 0);
}

// ==============================================================================
// RingBufferHeader Tests / RingBufferHeader 测试
// ==============================================================================

TEST(RingBufferHeaderTest, DefaultConstructor) {
    RingBufferHeader header;
    EXPECT_EQ(header.GetHead(), 0);
    EXPECT_EQ(header.GetTail(), 0);
    EXPECT_EQ(header.GetConsumerState(), ConsumerState::Active);
    EXPECT_EQ(header.GetDroppedCount(), 0);
    EXPECT_EQ(header.GetCapacity(), 0);
    EXPECT_EQ(header.GetFullPolicy(), QueueFullPolicy::DropNewest);
}

TEST(RingBufferHeaderTest, Init) {
    RingBufferHeader header;
    header.Init(1024, QueueFullPolicy::Block);
    
    EXPECT_EQ(header.GetHead(), 0);
    EXPECT_EQ(header.GetTail(), 0);
    EXPECT_EQ(header.GetCapacity(), 1024);
    EXPECT_EQ(header.GetFullPolicy(), QueueFullPolicy::Block);
}

TEST(RingBufferHeaderTest, HeadOperations) {
    RingBufferHeader header;
    header.Init(1024);
    
    header.SetHead(10);
    EXPECT_EQ(header.GetHead(), 10);
    
    header.SetHead(100);
    EXPECT_EQ(header.GetHead(), 100);
}

TEST(RingBufferHeaderTest, TailOperations) {
    RingBufferHeader header;
    header.Init(1024);
    
    header.SetTail(20);
    EXPECT_EQ(header.GetTail(), 20);
    
    size_t oldTail = header.FetchAddTail(5);
    EXPECT_EQ(oldTail, 20);
    EXPECT_EQ(header.GetTail(), 25);
}

TEST(RingBufferHeaderTest, ConsumerStateOperations) {
    RingBufferHeader header;
    header.Init(1024);
    
    EXPECT_EQ(header.GetConsumerState(), ConsumerState::Active);
    
    header.SetConsumerState(ConsumerState::Waiting);
    EXPECT_EQ(header.GetConsumerState(), ConsumerState::Waiting);
    
    header.SetConsumerState(ConsumerState::Active);
    EXPECT_EQ(header.GetConsumerState(), ConsumerState::Active);
}

TEST(RingBufferHeaderTest, DroppedCountOperations) {
    RingBufferHeader header;
    header.Init(1024);
    
    EXPECT_EQ(header.GetDroppedCount(), 0);
    
    uint64_t oldCount = header.FetchAddDroppedCount(1);
    EXPECT_EQ(oldCount, 0);
    EXPECT_EQ(header.GetDroppedCount(), 1);
    
    header.FetchAddDroppedCount(5);
    EXPECT_EQ(header.GetDroppedCount(), 6);
}

TEST(RingBufferHeaderTest, IsEmpty) {
    RingBufferHeader header;
    header.Init(1024);
    
    EXPECT_TRUE(header.IsEmpty());
    
    header.SetTail(10);
    EXPECT_FALSE(header.IsEmpty());
    
    header.SetHead(10);
    EXPECT_TRUE(header.IsEmpty());
}

TEST(RingBufferHeaderTest, IsFull) {
    RingBufferHeader header;
    header.Init(1024);
    
    EXPECT_FALSE(header.IsFull());
    
    header.SetTail(1024);
    EXPECT_TRUE(header.IsFull());
    
    header.SetHead(1);
    EXPECT_FALSE(header.IsFull());
}

TEST(RingBufferHeaderTest, Size) {
    RingBufferHeader header;
    header.Init(1024);
    
    EXPECT_EQ(header.Size(), 0);
    
    header.SetTail(10);
    EXPECT_EQ(header.Size(), 10);
    
    header.SetHead(5);
    EXPECT_EQ(header.Size(), 5);
}

// ==============================================================================
// RingBuffer Basic Tests / RingBuffer 基本测试
// ==============================================================================

TEST(RingBufferTest, DefaultConstructor) {
    RingBuffer<512, 16> buffer;
    
    // Should not be initialized yet
    EXPECT_EQ(buffer.Capacity(), 16);
}

TEST(RingBufferTest, Init) {
    RingBuffer<512, 16> buffer;
    buffer.Init(QueueFullPolicy::DropNewest);
    
    EXPECT_TRUE(buffer.IsEmpty());
    EXPECT_FALSE(buffer.IsFull());
    EXPECT_EQ(buffer.Size(), 0);
    EXPECT_EQ(buffer.Capacity(), 16);
    EXPECT_EQ(buffer.GetDroppedCount(), 0);
    EXPECT_EQ(buffer.GetFullPolicy(), QueueFullPolicy::DropNewest);
}

TEST(RingBufferTest, TryPushAndTryPop) {
    RingBuffer<512, 16> buffer;
    buffer.Init();
    
    // Push data
    const char* testData = "Test message";
    size_t dataSize = strlen(testData) + 1;
    EXPECT_TRUE(buffer.TryPush(testData, dataSize));
    
    EXPECT_FALSE(buffer.IsEmpty());
    EXPECT_EQ(buffer.Size(), 1);
    
    // Pop data
    char readBuffer[100];
    size_t readSize = sizeof(readBuffer);
    EXPECT_TRUE(buffer.TryPop(readBuffer, readSize));
    EXPECT_EQ(readSize, dataSize);
    EXPECT_STREQ(readBuffer, testData);
    
    EXPECT_TRUE(buffer.IsEmpty());
    EXPECT_EQ(buffer.Size(), 0);
}

TEST(RingBufferTest, TryPopEmpty) {
    RingBuffer<512, 16> buffer;
    buffer.Init();
    
    // Try to pop from empty buffer
    char readBuffer[100];
    size_t readSize = sizeof(readBuffer);
    EXPECT_FALSE(buffer.TryPop(readBuffer, readSize));
}

TEST(RingBufferTest, MultiplePushPop) {
    RingBuffer<512, 16> buffer;
    buffer.Init();
    
    // Push multiple items
    for (int i = 0; i < 10; ++i) {
        std::string data = "Message " + std::to_string(i);
        EXPECT_TRUE(buffer.TryPush(data.c_str(), data.size() + 1));
    }
    
    EXPECT_EQ(buffer.Size(), 10);
    
    // Pop and verify
    for (int i = 0; i < 10; ++i) {
        char readBuffer[100];
        size_t readSize = sizeof(readBuffer);
        EXPECT_TRUE(buffer.TryPop(readBuffer, readSize));
        
        std::string expected = "Message " + std::to_string(i);
        EXPECT_STREQ(readBuffer, expected.c_str());
    }
    
    EXPECT_TRUE(buffer.IsEmpty());
}

// ==============================================================================
// Queue Full Policy Tests / 队列满策略测试
// ==============================================================================

TEST(RingBufferTest, DropNewestPolicy) {
    RingBuffer<512, 4> buffer;  // Small buffer
    buffer.Init(QueueFullPolicy::DropNewest);
    
    // Fill the buffer
    for (int i = 0; i < 4; ++i) {
        std::string data = "Message " + std::to_string(i);
        EXPECT_TRUE(buffer.TryPush(data.c_str(), data.size() + 1));
    }
    
    EXPECT_TRUE(buffer.IsFull());
    EXPECT_EQ(buffer.GetDroppedCount(), 0);
    
    // Try to push when full - should drop newest
    std::string newData = "Message 4";
    EXPECT_FALSE(buffer.TryPush(newData.c_str(), newData.size() + 1));
    EXPECT_EQ(buffer.GetDroppedCount(), 1);
    
    // Verify old messages are still there
    for (int i = 0; i < 4; ++i) {
        char readBuffer[100];
        size_t readSize = sizeof(readBuffer);
        EXPECT_TRUE(buffer.TryPop(readBuffer, readSize));
        
        std::string expected = "Message " + std::to_string(i);
        EXPECT_STREQ(readBuffer, expected.c_str());
    }
}

TEST(RingBufferTest, BlockPolicy) {
    RingBuffer<512, 4> buffer;
    buffer.Init(QueueFullPolicy::Block);
    
    // Fill the buffer
    for (int i = 0; i < 4; ++i) {
        std::string data = "Message " + std::to_string(i);
        EXPECT_TRUE(buffer.TryPush(data.c_str(), data.size() + 1));
    }
    
    EXPECT_TRUE(buffer.IsFull());
    
    // Try to push when full in a separate thread
    std::atomic<bool> pushCompleted{false};
    std::thread producer([&]() {
        std::string data = "Message 4";
        buffer.TryPush(data.c_str(), data.size() + 1);
        pushCompleted = true;
    });
    
    // Give producer time to block
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_FALSE(pushCompleted);
    
    // Pop one item to make space
    char readBuffer[100];
    size_t readSize = sizeof(readBuffer);
    EXPECT_TRUE(buffer.TryPop(readBuffer, readSize));
    
    // Wait for producer to complete
    producer.join();
    EXPECT_TRUE(pushCompleted);
    
    // Verify the new message was pushed
    EXPECT_EQ(buffer.Size(), 4);
}

// ==============================================================================
// WFC Tests / WFC 测试
// ==============================================================================

TEST(RingBufferTest, TryPushWFC) {
    RingBuffer<512, 16> buffer;
    buffer.Init();
    
    // Push with WFC
    const char* testData = "WFC message";
    size_t dataSize = strlen(testData) + 1;
    EXPECT_TRUE(buffer.TryPushWFC(testData, dataSize));
    
    EXPECT_EQ(buffer.Size(), 1);
}

TEST(RingBufferTest, WaitForCompletion) {
    RingBuffer<512, 16> buffer;
    buffer.Init();
    
    // Push with WFC
    const char* testData = "WFC message";
    size_t dataSize = strlen(testData) + 1;
    EXPECT_TRUE(buffer.TryPushWFC(testData, dataSize));
    
    // Start consumer thread
    std::thread consumer([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        char readBuffer[100];
        size_t readSize = sizeof(readBuffer);
        buffer.TryPop(readBuffer, readSize);
    });
    
    // Wait for completion (should succeed)
    int64_t slotIndex = 0;  // First slot
    EXPECT_TRUE(buffer.WaitForCompletion(slotIndex, std::chrono::milliseconds(200)));
    
    consumer.join();
}

TEST(RingBufferTest, WaitForCompletionTimeout) {
    RingBuffer<512, 16> buffer;
    buffer.Init();
    
    // Push with WFC but don't consume
    const char* testData = "WFC message";
    size_t dataSize = strlen(testData) + 1;
    EXPECT_TRUE(buffer.TryPushWFC(testData, dataSize));
    
    // Wait for completion (should timeout)
    int64_t slotIndex = 0;
    EXPECT_FALSE(buffer.WaitForCompletion(slotIndex, std::chrono::milliseconds(50)));
}

// ==============================================================================
// Edge Cases / 边界情况测试
// ==============================================================================

TEST(RingBufferTest, PushNullData) {
    RingBuffer<512, 16> buffer;
    buffer.Init();
    
    EXPECT_FALSE(buffer.TryPush(nullptr, 10));
}

TEST(RingBufferTest, PushZeroSize) {
    RingBuffer<512, 16> buffer;
    buffer.Init();
    
    const char* data = "test";
    EXPECT_FALSE(buffer.TryPush(data, 0));
}

TEST(RingBufferTest, PushTooLarge) {
    RingBuffer<512, 16> buffer;
    buffer.Init();
    
    std::vector<uint8_t> largeData(buffer.kMaxDataSize + 1, 0xFF);
    EXPECT_FALSE(buffer.TryPush(largeData.data(), largeData.size()));
}

TEST(RingBufferTest, PopNullBuffer) {
    RingBuffer<512, 16> buffer;
    buffer.Init();
    
    // Push data first
    const char* data = "test";
    buffer.TryPush(data, strlen(data) + 1);
    
    // Try to pop with null buffer
    size_t size = 100;
    EXPECT_FALSE(buffer.TryPop(nullptr, size));
}

TEST(RingBufferTest, PopZeroSize) {
    RingBuffer<512, 16> buffer;
    buffer.Init();
    
    // Push data first
    const char* data = "test";
    buffer.TryPush(data, strlen(data) + 1);
    
    // Try to pop with zero size
    char readBuffer[100];
    size_t size = 0;
    EXPECT_FALSE(buffer.TryPop(readBuffer, size));
}

TEST(RingBufferTest, WrapAround) {
    RingBuffer<512, 4> buffer;
    buffer.Init();
    
    // Fill and empty multiple times to test wrap-around
    for (int cycle = 0; cycle < 3; ++cycle) {
        // Fill
        for (int i = 0; i < 4; ++i) {
            std::string data = "Cycle " + std::to_string(cycle) + " Message " + std::to_string(i);
            EXPECT_TRUE(buffer.TryPush(data.c_str(), data.size() + 1));
        }
        
        // Empty
        for (int i = 0; i < 4; ++i) {
            char readBuffer[100];
            size_t readSize = sizeof(readBuffer);
            EXPECT_TRUE(buffer.TryPop(readBuffer, readSize));
            
            std::string expected = "Cycle " + std::to_string(cycle) + " Message " + std::to_string(i);
            EXPECT_STREQ(readBuffer, expected.c_str());
        }
        
        EXPECT_TRUE(buffer.IsEmpty());
    }
}

// ==============================================================================
// Concurrent Tests / 并发测试
// ==============================================================================

TEST(RingBufferTest, MultiProducerSingleConsumer) {
    RingBuffer<512, 1024> buffer;
    buffer.Init(QueueFullPolicy::Block);
    
    constexpr int kNumProducers = 4;
    constexpr int kMessagesPerProducer = 100;
    std::atomic<int> totalPushed{0};
    std::atomic<int> totalPopped{0};
    
    // Start producers
    std::vector<std::thread> producers;
    for (int p = 0; p < kNumProducers; ++p) {
        producers.emplace_back([&, p]() {
            for (int i = 0; i < kMessagesPerProducer; ++i) {
                std::string data = "P" + std::to_string(p) + "M" + std::to_string(i);
                if (buffer.TryPush(data.c_str(), data.size() + 1)) {
                    totalPushed++;
                }
            }
        });
    }
    
    // Start consumer
    std::thread consumer([&]() {
        while (totalPopped < kNumProducers * kMessagesPerProducer) {
            char readBuffer[100];
            size_t readSize = sizeof(readBuffer);
            if (buffer.TryPop(readBuffer, readSize)) {
                totalPopped++;
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    // Wait for completion
    for (auto& t : producers) {
        t.join();
    }
    consumer.join();
    
    EXPECT_EQ(totalPushed, kNumProducers * kMessagesPerProducer);
    EXPECT_EQ(totalPopped, kNumProducers * kMessagesPerProducer);
    EXPECT_TRUE(buffer.IsEmpty());
}

// ==============================================================================
// Performance Tests / 性能测试
// ==============================================================================

TEST(RingBufferTest, PushPopPerformance) {
    RingBuffer<512, 1024> buffer;
    buffer.Init();
    
    constexpr int kIterations = 10000;
    const char* testData = "Performance test message";
    size_t dataSize = strlen(testData) + 1;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < kIterations; ++i) {
        EXPECT_TRUE(buffer.TryPush(testData, dataSize));
        
        char readBuffer[100];
        size_t readSize = sizeof(readBuffer);
        EXPECT_TRUE(buffer.TryPop(readBuffer, readSize));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double avgTime = static_cast<double>(duration.count()) / kIterations;
    std::cout << "Average push+pop time: " << avgTime << " microseconds" << std::endl;
    
    // Should be very fast (< 1 microsecond per operation on modern hardware)
    EXPECT_LT(avgTime, 10.0);
}
