/**
 * @file ring_buffer.hpp
 * @brief Lock-free MPSC Ring Buffer Implementation
 * @brief 无锁多生产者单消费者（MPSC）环形队列实现
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include "common.hpp"
#include <atomic>
#include <cstring>
#include <chrono>
#include <thread>

namespace oneplog::internal {

// Use definitions from common.hpp
// 使用 common.hpp 中的定义
using oneplog::internal::kCacheLineSize;
using oneplog::internal::SlotState;
using oneplog::internal::WFCState;
using oneplog::internal::QueueFullPolicy;
using oneplog::internal::ConsumerState;

/**
 * @brief A single data slot in the ring buffer.
 * @brief 环形缓冲区中的单个数据槽。
 * @tparam slotSize The TOTAL size of the slot in bytes (including control flags and padding).
 * 槽位的**总大小**（字节），包含控制标志位和填充。
 */
template <size_t slotSize>
struct alignas(kCacheLineSize) Slot {
    // Sanity checks / 合理性检查
    static_assert(slotSize % kCacheLineSize == 0,
        "SlotSize must be a multiple of kCacheLineSize to ensure strict alignment without implicit padding.");
    // SlotSize 必须是 kCacheLineSize 的倍数，以确保严格对齐且无隐式填充。

    // We remove internal alignment to pack data tightly.
    // 我们移除内部对齐以紧凑存储数据。
    static constexpr size_t kHeaderSize = sizeof(std::atomic<SlotState>) + sizeof(std::atomic<WFCState>) + sizeof(uint16_t) + 4;

    static_assert(slotSize > kHeaderSize, "SlotSize is too small!");

    /**
     * @brief Slot state (Atomic).
     * @brief 槽位状态（原子操作）。
     * @note Packed tightly. Accessed by the same thread owning the slot.
     * @note 紧凑排列。由拥有该槽位的同一个线程访问。
     */
    std::atomic<SlotState> m_state{SlotState::Empty};

    /**
     * @brief WFC state (Atomic).
     * @brief WFC 状态（原子操作）。
     */
    std::atomic<WFCState> m_wfc{WFCState::None};

    /**
     * @brief Actual data size stored in this slot.
     * @brief 此槽位中存储的实际数据大小。
     */
    uint16_t m_dataSize{0};

    /**
     * @brief Reserved bytes for alignment.
     * @brief 保留字节用于对齐。
     */
    uint8_t m_reserved[4]{};

    /**
     * @brief Actual data buffer.
     * @brief 实际数据缓冲区。
     * @details Fills the remaining space.
     * 填充剩余空间。
     */
    char m_item[slotSize - kHeaderSize];

    // ========== State Operations / 状态操作 ==========

    /**
     * @brief Get current slot state.
     * @brief 获取当前槽位状态。
     */
    SlotState GetState() const noexcept {
        return m_state.load(std::memory_order_acquire);
    }

    /**
     * @brief Set slot state.
     * @brief 设置槽位状态。
     */
    void SetState(SlotState state) noexcept {
        m_state.store(state, std::memory_order_release);
    }

    /**
     * @brief Compare and exchange slot state.
     * @brief 比较并交换槽位状态。
     * @param expected Expected state / 期望状态
     * @param desired Desired state / 目标状态
     * @return true if successful / 如果成功则返回 true
     */
    bool CompareExchangeState(SlotState& expected, SlotState desired) noexcept {
        return m_state.compare_exchange_strong(expected, desired,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire);
    }

    // ========== WFC Operations / WFC 操作 ==========

    /**
     * @brief Get current WFC state.
     * @brief 获取当前 WFC 状态。
     */
    WFCState GetWFC() const noexcept {
        return m_wfc.load(std::memory_order_acquire);
    }

    /**
     * @brief Set WFC state.
     * @brief 设置 WFC 状态。
     */
    void SetWFC(WFCState state) noexcept {
        m_wfc.store(state, std::memory_order_release);
    }

    // ========== Data Operations / 数据操作 ==========

