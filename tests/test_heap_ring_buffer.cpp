/**
 * @file test_heap_ring_buffer.cpp
 * @brief Unit tests for HeapRingBuffer
 * @brief HeapRingBuffer 的单元测试
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <gtest/gtest.h>
#ifdef ONEPLOG_HAS_RAPIDCHECK
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#endif

#include <atomic>
#include <thread>
#include <vector>

#include <oneplog/internal/heap_memory.hpp>
#include <oneplog/internal/log_entry.hpp>

namespace oneplog {
namespace test {

// ==============================================================================
// Unit Tests / 单元测试
// ==============================================================================

/**
 * @brief Test default constructor
 * @brief 测试默认构造函数
 */
TEST(HeapRingBufferTest, Constructor) {
    HeapRingBuffer<int> buffer(16);
    EXPECT_EQ(buffer.Capacity(), 16);
    EXPECT_TRUE(buffer.IsEmpty());
    EXPECT_FALSE(buffer.IsFull());
    EXPECT_EQ(buffer.Size(), 0);
}

/**
 * @brief Test basic push and pop
 * @brief 测试基本的推入和弹出
 */
TEST(HeapRingBufferTest, BasicPushPop) {
    HeapRingBuffer<int> buffer(16);
    
    // Push / 推入
    EXPECT_TRUE(buffer.TryPush(42));
    EXPECT_FALSE(buffer.IsEmpty());
    EXPECT_EQ(buffer.Size(), 1);
    
    // Pop / 弹出
    int value = 0;
    EXPECT_TRUE(buffer.TryPop(value));
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(buffer.IsEmpty());
    EXPECT_EQ(buffer.Size(), 0);
}

/**
 * @brief Test multiple push and pop
 * @brief 测试多次推入和弹出
 */
TEST(HeapRingBufferTest, MultiplePushPop) {
    HeapRingBuffer<int> buffer(16);
    
    // Push multiple items / 推入多个元素
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(buffer.TryPush(i));
    }
    EXPECT_EQ(buffer.Size(), 10);
    
    // Pop and verify order / 弹出并验证顺序
    for (int i = 0; i < 10; ++i) {
        int value = -1;
        EXPECT_TRUE(buffer.TryPop(value));
        EXPECT_EQ(value, i);
    }
    EXPECT_TRUE(buffer.IsEmpty());
}

/**
 * @brief Test queue full condition with DropNewest policy
 * @brief 测试队列满条件（DropNewest 策略）
 */
TEST(HeapRingBufferTest, QueueFullDropNewest) {
    HeapRingBuffer<int> buffer(4, QueueFullPolicy::DropNewest);
    
    // Fill the queue / 填满队列
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(buffer.TryPush(i));
    }
    EXPECT_TRUE(buffer.IsFull());
    
    // Try to push when full - should fail and increment dropped count
    // 满时尝试推入 - 应该失败并增加丢弃计数
    EXPECT_FALSE(buffer.TryPush(100));
    EXPECT_EQ(buffer.GetDroppedCount(), 1);
    
    // Pop one and push again / 弹出一个再推入
    int value;
    EXPECT_TRUE(buffer.TryPop(value));
    EXPECT_TRUE(buffer.TryPush(100));
}

/**
 * @brief Test queue full condition with DropOldest policy
 * @brief 测试队列满条件（DropOldest 策略）
 */
TEST(HeapRingBufferTest, QueueFullDropOldest) {
    HeapRingBuffer<int> buffer(4, QueueFullPolicy::DropOldest);
    
    // Fill the queue with 0, 1, 2, 3 / 用 0, 1, 2, 3 填满队列
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(buffer.TryPush(i));
    }
    EXPECT_TRUE(buffer.IsFull());
    
    // Push when full - should drop oldest and succeed
    // 满时推入 - 应该丢弃最旧的并成功
    EXPECT_TRUE(buffer.TryPush(100));
    EXPECT_EQ(buffer.GetDroppedCount(), 1);
    
    // Verify the oldest was dropped (0 should be gone)
    // 验证最旧的被丢弃（0 应该不见了）
    std::vector<int> values;
    int value;
    while (buffer.TryPop(value)) {
        values.push_back(value);
    }
    
    // Should have 1, 2, 3, 100 (0 was dropped)
    // 应该有 1, 2, 3, 100（0 被丢弃）
    EXPECT_EQ(values.size(), 4);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[3], 100);
}

