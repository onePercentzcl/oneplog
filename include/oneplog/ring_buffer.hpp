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

// Platform-specific CPU pause/yield intrinsics
// 平台特定的 CPU 暂停/让步指令
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #include <immintrin.h>
    #define CPU_PAUSE() _mm_pause()
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define CPU_PAUSE() __asm__ __volatile__("yield" ::: "memory")
#elif defined(__arm__) || defined(_M_ARM)
    #define CPU_PAUSE() __asm__ __volatile__("yield" ::: "memory")
#else
    #define CPU_PAUSE() std::this_thread::yield()
#endif

// Prefetch intrinsics
// 预取指令
#if defined(__GNUC__) || defined(__clang__)
    #define PREFETCH_READ(addr) __builtin_prefetch((addr), 0, 3)
    #define PREFETCH_WRITE(addr) __builtin_prefetch((addr), 1, 3)
#elif defined(_MSC_VER)
    #include <intrin.h>
    #define PREFETCH_READ(addr) _mm_prefetch((const char*)(addr), _MM_HINT_T0)
    #define PREFETCH_WRITE(addr) _mm_prefetch((const char*)(addr), _MM_HINT_T0)
#else
    #define PREFETCH_READ(addr) ((void)0)
    #define PREFETCH_WRITE(addr) ((void)0)
#endif

namespace oneplog::internal {

using oneplog::internal::kCacheLineSize;
using oneplog::internal::SlotState;
using oneplog::internal::QueueFullPolicy;
using oneplog::internal::ConsumerState;

/**
 * @brief Check if a number is a power of 2.
 * @brief 检查一个数是否是 2 的幂。
 */
constexpr bool IsPowerOf2(size_t n) noexcept {
    return n > 0 && (n & (n - 1)) == 0;
}

/**
 * @brief A single data slot in the ring buffer.
 * @brief 环形缓冲区中的单个数据槽。
 * 
 * Memory Layout (aligned to 8 bytes for m_item):
 * - m_state:    1 byte  (offset 0)
 * - m_padding1: 1 byte  (offset 1, padding for m_dataSize alignment)
 * - m_dataSize: 2 bytes (offset 2)
 * - m_padding2: 4 bytes (offset 4, padding to align m_item to 8 bytes)
 * - m_item:     rest    (offset 8)
 * 
 * 内存布局（m_item 对齐到 8 字节）：
 * - m_state:    1 字节  (偏移 0)
 * - m_padding1: 1 字节  (偏移 1，为 m_dataSize 对齐填充)
 * - m_dataSize: 2 字节  (偏移 2)
 * - m_padding2: 4 字节  (偏移 4，为 m_item 对齐到 8 字节填充)
 * - m_item:     剩余    (偏移 8)
 * 
 * @note WFC state removed from Slot - now using global m_processedTail for flush.
 * @note WFC 状态已从 Slot 移除 - 现在使用全局 m_processedTail 实现 flush。
 */
template <size_t slotSize>
struct alignas(kCacheLineSize) Slot {
    static_assert(slotSize % kCacheLineSize == 0,
        "SlotSize must be a multiple of kCacheLineSize.");

    // Header size: 8 bytes (aligned for efficient m_item access)
    // 头部大小：8 字节（为高效访问 m_item 对齐）
    static constexpr size_t kHeaderSize = 8;
    static_assert(slotSize > kHeaderSize, "SlotSize is too small!");

    std::atomic<SlotState> m_state{SlotState::Empty};  // 1 byte at offset 0
    uint8_t m_padding1{0};                              // 1 byte at offset 1
    uint16_t m_dataSize{0};                             // 2 bytes at offset 2
    uint8_t m_padding2[4]{};                            // 4 bytes at offset 4
    alignas(8) char m_item[slotSize - kHeaderSize];     // rest at offset 8

    SlotState GetState() const noexcept {
        return m_state.load(std::memory_order_acquire);
    }

    void SetState(SlotState state) noexcept {
        m_state.store(state, std::memory_order_release);
    }

    bool CompareExchangeState(SlotState& expected, SlotState desired) noexcept {
        return m_state.compare_exchange_strong(expected, desired,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire);
    }

