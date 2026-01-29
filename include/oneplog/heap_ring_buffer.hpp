/**
 * @file heap_ring_buffer.hpp
 * @brief Lock-free ring buffer for asynchronous logging
 * @brief 用于异步日志的无锁环形队列
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <vector>

#include "oneplog/ring_buffer.hpp"

// Platform-specific includes for notification mechanism
// 平台特定的通知机制头文件
#ifdef __linux__
#include <sys/eventfd.h>
#include <unistd.h>
#include <poll.h>
#elif defined(__APPLE__)
#include <dispatch/dispatch.h>
#include <unistd.h>
#endif

namespace oneplog {

// ==============================================================================
// Queue Full Policy / 队列满策略
// ==============================================================================

/**
 * @brief Policy when queue is full
 * @brief 队列满时的策略
 */
enum class QueueFullPolicy : uint8_t {
    Block = 0,      ///< Block until space available / 阻塞直到有空间
    DropNewest = 1, ///< Drop the new log entry / 丢弃新日志
    DropOldest = 2  ///< Drop the oldest log entry / 丢弃旧日志
};

// ==============================================================================
// Consumer State / 消费者状态
// ==============================================================================

/**
 * @brief Consumer state for notification optimization
 * @brief 用于通知优化的消费者状态
 */
enum class ConsumerState : uint8_t {
    Active = 0,         ///< Consumer is actively polling / 消费者正在主动轮询
    WaitingNotify = 1   ///< Consumer is waiting for notification / 消费者正在等待通知
};

/**
 * @brief Lock-free ring buffer for asynchronous logging
 * @brief 用于异步日志的无锁环形队列
 *
 * HeapRingBuffer is a multi-producer single-consumer (MPSC) lock-free queue
 * designed for high-performance asynchronous logging. It uses:
 * - Atomic operations for thread-safe access
 * - Cache-line aligned head/tail to prevent false sharing
 * - Slot-based state machine for coordination
 * - Configurable queue full policy (block/drop newest/drop oldest)
 * - Optimized notification mechanism with consumer state tracking
 *
 * HeapRingBuffer 是一个多生产者单消费者（MPSC）无锁队列，
 * 专为高性能异步日志设计。它使用：
 * - 原子操作实现线程安全访问
 * - 缓存行对齐的 head/tail 防止伪共享
 * - 基于槽位的状态机进行协调
 * - 可配置的队列满策略（阻塞/丢弃新日志/丢弃旧日志）
 * - 带消费者状态跟踪的优化通知机制
 *
 * @tparam T The element type to store / 要存储的元素类型
 */
template<typename T>
class HeapRingBuffer {
public:
    /// Invalid slot index / 无效槽位索引
    static constexpr int64_t kInvalidSlot = -1;

    /**
     * @brief Construct a ring buffer with specified capacity and policy
     * @brief 使用指定容量和策略构造环形队列
     *
     * @param capacity The maximum number of elements / 最大元素数量
     * @param policy Queue full policy / 队列满策略
     */
    explicit HeapRingBuffer(size_t capacity, 
                            QueueFullPolicy policy = QueueFullPolicy::DropNewest)
        : m_capacity(capacity)
        , m_policy(policy)
        , m_buffer(capacity)
        , m_slotStatus(capacity)
#ifdef __linux__
        , m_eventFd(eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE))
#elif defined(__APPLE__)
        , m_semaphore(dispatch_semaphore_create(0))
#endif
    {
        m_head.value.store(0, std::memory_order_relaxed);
        m_tail.value.store(0, std::memory_order_relaxed);
        m_consumerState.value.store(ConsumerState::Active, std::memory_order_relaxed);
        m_droppedCount.value.store(0, std::memory_order_relaxed);
        
        // Initialize all slots to empty state
        // 初始化所有槽位为空状态
        for (size_t i = 0; i < capacity; ++i) {
            m_slotStatus[i].Reset();
        }
    }

    /**
     * @brief Destructor
     * @brief 析构函数
     */
    ~HeapRingBuffer() {
#ifdef __linux__
        if (m_eventFd >= 0) {
            close(m_eventFd);
            m_eventFd = -1;
        }
#elif defined(__APPLE__)
        if (m_semaphore != nullptr) {
            m_semaphore = nullptr;
        }
#endif
    }

    // Non-copyable / 不可拷贝
    HeapRingBuffer(const HeapRingBuffer&) = delete;
    HeapRingBuffer& operator=(const HeapRingBuffer&) = delete;

    // Movable / 可移动
    HeapRingBuffer(HeapRingBuffer&& other) noexcept
        : m_capacity(other.m_capacity)
        , m_policy(other.m_policy)
        , m_buffer(std::move(other.m_buffer))
        , m_slotStatus(std::move(other.m_slotStatus))
