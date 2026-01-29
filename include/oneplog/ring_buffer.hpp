/**
 * @file ring_buffer.hpp
 * @brief Ring buffer slot state and status definitions
 * @brief 环形队列槽位状态定义
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include <atomic>
#include <cstdint>

// Include for std::hardware_destructive_interference_size (C++17)
// 包含 std::hardware_destructive_interference_size 支持（C++17）
#if defined(__cpp_lib_hardware_interference_size) && __cpp_lib_hardware_interference_size >= 201703L
#include <new>
#endif

namespace oneplog {

// ==============================================================================
// Cache Line Size Detection / 缓存行大小检测
// ==============================================================================

// Cache line size (compile-time detection) / 缓存行大小（编译时检测）
// Use C++17 hardware_destructive_interference_size if available
// 如果可用，使用 C++17 的 hardware_destructive_interference_size
#if defined(__cpp_lib_hardware_interference_size) && __cpp_lib_hardware_interference_size >= 201703L
constexpr size_t kCacheLineSize = std::hardware_destructive_interference_size;
#else
// Default to 64 bytes (common for x86/x86_64/ARM)
// 默认 64 字节（x86/x86_64/ARM 常见值）
constexpr size_t kCacheLineSize = 64;
#endif

// ==============================================================================
// CacheLineAligned Template / 缓存行对齐模板
// ==============================================================================

/**
 * @brief Cache-line aligned wrapper to prevent false sharing
 * @brief 缓存行对齐包装器，用于防止伪共享
 *
 * This template wraps a value and ensures it occupies its own cache line,
 * preventing false sharing between threads accessing different variables.
 * 此模板包装一个值并确保它占用自己的缓存行，防止访问不同变量的线程之间的伪共享。
 *
 * @tparam T The type to wrap / 要包装的类型
 *
 * Usage / 用法:
 * @code
 * CacheLineAligned<std::atomic<size_t>> head;
 * head.value.store(0);
 * @endcode
 */
template<typename T>
struct alignas(kCacheLineSize) CacheLineAligned {
    T value;  ///< The wrapped value / 包装的值

    /**
     * @brief Default constructor
     * @brief 默认构造函数
     */
    CacheLineAligned() = default;

    /**
     * @brief Construct with value
     * @brief 使用值构造
     */
    explicit CacheLineAligned(const T& v) : value(v) {}

    /**
     * @brief Construct with value (move)
     * @brief 使用值构造（移动）
     */
    explicit CacheLineAligned(T&& v) : value(std::move(v)) {}

    /**
     * @brief Copy constructor
     * @brief 拷贝构造函数
     */
    CacheLineAligned(const CacheLineAligned& other) : value(other.value) {}

    /**
     * @brief Move constructor
     * @brief 移动构造函数
     */
    CacheLineAligned(CacheLineAligned&& other) noexcept : value(std::move(other.value)) {}

    /**
     * @brief Copy assignment
     * @brief 拷贝赋值
     */
    CacheLineAligned& operator=(const CacheLineAligned& other) {
        if (this != &other) {
            value = other.value;
        }
        return *this;
    }

    /**
     * @brief Move assignment
     * @brief 移动赋值
     */
    CacheLineAligned& operator=(CacheLineAligned&& other) noexcept {
        if (this != &other) {
            value = std::move(other.value);
        }
        return *this;
    }

    /**
     * @brief Implicit conversion to reference
     * @brief 隐式转换为引用
     */
    operator T&() noexcept { return value; }

    /**
     * @brief Implicit conversion to const reference
     * @brief 隐式转换为常量引用
     */
    operator const T&() const noexcept { return value; }

    /**
     * @brief Arrow operator for pointer-like access
     * @brief 箭头运算符，用于类似指针的访问
     */
    T* operator->() noexcept { return &value; }

    /**
     * @brief Arrow operator for pointer-like access (const)
     * @brief 箭头运算符，用于类似指针的访问（常量）
     */
    const T* operator->() const noexcept { return &value; }

private:
    // Padding to ensure the struct occupies a full cache line
    // 填充以确保结构体占用完整的缓存行
    // Note: alignas already handles this, but we add explicit padding for clarity
    // 注意：alignas 已经处理了这个问题，但我们添加显式填充以提高清晰度
    static_assert(sizeof(T) <= kCacheLineSize, 
                  "Type T is larger than cache line size");
};

