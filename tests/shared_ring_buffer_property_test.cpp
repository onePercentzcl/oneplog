/**
 * @file shared_ring_buffer_property_test.cpp
 * @brief Property-based tests for SharedRingBuffer
 * @brief SharedRingBuffer 属性测试
 *
 * Feature: template-logger-refactor
 * Property 10: 多进程队列无数据竞争
 * 
 * *对于任意* 多进程并发写入共享内存队列的场景，所有日志条目应该被正确写入，
 * 不产生数据竞争或数据损坏。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <gtest/gtest.h>
#include "oneplog/shared_ring_buffer.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <random>
#include <set>
#include <cstring>

using namespace oneplog::internal;

// Test fixture for SharedRingBuffer property tests
// SharedRingBuffer 属性测试夹具
class SharedRingBufferPropertyTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up any leftover shared memory
        // 清理遗留的共享内存
#if !defined(_WIN32)
        shm_unlink("/test_srb_prop");
        shm_unlink("/test_srb_prop_multi");
#endif
    }

    void TearDown() override {
#if !defined(_WIN32)
        shm_unlink("/test_srb_prop");
        shm_unlink("/test_srb_prop_multi");
#endif
    }

    // Generate random data for testing
    // 生成随机测试数据
    std::string GenerateRandomData(std::mt19937& rng, size_t minLen, size_t maxLen) {
        std::uniform_int_distribution<size_t> lenDist(minLen, maxLen);
        std::uniform_int_distribution<int> charDist('A', 'Z');
        
        size_t len = lenDist(rng);
        std::string result;
        result.reserve(len);
        for (size_t i = 0; i < len; ++i) {
            result += static_cast<char>(charDist(rng));
        }
        return result;
    }
};


// ============================================================================
// Property 10: 多进程队列无数据竞争
// Multi-process queue data race freedom
// ============================================================================

/**
 * @brief Property 10.1: Concurrent writes from multiple threads preserve data integrity
 * @brief 属性 10.1: 多线程并发写入保持数据完整性
 * 
 * **Validates: Requirements 4.2**
 * 
 * For any number of concurrent writers, all written data should be readable
 * without corruption.
 * 对于任意数量的并发写入者，所有写入的数据应该可以无损读取。
 */