#ifdef __linux__
        , m_eventFd(other.m_eventFd)
#elif defined(__APPLE__)
        , m_semaphore(other.m_semaphore)
#endif
    {
        m_head.value.store(other.m_head.value.load(std::memory_order_relaxed), 
                          std::memory_order_relaxed);
        m_tail.value.store(other.m_tail.value.load(std::memory_order_relaxed), 
                          std::memory_order_relaxed);
        m_consumerState.value.store(other.m_consumerState.value.load(std::memory_order_relaxed),
                                    std::memory_order_relaxed);
        m_droppedCount.value.store(other.m_droppedCount.value.load(std::memory_order_relaxed),
                                   std::memory_order_relaxed);
#ifdef __linux__
        other.m_eventFd = -1;
#elif defined(__APPLE__)
        other.m_semaphore = nullptr;
#endif
    }

    HeapRingBuffer& operator=(HeapRingBuffer&& other) noexcept {
        if (this != &other) {
#ifdef __linux__
            if (m_eventFd >= 0) {
                close(m_eventFd);
            }
            m_eventFd = other.m_eventFd;
            other.m_eventFd = -1;
#elif defined(__APPLE__)
            m_semaphore = other.m_semaphore;
            other.m_semaphore = nullptr;
#endif
            m_capacity = other.m_capacity;
            m_policy = other.m_policy;
            m_buffer = std::move(other.m_buffer);
            m_slotStatus = std::move(other.m_slotStatus);
            m_head.value.store(other.m_head.value.load(std::memory_order_relaxed), 
                              std::memory_order_relaxed);
            m_tail.value.store(other.m_tail.value.load(std::memory_order_relaxed), 
                              std::memory_order_relaxed);
            m_consumerState.value.store(other.m_consumerState.value.load(std::memory_order_relaxed),
                                        std::memory_order_relaxed);
            m_droppedCount.value.store(other.m_droppedCount.value.load(std::memory_order_relaxed),
                                       std::memory_order_relaxed);
        }
        return *this;
    }

    // =========================================================================
    // Configuration / 配置
    // =========================================================================

    /**
     * @brief Set queue full policy
     * @brief 设置队列满策略
     */
    void SetPolicy(QueueFullPolicy policy) noexcept {
        m_policy = policy;
    }

    /**
     * @brief Get queue full policy
     * @brief 获取队列满策略
     */
    QueueFullPolicy GetPolicy() const noexcept {
        return m_policy;
    }

    /**
     * @brief Get number of dropped entries
     * @brief 获取丢弃的条目数量
     */
    uint64_t GetDroppedCount() const noexcept {
        return m_droppedCount.value.load(std::memory_order_relaxed);
    }

    /**
     * @brief Reset dropped count
     * @brief 重置丢弃计数
     */
    void ResetDroppedCount() noexcept {
        m_droppedCount.value.store(0, std::memory_order_relaxed);
    }

    // =========================================================================
    // Producer Operations / 生产者操作
    // =========================================================================

    /**
     * @brief Acquire a slot for writing (claim index first, then write)
     * @brief 获取一个用于写入的槽位（先抢占索引，再写入）
     *
     * @return Slot index if successful, kInvalidSlot if queue is full (DropNewest policy)
     * @return 成功时返回槽位索引，队列满时返回 kInvalidSlot（DropNewest 策略）
     */
    int64_t AcquireSlot() noexcept {
        size_t head = m_head.value.load(std::memory_order_relaxed);
        
        while (true) {
            size_t tail = m_tail.value.load(std::memory_order_acquire);
            
            // Check if queue is full / 检查队列是否已满
            if (head - tail >= m_capacity) {
                switch (m_policy) {
                    case QueueFullPolicy::DropNewest:
                        // Drop the new entry / 丢弃新条目
                        m_droppedCount.value.fetch_add(1, std::memory_order_relaxed);
                        return kInvalidSlot;
                    
                    case QueueFullPolicy::DropOldest:
                        // Try to advance tail to drop oldest / 尝试推进 tail 丢弃最旧的
                        if (DropOldestEntry()) {
                            // Retry after dropping / 丢弃后重试
                            continue;
                        }
                        // If drop failed, yield and retry / 如果丢弃失败，让出并重试
                        std::this_thread::yield();
                        continue;
                    
                    case QueueFullPolicy::Block:
                    default:
                        // Block until space available / 阻塞直到有空间
                        std::this_thread::yield();
                        head = m_head.value.load(std::memory_order_relaxed);
                        continue;
                }
            }
            
            // Try to claim the slot / 尝试抢占槽位
            if (m_head.value.compare_exchange_weak(head, head + 1,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_relaxed)) {
                size_t slot = head % m_capacity;
                
                // Wait for slot to be empty (in case of wrap-around)
                // 等待槽位为空（处理环绕情况）
                while (!m_slotStatus[slot].TryAcquire()) {
                    std::this_thread::yield();
                }
                
                return static_cast<int64_t>(slot);
            }
            // CAS failed, retry with updated head / CAS 失败，使用更新的 head 重试
        }
    }

    /**
     * @brief Commit data to a previously acquired slot
     * @brief 将数据提交到之前获取的槽位
     *
     * @param slot The slot index returned by AcquireSlot / AcquireSlot 返回的槽位索引
     * @param item The item to store / 要存储的元素
     */
    void CommitSlot(int64_t slot, T&& item) noexcept {
        if (slot < 0 || static_cast<size_t>(slot) >= m_capacity) {
            return;
        }
        
        size_t idx = static_cast<size_t>(slot);
        m_buffer[idx] = std::move(item);
        m_slotStatus[idx].Commit();
    }

    /**
     * @brief Commit data to a previously acquired slot (copy version)
     * @brief 将数据提交到之前获取的槽位（拷贝版本）
     */
    void CommitSlot(int64_t slot, const T& item) noexcept {
        if (slot < 0 || static_cast<size_t>(slot) >= m_capacity) {
            return;
        }
        
        size_t idx = static_cast<size_t>(slot);
        m_buffer[idx] = item;
        m_slotStatus[idx].Commit();
    }

    /**
     * @brief Try to push an item to the queue
     * @brief 尝试将元素推入队列
     *
     * @param item The item to push / 要推入的元素
     * @return true if successful, false if dropped (DropNewest policy)
     * @return 成功返回 true，被丢弃返回 false（DropNewest 策略）
     */
    bool TryPush(T&& item) noexcept {
        int64_t slot = AcquireSlot();
        if (slot == kInvalidSlot) {
            return false;
        }
        
        CommitSlot(slot, std::move(item));
        NotifyConsumerIfWaiting();
        return true;
    }

    /**
     * @brief Try to push an item to the queue (copy version)
     * @brief 尝试将元素推入队列（拷贝版本）
     */
    bool TryPush(const T& item) noexcept {
        int64_t slot = AcquireSlot();
        if (slot == kInvalidSlot) {
            return false;
        }
        
        CommitSlot(slot, item);
        NotifyConsumerIfWaiting();
        return true;
    }

    /**
     * @brief Try to push an item with WFC (Wait For Completion) flag
     * @brief 尝试推入带 WFC（等待完成）标志的元素
     *
     * @param item The item to push / 要推入的元素
     * @return Slot index if successful, kInvalidSlot if dropped
     * @return 成功返回槽位索引，被丢弃返回 kInvalidSlot
     */
    int64_t TryPushWFC(T&& item) noexcept {
        int64_t slot = AcquireSlot();
        if (slot == kInvalidSlot) {
            return kInvalidSlot;
        }
        
        size_t idx = static_cast<size_t>(slot);
        m_buffer[idx] = std::move(item);
        m_slotStatus[idx].EnableWFC();
        m_slotStatus[idx].Commit();
        NotifyConsumerIfWaiting();
        return slot;
    }

    /**
     * @brief Try to push an item with WFC flag (copy version)
     * @brief 尝试推入带 WFC 标志的元素（拷贝版本）
     */
    int64_t TryPushWFC(const T& item) noexcept {
        int64_t slot = AcquireSlot();
        if (slot == kInvalidSlot) {
            return kInvalidSlot;
        }
        
        size_t idx = static_cast<size_t>(slot);
        m_buffer[idx] = item;
        m_slotStatus[idx].EnableWFC();
        m_slotStatus[idx].Commit();
        NotifyConsumerIfWaiting();
        return slot;
    }

    // =========================================================================
    // Consumer Operations / 消费者操作
    // =========================================================================

    /**
     * @brief Try to pop an item from the queue
     * @brief 尝试从队列弹出元素
     *
     * @param item Output parameter for the popped item / 弹出元素的输出参数
     * @return true if successful, false if queue is empty
     * @return 成功返回 true，队列空返回 false
     */
    bool TryPop(T& item) noexcept {
        size_t tail = m_tail.value.load(std::memory_order_relaxed);
        size_t head = m_head.value.load(std::memory_order_acquire);
        
        // Check if queue is empty / 检查队列是否为空
        if (tail >= head) {
            return false;
        }
        
        size_t slot = tail % m_capacity;
        
        // Wait for slot to be ready / 等待槽位就绪
        if (!m_slotStatus[slot].TryStartRead()) {
            return false;
        }
        
        // Read the item / 读取元素
        item = std::move(m_buffer[slot]);
        
        // Check if WFC is enabled / 检查是否启用 WFC
        bool hasWFC = m_slotStatus[slot].IsWFCEnabled();
        
        // Complete the read / 完成读取
        m_slotStatus[slot].CompleteRead();
        
        // If WFC was enabled, mark it as completed
        // 如果启用了 WFC，标记为完成
        if (hasWFC) {
            m_slotStatus[slot].CompleteWFC();
        }
        
        // Advance tail / 推进 tail
        m_tail.value.store(tail + 1, std::memory_order_release);
        
        return true;
    }

    /**
     * @brief Try to pop multiple items from the queue
     * @brief 尝试从队列批量弹出元素
     *
     * @param items Output vector for popped items / 弹出元素的输出向量
     * @param maxCount Maximum number of items to pop / 最大弹出数量
     * @return Number of items actually popped / 实际弹出的元素数量
     */
    size_t TryPopBatch(std::vector<T>& items, size_t maxCount) noexcept {
        items.clear();
        items.reserve(maxCount);
        
        size_t count = 0;
        T item;
        
        while (count < maxCount && TryPop(item)) {
            items.push_back(std::move(item));
            ++count;
        }
        
        return count;
    }

    // =========================================================================
    // WFC Support / WFC 支持
    // =========================================================================

    /**
     * @brief Mark WFC as completed for a slot
     * @brief 标记槽位的 WFC 为完成
     */
    void MarkWFCComplete(int64_t slot) noexcept {
        if (slot >= 0 && static_cast<size_t>(slot) < m_capacity) {
            m_slotStatus[static_cast<size_t>(slot)].CompleteWFC();
        }
    }

    /**
     * @brief Wait for WFC completion on a slot
     * @brief 等待槽位的 WFC 完成
     */
    bool WaitForCompletion(int64_t slot, std::chrono::milliseconds timeout) noexcept {
        if (slot < 0 || static_cast<size_t>(slot) >= m_capacity) {
            return false;
        }
        
        size_t idx = static_cast<size_t>(slot);
        auto deadline = std::chrono::steady_clock::now() + timeout;
        
        while (std::chrono::steady_clock::now() < deadline) {
            if (m_slotStatus[idx].IsWFCCompleted()) {
                return true;
            }
            std::this_thread::yield();
        }
        
        return m_slotStatus[idx].IsWFCCompleted();
    }

    /**
     * @brief Get WFC state for a slot
     * @brief 获取槽位的 WFC 状态
     */
    WFCState GetWFCState(int64_t slot) const noexcept {
        if (slot < 0 || static_cast<size_t>(slot) >= m_capacity) {
            return kWFCNone;
        }
        return m_slotStatus[static_cast<size_t>(slot)].GetWFCState();
    }

    // =========================================================================
    // Status Query / 状态查询
    // =========================================================================

    /**
     * @brief Check if the queue is empty
     * @brief 检查队列是否为空
     */
    bool IsEmpty() const noexcept {
        size_t tail = m_tail.value.load(std::memory_order_acquire);
        size_t head = m_head.value.load(std::memory_order_acquire);
        return tail >= head;
    }

    /**
     * @brief Check if the queue is full
     * @brief 检查队列是否已满
     */
    bool IsFull() const noexcept {
        size_t tail = m_tail.value.load(std::memory_order_acquire);
        size_t head = m_head.value.load(std::memory_order_acquire);
        return head - tail >= m_capacity;
    }

    /**
     * @brief Get the current number of elements in the queue
     * @brief 获取队列中当前元素数量
     */
    size_t Size() const noexcept {
        size_t tail = m_tail.value.load(std::memory_order_acquire);
        size_t head = m_head.value.load(std::memory_order_acquire);
        return (head > tail) ? (head - tail) : 0;
    }

    /**
     * @brief Get the queue capacity
     * @brief 获取队列容量
     */
    size_t Capacity() const noexcept {
        return m_capacity;
    }

    /**
     * @brief Get consumer state
     * @brief 获取消费者状态
     */
    ConsumerState GetConsumerState() const noexcept {
        return m_consumerState.value.load(std::memory_order_acquire);
    }

    // =========================================================================
    // Notification Mechanism / 通知机制
    // =========================================================================

