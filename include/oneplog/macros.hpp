/**
 * @file macros.hpp
 * @brief Logging macros for onePlog
 * @brief onePlog 日志宏定义
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include "oneplog/logger.hpp"

// ==============================================================================
// Basic Logging Macros / 基本日志宏
// ==============================================================================

/**
 * @brief Log at TRACE level
 * @brief 以 TRACE 级别记录日志
 */
#ifndef ONEPLOG_DISABLE_TRACE
#define ONEPLOG_TRACE(...) \
    do { \
        auto logger = ::oneplog::DefaultLogger(); \
        if (logger) { \
            logger->Trace(__VA_ARGS__); \
        } \
    } while (0)
#else
#define ONEPLOG_TRACE(...) ((void)0)
#endif

/**
 * @brief Log at DEBUG level
 * @brief 以 DEBUG 级别记录日志
 */
#ifndef ONEPLOG_DISABLE_DEBUG
#define ONEPLOG_DEBUG(...) \
    do { \
        auto logger = ::oneplog::DefaultLogger(); \
        if (logger) { \
            logger->Debug(__VA_ARGS__); \
        } \
    } while (0)
#else
#define ONEPLOG_DEBUG(...) ((void)0)
#endif

/**
 * @brief Log at INFO level
 * @brief 以 INFO 级别记录日志
 */
#ifndef ONEPLOG_DISABLE_INFO
#define ONEPLOG_INFO(...) \
    do { \
        auto logger = ::oneplog::DefaultLogger(); \
        if (logger) { \
            logger->Info(__VA_ARGS__); \
        } \
    } while (0)
#else
#define ONEPLOG_INFO(...) ((void)0)
#endif

/**
 * @brief Log at WARN level
 * @brief 以 WARN 级别记录日志
 */
#ifndef ONEPLOG_DISABLE_WARN
#define ONEPLOG_WARN(...) \
    do { \
        auto logger = ::oneplog::DefaultLogger(); \
        if (logger) { \
            logger->Warn(__VA_ARGS__); \
        } \
    } while (0)
#else
#define ONEPLOG_WARN(...) ((void)0)
#endif

/**
 * @brief Log at ERROR level
 * @brief 以 ERROR 级别记录日志
 */
#ifndef ONEPLOG_DISABLE_ERROR
#define ONEPLOG_ERROR(...) \
    do { \
        auto logger = ::oneplog::DefaultLogger(); \
        if (logger) { \
            logger->Error(__VA_ARGS__); \
        } \
    } while (0)
#else
#define ONEPLOG_ERROR(...) ((void)0)
#endif

/**
 * @brief Log at CRITICAL level
 * @brief 以 CRITICAL 级别记录日志
 */
#ifndef ONEPLOG_DISABLE_CRITICAL
#define ONEPLOG_CRITICAL(...) \
    do { \
        auto logger = ::oneplog::DefaultLogger(); \
        if (logger) { \
            logger->Critical(__VA_ARGS__); \
        } \
    } while (0)
#else
#define ONEPLOG_CRITICAL(...) ((void)0)
#endif

// ==============================================================================
// WFC Logging Macros / WFC 日志宏
// ==============================================================================

#ifndef ONEPLOG_DISABLE_TRACE
#define ONEPLOG_TRACE_WFC(...) \
    do { \
        auto logger = ::oneplog::DefaultLogger(); \
        if (logger) { \
            logger->TraceWFC(__VA_ARGS__); \
        } \
    } while (0)
#else
#define ONEPLOG_TRACE_WFC(...) ((void)0)
#endif

#ifndef ONEPLOG_DISABLE_DEBUG
#define ONEPLOG_DEBUG_WFC(...) \
    do { \
        auto logger = ::oneplog::DefaultLogger(); \
        if (logger) { \
            logger->DebugWFC(__VA_ARGS__); \
        } \
    } while (0)
#else
#define ONEPLOG_DEBUG_WFC(...) ((void)0)
#endif

