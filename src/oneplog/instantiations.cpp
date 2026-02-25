/**
 * @file instantiations.cpp
 * @brief Explicit template instantiations for common Logger configurations
 * @brief 常用 Logger 配置的显式模板实例化
 *
 * This file provides explicit instantiations for the most commonly used
 * Logger configurations to reduce compile times when using the library
 * as a static or shared library.
 *
 * 此文件为最常用的 Logger 配置提供显式实例化，
 * 以减少使用静态或动态库时的编译时间。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <oneplog/logger.hpp>

namespace oneplog {

// ==============================================================================
// Explicit Instantiations for Sync Mode / 同步模式的显式实例化
// ==============================================================================

// SyncLogger with ConsoleSink / 带控制台输出的同步日志器
template class LoggerImpl<DefaultSyncConfig>;

// SyncLogger with FileSink / 带文件输出的同步日志器
template class LoggerImpl<LoggerConfig<
    Mode::Sync,
    Level::Debug,
    false,  // EnableWFC
    true,   // EnableShadowTail
    true,   // UseFmt
    8192,   // HeapRingBufferCapacity
    8192,   // SharedRingBufferCapacity
    QueueFullPolicy::Block,
    DefaultSharedMemoryName,
    10,     // PollTimeoutMs
    SinkBindingList<SinkBinding<FileSinkType, SimpleFormat>>
>>;

// ==============================================================================
// Explicit Instantiations for Async Mode / 异步模式的显式实例化
// ==============================================================================

// AsyncLogger with ConsoleSink / 带控制台输出的异步日志器
template class LoggerImpl<DefaultAsyncConfig>;

// AsyncLogger with FileSink / 带文件输出的异步日志器
template class LoggerImpl<LoggerConfig<
    Mode::Async,
    Level::Debug,
    false,  // EnableWFC
    true,   // EnableShadowTail
    true,   // UseFmt
    8192,   // HeapRingBufferCapacity
    8192,   // SharedRingBufferCapacity
    QueueFullPolicy::Block,
    DefaultSharedMemoryName,
    10,     // PollTimeoutMs
    SinkBindingList<SinkBinding<FileSinkType, SimpleFormat>>
>>;

// ==============================================================================
// Explicit Instantiations for MProc Mode / 多进程模式的显式实例化
// ==============================================================================

// MProcLogger with ConsoleSink / 带控制台输出的多进程日志器
template class LoggerImpl<DefaultMProcConfig>;

// MProcLogger with FileSink / 带文件输出的多进程日志器
template class LoggerImpl<LoggerConfig<
    Mode::MProc,
    Level::Debug,
    false,  // EnableWFC
    true,   // EnableShadowTail
    true,   // UseFmt
    8192,   // HeapRingBufferCapacity
    8192,   // SharedRingBufferCapacity
    QueueFullPolicy::Block,
    DefaultSharedMemoryName,
    10,     // PollTimeoutMs
    SinkBindingList<SinkBinding<FileSinkType, SimpleFormat>>
>>;

// ==============================================================================
// Explicit Instantiations for High Performance Configs / 高性能配置的显式实例化
// ==============================================================================

// High performance sync logger (minimal overhead)
// 高性能同步日志器（最小开销）
template class LoggerImpl<LoggerConfig<
    Mode::Sync,
    Level::Info,
    false,  // EnableWFC
    false,  // EnableShadowTail (disabled for performance)
    true,   // UseFmt
    8192,   // HeapRingBufferCapacity
    8192,   // SharedRingBufferCapacity
    QueueFullPolicy::Block,
    DefaultSharedMemoryName,
    10,     // PollTimeoutMs
    SinkBindingList<SinkBinding<NullSinkType, SimpleFormat>>
>>;

// High performance async logger (minimal overhead)
// 高性能异步日志器（最小开销）
template class LoggerImpl<LoggerConfig<
    Mode::Async,
    Level::Info,
    false,  // EnableWFC
    false,  // EnableShadowTail (disabled for performance)
    true,   // UseFmt
    8192,   // HeapRingBufferCapacity
    8192,   // SharedRingBufferCapacity
    QueueFullPolicy::Block,
    DefaultSharedMemoryName,
    10,     // PollTimeoutMs
    SinkBindingList<SinkBinding<NullSinkType, SimpleFormat>>
>>;

}  // namespace oneplog
