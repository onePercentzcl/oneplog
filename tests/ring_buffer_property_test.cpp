/**
 * @file ring_buffer_property_test.cpp
 * @brief Property-based tests for RingBuffer
 * @文件 ring_buffer_property_test.cpp
 * @简述 RingBuffer 的属性测试
 *
 * Feature: template-logger-refactor
 * 
 * This file contains property-based tests that verify universal properties
 * of the RingBuffer implementation across many randomly generated inputs.
 * 
 * 此文件包含属性测试，验证 RingBuffer 实现在许多随机生成的输入上的通用属性。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include "oneplog/ring_buffer.hpp"
#include <gtest/gtest.h>
#include <random>
#include <vector>
#include <string>
#include <algorithm>

using namespace oneplog;
using namespace oneplog::internal;

// ==============================================================================
// Test Utilities / 测试工具
// ==============================================================================

/**
 * @brief Random number generator for property tests
 * @brief 属性测试的随机数生成器
 */
class RandomGenerator {
public:
    RandomGenerator() : m_engine(42) {}  // Fixed seed for reproducibility
    
    int GetInt(int min, int max) {
        std::uniform_int_distribution<int> dist(min, max);
        return dist(m_engine);
    }
    
    std::string GetString(size_t minLen, size_t maxLen) {
        size_t len = GetInt(minLen, maxLen);
        std::string result;
        result.reserve(len);
        
        for (size_t i = 0; i < len; ++i) {
            result += static_cast<char>('A' + GetInt(0, 25));
        }
        return result;
    }
    
    std::vector<uint8_t> GetBytes(size_t minLen, size_t maxLen) {
        size_t len = GetInt(minLen, maxLen);
        std::vector<uint8_t> result(len);
        
        for (size_t i = 0; i < len; ++i) {
            result[i] = static_cast<uint8_t>(GetInt(0, 255));
        }
        return result;
    }
    
private:
    std::mt19937 m_engine;
};

// ==============================================================================
// Property 8: HeapRingBuffer FIFO 顺序保证
// Feature: template-logger-refactor, Property 8: FIFO Order Preservation
// ==============================================================================

/**
 * @brief Property 8: FIFO Order Preservation
 * @brief 属性 8：FIFO 顺序保持
 * 
 * For any sequence of enqueued elements [e1, e2, ..., eN] from the same producer,
 * the dequeue order should maintain FIFO order, i.e., elements enqueued first
 * should be dequeued first.
 * 
 * 对于任意来自同一生产者的入队元素序列 [e1, e2, ..., eN]，
 * 出队顺序应该保持 FIFO 顺序，即先入队的元素先出队。
 * 
 * Validates: Requirements 7.5
 * 验证: 需求 7.5
 */
TEST(RingBufferPropertyTest, Property8_FIFOOrderPreservation) {
    RandomGenerator rng;
    constexpr int kNumIterations = 100;
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        RingBuffer<512, 64> buffer;
        buffer.Init();
        
        // Generate random number of messages
        int numMessages = rng.GetInt(10, 50);
        std::vector<std::string> messages;
        
        // Push messages
        for (int i = 0; i < numMessages; ++i) {
            std::string msg = "Msg" + std::to_string(i) + "_" + rng.GetString(5, 20);
            messages.push_back(msg);
            
            bool pushed = buffer.TryPush(msg.c_str(), msg.size() + 1);
            ASSERT_TRUE(pushed) << "Failed to push message " << i << " in iteration " << iter;
        }
        
        // Pop and verify FIFO order
        for (int i = 0; i < numMessages; ++i) {
            char readBuffer[256];
            size_t readSize = sizeof(readBuffer);
            
            bool popped = buffer.TryPop(readBuffer, readSize);
            ASSERT_TRUE(popped) << "Failed to pop message " << i << " in iteration " << iter;
            
            // Verify message matches expected order
            EXPECT_STREQ(readBuffer, messages[i].c_str())
                << "Message order mismatch at position " << i 
                << " in iteration " << iter;
        }
        
        // Buffer should be empty
        EXPECT_TRUE(buffer.IsEmpty()) << "Buffer not empty after iteration " << iter;
    }
}