    /**
     * @brief Write data to slot.
     * @brief 向槽位写入数据。
     * @param data Data pointer / 数据指针
     * @param size Data size / 数据大小
     * @return true if successful / 如果成功则返回 true
     */
    bool WriteData(const void* data, size_t size) noexcept {
        if (!data || size == 0 || size > DataSize()) {
            return false;
        }
        m_dataSize = static_cast<uint16_t>(size);
        std::memcpy(m_item, data, size);
        return true;
    }

    /**
     * @brief Read data from slot.
     * @brief 从槽位读取数据。
     * @param buffer Output buffer / 输出缓冲区
     * @param size Buffer size (in/out) - input: buffer size, output: actual data size
     *             缓冲区大小（输入/输出）- 输入：缓冲区大小，输出：实际数据大小
     * @return true if successful / 如果成功则返回 true
     */
    bool ReadData(void* buffer, size_t& size) const noexcept {
        if (!buffer || size == 0) {
            return false;
        }
        // Return the actual data size stored
        // 返回存储的实际数据大小
        size_t actualSize = m_dataSize;
        if (actualSize > size) {
            return false;  // Buffer too small
        }
        std::memcpy(buffer, m_item, actualSize);
        size = actualSize;
        return true;
    }

    /**
     * @brief Get data pointer.
     * @brief 获取数据指针。
     */
    const char* Data() const noexcept {
        return m_item;
    }

    /**
     * @brief Get data pointer (mutable).
     * @brief 获取数据指针（可变）。
     */
    char* Data() noexcept {
        return m_item;
    }

    /**
     * @brief Get maximum data size.
     * @brief 获取最大数据大小。
     */
    static constexpr size_t DataSize() noexcept {
        return slotSize - kHeaderSize;
    }
};

/**
 * @brief Header containing control variables for the RingBuffer.
 * @brief 包含 RingBuffer 控制变量的头部。
 * @details Implements "Hot/Cold Splitting" to separate Producer-owned and Consumer-owned variables.
 * @details 实现了"冷热分离"，将生产者拥有和消费者拥有的变量物理隔离。
 */
struct alignas(kCacheLineSize) RingBufferHeader {
    // === Cache Line 1: Consumer Owned (Write Hot) / 消费者拥有（写热点） ===

    /**
     * @brief Head index (Consumer read/write).
     * @brief 头指针（消费者读/写）。
     */
    alignas(kCacheLineSize) std::atomic<size_t> m_head{0};

    /**
     * @brief Tail index (Producer read/write).
     * @brief 尾指针（生产者读/写）。
     * @note Aligned to start a new cache line to prevent false sharing with m_head.
     * @note 对齐到新缓存行起始位置，防止与 m_head 发生伪共享。
     */
    alignas(kCacheLineSize) std::atomic<size_t> m_tail{0};

    /**
     * @brief Consumer active state.
     * @brief 消费者活跃状态。
     * @note Aligned to avoid "noisy neighbor" effect from m_head or m_tail.
     * @note 对齐以避免来自 m_head 或 m_tail 的"吵闹邻居"效应。
     */
    alignas(kCacheLineSize) std::atomic<ConsumerState> m_consumerState{ConsumerState::Active};

    // === Cold / Less Contended Data / 冷数据或低竞争数据 ===

    std::atomic<uint64_t> m_droppedCount{0}; ///< Counter for dropped logs / 丢包计数器

    size_t m_capacity{0};                                 ///< Buffer capacity / 缓冲区容量
    QueueFullPolicy m_fullPolicy{QueueFullPolicy::DropNewest}; ///< Full policy / 满策略

    // Padding ensures the data section starts at a new cache line boundary.
    // 填充确保数据区从新的缓存行边界开始。
    char m_padding[kCacheLineSize - sizeof(std::atomic<uint64_t>) - sizeof(size_t) - sizeof(QueueFullPolicy)];

    // ========== Initialization / 初始化 ==========

    /**
     * @brief Initialize the header.
     * @brief 初始化头部。
     * @param capacity Buffer capacity / 缓冲区容量
     * @param policy Queue full policy / 队列满策略
     */
    void Init(size_t capacity, QueueFullPolicy policy = QueueFullPolicy::DropNewest) noexcept {
        m_head.store(0, std::memory_order_relaxed);
        m_tail.store(0, std::memory_order_relaxed);
        m_consumerState.store(ConsumerState::Active, std::memory_order_relaxed);
        m_droppedCount.store(0, std::memory_order_relaxed);
        m_capacity = capacity;
        m_fullPolicy = policy;
    }

