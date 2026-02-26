/**
 * @file static_formats.hpp
 * @brief Static format and sink types for Logger
 * @brief Logger 的静态格式化器和 Sink 类型
 *
 * This file provides predefined format and sink types that can be used
 * with Logger's SinkBinding system. All types are designed for compile-time
 * configuration and zero virtual call overhead.
 *
 * 本文件提供可与 Logger 的 SinkBinding 系统一起使用的预定义格式和 Sink 类型。
 * 所有类型都设计为编译期配置和零虚函数调用开销。
 *
 * Format Types / 格式类型:
 * - MessageOnlyFormat: Message only, no metadata (like spdlog's %v)
 *   MessageOnlyFormat：仅消息，无元数据（类似 spdlog 的 %v）
 * - SimpleFormat: [HH:MM:SS] [LEVEL] [进程名] [模块名] message (Release console)
 *   SimpleFormat：[HH:MM:SS] [LEVEL] [进程名] [模块名] message（Release 控制台）
 * - FullFormat: [HH:MM:SS.mmm] [LEVEL] [进程名:PID] [模块名:TID] message (Debug console)
 *   FullFormat：[HH:MM:SS.mmm] [LEVEL] [进程名:PID] [模块名:TID] message（Debug 控制台）
 * - FileFormat: [YYYY-MM-DD HH:MM:SS.nnnnnnnnn] [LEVEL] [进程名:PID] [模块名:TID] message (File output)
 *   FileFormat：[YYYY-MM-DD HH:MM:SS.nnnnnnnnn] [LEVEL] [进程名:PID] [模块名:TID] message（文件输出）
 *
 * Sink Types / Sink 类型:
 * - NullSinkType: Discards all output (for benchmarking)
 *   NullSinkType：丢弃所有输出（用于基准测试）
 * - ConsoleSinkType: Writes to stdout with color support
 *   ConsoleSinkType：写入 stdout，支持颜色
 * - StderrSinkType: Writes to stderr with color support
 *   StderrSinkType：写入 stderr，支持颜色
 * - FileSinkType: Writes to file with rotation support
 *   FileSinkType：写入文件，支持轮转
 *
 * Each format type declares its StaticFormatRequirements, enabling
 * Logger to conditionally acquire only the needed metadata.
 *
 * 每个格式类型声明其 StaticFormatRequirements，使 Logger 能够
 * 有条件地只获取所需的元数据。
 *
 * @see SinkBinding for binding sinks with formats
 * @see SinkBindingList for managing multiple bindings
 * @see LoggerConfig for compile-time configuration
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <string_view>
#include <type_traits>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "oneplog/common.hpp"
#include "oneplog/internal/logger_config.hpp"
#include "oneplog/internal/log_entry.hpp"
#include "oneplog/name_manager.hpp"
#include <fmt/format.h>

namespace oneplog {

// ==============================================================================
// ANSI Color Codes / ANSI 颜色代码
// ==============================================================================

namespace color {

/// Reset all attributes / 重置所有属性
inline constexpr const char* kReset = "\033[0m";

/// Bold prefix / 粗体前缀
inline constexpr const char* kBold = "\033[1m";

/// Level colors (all bold) / 级别颜色（全部加粗）
inline constexpr const char* kTrace = "\033[1m";         // Bold only (original color) / 仅加粗（原色）
inline constexpr const char* kDebug = "\033[1;34m";      // Bold Blue / 粗体蓝色
inline constexpr const char* kInfo = "\033[1;32m";       // Bold Green / 粗体绿色
inline constexpr const char* kWarn = "\033[1;38;5;208m"; // Bold Orange (256-color) / 粗体橙色
inline constexpr const char* kError = "\033[1;31m";      // Bold Red / 粗体红色
inline constexpr const char* kCritical = "\033[1;35m";   // Bold Magenta/Purple / 粗体紫色

/**
 * @brief Get color code for log level (includes bold)
 * @brief 获取日志级别的颜色代码（包含加粗）
 */
inline constexpr const char* GetLevelColor(Level level) noexcept {
    switch (level) {
        case Level::Trace:    return kTrace;
        case Level::Debug:    return kDebug;
        case Level::Info:     return kInfo;
        case Level::Warn:     return kWarn;
        case Level::Error:    return kError;
        case Level::Critical: return kCritical;
        default:              return kReset;
    }
}

}  // namespace color

// ==============================================================================
// Name Formatting Utilities / 名称格式化工具
// ==============================================================================

namespace internal {

/**
 * @brief Format name to fixed width (6 chars), centered, truncated if too long
 * @brief 将名称格式化为固定宽度（6字符），居中，过长则截断
 *
 * @param name Input name / 输入名称
 * @return Formatted name with exactly 6 characters / 恰好 6 字符的格式化名称
 */
inline std::string FormatFixedWidthName(std::string_view name) {
    constexpr size_t kFixedWidth = 6;
    
    if (name.length() >= kFixedWidth) {
        // Truncate to 6 characters / 截断到 6 字符
        return std::string(name.substr(0, kFixedWidth));
    }
    
    // Center the name / 居中名称
    size_t padding = kFixedWidth - name.length();
    size_t leftPad = padding / 2;
    size_t rightPad = padding - leftPad;
    
    std::string result;
    result.reserve(kFixedWidth);
    result.append(leftPad, ' ');
    result.append(name);
    result.append(rightPad, ' ');
    
    return result;
}

}  // namespace internal

