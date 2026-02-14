/**
 * @file heap_memory.hpp
 * @brief Heap-based ring buffer for asynchronous logging
 * @brief 基于堆的环形队列，用于异步日志
 *
 * This file contains:
 * - RingBufferBase: Base class for ring buffer implementation
 * - HeapRingBuffer: Ring buffer allocated on heap
 *
 * 本文件包含：
 * - RingBufferBase：环形队列基类实现
 * - HeapRingBuffer：在堆上分配的环形队列
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <thread>
#include <type_traits>
#include <vector>

#if defined(__cpp_lib_hardware_interference_size) && __cpp_lib_hardware_interference_size >= 201703L
    #include <new>
#endif

// Platform-specific includes
#ifdef __linux__
    #include <poll.h>
    #include <sys/eventfd.h>
    #include <unistd.h>
#elif defined(__APPLE__)
    #include <dispatch/dispatch.h>
    #include <unistd.h>
#endif

namespace oneplog {

// ==============================================================================
// Cache Line Size / 缓存行大小
// ==============================================================================

#if defined(__cpp_lib_hardware_interference_size) && __cpp_lib_hardware_interference_size >= 201703L
constexpr size_t kCacheLineSize = std::hardware_destructive_interference_size;
#else
constexpr size_t kCacheLineSize = 64;
#endif

namespace internal {

// ==============================================================================
// CacheLineAligned / 缓存行对齐
// ==============================================================================

template <typename T>
struct alignas(kCacheLineSize) CacheLineAligned {
    T value;

    CacheLineAligned() = default;
    explicit CacheLineAligned(const T& v) : value(v) {}
    explicit CacheLineAligned(T&& v) : value(std::move(v)) {}

    operator T&() noexcept { return value; }
    operator const T&() const noexcept { return value; }
    T* operator->() noexcept { return &value; }
    const T* operator->() const noexcept { return &value; }

    static_assert(sizeof(T) <= kCacheLineSize, "Type T is larger than cache line size");
};

// ==============================================================================
// Spin Wait Helper / 自旋等待辅助函数
// ==============================================================================

/**
 * @brief CPU pause instruction for spin-wait loops
 * @brief 用于自旋等待循环的 CPU pause 指令
 */
