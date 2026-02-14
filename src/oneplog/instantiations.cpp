/**
 * @file instantiations.cpp
 * @brief Explicit template instantiations for non-header-only builds
 * @brief 非仅头文件构建的显式模板实例化
 *
 * This file provides explicit template instantiations to reduce compile times
 * when building oneplog as a static or shared library.
 *
 * 此文件提供显式模板实例化，以减少将 oneplog 构建为静态库或共享库时的编译时间。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#ifndef ONEPLOG_HEADER_ONLY

#include "oneplog/oneplog.hpp"

namespace oneplog {

// ==============================================================================
// RingBuffer instantiations / RingBuffer 实例化
// ==============================================================================

// HeapRingBuffer with LogEntry
template class internal::HeapRingBuffer<LogEntry, true>;
template class internal::HeapRingBuffer<LogEntry, false>;

// SharedRingBuffer with LogEntry
template class internal::SharedRingBuffer<LogEntry, true>;
template class internal::SharedRingBuffer<LogEntry, false>;

// ==============================================================================
// Thread classes instantiations / 线程类实例化
// ==============================================================================

// WriterThread
template class WriterThread<true>;
template class WriterThread<false>;

// PipelineThread
template class PipelineThread<true>;
template class PipelineThread<false>;

// ==============================================================================
// SharedMemory instantiations / SharedMemory 实例化
// ==============================================================================

template class internal::SharedMemory<true>;
template class internal::SharedMemory<false>;

// ==============================================================================
// Logger instantiations / Logger 实例化
// ==============================================================================

// Common Logger configurations
// Async mode (default)
template class Logger<Mode::Async, Level::Trace, false>;
template class Logger<Mode::Async, Level::Debug, false>;
template class Logger<Mode::Async, Level::Info, false>;
template class Logger<Mode::Async, Level::Warn, false>;
template class Logger<Mode::Async, Level::Error, false>;
template class Logger<Mode::Async, Level::Critical, false>;

// Async mode with WFC
template class Logger<Mode::Async, Level::Trace, true>;
template class Logger<Mode::Async, Level::Debug, true>;
template class Logger<Mode::Async, Level::Info, true>;
template class Logger<Mode::Async, Level::Warn, true>;
template class Logger<Mode::Async, Level::Error, true>;
template class Logger<Mode::Async, Level::Critical, true>;

// Sync mode
template class Logger<Mode::Sync, Level::Trace, false>;
template class Logger<Mode::Sync, Level::Debug, false>;
template class Logger<Mode::Sync, Level::Info, false>;
template class Logger<Mode::Sync, Level::Warn, false>;
template class Logger<Mode::Sync, Level::Error, false>;
template class Logger<Mode::Sync, Level::Critical, false>;

// MProc mode
template class Logger<Mode::MProc, Level::Trace, false>;
template class Logger<Mode::MProc, Level::Debug, false>;
template class Logger<Mode::MProc, Level::Info, false>;
template class Logger<Mode::MProc, Level::Warn, false>;
template class Logger<Mode::MProc, Level::Error, false>;
template class Logger<Mode::MProc, Level::Critical, false>;

// MProc mode with WFC
template class Logger<Mode::MProc, Level::Trace, true>;
template class Logger<Mode::MProc, Level::Debug, true>;
template class Logger<Mode::MProc, Level::Info, true>;
template class Logger<Mode::MProc, Level::Warn, true>;
template class Logger<Mode::MProc, Level::Error, true>;
template class Logger<Mode::MProc, Level::Critical, true>;

// ==============================================================================
// NameManager instantiations / NameManager 实例化
// ==============================================================================

template class NameManager<true>;
template class NameManager<false>;

template class ThreadWithModuleName<true>;
template class ThreadWithModuleName<false>;

}  // namespace oneplog

#endif  // ONEPLOG_HEADER_ONLY