// ==============================================================================
// Static Format Types / 静态格式化器类型
// ==============================================================================

/**
 * @brief Message only format (like spdlog's %v)
 * @brief 仅消息格式（类似 spdlog 的 %v）
 *
 * Outputs only the log message without any metadata.
 * 仅输出日志消息，不包含任何元数据。
 *
 * _Requirements: 7.3, 7.4, 9.1, 9.2, 9.3_
 */
struct MessageOnlyFormat {
    /// Format requirements - no metadata needed / 格式化需求 - 不需要元数据
    using Requirements = StaticFormatRequirements<false, false, false, false, false>;

    /**
     * @brief Format to buffer using fmt library (sync mode)
     * @brief 使用 fmt 库格式化到缓冲区（同步模式）
     */
    template<typename... Args>
    static void FormatTo(fmt::memory_buffer& buffer, Level, uint64_t, uint32_t, uint32_t,
                         const char* fmt, Args&&... args) {
        if constexpr (sizeof...(Args) == 0) {
            buffer.append(std::string_view(fmt));
        } else {
            fmt::format_to(std::back_inserter(buffer), fmt::runtime(fmt), std::forward<Args>(args)...);
        }
    }
    
    /**
     * @brief Format LogEntry to buffer using fmt library (async mode)
     * @brief 使用 fmt 库将 LogEntry 格式化到缓冲区（异步模式）
     */
    static void FormatEntryTo(fmt::memory_buffer& buffer, const LogEntry& entry) {
        std::string msg = entry.snapshot.FormatAll();
        buffer.append(std::string_view(msg));
    }

    // =========================================================================
    // Non-fmt fallback implementations / 非 fmt 回退实现
    // =========================================================================
    
    /**
     * @brief Format LogEntry to string without fmt library (async mode)
     * @brief 不使用 fmt 库将 LogEntry 格式化为字符串（异步模式）
     */
    static std::string FormatEntry(const LogEntry& entry) {
        return entry.snapshot.FormatAll();
    }
    
    /**
     * @brief Format with metadata to string without fmt library (sync mode)
     * @brief 不使用 fmt 库将带元数据的内容格式化为字符串（同步模式）
     */
    static std::string FormatEntry(Level /*level*/, uint64_t /*timestamp*/, 
                                   uint32_t /*threadId*/, uint32_t /*processId*/,
                                   const BinarySnapshot& snapshot) {
        return snapshot.FormatAll();
    }
};

/**
 * @brief Simple format (Release mode): [HH:MM:SS] [LEVEL] [进程名] [模块名] message
 * @brief 简单格式（Release 模式）：[HH:MM:SS] [LEVEL] [进程名] [模块名] message
 *
 * Outputs timestamp (time only), colored log level, process name, module name, and message.
 * 输出时间戳（仅时间）、带颜色的日志级别、进程名、模块名和消息。
 *
 * _Requirements: 7.3, 7.4, 9.1, 9.2, 9.3_
 */
struct SimpleFormat {
    /// Format requirements - needs timestamp, level, thread ID, process ID
    /// 格式化需求 - 需要时间戳、级别、线程 ID、进程 ID
    using Requirements = StaticFormatRequirements<true, true, true, true, false>;

    /**
     * @brief Format to buffer using fmt library (sync mode)
     * @brief 使用 fmt 库格式化到缓冲区（同步模式）
     */
    template<typename... Args>
    static void FormatTo(fmt::memory_buffer& buffer, Level level, uint64_t timestamp,
                         uint32_t threadId, uint32_t /*processId*/, const char* fmt, Args&&... args) {
        auto seconds = static_cast<time_t>(timestamp / 1000000000ULL);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &seconds);
#else
        localtime_r(&seconds, &tm);
#endif
        
        // Get process and module names / 获取进程名和模块名
        std::string procName = internal::FormatFixedWidthName(GetProcessName());
        std::string modName = internal::FormatFixedWidthName(
            threadId == 0 ? GetModuleName() : LookupModuleName(threadId));
        
        // Format: [HH:MM:SS] [COLOR][LEVEL][RESET] [进程名] [模块名] message
        fmt::format_to(std::back_inserter(buffer), 
            "[{:02d}:{:02d}:{:02d}] {}[{}]{} [{}] [{}] ",
            tm.tm_hour, tm.tm_min, tm.tm_sec,
            color::GetLevelColor(level),
            LevelToString(level, LevelNameStyle::Short4),
            color::kReset,
            procName, modName);
        
        if constexpr (sizeof...(Args) == 0) {
            buffer.append(std::string_view(fmt));
        } else {
            fmt::format_to(std::back_inserter(buffer), fmt::runtime(fmt), std::forward<Args>(args)...);
        }
    }
    
    /**
     * @brief Format LogEntry to buffer using fmt library (async mode)
     * @brief 使用 fmt 库将 LogEntry 格式化到缓冲区（异步模式）
     */
    static void FormatEntryTo(fmt::memory_buffer& buffer, const LogEntry& entry) {
        auto seconds = static_cast<time_t>(entry.timestamp / 1000000000ULL);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &seconds);
