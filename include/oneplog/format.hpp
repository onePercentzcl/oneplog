/**
 * @file format.hpp
 * @brief Log entry formatters for onePlog
 * @brief onePlog 日志条目格式化器
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include <chrono>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#ifdef ONEPLOG_USE_FMT
#include <fmt/format.h>
#include <fmt/chrono.h>
#endif

#include "oneplog/common.hpp"
#include "oneplog/log_entry.hpp"
#include "oneplog/name_manager.hpp"

namespace oneplog {

// Forward declaration
class Sink;

// ==============================================================================
// ANSI Color Codes / ANSI 颜色代码
// ==============================================================================

namespace color {
    constexpr const char* kReset = "\033[0m";
    constexpr const char* kBlue = "\033[34m";
    constexpr const char* kGreen = "\033[32m";
    constexpr const char* kYellow = "\033[33m";
    constexpr const char* kRed = "\033[31m";
    constexpr const char* kMagenta = "\033[35m";
    constexpr const char* kCyan = "\033[36m";
}  // namespace color

/**
 * @brief Get color code for log level
 * @brief 获取日志级别的颜色代码
 */
inline const char* GetLevelColor(Level level) {
    switch (level) {
        case Level::Trace:    return "";  // No color / 无颜色
        case Level::Debug:    return color::kBlue;
        case Level::Info:     return color::kGreen;
        case Level::Warn:     return color::kYellow;
        case Level::Error:    return color::kRed;
        case Level::Critical: return color::kMagenta;
        default:              return "";
    }
}

// ==============================================================================
// Format Requirements / 格式化需求标志
// ==============================================================================

/**
 * @brief Flags indicating what metadata the formatter needs
 * @brief 指示格式化器需要哪些元数据的标志
 */
struct FormatRequirements {
    bool needsTimestamp = true;      ///< Needs timestamp / 需要时间戳
    bool needsLevel = true;          ///< Needs log level / 需要日志级别
    bool needsThreadId = false;      ///< Needs thread ID / 需要线程 ID
    bool needsProcessId = false;     ///< Needs process ID / 需要进程 ID
    bool needsSourceLocation = false; ///< Needs file/line/function / 需要文件/行号/函数名
};

// ==============================================================================
// Format Base Class / 格式化器基类
// ==============================================================================

/**
 * @brief Base class for log formatters
 * @brief 日志格式化器基类
 */
class Format {
public:
    virtual ~Format() = default;

    /**
     * @brief Format a log entry into a string
     * @brief 将日志条目格式化为字符串
     */
    virtual std::string FormatEntry(const LogEntry& entry) = 0;

    /**
     * @brief Direct format without LogEntry (for sync mode optimization)
     * @brief 直接格式化，不使用 LogEntry（用于同步模式优化）
     *
     * @param level Log level / 日志级别
     * @param timestamp Nanosecond timestamp / 纳秒时间戳
     * @param threadId Thread ID (0 if not needed) / 线程 ID（不需要时为 0）
     * @param processId Process ID (0 if not needed) / 进程 ID（不需要时为 0）
     * @param message Formatted message / 格式化后的消息
     * @return Formatted log string / 格式化后的日志字符串
     */
    virtual std::string FormatDirect(Level level, uint64_t timestamp,
                                     uint32_t threadId, uint32_t processId,
                                     const std::string& message) {
        // Default implementation creates a temporary LogEntry
        // 默认实现创建临时 LogEntry
        LogEntry entry;
        entry.level = level;
        entry.timestamp = timestamp;
        entry.threadId = threadId;
        entry.processId = processId;
        entry.snapshot.CaptureStringView(message);
        return FormatEntry(entry);
    }

#ifdef ONEPLOG_USE_FMT
    /**
     * @brief Direct format to buffer without heap allocation (zero-copy)
     * @brief 直接格式化到缓冲区，无堆分配（零拷贝）
     *
     * @param buffer Output buffer (fmt::memory_buffer) / 输出缓冲区
     * @param level Log level / 日志级别
     * @param timestamp Nanosecond timestamp / 纳秒时间戳
     * @param threadId Thread ID (0 if not needed) / 线程 ID（不需要时为 0）
     * @param processId Process ID (0 if not needed) / 进程 ID（不需要时为 0）
     * @param message Formatted message / 格式化后的消息
     */
    virtual void FormatDirectToBuffer(fmt::memory_buffer& buffer,
                                      Level /*level*/, uint64_t /*timestamp*/,
                                      uint32_t /*threadId*/, uint32_t /*processId*/,
                                      std::string_view message) {
        // Default implementation: just append message
        // 默认实现：仅追加消息
        buffer.append(message);
    }
#endif

