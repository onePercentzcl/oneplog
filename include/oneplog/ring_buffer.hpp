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

namespace oneplog::internal {
/**
 * @brief State of the consumer thread.
 * @brief 消费者线程的状态。
 */
enum class ConsumerState : uint8_t {
    Active, ///< Consumer is processing / 消费者正在处理
    Waiting ///< Consumer is sleeping/waiting / 消费者正在等待/休眠
};

/**
 * @brief State of a single slot in the buffer.
 * @brief 缓冲区中单个槽位的状态。
 */
enum class SlotState : uint8_t {
    Empty = 0,   ///< Slot is free to write / 槽位空闲，可写入
    Writing = 1, ///< Producer is writing / 生产者正在写入
    Ready = 2,   ///< Data is ready to read / 数据已准备好读取
    Reading = 3  ///< Consumer is reading / 消费者正在读取
};

/**
 * @brief Wait-Free Consolidation state.
 * @brief 无锁合并状态。
 */
enum class WFCState : uint8_t {
    None = 0,     ///< No consolidation / 无合并
    Enabled = 1,  ///< Consolidation enabled / 开启合并
    Completed = 2 ///< Consolidation completed / 合并完成
};

/**
 * @brief Policy when the queue is full.
 * @brief 队列已满时的策略。
 */
enum class QueueFullPolicy : uint8_t {
    Brock = 0,      ///< Block producer until space available / 阻塞生产者直到有空间
    DropNewest = 1, ///< Drop the log being currently written / 丢弃当前正在写入的日志
    DropOldest = 2  ///< Overwrite the oldest log (Not supported in MPSC yet) / 覆盖最旧的日志（MPSC 暂不支持）
};

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
    static constexpr size_t kHeaderSize = sizeof(std::atomic<SlotState>) + sizeof(std::atomic<WFCState>);

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
     * @brief Actual data buffer.
     * @brief 实际数据缓冲区。
     * @details Fills the remaining space.
     * 填充剩余空间。
     */
    char m_item[slotSize - kHeaderSize];
};

/**
 * @brief Header containing control variables for the RingBuffer.
 * @brief 包含 RingBuffer 控制变量的头部。
 * * @details Implements "Hot/Cold Splitting" to separate Producer-owned and Consumer-owned variables.
 * @details 实现了“冷热分离”，将生产者拥有和消费者拥有的变量物理隔离。
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
     * @note 对齐以避免来自 m_head 或 m_tail 的“吵闹邻居”效应。
     */
    alignas(kCacheLineSize) std::atomic<ConsumerState> m_consumerState{ConsumerState::Active};

    // === Cold / Less Contended Data / 冷数据或低竞争数据 ===

    std::atomic<uint64_t> m_droppedCount{0}; ///< Counter for dropped logs / 丢包计数器

    size_t m_capacity{0};                                 ///< Buffer capacity / 缓冲区容量
    QueueFullPolicy m_fullPolicy{QueueFullPolicy::Brock}; ///< Full policy / 满策略

    // Padding ensures the data section starts at a new cache line boundary.
    // 填充确保数据区从新的缓存行边界开始。
    char m_padding[kCacheLineSize - sizeof(size_t) - sizeof(QueueFullPolicy)];
};

/**
 * @brief The Ring Buffer Class.
 * @brief 环形缓冲区类。
 * * @tparam T Type of data item.
 * @tparam Size Buffer capacity (Should be power of 2 for performance, e.g., 1024).
 */
template <size_t slotSize = 512, size_t slotCount=1024>
class alignas(kCacheLineSize) RingBuffer {
public:
    /**
     * @brief Initialize the ring buffer header.
     * @brief 初始化环形缓冲区头部。
     */
    void Init() {
        m_header.m_capacity = slotCount;
        // Additional init logic...
    }

private:
    RingBufferHeader m_header{};       ///< Control header / 控制头部
    Slot<slotSize> m_slots[slotCount]; ///< Data slots array / 数据槽位数组
};
} // namespace oneplog::internal