#ifdef __linux__
    /**
     * @brief Get the eventfd file descriptor (Linux only)
     * @brief 获取 eventfd 文件描述符（仅 Linux）
     */
    int GetEventFD() const noexcept {
        return m_eventFd;
    }
#endif

    /**
     * @brief Notify consumer only if it's in waiting state
     * @brief 仅当消费者处于等待状态时才通知
     *
     * This optimizes notification by avoiding unnecessary system calls
     * when the consumer is actively polling.
     * 通过避免消费者主动轮询时的不必要系统调用来优化通知。
     */
    void NotifyConsumerIfWaiting() noexcept {
        // Only notify if consumer is waiting / 仅当消费者等待时才通知
        if (m_consumerState.value.load(std::memory_order_acquire) == ConsumerState::WaitingNotify) {
            DoNotify();
        }
    }

    /**
     * @brief Force notify consumer (unconditional)
     * @brief 强制通知消费者（无条件）
     */
    void NotifyConsumer() noexcept {
        DoNotify();
    }

    /**
     * @brief Wait for data with polling and timeout (consumer side)
     * @brief 使用轮询和超时等待数据（消费者端）
     *
     * Consumer workflow:
     * 1. Try to read data from queue
     * 2. If empty, poll for pollInterval
     * 3. If still empty, set state to WaitingNotify and wait for notification
     * 4. After waking up, set state back to Active
     *
     * 消费者工作流程：
     * 1. 尝试从队列读取数据
     * 2. 如果为空，轮询 pollInterval 时间
     * 3. 如果仍然为空，设置状态为 WaitingNotify 并等待通知
     * 4. 唤醒后，将状态设置回 Active
     *
     * @param pollInterval Polling interval before entering wait state / 进入等待状态前的轮询间隔
     * @param pollTimeout Maximum wait time for notification / 等待通知的最大时间
     * @return true if data is available, false if timeout
     * @return 有数据返回 true，超时返回 false
     */
    bool WaitForData(std::chrono::microseconds pollInterval,
                     std::chrono::milliseconds pollTimeout) noexcept {
        // Set consumer state to Active (polling phase)
        // 设置消费者状态为 Active（轮询阶段）
        m_consumerState.value.store(ConsumerState::Active, std::memory_order_release);
        
        // Phase 1: Poll for a short time / 阶段 1：短时间轮询
        auto pollEnd = std::chrono::steady_clock::now() + pollInterval;
        while (std::chrono::steady_clock::now() < pollEnd) {
            if (!IsEmpty()) {
                return true;
            }
            std::this_thread::yield();
        }
        
        // Check again before entering wait state / 进入等待状态前再次检查
        if (!IsEmpty()) {
            return true;
        }
        
        // Phase 2: Enter waiting state / 阶段 2：进入等待状态
        // Set state to WaitingNotify BEFORE checking queue again
        // 在再次检查队列之前设置状态为 WaitingNotify
        m_consumerState.value.store(ConsumerState::WaitingNotify, std::memory_order_release);
        
        // Double-check after setting state (avoid race condition)
        // 设置状态后再次检查（避免竞态条件）
        if (!IsEmpty()) {
            m_consumerState.value.store(ConsumerState::Active, std::memory_order_release);
            return true;
        }
        
        // Phase 3: Wait for notification / 阶段 3：等待通知
        bool hasData = DoWait(pollTimeout);
        
        // Set state back to Active / 将状态设置回 Active
        m_consumerState.value.store(ConsumerState::Active, std::memory_order_release);
        
        return hasData || !IsEmpty();
    }