    bool WriteData(const void* data, size_t size) noexcept {
        if (!data || size == 0 || size > DataSize()) return false;
        m_dataSize = static_cast<uint16_t>(size);
        std::memcpy(m_item, data, size);
        return true;
    }

    bool ReadData(void* buffer, size_t& size) const noexcept {
        if (!buffer || size == 0) return false;
        size_t actualSize = m_dataSize;
        if (actualSize > size) return false;
        std::memcpy(buffer, m_item, actualSize);
        size = actualSize;
        return true;
    }

    const char* Data() const noexcept { return m_item; }
    char* Data() noexcept { return m_item; }
    static constexpr size_t DataSize() noexcept { return slotSize - kHeaderSize; }
};


/**
 * @brief Header containing control variables for the RingBuffer.
 * @brief 包含 RingBuffer 控制变量的头部。
 * 
 * Memory Layout (5 cache lines):
 * Cache Line 0: [m_head] - Consumer write hot
 * Cache Line 1: [m_cachedHead] - Consumer occasional write, producer read
 * Cache Line 2: [m_tail] - Producer write hot
 * Cache Line 3: [m_processedTail] - Consumer write, producer read for WFC
 * Cache Line 4: [m_consumerState, m_droppedCount, ...] - Less contended
 * 
 * 内存布局（5 个缓存行）：
 * 缓存行 0: [m_head] - 消费者写热点
 * 缓存行 1: [m_cachedHead] - 消费者偶尔写，生产者读
 * 缓存行 2: [m_tail] - 生产者写热点
 * 缓存行 3: [m_processedTail] - 消费者写，生产者读（用于 WFC）
 * 缓存行 4: [m_consumerState, m_droppedCount, ...] - 低竞争
 */
struct alignas(kCacheLineSize) RingBufferHeader {
    // Cache Line 0: Consumer write hot
    alignas(kCacheLineSize) std::atomic<size_t> m_head{0};
    
    // Cache Line 1: Shadow head for producer optimization
    alignas(kCacheLineSize) std::atomic<size_t> m_cachedHead{0};
    
    // Cache Line 2: Producer write hot
    alignas(kCacheLineSize) std::atomic<size_t> m_tail{0};
    
    // Cache Line 3: Processed tail for WFC (consumer write, producer read)
    // 已处理的尾指针，用于 WFC（消费者写，生产者读）
    alignas(kCacheLineSize) std::atomic<size_t> m_processedTail{0};
    
    // Cache Line 4: Less contended data
    alignas(kCacheLineSize) std::atomic<ConsumerState> m_consumerState{ConsumerState::Active};
    std::atomic<uint64_t> m_droppedCount{0};
    size_t m_capacity{0};
    QueueFullPolicy m_fullPolicy{QueueFullPolicy::DropNewest};

    void Init(size_t capacity, QueueFullPolicy policy = QueueFullPolicy::DropNewest) noexcept {
        m_head.store(0, std::memory_order_relaxed);
        m_tail.store(0, std::memory_order_relaxed);
        m_cachedHead.store(0, std::memory_order_relaxed);
        m_processedTail.store(0, std::memory_order_relaxed);
        m_consumerState.store(ConsumerState::Active, std::memory_order_relaxed);
        m_droppedCount.store(0, std::memory_order_relaxed);
        m_capacity = capacity;
        m_fullPolicy = policy;
    }

    size_t GetHead() const noexcept { return m_head.load(std::memory_order_acquire); }
    void SetHead(size_t value) noexcept { m_head.store(value, std::memory_order_release); }
    size_t GetTail() const noexcept { return m_tail.load(std::memory_order_acquire); }
    size_t FetchAddTail(size_t delta) noexcept { return m_tail.fetch_add(delta, std::memory_order_acq_rel); }
    size_t GetCachedHead() const noexcept { return m_cachedHead.load(std::memory_order_acquire); }
    void SetCachedHead(size_t value) noexcept { m_cachedHead.store(value, std::memory_order_release); }
    
    // Processed tail for WFC
    size_t GetProcessedTail() const noexcept { return m_processedTail.load(std::memory_order_acquire); }
    void SetProcessedTail(size_t value) noexcept { m_processedTail.store(value, std::memory_order_release); }
    
