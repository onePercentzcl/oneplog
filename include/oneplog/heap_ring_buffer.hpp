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

/**
 * @brief Lock-free ring buffer for asynchronous logging
 * @brief 用于异步日志的无锁环形队列
 *
 * HeapRingBuffer is a multi-producer single-consumer (MPSC) lock-free queue
 * designed for high-performance asynchronous logging. It uses:
 * - Atomic operations for thread-safe access
 * - Cache-line aligned head/tail to prevent false sharing
 * - Slot-based state machine for coordination
 *
 * HeapRingBuffer 是一个多生产者单消费者（MPSC）无锁队列，
 * 专为高性能异步日志设计。它使用：
 * - 原子操作实现线程安全访问
 * - 缓存行对齐的 head/tail 防止伪共享
 * - 基于槽位的状态机进行协调
 *
 * @tparam T The element type to store / 要存储的元素类型
 */
template<typename T>
class HeapRingBuffer {
public:
    /// Invalid slot index / 无效槽位索引
    static constexpr int64_t kInvalidSlot = -1;

    /**
     * @brief Construct a ring buffer with specified capacity
     * @brief 使用指定容量构造环形队列
     *
     * @param capacity The maximum number of elements / 最大元素数量
     */
    explicit HeapRingBuffer(size_t capacity)
        : m_capacity(capacity)
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
            // dispatch_release is not needed for semaphores created with dispatch_semaphore_create
            // on modern macOS, but we set to nullptr for safety
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
            m_buffer = std::move(other.m_buffer);
            m_slotStatus = std::move(other.m_slotStatus);
            m_head.value.store(other.m_head.value.load(std::memory_order_relaxed), 
                              std::memory_order_relaxed);
            m_tail.value.store(other.m_tail.value.load(std::memory_order_relaxed), 
                              std::memory_order_relaxed);
        }
        return *this;
    }

    // =========================================================================
    // Producer Operations / 生产者操作
    // =========================================================================

    /**
     * @brief Acquire a slot for writing (claim index first, then write)
     * @brief 获取一个用于写入的槽位（先抢占索引，再写入）
     *
     * @return Slot index if successful, kInvalidSlot if queue is full
     * @return 成功时返回槽位索引，队列满时返回 kInvalidSlot
     */
    int64_t AcquireSlot() noexcept {
        size_t head = m_head.value.load(std::memory_order_relaxed);
        
        while (true) {
            size_t tail = m_tail.value.load(std::memory_order_acquire);
            
            // Check if queue is full / 检查队列是否已满
            if (head - tail >= m_capacity) {
                return kInvalidSlot;
            }
            
            // Try to claim the slot / 尝试抢占槽位
            if (m_head.value.compare_exchange_weak(head, head + 1,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_relaxed)) {
                size_t slot = head % m_capacity;
                
                // Wait for slot to be empty (in case of wrap-around)
                // 等待槽位为空（处理环绕情况）
                while (!m_slotStatus[slot].TryAcquire()) {
                    // Spin wait / 自旋等待
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
     *
     * @param slot The slot index returned by AcquireSlot / AcquireSlot 返回的槽位索引
     * @param item The item to store / 要存储的元素
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
     * @return true if successful, false if queue is full
     * @return 成功返回 true，队列满返回 false
     */
    bool TryPush(T&& item) noexcept {
        int64_t slot = AcquireSlot();
        if (slot == kInvalidSlot) {
            return false;
        }
        
        CommitSlot(slot, std::move(item));
        NotifyConsumer();
        return true;
    }

    /**
     * @brief Try to push an item to the queue (copy version)
     * @brief 尝试将元素推入队列（拷贝版本）
     *
     * @param item The item to push / 要推入的元素
     * @return true if successful, false if queue is full
     * @return 成功返回 true，队列满返回 false
     */
    bool TryPush(const T& item) noexcept {
        int64_t slot = AcquireSlot();
        if (slot == kInvalidSlot) {
            return false;
        }
        
        CommitSlot(slot, item);
        NotifyConsumer();
        return true;
    }

    /**
     * @brief Try to push an item with WFC (Wait For Completion) flag
     * @brief 尝试推入带 WFC（等待完成）标志的元素
     *
     * @param item The item to push / 要推入的元素
     * @return Slot index if successful, kInvalidSlot if queue is full
     * @return 成功返回槽位索引，队列满返回 kInvalidSlot
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
        NotifyConsumer();
        return slot;
    }

    /**
     * @brief Try to push an item with WFC flag (copy version)
     * @brief 尝试推入带 WFC 标志的元素（拷贝版本）
     *
     * @param item The item to push / 要推入的元素
     * @return Slot index if successful, kInvalidSlot if queue is full
     * @return 成功返回槽位索引，队列满返回 kInvalidSlot
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
        NotifyConsumer();
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
     *
     * @param slot The slot index / 槽位索引
     */
    void MarkWFCComplete(int64_t slot) noexcept {
        if (slot >= 0 && static_cast<size_t>(slot) < m_capacity) {
            m_slotStatus[static_cast<size_t>(slot)].CompleteWFC();
        }
    }

    /**
     * @brief Wait for WFC completion on a slot
     * @brief 等待槽位的 WFC 完成
     *
     * @param slot The slot index / 槽位索引
     * @param timeout Maximum time to wait / 最大等待时间
     * @return true if completed, false if timeout
     * @return 完成返回 true，超时返回 false
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
     *
     * @param slot The slot index / 槽位索引
     * @return WFC state / WFC 状态
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
     * @brief Notify consumer that new data is available
     * @brief 通知消费者有新数据可用
     */
    void NotifyConsumer() noexcept {
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
     * @brief Wait for data with polling and timeout
     * @brief 使用轮询和超时等待数据
     *
     * First polls for pollInterval, then waits for notification up to pollTimeout.
     * 首先轮询 pollInterval 时间，然后等待通知最多 pollTimeout 时间。
     *
     * @param pollInterval Polling interval / 轮询间隔
     * @param pollTimeout Maximum wait time after polling / 轮询后的最大等待时间
     * @return true if data is available, false if timeout
     * @return 有数据返回 true，超时返回 false
     */
    bool WaitForData(std::chrono::microseconds pollInterval,
                     std::chrono::milliseconds pollTimeout) noexcept {
        // First, poll for a short time / 首先，短时间轮询
        auto pollEnd = std::chrono::steady_clock::now() + pollInterval;
        while (std::chrono::steady_clock::now() < pollEnd) {
            if (!IsEmpty()) {
                return true;
            }
            std::this_thread::yield();
        }
        
        // If still empty, wait for notification / 如果仍然为空，等待通知
        if (!IsEmpty()) {
            return true;
        }

#ifdef __linux__
        if (m_eventFd >= 0) {
            struct pollfd pfd;
            pfd.fd = m_eventFd;
            pfd.events = POLLIN;
            pfd.revents = 0;
            
            int ret = poll(&pfd, 1, static_cast<int>(pollTimeout.count()));
            if (ret > 0 && (pfd.revents & POLLIN)) {
                // Consume the event / 消费事件
                uint64_t val;
                [[maybe_unused]] ssize_t r = read(m_eventFd, &val, sizeof(val));
                return !IsEmpty();
            }
        }
#elif defined(__APPLE__)
        if (m_semaphore != nullptr) {
            dispatch_time_t timeout = dispatch_time(
                DISPATCH_TIME_NOW, 
                static_cast<int64_t>(pollTimeout.count()) * NSEC_PER_MSEC);
            long ret = dispatch_semaphore_wait(m_semaphore, timeout);
            if (ret == 0) {
                return !IsEmpty();
            }
        }
#else
        // Fallback: just sleep and check / 回退：只是睡眠并检查
        std::this_thread::sleep_for(pollTimeout);
#endif
        
        return !IsEmpty();
    }

private:
    // Cache-line aligned head and tail to prevent false sharing
    // 缓存行对齐的 head 和 tail 以防止伪共享
    CacheLineAligned<std::atomic<size_t>> m_head;
    CacheLineAligned<std::atomic<size_t>> m_tail;
    
    size_t m_capacity;                    ///< Queue capacity / 队列容量
    std::vector<T> m_buffer;              ///< Data buffer / 数据缓冲区
    std::vector<SlotStatus> m_slotStatus; ///< Slot status array / 槽位状态数组

#ifdef __linux__
    int m_eventFd;                        ///< EventFD for notification / 用于通知的 EventFD
#elif defined(__APPLE__)
    dispatch_semaphore_t m_semaphore;     ///< Semaphore for notification / 用于通知的信号量
#endif
};

}  // namespace oneplog