#else
        localtime_r(&seconds, &tm);
#endif
        
        // Get process and module names / 获取进程名和模块名
        std::string procName = internal::FormatFixedWidthName(
            entry.processId == 0 ? GetProcessName() : std::to_string(entry.processId));
        std::string modName = internal::FormatFixedWidthName(
            LookupModuleName(entry.threadId));
        
        fmt::format_to(std::back_inserter(buffer), 
            "[{:02d}:{:02d}:{:02d}] {}[{}]{} [{}] [{}] ",
            tm.tm_hour, tm.tm_min, tm.tm_sec,
            color::GetLevelColor(entry.level),
            LevelToString(entry.level, LevelNameStyle::Short4),
            color::kReset,
            procName, modName);
        
        std::string msg = entry.snapshot.FormatAll();
        buffer.append(std::string_view(msg));
    }

    // =========================================================================
    // Non-fmt fallback implementations / 非 fmt 回退实现
    // =========================================================================
    
    /**
     * @brief Format LogEntry to string without fmt library (async mode)
     * @brief 不使用 fmt 库将 LogEntry 格式化为字符串（异步模式）
     */
    static std::string FormatEntry(const LogEntry& entry) {
        return FormatEntry(entry.level, entry.timestamp, entry.threadId, 
                          entry.processId, entry.snapshot);
    }
    
    /**
     * @brief Format with metadata to string without fmt library (sync mode)
     * @brief 不使用 fmt 库将带元数据的内容格式化为字符串（同步模式）
     */
    static std::string FormatEntry(Level level, uint64_t timestamp, 
                                   uint32_t threadId, uint32_t processId,
                                   const BinarySnapshot& snapshot) {
        std::string result;
        result.reserve(256);
        
        // Format timestamp as [HH:MM:SS]
        auto seconds = static_cast<time_t>(timestamp / 1000000000ULL);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &seconds);
#else
        localtime_r(&seconds, &tm);
#endif
        
        char timeBuf[16];
        std::snprintf(timeBuf, sizeof(timeBuf), "[%02d:%02d:%02d]", 
                      tm.tm_hour, tm.tm_min, tm.tm_sec);
        result += timeBuf;
        
        // Format level with color as [COLOR][LEVEL][RESET]
        result += " ";
        result += color::GetLevelColor(level);
        result += "[";
        result += LevelToString(level, LevelNameStyle::Short4);
        result += "]";
        result += color::kReset;
        
        // Get process and module names / 获取进程名和模块名
        std::string procName = internal::FormatFixedWidthName(
            processId == 0 ? GetProcessName() : std::to_string(processId));
        std::string modName = internal::FormatFixedWidthName(
            threadId == 0 ? GetModuleName() : LookupModuleName(threadId));
        
        // Format [进程名] [模块名]
        result += " [";
        result += procName;
        result += "] [";
        result += modName;
        result += "] ";
        
        // Append message
        result += snapshot.FormatAll();
        
        return result;
    }
};


/**
 * @brief Full format (Debug mode): [HH:MM:SS.mmm] [LEVEL] [进程名:PID] [模块名:TID] message
 * @brief 完整格式（Debug 模式）：[HH:MM:SS.mmm] [LEVEL] [进程名:PID] [模块名:TID] message
 *
 * Outputs timestamp with milliseconds, colored log level, process name with PID,
 * module name with TID, and message.
 * 输出带毫秒的时间戳、带颜色的日志级别、带 PID 的进程名、带 TID 的模块名和消息。
 *
 * _Requirements: 7.3, 7.4, 9.1, 9.2, 9.3_
 */
struct FullFormat {
    /// Format requirements - needs all metadata / 格式化需求 - 需要所有元数据
    using Requirements = StaticFormatRequirements<true, true, true, true, false>;

    /**
     * @brief Format to buffer using fmt library (sync mode)
     * @brief 使用 fmt 库格式化到缓冲区（同步模式）
     */
    template<typename... Args>
    static void FormatTo(fmt::memory_buffer& buffer, Level level, uint64_t timestamp,
                         uint32_t threadId, uint32_t processId, const char* fmt, Args&&... args) {
        auto seconds = static_cast<time_t>(timestamp / 1000000000ULL);
        auto millis = static_cast<uint32_t>((timestamp % 1000000000ULL) / 1000000);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &seconds);
#else
        localtime_r(&seconds, &tm);
#endif
        
        // Get process and module names / 获取进程名和模块名
        std::string procName = internal::FormatFixedWidthName(GetProcessName());
        std::string modName = internal::FormatFixedWidthName(
            threadId == 0 ? GetModuleName() : LookupModuleName(threadId));
        
        // Get actual PID if not provided / 如果未提供则获取实际 PID
        uint32_t pid = processId;
        if (pid == 0) {
#ifdef _WIN32
            pid = static_cast<uint32_t>(GetCurrentProcessId());
#else
            pid = static_cast<uint32_t>(getpid());
#endif
        }
        
        // Get actual TID if not provided / 如果未提供则获取实际 TID
        uint32_t tid = threadId;
        if (tid == 0) {
            tid = internal::GetCurrentThreadIdInternal();
        }
        
