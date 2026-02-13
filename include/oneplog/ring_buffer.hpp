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
inline int SpinWait(const int spinCount) noexcept {
    if (constexpr int kSpinCount = 1000; spinCount < kSpinCount) {
        // Phase 1: Pure spin / 阶段1：纯自旋
        return spinCount + 1;
    } else if (constexpr int kPauseCount = 2000; spinCount < kPauseCount) {
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

    bool IsEmpty() const noexcept {
        return state.load(std::memory_order_acquire) == SlotState::Empty;
    }
    bool IsReady() const noexcept {
        return state.load(std::memory_order_acquire) == SlotState::Ready;
    }
    bool IsWFCEnabled() const noexcept {
        return wfc.load(std::memory_order_acquire) == kWFCEnabled;
    }
    bool IsWFCCompleted() const noexcept {
        return wfc.load(std::memory_order_acquire) == kWFCCompleted;
    }

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
 *
 * Shadow Tail Optimization / 影子 Tail 优化:
 * - shadowTail is updated by consumer every kShadowTailUpdateInterval (32) pops
 * - Producers read shadowTail instead of tail to reduce cache coherency traffic
 * - This reduces the frequency of cross-core cache line transfers
 *
 * - shadowTail 由消费者每 kShadowTailUpdateInterval (32) 次消费更新一次
 * - 生产者读取 shadowTail 而非 tail，减少缓存一致性流量
 * - 这减少了跨核缓存行传输的频率
 */
struct alignas(kCacheLineSize) RingBufferHeader {
    /// Shadow tail update interval (consumer updates shadowTail every N pops)
    /// 影子 tail 更新间隔（消费者每 N 次消费更新一次 shadowTail）
    static constexpr size_t kShadowTailUpdateInterval = 32;

    alignas(kCacheLineSize) std::atomic<size_t> head{0};
    
    /// Real tail - only updated by consumer, rarely read by producer
    /// 真实 tail - 仅由消费者更新，生产者很少读取
    alignas(kCacheLineSize) std::atomic<size_t> tail{0};
    
    /// Shadow tail - updated every kShadowTailUpdateInterval pops, frequently read by producers
    /// 影子 tail - 每 kShadowTailUpdateInterval 次消费更新，生产者频繁读取
    alignas(kCacheLineSize) std::atomic<size_t> shadowTail{0};
    
    alignas(kCacheLineSize) std::atomic<ConsumerState> consumerState{ConsumerState::Active};
    alignas(kCacheLineSize) std::atomic<uint64_t> droppedCount{0};

    size_t capacity{0};
    QueueFullPolicy policy{QueueFullPolicy::DropNewest};
    uint32_t magic{0};    // Magic number for validation
    uint32_t version{0};  // Version for compatibility check

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
// RingBufferBase - Full Implementation / 环形队列基类（完整实现）
// ==============================================================================

/**
 * @brief Producer local cache for shadow tail optimization
 * @brief 生产者本地缓存，用于影子 tail 优化
 *
 * Each producer thread maintains a local cache of the shadow tail to minimize
 * cross-core cache line reads. The cache includes a skip counter for batch
 * dropping in DropNewest mode when the queue is persistently full.
 *
 * 每个生产者线程维护一个影子 tail 的本地缓存，以最小化跨核缓存行读取。
 * 缓存包含一个跳过计数器，用于在队列持续满时批量丢弃（DropNewest 模式）。
 */
struct ProducerLocalCache {
    size_t cachedShadowTail{0};  ///< Cached shadow tail value / 缓存的影子 tail 值
    size_t skipCount{0};         ///< Remaining logs to skip in drop mode / 丢弃模式下剩余跳过的日志数
    
    /// Number of logs to skip when queue is persistently full
    /// 队列持续满时跳过的日志数
    static constexpr size_t kSkipCountOnPersistentFull = 31;
};

/**
 * @brief Ring buffer base class with full implementation
 * @brief 环形队列基类（包含完整实现）
 *
 * HeapRingBuffer and SharedRingBuffer inherit from this class.
 * The main difference is storage location (heap vs shared memory).
 *
 * Shadow Tail Optimization / 影子 Tail 优化:
 * - Producer first checks thread-local cached shadow tail (no cross-core read)
 * - If queue appears full, reads shadow tail from ring buffer header
 * - If still full and shadow tail unchanged, enters batch skip mode (DropNewest)
 *   or exponential backoff (Block mode)
 *
 * - 生产者首先检查线程本地缓存的影子 tail（无跨核读取）
 * - 如果队列看起来满了，从环形缓冲区头部读取影子 tail
 * - 如果仍然满且影子 tail 未变，进入批量跳过模式（DropNewest）
 *   或指数退避（Block 模式）
 *
 * @tparam T The element type to store / 要存储的元素类型
 * @tparam EnableWFC Enable WFC (Wait For Completion) support / 启用 WFC 支持
 *                   When false, WFC checks are completely eliminated at compile time.
 *                   当为 false 时，WFC 检查在编译时完全消除。
 * @tparam EnableShadowTail Enable shadow tail optimization / 启用影子 tail 优化
 *                          When true (default), uses shadow tail for reduced cache coherency traffic.
 *                          When false, uses original algorithm with real tail (better for low contention).
 *                          当为 true（默认），使用影子 tail 减少缓存一致性流量。
 *                          当为 false，使用原始算法和真实 tail（适合低竞争场景）。
 */
template <typename T, bool EnableWFC = true, bool EnableShadowTail = true>
class RingBufferBase {
   public:
    static constexpr int64_t kInvalidSlot = -1;

    virtual ~RingBufferBase() { CleanupNotification(); }

    // Non-copyable
    RingBufferBase(const RingBufferBase&) = delete;
    RingBufferBase& operator=(const RingBufferBase&) = delete;

    // =========================================================================
    // Configuration / 配置
    // =========================================================================

    void SetPolicy(QueueFullPolicy policy) noexcept {
        if (m_header) {
            m_header->policy = policy;
        }
    }

    QueueFullPolicy GetPolicy() const noexcept {
        return m_header ? m_header->policy : QueueFullPolicy::DropNewest;
    }

    uint64_t GetDroppedCount() const noexcept {
        return m_header ? m_header->droppedCount.load(std::memory_order_relaxed) : 0;
    }

    void ResetDroppedCount() noexcept {
        if (m_header) {
            m_header->droppedCount.store(0, std::memory_order_relaxed);
        }
    }

    // =========================================================================
    // Producer Operations / 生产者操作
    // =========================================================================

    /**
     * @brief Get thread-local producer cache
     * @brief 获取线程本地生产者缓存
     */
    static ProducerLocalCache& GetProducerCache() noexcept {
        thread_local ProducerLocalCache cache;
        return cache;
    }

    /**
     * @brief Acquire a slot for writing
     * @brief 获取一个用于写入的槽位
     *
     * When EnableShadowTail is true:
     * - Uses shadow tail optimization for reduced cache coherency traffic
     * - Better for high contention scenarios
     *
     * When EnableShadowTail is false:
     * - Uses original algorithm with real tail
     * - Better for low contention scenarios (lower overhead)
     *
     * @param isWFC If true, always block when full (WFC logs never dropped)
     */
    int64_t AcquireSlot(bool isWFC = false) noexcept {
        if (!m_header || !m_slotStatus) {
            return kInvalidSlot;
        }

        // When shadow tail is disabled, always use the original algorithm
        // 当禁用影子 tail 时，始终使用原始算法
        if constexpr (!EnableShadowTail) {
            return AcquireSlotSmallQueue(isWFC);
        } else {
            size_t capacity = m_header->capacity;
            
            // For small queues, use the original algorithm with real tail
            // 对于小队列，使用原始算法和真实 tail
            if (capacity <= RingBufferHeader::kShadowTailUpdateInterval) {
                return AcquireSlotSmallQueue(isWFC);
            }

            // For larger queues, use shadow tail optimization
            // 对于较大队列，使用影子 tail 优化
            return AcquireSlotWithShadowTail(isWFC);
        }
    }

private:
    /**
     * @brief Acquire slot for small queues (uses real tail)
     * @brief 为小队列获取槽位（使用真实 tail）
     */
    int64_t AcquireSlotSmallQueue(bool isWFC) noexcept {
        size_t head = m_header->head.load(std::memory_order_relaxed);
        size_t capacity = m_header->capacity;
        int spinCount = 0;

        while (true) {
            size_t tail = m_header->tail.load(std::memory_order_acquire);

            if (head - tail >= capacity) {
                // WFC logs NEVER get dropped
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
                        if (DropOldestEntry()) {
                            continue;
                        }
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

    /**
     * @brief Acquire slot with shadow tail optimization (for larger queues)
     * @brief 使用影子 tail 优化获取槽位（用于较大队列）
     */
    int64_t AcquireSlotWithShadowTail(bool isWFC) noexcept {
        ProducerLocalCache& cache = GetProducerCache();
        size_t capacity = m_header->capacity;

        // Fast path: Check if we should skip this log (batch drop mode)
        // 快速路径：检查是否应该跳过此日志（批量丢弃模式）
        if (cache.skipCount > 0) {
            --cache.skipCount;
            m_header->droppedCount.fetch_add(1, std::memory_order_relaxed);
            return kInvalidSlot;
        }

        size_t head = m_header->head.load(std::memory_order_relaxed);
        int spinCount = 0;

        while (true) {
            // Step 1: Check with cached shadow tail (no cross-core read)
            // 步骤 1：使用缓存的影子 tail 检查（无跨核读取）
            if (head - cache.cachedShadowTail < capacity) {
                // Queue not full according to cache, try to acquire slot
                // 根据缓存队列未满，尝试获取槽位
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
                // CAS failed, retry with updated head
                continue;
            }

            // Step 2: Cache shows full, read shadow tail from header
            // 步骤 2：缓存显示满，从头部读取影子 tail
            size_t newShadowTail = m_header->shadowTail.load(std::memory_order_acquire);
            
            // Check if queue is actually full with fresh shadow tail
            // 使用新的影子 tail 检查队列是否真的满了
            if (head - newShadowTail < capacity) {
                // Queue has space now, update cache and retry
                // 队列现在有空间，更新缓存并重试
                cache.cachedShadowTail = newShadowTail;
                continue;
            }

            // Step 3: Queue is really full, handle based on policy
            // 步骤 3：队列确实满了，根据策略处理

            // WFC logs NEVER get dropped (only when EnableWFC is true)
            // WFC 日志永不丢弃（仅当 EnableWFC 为 true 时）
            if constexpr (EnableWFC) {
                if (isWFC) {
                    spinCount = SpinWait(spinCount);
                    head = m_header->head.load(std::memory_order_relaxed);
                    // Periodically refresh shadow tail cache during spin
                    // 自旋期间周期性刷新影子 tail 缓存
                    if (spinCount % 100 == 0) {
                        cache.cachedShadowTail = m_header->shadowTail.load(std::memory_order_acquire);
                    }
                    continue;
                }
            }

            switch (m_header->policy) {
                case QueueFullPolicy::DropNewest: {
                    // Check if shadow tail is unchanged (queue persistently full)
                    // 检查影子 tail 是否未变（队列持续满）
                    if (newShadowTail == cache.cachedShadowTail) {
                        // Shadow tail unchanged, enter batch skip mode
                        // 影子 tail 未变，进入批量跳过模式
                        cache.skipCount = ProducerLocalCache::kSkipCountOnPersistentFull;
                    }
                    // Update cache with new shadow tail
                    cache.cachedShadowTail = newShadowTail;
                    m_header->droppedCount.fetch_add(1, std::memory_order_relaxed);
                    return kInvalidSlot;
                }

                case QueueFullPolicy::DropOldest: {
                    // For DropOldest, we need to read real tail to drop oldest entry
                    // 对于 DropOldest，需要读取真实 tail 来丢弃最旧条目
                    size_t tail = m_header->tail.load(std::memory_order_acquire);
                    size_t oldestSlot = tail % capacity;
                    
                    // WFC check only when EnableWFC is true
                    if constexpr (EnableWFC) {
                        if (m_slotStatus[oldestSlot].IsWFCEnabled()) {
                            spinCount = SpinWait(spinCount);
                            head = m_header->head.load(std::memory_order_relaxed);
                            continue;
                        }
                    }
                    if (DropOldestEntry()) {
                        // Update cache after successful drop
                        cache.cachedShadowTail = m_header->shadowTail.load(std::memory_order_relaxed);
                        continue;
                    }
                    spinCount = SpinWait(spinCount);
                    head = m_header->head.load(std::memory_order_relaxed);
                    continue;
                }

                case QueueFullPolicy::Block:
                default: {
                    // Exponential backoff with periodic shadow tail refresh
                    // 指数退避，周期性刷新影子 tail
                    spinCount = SpinWait(spinCount);
                    head = m_header->head.load(std::memory_order_relaxed);
                    
                    // Refresh shadow tail cache periodically during spin
                    // 自旋期间周期性刷新影子 tail 缓存
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
        if (!m_header || !m_buffer || slot < 0 || static_cast<size_t>(slot) >= m_header->capacity) {
            return;
        }
        size_t idx = static_cast<size_t>(slot);
        m_buffer[idx] = std::move(item);
        m_slotStatus[idx].Commit();
    }

    void CommitSlot(int64_t slot, const T& item) noexcept {
        if (!m_header || !m_buffer || slot < 0 || static_cast<size_t>(slot) >= m_header->capacity) {
            return;
        }
        size_t idx = static_cast<size_t>(slot);
        m_buffer[idx] = item;
        m_slotStatus[idx].Commit();
    }

    bool TryPush(T&& item) noexcept {
        int64_t slot = AcquireSlot(false);
        if (slot == kInvalidSlot) {
            return false;
        }
        CommitSlot(slot, std::move(item));
        NotifyConsumerIfWaiting();
        return true;
    }

    bool TryPush(const T& item) noexcept {
        // ReSharper disable once CppLocalVariableMayBeConst
        int64_t slot = AcquireSlot(false);
        if (slot == kInvalidSlot) {
            return false;
        }
        CommitSlot(slot, item);
        NotifyConsumerIfWaiting();
        return true;
    }

    /**
     * @brief Push with WFC - NEVER dropped, blocks if queue is full
     * @note Only available when EnableWFC is true
     */
    template <bool E = EnableWFC, typename = std::enable_if_t<E>>
    int64_t TryPushWFC(T&& item) noexcept {
        int64_t slot = AcquireSlot(true);
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

    template <bool E = EnableWFC, typename = std::enable_if_t<E>>
    int64_t TryPushWFC(const T& item) noexcept {
        int64_t slot = AcquireSlot(true);
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
     * @brief 尝试从队列弹出一个元素
     *
     * When EnableShadowTail is true:
     * - Shadow tail is updated every kShadowTailUpdateInterval (32) pops
     * - This reduces the frequency of writes that producers need to observe
     *
     * When EnableShadowTail is false:
     * - Only real tail is updated (original behavior)
     */
    bool TryPop(T& item) noexcept {
        if (!m_header || !m_buffer || !m_slotStatus) {
            return false;
        }

        size_t tail = m_header->tail.load(std::memory_order_relaxed);
        size_t slot = tail % m_header->capacity;

        // Check slot state directly - only reads one cache line
        // 直接检查槽位状态 - 只读取一个缓存行
        if (!m_slotStatus[slot].TryStartRead()) {
            return false;
        }

        item = std::move(m_buffer[slot]);

        // WFC check is completely eliminated at compile time when EnableWFC is false
        // 当 EnableWFC 为 false 时，WFC 检查在编译时完全消除
        if constexpr (EnableWFC) {
            bool hasWFC = m_slotStatus[slot].IsWFCEnabled();
            m_slotStatus[slot].CompleteRead();
            if (hasWFC) {
                m_slotStatus[slot].CompleteWFC();
            }
        } else {
            m_slotStatus[slot].CompleteRead();
        }

        // Update real tail
        // 更新真实 tail
        size_t newTail = tail + 1;
        m_header->tail.store(newTail, std::memory_order_release);

        // Update shadow tail only when EnableShadowTail is true
        // 仅当 EnableShadowTail 为 true 时更新影子 tail
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
    // Only available when EnableWFC is true / 仅在 EnableWFC 为 true 时可用
    // =========================================================================

    template <bool E = EnableWFC, typename = std::enable_if_t<E>>
    void MarkWFCComplete(int64_t slot) noexcept {
        if (m_header && m_slotStatus && slot >= 0 &&
            static_cast<size_t>(slot) < m_header->capacity) {
            m_slotStatus[static_cast<size_t>(slot)].CompleteWFC();
        }
    }

    template <bool E = EnableWFC, typename = std::enable_if_t<E>>
    bool WaitForCompletion(int64_t slot, std::chrono::milliseconds timeout) noexcept {
        if (!m_header || !m_slotStatus || slot < 0 ||
            static_cast<size_t>(slot) >= m_header->capacity) {
            return false;
        }

        size_t idx = static_cast<size_t>(slot);
        auto deadline = std::chrono::steady_clock::now() + timeout;
        int spinCount = 0;

        while (std::chrono::steady_clock::now() < deadline) {
            if (m_slotStatus[idx].IsWFCCompleted()) {
                return true;
            }
            spinCount = SpinWait(spinCount);
        }
        return m_slotStatus[idx].IsWFCCompleted();
    }

    template <bool E = EnableWFC, typename = std::enable_if_t<E>>
    WFCState GetWFCState(int64_t slot) const noexcept {
        if (!m_header || !m_slotStatus || slot < 0 ||
            static_cast<size_t>(slot) >= m_header->capacity) {
            return kWFCNone;
        }
        return m_slotStatus[static_cast<size_t>(slot)].GetWFCState();
    }

    // =========================================================================
    // Status Query / 状态查询
    // =========================================================================

    bool IsEmpty() const noexcept {
        if (!m_header || !m_slotStatus) {
            return true;
        }
        size_t tail = m_header->tail.load(std::memory_order_relaxed);
        size_t slot = tail % m_header->capacity;
        // Check if next slot is ready - only reads one cache line
        // 检查下一个槽位是否就绪 - 只读取一个缓存行
        return !m_slotStatus[slot].IsReady();
    }

    bool IsFull() const noexcept {
        if (!m_header) {
            return false;
        }
        size_t head = m_header->head.load(std::memory_order_acquire);
        
        // When shadow tail is disabled, always use real tail
        // 当禁用影子 tail 时，始终使用真实 tail
        if constexpr (!EnableShadowTail) {
            size_t tail = m_header->tail.load(std::memory_order_acquire);
            return head - tail >= m_header->capacity;
        } else {
            // For small queues (capacity <= shadow tail interval), use real tail for accuracy
            // 对于小队列（容量 <= 影子 tail 间隔），使用真实 tail 以保证准确性
            if (m_header->capacity <= RingBufferHeader::kShadowTailUpdateInterval) {
                size_t tail = m_header->tail.load(std::memory_order_acquire);
                return head - tail >= m_header->capacity;
            }
            // For larger queues, use shadow tail for approximate check
            // 对于较大队列，使用影子 tail 进行近似检查
            size_t shadowTail = m_header->shadowTail.load(std::memory_order_acquire);
            return head - shadowTail >= m_header->capacity;
        }
    }

    size_t Size() const noexcept {
        if (!m_header) {
            return 0;
        }
        // Use real tail for accurate size
        // 使用真实 tail 获取准确大小
        size_t tail = m_header->tail.load(std::memory_order_acquire);
        size_t head = m_header->head.load(std::memory_order_acquire);
        return (head > tail) ? (head - tail) : 0;
    }

    /**
     * @brief Get approximate size using shadow tail (faster but less accurate)
     * @brief 使用影子 tail 获取近似大小（更快但不太准确）
     */
    size_t ApproximateSize() const noexcept {
        if (!m_header) {
            return 0;
        }
        size_t shadowTail = m_header->shadowTail.load(std::memory_order_relaxed);
        size_t head = m_header->head.load(std::memory_order_relaxed);
        return (head > shadowTail) ? (head - shadowTail) : 0;
    }

    size_t Capacity() const noexcept { return m_header ? m_header->capacity : 0; }

    ConsumerState GetConsumerState() const noexcept {
        return m_header ? m_header->consumerState.load(std::memory_order_acquire)
                        : ConsumerState::Active;
    }

    // =========================================================================
    // Notification Mechanism / 通知机制
    // =========================================================================

#ifdef __linux__
    int GetEventFD() const noexcept { return m_eventFd; }
    void SetEventFD(int fd) noexcept {
        m_eventFd = fd;
        m_ownsEventFd = false;
    }
#endif

    void NotifyConsumerIfWaiting() noexcept {
        if (m_header && m_header->consumerState.load(std::memory_order_acquire) ==
                            ConsumerState::WaitingNotify) {
            DoNotify();
        }
    }

    void NotifyConsumer() noexcept { DoNotify(); }

    bool WaitForData(std::chrono::microseconds pollInterval,
                     std::chrono::milliseconds pollTimeout) noexcept {
        if (!m_header) {
            return false;
        }

        m_header->consumerState.store(ConsumerState::Active, std::memory_order_release);

        // Phase 1: Spin poll with exponential backoff
        // 阶段1：使用指数退避的自旋轮询
        auto pollEnd = std::chrono::steady_clock::now() + pollInterval;
        int spinCount = 0;
        while (std::chrono::steady_clock::now() < pollEnd) {
            if (!IsEmpty()) {
                return true;
            }
            spinCount = SpinWait(spinCount);
        }

        if (!IsEmpty()) {
            return true;
        }

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
    void InitWithExternalMemory(RingBufferHeader* header, T* buffer, SlotStatus* slotStatus,
                                bool initHeader, size_t capacity, QueueFullPolicy policy) noexcept {
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
        if (!m_header || !m_slotStatus) {
            return false;
        }

        size_t tail = m_header->tail.load(std::memory_order_relaxed);
        size_t head = m_header->head.load(std::memory_order_acquire);

        if (tail >= head) {
            return false;
        }

        size_t slot = tail % m_header->capacity;

        // WFC check only when EnableWFC is true
        if constexpr (EnableWFC) {
            if (m_slotStatus[slot].IsWFCEnabled()) {
                return false;
            }
        }
        if (!m_slotStatus[slot].TryStartRead()) {
            return false;
        }

        m_slotStatus[slot].CompleteRead();

        // Use release to ensure slot cleanup is visible before tail update
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
                DISPATCH_TIME_NOW, static_cast<int64_t>(timeout.count()) * NSEC_PER_MSEC);
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