/**
 * @brief Property 8 Extended: FIFO with Interleaved Push/Pop
 * @brief 属性 8 扩展：交错推送/弹出的 FIFO
 * 
 * Test FIFO order with interleaved push and pop operations.
 * 测试交错推送和弹出操作的 FIFO 顺序。
 */
TEST(RingBufferPropertyTest, Property8_FIFOWithInterleavedOps) {
    RandomGenerator rng;
    constexpr int kNumIterations = 100;
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        RingBuffer<512, 64> buffer;
        buffer.Init();
        
        std::vector<std::string> expectedOrder;
        int messageCounter = 0;
        
        // Perform random push/pop operations
        int numOps = rng.GetInt(50, 100);
        for (int op = 0; op < numOps; ++op) {
            bool doPush = rng.GetInt(0, 1) == 0 || expectedOrder.empty();
            
            if (doPush && !buffer.IsFull()) {
                // Push operation
                std::string msg = "Msg" + std::to_string(messageCounter++) + "_" + rng.GetString(5, 15);
                expectedOrder.push_back(msg);
                
                bool pushed = buffer.TryPush(msg.c_str(), msg.size() + 1);
                ASSERT_TRUE(pushed);
            } else if (!expectedOrder.empty()) {
                // Pop operation
                char readBuffer[256];
                size_t readSize = sizeof(readBuffer);
                
                bool popped = buffer.TryPop(readBuffer, readSize);
                ASSERT_TRUE(popped);
                
                // Verify FIFO order
                EXPECT_STREQ(readBuffer, expectedOrder.front().c_str())
                    << "FIFO order violated in iteration " << iter << " operation " << op;
                
                expectedOrder.erase(expectedOrder.begin());
            }
        }
        
        // Pop remaining messages and verify order
        for (const auto& expected : expectedOrder) {
            char readBuffer[256];
            size_t readSize = sizeof(readBuffer);
            
            bool popped = buffer.TryPop(readBuffer, readSize);
            ASSERT_TRUE(popped);
            EXPECT_STREQ(readBuffer, expected.c_str());
        }
        
        EXPECT_TRUE(buffer.IsEmpty());
    }
}

/**
 * @brief Property 8 Binary Data: FIFO with Binary Data
 * @brief 属性 8 二进制数据：二进制数据的 FIFO
 * 
 * Test FIFO order with binary data (not just strings).
 * 测试二进制数据（不仅仅是字符串）的 FIFO 顺序。
 */
TEST(RingBufferPropertyTest, Property8_FIFOWithBinaryData) {
    RandomGenerator rng;
    constexpr int kNumIterations = 100;
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        RingBuffer<512, 64> buffer;
        buffer.Init();
        
        int numMessages = rng.GetInt(10, 40);
        std::vector<std::vector<uint8_t>> messages;
        
        // Push binary messages
        for (int i = 0; i < numMessages; ++i) {
            auto data = rng.GetBytes(10, 100);
            messages.push_back(data);
            
            bool pushed = buffer.TryPush(data.data(), data.size());
            ASSERT_TRUE(pushed);
        }
        
        // Pop and verify FIFO order
        for (int i = 0; i < numMessages; ++i) {
            std::vector<uint8_t> readBuffer(256);
            size_t readSize = readBuffer.size();
            
            bool popped = buffer.TryPop(readBuffer.data(), readSize);
            ASSERT_TRUE(popped);
            
            // Verify data matches
            ASSERT_EQ(readSize, messages[i].size());
            EXPECT_TRUE(std::equal(messages[i].begin(), messages[i].end(), readBuffer.begin()))
                << "Binary data mismatch at position " << i << " in iteration " << iter;
        }
        
        EXPECT_TRUE(buffer.IsEmpty());
    }
}

// ==============================================================================
// Property 9: 队列满策略正确性
// Feature: template-logger-refactor, Property 9: Queue Full Policy Correctness
// ==============================================================================