        // Format: [HH:MM:SS.mmm] [COLOR][LEVEL][RESET] [进程名:PID] [模块名:TID] message
        fmt::format_to(std::back_inserter(buffer),
            "[{:02d}:{:02d}:{:02d}.{:03d}] {}[{}]{} [{}:{}] [{}:{}] ",
            tm.tm_hour, tm.tm_min, tm.tm_sec, millis,
            color::GetLevelColor(level),
            LevelToString(level, LevelNameStyle::Short4),
            color::kReset,
            procName, pid, modName, tid);
        
        if constexpr (sizeof...(Args) == 0) {
            buffer.append(std::string_view(fmt));
        } else {
            fmt::format_to(std::back_inserter(buffer), fmt::runtime(fmt), std::forward<Args>(args)...);
        }
    }
    
    /**
     * @brief Format LogEntry to buffer using fmt library (async mode)
     * @brief 使用 fmt 库将 LogEntry 格式化到缓冲区（异步模式）
     */
    static void FormatEntryTo(fmt::memory_buffer& buffer, const LogEntry& entry) {
        auto seconds = static_cast<time_t>(entry.timestamp / 1000000000ULL);
        auto millis = static_cast<uint32_t>((entry.timestamp % 1000000000ULL) / 1000000);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &seconds);
#else
        localtime_r(&seconds, &tm);
#endif
        
        // Get process and module names / 获取进程名和模块名
        std::string procName = internal::FormatFixedWidthName(
            entry.processId == 0 ? GetProcessName() : std::to_string(entry.processId));
        std::string modName = internal::FormatFixedWidthName(
            LookupModuleName(entry.threadId));
        
        fmt::format_to(std::back_inserter(buffer),
            "[{:02d}:{:02d}:{:02d}.{:03d}] {}[{}]{} [{}:{}] [{}:{}] ",
            tm.tm_hour, tm.tm_min, tm.tm_sec, millis,
            color::GetLevelColor(entry.level),
            LevelToString(entry.level, LevelNameStyle::Short4),
            color::kReset,
            procName, entry.processId, modName, entry.threadId);
        
        std::string msg = entry.snapshot.FormatAll();
        buffer.append(std::string_view(msg));
    }

    // =========================================================================
    // Non-fmt fallback implementations / 非 fmt 回退实现
    // =========================================================================
    
    /**
     * @brief Format LogEntry to string without fmt library (async mode)
     * @brief 不使用 fmt 库将 LogEntry 格式化为字符串（异步模式）
     */
    static std::string FormatEntry(const LogEntry& entry) {
        return FormatEntry(entry.level, entry.timestamp, entry.threadId, 
                          entry.processId, entry.snapshot);
    }
    
    /**
     * @brief Format with metadata to string without fmt library (sync mode)
     * @brief 不使用 fmt 库将带元数据的内容格式化为字符串（同步模式）
     */
    static std::string FormatEntry(Level level, uint64_t timestamp, 
                                   uint32_t threadId, uint32_t processId,
                                   const BinarySnapshot& snapshot) {
        std::string result;
        result.reserve(512);
        
        // Format timestamp as [HH:MM:SS.mmm]
        auto seconds = static_cast<time_t>(timestamp / 1000000000ULL);
        auto millis = static_cast<uint32_t>((timestamp % 1000000000ULL) / 1000000);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &seconds);
#else
        localtime_r(&seconds, &tm);
#endif
        
        char timeBuf[16];
        std::snprintf(timeBuf, sizeof(timeBuf), "[%02d:%02d:%02d.%03u]",
                      tm.tm_hour, tm.tm_min, tm.tm_sec, millis);
        result += timeBuf;
        
        // Format level with color as [COLOR][LEVEL][RESET]
        result += " ";
        result += color::GetLevelColor(level);
        result += "[";
        result += LevelToString(level, LevelNameStyle::Short4);
        result += "]";
        result += color::kReset;
        
        // Get process and module names / 获取进程名和模块名
        std::string procName = internal::FormatFixedWidthName(
            processId == 0 ? GetProcessName() : std::to_string(processId));
        std::string modName = internal::FormatFixedWidthName(
            threadId == 0 ? GetModuleName() : LookupModuleName(threadId));
        
        // Get actual PID if not provided / 如果未提供则获取实际 PID
        uint32_t pid = processId;
        if (pid == 0) {
#ifdef _WIN32
            pid = static_cast<uint32_t>(GetCurrentProcessId());
#else
            pid = static_cast<uint32_t>(getpid());
#endif
        }
        
        // Get actual TID if not provided / 如果未提供则获取实际 TID
        uint32_t tid = threadId;
        if (tid == 0) {
            tid = internal::GetCurrentThreadIdInternal();
        }
        
        // Format [进程名:PID] [模块名:TID]
        char idBuf[64];
        std::snprintf(idBuf, sizeof(idBuf), " [%s:%u] [%s:%u] ", 
                      procName.c_str(), pid, modName.c_str(), tid);
        result += idBuf;
        
        // Append message
        result += snapshot.FormatAll();
        
        return result;
    }
};