inline void CpuPause() noexcept {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    __asm__ __volatile__("pause" ::: "memory");
#elif defined(__aarch64__) || defined(_M_ARM64)
    __asm__ __volatile__("yield" ::: "memory");
#elif defined(__arm__) || defined(_M_ARM)
    __asm__ __volatile__("yield" ::: "memory");
#else
    std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

/**
 * @brief Exponential backoff spin wait
 * @brief 指数退避自旋等待
 */
inline int SpinWait(const int spinCount) noexcept {
    if (constexpr int kSpinCount = 1000; spinCount < kSpinCount) {
        return spinCount + 1;
    } else if (constexpr int kPauseCount = 2000; spinCount < kPauseCount) {
        CpuPause();
        return spinCount + 1;
    } else {
        std::this_thread::yield();
        return 0;
    }
}

inline void SpinOnce() noexcept {
    CpuPause();
}

// ==============================================================================
// Enums / 枚举
// ==============================================================================

enum class QueueFullPolicy : uint8_t { Block = 0, DropNewest = 1, DropOldest = 2 };

enum class ConsumerState : uint8_t { Active = 0, WaitingNotify = 1 };

enum class SlotState : uint8_t { Empty = 0, Writing = 1, Ready = 2, Reading = 3 };

using WFCState = uint8_t;
constexpr WFCState kWFCNone = 0;
constexpr WFCState kWFCEnabled = 1;
constexpr WFCState kWFCCompleted = 2;

// ==============================================================================
// SlotStatus / 槽位状态
// ==============================================================================

struct alignas(kCacheLineSize) SlotStatus {
    alignas(kCacheLineSize) std::atomic<SlotState> state{SlotState::Empty};
    alignas(kCacheLineSize) std::atomic<WFCState> wfc{kWFCNone};

    SlotStatus() = default;
    SlotStatus(const SlotStatus&) = delete;
    SlotStatus& operator=(const SlotStatus&) = delete;

    SlotStatus(SlotStatus&& other) noexcept
        : state(other.state.load(std::memory_order_relaxed)),
          wfc(other.wfc.load(std::memory_order_relaxed)) {}

    SlotStatus& operator=(SlotStatus&& other) noexcept {
        if (this != &other) {
            state.store(other.state.load(std::memory_order_relaxed), std::memory_order_relaxed);
            wfc.store(other.wfc.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        return *this;
    }

    void Reset() noexcept {
        state.store(SlotState::Empty, std::memory_order_release);
        wfc.store(kWFCNone, std::memory_order_release);
    }

    bool IsEmpty() const noexcept { return state.load(std::memory_order_acquire) == SlotState::Empty; }
    bool IsReady() const noexcept { return state.load(std::memory_order_acquire) == SlotState::Ready; }
    bool IsWFCEnabled() const noexcept { return wfc.load(std::memory_order_acquire) == kWFCEnabled; }
    bool IsWFCCompleted() const noexcept { return wfc.load(std::memory_order_acquire) == kWFCCompleted; }

    bool TryAcquire() noexcept {
        SlotState expected = SlotState::Empty;
        return state.compare_exchange_strong(expected, SlotState::Writing,
                                             std::memory_order_acquire, std::memory_order_relaxed);
    }

    void Commit() noexcept { state.store(SlotState::Ready, std::memory_order_release); }

    bool TryStartRead() noexcept {
        SlotState expected = SlotState::Ready;
        return state.compare_exchange_strong(expected, SlotState::Reading,
                                             std::memory_order_acquire, std::memory_order_relaxed);
    }

    void CompleteRead() noexcept { state.store(SlotState::Empty, std::memory_order_release); }
    void EnableWFC() noexcept { wfc.store(kWFCEnabled, std::memory_order_release); }
    void CompleteWFC() noexcept { wfc.store(kWFCCompleted, std::memory_order_release); }
    void ClearWFC() noexcept { wfc.store(kWFCNone, std::memory_order_release); }
    WFCState GetWFCState() const noexcept { return wfc.load(std::memory_order_acquire); }
};

using SharedSlotStatus = SlotStatus;

// ==============================================================================
// RingBufferHeader - Shared Memory Header / 共享内存头部
// ==============================================================================

struct alignas(kCacheLineSize) RingBufferHeader {
    static constexpr size_t kShadowTailUpdateInterval = 32;

    alignas(kCacheLineSize) std::atomic<size_t> head{0};
    alignas(kCacheLineSize) std::atomic<size_t> tail{0};
    alignas(kCacheLineSize) std::atomic<size_t> shadowTail{0};
    alignas(kCacheLineSize) std::atomic<ConsumerState> consumerState{ConsumerState::Active};
    alignas(kCacheLineSize) std::atomic<uint64_t> droppedCount{0};

    size_t capacity{0};
    QueueFullPolicy policy{QueueFullPolicy::DropNewest};
    uint32_t magic{0};
    uint32_t version{0};

    static constexpr uint32_t kMagic = 0x4F4E4550;  // "ONEP"
    static constexpr uint32_t kVersion = 1;

    void Init(size_t cap, QueueFullPolicy pol) noexcept {
        head.store(0, std::memory_order_relaxed);
        tail.store(0, std::memory_order_relaxed);
        shadowTail.store(0, std::memory_order_relaxed);
        consumerState.store(ConsumerState::Active, std::memory_order_relaxed);
        droppedCount.store(0, std::memory_order_relaxed);
        capacity = cap;
        policy = pol;
        magic = kMagic;
        version = kVersion;
    }

    bool IsValid() const noexcept { return magic == kMagic && version == kVersion; }
};

// ==============================================================================
// ProducerLocalCache / 生产者本地缓存
// ==============================================================================

struct ProducerLocalCache {
    size_t cachedShadowTail{0};
    size_t skipCount{0};
    static constexpr size_t kSkipCountOnPersistentFull = 31;
};

// ==============================================================================
// RingBufferBase - Full Implementation / 环形队列基类
// ==============================================================================

template <typename T, bool EnableWFC = true, bool EnableShadowTail = true>
class RingBufferBase {
public:
    static constexpr int64_t kInvalidSlot = -1;

    virtual ~RingBufferBase() { CleanupNotification(); }

    RingBufferBase(const RingBufferBase&) = delete;
    RingBufferBase& operator=(const RingBufferBase&) = delete;

    // =========================================================================
    // Configuration / 配置
    // =========================================================================

    void SetPolicy(QueueFullPolicy policy) noexcept {
        if (m_header) { m_header->policy = policy; }
    }

    QueueFullPolicy GetPolicy() const noexcept {
        return m_header ? m_header->policy : QueueFullPolicy::DropNewest;
    }

    uint64_t GetDroppedCount() const noexcept {
        return m_header ? m_header->droppedCount.load(std::memory_order_relaxed) : 0;
    }

    void ResetDroppedCount() noexcept {
        if (m_header) { m_header->droppedCount.store(0, std::memory_order_relaxed); }
    }

    // =========================================================================
    // Producer Operations / 生产者操作
    // =========================================================================

    static ProducerLocalCache& GetProducerCache() noexcept {
        thread_local ProducerLocalCache cache;
        return cache;
    }

    int64_t AcquireSlot(bool isWFC = false) noexcept {
        if (!m_header || !m_slotStatus) { return kInvalidSlot; }

        if constexpr (!EnableShadowTail) {
            return AcquireSlotSmallQueue(isWFC);
        } else {
            size_t capacity = m_header->capacity;
            if (capacity <= RingBufferHeader::kShadowTailUpdateInterval) {
                return AcquireSlotSmallQueue(isWFC);
            }
            return AcquireSlotWithShadowTail(isWFC);
        }
    }

private:
    int64_t AcquireSlotSmallQueue(bool isWFC) noexcept {
        size_t head = m_header->head.load(std::memory_order_relaxed);
        size_t capacity = m_header->capacity;
        int spinCount = 0;

        while (true) {
            size_t tail = m_header->tail.load(std::memory_order_acquire);

            if (head - tail >= capacity) {
                if constexpr (EnableWFC) {
                    if (isWFC) {
                        spinCount = SpinWait(spinCount);
                        head = m_header->head.load(std::memory_order_relaxed);
                        continue;
                    }
                }

                switch (m_header->policy) {
                    case QueueFullPolicy::DropNewest:
                        m_header->droppedCount.fetch_add(1, std::memory_order_relaxed);
                        return kInvalidSlot;

                    case QueueFullPolicy::DropOldest: {
                        size_t oldestSlot = tail % capacity;
                        if constexpr (EnableWFC) {
                            if (m_slotStatus[oldestSlot].IsWFCEnabled()) {
                                spinCount = SpinWait(spinCount);
                                head = m_header->head.load(std::memory_order_relaxed);
                                continue;
                            }
                        }
                        if (DropOldestEntry()) { continue; }
                        spinCount = SpinWait(spinCount);
                        head = m_header->head.load(std::memory_order_relaxed);
                        continue;
                    }

                    case QueueFullPolicy::Block:
                    default:
                        spinCount = SpinWait(spinCount);
                        head = m_header->head.load(std::memory_order_relaxed);
                        continue;
                }
            }

            if (m_header->head.compare_exchange_weak(head, head + 1, 
                                                     std::memory_order_release,
                                                     std::memory_order_relaxed)) {
                size_t slot = head % capacity;
                int slotSpinCount = 0;
                while (!m_slotStatus[slot].TryAcquire()) {
                    slotSpinCount = SpinWait(slotSpinCount);
                }
                return static_cast<int64_t>(slot);
            }
        }
    }

    int64_t AcquireSlotWithShadowTail(bool isWFC) noexcept {
        ProducerLocalCache& cache = GetProducerCache();
        size_t capacity = m_header->capacity;

        if (cache.skipCount > 0) {
            --cache.skipCount;
            m_header->droppedCount.fetch_add(1, std::memory_order_relaxed);
            return kInvalidSlot;
        }

        size_t head = m_header->head.load(std::memory_order_relaxed);
        int spinCount = 0;

        while (true) {
            if (head - cache.cachedShadowTail < capacity) {
                if (m_header->head.compare_exchange_weak(head, head + 1, 
                                                         std::memory_order_release,
                                                         std::memory_order_relaxed)) {
                    size_t slot = head % capacity;
                    int slotSpinCount = 0;
                    while (!m_slotStatus[slot].TryAcquire()) {
                        slotSpinCount = SpinWait(slotSpinCount);
                    }
                    return static_cast<int64_t>(slot);
                }
                continue;
            }

            size_t newShadowTail = m_header->shadowTail.load(std::memory_order_acquire);
            
            if (head - newShadowTail < capacity) {
                cache.cachedShadowTail = newShadowTail;
                continue;
            }

            if constexpr (EnableWFC) {
                if (isWFC) {
                    spinCount = SpinWait(spinCount);
                    head = m_header->head.load(std::memory_order_relaxed);
                    if (spinCount % 100 == 0) {
                        cache.cachedShadowTail = m_header->shadowTail.load(std::memory_order_acquire);
                    }
                    continue;
                }
            }

            switch (m_header->policy) {
                case QueueFullPolicy::DropNewest: {
                    if (newShadowTail == cache.cachedShadowTail) {
                        cache.skipCount = ProducerLocalCache::kSkipCountOnPersistentFull;
                    }
                    cache.cachedShadowTail = newShadowTail;
                    m_header->droppedCount.fetch_add(1, std::memory_order_relaxed);
                    return kInvalidSlot;
                }

                case QueueFullPolicy::DropOldest: {
                    size_t tail = m_header->tail.load(std::memory_order_acquire);
                    size_t oldestSlot = tail % capacity;
                    
                    if constexpr (EnableWFC) {
                        if (m_slotStatus[oldestSlot].IsWFCEnabled()) {
                            spinCount = SpinWait(spinCount);
                            head = m_header->head.load(std::memory_order_relaxed);
                            continue;
                        }
                    }
                    if (DropOldestEntry()) {
                        cache.cachedShadowTail = m_header->shadowTail.load(std::memory_order_relaxed);
                        continue;
                    }
                    spinCount = SpinWait(spinCount);
                    head = m_header->head.load(std::memory_order_relaxed);
                    continue;
                }

                case QueueFullPolicy::Block:
                default: {
                    spinCount = SpinWait(spinCount);
                    head = m_header->head.load(std::memory_order_relaxed);
                    if (spinCount % 100 == 0) {
                        cache.cachedShadowTail = m_header->shadowTail.load(std::memory_order_acquire);
                    }
                    continue;
                }
            }
        }
    }

public:
    void CommitSlot(int64_t slot, T&& item) noexcept {
        if (!m_header || !m_buffer || slot < 0 || static_cast<size_t>(slot) >= m_header->capacity) { return; }
        size_t idx = static_cast<size_t>(slot);
        m_buffer[idx] = std::move(item);
        m_slotStatus[idx].Commit();
    }

    void CommitSlot(int64_t slot, const T& item) noexcept {
        if (!m_header || !m_buffer || slot < 0 || static_cast<size_t>(slot) >= m_header->capacity) { return; }
        size_t idx = static_cast<size_t>(slot);
        m_buffer[idx] = item;
        m_slotStatus[idx].Commit();
    }

    bool TryPush(T&& item) noexcept {
        int64_t slot = AcquireSlot(false);
        if (slot == kInvalidSlot) { return false; }
        CommitSlot(slot, std::move(item));
        NotifyConsumerIfWaiting();
        return true;
    }

    bool TryPush(const T& item) noexcept {
        int64_t slot = AcquireSlot(false);
        if (slot == kInvalidSlot) { return false; }
        CommitSlot(slot, item);
        NotifyConsumerIfWaiting();
        return true;
    }

    template <bool E = EnableWFC, typename = std::enable_if_t<E>>
    int64_t TryPushWFC(T&& item) noexcept {
        int64_t slot = AcquireSlot(true);
        if (slot == kInvalidSlot) { return kInvalidSlot; }
        size_t idx = static_cast<size_t>(slot);
        m_buffer[idx] = std::move(item);
        m_slotStatus[idx].EnableWFC();
        m_slotStatus[idx].Commit();
        NotifyConsumerIfWaiting();
        return slot;
    }

    template <bool E = EnableWFC, typename = std::enable_if_t<E>>
    int64_t TryPushWFC(const T& item) noexcept {
        int64_t slot = AcquireSlot(true);
        if (slot == kInvalidSlot) { return kInvalidSlot; }
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

    bool TryPop(T& item) noexcept {
        if (!m_header || !m_buffer || !m_slotStatus) { return false; }

        size_t tail = m_header->tail.load(std::memory_order_relaxed);
        size_t slot = tail % m_header->capacity;

        if (!m_slotStatus[slot].TryStartRead()) { return false; }

        item = std::move(m_buffer[slot]);

        if constexpr (EnableWFC) {
            bool hasWFC = m_slotStatus[slot].IsWFCEnabled();
            m_slotStatus[slot].CompleteRead();
            if (hasWFC) { m_slotStatus[slot].CompleteWFC(); }
        } else {
            m_slotStatus[slot].CompleteRead();
        }

        size_t newTail = tail + 1;
        m_header->tail.store(newTail, std::memory_order_release);

        if constexpr (EnableShadowTail) {
            if ((newTail % RingBufferHeader::kShadowTailUpdateInterval) == 0) {
                m_header->shadowTail.store(newTail, std::memory_order_release);
            }
        }

        return true;
    }

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

    template <bool E = EnableWFC, typename = std::enable_if_t<E>>
    void MarkWFCComplete(int64_t slot) noexcept {
        if (m_header && m_slotStatus && slot >= 0 && static_cast<size_t>(slot) < m_header->capacity) {
            m_slotStatus[static_cast<size_t>(slot)].CompleteWFC();
        }
    }

    template <bool E = EnableWFC, typename = std::enable_if_t<E>>
    bool WaitForCompletion(int64_t slot, std::chrono::milliseconds timeout) noexcept {
        if (!m_header || !m_slotStatus || slot < 0 || static_cast<size_t>(slot) >= m_header->capacity) {
            return false;
        }
        size_t idx = static_cast<size_t>(slot);
        auto deadline = std::chrono::steady_clock::now() + timeout;
        int spinCount = 0;
        while (std::chrono::steady_clock::now() < deadline) {
            if (m_slotStatus[idx].IsWFCCompleted()) { return true; }
            spinCount = SpinWait(spinCount);
        }
        return m_slotStatus[idx].IsWFCCompleted();
    }

    template <bool E = EnableWFC, typename = std::enable_if_t<E>>
    WFCState GetWFCState(int64_t slot) const noexcept {
        if (!m_header || !m_slotStatus || slot < 0 || static_cast<size_t>(slot) >= m_header->capacity) {
            return kWFCNone;
        }
        return m_slotStatus[static_cast<size_t>(slot)].GetWFCState();
    }

    // =========================================================================
    // Status Query / 状态查询
    // =========================================================================

    bool IsEmpty() const noexcept {
        if (!m_header || !m_slotStatus) { return true; }
        size_t tail = m_header->tail.load(std::memory_order_relaxed);
        size_t slot = tail % m_header->capacity;
        return !m_slotStatus[slot].IsReady();
    }

    bool IsFull() const noexcept {
        if (!m_header) { return false; }
        size_t head = m_header->head.load(std::memory_order_acquire);
        
        if constexpr (!EnableShadowTail) {
            size_t tail = m_header->tail.load(std::memory_order_acquire);
            return head - tail >= m_header->capacity;
        } else {
            if (m_header->capacity <= RingBufferHeader::kShadowTailUpdateInterval) {
                size_t tail = m_header->tail.load(std::memory_order_acquire);
                return head - tail >= m_header->capacity;
            }
            size_t shadowTail = m_header->shadowTail.load(std::memory_order_acquire);
            return head - shadowTail >= m_header->capacity;
        }
    }

    size_t Size() const noexcept {
        if (!m_header) { return 0; }
        size_t tail = m_header->tail.load(std::memory_order_acquire);
        size_t head = m_header->head.load(std::memory_order_acquire);
        return (head > tail) ? (head - tail) : 0;
    }

    size_t ApproximateSize() const noexcept {
        if (!m_header) { return 0; }
        size_t shadowTail = m_header->shadowTail.load(std::memory_order_relaxed);
        size_t head = m_header->head.load(std::memory_order_relaxed);
        return (head > shadowTail) ? (head - shadowTail) : 0;
    }

    size_t Capacity() const noexcept { return m_header ? m_header->capacity : 0; }

    ConsumerState GetConsumerState() const noexcept {
        return m_header ? m_header->consumerState.load(std::memory_order_acquire) : ConsumerState::Active;
    }

    // =========================================================================
    // Notification Mechanism / 通知机制
    // =========================================================================

#ifdef __linux__
    int GetEventFD() const noexcept { return m_eventFd; }
    void SetEventFD(int fd) noexcept { m_eventFd = fd; m_ownsEventFd = false; }
#endif

    void NotifyConsumerIfWaiting() noexcept {
        if (m_header && m_header->consumerState.load(std::memory_order_acquire) == ConsumerState::WaitingNotify) {
            DoNotify();
        }
    }

    void NotifyConsumer() noexcept { DoNotify(); }

    bool WaitForData(std::chrono::microseconds pollInterval, std::chrono::milliseconds pollTimeout) noexcept {
        if (!m_header) { return false; }

        m_header->consumerState.store(ConsumerState::Active, std::memory_order_release);

        auto pollEnd = std::chrono::steady_clock::now() + pollInterval;
        int spinCount = 0;
        while (std::chrono::steady_clock::now() < pollEnd) {
            if (!IsEmpty()) { return true; }
            spinCount = SpinWait(spinCount);
        }

        if (!IsEmpty()) { return true; }

        m_header->consumerState.store(ConsumerState::WaitingNotify, std::memory_order_release);

        if (!IsEmpty()) {
            m_header->consumerState.store(ConsumerState::Active, std::memory_order_release);
            return true;
        }

        bool hasData = DoWait(pollTimeout);
        m_header->consumerState.store(ConsumerState::Active, std::memory_order_release);

        return hasData || !IsEmpty();
    }

protected:
    RingBufferBase() = default;

    void InitWithExternalMemory(RingBufferHeader* header, T* buffer, SlotStatus* slotStatus,
                                bool initHeader, size_t capacity, QueueFullPolicy policy) noexcept {
        m_header = header;
        m_buffer = buffer;
        m_slotStatus = slotStatus;

        if (initHeader && m_header) { m_header->Init(capacity, policy); }
        if (initHeader && m_slotStatus) {
            for (size_t i = 0; i < capacity; ++i) { m_slotStatus[i].Reset(); }
        }
    }

    void InitNotification() noexcept {
#ifdef __linux__
        if (m_eventFd < 0) { m_eventFd = eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE); m_ownsEventFd = true; }
#elif defined(__APPLE__)
        if (m_semaphore == nullptr) { m_semaphore = dispatch_semaphore_create(0); m_ownsSemaphore = true; }
#endif
    }

    void CleanupNotification() noexcept {
#ifdef __linux__
        if (m_ownsEventFd && m_eventFd >= 0) { close(m_eventFd); m_eventFd = -1; }
#elif defined(__APPLE__)
        // dispatch_semaphore_t must be released with dispatch_release
        if (m_ownsSemaphore && m_semaphore != nullptr) { 
            dispatch_release(m_semaphore);
            m_semaphore = nullptr; 
            m_ownsSemaphore = false;
        }
#endif
    }

    bool DropOldestEntry() noexcept {
        if (!m_header || !m_slotStatus) { return false; }

        size_t tail = m_header->tail.load(std::memory_order_relaxed);
        size_t head = m_header->head.load(std::memory_order_acquire);

        if (tail >= head) { return false; }

        size_t slot = tail % m_header->capacity;

        if constexpr (EnableWFC) {
            if (m_slotStatus[slot].IsWFCEnabled()) { return false; }
        }
        if (!m_slotStatus[slot].TryStartRead()) { return false; }

        m_slotStatus[slot].CompleteRead();

        if (m_header->tail.compare_exchange_strong(tail, tail + 1, std::memory_order_release,
                                                   std::memory_order_relaxed)) {
            m_header->droppedCount.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        return false;
    }

    void DoNotify() noexcept {
#ifdef __linux__
        if (m_eventFd >= 0) {
            uint64_t val = 1;
            [[maybe_unused]] ssize_t ret = write(m_eventFd, &val, sizeof(val));
        }
#elif defined(__APPLE__)
        if (m_semaphore != nullptr) { dispatch_semaphore_signal(m_semaphore); }
#endif
    }

    bool DoWait(std::chrono::milliseconds timeout) noexcept {
#ifdef __linux__
        if (m_eventFd >= 0) {
            struct pollfd pfd;
            pfd.fd = m_eventFd;
            pfd.events = POLLIN;
            pfd.revents = 0;
            int ret = poll(&pfd, 1, static_cast<int>(timeout.count()));
            if (ret > 0 && (pfd.revents & POLLIN)) {
                uint64_t val;
                [[maybe_unused]] ssize_t r = read(m_eventFd, &val, sizeof(val));
                return true;
            }
            return false;
        }
#elif defined(__APPLE__)
        if (m_semaphore != nullptr) {
            dispatch_time_t dt = dispatch_time(DISPATCH_TIME_NOW, static_cast<int64_t>(timeout.count()) * NSEC_PER_MSEC);
            long ret = dispatch_semaphore_wait(m_semaphore, dt);
            return ret == 0;
        }
#endif
        std::this_thread::sleep_for(timeout);
        return !IsEmpty();
    }

protected:
    RingBufferHeader* m_header{nullptr};
    T* m_buffer{nullptr};
    SlotStatus* m_slotStatus{nullptr};

#ifdef __linux__
    int m_eventFd{-1};
    bool m_ownsEventFd{false};
#elif defined(__APPLE__)
    dispatch_semaphore_t m_semaphore{nullptr};
    bool m_ownsSemaphore{false};
#endif
};

// ==============================================================================
// HeapRingBuffer - Ring buffer allocated on heap / 堆上分配的环形队列
// ==============================================================================

template<typename T, bool EnableWFC = true, bool EnableShadowTail = true>
class HeapRingBuffer : public RingBufferBase<T, EnableWFC, EnableShadowTail> {
public:
    using Base = RingBufferBase<T, EnableWFC, EnableShadowTail>;

    explicit HeapRingBuffer(size_t capacity, QueueFullPolicy policy = QueueFullPolicy::DropNewest)
        : Base()
        , m_headerStorage(std::make_unique<RingBufferHeader>())
        , m_bufferStorage(capacity)
        , m_slotStatusStorage(capacity)
    {
        Base::InitWithExternalMemory(
            m_headerStorage.get(),
            m_bufferStorage.data(),
            m_slotStatusStorage.data(),
            true,
            capacity,
            policy
        );
        Base::InitNotification();
    }

    ~HeapRingBuffer() override = default;

    HeapRingBuffer(const HeapRingBuffer&) = delete;
    HeapRingBuffer& operator=(const HeapRingBuffer&) = delete;

    HeapRingBuffer(HeapRingBuffer&& other) noexcept
        : Base()
        , m_headerStorage(std::move(other.m_headerStorage))
        , m_bufferStorage(std::move(other.m_bufferStorage))
        , m_slotStatusStorage(std::move(other.m_slotStatusStorage))
    {
        Base::m_header = m_headerStorage.get();
        Base::m_buffer = m_bufferStorage.data();
        Base::m_slotStatus = m_slotStatusStorage.data();
        
#ifdef __linux__
        Base::m_eventFd = other.m_eventFd;
        Base::m_ownsEventFd = other.m_ownsEventFd;
        other.m_eventFd = -1;
        other.m_ownsEventFd = false;
#elif defined(__APPLE__)
        Base::m_semaphore = other.m_semaphore;
        Base::m_ownsSemaphore = other.m_ownsSemaphore;
        other.m_semaphore = nullptr;
        other.m_ownsSemaphore = false;
#endif
        
        other.m_header = nullptr;
        other.m_buffer = nullptr;
        other.m_slotStatus = nullptr;
    }

    HeapRingBuffer& operator=(HeapRingBuffer&& other) noexcept {
        if (this != &other) {
            Base::CleanupNotification();
            
            m_headerStorage = std::move(other.m_headerStorage);
            m_bufferStorage = std::move(other.m_bufferStorage);
            m_slotStatusStorage = std::move(other.m_slotStatusStorage);
            
            Base::m_header = m_headerStorage.get();
            Base::m_buffer = m_bufferStorage.data();
            Base::m_slotStatus = m_slotStatusStorage.data();
            
#ifdef __linux__
            Base::m_eventFd = other.m_eventFd;
            Base::m_ownsEventFd = other.m_ownsEventFd;
            other.m_eventFd = -1;
            other.m_ownsEventFd = false;
#elif defined(__APPLE__)
            Base::m_semaphore = other.m_semaphore;
            Base::m_ownsSemaphore = other.m_ownsSemaphore;
            other.m_semaphore = nullptr;
            other.m_ownsSemaphore = false;
#endif
            
            other.m_header = nullptr;
            other.m_buffer = nullptr;
            other.m_slotStatus = nullptr;
        }
        return *this;
    }

private:
    std::unique_ptr<RingBufferHeader> m_headerStorage;
    std::vector<T> m_bufferStorage;
    std::vector<SlotStatus> m_slotStatusStorage;
};

}  // namespace internal

// ==============================================================================
// Public API aliases / 公共 API 别名
// ==============================================================================

template<typename T>
using CacheLineAligned = internal::CacheLineAligned<T>;

using QueueFullPolicy = internal::QueueFullPolicy;
using ConsumerState = internal::ConsumerState;
using SlotState = internal::SlotState;
using WFCState = internal::WFCState;
constexpr WFCState kWFCNone = internal::kWFCNone;
constexpr WFCState kWFCEnabled = internal::kWFCEnabled;
constexpr WFCState kWFCCompleted = internal::kWFCCompleted;
using SlotStatus = internal::SlotStatus;
using RingBufferHeader = internal::RingBufferHeader;

template<typename T, bool EnableWFC = true, bool EnableShadowTail = true>
using RingBufferBase = internal::RingBufferBase<T, EnableWFC, EnableShadowTail>;

template<typename T, bool EnableWFC = true, bool EnableShadowTail = true>
using HeapRingBuffer = internal::HeapRingBuffer<T, EnableWFC, EnableShadowTail>;

}  // namespace oneplog