// ==============================================================================
// Slot State / 槽位状态
// ==============================================================================

/**
 * @brief Slot state enumeration for ring buffer
 * @brief 环形队列槽位状态枚举
 *
 * State transitions / 状态转换:
 * - Empty → Writing (producer acquires slot / 生产者获取槽位)
 * - Writing → Ready (producer commits data / 生产者提交数据)
 * - Ready → Reading (consumer starts reading / 消费者开始读取)
 * - Reading → Empty (consumer completes / 消费者完成)
 */
enum class SlotState : uint8_t {
    Empty = 0,    ///< Slot is empty and available / 槽位空闲可用
    Writing = 1,  ///< Producer is writing data / 生产者正在写入
    Ready = 2,    ///< Data is ready for consumption / 数据就绪待消费
    Reading = 3   ///< Consumer is reading data / 消费者正在读取
};

/**
 * @brief WFC (Wait For Completion) state
 * @brief WFC（等待完成）状态
 *
 * Values:
 * - 1: WFC enabled, waiting for completion / WFC 启用，等待完成
 * - 2: WFC completed / WFC 完成
 * - Other: No WFC operation needed / 其它值表示无需 WFC 操作
 */
using WFCState = uint8_t;

constexpr WFCState kWFCNone = 0;       ///< No WFC operation / 无需 WFC 操作
constexpr WFCState kWFCEnabled = 1;    ///< WFC enabled / WFC 启用
constexpr WFCState kWFCCompleted = 2;  ///< WFC completed / WFC 完成

/**
 * @brief Slot status structure for ring buffer
 * @brief 环形队列槽位状态结构
 *
 * Contains two cache-line-aligned atomic flags:
 * 包含两个缓存行对齐的原子标志位：
 * 1. state: Slot state (Empty/Writing/Ready/Reading) / 槽位状态
 * 2. wfc: WFC state (0=None, 1=Enabled, 2=Completed) / WFC 状态
 *
 * Each flag occupies its own cache line to prevent false sharing.
 * 每个标志位独占一个缓存行以防止伪共享。
 *
 * WFC workflow in multi-process mode:
 * 多进程模式下的 WFC 工作流程：
 * 1. Source writes to heap queue, sets wfc=1, monitors this flag
 *    Source 写入 heap 队列，wfc 置 1，监控此状态位
 * 2. Pipeline sees wfc=1, writes to shared queue with wfc=1, monitors shared queue wfc
 *    Pipeline 看到 wfc=1，向 shared 队列写入 wfc=1，监控 shared 队列 wfc
 * 3. Writer sees wfc=1, after output completes, sets wfc=2
 *    Writer 看到 wfc=1，输出完成后将 wfc 置 2
 * 4. Pipeline sees wfc=2 in shared queue, sets heap queue wfc=2
 *    Pipeline 发现 shared 队列 wfc 变为 2，将 heap 队列 wfc 置 2
 * 5. Source is awakened and continues execution
 *    Source 被唤醒，继续执行
 * 6. When reusing this slot for non-WFC log, consumer ignores wfc=2 flag
 *    再次使用此槽位写入非 WFC 日志时，消费者看到 wfc=2 也不执行 WFC 操作
 */
struct alignas(kCacheLineSize) SlotStatus {
    // Slot state (cache-line aligned) / 槽位状态（缓存行对齐）
    alignas(kCacheLineSize) std::atomic<SlotState> state{SlotState::Empty};
    
    // WFC state (cache-line aligned) / WFC 状态（缓存行对齐）
    alignas(kCacheLineSize) std::atomic<WFCState> wfc{kWFCNone};

    /**
     * @brief Default constructor
     * @brief 默认构造函数
     */
    SlotStatus() = default;

    /**
     * @brief Copy constructor (deleted due to atomic members)
     * @brief 拷贝构造函数（由于原子成员而删除）
     */
    SlotStatus(const SlotStatus&) = delete;

    /**
     * @brief Copy assignment (deleted due to atomic members)
     * @brief 拷贝赋值（由于原子成员而删除）
     */
    SlotStatus& operator=(const SlotStatus&) = delete;