/**
 * @brief File format: [YYYY-MM-DD HH:MM:SS.nnnnnnnnn] [LEVEL] [进程名:PID] [模块名:TID] message
 * @brief 文件格式：[YYYY-MM-DD HH:MM:SS.nnnnnnnnn] [LEVEL] [进程名:PID] [模块名:TID] message
 *
 * Outputs full timestamp with nanoseconds, log level (no color), process name with PID,
 * module name with TID, and message. Designed for file output where colors are not needed
 * and maximum timestamp precision is desired.
 * 输出带纳秒的完整时间戳、日志级别（无颜色）、带 PID 的进程名、带 TID 的模块名和消息。
 * 专为文件输出设计，不需要颜色，需要最大时间戳精度。
 *
 * _Requirements: 7.3, 7.4, 9.1, 9.2, 9.3_
 */
struct FileFormat {
    /// Format requirements - needs all metadata / 格式化需求 - 需要所有元数据
    using Requirements = StaticFormatRequirements<true, true, true, true, false>;

    /**
     * @brief Format to buffer using fmt library (sync mode)
     * @brief 使用 fmt 库格式化到缓冲区（同步模式）
     */
    template<typename... Args>
    static void FormatTo(fmt::memory_buffer& buffer, Level level, uint64_t timestamp,
                         uint32_t threadId, uint32_t processId, const char* fmt, Args&&... args) {
        auto seconds = static_cast<time_t>(timestamp / 1000000000ULL);
        auto nanos = static_cast<uint32_t>(timestamp % 1000000000ULL);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &seconds);
#else
        localtime_r(&seconds, &tm);
#endif
        
        // Get process and module names / 获取进程名和模块名
        std::string procName = internal::FormatFixedWidthName(GetProcessName());
        std::string modName = internal::FormatFixedWidthName(
            threadId == 0 ? GetModuleName() : LookupModuleName(threadId));
        
        // Get actual PID if not provided / 如果未提供则获取实际 PID
        uint32_t pid = processId;
        if (pid == 0) {
#ifdef _WIN32
            pid = static_cast<uint32_t>(GetCurrentProcessId());
#else
            pid = static_cast<uint32_t>(getpid());
#endif
        }
        
        // Get actual TID if not provided / 如果未提供则获取实际 TID
        uint32_t tid = threadId;
        if (tid == 0) {
            tid = internal::GetCurrentThreadIdInternal();
        }
        
        // Format: [YYYY-MM-DD HH:MM:SS.nnnnnnnnn] [LEVEL] [进程名:PID] [模块名:TID] message
        // No color codes for file output / 文件输出不使用颜色代码
        fmt::format_to(std::back_inserter(buffer),
            "[{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:09d}] [{}] [{}:{}] [{}:{}] ",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec, nanos,
            LevelToString(level, LevelNameStyle::Short4),
            procName, pid, modName, tid);
        
        if constexpr (sizeof...(Args) == 0) {
            buffer.append(std::string_view(fmt));
        } else {
            fmt::format_to(std::back_inserter(buffer), fmt::runtime(fmt), std::forward<Args>(args)...);
        }
    }
    
    /**
     * @brief Format LogEntry to buffer using fmt library (async mode)
     * @brief 使用 fmt 库将 LogEntry 格式化到缓冲区（异步模式）
     */
    static void FormatEntryTo(fmt::memory_buffer& buffer, const LogEntry& entry) {
        auto seconds = static_cast<time_t>(entry.timestamp / 1000000000ULL);
        auto nanos = static_cast<uint32_t>(entry.timestamp % 1000000000ULL);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &seconds);
#else
        localtime_r(&seconds, &tm);
#endif
        
        // Get process and module names / 获取进程名和模块名
        std::string procName = internal::FormatFixedWidthName(
            entry.processId == 0 ? GetProcessName() : std::to_string(entry.processId));
        std::string modName = internal::FormatFixedWidthName(
            LookupModuleName(entry.threadId));
        
        // Get actual PID / 获取实际 PID
        uint32_t pid = entry.processId;
        if (pid == 0) {
#ifdef _WIN32
            pid = static_cast<uint32_t>(GetCurrentProcessId());
#else
            pid = static_cast<uint32_t>(getpid());
#endif
        }
        
        fmt::format_to(std::back_inserter(buffer),
            "[{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:09d}] [{}] [{}:{}] [{}:{}] ",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec, nanos,
            LevelToString(entry.level, LevelNameStyle::Short4),
            procName, pid, modName, entry.threadId);
        
        std::string msg = entry.snapshot.FormatAll();
        buffer.append(std::string_view(msg));
    }

    // =========================================================================
    // Non-fmt fallback implementations / 非 fmt 回退实现
    // =========================================================================
    
    /**
     * @brief Format LogEntry to string without fmt library (async mode)
     * @brief 不使用 fmt 库将 LogEntry 格式化为字符串（异步模式）
     */
    static std::string FormatEntry(const LogEntry& entry) {
        return FormatEntry(entry.level, entry.timestamp, entry.threadId, 
                          entry.processId, entry.snapshot);
    }
    
    /**
     * @brief Format with metadata to string without fmt library (sync mode)
     * @brief 不使用 fmt 库将带元数据的内容格式化为字符串（同步模式）
     */
    static std::string FormatEntry(Level level, uint64_t timestamp, 
                                   uint32_t threadId, uint32_t processId,
                                   const BinarySnapshot& snapshot) {
        std::string result;
        result.reserve(512);
        
        // Format timestamp as [YYYY-MM-DD HH:MM:SS.nnnnnnnnn]
        auto seconds = static_cast<time_t>(timestamp / 1000000000ULL);
        auto nanos = static_cast<uint32_t>(timestamp % 1000000000ULL);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &seconds);
