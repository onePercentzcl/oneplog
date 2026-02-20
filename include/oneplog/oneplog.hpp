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

