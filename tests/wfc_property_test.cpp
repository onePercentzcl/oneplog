/**
 * @file wfc_property_test.cpp
 * @brief Property-based tests for WFC (Wait For Completion) mechanism
 * @文件 wfc_property_test.cpp
 * @简述 WFC（等待完成）机制的属性测试
 *
 * Feature: template-logger-refactor
 * 
 * This file contains property-based tests that verify the WFC mechanism
 * properties across many randomly generated inputs.
 * 
 * 此文件包含属性测试，验证 WFC 机制在许多随机生成的输入上的属性。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include "oneplog/ring_buffer.hpp"
#include "oneplog/heap_ring_buffer.hpp"
#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>

using namespace oneplog;
using namespace oneplog::internal;

// ==============================================================================
// Test Utilities / 测试工具
// ==============================================================================

class WFCRandomGenerator {
public:
    WFCRandomGenerator() : m_engine(42) {}  // Fixed seed for reproducibility
    
    int GetInt(int min, int max) {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(m_engine);
    }
    
    std::string GetString(size_t minLen, size_t maxLen) {
        size_t len = GetInt(static_cast<int>(minLen), static_cast<int>(maxLen));
        std::string result;
        result.reserve(len);
        for (size_t i = 0; i < len; ++i) {
            result += static_cast<char>('A' + GetInt(0, 25));
        }
        return result;
    }
    
private:
    std::mt19937 m_engine;
};


// ==============================================================================
// Property 19: WFC 完成保证
// Feature: template-logger-refactor, Property 19: WFC Completion Guarantee
// ==============================================================================

/**
 * @brief Property 19: WFC Completion Guarantee - Basic
 * @brief 属性 19：WFC 完成保证 - 基本
 * 
 * For any WFC log request (via TryPushEx to get slotIndex), after the consumer
 * processes that slot (m_processedTail > slotIndex), WaitForCompletion(slotIndex)
 * should return true.
 * 
 * 对于任意 WFC 日志请求（通过 TryPushEx 获取 slotIndex），在消费者处理完该槽位后
 * （m_processedTail > slotIndex），WaitForCompletion(slotIndex) 应该返回 true。
 * 
 * Validates: Requirements 12.2, 12.3, 12.4, 12.7, 12.8
 * 验证: 需求 12.2, 12.3, 12.4, 12.7, 12.8
 */
TEST(WFCPropertyTest, Property19_WFCCompletionGuarantee_Basic) {
    WFCRandomGenerator rng;
    constexpr int kNumIterations = 100;
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        RingBuffer<512, 64> buffer;
        buffer.Init();
        
        // Push with TryPushEx to get slot index
        std::string msg = "WFC_" + rng.GetString(5, 20);
        int64_t slotIndex = buffer.TryPushEx(msg.c_str(), msg.size() + 1);
        
        ASSERT_GE(slotIndex, 0) << "TryPushEx failed in iteration " << iter;
        
        // Verify processedTail is less than or equal to slotIndex before pop
        size_t processedBefore = buffer.GetProcessedTail();
        EXPECT_LE(processedBefore, static_cast<size_t>(slotIndex))
            << "ProcessedTail should be <= slotIndex before pop in iteration " << iter;
        
        // Consumer pops the message
        char readBuffer[256];
        size_t readSize = sizeof(readBuffer);
        bool popped = buffer.TryPop(readBuffer, readSize);
        ASSERT_TRUE(popped) << "TryPop failed in iteration " << iter;
        
        // Verify processedTail is now greater than slotIndex
        size_t processedAfter = buffer.GetProcessedTail();
        EXPECT_GT(processedAfter, static_cast<size_t>(slotIndex))
            << "ProcessedTail should be > slotIndex after pop in iteration " << iter;
        
        // WaitForCompletion should return true immediately
        bool completed = buffer.WaitForCompletion(slotIndex, std::chrono::milliseconds(10));
        EXPECT_TRUE(completed) 
            << "WaitForCompletion should return true after pop in iteration " << iter;
    }
}

/**
 * @brief Property 19: WFC Completion Guarantee - Concurrent
 * @brief 属性 19：WFC 完成保证 - 并发
 * 
 * Test WFC completion guarantee with concurrent producer and consumer.
 * 测试并发生产者和消费者的 WFC 完成保证。
 */