    // ========== Head Operations / 头指针操作 ==========

    /**
     * @brief Get head index.
     * @brief 获取头指针。
     */
    size_t GetHead() const noexcept {
        return m_head.load(std::memory_order_acquire);
    }

    /**
     * @brief Set head index.
     * @brief 设置头指针。
     */
    void SetHead(size_t value) noexcept {
        m_head.store(value, std::memory_order_release);
    }

    // ========== Tail Operations / 尾指针操作 ==========

    /**
     * @brief Get tail index.
     * @brief 获取尾指针。
     */
    size_t GetTail() const noexcept {
        return m_tail.load(std::memory_order_acquire);
    }

    /**
     * @brief Set tail index.
     * @brief 设置尾指针。
     */
    void SetTail(size_t value) noexcept {
        m_tail.store(value, std::memory_order_release);
    }

    /**
     * @brief Atomically increment tail and return old value.
     * @brief 原子递增尾指针并返回旧值。
     */
    size_t FetchAddTail(size_t delta) noexcept {
        return m_tail.fetch_add(delta, std::memory_order_acq_rel);
    }

    // ========== Consumer State Operations / 消费者状态操作 ==========

    /**
     * @brief Get consumer state.
     * @brief 获取消费者状态。
     */
    ConsumerState GetConsumerState() const noexcept {
        return m_consumerState.load(std::memory_order_acquire);
    }

    /**
     * @brief Set consumer state.
     * @brief 设置消费者状态。
     */
    void SetConsumerState(ConsumerState state) noexcept {
        m_consumerState.store(state, std::memory_order_release);
    }

    // ========== Dropped Count Operations / 丢弃计数操作 ==========

    /**
     * @brief Get dropped count.
     * @brief 获取丢弃计数。
     */
    uint64_t GetDroppedCount() const noexcept {
        return m_droppedCount.load(std::memory_order_acquire);
    }

    /**
     * @brief Atomically increment dropped count and return old value.
     * @brief 原子递增丢弃计数并返回旧值。
     */
    uint64_t FetchAddDroppedCount(uint64_t delta) noexcept {
        return m_droppedCount.fetch_add(delta, std::memory_order_acq_rel);
    }

    // ========== Configuration Queries / 配置查询 ==========

    /**
     * @brief Get buffer capacity.
     * @brief 获取缓冲区容量。
     */
    size_t GetCapacity() const noexcept {
        return m_capacity;
    }

    /**
     * @brief Get queue full policy.
     * @brief 获取队列满策略。
     */
    QueueFullPolicy GetFullPolicy() const noexcept {
        return m_fullPolicy;
    }

    // ========== State Queries / 状态查询 ==========

    /**
     * @brief Check if buffer is empty.
     * @brief 检查缓冲区是否为空。
     */
    bool IsEmpty() const noexcept {
        return GetHead() == GetTail();
    }

    /**
     * @brief Check if buffer is full.
     * @brief 检查缓冲区是否已满。
     */
    bool IsFull() const noexcept {
        return Size() >= m_capacity;
    }

    /**
     * @brief Get current size.
     * @brief 获取当前大小。
     */
    size_t Size() const noexcept {
        size_t tail = GetTail();
        size_t head = GetHead();
        return tail >= head ? (tail - head) : 0;
    }
};

/**
 * @brief The Ring Buffer Class.
 * @brief 环形缓冲区类。
 * @tparam slotSize Size of each slot in bytes / 每个槽位的大小（字节）
 * @tparam slotCount Number of slots (should be power of 2) / 槽位数量（应为2的幂）
 */