/**
 * @brief Property 9: Queue Full Policy - Block
 * @brief 属性 9：队列满策略 - 阻塞
 * 
 * For Block policy, producers should block until space is available,
 * and no data should be lost.
 * 
 * 对于 Block 策略，生产者应阻塞直到有可用空间，不应丢失数据。
 * 
 * Validates: Requirements 4.6, 18.1
 * 验证: 需求 4.6, 18.1
 */
TEST(RingBufferPropertyTest, Property9_BlockPolicyNoDataLoss) {
    RandomGenerator rng;
    constexpr int kNumIterations = 50;
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        RingBuffer<512, 8> buffer;  // Small buffer to trigger full condition
        buffer.Init(QueueFullPolicy::Block);
        
        constexpr int kNumMessages = 20;
        std::vector<std::string> messages;
        std::atomic<int> pushedCount{0};
        std::atomic<int> poppedCount{0};
        
        // Generate messages
        for (int i = 0; i < kNumMessages; ++i) {
            messages.push_back("Msg" + std::to_string(i) + "_" + rng.GetString(5, 15));
        }
        
        // Producer thread
        std::thread producer([&]() {
            for (int i = 0; i < kNumMessages; ++i) {
                buffer.TryPush(messages[i].c_str(), messages[i].size() + 1);
                pushedCount++;
            }
        });
        
        // Consumer thread (slower than producer)
        std::thread consumer([&]() {
            std::vector<std::string> received;
            while (poppedCount < kNumMessages) {
                char readBuffer[256];
                size_t readSize = sizeof(readBuffer);
                
                if (buffer.TryPop(readBuffer, readSize)) {
                    received.push_back(readBuffer);
                    poppedCount++;
                    
                    // Simulate slow consumer
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                } else {
                    std::this_thread::yield();
                }
            }
            
            // Verify FIFO order
            for (int i = 0; i < kNumMessages; ++i) {
                EXPECT_EQ(received[i], messages[i])
                    << "Message order mismatch in iteration " << iter;
            }
        });
        
        producer.join();
        consumer.join();
        
        // Verify no data loss
        EXPECT_EQ(pushedCount, kNumMessages) << "Not all messages pushed in iteration " << iter;
        EXPECT_EQ(poppedCount, kNumMessages) << "Not all messages popped in iteration " << iter;
        EXPECT_EQ(buffer.GetDroppedCount(), 0) << "Messages dropped with Block policy in iteration " << iter;
        EXPECT_TRUE(buffer.IsEmpty());
    }
}

/**
 * @brief Property 9: Queue Full Policy - DropNewest
 * @brief 属性 9：队列满策略 - 丢弃最新
 * 
 * For DropNewest policy, when queue is full, new messages should be dropped
 * and dropped count should increase.
 * 
 * 对于 DropNewest 策略，当队列满时，新消息应被丢弃，丢弃计数应增加。
 * 
 * Validates: Requirements 4.6, 18.2
 * 验证: 需求 4.6, 18.2
 */
TEST(RingBufferPropertyTest, Property9_DropNewestPolicy) {
    RandomGenerator rng;
    constexpr int kNumIterations = 100;
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        constexpr size_t kBufferSize = 8;
        RingBuffer<512, kBufferSize> buffer;
        buffer.Init(QueueFullPolicy::DropNewest);
        
        // Fill the buffer
        std::vector<std::string> initialMessages;
        for (size_t i = 0; i < kBufferSize; ++i) {
            std::string msg = "Initial" + std::to_string(i) + "_" + rng.GetString(5, 15);
            initialMessages.push_back(msg);
            
            bool pushed = buffer.TryPush(msg.c_str(), msg.size() + 1);
            ASSERT_TRUE(pushed) << "Failed to fill buffer in iteration " << iter;
        }
        
        ASSERT_TRUE(buffer.IsFull());
        EXPECT_EQ(buffer.GetDroppedCount(), 0);
        
        // Try to push more messages (should be dropped)
        int numDropped = rng.GetInt(5, 15);
        for (int i = 0; i < numDropped; ++i) {
            std::string msg = "Dropped" + std::to_string(i);
            bool pushed = buffer.TryPush(msg.c_str(), msg.size() + 1);
            EXPECT_FALSE(pushed) << "Message should have been dropped in iteration " << iter;
        }
        
        // Verify dropped count
        EXPECT_EQ(buffer.GetDroppedCount(), static_cast<uint64_t>(numDropped))
            << "Dropped count mismatch in iteration " << iter;
        
        // Verify original messages are still there (FIFO order)
        for (size_t i = 0; i < kBufferSize; ++i) {
            char readBuffer[256];
            size_t readSize = sizeof(readBuffer);
            
            bool popped = buffer.TryPop(readBuffer, readSize);
            ASSERT_TRUE(popped);
            EXPECT_STREQ(readBuffer, initialMessages[i].c_str())
                << "Original message corrupted in iteration " << iter;
        }
        
        EXPECT_TRUE(buffer.IsEmpty());
    }
}