    /**
     * @brief Get format requirements
     * @brief 获取格式化需求
     */
    virtual FormatRequirements GetRequirements() const {
        FormatRequirements req;
        req.needsTimestamp = true;
        req.needsLevel = true;
        req.needsThreadId = true;
        req.needsProcessId = true;
        req.needsSourceLocation = true;
        return req;
    }

    /**
     * @brief Convert log level to string representation
     * @brief 将日志级别转换为字符串表示
     */
    static const char* LevelToString(Level level, LevelNameStyle style = LevelNameStyle::Short4) {
        return oneplog::LevelToString(level, style).data();
    }

    void BindSink(std::shared_ptr<Sink> sink) { m_sinks.push_back(std::move(sink)); }
    const std::vector<std::shared_ptr<Sink>>& GetSinks() const { return m_sinks; }
    void ClearSinks() { m_sinks.clear(); }

    void SetProcessName(const std::string& name) { m_processName = PadOrTruncate(name, 6); }
    void SetModuleName(const std::string& name) { m_moduleName = PadOrTruncate(name, 6); }
    const std::string& GetProcessName() const { return m_processName; }
    const std::string& GetModuleName() const { return m_moduleName; }

    /**
     * @brief Resolve process name from NameManager by ID
     * @brief 通过 ID 从 NameManager 解析进程名
     *
     * @param processId Process ID (0 for current) / 进程 ID（0 表示当前）
     * @return Resolved process name (padded to 6 chars) / 解析后的进程名（填充到 6 字符）
     */
    static std::string ResolveProcessName(uint32_t processId = 0) {
        std::string name = NameManager<>::GetProcessName(processId);
        return PadOrTruncate(name, 6);
    }

    /**
     * @brief Resolve module name from NameManager by thread ID
     * @brief 通过线程 ID 从 NameManager 解析模块名
     *
     * @param threadId Thread ID (0 for current) / 线程 ID（0 表示当前）
     * @return Resolved module name (padded to 6 chars) / 解析后的模块名（填充到 6 字符）
     */
    static std::string ResolveModuleName(uint32_t threadId = 0) {
        std::string name = NameManager<>::GetModuleName(threadId);
        return PadOrTruncate(name, 6);
    }

    /**
     * @brief Enable/disable dynamic name resolution from NameManager
     * @brief 启用/禁用从 NameManager 动态解析名称
     */
    void SetDynamicNameResolution(bool enabled) { m_useDynamicNames = enabled; }
    bool IsDynamicNameResolutionEnabled() const { return m_useDynamicNames; }

protected:
    /**
     * @brief Pad or truncate string to fixed width (centered)
     * @brief 将字符串填充或截断到固定宽度（居中）
     */
    static std::string PadOrTruncate(const std::string& str, size_t width) {
        if (str.size() >= width) {
            return str.substr(0, width);
        }
        size_t padding = width - str.size();
        size_t leftPad = padding / 2;
        size_t rightPad = padding - leftPad;
        return std::string(leftPad, ' ') + str + std::string(rightPad, ' ');
    }

    /**
     * @brief Format timestamp (milliseconds precision)
     * @brief 格式化时间戳（毫秒精度）
     */
    static std::string FormatTimestampMs(uint64_t timestamp) {
        auto seconds = static_cast<time_t>(timestamp / 1000000000ULL);
        auto millis = static_cast<uint32_t>((timestamp % 1000000000ULL) / 1000000);

        std::tm tm_time{};
#ifdef _WIN32
        localtime_s(&tm_time, &seconds);
#else
        localtime_r(&seconds, &tm_time);
#endif

#ifdef ONEPLOG_USE_FMT
        return fmt::format("{:02d}:{:02d}:{:02d}:{:03d}",
            tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec, millis);
#else
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d:%03d",
            tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec, millis);
        return buf;
#endif
    }

    /**
     * @brief Format timestamp (seconds precision)
     * @brief 格式化时间戳（秒精度）
     */
    static std::string FormatTimestampSec(uint64_t timestamp) {
        auto seconds = static_cast<time_t>(timestamp / 1000000000ULL);

        std::tm tm_time{};
#ifdef _WIN32
        localtime_s(&tm_time, &seconds);
#else
        localtime_r(&seconds, &tm_time);
#endif

#ifdef ONEPLOG_USE_FMT
        return fmt::format("{:02d}:{:02d}:{:02d}",
            tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
#else
        char buf[12];
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
            tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec);
        return buf;
#endif
    }

    /**
     * @brief Format full timestamp with date
     * @brief 格式化完整时间戳（含日期）
     */
    static std::string FormatTimestampFull(uint64_t timestamp) {
        auto seconds = static_cast<time_t>(timestamp / 1000000000ULL);
        auto millis = static_cast<uint32_t>((timestamp % 1000000000ULL) / 1000000);

        std::tm tm_time{};
#ifdef _WIN32
        localtime_s(&tm_time, &seconds);
#else
        localtime_r(&seconds, &tm_time);
#endif

#ifdef ONEPLOG_USE_FMT
        return fmt::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:03d}",
            tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
            tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec, millis);