    /**
     * @brief Move constructor
     * @brief 移动构造函数
     */
    SlotStatus(SlotStatus&& other) noexcept
        : state(other.state.load(std::memory_order_relaxed)),
          wfc(other.wfc.load(std::memory_order_relaxed)) {}

    /**
     * @brief Move assignment
     * @brief 移动赋值
     */
    SlotStatus& operator=(SlotStatus&& other) noexcept {
        if (this != &other) {
            state.store(other.state.load(std::memory_order_relaxed), std::memory_order_relaxed);
            wfc.store(other.wfc.load(std::memory_order_relaxed), std::memory_order_relaxed);
        }
        return *this;
    }

    /**
     * @brief Reset slot to empty state
     * @brief 重置槽位为空状态
     */
    void Reset() noexcept {
        state.store(SlotState::Empty, std::memory_order_release);
        wfc.store(kWFCNone, std::memory_order_release);
    }

    /**
     * @brief Check if slot is empty
     * @brief 检查槽位是否为空
     */
    bool IsEmpty() const noexcept {
        return state.load(std::memory_order_acquire) == SlotState::Empty;
    }

    /**
     * @brief Check if slot is ready for reading
     * @brief 检查槽位是否就绪可读
     */
    bool IsReady() const noexcept {
        return state.load(std::memory_order_acquire) == SlotState::Ready;
    }

    /**
     * @brief Check if WFC is enabled for this slot
     * @brief 检查此槽位是否启用 WFC
     */
    bool IsWFCEnabled() const noexcept {
        return wfc.load(std::memory_order_acquire) == kWFCEnabled;
    }

    /**
     * @brief Check if WFC is completed for this slot
     * @brief 检查此槽位 WFC 是否完成
     */
    bool IsWFCCompleted() const noexcept {
        return wfc.load(std::memory_order_acquire) == kWFCCompleted;
    }

    /**
     * @brief Try to acquire slot for writing
     * @brief 尝试获取槽位用于写入
     *
     * @return true if successfully acquired / 如果成功获取则返回 true
     */
    bool TryAcquire() noexcept {
        SlotState expected = SlotState::Empty;
        return state.compare_exchange_strong(expected, SlotState::Writing,
                                             std::memory_order_acq_rel,
                                             std::memory_order_relaxed);
    }

    /**
     * @brief Commit data after writing (set state to Ready)
     * @brief 写入后提交数据（将状态设置为 Ready）
     */
    void Commit() noexcept {
        state.store(SlotState::Ready, std::memory_order_release);
    }

    /**
     * @brief Try to start reading from slot
     * @brief 尝试开始从槽位读取
     *
     * @return true if successfully started reading / 如果成功开始读取则返回 true
     */
    bool TryStartRead() noexcept {
        SlotState expected = SlotState::Ready;
        return state.compare_exchange_strong(expected, SlotState::Reading,
                                             std::memory_order_acq_rel,
                                             std::memory_order_relaxed);
    }

    /**
     * @brief Complete reading and release slot (set state to Empty)
     * @brief 完成读取并释放槽位（将状态设置为 Empty）
     */
    void CompleteRead() noexcept {
        state.store(SlotState::Empty, std::memory_order_release);
    }

    /**
     * @brief Enable WFC for this slot
     * @brief 为此槽位启用 WFC
     */
    void EnableWFC() noexcept {
        wfc.store(kWFCEnabled, std::memory_order_release);
    }

    /**
     * @brief Mark WFC as completed
     * @brief 标记 WFC 为完成
     */
    void CompleteWFC() noexcept {
        wfc.store(kWFCCompleted, std::memory_order_release);
    }

    /**
     * @brief Clear WFC state (set to None)
     * @brief 清除 WFC 状态（设置为 None）
     */
    void ClearWFC() noexcept {
        wfc.store(kWFCNone, std::memory_order_release);
    }

    /**
     * @brief Get current WFC state
     * @brief 获取当前 WFC 状态
     */
    WFCState GetWFCState() const noexcept {
        return wfc.load(std::memory_order_acquire);
    }
};

/**
 * @brief Shared slot status for SharedRingBuffer
 * @brief SharedRingBuffer 的共享槽位状态
 *
 * Same as SlotStatus but explicitly for shared memory usage.
 * 与 SlotStatus 相同，但明确用于共享内存。
 */
using SharedSlotStatus = SlotStatus;

}  // namespace oneplog