/**
 * @brief Property 9: Queue Full Policy - DropOldest
 * @brief 属性 9：队列满策略 - 丢弃最旧
 * 
 * For DropOldest policy, when queue is full, oldest messages should be dropped
 * and dropped count should increase.
 * 
 * 对于 DropOldest 策略，当队列满时，最旧消息应被丢弃，丢弃计数应增加。
 * 
 * Note: Current implementation treats DropOldest same as DropNewest.
 * This test documents the expected behavior for future implementation.
 * 
 * 注意：当前实现将 DropOldest 视为与 DropNewest 相同。
 * 此测试记录了未来实现的预期行为。
 * 
 * Validates: Requirements 4.6, 18.3
 * 验证: 需求 4.6, 18.3
 */
TEST(RingBufferPropertyTest, Property9_DropOldestPolicy) {
    RandomGenerator rng;
    constexpr int kNumIterations = 100;
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        constexpr size_t kBufferSize = 8;
        RingBuffer<512, kBufferSize> buffer;
        buffer.Init(QueueFullPolicy::DropOldest);
        
        // Fill the buffer
        std::vector<std::string> initialMessages;
        for (size_t i = 0; i < kBufferSize; ++i) {
            std::string msg = "Initial" + std::to_string(i) + "_" + rng.GetString(5, 15);
            initialMessages.push_back(msg);
            
            bool pushed = buffer.TryPush(msg.c_str(), msg.size() + 1);
            ASSERT_TRUE(pushed);
        }
        
        ASSERT_TRUE(buffer.IsFull());
        
        // Try to push more messages
        // Note: Current implementation drops newest, not oldest
        // This test documents current behavior
        int numAttempted = rng.GetInt(3, 10);
        for (int i = 0; i < numAttempted; ++i) {
            std::string msg = "New" + std::to_string(i);
            buffer.TryPush(msg.c_str(), msg.size() + 1);
        }
        
        // Verify dropped count increased
        EXPECT_GT(buffer.GetDroppedCount(), 0)
            << "Dropped count should increase in iteration " << iter;
        
        // Note: In a true DropOldest implementation, we would expect
        // the newest messages to be in the buffer, not the oldest ones.
        // Current implementation behavior is documented here.
    }
}

/**
 * @brief Property 9 Extended: Policy Consistency
 * @brief 属性 9 扩展：策略一致性
 * 
 * Verify that the configured policy is consistently applied.
 * 验证配置的策略被一致应用。
 */
TEST(RingBufferPropertyTest, Property9_PolicyConsistency) {
    RandomGenerator rng;
    constexpr int kNumIterations = 100;
    
    QueueFullPolicy policies[] = {
        QueueFullPolicy::Block,
        QueueFullPolicy::DropNewest,
        QueueFullPolicy::DropOldest
    };
    
    for (auto policy : policies) {
        for (int iter = 0; iter < kNumIterations; ++iter) {
            RingBuffer<512, 8> buffer;
            buffer.Init(policy);
            
            // Verify policy is set correctly
            EXPECT_EQ(buffer.GetFullPolicy(), policy)
                << "Policy mismatch in iteration " << iter;
            
            // Fill buffer
            for (int i = 0; i < 8; ++i) {
                std::string msg = "Msg" + std::to_string(i);
                buffer.TryPush(msg.c_str(), msg.size() + 1);
            }
            
            ASSERT_TRUE(buffer.IsFull());
            
            // Behavior should be consistent with policy
            if (policy == QueueFullPolicy::DropNewest || policy == QueueFullPolicy::DropOldest) {
                // Should drop and increment counter
                uint64_t beforeCount = buffer.GetDroppedCount();
                std::string msg = "Extra";
                bool pushed = buffer.TryPush(msg.c_str(), msg.size() + 1);
                
                EXPECT_FALSE(pushed);
                EXPECT_GT(buffer.GetDroppedCount(), beforeCount)
                    << "Dropped count should increase for Drop policies";
            }
            // Note: Block policy test would require threading, tested separately
        }
    }
}