#else
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
            tm_time.tm_year + 1900, tm_time.tm_mon + 1, tm_time.tm_mday,
            tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec, millis);
        return buf;
#endif
    }

    /**
     * @brief Format timestamp to stack buffer (milliseconds precision)
     * @brief 格式化时间戳到栈缓冲区（毫秒精度）
     */
    static size_t FormatTimestampMsToBuffer(uint64_t timestamp, char* buf, size_t bufSize) {
        auto seconds = static_cast<time_t>(timestamp / 1000000000ULL);
        auto millis = static_cast<uint32_t>((timestamp % 1000000000ULL) / 1000000);

        std::tm tm_time{};
#ifdef _WIN32
        localtime_s(&tm_time, &seconds);
#else
        localtime_r(&seconds, &tm_time);
#endif
        return static_cast<size_t>(std::snprintf(buf, bufSize, "%02d:%02d:%02d:%03d",
            tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec, millis));
    }

    /**
     * @brief Format timestamp to stack buffer (seconds precision)
     * @brief 格式化时间戳到栈缓冲区（秒精度）
     */
    static size_t FormatTimestampSecToBuffer(uint64_t timestamp, char* buf, size_t bufSize) {
        auto seconds = static_cast<time_t>(timestamp / 1000000000ULL);

        std::tm tm_time{};
#ifdef _WIN32
        localtime_s(&tm_time, &seconds);
#else
        localtime_r(&seconds, &tm_time);
#endif
        return static_cast<size_t>(std::snprintf(buf, bufSize, "%02d:%02d:%02d",
            tm_time.tm_hour, tm_time.tm_min, tm_time.tm_sec));
    }

    std::vector<std::shared_ptr<Sink>> m_sinks;
    std::string m_processName = " main ";  // Default 6 chars centered
    std::string m_moduleName = " main ";   // Default 6 chars centered
    bool m_useDynamicNames = true;         // Use NameManager by default / 默认使用 NameManager
};


// ==============================================================================
// ConsoleFormat / 控制台格式化器
// ==============================================================================

/**
 * @brief Console formatter with Debug/Release mode support
 * @brief 支持 Debug/Release 模式的控制台格式化器
 *
 * Debug mode:  [15:20:23:123] [INFO] [进程名:PID] [模块名:TID] 消息
 * Release mode: [15:20:23] [INFO] [进程名] [模块名] 消息 (with colors)
 */
class ConsoleFormat : public Format {
public:
    ConsoleFormat() = default;

    std::string FormatEntry(const LogEntry& entry) override {
#ifdef NDEBUG
        return FormatRelease(entry);
#else
        return FormatDebug(entry);
#endif
    }

    /**
     * @brief Direct format for sync mode optimization
     * @brief 同步模式优化的直接格式化
     */
    std::string FormatDirect(Level level, uint64_t timestamp,
                             uint32_t threadId, uint32_t processId,
                             const std::string& message) override {
#ifdef NDEBUG
        (void)threadId;
        (void)processId;
        return FormatReleaseDirect(level, timestamp, message);
#else
        return FormatDebugDirect(level, timestamp, threadId, processId, message);
#endif
    }

#ifdef ONEPLOG_USE_FMT
    /**
     * @brief Direct format to buffer (zero heap allocation)
     * @brief 直接格式化到缓冲区（零堆分配）
     */
    void FormatDirectToBuffer(fmt::memory_buffer& buffer,
                              Level level, uint64_t timestamp,
                              uint32_t threadId, uint32_t processId,
                              std::string_view message) override {
#ifdef NDEBUG
        (void)threadId;
        (void)processId;
        FormatReleaseToBuffer(buffer, level, timestamp, message);
#else
        FormatDebugToBuffer(buffer, level, timestamp, threadId, processId, message);
#endif
    }
#endif