#ifndef ONEPLOG_DISABLE_INFO
#define ONEPLOG_INFO_WFC(...) \
    do { \
        auto logger = ::oneplog::DefaultLogger(); \
        if (logger) { \
            logger->InfoWFC(__VA_ARGS__); \
        } \
    } while (0)
#else
#define ONEPLOG_INFO_WFC(...) ((void)0)
#endif

#ifndef ONEPLOG_DISABLE_WARN
#define ONEPLOG_WARN_WFC(...) \
    do { \
        auto logger = ::oneplog::DefaultLogger(); \
        if (logger) { \
            logger->WarnWFC(__VA_ARGS__); \
        } \
    } while (0)
#else
#define ONEPLOG_WARN_WFC(...) ((void)0)
#endif

#ifndef ONEPLOG_DISABLE_ERROR
#define ONEPLOG_ERROR_WFC(...) \
    do { \
        auto logger = ::oneplog::DefaultLogger(); \
        if (logger) { \
            logger->ErrorWFC(__VA_ARGS__); \
        } \
    } while (0)
#else
#define ONEPLOG_ERROR_WFC(...) ((void)0)
#endif

#ifndef ONEPLOG_DISABLE_CRITICAL
#define ONEPLOG_CRITICAL_WFC(...) \
    do { \
        auto logger = ::oneplog::DefaultLogger(); \
        if (logger) { \
            logger->CriticalWFC(__VA_ARGS__); \
        } \
    } while (0)
#else
#define ONEPLOG_CRITICAL_WFC(...) ((void)0)
#endif

// ==============================================================================
// Conditional Logging Macros / 条件日志宏
// ==============================================================================

#define ONEPLOG_TRACE_IF(condition, ...) \
    do { if (condition) { ONEPLOG_TRACE(__VA_ARGS__); } } while (0)

#define ONEPLOG_DEBUG_IF(condition, ...) \
    do { if (condition) { ONEPLOG_DEBUG(__VA_ARGS__); } } while (0)

#define ONEPLOG_INFO_IF(condition, ...) \
    do { if (condition) { ONEPLOG_INFO(__VA_ARGS__); } } while (0)

#define ONEPLOG_WARN_IF(condition, ...) \
    do { if (condition) { ONEPLOG_WARN(__VA_ARGS__); } } while (0)

#define ONEPLOG_ERROR_IF(condition, ...) \
    do { if (condition) { ONEPLOG_ERROR(__VA_ARGS__); } } while (0)

#define ONEPLOG_CRITICAL_IF(condition, ...) \
    do { if (condition) { ONEPLOG_CRITICAL(__VA_ARGS__); } } while (0)

// ==============================================================================
// Compile-time Log Level / 编译时日志级别
// ==============================================================================

#ifndef ONEPLOG_ACTIVE_LEVEL
#define ONEPLOG_ACTIVE_LEVEL 0
#endif

#if ONEPLOG_ACTIVE_LEVEL > 0
#ifndef ONEPLOG_DISABLE_TRACE
#define ONEPLOG_DISABLE_TRACE
#endif
#endif

#if ONEPLOG_ACTIVE_LEVEL > 1
#ifndef ONEPLOG_DISABLE_DEBUG
#define ONEPLOG_DISABLE_DEBUG
#endif
#endif

#if ONEPLOG_ACTIVE_LEVEL > 2
#ifndef ONEPLOG_DISABLE_INFO
#define ONEPLOG_DISABLE_INFO
#endif
#endif

#if ONEPLOG_ACTIVE_LEVEL > 3
#ifndef ONEPLOG_DISABLE_WARN
#define ONEPLOG_DISABLE_WARN
#endif
#endif

#if ONEPLOG_ACTIVE_LEVEL > 4
#ifndef ONEPLOG_DISABLE_ERROR
#define ONEPLOG_DISABLE_ERROR
#endif
#endif

#if ONEPLOG_ACTIVE_LEVEL > 5
#ifndef ONEPLOG_DISABLE_CRITICAL
#define ONEPLOG_DISABLE_CRITICAL
#endif
#endif

