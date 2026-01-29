/**
 * @file heap_ring_buffer.hpp
 * @brief HeapRingBuffer - Ring buffer allocated on heap
 * @brief HeapRingBuffer - 在堆上分配的环形队列
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include "oneplog/ring_buffer.hpp"

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
 */
template<typename T>
class HeapRingBuffer : public RingBufferBase<T> {
public:
    using Base = RingBufferBase<T>;

    /**
     * @brief Construct a heap ring buffer
     * @brief 构造堆环形队列
     *
     * @param capacity The maximum number of elements / 最大元素数量
     * @param policy Queue full policy / 队列满策略
     */
    explicit HeapRingBuffer(size_t capacity, 
                            QueueFullPolicy policy = QueueFullPolicy::DropNewest)
        : Base(capacity, policy) {}

    ~HeapRingBuffer() override = default;

    // Inherit all methods from base class
    // 继承基类的所有方法
};

}  // namespace oneplog