    /**
     * @brief Get format requirements
     * @brief 获取格式化需求
     */
    FormatRequirements GetRequirements() const override {
        FormatRequirements req;
        req.needsTimestamp = true;
        req.needsLevel = true;
#ifdef NDEBUG
        // Release mode: need thread/process ID for dynamic name resolution
        // Release 模式：动态名称解析需要线程/进程 ID
        req.needsThreadId = m_useDynamicNames;
        req.needsProcessId = m_useDynamicNames;
        req.needsSourceLocation = false;
#else
        // Debug mode needs thread/process ID
        req.needsThreadId = true;
        req.needsProcessId = true;
        req.needsSourceLocation = false;
#endif
        return req;
    }

    void SetColorEnabled(bool enabled) { m_colorEnabled = enabled; }
    bool IsColorEnabled() const { return m_colorEnabled; }

private:
    /**
     * @brief Get process name (dynamic or static)
     * @brief 获取进程名（动态或静态）
     */
    std::string GetEffectiveProcessName(uint32_t processId) const {
        if (m_useDynamicNames) {
            return ResolveProcessName(processId);
        }
        return m_processName;
    }

    /**
     * @brief Get module name (dynamic or static)
     * @brief 获取模块名（动态或静态）
     */
    std::string GetEffectiveModuleName(uint32_t threadId) const {
        if (m_useDynamicNames) {
            return ResolveModuleName(threadId);
        }
        return m_moduleName;
    }

    /**
     * @brief Debug mode format with colors
     * @brief Debug 模式格式（带颜色）
     * [15:20:23:123] [INFO] [进程名:PID] [模块名:TID] 消息
     */
    std::string FormatDebug(const LogEntry& entry) {
        const char* levelColor = m_colorEnabled ? GetLevelColor(entry.level) : "";
        const char* reset = m_colorEnabled ? color::kReset : "";
        
        std::string processName = GetEffectiveProcessName(entry.processId);
        std::string moduleName = GetEffectiveModuleName(entry.threadId);

#ifdef ONEPLOG_USE_FMT
        return fmt::format("[{}] {}[{}]{} [{}:{}] [{}:{}] {}",
            FormatTimestampMs(entry.timestamp),
            levelColor,
            LevelToString(entry.level, LevelNameStyle::Short4),
            reset,
            processName, entry.processId,
            moduleName, entry.threadId,
            entry.snapshot.FormatAll());
#else
        std::string result;
        result.reserve(256);
        result += "[";
        result += FormatTimestampMs(entry.timestamp);
        result += "] ";
        result += levelColor;
        result += "[";
        result += LevelToString(entry.level, LevelNameStyle::Short4);
        result += "]";
        result += reset;
        result += " [";
        result += processName;
        result += ":";
        result += std::to_string(entry.processId);
        result += "] [";
        result += moduleName;
        result += ":";
        result += std::to_string(entry.threadId);
        result += "] ";
        result += entry.snapshot.FormatAll();
        return result;
#endif
    }