template <size_t slotSize = 512, size_t slotCount = 1024>
class alignas(kCacheLineSize) RingBuffer {
public:
    static constexpr size_t kMaxDataSize = Slot<slotSize>::DataSize();

private:
    /**
     * @brief Shadow head for producer-side optimization.
     * @brief 生产者侧优化的影子头指针。
     * @details This is a cached copy of head that producers use to avoid
     *          frequent reads of the actual head. It's always <= actual head,
     *          providing a conservative estimate of queue fullness.
     * @details 这是生产者用来避免频繁读取实际 head 的缓存副本。
     *          它总是 <= 实际 head，提供队列满状态的保守估计。
     */
    alignas(kCacheLineSize) std::atomic<size_t> m_cachedHead{0};

public:

    /**
     * @brief Initialize the ring buffer.
     * @brief 初始化环形缓冲区。
     * @param policy Queue full policy / 队列满策略
     */
    void Init(QueueFullPolicy policy = QueueFullPolicy::DropNewest) noexcept {
        m_header.Init(slotCount, policy);
        m_cachedHead.store(0, std::memory_order_relaxed);
        
        // Initialize all slots to Empty state
        // 初始化所有槽位为空闲状态
        for (size_t i = 0; i < slotCount; ++i) {
            m_slots[i].SetState(SlotState::Empty);
            m_slots[i].SetWFC(WFCState::None);
        }
    }

    // ========== Producer Operations / 生产者操作 ==========

    /**
     * @brief Acquire a slot for writing.
     * @brief 获取一个槽位用于写入。
     * @return Slot index, or -1 if failed / 槽位索引，失败返回 -1
     */
    int64_t AcquireSlot() noexcept {
        // For DropNewest/DropOldest policies, use shadow head optimization
        // 对于 DropNewest/DropOldest 策略，使用影子头指针优化
        if (m_header.GetFullPolicy() != QueueFullPolicy::Block) {
            // Step 1: Read current tail (will be incremented soon)
            // 步骤 1：读取当前 tail（即将递增）
            size_t currentTail = m_header.GetTail();
            
            // Step 2: Read cached head (conservative estimate)
            // 步骤 2：读取缓存的 head（保守估计）
            size_t cachedHead = m_cachedHead.load(std::memory_order_acquire);
            
            // Step 3: Calculate estimated size
            // 步骤 3：计算估算大小
            size_t estimatedSize = currentTail - cachedHead;
            
            // Step 4: If estimated size suggests queue might be full, refresh cached head
            // 步骤 4：如果估算大小表明队列可能满了，刷新缓存的 head
            if (estimatedSize >= slotCount) {
                // Read actual head and update cached head
                // 读取实际 head 并更新缓存的 head
                size_t actualHead = m_header.GetHead();
                m_cachedHead.store(actualHead, std::memory_order_release);
                
                // Recalculate size with actual head
                // 使用实际 head 重新计算大小
                size_t actualSize = currentTail - actualHead;
                
                if (actualSize >= slotCount) {
                    // Queue is truly full, drop this message
                    // 队列确实满了，丢弃此消息
                    m_header.FetchAddDroppedCount(1);
                    return -1;
                }
            }
            // If we reach here, queue is not full (based on cached or actual head)
            // 如果到达这里，队列未满（基于缓存或实际 head）
        }
        
        // Atomically increment tail to reserve a slot
        // 原子递增尾指针以预留槽位
        size_t slotIndex = m_header.FetchAddTail(1);
        size_t index = slotIndex % slotCount;
        
        // Wait for slot to become Empty
        // 等待槽位变为空闲状态
        auto& slot = m_slots[index];
        SlotState expected = SlotState::Empty;
        
        // Try to transition Empty -> Writing
        // 尝试转换 Empty -> Writing
        int retries = 0;
        constexpr int kMaxRetries = 10000;
        
        while (!slot.CompareExchangeState(expected, SlotState::Writing)) {
            if (expected != SlotState::Empty) {
                // Slot is busy - this shouldn't happen often if our size check worked
                // 槽位忙 - 如果我们的大小检查有效，这种情况不应该经常发生
                
                // For Block policy, keep waiting
                // 对于 Block 策略，继续等待
                if (m_header.GetFullPolicy() == QueueFullPolicy::Block) {
                    std::this_thread::yield();
                    expected = SlotState::Empty;
                    continue;
                }
                
                // For Drop policies, if we've waited too long, give up
                // 对于 Drop 策略，如果等待太久，放弃
                if (++retries > kMaxRetries) {
                    // Failed to acquire slot, drop this message
                    // 无法获取槽位，丢弃此消息
                    m_header.FetchAddDroppedCount(1);
                    return -1;
                }
                
                // Spin wait
                // 自旋等待
                std::this_thread::yield();
                expected = SlotState::Empty;
            }
        }
        
        return static_cast<int64_t>(slotIndex);
    }

