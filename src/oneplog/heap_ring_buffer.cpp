/**
 * @file heap_ring_buffer.cpp
 * @brief HeapRingBuffer implementation (template instantiation)
 * @brief HeapRingBuffer 实现（模板实例化）
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include "oneplog/heap_ring_buffer.hpp"
#include "oneplog/log_entry.hpp"

namespace oneplog {

// Explicit template instantiation for LogEntry
// LogEntry 的显式模板实例化
template class HeapRingBuffer<LogEntry>;

}  // namespace oneplog