    ConsumerState GetConsumerState() const noexcept { return m_consumerState.load(std::memory_order_acquire); }
    void SetConsumerState(ConsumerState state) noexcept { m_consumerState.store(state, std::memory_order_release); }
    
    // Relaxed for statistics
    uint64_t GetDroppedCount() const noexcept { return m_droppedCount.load(std::memory_order_relaxed); }
    void IncrementDroppedCount() noexcept { m_droppedCount.fetch_add(1, std::memory_order_relaxed); }
    
    size_t GetCapacity() const noexcept { return m_capacity; }
    QueueFullPolicy GetFullPolicy() const noexcept { return m_fullPolicy; }
    bool IsEmpty() const noexcept { return GetHead() == GetTail(); }
    bool IsFull() const noexcept { return Size() >= m_capacity; }
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
 * @tparam slotCount Number of slots (MUST be power of 2) / 槽位数量（必须是2的幂）
 * 
 * WFC Architecture:
 * - Producer calls TryPush() and records the returned slotIndex
 * - Consumer calls TryPop() and updates m_processedTail after each pop
 * - Producer calls WaitForCompletion(slotIndex) which spins on m_processedTail >= slotIndex
 * - Slot is immediately released, no blocking on slot state
 * 
 * WFC 架构：
 * - 生产者调用 TryPush() 并记录返回的 slotIndex
 * - 消费者调用 TryPop() 并在每次 pop 后更新 m_processedTail
 * - 生产者调用 WaitForCompletion(slotIndex) 自旋等待 m_processedTail >= slotIndex
 * - Slot 立即释放，不阻塞在槽位状态上
 */
template <size_t slotSize = 512, size_t slotCount = 1024>
class alignas(kCacheLineSize) RingBuffer {
    static_assert(IsPowerOf2(slotCount), "slotCount MUST be a power of 2.");
    
public:
    static constexpr size_t kMaxDataSize = Slot<slotSize>::DataSize();
    static constexpr size_t kCachedHeadUpdateInterval = 32;
    static constexpr size_t kIndexMask = slotCount - 1;

private:
    struct ProducerLocalState {
        size_t localCachedHead{0};
    };
    
    static ProducerLocalState& GetProducerLocalState() noexcept {
        thread_local ProducerLocalState state;
        return state;
    }

public:
    void Init(QueueFullPolicy policy = QueueFullPolicy::DropNewest) noexcept {
        m_header.Init(slotCount, policy);
        for (size_t i = 0; i < slotCount; ++i) {
            m_slots[i].SetState(SlotState::Empty);
        }
    }