    /**
     * @brief Commit a slot after writing.
     * @brief 写入完成后提交槽位。
     * @param slotIndex Slot index returned by AcquireSlot / AcquireSlot 返回的槽位索引
     */
    void CommitSlot(int64_t slotIndex) noexcept {
        if (slotIndex < 0) return;
        
        size_t index = static_cast<size_t>(slotIndex) % slotCount;
        auto& slot = m_slots[index];
        
        // Transition Writing -> Ready
        // 转换 Writing -> Ready
        slot.SetState(SlotState::Ready);
    }

    /**
     * @brief Try to push data to the buffer.
     * @brief 尝试向缓冲区推送数据。
     * @param data Data pointer / 数据指针
     * @param size Data size / 数据大小
     * @return true if successful / 如果成功则返回 true
     */
    bool TryPush(const void* data, size_t size) noexcept {
        if (!data || size == 0 || size > kMaxDataSize) {
            return false;
        }
        
        int64_t slotIndex = AcquireSlot();
        if (slotIndex < 0) {
            return false;
        }
        
        size_t index = static_cast<size_t>(slotIndex) % slotCount;
        auto& slot = m_slots[index];
        
        if (!slot.WriteData(data, size)) {
            // Write failed, release slot
            // 写入失败，释放槽位
            slot.SetState(SlotState::Empty);
            return false;
        }
        
        CommitSlot(slotIndex);
        return true;
    }

    /**
     * @brief Try to push data with WFC enabled.
     * @brief 尝试推送带 WFC 标记的数据。
     * @param data Data pointer / 数据指针
     * @param size Data size / 数据大小
     * @return true if successful / 如果成功则返回 true
     */
    bool TryPushWFC(const void* data, size_t size) noexcept {
        if (!data || size == 0 || size > kMaxDataSize) {
            return false;
        }
        
        int64_t slotIndex = AcquireSlot();
        if (slotIndex < 0) {
            return false;
        }
        
        size_t index = static_cast<size_t>(slotIndex) % slotCount;
        auto& slot = m_slots[index];
        
        // Set WFC state
        // 设置 WFC 状态
        slot.SetWFC(WFCState::Enabled);
        
        if (!slot.WriteData(data, size)) {
            slot.SetState(SlotState::Empty);
            slot.SetWFC(WFCState::None);
            return false;
        }
        
        CommitSlot(slotIndex);
        return true;
    }

    // ========== Consumer Operations / 消费者操作 ==========

    /**
     * @brief Try to pop data from the buffer.
     * @brief 尝试从缓冲区弹出数据。
     * @param buffer Output buffer / 输出缓冲区
     * @param size Buffer size (in/out) / 缓冲区大小（输入/输出）
     * @return true if successful / 如果成功则返回 true
     */
    bool TryPop(void* buffer, size_t& size) noexcept {
        if (!buffer || size == 0) {
            return false;
        }
        
        size_t head = m_header.GetHead();
        size_t tail = m_header.GetTail();
        
        if (head >= tail) {
            // Buffer is empty
            // 缓冲区为空
            return false;
        }
        
        size_t index = head % slotCount;
        auto& slot = m_slots[index];
        
        // Check if slot is Ready
        // 检查槽位是否就绪
        SlotState expected = SlotState::Ready;
        if (!slot.CompareExchangeState(expected, SlotState::Reading)) {
            return false;
        }
        
        // Read data
        // 读取数据
        if (!slot.ReadData(buffer, size)) {
            slot.SetState(SlotState::Ready);
            return false;
        }
        
        // Mark WFC as completed if enabled (before clearing the slot)
        // 如果启用了 WFC，标记为完成（在清除槽位之前）
        if (slot.GetWFC() == WFCState::Enabled) {
            slot.SetWFC(WFCState::Completed);
            // Give WaitForCompletion a chance to see the Completed state
            // 给 WaitForCompletion 一个机会看到完成状态
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        // Transition Reading -> Empty
        // 转换 Reading -> Empty
        slot.SetState(SlotState::Empty);
        
        // Clear WFC state after a delay
        // 延迟清除 WFC 状态
        if (slot.GetWFC() == WFCState::Completed) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            slot.SetWFC(WFCState::None);
        }
        
        // Advance head
        // 推进头指针
        m_header.SetHead(head + 1);
        
        return true;
    }