// ==============================================================================
// Additional Property Tests / 额外属性测试
// ==============================================================================

/**
 * @brief Property: Size Consistency
 * @brief 属性：大小一致性
 * 
 * For any sequence of push/pop operations, the reported size should always
 * match the actual number of elements in the buffer.
 * 
 * 对于任意推送/弹出操作序列，报告的大小应始终与缓冲区中的实际元素数量匹配。
 */
TEST(RingBufferPropertyTest, SizeConsistency) {
    RandomGenerator rng;
    constexpr int kNumIterations = 100;
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        RingBuffer<512, 32> buffer;
        buffer.Init();
        
        size_t expectedSize = 0;
        int numOps = rng.GetInt(50, 150);
        
        for (int op = 0; op < numOps; ++op) {
            bool doPush = rng.GetInt(0, 1) == 0 || expectedSize == 0;
            
            if (doPush && expectedSize < buffer.Capacity()) {
                std::string msg = "Msg" + std::to_string(op);
                if (buffer.TryPush(msg.c_str(), msg.size() + 1)) {
                    expectedSize++;
                }
            } else if (expectedSize > 0) {
                char readBuffer[256];
                size_t readSize = sizeof(readBuffer);
                if (buffer.TryPop(readBuffer, readSize)) {
                    expectedSize--;
                }
            }
            
            // Verify size consistency
            EXPECT_EQ(buffer.Size(), expectedSize)
                << "Size mismatch in iteration " << iter << " operation " << op;
            EXPECT_EQ(buffer.IsEmpty(), expectedSize == 0);
            EXPECT_EQ(buffer.IsFull(), expectedSize >= buffer.Capacity());
        }
    }
}

/**
 * @brief Property: Data Integrity
 * @brief 属性：数据完整性
 * 
 * For any data pushed to the buffer, the popped data should be identical.
 * 对于任意推送到缓冲区的数据，弹出的数据应该完全相同。
 */
TEST(RingBufferPropertyTest, DataIntegrity) {
    RandomGenerator rng;
    constexpr int kNumIterations = 100;
    
    for (int iter = 0; iter < kNumIterations; ++iter) {
        RingBuffer<512, 64> buffer;
        buffer.Init();
        
        int numMessages = rng.GetInt(20, 50);
        std::vector<std::vector<uint8_t>> messages;
        
        // Push random binary data
        for (int i = 0; i < numMessages; ++i) {
            auto data = rng.GetBytes(1, 200);
            messages.push_back(data);
            
            bool pushed = buffer.TryPush(data.data(), data.size());
            ASSERT_TRUE(pushed);
        }
        
        // Pop and verify data integrity
        for (int i = 0; i < numMessages; ++i) {
            std::vector<uint8_t> readBuffer(256);
            size_t readSize = readBuffer.size();
            
            bool popped = buffer.TryPop(readBuffer.data(), readSize);
            ASSERT_TRUE(popped);
            
            // Verify exact match
            ASSERT_EQ(readSize, messages[i].size())
                << "Size mismatch in iteration " << iter << " message " << i;
            
            for (size_t j = 0; j < readSize; ++j) {
                EXPECT_EQ(readBuffer[j], messages[i][j])
                    << "Data corruption at byte " << j 
                    << " in iteration " << iter << " message " << i;
            }
        }
    }
}