    /**
     * @brief Debug mode direct format (no LogEntry)
     * @brief Debug 模式直接格式化（无 LogEntry）
     */
    std::string FormatDebugDirect(Level level, uint64_t timestamp,
                                  uint32_t threadId, uint32_t processId,
                                  const std::string& message) {
        const char* levelColor = m_colorEnabled ? GetLevelColor(level) : "";
        const char* reset = m_colorEnabled ? color::kReset : "";
        
        std::string processName = GetEffectiveProcessName(processId);
        std::string moduleName = GetEffectiveModuleName(threadId);

#ifdef ONEPLOG_USE_FMT
        return fmt::format("[{}] {}[{}]{} [{}:{}] [{}:{}] {}",
            FormatTimestampMs(timestamp),
            levelColor,
            LevelToString(level, LevelNameStyle::Short4),
            reset,
            processName, processId,
            moduleName, threadId,
            message);
#else
        std::string result;
        result.reserve(256);
        result += "[";
        result += FormatTimestampMs(timestamp);
        result += "] ";
        result += levelColor;
        result += "[";
        result += LevelToString(level, LevelNameStyle::Short4);
        result += "]";
        result += reset;
        result += " [";
        result += processName;
        result += ":";
        result += std::to_string(processId);
        result += "] [";
        result += moduleName;
        result += ":";
        result += std::to_string(threadId);
        result += "] ";
        result += message;
        return result;
#endif
    }

#ifdef ONEPLOG_USE_FMT
    /**
     * @brief Debug mode format to buffer (zero heap allocation)
     * @brief Debug 模式格式化到缓冲区（零堆分配）
     */
    void FormatDebugToBuffer(fmt::memory_buffer& buffer,
                             Level level, uint64_t timestamp,
                             uint32_t threadId, uint32_t processId,
                             std::string_view message) {
        const char* levelColor = m_colorEnabled ? GetLevelColor(level) : "";
        const char* reset = m_colorEnabled ? color::kReset : "";
        
        std::string processName = GetEffectiveProcessName(processId);
        std::string moduleName = GetEffectiveModuleName(threadId);

        // Format timestamp to stack buffer
        char timeBuf[16];
        FormatTimestampMsToBuffer(timestamp, timeBuf, sizeof(timeBuf));

        fmt::format_to(std::back_inserter(buffer),
            "[{}] {}[{}]{} [{}:{}] [{}:{}] {}",
            timeBuf,
            levelColor,
            LevelToString(level, LevelNameStyle::Short4),
            reset,
            processName, processId,
            moduleName, threadId,
            message);
    }
#endif

    /**
     * @brief Release mode format with colors
     * @brief Release 模式格式（带颜色）
     * [15:20:23] [INFO] [进程名] [模块名] 消息
     */
    std::string FormatRelease(const LogEntry& entry) {
        const char* levelColor = m_colorEnabled ? GetLevelColor(entry.level) : "";
        const char* reset = m_colorEnabled ? color::kReset : "";
        
        std::string processName = GetEffectiveProcessName(entry.processId);
        std::string moduleName = GetEffectiveModuleName(entry.threadId);

#ifdef ONEPLOG_USE_FMT
        return fmt::format("[{}] {}[{}]{} [{}] [{}] {}",
            FormatTimestampSec(entry.timestamp),
            levelColor,
            LevelToString(entry.level, LevelNameStyle::Short4),
            reset,
            processName,
            moduleName,
            entry.snapshot.FormatAll());
#else
        std::string result;
        result.reserve(256);
        result += "[";
        result += FormatTimestampSec(entry.timestamp);
        result += "] ";
        result += levelColor;
        result += "[";
        result += LevelToString(entry.level, LevelNameStyle::Short4);
        result += "]";
        result += reset;
        result += " [";
        result += processName;
        result += "] [";
        result += moduleName;
        result += "] ";
        result += entry.snapshot.FormatAll();
        return result;
#endif
    }

    /**
     * @brief Release mode direct format (no LogEntry)
     * @brief Release 模式直接格式化（无 LogEntry）
     */
    std::string FormatReleaseDirect(Level level, uint64_t timestamp,
                                    const std::string& message) {
        const char* levelColor = m_colorEnabled ? GetLevelColor(level) : "";
        const char* reset = m_colorEnabled ? color::kReset : "";
        
        std::string processName = GetEffectiveProcessName(0);
        std::string moduleName = GetEffectiveModuleName(0);

#ifdef ONEPLOG_USE_FMT
        return fmt::format("[{}] {}[{}]{} [{}] [{}] {}",
            FormatTimestampSec(timestamp),
            levelColor,
            LevelToString(level, LevelNameStyle::Short4),
            reset,
            processName,
            moduleName,
            message);
#else
        std::string result;
        result.reserve(256);
        result += "[";
        result += FormatTimestampSec(timestamp);
        result += "] ";
        result += levelColor;
        result += "[";
        result += LevelToString(level, LevelNameStyle::Short4);
        result += "]";
        result += reset;
        result += " [";
        result += processName;
        result += "] [";
        result += moduleName;
        result += "] ";
        result += message;
        return result;
#endif
    }

#ifdef ONEPLOG_USE_FMT
    /**
     * @brief Release mode format to buffer (zero heap allocation)
     * @brief Release 模式格式化到缓冲区（零堆分配）
     */
    void FormatReleaseToBuffer(fmt::memory_buffer& buffer,
                               Level level, uint64_t timestamp,
                               std::string_view message) {
        const char* levelColor = m_colorEnabled ? GetLevelColor(level) : "";
        const char* reset = m_colorEnabled ? color::kReset : "";
        
        std::string processName = GetEffectiveProcessName(0);
        std::string moduleName = GetEffectiveModuleName(0);

        // Format timestamp to stack buffer
        char timeBuf[12];
        FormatTimestampSecToBuffer(timestamp, timeBuf, sizeof(timeBuf));

        fmt::format_to(std::back_inserter(buffer),
            "[{}] {}[{}]{} [{}] [{}] {}",
            timeBuf,
            levelColor,
            LevelToString(level, LevelNameStyle::Short4),
            reset,
            processName,
            moduleName,
            message);
    }
#endif

