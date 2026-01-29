/**
 * @file oneplog.hpp
 * @brief Main header file for onePlog
 * @brief onePlog 主头文件
 *
 * Include this file to use all onePlog features.
 * 包含此文件以使用所有 onePlog 功能。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

// Core types / 核心类型
#include "oneplog/common.hpp"
#include "oneplog/binary_snapshot.hpp"
#include "oneplog/log_entry.hpp"

// Ring buffers / 环形队列
#include "oneplog/ring_buffer.hpp"
#include "oneplog/heap_ring_buffer.hpp"

#ifndef ONEPLOG_SYNC_ONLY
#include "oneplog/shared_ring_buffer.hpp"
#include "oneplog/shared_memory.hpp"
#endif

// Formatting and output / 格式化和输出
#include "oneplog/format.hpp"
#include "oneplog/sink.hpp"

// Threads / 线程
#ifndef ONEPLOG_SYNC_ONLY
#include "oneplog/pipeline_thread.hpp"
#include "oneplog/writer_thread.hpp"
#endif

// Logger / 日志器
#include "oneplog/logger.hpp"

// Macros / 宏定义
#include "oneplog/macros.hpp"
