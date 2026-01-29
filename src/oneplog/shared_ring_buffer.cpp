/**
 * @file shared_ring_buffer.cpp
 * @brief SharedRingBuffer implementation (template instantiations)
 * @brief SharedRingBuffer 实现（模板实例化）
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include "oneplog/shared_ring_buffer.hpp"
#include "oneplog/log_entry.hpp"

namespace oneplog {

// Explicit template instantiations for common types
// 常用类型的显式模板实例化

// LogEntry is the primary use case
// LogEntry 是主要用例
template class SharedRingBuffer<LogEntry>;

}  // namespace oneplog