    bool m_colorEnabled = true;
};

// ==============================================================================
// FileFormat / 文件格式化器
// ==============================================================================

/**
 * @brief File formatter with full information
 * @brief 输出全部信息的文件格式化器
 *
 * Format: [2024-01-01 15:20:23.123] [INFO] [进程名:PID] [模块名:TID] [file:line] [function] 消息
 */
class FileFormat : public Format {
public:
    FileFormat() = default;

    std::string FormatEntry(const LogEntry& entry) override {
        std::string processName = m_useDynamicNames ? ResolveProcessName(entry.processId) : m_processName;
        std::string moduleName = m_useDynamicNames ? ResolveModuleName(entry.threadId) : m_moduleName;
        
#ifdef ONEPLOG_USE_FMT
#ifndef NDEBUG
        const char* filename = entry.file ? ExtractFilename(entry.file) : "";
        const char* function = entry.function ? entry.function : "";
        return fmt::format("[{}] [{}] [{}:{}] [{}:{}] [{}:{}] [{}] {}",
            FormatTimestampFull(entry.timestamp),
            LevelToString(entry.level, LevelNameStyle::Short4),
            processName, entry.processId,
            moduleName, entry.threadId,
            filename, entry.line,
            function,
            entry.snapshot.FormatAll());
#else
        return fmt::format("[{}] [{}] [{}:{}] [{}:{}] {}",
            FormatTimestampFull(entry.timestamp),
            LevelToString(entry.level, LevelNameStyle::Short4),
            processName, entry.processId,
            moduleName, entry.threadId,
            entry.snapshot.FormatAll());
#endif
#else
        std::string result;
        result.reserve(512);
        result += "[";
        result += FormatTimestampFull(entry.timestamp);
        result += "] [";
        result += LevelToString(entry.level, LevelNameStyle::Short4);
        result += "] [";
        result += processName;
        result += ":";
        result += std::to_string(entry.processId);
        result += "] [";
        result += moduleName;
        result += ":";
        result += std::to_string(entry.threadId);
        result += "]";
#ifndef NDEBUG
        if (entry.file) {
            result += " [";
            result += ExtractFilename(entry.file);
            result += ":";
            result += std::to_string(entry.line);
            result += "]";
        }
        if (entry.function) {
            result += " [";
            result += entry.function;
            result += "]";
        }
#endif
        result += " ";
        result += entry.snapshot.FormatAll();
        return result;
#endif
    }

private:
    static const char* ExtractFilename(const char* path) {
        const char* filename = path;
        const char* lastSlash = std::strrchr(path, '/');
        if (lastSlash) filename = lastSlash + 1;
#ifdef _WIN32
        const char* lastBackslash = std::strrchr(path, '\\');
        if (lastBackslash && lastBackslash > filename) filename = lastBackslash + 1;
#endif
        return filename;
    }
};

// ==============================================================================
// JsonFormat / JSON 格式化器 (用于数据库)
// ==============================================================================

/**
 * @brief JSON formatter for database storage
 * @brief 用于数据库存储的 JSON 格式化器
 */
class JsonFormat : public Format {
public:
    JsonFormat() = default;

