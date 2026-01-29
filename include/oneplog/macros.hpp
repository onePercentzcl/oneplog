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
        if (logger && logger->ShouldLog(::oneplog::Level::Trace)) { \
            logger->Log(::oneplog::Level::Trace, ONEPLOG_CURRENT_LOCATION, __VA_ARGS__); \
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
        if (logger && logger->ShouldLog(::oneplog::Level::Debug)) { \
            logger->Log(::oneplog::Level::Debug, ONEPLOG_CURRENT_LOCATION, __VA_ARGS__); \
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
        if (logger && logger->ShouldLog(::oneplog::Level::Info)) { \
            logger->Log(::oneplog::Level::Info, ONEPLOG_CURRENT_LOCATION, __VA_ARGS__); \
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
        if (logger && logger->ShouldLog(::oneplog::Level::Warn)) { \
            logger->Log(::oneplog::Level::Warn, ONEPLOG_CURRENT_LOCATION, __VA_ARGS__); \
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
        if (logger && logger->ShouldLog(::oneplog::Level::Error)) { \
            logger->Log(::oneplog::Level::Error, ONEPLOG_CURRENT_LOCATION, __VA_ARGS__); \
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
        if (logger && logger->ShouldLog(::oneplog::Level::Critical)) { \
            logger->Log(::oneplog::Level::Critical, ONEPLOG_CURRENT_LOCATION, __VA_ARGS__); \
        } \
    } while (0)
#else
#define ONEPLOG_CRITICAL(...) ((void)0)
#endif

// ==============================================================================
// WFC Logging Macros / WFC 日志宏
// ==============================================================================

/**
 * @brief Log at TRACE level with Wait For Completion
 * @brief 以 TRACE 级别记录日志并等待完成
 */
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

/**
 * @brief Log at DEBUG level with Wait For Completion
 * @brief 以 DEBUG 级别记录日志并等待完成
 */
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

/**
 * @brief Log at INFO level with Wait For Completion
 * @brief 以 INFO 级别记录日志并等待完成
 */
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

/**
 * @brief Log at WARN level with Wait For Completion
 * @brief 以 WARN 级别记录日志并等待完成
 */
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

/**
 * @brief Log at ERROR level with Wait For Completion
 * @brief 以 ERROR 级别记录日志并等待完成
 */
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

/**
 * @brief Log at CRITICAL level with Wait For Completion
 * @brief 以 CRITICAL 级别记录日志并等待完成
 */
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

/**
 * @brief Conditional logging macro
 * @brief 条件日志宏
 *
 * @param condition Condition to check / 要检查的条件
 * @param level Log level / 日志级别
 * @param ... Format string and arguments / 格式字符串和参数
 */
#define ONEPLOG_IF(condition, level, ...) \
    do { \
        if (condition) { \
            auto logger = ::oneplog::DefaultLogger(); \
            if (logger && logger->ShouldLog(level)) { \
                logger->Log(level, ONEPLOG_CURRENT_LOCATION, __VA_ARGS__); \
            } \
        } \
    } while (0)

/**
 * @brief Conditional TRACE logging
 * @brief 条件 TRACE 日志
 */
#define ONEPLOG_TRACE_IF(condition, ...) \
    ONEPLOG_IF(condition, ::oneplog::Level::Trace, __VA_ARGS__)

/**
 * @brief Conditional DEBUG logging
 * @brief 条件 DEBUG 日志
 */
#define ONEPLOG_DEBUG_IF(condition, ...) \
    ONEPLOG_IF(condition, ::oneplog::Level::Debug, __VA_ARGS__)

/**
 * @brief Conditional INFO logging
 * @brief 条件 INFO 日志
 */
#define ONEPLOG_INFO_IF(condition, ...) \
    ONEPLOG_IF(condition, ::oneplog::Level::Info, __VA_ARGS__)

/**
 * @brief Conditional WARN logging
 * @brief 条件 WARN 日志
 */
#define ONEPLOG_WARN_IF(condition, ...) \
    ONEPLOG_IF(condition, ::oneplog::Level::Warn, __VA_ARGS__)

/**
 * @brief Conditional ERROR logging
 * @brief 条件 ERROR 日志
 */
#define ONEPLOG_ERROR_IF(condition, ...) \
    ONEPLOG_IF(condition, ::oneplog::Level::Error, __VA_ARGS__)

/**
 * @brief Conditional CRITICAL logging
 * @brief 条件 CRITICAL 日志
 */
#define ONEPLOG_CRITICAL_IF(condition, ...) \
    ONEPLOG_IF(condition, ::oneplog::Level::Critical, __VA_ARGS__)

// ==============================================================================
// Compile-time Disable Macros / 编译时禁用宏
// ==============================================================================

// ONEPLOG_DISABLE_TRACE - Disable TRACE level at compile time
// ONEPLOG_DISABLE_DEBUG - Disable DEBUG level at compile time
// ONEPLOG_DISABLE_INFO - Disable INFO level at compile time
// ONEPLOG_DISABLE_WARN - Disable WARN level at compile time
// ONEPLOG_DISABLE_ERROR - Disable ERROR level at compile time
// ONEPLOG_DISABLE_CRITICAL - Disable CRITICAL level at compile time

// ==============================================================================
// Sync-Only Mode / 仅同步模式
// ==============================================================================

#ifdef ONEPLOG_SYNC_ONLY
// When ONEPLOG_SYNC_ONLY is defined, only sync mode code is compiled
// 当定义 ONEPLOG_SYNC_ONLY 时，仅编译同步模式代码
#endif