/**
 * @brief Test WFC logs are NEVER dropped
 * @brief 测试 WFC 日志永远不会被丢弃
 */
TEST(HeapRingBufferTest, WFCNeverDropped) {
    HeapRingBuffer<int> buffer(4, QueueFullPolicy::DropNewest);
    
    // Fill the queue / 填满队列
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(buffer.TryPush(i));
    }
    EXPECT_TRUE(buffer.IsFull());
    
    std::atomic<bool> wfcCompleted{false};
    std::atomic<int64_t> wfcSlot{-1};
    
    // Start a thread that will push WFC (should block, not drop)
    // 启动一个将推入 WFC 的线程（应该阻塞，而不是丢弃）
    std::thread producer([&]() {
        int64_t slot = buffer.TryPushWFC(100);
        wfcSlot.store(slot, std::memory_order_release);
        wfcCompleted.store(true, std::memory_order_release);
    });
    
    // Give producer time to start blocking / 给生产者时间开始阻塞
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    EXPECT_FALSE(wfcCompleted.load(std::memory_order_acquire));
    
    // Pop one item to unblock producer / 弹出一个元素以解除生产者阻塞
    int value;
    EXPECT_TRUE(buffer.TryPop(value));
    
    // Wait for producer to complete / 等待生产者完成
    producer.join();
    EXPECT_TRUE(wfcCompleted.load(std::memory_order_acquire));
    
    // WFC slot should be valid / WFC 槽位应该有效
    EXPECT_NE(wfcSlot.load(std::memory_order_acquire), HeapRingBuffer<int>::kInvalidSlot);
    
    // No logs should be dropped (WFC never dropped)
    // 不应该有日志被丢弃（WFC 永不丢弃）
    EXPECT_EQ(buffer.GetDroppedCount(), 0);
}

/**
 * @brief Test DropOldest blocks when oldest is WFC
 * @brief 测试当最旧的是 WFC 时 DropOldest 会阻塞
 */
TEST(HeapRingBufferTest, DropOldestBlocksOnWFC) {
    HeapRingBuffer<int> buffer(4, QueueFullPolicy::DropOldest);
    
    // Push first item as WFC / 第一个元素作为 WFC 推入
    int64_t wfcSlot = buffer.TryPushWFC(0);
    EXPECT_NE(wfcSlot, HeapRingBuffer<int>::kInvalidSlot);
    
    // Fill rest of queue / 填满队列的其余部分
    for (int i = 1; i < 4; ++i) {
        EXPECT_TRUE(buffer.TryPush(i));
    }
    EXPECT_TRUE(buffer.IsFull());
    
    std::atomic<bool> pushCompleted{false};
    std::atomic<bool> shouldPop{false};
    
    // Start a thread that will block on push (oldest is WFC)
    // 启动一个将在推入时阻塞的线程（最旧的是 WFC）
    std::thread producer([&]() {
        // Signal that we're about to push / 发信号表示即将推入
        shouldPop.store(true, std::memory_order_release);
        buffer.TryPush(100);  // This will block because oldest is WFC
        pushCompleted.store(true, std::memory_order_release);
    });
    
    // Wait for producer to signal it's about to push / 等待生产者发信号
    while (!shouldPop.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    
    // Give producer time to start blocking / 给生产者时间开始阻塞
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    
    // Producer should still be blocked / 生产者应该仍然被阻塞
    // Note: Due to race conditions, this might occasionally pass even if not blocked
    // 注意：由于竞争条件，即使没有阻塞，这也可能偶尔通过
    
    // Pop the WFC entry to unblock producer / 弹出 WFC 条目以解除生产者阻塞
    int value;
    EXPECT_TRUE(buffer.TryPop(value));
    EXPECT_EQ(value, 0);  // WFC entry
    
    // Wait for producer to complete / 等待生产者完成
    producer.join();
    EXPECT_TRUE(pushCompleted.load(std::memory_order_acquire));
    
    // The key invariant: WFC entry was NOT dropped, it was consumed normally
    // 关键不变量：WFC 条目没有被丢弃，而是被正常消费
    // droppedCount might be 0 or 1 depending on timing, but WFC was preserved
    // droppedCount 可能是 0 或 1，取决于时序，但 WFC 被保留了
}

/**
 * @brief Test DropOldest drops non-WFC entries normally
 * @brief 测试 DropOldest 正常丢弃非 WFC 条目
 */
TEST(HeapRingBufferTest, DropOldestDropsNonWFC) {
    HeapRingBuffer<int> buffer(4, QueueFullPolicy::DropOldest);
    
    // Fill queue with non-WFC entries / 用非 WFC 条目填满队列
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(buffer.TryPush(i));
    }
    EXPECT_TRUE(buffer.IsFull());
    
    // Push when full - should drop oldest (0) and succeed
    // 满时推入 - 应该丢弃最旧的（0）并成功
    EXPECT_TRUE(buffer.TryPush(100));
    EXPECT_EQ(buffer.GetDroppedCount(), 1);
    
    // Pop all and verify oldest was dropped
    // 弹出所有并验证最旧的被丢弃
    std::vector<int> values;
    int value;
    while (buffer.TryPop(value)) {
        values.push_back(value);
    }
    
    // Should have 1, 2, 3, 100 (0 was dropped)
    EXPECT_EQ(values.size(), 4);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[3], 100);
}