#else
        localtime_r(&seconds, &tm);
#endif
        
        char timeBuf[40];
        std::snprintf(timeBuf, sizeof(timeBuf), "[%04d-%02d-%02d %02d:%02d:%02d.%09u]",
                      tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                      tm.tm_hour, tm.tm_min, tm.tm_sec, nanos);
        result += timeBuf;
        
        // Format level (no color) as [LEVEL]
        result += " [";
        result += LevelToString(level, LevelNameStyle::Short4);
        result += "]";
        
        // Get process and module names / 获取进程名和模块名
        std::string procName = internal::FormatFixedWidthName(
            processId == 0 ? GetProcessName() : std::to_string(processId));
        std::string modName = internal::FormatFixedWidthName(
            threadId == 0 ? GetModuleName() : LookupModuleName(threadId));
        
        // Get actual PID if not provided / 如果未提供则获取实际 PID
        uint32_t pid = processId;
        if (pid == 0) {
#ifdef _WIN32
            pid = static_cast<uint32_t>(GetCurrentProcessId());
#else
            pid = static_cast<uint32_t>(getpid());
#endif
        }
        
        // Get actual TID if not provided / 如果未提供则获取实际 TID
        uint32_t tid = threadId;
        if (tid == 0) {
            tid = internal::GetCurrentThreadIdInternal();
        }
        
        // Format [进程名:PID] [模块名:TID]
        char idBuf[64];
        std::snprintf(idBuf, sizeof(idBuf), " [%s:%u] [%s:%u] ", 
                      procName.c_str(), pid, modName.c_str(), tid);
        result += idBuf;
        
        // Append message
        result += snapshot.FormatAll();
        
        return result;
    }
};


// ==============================================================================
// Static Sink Types / 静态 Sink 类型
// ==============================================================================

/**
 * @brief Null sink that discards all output (for benchmarking)
 * @brief 丢弃所有输出的空 Sink（用于基准测试）
 *
 * _Requirements: 7.3, 7.4_
 */
struct NullSinkType {
    /**
     * @brief Write formatted message (discards it)
     * @brief 写入格式化消息（丢弃）
     */
    static void Write(const char* /*data*/, size_t /*size*/) noexcept {
        // Intentionally empty - discard all output
    }
    
    /**
     * @brief Write string_view (discards it)
     * @brief 写入 string_view（丢弃）
     */
    static void Write(std::string_view /*msg*/) noexcept {
        // Intentionally empty - discard all output
    }
    
    /**
     * @brief Flush (no-op)
     * @brief 刷新（无操作）
     */
    static void Flush() noexcept {
        // Intentionally empty
    }
    
    /**
     * @brief Close (no-op)
     * @brief 关闭（无操作）
     */
    static void Close() noexcept {
        // Intentionally empty
    }
};

/**
 * @brief Console sink that writes to stdout
 * @brief 写入 stdout 的控制台 Sink
 *
 * _Requirements: 7.3, 7.4_
 */
struct ConsoleSinkType {
    /**
     * @brief Write formatted message to stdout
     * @brief 将格式化消息写入 stdout
     */
    static void Write(const char* data, size_t size) noexcept {
        std::fwrite(data, 1, size, stdout);
        std::fputc('\n', stdout);
    }
    
    /**
     * @brief Write string_view to stdout
     * @brief 将 string_view 写入 stdout
     */
    static void Write(std::string_view msg) noexcept {
        std::fwrite(msg.data(), 1, msg.size(), stdout);
        std::fputc('\n', stdout);
    }
    
    /**
     * @brief Flush stdout
     * @brief 刷新 stdout
     */
    static void Flush() noexcept {
        std::fflush(stdout);
    }
    
    /**
     * @brief Close (no-op for stdout)
     * @brief 关闭（stdout 无操作）
     */
    static void Close() noexcept {
        // stdout should not be closed
    }
};

/**
 * @brief Stderr sink that writes to stderr
 * @brief 写入 stderr 的 Sink
 *
 * _Requirements: 7.3, 7.4_
 */
struct StderrSinkType {
    /**
     * @brief Write formatted message to stderr
     * @brief 将格式化消息写入 stderr
     */
    static void Write(const char* data, size_t size) noexcept {
        std::fwrite(data, 1, size, stderr);
        std::fputc('\n', stderr);
    }
    
    /**
     * @brief Write string_view to stderr
     * @brief 将 string_view 写入 stderr
     */
    static void Write(std::string_view msg) noexcept {
        std::fwrite(msg.data(), 1, msg.size(), stderr);
        std::fputc('\n', stderr);
    }
    
    /**
     * @brief Flush stderr
     * @brief 刷新 stderr
     */
    static void Flush() noexcept {
        std::fflush(stderr);
    }
    
    /**
     * @brief Close (no-op for stderr)
     * @brief 关闭（stderr 无操作）
     */
    static void Close() noexcept {
        // stderr should not be closed
    }
};

