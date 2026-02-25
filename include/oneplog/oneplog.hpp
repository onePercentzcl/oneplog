/**
 * @file oneplog.hpp
 * @brief Main header file for onePlog - High-performance C++ logging library
 * @brief onePlog 主头文件 - 高性能 C++ 日志库
 *
 * Include this file to use all onePlog features. This is the recommended
 * single-include header for most use cases.
 * 包含此文件以使用所有 onePlog 功能。这是大多数用例推荐的单一包含头文件。
 *
 * @section features Features / 功能特性
 * - Three operating modes: Sync, Async, MProc (multi-process)
 *   三种运行模式：同步、异步、多进程
 * - Compile-time configuration for zero-overhead abstraction
 *   编译期配置实现零开销抽象
 * - Lock-free ring buffer for high-throughput async logging
 *   无锁环形队列实现高吞吐量异步日志
 * - Shared memory support for cross-process logging
 *   共享内存支持跨进程日志
 *
 * @section usage Basic Usage / 基本用法
 * @code
 * #include <oneplog/oneplog.hpp>
 * 
 * int main() {
 *     oneplog::AsyncLogger logger;
 *     logger.Info("Hello, {}!", "world");
 *     return 0;
 * }
 * @endcode
 *
 * @see LoggerImpl for the main logger class
 * @see LoggerConfig for compile-time configuration options
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

// Core types / 核心类型
#include "oneplog/common.hpp"
#include "oneplog/internal/binary_snapshot.hpp"
#include "oneplog/internal/log_entry.hpp"

// Ring buffers / 环形队列
#include "oneplog/internal/heap_memory.hpp"

#ifndef ONEPLOG_SYNC_ONLY
#include "oneplog/internal/shared_memory.hpp"
#endif

// Formatting and output / 格式化和输出
#include "oneplog/internal/static_formats.hpp"
#include "oneplog/internal/logger_config.hpp"

// Threads / 线程
#ifndef ONEPLOG_SYNC_ONLY
#include "oneplog/internal/pipeline_thread.hpp"
#endif

// Logger / 日志器
#include "oneplog/logger.hpp"