TEST(WFCPropertyTest, Property19_WFCCompletionGuarantee_Concurrent) {
    WFCRandomGenerator rng;
    constexpr int kNumIterations = 50;
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        RingBuffer<512, 64> buffer;
        buffer.Init();
        
        int numMessages = rng.GetInt(5, 20);
        std::vector<int64_t> slotIndices;
        std::atomic<int> completedCount{0};
        std::atomic<bool> consumerDone{false};
        
        // Producer: push messages and record slot indices
        std::thread producer([&]() {
            for (int i = 0; i < numMessages; ++i) {
                std::string msg = "Msg" + std::to_string(i) + "_" + rng.GetString(5, 15);
                int64_t slotIndex = buffer.TryPushEx(msg.c_str(), msg.size() + 1);
                
                if (slotIndex >= 0) {
                    slotIndices.push_back(slotIndex);
                }
            }
        });
        
        // Consumer: pop messages with small delay
        std::thread consumer([&]() {
            int consumed = 0;
            while (consumed < numMessages) {
                char readBuffer[256];
                size_t readSize = sizeof(readBuffer);
                
                if (buffer.TryPop(readBuffer, readSize)) {
                    consumed++;
                    // Small delay to simulate processing
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                } else {
                    std::this_thread::yield();
                }
            }
            consumerDone = true;
        });
        
        producer.join();
        consumer.join();
        
        // All WaitForCompletion calls should succeed now
        for (int64_t slotIndex : slotIndices) {
            bool completed = buffer.WaitForCompletion(slotIndex, std::chrono::milliseconds(100));
            EXPECT_TRUE(completed) 
                << "WaitForCompletion failed for slot " << slotIndex 
                << " in iteration " << iter;
            if (completed) {
                completedCount++;
            }
        }
        
        EXPECT_EQ(completedCount, static_cast<int>(slotIndices.size()))
            << "Not all WFC completed in iteration " << iter;
    }
}


/**
 * @brief Property 19: WFC Completion Guarantee - Multiple Slots
 * @brief 属性 19：WFC 完成保证 - 多槽位
 * 
 * Test WFC completion guarantee with multiple slots being tracked.
 * 测试跟踪多个槽位的 WFC 完成保证。
 */
TEST(WFCPropertyTest, Property19_WFCCompletionGuarantee_MultipleSlots) {
    WFCRandomGenerator rng;
    constexpr int kNumIterations = 50;
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        RingBuffer<512, 64> buffer;
        buffer.Init();
        
        int numMessages = rng.GetInt(10, 30);
        std::vector<int64_t> slotIndices;
        
        // Push multiple messages and record all slot indices
        for (int i = 0; i < numMessages; ++i) {
            std::string msg = "Msg" + std::to_string(i);
            int64_t slotIndex = buffer.TryPushEx(msg.c_str(), msg.size() + 1);
            ASSERT_GE(slotIndex, 0);
            slotIndices.push_back(slotIndex);
        }
        
        // Pop messages one by one and verify WFC for each
        for (int i = 0; i < numMessages; ++i) {
            char readBuffer[256];
            size_t readSize = sizeof(readBuffer);
            
            bool popped = buffer.TryPop(readBuffer, readSize);
            ASSERT_TRUE(popped);
            
            // WaitForCompletion for this slot should succeed
            bool completed = buffer.WaitForCompletion(slotIndices[i], std::chrono::milliseconds(10));
            EXPECT_TRUE(completed) 
                << "WaitForCompletion failed for slot " << i << " in iteration " << iter;
            
            // All previous slots should also be completed
            for (int j = 0; j <= i; ++j) {
                bool prevCompleted = buffer.WaitForCompletion(slotIndices[j], std::chrono::milliseconds(1));
                EXPECT_TRUE(prevCompleted) 
                    << "Previous slot " << j << " should be completed in iteration " << iter;
            }
        }
    }
}

/**
 * @brief Property 19: WFC Timeout Behavior
 * @brief 属性 19：WFC 超时行为
 * 
 * Test that WaitForCompletion returns false when timeout expires
 * and the slot has not been processed.
 * 
 * 测试当超时到期且槽位未被处理时，WaitForCompletion 返回 false。
 */
TEST(WFCPropertyTest, Property19_WFCTimeoutBehavior) {
    WFCRandomGenerator rng;
    constexpr int kNumIterations = 50;
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        RingBuffer<512, 64> buffer;
        buffer.Init();
        
        // Push message but don't consume
        std::string msg = "WFC_Timeout_" + rng.GetString(5, 15);
        int64_t slotIndex = buffer.TryPushEx(msg.c_str(), msg.size() + 1);
        ASSERT_GE(slotIndex, 0);
        
        // WaitForCompletion should timeout
        auto start = std::chrono::steady_clock::now();
        bool completed = buffer.WaitForCompletion(slotIndex, std::chrono::milliseconds(20));
        auto end = std::chrono::steady_clock::now();
        
        EXPECT_FALSE(completed) 
            << "WaitForCompletion should timeout in iteration " << iter;
        
        // Verify timeout duration is approximately correct
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        EXPECT_GE(duration.count(), 15) 
            << "Timeout too short in iteration " << iter;
        EXPECT_LE(duration.count(), 100) 
            << "Timeout too long in iteration " << iter;
        
        // Clean up: pop the message
        char readBuffer[256];
        size_t readSize = sizeof(readBuffer);
        buffer.TryPop(readBuffer, readSize);
    }
}