    // ========== WFC Operations / WFC 操作 ==========

    /**
     * @brief Mark WFC as completed for a slot.
     * @brief 标记槽位的 WFC 为完成状态。
     * @param slotIndex Slot index / 槽位索引
     */
    void MarkWFCComplete(int64_t slotIndex) noexcept {
        if (slotIndex < 0) return;
        
        size_t index = static_cast<size_t>(slotIndex) % slotCount;
        m_slots[index].SetWFC(WFCState::Completed);
    }

    /**
     * @brief Wait for WFC completion.
     * @brief 等待 WFC 完成。
     * @param slotIndex Slot index / 槽位索引
     * @param timeout Timeout duration / 超时时间
     * @return true if completed within timeout / 如果在超时时间内完成则返回 true
     */
    bool WaitForCompletion(int64_t slotIndex, std::chrono::milliseconds timeout) noexcept {
        if (slotIndex < 0) return false;
        
        size_t index = static_cast<size_t>(slotIndex) % slotCount;
        auto& slot = m_slots[index];
        
        auto start = std::chrono::steady_clock::now();
        
        // Exponential backoff
        // 指数回避
        int backoff = 1;
        while (slot.GetWFC() != WFCState::Completed) {
            auto now = std::chrono::steady_clock::now();
            if (now - start >= timeout) {
                return false;
            }
            
            std::this_thread::sleep_for(std::chrono::microseconds(backoff));
            backoff = std::min(backoff * 2, 1000);
        }
        
        return true;
    }

    // ========== State Queries / 状态查询 ==========

    /**
     * @brief Check if buffer is empty.
     * @brief 检查缓冲区是否为空。
     */
    bool IsEmpty() const noexcept {
        return m_header.IsEmpty();
    }

    /**
     * @brief Check if buffer is full.
     * @brief 检查缓冲区是否已满。
     */
    bool IsFull() const noexcept {
        return m_header.IsFull();
    }

    /**
     * @brief Get current size.
     * @brief 获取当前大小。
     */
    size_t Size() const noexcept {
        return m_header.Size();
    }

    /**
     * @brief Get buffer capacity.
     * @brief 获取缓冲区容量。
     */
    size_t Capacity() const noexcept {
        return slotCount;
    }

    /**
     * @brief Get dropped count.
     * @brief 获取丢弃计数。
     */
    uint64_t GetDroppedCount() const noexcept {
        return m_header.GetDroppedCount();
    }

    /**
     * @brief Get queue full policy.
     * @brief 获取队列满策略。
     */
    QueueFullPolicy GetFullPolicy() const noexcept {
        return m_header.GetFullPolicy();
    }

    /**
     * @brief Get consumer state.
     * @brief 获取消费者状态。
     */
    ConsumerState GetConsumerState() const noexcept {
        return m_header.GetConsumerState();
    }

    /**
     * @brief Set consumer state.
     * @brief 设置消费者状态。
     */
    void SetConsumerState(ConsumerState state) noexcept {
        m_header.SetConsumerState(state);
    }

private:
    /**
     * @brief Get slot by index.
     * @brief 根据索引获取槽位。
     */
    Slot<slotSize>& GetSlot(size_t index) noexcept {
        return m_slots[index % slotCount];
    }

    RingBufferHeader m_header{};       ///< Control header / 控制头部
    Slot<slotSize> m_slots[slotCount]; ///< Data slots array / 数据槽位数组
};

} // namespace oneplog::internal