    std::string FormatEntry(const LogEntry& entry) override {
        std::string processName = m_useDynamicNames ? ResolveProcessName(entry.processId) : m_processName;
        std::string moduleName = m_useDynamicNames ? ResolveModuleName(entry.threadId) : m_moduleName;
        
#ifdef ONEPLOG_USE_FMT
        std::string message = entry.snapshot.FormatAll();
        EscapeJsonInPlace(message);

#ifndef NDEBUG
        const char* filename = entry.file ? ExtractFilename(entry.file) : "";
        const char* function = entry.function ? entry.function : "";
        
        if (m_prettyPrint) {
            return fmt::format(
                "{{\n"
                "  \"timestamp\": \"{}\",\n"
                "  \"level\": \"{}\",\n"
                "  \"processName\": \"{}\",\n"
                "  \"processId\": {},\n"
                "  \"moduleName\": \"{}\",\n"
                "  \"threadId\": {},\n"
                "  \"file\": \"{}\",\n"
                "  \"line\": {},\n"
                "  \"function\": \"{}\",\n"
                "  \"message\": \"{}\"\n"
                "}}",
                FormatTimestampFull(entry.timestamp),
                LevelToString(entry.level, LevelNameStyle::Full),
                processName, entry.processId,
                moduleName, entry.threadId,
                filename, entry.line, function,
                message);
        } else {
            return fmt::format(
                "{{\"timestamp\":\"{}\",\"level\":\"{}\",\"processName\":\"{}\","
                "\"processId\":{},\"moduleName\":\"{}\",\"threadId\":{},"
                "\"file\":\"{}\",\"line\":{},\"function\":\"{}\",\"message\":\"{}\"}}",
                FormatTimestampFull(entry.timestamp),
                LevelToString(entry.level, LevelNameStyle::Full),
                processName, entry.processId,
                moduleName, entry.threadId,
                filename, entry.line, function,
                message);
        }
#else
        if (m_prettyPrint) {
            return fmt::format(
                "{{\n"
                "  \"timestamp\": \"{}\",\n"
                "  \"level\": \"{}\",\n"
                "  \"processName\": \"{}\",\n"
                "  \"processId\": {},\n"
                "  \"moduleName\": \"{}\",\n"
                "  \"threadId\": {},\n"
                "  \"message\": \"{}\"\n"
                "}}",
                FormatTimestampFull(entry.timestamp),
                LevelToString(entry.level, LevelNameStyle::Full),
                processName, entry.processId,
                moduleName, entry.threadId,
                message);
        } else {
            return fmt::format(
                "{{\"timestamp\":\"{}\",\"level\":\"{}\",\"processName\":\"{}\","
                "\"processId\":{},\"moduleName\":\"{}\",\"threadId\":{},\"message\":\"{}\"}}",
                FormatTimestampFull(entry.timestamp),
                LevelToString(entry.level, LevelNameStyle::Full),
                processName, entry.processId,
                moduleName, entry.threadId,
                message);
        }
#endif
#else
        return FormatJsonManual(entry, processName, moduleName);
#endif
    }

    void SetPrettyPrint(bool enable) { m_prettyPrint = enable; }
    bool GetPrettyPrint() const { return m_prettyPrint; }

private:
    static const char* ExtractFilename(const char* path) {
        const char* filename = path;
        const char* lastSlash = std::strrchr(path, '/');
        if (lastSlash) filename = lastSlash + 1;
#ifdef _WIN32
        const char* lastBackslash = std::strrchr(path, '\\');
        if (lastBackslash && lastBackslash > filename) filename = lastBackslash + 1;
#endif
        return filename;
    }

    static void EscapeJsonInPlace(std::string& str) {
        std::string result;
        result.reserve(str.size() + 16);
        for (char c : str) {
            switch (c) {
                case '"':  result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:   result += c; break;
            }
        }
        str = std::move(result);
    }

    std::string FormatJsonManual(const LogEntry& entry, 
                                  const std::string& processName,
                                  const std::string& moduleName) {
        std::string message = entry.snapshot.FormatAll();
        EscapeJsonInPlace(message);

        std::ostringstream oss;
        const char* nl = m_prettyPrint ? "\n" : "";
        const char* sp = m_prettyPrint ? "  " : "";

        oss << "{" << nl;
        oss << sp << "\"timestamp\":\"" << FormatTimestampFull(entry.timestamp) << "\"," << nl;
        oss << sp << "\"level\":\"" << LevelToString(entry.level, LevelNameStyle::Full) << "\"," << nl;
        oss << sp << "\"processName\":\"" << processName << "\"," << nl;
        oss << sp << "\"processId\":" << entry.processId << "," << nl;
        oss << sp << "\"moduleName\":\"" << moduleName << "\"," << nl;
        oss << sp << "\"threadId\":" << entry.threadId << "," << nl;
#ifndef NDEBUG
        if (entry.file) {
            oss << sp << "\"file\":\"" << ExtractFilename(entry.file) << "\"," << nl;
            oss << sp << "\"line\":" << entry.line << "," << nl;
        }
        if (entry.function) {
            oss << sp << "\"function\":\"" << entry.function << "\"," << nl;
        }
#endif
        oss << sp << "\"message\":\"" << message << "\"" << nl;
        oss << "}";

        return oss.str();
    }

