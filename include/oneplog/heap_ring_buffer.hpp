/**
 * @file heap_ring_buffer.hpp
 * @brief Heap-based Ring Buffer Implementation
 * @brief 基于堆内存的环形缓冲区实现
 *
 * HeapRingBuffer inherits from RingBuffer and provides dynamic heap memory
 * allocation. It is used in Async mode for log transmission between producer
 * threads and the WriterThread.
 *
 * HeapRingBuffer 继承自 RingBuffer，提供动态堆内存分配。
 * 用于异步模式下生产者线程和 WriterThread 之间的日志传输。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include "common.hpp"
#include "ring_buffer.hpp"
#include <memory>
#include <cstdlib>

namespace oneplog::internal {

/**
 * @brief Heap-based Ring Buffer for async logging.
 * @brief 用于异步日志的堆内存环形缓冲区。
 *
 * This class inherits from RingBuffer and adds:
 * - Static factory methods for heap allocation with proper alignment
 * - RAII wrapper for automatic memory management
 *
 * 该类继承自 RingBuffer 并添加：
 * - 用于堆分配的静态工厂方法（带正确对齐）
 * - 用于自动内存管理的 RAII 包装器
 *
 * @tparam slotSize Size of each slot in bytes (must be multiple of kCacheLineSize)
 *                  每个槽位的大小（字节，必须是 kCacheLineSize 的整数倍）
 * @tparam slotCount Number of slots (must be power of 2)
 *                   槽位数量（必须是 2 的幂）
 */
template <size_t slotSize = 512, size_t slotCount = 1024>
class HeapRingBuffer : public RingBuffer<slotSize, slotCount> {
public:
    using Base = RingBuffer<slotSize, slotCount>;
    
    // Inherit constants from base class
    // 从基类继承常量
    using Base::kMaxDataSize;
    using Base::kIndexMask;

    /**
     * @brief Custom deleter for aligned memory.
     * @brief 用于对齐内存的自定义删除器。
     */
    struct AlignedDeleter {
        void operator()(HeapRingBuffer* ptr) const {
            if (ptr) {
                ptr->~HeapRingBuffer();
#if defined(_MSC_VER)
                _aligned_free(ptr);
#else
                free(ptr);
#endif
            }
        }
    };

    /**
     * @brief Unique pointer type for HeapRingBuffer with aligned memory.
     * @brief 带对齐内存的 HeapRingBuffer 唯一指针类型。
     */
    using Ptr = std::unique_ptr<HeapRingBuffer, AlignedDeleter>;

    /**
     * @brief Create a HeapRingBuffer on the heap with proper alignment.
     * @brief 在堆上创建一个正确对齐的 HeapRingBuffer。
     * @param policy Queue full policy / 队列满策略
     * @return Unique pointer to the created buffer, or nullptr on failure
     *         指向创建的缓冲区的唯一指针，失败时返回 nullptr
     */
    static Ptr Create(QueueFullPolicy policy = QueueFullPolicy::DropNewest) {
        void* ptr = AllocateAligned();
        if (!ptr) {
            return nullptr;
        }
        
        // Use placement new to construct
        // 使用 placement new 构造
        auto* buffer = new(ptr) HeapRingBuffer();
        buffer->Init(policy);
        return Ptr(buffer);
    }

    /**
     * @brief Get the total memory size for this buffer type.
     * @brief 获取此缓冲区类型的总内存大小。
     */
    static constexpr size_t GetTotalMemorySize() noexcept {
        return sizeof(HeapRingBuffer);
    }

    /**
     * @brief Default constructor.
     * @brief 默认构造函数。
     * @note Use Create() factory method for heap allocation.
     * @note 使用 Create() 工厂方法进行堆分配。
     */
    HeapRingBuffer() = default;

    /**
     * @brief Destructor.
     * @brief 析构函数。
     */
    ~HeapRingBuffer() = default;

    // Non-copyable / 不可复制
    HeapRingBuffer(const HeapRingBuffer&) = delete;
    HeapRingBuffer& operator=(const HeapRingBuffer&) = delete;

    // Non-movable (due to aligned allocation requirements)
    // 不可移动（由于对齐分配要求）
    HeapRingBuffer(HeapRingBuffer&&) = delete;
    HeapRingBuffer& operator=(HeapRingBuffer&&) = delete;

private:
    /**
     * @brief Allocate aligned memory for HeapRingBuffer.
     * @brief 为 HeapRingBuffer 分配对齐的内存。
     */
    static void* AllocateAligned() {
#if defined(_MSC_VER)
        return _aligned_malloc(sizeof(HeapRingBuffer), kCacheLineSize);
#else
        void* ptr = nullptr;
        if (posix_memalign(&ptr, kCacheLineSize, sizeof(HeapRingBuffer)) != 0) {
            return nullptr;
        }
        return ptr;
#endif
    }
};

// ============================================================================
// Type Aliases / 类型别名
// ============================================================================

/**
 * @brief Default HeapRingBuffer with 512-byte slots and 1024 slots.
 * @brief 默认的 HeapRingBuffer，512 字节槽位，1024 个槽位。
 */
using DefaultHeapRingBuffer = HeapRingBuffer<512, 1024>;

/**
 * @brief Small HeapRingBuffer with 256-byte slots and 256 slots.
 * @brief 小型 HeapRingBuffer，256 字节槽位，256 个槽位。
 */
using SmallHeapRingBuffer = HeapRingBuffer<256, 256>;

/**
 * @brief Large HeapRingBuffer with 1024-byte slots and 4096 slots.
 * @brief 大型 HeapRingBuffer，1024 字节槽位，4096 个槽位。
 */
using LargeHeapRingBuffer = HeapRingBuffer<1024, 4096>;

} // namespace oneplog::internal