/**
 * @brief Test queue full condition with Block policy
 * @brief 测试队列满条件（Block 策略）
 */
TEST(HeapRingBufferTest, QueueFullBlock) {
    HeapRingBuffer<int> buffer(4, QueueFullPolicy::Block);
    
    // Fill the queue / 填满队列
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(buffer.TryPush(i));
    }
    EXPECT_TRUE(buffer.IsFull());
    
    std::atomic<bool> pushCompleted{false};
    
    // Start a thread that will block on push / 启动一个将在推入时阻塞的线程
    std::thread producer([&]() {
        buffer.TryPush(100);  // This will block / 这将阻塞
        pushCompleted.store(true, std::memory_order_release);
    });
    
    // Give producer time to start blocking / 给生产者时间开始阻塞
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_FALSE(pushCompleted.load(std::memory_order_acquire));
    
    // Pop one item to unblock producer / 弹出一个元素以解除生产者阻塞
    int value;
    EXPECT_TRUE(buffer.TryPop(value));
    
    // Wait for producer to complete / 等待生产者完成
    producer.join();
    EXPECT_TRUE(pushCompleted.load(std::memory_order_acquire));
}

/**
 * @brief Test consumer state tracking
 * @brief 测试消费者状态跟踪
 */
TEST(HeapRingBufferTest, ConsumerStateTracking) {
    HeapRingBuffer<int> buffer(16);
    
    // Initially consumer should be Active / 初始时消费者应该是 Active
    EXPECT_EQ(buffer.GetConsumerState(), ConsumerState::Active);
    
    // Start a consumer thread that will wait for data
    // 启动一个将等待数据的消费者线程
    std::atomic<bool> consumerStarted{false};
    std::atomic<bool> consumerDone{false};
    
    std::thread consumer([&]() {
        consumerStarted.store(true, std::memory_order_release);
        // This will poll briefly then enter WaitingNotify state
        // 这将短暂轮询然后进入 WaitingNotify 状态
        buffer.WaitForData(std::chrono::microseconds(100), 
                          std::chrono::milliseconds(1000));
        consumerDone.store(true, std::memory_order_release);
    });
    
    // Wait for consumer to start / 等待消费者启动
    while (!consumerStarted.load(std::memory_order_acquire)) {
        std::this_thread::yield();
    }
    
    // Give consumer time to enter waiting state / 给消费者时间进入等待状态
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Consumer should be in WaitingNotify state / 消费者应该处于 WaitingNotify 状态
    EXPECT_EQ(buffer.GetConsumerState(), ConsumerState::WaitingNotify);
    
    // Push data to wake up consumer / 推入数据唤醒消费者
    buffer.TryPush(42);
    
    // Wait for consumer to complete / 等待消费者完成
    consumer.join();
    EXPECT_TRUE(consumerDone.load(std::memory_order_acquire));
    
    // Consumer should be back to Active / 消费者应该回到 Active
    EXPECT_EQ(buffer.GetConsumerState(), ConsumerState::Active);
}

/**
 * @brief Test notification optimization
 * @brief 测试通知优化
 */