private:
    /**
     * @brief Drop the oldest entry when queue is full
     * @brief 队列满时丢弃最旧的条目
     *
     * @return true if successfully dropped, false otherwise
     * @return 成功丢弃返回 true，否则返回 false
     */
    bool DropOldestEntry() noexcept {
        size_t tail = m_tail.value.load(std::memory_order_relaxed);
        size_t head = m_head.value.load(std::memory_order_acquire);
        
        // Check if there's something to drop / 检查是否有东西可以丢弃
        if (tail >= head) {
            return false;
        }
        
        size_t slot = tail % m_capacity;
        
        // Try to start reading (to drop) / 尝试开始读取（以丢弃）
        if (!m_slotStatus[slot].TryStartRead()) {
            return false;
        }
        
        // Complete the read (drop the entry) / 完成读取（丢弃条目）
        m_slotStatus[slot].CompleteRead();
        
        // Advance tail / 推进 tail
        if (m_tail.value.compare_exchange_strong(tail, tail + 1,
                                                  std::memory_order_acq_rel,
                                                  std::memory_order_relaxed)) {
            m_droppedCount.value.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        
        return false;
    }

    /**
     * @brief Perform the actual notification
     * @brief 执行实际的通知
     */
    void DoNotify() noexcept {
#ifdef __linux__
        if (m_eventFd >= 0) {
            uint64_t val = 1;
            [[maybe_unused]] ssize_t ret = write(m_eventFd, &val, sizeof(val));
        }
#elif defined(__APPLE__)
        if (m_semaphore != nullptr) {
            dispatch_semaphore_signal(m_semaphore);
        }
#endif
    }

    /**
     * @brief Perform the actual wait
     * @brief 执行实际的等待
     */
    bool DoWait(std::chrono::milliseconds timeout) noexcept {
#ifdef __linux__
        if (m_eventFd >= 0) {
            struct pollfd pfd;
            pfd.fd = m_eventFd;
            pfd.events = POLLIN;
            pfd.revents = 0;
            
            int ret = poll(&pfd, 1, static_cast<int>(timeout.count()));
            if (ret > 0 && (pfd.revents & POLLIN)) {
                // Consume the event / 消费事件
                uint64_t val;
                [[maybe_unused]] ssize_t r = read(m_eventFd, &val, sizeof(val));
                return true;
            }
            return false;
        }
#elif defined(__APPLE__)
        if (m_semaphore != nullptr) {
            dispatch_time_t dt = dispatch_time(
                DISPATCH_TIME_NOW, 
                static_cast<int64_t>(timeout.count()) * NSEC_PER_MSEC);
            long ret = dispatch_semaphore_wait(m_semaphore, dt);
            return ret == 0;
        }
#endif
        // Fallback: just sleep / 回退：只是睡眠
        std::this_thread::sleep_for(timeout);
        return !IsEmpty();
    }

private:
    // Cache-line aligned head and tail to prevent false sharing
    // 缓存行对齐的 head 和 tail 以防止伪共享
    CacheLineAligned<std::atomic<size_t>> m_head;
    CacheLineAligned<std::atomic<size_t>> m_tail;
    
    // Consumer state for notification optimization
    // 用于通知优化的消费者状态
    CacheLineAligned<std::atomic<ConsumerState>> m_consumerState;
    
    // Dropped entry count / 丢弃的条目计数
    CacheLineAligned<std::atomic<uint64_t>> m_droppedCount;
    
    size_t m_capacity;                    ///< Queue capacity / 队列容量
    QueueFullPolicy m_policy;             ///< Queue full policy / 队列满策略
    std::vector<T> m_buffer;              ///< Data buffer / 数据缓冲区
    std::vector<SlotStatus> m_slotStatus; ///< Slot status array / 槽位状态数组

#ifdef __linux__
    int m_eventFd;                        ///< EventFD for notification / 用于通知的 EventFD
#elif defined(__APPLE__)
    dispatch_semaphore_t m_semaphore;     ///< Semaphore for notification / 用于通知的信号量
#endif
};

}  // namespace oneplog
