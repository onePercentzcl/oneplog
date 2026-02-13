/**
 * @file heap_ring_buffer.hpp
 * @brief HeapRingBuffer - Ring buffer allocated on heap
 * @brief HeapRingBuffer - 在堆上分配的环形队列
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include "oneplog/ring_buffer.hpp"
#include <memory>

namespace oneplog {

/**
 * @brief Ring buffer allocated on heap for asynchronous logging
 * @brief 在堆上分配的环形队列，用于异步日志
 *
 * HeapRingBuffer inherits all functionality from RingBufferBase.
 * It's used for single-process async logging (Source → HeapRingBuffer → Writer).
 *
 * HeapRingBuffer 继承 RingBufferBase 的所有功能。
 * 用于单进程异步日志（Source → HeapRingBuffer → Writer）。
 *
 * @tparam T The element type to store / 要存储的元素类型
 * @tparam EnableWFC Enable WFC support / 启用 WFC 支持
 * @tparam EnableShadowTail Enable shadow tail optimization / 启用影子 tail 优化
 *                          Default is true for high contention scenarios.
 *                          Set to false for low contention scenarios (lower overhead).
 *                          默认为 true，适合高竞争场景。
 *                          设为 false 适合低竞争场景（更低开销）。
 */
template<typename T, bool EnableWFC = true, bool EnableShadowTail = true>
class HeapRingBuffer : public RingBufferBase<T, EnableWFC, EnableShadowTail> {
public:
    using Base = RingBufferBase<T, EnableWFC, EnableShadowTail>;

    /**
     * @brief Construct a heap ring buffer
     * @brief 构造堆环形队列
     *
     * @param capacity The maximum number of elements / 最大元素数量
     * @param policy Queue full policy / 队列满策略
     */
    explicit HeapRingBuffer(size_t capacity, 
                            QueueFullPolicy policy = QueueFullPolicy::DropNewest)
        : Base()
        , m_headerStorage(std::make_unique<RingBufferHeader>())
        , m_bufferStorage(capacity)
        , m_slotStatusStorage(capacity)
    {
        // Initialize base class with our storage
        Base::InitWithExternalMemory(
            m_headerStorage.get(),
            m_bufferStorage.data(),
            m_slotStatusStorage.data(),
            true,  // initHeader
            capacity,
            policy
        );
        
        // Initialize notification mechanism
        Base::InitNotification();
    }

    ~HeapRingBuffer() override = default;

    // Non-copyable
    HeapRingBuffer(const HeapRingBuffer&) = delete;
    HeapRingBuffer& operator=(const HeapRingBuffer&) = delete;

    // Movable
    HeapRingBuffer(HeapRingBuffer&& other) noexcept
        : Base()
        , m_headerStorage(std::move(other.m_headerStorage))
        , m_bufferStorage(std::move(other.m_bufferStorage))
        , m_slotStatusStorage(std::move(other.m_slotStatusStorage))
    {
        // Update base class pointers
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
        
        // Clear other's pointers
        other.m_header = nullptr;
        other.m_buffer = nullptr;
        other.m_slotStatus = nullptr;
    }

    HeapRingBuffer& operator=(HeapRingBuffer&& other) noexcept {
        if (this != &other) {
            // Clean up current notification
            Base::CleanupNotification();
            
            // Move storage
            m_headerStorage = std::move(other.m_headerStorage);
            m_bufferStorage = std::move(other.m_bufferStorage);
            m_slotStatusStorage = std::move(other.m_slotStatusStorage);
            
            // Update base class pointers
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
            
            // Clear other's pointers
            other.m_header = nullptr;
            other.m_buffer = nullptr;
            other.m_slotStatus = nullptr;
        }
        return *this;
    }

private:
    // Heap-allocated storage
    std::unique_ptr<RingBufferHeader> m_headerStorage;
    std::vector<T> m_bufferStorage;
    std::vector<SlotStatus> m_slotStatusStorage;
};

}  // namespace oneplog