    /**
     * @brief Acquire a slot for writing using compare-then-claim strategy.
     * @brief 使用先对比再抢占策略获取一个槽位用于写入。
     * 
     * Strategy: Compare first, then CAS to claim the index.
     * This avoids wasting indices when the queue is full.
     * 
     * 策略：先对比，再用 CAS 抢占索引。
     * 这避免了队列满时浪费索引。
     * 
     * @return Slot index (use for WaitForCompletion), or -1 if failed
     * @return 槽位索引（用于 WaitForCompletion），失败返回 -1
     */
    int64_t AcquireSlot() noexcept {
        auto& localState = GetProducerLocalState();
        
        while (true) {
            // Step 1: Read current tail (relaxed is OK, we'll verify with CAS)
            // 步骤 1：读取当前尾指针（relaxed 即可，CAS 会验证）
            size_t currentTail = m_header.m_tail.load(std::memory_order_relaxed);
            
            // Step 2: Check if queue is full
            // 步骤 2：检查队列是否已满
            size_t headToUse;
            if (m_header.GetFullPolicy() == QueueFullPolicy::Block) {
                // Block mode: read actual head for accurate check
                // 阻塞模式：读取真实头指针以获得准确检查
                headToUse = m_header.GetHead();
            } else {
                // Non-block mode: use cached head for performance
                // 非阻塞模式：使用缓存头指针以提高性能
                headToUse = localState.localCachedHead;
            }
            
            size_t estimatedSize = (currentTail + 1) - headToUse;
            
            if (estimatedSize > slotCount) {
                if (m_header.GetFullPolicy() != QueueFullPolicy::Block) {
                    // Update local cached head from global cached head
                    // 从全局缓存头指针更新本地缓存
                    size_t cachedHead = m_header.GetCachedHead();
                    
                    if (cachedHead != localState.localCachedHead) {
                        localState.localCachedHead = cachedHead;
                        estimatedSize = (currentTail + 1) - cachedHead;
                    }
                    
                    // Still full after update
                    // 更新后仍然满
                    if (estimatedSize > slotCount) {
                        // Drop mode: increment dropped count and return
                        // 丢弃模式：增加丢弃计数并返回
                        m_header.IncrementDroppedCount();
                        return -1;
                    }
                } else {
                    // Block mode: spin and retry
                    // 阻塞模式：自旋重试
                    CPU_PAUSE();
                    continue;
                }
            }
            
            // Step 3: Try to claim the slot using CAS (compare-and-swap)
            // 步骤 3：使用 CAS 尝试抢占槽位
            size_t expectedTail = currentTail;
            if (!m_header.m_tail.compare_exchange_weak(expectedTail, currentTail + 1,
                                                        std::memory_order_acq_rel,
                                                        std::memory_order_relaxed)) {
                // CAS failed, another producer claimed it, retry
                // CAS 失败，其他生产者抢占了，重试
                CPU_PAUSE();
                continue;
            }
            
            // Step 4: Successfully claimed slot, now acquire the slot state
            // 步骤 4：成功抢占槽位，现在获取槽位状态
            size_t index = currentTail & kIndexMask;
            auto& slot = m_slots[index];
            
            SlotState expected = SlotState::Empty;
            int retries = 0;
            constexpr int kMaxRetries = 10000;
            
            while (!slot.CompareExchangeState(expected, SlotState::Writing)) {
                if (m_header.GetFullPolicy() == QueueFullPolicy::Block) {
                    // Block mode: wait for slot to become empty
                    // 阻塞模式：等待槽位变空
                    CPU_PAUSE();
                    expected = SlotState::Empty;
                    continue;
                }
                
                if (++retries > kMaxRetries) {
                    // Timeout: slot still not empty (shouldn't happen normally)
                    // 超时：槽位仍未空（正常情况下不应发生）
                    m_header.IncrementDroppedCount();
                    return -1;
                }
                CPU_PAUSE();
                expected = SlotState::Empty;
            }
            
            return static_cast<int64_t>(currentTail);
        }
    }

    void CommitSlot(int64_t slotIndex) noexcept {
        if (slotIndex < 0) return;
        size_t index = static_cast<size_t>(slotIndex) & kIndexMask;
        m_slots[index].SetState(SlotState::Ready);
    }

    /**
     * @brief Try to push data to the buffer.
     * @brief 尝试向缓冲区推送数据。
     * @return Slot index for WaitForCompletion, or -1 if failed
     * @return 用于 WaitForCompletion 的槽位索引，失败返回 -1
     */
    int64_t TryPushEx(const void* data, size_t size) noexcept {
        if (!data || size == 0 || size > kMaxDataSize) return -1;
        
        int64_t slotIndex = AcquireSlot();
        if (slotIndex < 0) return -1;
        
        size_t index = static_cast<size_t>(slotIndex) & kIndexMask;
        auto& slot = m_slots[index];
        
        if (!slot.WriteData(data, size)) {
            slot.SetState(SlotState::Empty);
            return -1;
        }
        
        CommitSlot(slotIndex);
        return slotIndex;
    }

    bool TryPush(const void* data, size_t size) noexcept {
        return TryPushEx(data, size) >= 0;
    }