/**
 * @brief Property 19: Flush Waits For All Messages
 * @brief 属性 19：Flush 等待所有消息
 * 
 * Test that Flush waits for all pending messages to be processed.
 * 测试 Flush 等待所有待处理消息被处理。
 */
TEST(WFCPropertyTest, Property19_FlushWaitsForAll) {
    WFCRandomGenerator rng;
    constexpr int kNumIterations = 50;
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        RingBuffer<512, 64> buffer;
        buffer.Init();
        
        int numMessages = rng.GetInt(5, 20);
        
        // Push multiple messages
        for (int i = 0; i < numMessages; ++i) {
            std::string msg = "Flush_" + std::to_string(i);
            buffer.TryPush(msg.c_str(), msg.size() + 1);
        }
        
        std::atomic<bool> flushCompleted{false};
        
        // Consumer thread
        std::thread consumer([&]() {
            // Small delay before starting
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            
            for (int i = 0; i < numMessages; ++i) {
                char readBuffer[256];
                size_t readSize = sizeof(readBuffer);
                buffer.TryPop(readBuffer, readSize);
                
                // Small delay between pops
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        });
        
        // Flush should wait for all messages
        bool flushed = buffer.Flush(std::chrono::milliseconds(500));
        flushCompleted = true;
        
        consumer.join();
        
        EXPECT_TRUE(flushed) << "Flush failed in iteration " << iter;
        EXPECT_TRUE(buffer.IsEmpty()) << "Buffer not empty after flush in iteration " << iter;
    }
}


/**
 * @brief Property 19: ProcessedTail Monotonically Increasing
 * @brief 属性 19：ProcessedTail 单调递增
 * 
 * Test that m_processedTail is monotonically increasing as messages are consumed.
 * 测试 m_processedTail 随着消息被消费而单调递增。
 */
TEST(WFCPropertyTest, Property19_ProcessedTailMonotonic) {
    WFCRandomGenerator rng;
    constexpr int kNumIterations = 100;
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        RingBuffer<512, 64> buffer;
        buffer.Init();
        
        int numMessages = rng.GetInt(10, 40);
        
        // Push messages
        for (int i = 0; i < numMessages; ++i) {
            std::string msg = "Mono_" + std::to_string(i);
            buffer.TryPush(msg.c_str(), msg.size() + 1);
        }
        
        size_t prevProcessedTail = buffer.GetProcessedTail();
        
        // Pop messages and verify monotonic increase
        for (int i = 0; i < numMessages; ++i) {
            char readBuffer[256];
            size_t readSize = sizeof(readBuffer);
            
            bool popped = buffer.TryPop(readBuffer, readSize);
            ASSERT_TRUE(popped);
            
            size_t currentProcessedTail = buffer.GetProcessedTail();
            
            // ProcessedTail should be strictly greater than before
            EXPECT_GT(currentProcessedTail, prevProcessedTail)
                << "ProcessedTail not increasing at message " << i 
                << " in iteration " << iter;
            
            prevProcessedTail = currentProcessedTail;
        }
        
        // Final processedTail should equal numMessages
        EXPECT_EQ(buffer.GetProcessedTail(), static_cast<size_t>(numMessages))
            << "Final processedTail mismatch in iteration " << iter;
    }
}

/**
 * @brief Property 19: WFC with HeapRingBuffer
 * @brief 属性 19：HeapRingBuffer 的 WFC
 * 
 * Test WFC completion guarantee with HeapRingBuffer.
 * 测试 HeapRingBuffer 的 WFC 完成保证。
 */
