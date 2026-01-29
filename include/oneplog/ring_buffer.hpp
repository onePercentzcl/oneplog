/**
 * @file ring_buffer.hpp
 * @brief Ring buffer base class with full implementation
 * @brief 环形队列基类（包含完整实现）
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <type_traits>
#include <vector>

#if defined(__cpp_lib_hardware_interference_size) && __cpp_lib_hardware_interference_size >= 201703L
#include <new>
#endif

// Platform-specific includes
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
// Cache Line Size / 缓存行大小
// ==============================================================================

#if defined(__cpp_lib_hardware_interference_size) && __cpp_lib_hardware_interference_size >= 201703L
constexpr size_t kCacheLineSize = std::hardware_destructive_interference_size;
#else
constexpr size_t kCacheLineSize = 64;
#endif

// ==============================================================================
// CacheLineAligned / 缓存行对齐
// ==============================================================================

template<typename T>
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
    // Fallback: compiler barrier / 回退：编译器屏障
    std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
}

/**
 * @brief Exponential backoff spin wait
 * @brief 指数退避自旋等待
 *
 * Strategy / 策略:
 * 1. Spin for kSpinCount iterations (pure CPU spin)
 * 2. Execute pause instruction for kPauseCount iterations
 * 3. Finally yield to OS scheduler
 *
 * @param spinCount Current spin iteration count / 当前自旋迭代计数
 * @return Updated spin count / 更新后的自旋计数
 */
inline int SpinWait(int spinCount) noexcept {
    constexpr int kSpinCount = 1000;   // Pure spin iterations / 纯自旋迭代次数
    constexpr int kPauseCount = 2000;  // Pause iterations / pause 迭代次数
    
    if (spinCount < kSpinCount) {
        // Phase 1: Pure spin / 阶段1：纯自旋
        return spinCount + 1;
    } else if (spinCount < kPauseCount) {
        // Phase 2: Pause instruction / 阶段2：pause 指令
        CpuPause();
        return spinCount + 1;
    } else {
        // Phase 3: Yield to OS / 阶段3：让出 CPU
        std::this_thread::yield();
        return 0;  // Reset for next round / 重置以进行下一轮
    }
}

/**
 * @brief Simple spin wait without backoff (for short waits)
 * @brief 简单自旋等待，无退避（用于短等待）
 */
inline void SpinOnce() noexcept {
    CpuPause();
}

// ==============================================================================
// Enums / 枚举
// ==============================================================================

enum class QueueFullPolicy : uint8_t {
    Block = 0,
    DropNewest = 1,
    DropOldest = 2
};

enum class ConsumerState : uint8_t {
    Active = 0,
    WaitingNotify = 1
};