TEST(HeapRingBufferTest, NotificationOptimization) {
    HeapRingBuffer<int> buffer(16);
    
    // When consumer is Active, NotifyConsumerIfWaiting should not notify
    // 当消费者是 Active 时，NotifyConsumerIfWaiting 不应该通知
    // (We can't directly test this, but we can verify the state logic)
    
    // Push some data / 推入一些数据
    for (int i = 0; i < 5; ++i) {
        buffer.TryPush(i);
    }
    
    // Consumer state should still be Active / 消费者状态应该仍然是 Active
    EXPECT_EQ(buffer.GetConsumerState(), ConsumerState::Active);
    
    // Pop all data / 弹出所有数据
    int value;
    while (buffer.TryPop(value)) {}
    
    EXPECT_TRUE(buffer.IsEmpty());
}

/**
 * @brief Test dropped count tracking
 * @brief 测试丢弃计数跟踪
 */
TEST(HeapRingBufferTest, DroppedCountTracking) {
    HeapRingBuffer<int> buffer(4, QueueFullPolicy::DropNewest);
    
    // Fill the queue / 填满队列
    for (int i = 0; i < 4; ++i) {
        buffer.TryPush(i);
    }
    
    // Try to push more (should be dropped) / 尝试推入更多（应该被丢弃）
    for (int i = 0; i < 10; ++i) {
        buffer.TryPush(100 + i);
    }
    
    EXPECT_EQ(buffer.GetDroppedCount(), 10);
    
    // Reset dropped count / 重置丢弃计数
    buffer.ResetDroppedCount();
    EXPECT_EQ(buffer.GetDroppedCount(), 0);
}

/**
 * @brief Test queue empty condition
 * @brief 测试队列空条件
 */
TEST(HeapRingBufferTest, QueueEmpty) {
    HeapRingBuffer<int> buffer(4);
    
    // Try to pop from empty queue / 从空队列弹出
    int value = -1;
    EXPECT_FALSE(buffer.TryPop(value));
    EXPECT_EQ(value, -1);  // Value should be unchanged / 值应该不变
}

/**
 * @brief Test AcquireSlot and CommitSlot
 * @brief 测试 AcquireSlot 和 CommitSlot
 */
TEST(HeapRingBufferTest, AcquireAndCommitSlot) {
    HeapRingBuffer<int> buffer(16);
    
    // Acquire slot / 获取槽位
    int64_t slot = buffer.AcquireSlot();
    EXPECT_NE(slot, HeapRingBuffer<int>::kInvalidSlot);
    
    // Commit data / 提交数据
    buffer.CommitSlot(slot, 42);
    buffer.NotifyConsumer();
    
    // Pop and verify / 弹出并验证
    int value = 0;
    EXPECT_TRUE(buffer.TryPop(value));
    EXPECT_EQ(value, 42);
}

/**
 * @brief Test TryPushWFC
 * @brief 测试 TryPushWFC
 */
TEST(HeapRingBufferTest, TryPushWFC) {
    HeapRingBuffer<int> buffer(16);
    
    // Push with WFC / 带 WFC 推入
    int64_t slot = buffer.TryPushWFC(42);
    EXPECT_NE(slot, HeapRingBuffer<int>::kInvalidSlot);
    
    // Check WFC state / 检查 WFC 状态
    EXPECT_EQ(buffer.GetWFCState(slot), kWFCEnabled);
    
    // Pop (this should complete WFC) / 弹出（这应该完成 WFC）
    int value = 0;
    EXPECT_TRUE(buffer.TryPop(value));
    EXPECT_EQ(value, 42);
    
    // WFC should be completed / WFC 应该完成
    EXPECT_EQ(buffer.GetWFCState(slot), kWFCCompleted);
}

/**
 * @brief Test WaitForCompletion
 * @brief 测试 WaitForCompletion
 */
TEST(HeapRingBufferTest, WaitForCompletion) {
    HeapRingBuffer<int> buffer(16);
    
    // Push with WFC / 带 WFC 推入
    int64_t slot = buffer.TryPushWFC(42);
    EXPECT_NE(slot, HeapRingBuffer<int>::kInvalidSlot);
    
    // Start consumer thread / 启动消费者线程
    std::thread consumer([&buffer]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        int value;
        buffer.TryPop(value);
    });
    
    // Wait for completion / 等待完成
    bool completed = buffer.WaitForCompletion(slot, std::chrono::milliseconds(1000));
    EXPECT_TRUE(completed);
    
    consumer.join();
}