/**
 * @brief File sink configuration for static file sink
 * @brief 静态文件 Sink 配置
 *
 * Configuration for file-based sinks with rotation support.
 * 支持轮转的文件 Sink 配置。
 *
 * _Requirements: 13.4_
 */
struct StaticFileSinkConfig {
    /// File path / 文件路径
    std::string filename;
    
    /// Maximum file size before rotation (0 = no limit) / 轮转前的最大文件大小（0 = 无限制）
    size_t maxSize{0};
    
    /// Maximum number of rotated files (0 = no rotation) / 轮转文件的最大数量（0 = 不轮转）
    size_t maxFiles{0};
    
    /// Rotate on open / 打开时轮转
    bool rotateOnOpen{false};
};

/**
 * @brief File sink that writes to a file with rotation support
 * @brief 写入文件的 Sink，支持轮转
 *
 * Supports file rotation based on size and number of backup files.
 * 支持基于大小和备份文件数量的文件轮转。
 *
 * _Requirements: 7.3, 7.4, 13.4_
 */
class FileSinkType {
public:
    /**
     * @brief Default constructor
     * @brief 默认构造函数
     */
    FileSinkType() = default;
    
    /**
     * @brief Construct with filename only (no rotation)
     * @brief 仅使用文件名构造（无轮转）
     */
    explicit FileSinkType(const char* filename)
        : m_filename(filename)
        , m_maxSize(0)
        , m_maxFiles(0)
        , m_rotateOnOpen(false)
        , m_currentSize(0)
        , m_file(nullptr) {
        OpenFile();
    }
    
    /**
     * @brief Construct with StaticFileSinkConfig
     * @brief 使用 StaticFileSinkConfig 构造
     */
    explicit FileSinkType(const StaticFileSinkConfig& config)
        : m_filename(config.filename)
        , m_maxSize(config.maxSize)
        , m_maxFiles(config.maxFiles)
        , m_rotateOnOpen(config.rotateOnOpen)
        , m_currentSize(0)
        , m_file(nullptr) {
        OpenFile();
    }
    
    /**
     * @brief Construct with any config type that has filename, maxSize, maxFiles, rotateOnOpen
     * @brief 使用任何具有 filename, maxSize, maxFiles, rotateOnOpen 的配置类型构造
     *
     * This template constructor allows using FileSinkConfig from logger.hpp
     * 此模板构造函数允许使用 logger.hpp 中的 FileSinkConfig
     */
    template<typename ConfigT, 
             typename = std::enable_if_t<!std::is_same_v<std::decay_t<ConfigT>, StaticFileSinkConfig> &&
                                         !std::is_same_v<std::decay_t<ConfigT>, FileSinkType> &&
                                         std::is_class_v<std::decay_t<ConfigT>>>>
    explicit FileSinkType(const ConfigT& config)
        : m_filename(config.filename)
        , m_maxSize(config.maxSize)
        , m_maxFiles(config.maxFiles)
        , m_rotateOnOpen(config.rotateOnOpen)
        , m_currentSize(0)
        , m_file(nullptr) {
        OpenFile();
    }
    
    /**
     * @brief Destructor - closes file
     * @brief 析构函数 - 关闭文件
     */
    ~FileSinkType() {
        Close();
    }
    
    // Non-copyable
    FileSinkType(const FileSinkType&) = delete;
    FileSinkType& operator=(const FileSinkType&) = delete;
    
    // Movable
    FileSinkType(FileSinkType&& other) noexcept
        : m_filename(std::move(other.m_filename))
        , m_maxSize(other.m_maxSize)
        , m_maxFiles(other.m_maxFiles)
        , m_rotateOnOpen(other.m_rotateOnOpen)
        , m_currentSize(other.m_currentSize)
        , m_file(other.m_file) {
        other.m_file = nullptr;
        other.m_currentSize = 0;
    }
    
    FileSinkType& operator=(FileSinkType&& other) noexcept {
        if (this != &other) {
            Close();
            m_filename = std::move(other.m_filename);
            m_maxSize = other.m_maxSize;
            m_maxFiles = other.m_maxFiles;
            m_rotateOnOpen = other.m_rotateOnOpen;
            m_currentSize = other.m_currentSize;
            m_file = other.m_file;
            other.m_file = nullptr;
            other.m_currentSize = 0;
        }
        return *this;
    }
    
    /**
     * @brief Open file for writing
     * @brief 打开文件用于写入
     */
    bool Open(const char* filename) {
        Close();
        m_filename = filename;
        m_file = std::fopen(filename, "a");
        if (m_file) {
            std::fseek(m_file, 0, SEEK_END);
            m_currentSize = static_cast<size_t>(std::ftell(m_file));
        }
        return m_file != nullptr;
    }
    
    /**
     * @brief Close file
     * @brief 关闭文件
     */
    void Close() noexcept {
        if (m_file) {
            std::fclose(m_file);
            m_file = nullptr;
        }
    }
    
    /**
     * @brief Write formatted message to file
     * @brief 将格式化消息写入文件
     */
    void Write(const char* data, size_t size) noexcept {
        if (m_file) {
            CheckRotation(size);
            std::fwrite(data, 1, size, m_file);
            std::fputc('\n', m_file);
            m_currentSize += size + 1;
        }
    }
    