    /**
     * @brief Try to pop data from the buffer.
     * @brief 尝试从缓冲区弹出数据。
     * 
     * Updates m_processedTail after successful pop for WFC support.
     * m_processedTail represents the next slot to be processed (i.e., one past the last processed).
     * 成功 pop 后更新 m_processedTail 以支持 WFC。
     * m_processedTail 表示下一个待处理的槽位（即最后处理的槽位 + 1）。
     */
    bool TryPop(void* buffer, size_t& size) noexcept {
        if (!buffer || size == 0) return false;
        
        size_t head = m_header.GetHead();
        size_t index = head & kIndexMask;
        
        PREFETCH_READ(&m_slots[index]);
        
        auto& slot = m_slots[index];
        
        SlotState expected = SlotState::Ready;
        if (!slot.CompareExchangeState(expected, SlotState::Reading)) {
            return false;
        }
        
        if (!slot.ReadData(buffer, size)) {
            slot.SetState(SlotState::Ready);
            return false;
        }
        
        slot.SetState(SlotState::Empty);
        
        size_t newHead = head + 1;
        m_header.SetHead(newHead);
        
        // Update processed tail for WFC: set to newHead (one past the processed slot)
        // 更新已处理尾指针用于 WFC：设置为 newHead（已处理槽位 + 1）
        m_header.SetProcessedTail(newHead);
        
        // Batch update cached head
        if ((newHead & (kCachedHeadUpdateInterval - 1)) == 0) {
            m_header.SetCachedHead(newHead);
        }
        
        return true;
    }

    /**
     * @brief Wait for a specific slot to be processed by consumer.
     * @brief 等待特定槽位被消费者处理。
     * 
     * @param slotIndex The slot index returned by TryPushEx / TryPushEx 返回的槽位索引
     * @param timeout Maximum wait time / 最大等待时间
     * @return true if processed within timeout / 如果在超时内处理完成返回 true
     * 
     * Architecture: Spins on m_processedTail > slotIndex.
     * m_processedTail represents the next slot to be processed (one past the last processed).
     * Slot is already released, no blocking on slot state.
     * 
     * 架构：自旋等待 m_processedTail > slotIndex。
     * m_processedTail 表示下一个待处理的槽位（最后处理的槽位 + 1）。
     * 槽位已释放，不阻塞在槽位状态上。
     */
    bool WaitForCompletion(int64_t slotIndex, std::chrono::milliseconds timeout) noexcept {
        if (slotIndex < 0) return false;
        
        auto start = std::chrono::steady_clock::now();
        int backoff = 1;
        
        while (true) {
            size_t processed = m_header.GetProcessedTail();
            // Check if our slot has been processed
            // processed > slotIndex means slot slotIndex has been consumed
            // 检查我们的槽位是否已被处理
            // processed > slotIndex 表示槽位 slotIndex 已被消费
            if (processed > static_cast<size_t>(slotIndex)) {
                return true;
            }
            
            auto now = std::chrono::steady_clock::now();
            if (now - start >= timeout) return false;
            
            // Exponential backoff with CPU pause
            for (int i = 0; i < backoff; ++i) {
                CPU_PAUSE();
            }
            backoff = std::min(backoff * 2, 64);
            
            if (backoff >= 64) {
                std::this_thread::yield();
            }
        }
    }

    /**
     * @brief Flush: wait for all pending messages to be processed.
     * @brief Flush：等待所有待处理消息被处理。
     * 
     * Waits until m_processedTail > (currentTail - 1), meaning all slots up to currentTail-1 are processed.
     * 等待直到 m_processedTail > (currentTail - 1)，即所有到 currentTail-1 的槽位都已处理。
     */
    bool Flush(std::chrono::milliseconds timeout) noexcept {
        size_t currentTail = m_header.GetTail();
        if (currentTail == 0) return true;
        return WaitForCompletion(static_cast<int64_t>(currentTail - 1), timeout);
    }

    bool IsEmpty() const noexcept { return m_header.IsEmpty(); }
    bool IsFull() const noexcept { return m_header.IsFull(); }
    size_t Size() const noexcept { return m_header.Size(); }
    size_t Capacity() const noexcept { return slotCount; }
    uint64_t GetDroppedCount() const noexcept { return m_header.GetDroppedCount(); }
    QueueFullPolicy GetFullPolicy() const noexcept { return m_header.GetFullPolicy(); }
    ConsumerState GetConsumerState() const noexcept { return m_header.GetConsumerState(); }
    void SetConsumerState(ConsumerState state) noexcept { m_header.SetConsumerState(state); }
    size_t GetProcessedTail() const noexcept { return m_header.GetProcessedTail(); }

private:
    RingBufferHeader m_header{};
    Slot<slotSize> m_slots[slotCount];
};

} // namespace oneplog::internal