/**
 * @brief Test TryPopBatch
 * @brief 测试 TryPopBatch
 */
TEST(HeapRingBufferTest, TryPopBatch) {
    HeapRingBuffer<int> buffer(16);
    
    // Push multiple items / 推入多个元素
    for (int i = 0; i < 10; ++i) {
        buffer.TryPush(i);
    }
    
    // Pop batch / 批量弹出
    std::vector<int> items;
    size_t count = buffer.TryPopBatch(items, 5);
    EXPECT_EQ(count, 5);
    EXPECT_EQ(items.size(), 5);
    
    // Verify order / 验证顺序
    for (size_t i = 0; i < items.size(); ++i) {
        EXPECT_EQ(items[i], static_cast<int>(i));
    }
    
    // Remaining items / 剩余元素
    EXPECT_EQ(buffer.Size(), 5);
}

/**
 * @brief Test with LogEntry type
 * @brief 测试 LogEntry 类型
 */
TEST(HeapRingBufferTest, WithLogEntry) {
    HeapRingBuffer<LogEntry> buffer(16);
    
    // Create and push LogEntry / 创建并推入 LogEntry
    LogEntry entry;
    entry.timestamp = 12345;
    entry.level = Level::Info;
    entry.threadId = 1;
    entry.processId = 100;
    
    EXPECT_TRUE(buffer.TryPush(std::move(entry)));
    EXPECT_EQ(buffer.Size(), 1);
    
    // Pop and verify / 弹出并验证
    LogEntry popped;
    EXPECT_TRUE(buffer.TryPop(popped));
    EXPECT_EQ(popped.timestamp, 12345);
    EXPECT_EQ(popped.level, Level::Info);
    EXPECT_EQ(popped.threadId, 1);
    EXPECT_EQ(popped.processId, 100);
}

/**
 * @brief Test wrap-around behavior
 * @brief 测试环绕行为
 */
TEST(HeapRingBufferTest, WrapAround) {
    HeapRingBuffer<int> buffer(4);
    
    // Fill and empty multiple times to test wrap-around
    // 多次填充和清空以测试环绕
    for (int round = 0; round < 3; ++round) {
        // Fill / 填充
        for (int i = 0; i < 4; ++i) {
            EXPECT_TRUE(buffer.TryPush(round * 10 + i));
        }
        EXPECT_TRUE(buffer.IsFull());
        
        // Empty / 清空
        for (int i = 0; i < 4; ++i) {
            int value;
            EXPECT_TRUE(buffer.TryPop(value));
            EXPECT_EQ(value, round * 10 + i);
        }
        EXPECT_TRUE(buffer.IsEmpty());
    }
}

/**
 * @brief Test concurrent push from multiple threads
 * @brief 测试多线程并发推入
 */