TEST(WFCPropertyTest, Property19_WFCWithHeapRingBuffer) {
    WFCRandomGenerator rng;
    constexpr int kNumIterations = 50;
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        auto buffer = HeapRingBuffer<512, 64>::Create();
        ASSERT_NE(buffer, nullptr);
        
        int numMessages = rng.GetInt(5, 20);
        std::vector<int64_t> slotIndices;
        
        // Push messages
        for (int i = 0; i < numMessages; ++i) {
            std::string msg = "Heap_" + std::to_string(i) + "_" + rng.GetString(5, 15);
            int64_t slotIndex = buffer->TryPushEx(msg.c_str(), msg.size() + 1);
            ASSERT_GE(slotIndex, 0);
            slotIndices.push_back(slotIndex);
        }
        
        // Pop and verify WFC
        for (int i = 0; i < numMessages; ++i) {
            char readBuffer[256];
            size_t readSize = sizeof(readBuffer);
            
            bool popped = buffer->TryPop(readBuffer, readSize);
            ASSERT_TRUE(popped);
            
            // WaitForCompletion should succeed
            bool completed = buffer->WaitForCompletion(slotIndices[i], std::chrono::milliseconds(10));
            EXPECT_TRUE(completed) 
                << "HeapRingBuffer WFC failed for slot " << i << " in iteration " << iter;
        }
    }
}

/**
 * @brief Property 19: WFC Slot Reuse Safety
 * @brief 属性 19：WFC 槽位重用安全性
 * 
 * Test that WFC works correctly even when slots are reused after wrap-around.
 * 测试即使在环绕后槽位被重用，WFC 也能正确工作。
 */
TEST(WFCPropertyTest, Property19_WFCSlotReuseSafety) {
    WFCRandomGenerator rng;
    constexpr int kNumIterations = 30;
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        constexpr size_t kBufferSize = 16;  // Larger buffer to avoid full condition
        RingBuffer<512, kBufferSize> buffer;
        buffer.Init(QueueFullPolicy::Block);  // Use Block policy to ensure push succeeds
        
        // Fill and empty buffer multiple times to cause wrap-around
        int numCycles = rng.GetInt(3, 6);
        int messagesPerCycle = rng.GetInt(5, 10);  // Less than buffer size
        
        for (int cycle = 0; cycle < numCycles; ++cycle) {
            std::vector<int64_t> slotIndices;
            
            // Push messages (less than buffer size)
            for (int i = 0; i < messagesPerCycle; ++i) {
                std::string msg = "Cycle" + std::to_string(cycle) + "_" + std::to_string(i);
                int64_t slotIndex = buffer.TryPushEx(msg.c_str(), msg.size() + 1);
                ASSERT_GE(slotIndex, 0) 
                    << "TryPushEx failed in cycle " << cycle << " message " << i 
                    << " in iteration " << iter;
                slotIndices.push_back(slotIndex);
            }
            
            // Pop messages and verify WFC
            for (int i = 0; i < messagesPerCycle; ++i) {
                char readBuffer[256];
                size_t readSize = sizeof(readBuffer);
                
                bool popped = buffer.TryPop(readBuffer, readSize);
                ASSERT_TRUE(popped);
                
                // WFC should work correctly
                bool completed = buffer.WaitForCompletion(slotIndices[i], std::chrono::milliseconds(10));
                EXPECT_TRUE(completed) 
                    << "WFC failed in cycle " << cycle << " slot " << i 
                    << " in iteration " << iter;
            }
            
            EXPECT_TRUE(buffer.IsEmpty());
        }
    }
}

/**
 * @brief Property 19: Invalid Slot Index Handling
 * @brief 属性 19：无效槽位索引处理
 * 
 * Test that WaitForCompletion handles invalid slot indices correctly.
 * 测试 WaitForCompletion 正确处理无效槽位索引。
 */
TEST(WFCPropertyTest, Property19_InvalidSlotIndexHandling) {
    WFCRandomGenerator rng;
    constexpr int kNumIterations = 50;
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        RingBuffer<512, 64> buffer;
        buffer.Init();
        
        // Test with negative slot index
        bool completed = buffer.WaitForCompletion(-1, std::chrono::milliseconds(10));
        EXPECT_FALSE(completed) 
            << "WaitForCompletion should return false for -1 in iteration " << iter;
        
        // Push some messages
        int numMessages = rng.GetInt(5, 15);
        for (int i = 0; i < numMessages; ++i) {
            std::string msg = "Msg" + std::to_string(i);
            buffer.TryPush(msg.c_str(), msg.size() + 1);
        }
        
        // Pop all messages
        for (int i = 0; i < numMessages; ++i) {
            char readBuffer[256];
            size_t readSize = sizeof(readBuffer);
            buffer.TryPop(readBuffer, readSize);
        }
        
        // WaitForCompletion for already processed slots should succeed
        for (int i = 0; i < numMessages; ++i) {
            bool completed = buffer.WaitForCompletion(i, std::chrono::milliseconds(1));
            EXPECT_TRUE(completed) 
                << "WaitForCompletion should succeed for processed slot " << i 
                << " in iteration " << iter;
        }
    }
}