TEST_F(SharedRingBufferPropertyTest, Property10_ConcurrentWritesPreserveDataIntegrity) {
    constexpr int kIterations = 100;
    
    for (int iter = 0; iter < kIterations; ++iter) {
        auto buffer = DefaultSharedRingBuffer::Create("/test_srb_prop", QueueFullPolicy::Block);
        ASSERT_NE(buffer, nullptr);
        
        std::mt19937 rng(iter);
        constexpr int kNumThreads = 4;
        constexpr int kMessagesPerThread = 50;
        
        std::vector<std::set<std::string>> expectedMessages(kNumThreads);
        std::atomic<int> writeCount{0};
        
        // Producer threads
        // 生产者线程
        std::vector<std::thread> producers;
        for (int t = 0; t < kNumThreads; ++t) {
            producers.emplace_back([this, &buffer, &expectedMessages, &writeCount, t, &rng]() {
                std::mt19937 localRng(t * 1000 + rng());
                for (int i = 0; i < kMessagesPerThread; ++i) {
                    std::string msg = "T" + std::to_string(t) + "_" + std::to_string(i) + "_" +
                                      GenerateRandomData(localRng, 10, 50);
                    expectedMessages[t].insert(msg);
                    
                    while (!buffer->TryPush(msg.c_str(), msg.size() + 1)) {
                        std::this_thread::yield();
                    }
                    writeCount.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }
        
        // Consumer thread
        // 消费者线程
        std::set<std::string> readMessages;
        std::atomic<bool> stopConsumer{false};
        std::thread consumer([&buffer, &readMessages, &stopConsumer]() {
            while (!stopConsumer.load(std::memory_order_relaxed) || !buffer->IsEmpty()) {
                char readBuffer[512];
                size_t readSize = sizeof(readBuffer);
                if (buffer->TryPop(readBuffer, readSize)) {
                    readMessages.insert(std::string(readBuffer));
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
        
        // Verify all messages were received
        // 验证所有消息都已接收
        std::set<std::string> allExpected;
        for (const auto& msgs : expectedMessages) {
            allExpected.insert(msgs.begin(), msgs.end());
        }
        
        EXPECT_EQ(readMessages.size(), allExpected.size()) 
            << "Iteration " << iter << ": Message count mismatch";
        EXPECT_EQ(readMessages, allExpected) 
            << "Iteration " << iter << ": Message content mismatch";
    }
}


/**
 * @brief Property 10.2: Shared memory visibility across connections
 * @brief 属性 10.2: 共享内存跨连接可见性
 * 
 * **Validates: Requirements 4.2**
 * 
 * Data written by creator should be immediately visible to connectors.
 * 创建者写入的数据应该立即对连接者可见。
 */
TEST_F(SharedRingBufferPropertyTest, Property10_SharedMemoryVisibility) {
    constexpr int kIterations = 100;
    
    for (int iter = 0; iter < kIterations; ++iter) {
        auto creator = DefaultSharedRingBuffer::Create("/test_srb_prop_multi");
        ASSERT_NE(creator, nullptr);
        
        auto connector = DefaultSharedRingBuffer::Connect("/test_srb_prop_multi");
        ASSERT_NE(connector, nullptr);
        
        std::mt19937 rng(iter);
        std::string testData = "Iter" + std::to_string(iter) + "_" + 
                               GenerateRandomData(rng, 20, 100);
        
        // Creator writes
        // 创建者写入
        ASSERT_TRUE(creator->TryPush(testData.c_str(), testData.size() + 1));
        
        // Connector should see the data immediately
        // 连接者应该立即看到数据
        EXPECT_EQ(connector->Size(), 1) << "Iteration " << iter;
        
        // Connector reads
        // 连接者读取
        char readBuffer[512];
        size_t readSize = sizeof(readBuffer);
        ASSERT_TRUE(connector->TryPop(readBuffer, readSize));
        EXPECT_STREQ(readBuffer, testData.c_str()) << "Iteration " << iter;
        
        // Both should see empty now
        // 两者现在都应该看到空
        EXPECT_TRUE(creator->IsEmpty()) << "Iteration " << iter;
        EXPECT_TRUE(connector->IsEmpty()) << "Iteration " << iter;
    }
}

/**
 * @brief Property 10.3: No data corruption under high contention
 * @brief 属性 10.3: 高竞争下无数据损坏
 * 
 * **Validates: Requirements 4.2**
 * 
 * Under high contention with many threads, data should not be corrupted.
 * 在多线程高竞争下，数据不应该损坏。
 */
TEST_F(SharedRingBufferPropertyTest, Property10_NoDataCorruptionUnderHighContention) {
    constexpr int kIterations = 50;
    
    for (int iter = 0; iter < kIterations; ++iter) {
        // Use smaller buffer to increase contention
        // 使用较小的缓冲区以增加竞争
        auto buffer = SharedRingBuffer<256, 64>::Create("/test_srb_prop", QueueFullPolicy::DropNewest);
        ASSERT_NE(buffer, nullptr);
        
        constexpr int kNumProducers = 8;
        constexpr int kMessagesPerProducer = 100;
        
        std::atomic<int> successfulWrites{0};
        std::atomic<int> successfulReads{0};
        std::atomic<int> checksumErrors{0};
        std::atomic<bool> stopConsumer{false};
        
        // Multiple producer threads
        // 多个生产者线程
        std::vector<std::thread> producers;
        for (int t = 0; t < kNumProducers; ++t) {
            producers.emplace_back([&buffer, &successfulWrites, t]() {
                for (int i = 0; i < kMessagesPerProducer; ++i) {
                    // Create message with checksum pattern
                    // 创建带校验和模式的消息
                    char msg[64];
                    int len = snprintf(msg, sizeof(msg), "P%02d_M%04d_CHK", t, i);
                    
                    // Add simple checksum
                    // 添加简单校验和
                    uint8_t checksum = 0;
                    for (int j = 0; j < len; ++j) {
                        checksum ^= static_cast<uint8_t>(msg[j]);
                    }
                    msg[len] = static_cast<char>(checksum);
                    msg[len + 1] = '\0';
                    
                    if (buffer->TryPush(msg, len + 2)) {
                        successfulWrites.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            });
        }
        
        // Consumer thread with checksum verification
        // 带校验和验证的消费者线程
        std::thread consumer([&buffer, &successfulReads, &checksumErrors, &stopConsumer]() {
            while (!stopConsumer.load(std::memory_order_relaxed) || !buffer->IsEmpty()) {
                char readBuffer[256];
                size_t readSize = sizeof(readBuffer);
                if (buffer->TryPop(readBuffer, readSize)) {
                    // Verify checksum
                    // 验证校验和
                    if (readSize > 1) {
                        uint8_t expectedChecksum = static_cast<uint8_t>(readBuffer[readSize - 2]);
                        uint8_t actualChecksum = 0;
                        for (size_t j = 0; j < readSize - 2; ++j) {
                            actualChecksum ^= static_cast<uint8_t>(readBuffer[j]);
                        }
                        if (actualChecksum == expectedChecksum) {
                            successfulReads.fetch_add(1, std::memory_order_relaxed);
                        } else {
                            checksumErrors.fetch_add(1, std::memory_order_relaxed);
                        }
                    }
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
        
        // All successfully written messages should be successfully read with correct checksum
        // 所有成功写入的消息都应该成功读取且校验和正确
        int writes = successfulWrites.load();
        int reads = successfulReads.load();
        int errors = checksumErrors.load();
        
        // No checksum errors should occur (data integrity)
        // 不应该发生校验和错误（数据完整性）
        EXPECT_EQ(errors, 0) 
            << "Iteration " << iter << ": checksum errors detected";
        
        // All written messages should be read
        // 所有写入的消息都应该被读取
        EXPECT_EQ(reads, writes) 
            << "Iteration " << iter << ": reads=" << reads << ", writes=" << writes;
    }
}


/**
 * @brief Property 10.4: FIFO ordering preserved in single-producer scenario
 * @brief 属性 10.4: 单生产者场景下保持 FIFO 顺序
 * 
 * **Validates: Requirements 4.2**
 * 
 * In single-producer scenario, messages should be read in FIFO order.
 * 在单生产者场景下，消息应该按 FIFO 顺序读取。
 */
TEST_F(SharedRingBufferPropertyTest, Property10_FIFOOrderingPreserved) {
    constexpr int kIterations = 100;
    
    for (int iter = 0; iter < kIterations; ++iter) {
        auto buffer = DefaultSharedRingBuffer::Create("/test_srb_prop", QueueFullPolicy::Block);
        ASSERT_NE(buffer, nullptr);
        
        constexpr int kNumMessages = 100;
        
        // Write messages in order
        // 按顺序写入消息
        for (int i = 0; i < kNumMessages; ++i) {
            std::string msg = "MSG_" + std::to_string(i);
            while (!buffer->TryPush(msg.c_str(), msg.size() + 1)) {
                std::this_thread::yield();
            }
        }
        
        // Read and verify order
        // 读取并验证顺序
        for (int i = 0; i < kNumMessages; ++i) {
            char readBuffer[256];
            size_t readSize = sizeof(readBuffer);
            ASSERT_TRUE(buffer->TryPop(readBuffer, readSize)) 
                << "Iteration " << iter << ", message " << i;
            
            std::string expected = "MSG_" + std::to_string(i);
            EXPECT_STREQ(readBuffer, expected.c_str()) 
                << "Iteration " << iter << ", message " << i;
        }
    }
}

/**
 * @brief Property 10.5: Metadata validation prevents incompatible connections
 * @brief 属性 10.5: 元数据验证防止不兼容的连接
 * 
 * **Validates: Requirements 4.2**
 * 
 * Connecting with wrong template parameters should fail.
 * 使用错误的模板参数连接应该失败。
 */
TEST_F(SharedRingBufferPropertyTest, Property10_MetadataValidationPreventsIncompatibleConnections) {
    // Create with default parameters
    // 使用默认参数创建
    auto creator = DefaultSharedRingBuffer::Create("/test_srb_prop");
    ASSERT_NE(creator, nullptr);
    
    // Try to connect with different parameters - should fail
    // 尝试使用不同参数连接 - 应该失败
    auto wrongConnector = SmallSharedRingBuffer::Connect("/test_srb_prop");
    EXPECT_EQ(wrongConnector, nullptr) << "Should fail to connect with incompatible parameters";
    
    // Connect with correct parameters - should succeed
    // 使用正确参数连接 - 应该成功
    auto correctConnector = DefaultSharedRingBuffer::Connect("/test_srb_prop");
    EXPECT_NE(correctConnector, nullptr) << "Should succeed with compatible parameters";
}

/**
 * @brief Property 10.6: WFC works correctly across shared memory
 * @brief 属性 10.6: WFC 在共享内存中正确工作
 * 
 * **Validates: Requirements 4.2**
 * 
 * WaitForCompletion should work correctly when producer and consumer
 * access the buffer through different connections.
 * 当生产者和消费者通过不同连接访问缓冲区时，WaitForCompletion 应该正确工作。
 */
TEST_F(SharedRingBufferPropertyTest, Property10_WFCWorksAcrossConnections) {
    constexpr int kIterations = 50;
    
    for (int iter = 0; iter < kIterations; ++iter) {
        auto creator = DefaultSharedRingBuffer::Create("/test_srb_prop_multi", QueueFullPolicy::Block);
        ASSERT_NE(creator, nullptr);
        
        auto connector = DefaultSharedRingBuffer::Connect("/test_srb_prop_multi");
        ASSERT_NE(connector, nullptr);
        
        std::string testData = "WFC_Test_" + std::to_string(iter);
        
        // Producer pushes via creator
        // 生产者通过创建者推送
        int64_t slotIndex = creator->TryPushEx(testData.c_str(), testData.size() + 1);
        ASSERT_GE(slotIndex, 0) << "Iteration " << iter;
        
        // Consumer pops via connector in another thread
        // 消费者在另一个线程中通过连接者弹出
        std::thread consumer([&connector]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            char readBuffer[256];
            size_t readSize = sizeof(readBuffer);
            connector->TryPop(readBuffer, readSize);
        });
        
        // Wait for completion via creator
        // 通过创建者等待完成
        bool completed = creator->WaitForCompletion(slotIndex, std::chrono::milliseconds(1000));
        EXPECT_TRUE(completed) << "Iteration " << iter;
        
        consumer.join();
    }
}


/**
 * @brief Property 10.7: Buffer state consistency across connections
 * @brief 属性 10.7: 缓冲区状态跨连接一致性
 * 
 * **Validates: Requirements 4.2**
 * 
 * Buffer state (size, empty, full) should be consistent across all connections.
 * 缓冲区状态（大小、空、满）应该在所有连接中保持一致。
 */
TEST_F(SharedRingBufferPropertyTest, Property10_BufferStateConsistencyAcrossConnections) {
    constexpr int kIterations = 100;
    
    for (int iter = 0; iter < kIterations; ++iter) {
        auto creator = DefaultSharedRingBuffer::Create("/test_srb_prop_multi");
        ASSERT_NE(creator, nullptr);
        
        auto connector1 = DefaultSharedRingBuffer::Connect("/test_srb_prop_multi");
        auto connector2 = DefaultSharedRingBuffer::Connect("/test_srb_prop_multi");
        ASSERT_NE(connector1, nullptr);
        ASSERT_NE(connector2, nullptr);
        
        std::mt19937 rng(iter);
        std::uniform_int_distribution<int> numMsgDist(1, 50);
        int numMessages = numMsgDist(rng);
        
        // Push messages
        // 推送消息
        for (int i = 0; i < numMessages; ++i) {
            std::string msg = "M" + std::to_string(i);
            creator->TryPush(msg.c_str(), msg.size() + 1);
        }
        
        // All connections should see the same state
        // 所有连接应该看到相同的状态
        EXPECT_EQ(creator->Size(), connector1->Size()) << "Iteration " << iter;
        EXPECT_EQ(creator->Size(), connector2->Size()) << "Iteration " << iter;
        EXPECT_EQ(creator->IsEmpty(), connector1->IsEmpty()) << "Iteration " << iter;
        EXPECT_EQ(creator->IsEmpty(), connector2->IsEmpty()) << "Iteration " << iter;
        
        // Pop some messages
        // 弹出一些消息
        std::uniform_int_distribution<int> popDist(0, numMessages);
        int numPops = popDist(rng);
        for (int i = 0; i < numPops; ++i) {
            char readBuffer[256];
            size_t readSize = sizeof(readBuffer);
            connector1->TryPop(readBuffer, readSize);
        }
        
        // State should still be consistent
        // 状态应该仍然一致
        EXPECT_EQ(creator->Size(), connector1->Size()) << "Iteration " << iter;
        EXPECT_EQ(creator->Size(), connector2->Size()) << "Iteration " << iter;
    }
}

/**
 * @brief Property 10.8: Cleanup does not affect other connections
 * @brief 属性 10.8: 清理不影响其他连接
 * 
 * **Validates: Requirements 4.2**
 * 
 * When a connector is destroyed, other connections should continue to work.
 * 当一个连接器被销毁时，其他连接应该继续工作。
 */
TEST_F(SharedRingBufferPropertyTest, Property10_CleanupDoesNotAffectOtherConnections) {
    constexpr int kIterations = 50;
    
    for (int iter = 0; iter < kIterations; ++iter) {
        auto creator = DefaultSharedRingBuffer::Create("/test_srb_prop_multi");
        ASSERT_NE(creator, nullptr);
        
        // Create and destroy multiple connectors
        // 创建并销毁多个连接器
        for (int i = 0; i < 5; ++i) {
            auto connector = DefaultSharedRingBuffer::Connect("/test_srb_prop_multi");
            ASSERT_NE(connector, nullptr);
            
            std::string msg = "Iter" + std::to_string(iter) + "_Conn" + std::to_string(i);
            creator->TryPush(msg.c_str(), msg.size() + 1);
            
            // Connector goes out of scope here
            // 连接器在这里超出作用域
        }
        
        // Creator should still work
        // 创建者应该仍然工作
        EXPECT_EQ(creator->Size(), 5) << "Iteration " << iter;
        
        // New connector should still be able to connect
        // 新连接器应该仍然能够连接
        auto newConnector = DefaultSharedRingBuffer::Connect("/test_srb_prop_multi");
        ASSERT_NE(newConnector, nullptr);
        EXPECT_EQ(newConnector->Size(), 5) << "Iteration " << iter;
    }
}

/**
 * @brief Property 10.9: Random operations maintain invariants
 * @brief 属性 10.9: 随机操作维持不变量
 * 
 * **Validates: Requirements 4.2**
 * 
 * Random sequences of push/pop operations should maintain buffer invariants.
 * 随机的推送/弹出操作序列应该维持缓冲区不变量。
 */
TEST_F(SharedRingBufferPropertyTest, Property10_RandomOperationsMaintainInvariants) {
    constexpr int kIterations = 100;
    
    for (int iter = 0; iter < kIterations; ++iter) {
        auto buffer = DefaultSharedRingBuffer::Create("/test_srb_prop", QueueFullPolicy::DropNewest);
        ASSERT_NE(buffer, nullptr);
        
        std::mt19937 rng(iter);
        std::uniform_int_distribution<int> opDist(0, 1);  // 0 = push, 1 = pop
        std::uniform_int_distribution<int> sizeDist(1, 100);
        
        int expectedSize = 0;
        constexpr int kNumOperations = 200;
        
        for (int op = 0; op < kNumOperations; ++op) {
            if (opDist(rng) == 0) {
                // Push
                std::string msg = "Op" + std::to_string(op);
                if (buffer->TryPush(msg.c_str(), msg.size() + 1)) {
                    expectedSize++;
                }
            } else {
                // Pop
                char readBuffer[256];
                size_t readSize = sizeof(readBuffer);
                if (buffer->TryPop(readBuffer, readSize)) {
                    expectedSize--;
                }
            }
            
            // Invariants
            // 不变量
            EXPECT_GE(buffer->Size(), 0) << "Iteration " << iter << ", op " << op;
            EXPECT_LE(buffer->Size(), buffer->Capacity()) << "Iteration " << iter << ", op " << op;
            EXPECT_EQ(buffer->IsEmpty(), buffer->Size() == 0) << "Iteration " << iter << ", op " << op;
        }
    }
}