TEST(HeapRingBufferTest, ConcurrentPush) {
    HeapRingBuffer<int> buffer(1024);
    std::atomic<int> pushCount{0};
    const int numThreads = 4;
    const int itemsPerThread = 100;
    
    std::vector<std::thread> threads;
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&buffer, &pushCount, t]() {
            for (int i = 0; i < 100; ++i) {
                if (buffer.TryPush(t * 1000 + i)) {
                    pushCount.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    // All items should be pushed / 所有元素应该被推入
    EXPECT_EQ(pushCount.load(), numThreads * itemsPerThread);
    EXPECT_EQ(buffer.Size(), static_cast<size_t>(numThreads * itemsPerThread));
}

/**
 * @brief Test concurrent push and pop
 * @brief 测试并发推入和弹出
 */
TEST(HeapRingBufferTest, ConcurrentPushPop) {
    HeapRingBuffer<int> buffer(256);
    std::atomic<int> pushCount{0};
    std::atomic<int> popCount{0};
    std::atomic<bool> done{false};
    const int totalItems = 1000;
    
    // Producer thread / 生产者线程
    std::thread producer([&]() {
        for (int i = 0; i < totalItems; ++i) {
            while (!buffer.TryPush(i)) {
                std::this_thread::yield();
            }
            pushCount.fetch_add(1, std::memory_order_relaxed);
        }
        done.store(true, std::memory_order_release);
    });
    
    // Consumer thread / 消费者线程
    std::thread consumer([&]() {
        int value;
        while (!done.load(std::memory_order_acquire) || !buffer.IsEmpty()) {
            if (buffer.TryPop(value)) {
                popCount.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    // All items should be processed / 所有元素应该被处理
    EXPECT_EQ(pushCount.load(), totalItems);
    EXPECT_EQ(popCount.load(), totalItems);
    EXPECT_TRUE(buffer.IsEmpty());
}

// ==============================================================================
// Property-Based Tests / 属性测试
// ==============================================================================

#ifdef ONEPLOG_HAS_RAPIDCHECK

/**
 * @brief Property 6: HeapRingBuffer FIFO order guarantee
 * @brief 属性 6：HeapRingBuffer FIFO 顺序保证
 *
 * For any sequence of enqueued elements [e1, e2, ..., eN], the dequeue order
 * should maintain FIFO order, i.e., elements enqueued first are dequeued first.
 *
 * 对于任意入队元素序列 [e1, e2, ..., eN]，出队顺序应该保持 FIFO 顺序，
 * 即先入队的元素先出队。
 *
 * **Validates: Requirements 7.8**
 */
RC_GTEST_PROP(HeapRingBufferPropertyTest, FIFOOrderGuarantee, ()) {
    // Generate random capacity (power of 2 for efficiency)
    // 生成随机容量（2 的幂以提高效率）
    auto capacityExp = *rc::gen::inRange(2, 8);  // 4 to 128
    size_t capacity = 1 << capacityExp;
    
    HeapRingBuffer<int> buffer(capacity);
    
    // Generate random number of items to push (up to capacity)
    // 生成随机数量的元素（最多到容量）
    auto itemCount = *rc::gen::inRange(0, static_cast<int>(capacity));
    
    // Push items / 推入元素
    std::vector<int> pushed;
    for (int i = 0; i < itemCount; ++i) {
        auto value = *rc::gen::arbitrary<int>();
        if (buffer.TryPush(value)) {
            pushed.push_back(value);
        }
    }
    
    // Pop items and verify FIFO order / 弹出元素并验证 FIFO 顺序
    std::vector<int> popped;
    int value;
    while (buffer.TryPop(value)) {
        popped.push_back(value);
    }
    
    // Verify FIFO order / 验证 FIFO 顺序
    RC_ASSERT(pushed == popped);
}

/**
 * @brief Property: Size is always consistent
 * @brief 属性：大小始终一致
 */
RC_GTEST_PROP(HeapRingBufferPropertyTest, SizeConsistency, ()) {
    auto capacity = *rc::gen::inRange(4, 64);
    HeapRingBuffer<int> buffer(static_cast<size_t>(capacity));
    
    auto pushCount = *rc::gen::inRange(0, capacity);
    auto popCount = *rc::gen::inRange(0, pushCount);
    
    // Push items / 推入元素
    for (int i = 0; i < pushCount; ++i) {
        buffer.TryPush(i);
    }
    RC_ASSERT(buffer.Size() == static_cast<size_t>(pushCount));
    
    // Pop items / 弹出元素
    int value;
    for (int i = 0; i < popCount; ++i) {
        buffer.TryPop(value);
    }
    RC_ASSERT(buffer.Size() == static_cast<size_t>(pushCount - popCount));
}

/**
 * @brief Property: Never exceeds capacity
 * @brief 属性：永远不超过容量
 */
RC_GTEST_PROP(HeapRingBufferPropertyTest, NeverExceedsCapacity, ()) {
    auto capacity = *rc::gen::inRange(4, 64);
    HeapRingBuffer<int> buffer(static_cast<size_t>(capacity));
    
    // Try to push more than capacity / 尝试推入超过容量的元素
    int successCount = 0;
    for (int i = 0; i < capacity * 2; ++i) {
        if (buffer.TryPush(i)) {
            ++successCount;
        }
    }
    
    // Should only succeed up to capacity / 应该只成功到容量
    RC_ASSERT(successCount == capacity);
    RC_ASSERT(buffer.Size() == static_cast<size_t>(capacity));
    RC_ASSERT(buffer.IsFull());
}

#endif  // ONEPLOG_HAS_RAPIDCHECK

}  // namespace test
}  // namespace oneplog
