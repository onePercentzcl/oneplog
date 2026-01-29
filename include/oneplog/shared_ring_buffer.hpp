/**
 * @file shared_ring_buffer.hpp
 * @brief SharedRingBuffer - Ring buffer in shared memory
 * @brief SharedRingBuffer - 共享内存中的环形队列
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include "oneplog/ring_buffer.hpp"
#include <cstring>

#ifdef __linux__
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace oneplog {

/**
 * @brief Ring buffer in shared memory for multi-process logging
 * @brief 共享内存中的环形队列，用于多进程日志
 *
 * SharedRingBuffer inherits all functionality from RingBufferBase.
 * It's used for multi-process logging where data is stored in shared memory.
 *
 * SharedRingBuffer 继承 RingBufferBase 的所有功能。
 * 用于多进程日志，数据存储在共享内存中。
 *
 * Memory Layout / 内存布局:
 * +---------------------------+
 * | RingBufferHeader          |  (cache-line aligned)
 * +---------------------------+
 * | SlotStatus[0]             |  (cache-line aligned)
 * | SlotStatus[1]             |
 * | ...                       |
 * | SlotStatus[capacity-1]    |
 * +---------------------------+
 * | T[0]                      |  (data buffer)
 * | T[1]                      |
 * | ...                       |
 * | T[capacity-1]             |
 * +---------------------------+
 *
 * @tparam T The element type to store / 要存储的元素类型
 */
template<typename T>
class SharedRingBuffer : public RingBufferBase<T> {
public:
    using Base = RingBufferBase<T>;

    /**
     * @brief Calculate required memory size for shared ring buffer
     * @brief 计算共享环形队列所需的内存大小
     *
     * @param capacity Number of slots / 槽位数量
     * @return Required memory size in bytes / 所需内存大小（字节）
     */
    static size_t CalculateRequiredSize(size_t capacity) noexcept {
        // Header is already cache-line aligned
        size_t headerSize = AlignUp(sizeof(RingBufferHeader), kCacheLineSize);
        
        // SlotStatus is cache-line aligned (each one takes kCacheLineSize * 3 due to internal alignment)
        // sizeof(SlotStatus) already accounts for alignment
        size_t slotStatusSize = AlignUp(sizeof(SlotStatus) * capacity, kCacheLineSize);
        
        // Buffer for T elements
        size_t bufferSize = AlignUp(sizeof(T) * capacity, kCacheLineSize);
        
        return headerSize + slotStatusSize + bufferSize;
    }

    /**
     * @brief Create a new SharedRingBuffer in provided memory
     * @brief 在提供的内存中创建新的 SharedRingBuffer
     *
     * @param memory Pointer to shared memory / 共享内存指针
     * @param memorySize Size of shared memory / 共享内存大小
     * @param capacity Number of slots / 槽位数量
     * @param policy Queue full policy / 队列满策略
     * @return Pointer to created SharedRingBuffer, or nullptr on failure
     */
    static SharedRingBuffer* Create(void* memory, size_t memorySize, size_t capacity,
                                    QueueFullPolicy policy = QueueFullPolicy::DropNewest) {
        if (!memory || capacity == 0) { return nullptr; }
        
        size_t requiredSize = CalculateRequiredSize(capacity);
        if (memorySize < requiredSize) { return nullptr; }
        
        // Calculate offsets
        size_t headerOffset = 0;
        size_t slotStatusOffset = AlignUp(sizeof(RingBufferHeader), kCacheLineSize);
        size_t bufferOffset = slotStatusOffset + AlignUp(sizeof(SlotStatus) * capacity, kCacheLineSize);
        
        // Get pointers
        auto* header = reinterpret_cast<RingBufferHeader*>(
            static_cast<uint8_t*>(memory) + headerOffset);
        auto* slotStatus = reinterpret_cast<SlotStatus*>(
            static_cast<uint8_t*>(memory) + slotStatusOffset);
        auto* buffer = reinterpret_cast<T*>(
            static_cast<uint8_t*>(memory) + bufferOffset);
        
        // Construct SharedRingBuffer in place
        auto* rb = new SharedRingBuffer(memory, memorySize, true);
        rb->InitWithExternalMemory(header, buffer, slotStatus, true, capacity, policy);
        rb->InitNotification();
        
        // Initialize buffer elements with placement new
        for (size_t i = 0; i < capacity; ++i) {
            new (&buffer[i]) T();
        }
        
        return rb;
    }

    /**
     * @brief Connect to an existing SharedRingBuffer in shared memory
     * @brief 连接到共享内存中已存在的 SharedRingBuffer
     *
     * @param memory Pointer to shared memory / 共享内存指针
     * @param memorySize Size of shared memory / 共享内存大小
     * @return Pointer to connected SharedRingBuffer, or nullptr on failure
     */
    static SharedRingBuffer* Connect(void* memory, size_t memorySize) {
        if (!memory) { return nullptr; }
        
        // Validate header
        auto* header = reinterpret_cast<RingBufferHeader*>(memory);
        if (!header->IsValid()) { return nullptr; }
        
        size_t capacity = header->capacity;
        size_t requiredSize = CalculateRequiredSize(capacity);
        if (memorySize < requiredSize) { return nullptr; }
        
        // Calculate offsets
        size_t slotStatusOffset = AlignUp(sizeof(RingBufferHeader), kCacheLineSize);
        size_t bufferOffset = slotStatusOffset + AlignUp(sizeof(SlotStatus) * capacity, kCacheLineSize);
        
        // Get pointers
        auto* slotStatus = reinterpret_cast<SlotStatus*>(
            static_cast<uint8_t*>(memory) + slotStatusOffset);
        auto* buffer = reinterpret_cast<T*>(
            static_cast<uint8_t*>(memory) + bufferOffset);
        
        // Construct SharedRingBuffer
        auto* rb = new SharedRingBuffer(memory, memorySize, false);
        rb->InitWithExternalMemory(header, buffer, slotStatus, false, capacity, header->policy);
        rb->InitNotification();
        
        return rb;
    }

    ~SharedRingBuffer() override {
        if (m_isOwner && Base::m_buffer && Base::m_header) {
            // Destroy buffer elements
            size_t capacity = Base::m_header->capacity;
            for (size_t i = 0; i < capacity; ++i) {
                Base::m_buffer[i].~T();
            }
        }
    }

    // Non-copyable, non-movable (shared memory cannot be moved)
    SharedRingBuffer(const SharedRingBuffer&) = delete;
    SharedRingBuffer& operator=(const SharedRingBuffer&) = delete;
    SharedRingBuffer(SharedRingBuffer&&) = delete;
    SharedRingBuffer& operator=(SharedRingBuffer&&) = delete;

    /**
     * @brief Check if this instance is the owner (creator)
     * @brief 检查此实例是否是所有者（创建者）
     */
    bool IsOwner() const noexcept { return m_isOwner; }

    /**
     * @brief Get the shared memory pointer
     * @brief 获取共享内存指针
     */
    void* GetMemory() const noexcept { return m_memory; }

    /**
     * @brief Get the shared memory size
     * @brief 获取共享内存大小
     */
    size_t GetMemorySize() const noexcept { return m_memorySize; }

private:
    SharedRingBuffer(void* memory, size_t memorySize, bool isOwner)
        : Base()
        , m_memory(memory)
        , m_memorySize(memorySize)
        , m_isOwner(isOwner)
    {}

    static size_t AlignUp(size_t value, size_t alignment) noexcept {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    void* m_memory{nullptr};
    size_t m_memorySize{0};
    bool m_isOwner{false};
};

}  // namespace oneplog