enum class SlotState : uint8_t {
    Empty = 0,
    Writing = 1,
    Ready = 2,
    Reading = 3
};

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
        // Use acquire on success (to see previous writes), relaxed on failure
        // 成功时使用 acquire（看到之前的写入），失败时使用 relaxed
        return state.compare_exchange_strong(expected, SlotState::Writing,
                                             std::memory_order_acquire, std::memory_order_relaxed);
    }

    void Commit() noexcept { state.store(SlotState::Ready, std::memory_order_release); }

    bool TryStartRead() noexcept {
        SlotState expected = SlotState::Ready;
        // Use acquire on success (to see data written by producer), relaxed on failure
        // 成功时使用 acquire（看到生产者写入的数据），失败时使用 relaxed
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

/**
 * @brief Ring buffer header for shared memory layout
 * @brief 共享内存布局的环形队列头部
 *
 * This structure is placed at the beginning of shared memory.
 * All atomic variables are cache-line aligned to prevent false sharing.
 */
struct alignas(kCacheLineSize) RingBufferHeader {
    alignas(kCacheLineSize) std::atomic<size_t> head{0};
    alignas(kCacheLineSize) std::atomic<size_t> tail{0};
    alignas(kCacheLineSize) std::atomic<ConsumerState> consumerState{ConsumerState::Active};
    alignas(kCacheLineSize) std::atomic<uint64_t> droppedCount{0};
    
    size_t capacity{0};
    QueueFullPolicy policy{QueueFullPolicy::DropNewest};
    uint32_t magic{0};      // Magic number for validation
    uint32_t version{0};    // Version for compatibility check
    
    static constexpr uint32_t kMagic = 0x4F4E4550;  // "ONEP"
    static constexpr uint32_t kVersion = 1;
    
    void Init(size_t cap, QueueFullPolicy pol) noexcept {
        head.store(0, std::memory_order_relaxed);
        tail.store(0, std::memory_order_relaxed);
        consumerState.store(ConsumerState::Active, std::memory_order_relaxed);
        droppedCount.store(0, std::memory_order_relaxed);
        capacity = cap;
        policy = pol;
        magic = kMagic;
        version = kVersion;
    }
    
    bool IsValid() const noexcept {
        return magic == kMagic && version == kVersion;
    }
};


// ==============================================================================
// RingBufferBase - Full Implementation / 环形队列基类（完整实现）
// ==============================================================================

/**
 * @brief Ring buffer base class with full implementation
 * @brief 环形队列基类（包含完整实现）
 *
 * HeapRingBuffer and SharedRingBuffer inherit from this class.
 * The main difference is storage location (heap vs shared memory).
 *
 * @tparam T The element type to store / 要存储的元素类型
 * @tparam EnableWFC Enable WFC (Wait For Completion) support / 启用 WFC 支持
 *                   When false, WFC checks are completely eliminated at compile time.
 *                   当为 false 时，WFC 检查在编译时完全消除。
 */
template<typename T, bool EnableWFC = true>
class RingBufferBase {
public:
    static constexpr int64_t kInvalidSlot = -1;

    virtual ~RingBufferBase() {
        CleanupNotification();
    }

    // Non-copyable
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

    /**
     * @brief Acquire a slot for writing
     * @brief 获取一个用于写入的槽位
     *
     * @param isWFC If true, always block when full (WFC logs never dropped)
     */
    int64_t AcquireSlot(bool isWFC = false) noexcept {
        if (!m_header || !m_slotStatus) { return kInvalidSlot; }
        
        size_t head = m_header->head.load(std::memory_order_relaxed);
        size_t capacity = m_header->capacity;
        int spinCount = 0;
        
        while (true) {
            // Only need acquire to see tail updates from consumer
            size_t tail = m_header->tail.load(std::memory_order_acquire);
            
            if (head - tail >= capacity) {
                // WFC logs NEVER get dropped (only when EnableWFC is true)
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
                        // WFC check only when EnableWFC is true
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
            
            // Use release on success to ensure slot data is visible before head update
            // Use relaxed on failure since we'll retry anyway
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

    void CommitSlot(int64_t slot, T&& item) noexcept {
        if (!m_header || !m_buffer || slot < 0 || 
            static_cast<size_t>(slot) >= m_header->capacity) { return; }
        size_t idx = static_cast<size_t>(slot);
        m_buffer[idx] = std::move(item);
        m_slotStatus[idx].Commit();
    }

    void CommitSlot(int64_t slot, const T& item) noexcept {
        if (!m_header || !m_buffer || slot < 0 || 
            static_cast<size_t>(slot) >= m_header->capacity) { return; }
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

    /**
     * @brief Push with WFC - NEVER dropped, blocks if queue is full
     * @note Only available when EnableWFC is true
     */
    template<bool E = EnableWFC, typename = std::enable_if_t<E>>
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

    template<bool E = EnableWFC, typename = std::enable_if_t<E>>
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
        
        // Check slot state directly - only reads one cache line
        // 直接检查槽位状态 - 只读取一个缓存行
        if (!m_slotStatus[slot].TryStartRead()) { return false; }
        
        item = std::move(m_buffer[slot]);
        
        // WFC check is completely eliminated at compile time when EnableWFC is false
        // 当 EnableWFC 为 false 时，WFC 检查在编译时完全消除
        if constexpr (EnableWFC) {
            bool hasWFC = m_slotStatus[slot].IsWFCEnabled();
            m_slotStatus[slot].CompleteRead();
            if (hasWFC) { m_slotStatus[slot].CompleteWFC(); }
        } else {
            m_slotStatus[slot].CompleteRead();
        }
        
        // Use release to ensure item read is complete before tail update
        m_header->tail.store(tail + 1, std::memory_order_release);
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
    // Only available when EnableWFC is true / 仅在 EnableWFC 为 true 时可用
    // =========================================================================

    template<bool E = EnableWFC, typename = std::enable_if_t<E>>
    void MarkWFCComplete(int64_t slot) noexcept {
        if (m_header && m_slotStatus && slot >= 0 && 
            static_cast<size_t>(slot) < m_header->capacity) {
            m_slotStatus[static_cast<size_t>(slot)].CompleteWFC();
        }
    }

    template<bool E = EnableWFC, typename = std::enable_if_t<E>>
    bool WaitForCompletion(int64_t slot, std::chrono::milliseconds timeout) noexcept {
        if (!m_header || !m_slotStatus || slot < 0 || 
            static_cast<size_t>(slot) >= m_header->capacity) { return false; }
        
        size_t idx = static_cast<size_t>(slot);
        auto deadline = std::chrono::steady_clock::now() + timeout;
        int spinCount = 0;
        
        while (std::chrono::steady_clock::now() < deadline) {
            if (m_slotStatus[idx].IsWFCCompleted()) { return true; }
            spinCount = SpinWait(spinCount);
        }
        return m_slotStatus[idx].IsWFCCompleted();
    }

    template<bool E = EnableWFC, typename = std::enable_if_t<E>>
    WFCState GetWFCState(int64_t slot) const noexcept {
        if (!m_header || !m_slotStatus || slot < 0 || 
            static_cast<size_t>(slot) >= m_header->capacity) { return kWFCNone; }
        return m_slotStatus[static_cast<size_t>(slot)].GetWFCState();
    }

    // =========================================================================
    // Status Query / 状态查询
    // =========================================================================

    bool IsEmpty() const noexcept {
        if (!m_header || !m_slotStatus) { return true; }
        size_t tail = m_header->tail.load(std::memory_order_relaxed);
        size_t slot = tail % m_header->capacity;
        // Check if next slot is ready - only reads one cache line
        // 检查下一个槽位是否就绪 - 只读取一个缓存行
        return !m_slotStatus[slot].IsReady();
    }

    bool IsFull() const noexcept {
        if (!m_header) { return false; }
        size_t tail = m_header->tail.load(std::memory_order_acquire);
        size_t head = m_header->head.load(std::memory_order_acquire);
        return head - tail >= m_header->capacity;
    }

    size_t Size() const noexcept {
        if (!m_header) { return 0; }
        size_t tail = m_header->tail.load(std::memory_order_acquire);
        size_t head = m_header->head.load(std::memory_order_acquire);
        return (head > tail) ? (head - tail) : 0;
    }

    size_t Capacity() const noexcept { 
        return m_header ? m_header->capacity : 0; 
    }
    
    ConsumerState GetConsumerState() const noexcept { 
        return m_header ? m_header->consumerState.load(std::memory_order_acquire) 
                        : ConsumerState::Active; 
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

    bool WaitForData(std::chrono::microseconds pollInterval,
                     std::chrono::milliseconds pollTimeout) noexcept {
        if (!m_header) { return false; }
        
        m_header->consumerState.store(ConsumerState::Active, std::memory_order_release);
        
        // Phase 1: Spin poll with exponential backoff
        // 阶段1：使用指数退避的自旋轮询
        auto pollEnd = std::chrono::steady_clock::now() + pollInterval;
        int spinCount = 0;
        while (std::chrono::steady_clock::now() < pollEnd) {
            if (!IsEmpty()) { return true; }
            spinCount = SpinWait(spinCount);
        }
        
        if (!IsEmpty()) { return true; }
        
        // Phase 2: Enter waiting state
        // 阶段2：进入等待状态
        m_header->consumerState.store(ConsumerState::WaitingNotify, std::memory_order_release);
        
        // Double-check
        if (!IsEmpty()) {
            m_header->consumerState.store(ConsumerState::Active, std::memory_order_release);
            return true;
        }
        
        // Phase 3: Wait for notification
        // 阶段3：等待通知
        bool hasData = DoWait(pollTimeout);
        m_header->consumerState.store(ConsumerState::Active, std::memory_order_release);
        
        return hasData || !IsEmpty();
    }

protected:
    // Protected constructor for derived classes
    RingBufferBase() = default;
    
    // Initialize with external memory (for SharedRingBuffer)
    void InitWithExternalMemory(RingBufferHeader* header, T* buffer, 
                                SlotStatus* slotStatus, bool initHeader,
                                size_t capacity, QueueFullPolicy policy) noexcept {
        m_header = header;
        m_buffer = buffer;
        m_slotStatus = slotStatus;
        
        if (initHeader && m_header) {
            m_header->Init(capacity, policy);
        }
        
        if (initHeader && m_slotStatus) {
            for (size_t i = 0; i < capacity; ++i) {
                m_slotStatus[i].Reset();
            }
        }
    }
    
    // Initialize notification mechanism
    void InitNotification() noexcept {
#ifdef __linux__
        if (m_eventFd < 0) {
            m_eventFd = eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE);
            m_ownsEventFd = true;
        }
#elif defined(__APPLE__)
        if (m_semaphore == nullptr) {
            m_semaphore = dispatch_semaphore_create(0);
            m_ownsSemaphore = true;
        }
#endif
    }
    
    void CleanupNotification() noexcept {
#ifdef __linux__
        if (m_ownsEventFd && m_eventFd >= 0) {
            close(m_eventFd);
            m_eventFd = -1;
        }
#elif defined(__APPLE__)
        if (m_ownsSemaphore && m_semaphore != nullptr) {
            m_semaphore = nullptr;
        }
#endif
    }

    bool DropOldestEntry() noexcept {
        if (!m_header || !m_slotStatus) { return false; }
        
        size_t tail = m_header->tail.load(std::memory_order_relaxed);
        size_t head = m_header->head.load(std::memory_order_acquire);
        
        if (tail >= head) { return false; }
        
        size_t slot = tail % m_header->capacity;
        
        // WFC check only when EnableWFC is true
        if constexpr (EnableWFC) {
            if (m_slotStatus[slot].IsWFCEnabled()) { return false; }
        }
        if (!m_slotStatus[slot].TryStartRead()) { return false; }
        
        m_slotStatus[slot].CompleteRead();
        
        // Use release to ensure slot cleanup is visible before tail update
        if (m_header->tail.compare_exchange_strong(tail, tail + 1,
                                                    std::memory_order_release,
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
        if (m_semaphore != nullptr) {
            dispatch_semaphore_signal(m_semaphore);
        }
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
            dispatch_time_t dt = dispatch_time(
                DISPATCH_TIME_NOW, 
                static_cast<int64_t>(timeout.count()) * NSEC_PER_MSEC);
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

}  // namespace oneplog