    bool m_prettyPrint = false;
};

// ==============================================================================
// PatternFormat / 模式格式化器 (保留兼容性)
// ==============================================================================

/**
 * @brief Pattern-based log formatter (for backward compatibility)
 * @brief 基于模式的日志格式化器（保持向后兼容）
 */
class PatternFormat : public Format {
public:
    static constexpr const char* kDefaultPattern = "[%t] [%l] %m";

    explicit PatternFormat(const std::string& pattern = kDefaultPattern)
        : m_pattern(pattern), m_levelStyle(LevelNameStyle::Short4) {
        ParsePattern();
    }

    std::string FormatEntry(const LogEntry& entry) override {
        std::string result;
        result.reserve(256);

        for (const auto& token : m_tokens) {
            switch (token.type) {
                case TokenType::Literal:
                    result += token.literal;
                    break;
                case TokenType::Timestamp:
                    result += FormatTimestampFull(entry.timestamp);
                    break;
                case TokenType::Level:
                    result += LevelToString(entry.level, m_levelStyle);
                    break;
                case TokenType::File:
#ifndef NDEBUG
                    if (entry.file) {
                        const char* filename = entry.file;
                        const char* lastSlash = std::strrchr(entry.file, '/');
                        if (lastSlash) filename = lastSlash + 1;
                        result += filename;
                    }
#endif
                    break;
                case TokenType::Line:
#ifndef NDEBUG
                    result += std::to_string(entry.line);
#endif
                    break;
                case TokenType::Function:
#ifndef NDEBUG
                    if (entry.function) result += entry.function;
#endif
                    break;
                case TokenType::ThreadId:
                    result += std::to_string(entry.threadId);
                    break;
                case TokenType::ProcessId:
                    result += std::to_string(entry.processId);
                    break;
                case TokenType::ProcessName:
                    result += m_processName;
                    break;
                case TokenType::ModuleName:
                    result += m_moduleName;
                    break;
                case TokenType::Message:
                    result += entry.snapshot.FormatAll();
                    break;
            }
        }
        return result;
    }

    void SetLevelStyle(LevelNameStyle style) { m_levelStyle = style; }
    void SetPattern(const std::string& pattern) { m_pattern = pattern; ParsePattern(); }

private:
    enum class TokenType {
        Literal, Timestamp, Level, File, Line, Function,
        ThreadId, ProcessId, ProcessName, ModuleName, Message
    };

    struct Token {
        TokenType type;
        std::string literal;
    };

    void ParsePattern() {
        m_tokens.clear();
        const char* p = m_pattern.c_str();
        std::string literal;

        while (*p) {
            if (*p == '%') {
                if (!literal.empty()) {
                    m_tokens.push_back({TokenType::Literal, literal});
                    literal.clear();
                }
                ++p;
                if (!*p) break;
                switch (*p) {
                    case 't': m_tokens.push_back({TokenType::Timestamp, ""}); break;
                    case 'l': m_tokens.push_back({TokenType::Level, ""}); break;
                    case 'f': m_tokens.push_back({TokenType::File, ""}); break;
                    case 'n': m_tokens.push_back({TokenType::Line, ""}); break;
                    case 'F': m_tokens.push_back({TokenType::Function, ""}); break;
                    case 'T': m_tokens.push_back({TokenType::ThreadId, ""}); break;
                    case 'P': m_tokens.push_back({TokenType::ProcessId, ""}); break;
                    case 'N': m_tokens.push_back({TokenType::ProcessName, ""}); break;
                    case 'M': m_tokens.push_back({TokenType::ModuleName, ""}); break;
                    case 'm': m_tokens.push_back({TokenType::Message, ""}); break;
                    case '%': literal += '%'; break;
                    default: literal += '%'; literal += *p; break;
                }
                ++p;
            } else {
                literal += *p++;
            }
        }
        if (!literal.empty()) {
            m_tokens.push_back({TokenType::Literal, literal});
        }
    }

    std::string m_pattern;
    LevelNameStyle m_levelStyle;
    std::vector<Token> m_tokens;
};

}  // namespace oneplog