    /**
     * @brief Write string_view to file
     * @brief 将 string_view 写入文件
     */
    void Write(std::string_view msg) noexcept {
        Write(msg.data(), msg.size());
    }
    
    /**
     * @brief Flush file
     * @brief 刷新文件
     */
    void Flush() noexcept {
        if (m_file) {
            std::fflush(m_file);
        }
    }

private:
    /**
     * @brief Check if rotation is needed and perform it
     * @brief 检查是否需要轮转并执行
     */
    void CheckRotation(size_t newDataSize) {
        if (m_maxSize > 0 && m_currentSize + newDataSize > m_maxSize) {
            Rotate();
        }
    }
    
    /**
     * @brief Perform file rotation
     * @brief 执行文件轮转
     */
    void Rotate() {
        if (m_file) {
            std::fclose(m_file);
            m_file = nullptr;
        }
        
        if (m_maxFiles > 0) {
            // Delete oldest file if it exists
            std::string oldestFile = m_filename + "." + std::to_string(m_maxFiles);
            std::remove(oldestFile.c_str());
            
            // Rename existing backup files
            for (size_t i = m_maxFiles - 1; i >= 1; --i) {
                std::string oldName = m_filename + "." + std::to_string(i);
                std::string newName = m_filename + "." + std::to_string(i + 1);
                std::rename(oldName.c_str(), newName.c_str());
            }
            
            // Rename current file to .1
            std::string backupName = m_filename + ".1";
            std::rename(m_filename.c_str(), backupName.c_str());
        }
        
        // Open new file
        m_file = std::fopen(m_filename.c_str(), "w");
        m_currentSize = 0;
    }
    
    /**
     * @brief Open file based on current configuration
     * @brief 根据当前配置打开文件
     */
    void OpenFile() {
        if (!m_filename.empty()) {
            if (m_rotateOnOpen) {
                Rotate();
            } else {
                m_file = std::fopen(m_filename.c_str(), "a");
                if (m_file) {
                    // Get current file size
                    std::fseek(m_file, 0, SEEK_END);
                    m_currentSize = static_cast<size_t>(std::ftell(m_file));
                }
            }
        }
    }
    
    std::string m_filename;
    size_t m_maxSize{0};
    size_t m_maxFiles{0};
    bool m_rotateOnOpen{false};
    size_t m_currentSize{0};
    FILE* m_file{nullptr};
};

// ==============================================================================
// Default SinkBindings Type Aliases / 默认 SinkBindings 类型别名
// ==============================================================================

/**
 * @brief Default format type based on build configuration
 * @brief 基于构建配置的默认格式类型
 *
 * - Debug builds: FullFormat (with milliseconds, PID, TID)
 * - Release builds: SimpleFormat (time only, no IDs)
 *
 * - Debug 构建：FullFormat（带毫秒、PID、TID）
 * - Release 构建：SimpleFormat（仅时间，无 ID）
 */
#ifdef NDEBUG
using DefaultFormat = SimpleFormat;
#else
using DefaultFormat = FullFormat;
#endif

/**
 * @brief Default sink type (console output)
 * @brief 默认 Sink 类型（控制台输出）
 */
using DefaultSink = ConsoleSinkType;

/**
 * @brief Default SinkBindings - Console with DefaultFormat
 * @brief 默认 SinkBindings - 控制台 + 默认格式
 *
 * Uses SimpleFormat in Release mode, FullFormat in Debug mode.
 * Release 模式使用 SimpleFormat，Debug 模式使用 FullFormat。
 */
using DefaultSinkBindings = SinkBindingList<SinkBinding<ConsoleSinkType, DefaultFormat>>;

/**
 * @brief High performance SinkBindings - NullSink with MessageOnlyFormat
 * @brief 高性能 SinkBindings - NullSink + MessageOnlyFormat
 *
 * For benchmarking - discards all output with minimal formatting overhead.
 * 用于基准测试 - 以最小的格式化开销丢弃所有输出。
 */
using HighPerformanceSinkBindings = SinkBindingList<SinkBinding<NullSinkType, MessageOnlyFormat>>;

/**
 * @brief Console SinkBindings with SimpleFormat
 * @brief 控制台 SinkBindings + SimpleFormat
 */
using ConsoleSinkBindings = SinkBindingList<SinkBinding<ConsoleSinkType, SimpleFormat>>;

/**
 * @brief Console SinkBindings with FullFormat
 * @brief 控制台 SinkBindings + FullFormat
 */
using ConsoleFullSinkBindings = SinkBindingList<SinkBinding<ConsoleSinkType, FullFormat>>;

/**
 * @brief Stderr SinkBindings with SimpleFormat
 * @brief Stderr SinkBindings + SimpleFormat
 */
using StderrSinkBindings = SinkBindingList<SinkBinding<StderrSinkType, SimpleFormat>>;

/**
 * @brief File SinkBindings with FileFormat (nanosecond precision, no color)
 * @brief 文件 SinkBindings + FileFormat（纳秒精度，无颜色）
 *
 * Designed for file output with maximum timestamp precision and no ANSI color codes.
 * 专为文件输出设计，具有最大时间戳精度，无 ANSI 颜色代码。
 */
template<typename FileSink = FileSinkType>
using FileSinkBindings = SinkBindingList<SinkBinding<FileSink, FileFormat>>;

}  // namespace oneplog
