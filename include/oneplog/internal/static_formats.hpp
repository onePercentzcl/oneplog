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
 * - SimpleFormat: [HH:MM:SS] [LEVEL] message
 *   SimpleFormat：[HH:MM:SS] [LEVEL] message
 * - FullFormat: [YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [PID:TID] message
 *   FullFormat：[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [PID:TID] message
 *
 * Sink Types / Sink 类型:
 * - NullSinkType: Discards all output (for benchmarking)
 *   NullSinkType：丢弃所有输出（用于基准测试）
 * - ConsoleSinkType: Writes to stdout
 *   ConsoleSinkType：写入 stdout
 * - StderrSinkType: Writes to stderr
 *   StderrSinkType：写入 stderr
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

#include "oneplog/common.hpp"
#include "oneplog/internal/logger_config.hpp"
#include "oneplog/internal/log_entry.hpp"
#include <fmt/format.h>

namespace oneplog {

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
 * @brief Simple format: [HH:MM:SS] [LEVEL] message
 * @brief 简单格式：[HH:MM:SS] [LEVEL] message
 *
 * Outputs timestamp (time only) and log level with the message.
 * 输出时间戳（仅时间）和日志级别以及消息。
 *
 * _Requirements: 7.3, 7.4, 9.1, 9.2, 9.3_
 */
struct SimpleFormat {
    /// Format requirements - needs timestamp and level / 格式化需求 - 需要时间戳和级别
    using Requirements = StaticFormatRequirements<true, true, false, false, false>;

    /**
     * @brief Format to buffer using fmt library (sync mode)
     * @brief 使用 fmt 库格式化到缓冲区（同步模式）
     */
    template<typename... Args>
    static void FormatTo(fmt::memory_buffer& buffer, Level level, uint64_t timestamp,
                         uint32_t, uint32_t, const char* fmt, Args&&... args) {
        auto seconds = static_cast<time_t>(timestamp / 1000000000ULL);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &seconds);
#else
        localtime_r(&seconds, &tm);
#endif
        fmt::format_to(std::back_inserter(buffer), "[{:02d}:{:02d}:{:02d}] [{}] ",
                       tm.tm_hour, tm.tm_min, tm.tm_sec, LevelToString(level, LevelNameStyle::Short4));
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
        fmt::format_to(std::back_inserter(buffer), "[{:02d}:{:02d}:{:02d}] [{}] ",
                       tm.tm_hour, tm.tm_min, tm.tm_sec, 
                       LevelToString(entry.level, LevelNameStyle::Short4));
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
        return FormatEntry(entry.level, entry.timestamp, 0, 0, entry.snapshot);
    }
    
    /**
     * @brief Format with metadata to string without fmt library (sync mode)
     * @brief 不使用 fmt 库将带元数据的内容格式化为字符串（同步模式）
     */
    static std::string FormatEntry(Level level, uint64_t timestamp, 
                                   uint32_t /*threadId*/, uint32_t /*processId*/,
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
        
        // Format level as [LEVEL]
        result += " [";
        result += LevelToString(level, LevelNameStyle::Short4);
        result += "] ";
        
        // Append message
        result += snapshot.FormatAll();
        
        return result;
    }
};

/**
 * @brief Full format: [YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [PID:TID] message
 * @brief 完整格式：[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [PID:TID] message
 *
 * Outputs full timestamp, log level, process ID, thread ID, and message.
 * 输出完整时间戳、日志级别、进程 ID、线程 ID 和消息。
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
        fmt::format_to(std::back_inserter(buffer),
            "[{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:03d}] [{}] [{}:{}] ",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec, millis,
            LevelToString(level, LevelNameStyle::Short4), processId, threadId);
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
        fmt::format_to(std::back_inserter(buffer),
            "[{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:03d}] [{}] [{}:{}] ",
            tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
            tm.tm_hour, tm.tm_min, tm.tm_sec, millis,
            LevelToString(entry.level, LevelNameStyle::Short4), 
            entry.processId, entry.threadId);
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
        
        // Format timestamp as [YYYY-MM-DD HH:MM:SS.mmm]
        auto seconds = static_cast<time_t>(timestamp / 1000000000ULL);
        auto millis = static_cast<uint32_t>((timestamp % 1000000000ULL) / 1000000);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &seconds);
#else
        localtime_r(&seconds, &tm);
#endif
        
        char timeBuf[32];
        std::snprintf(timeBuf, sizeof(timeBuf), "[%04d-%02d-%02d %02d:%02d:%02d.%03u]",
                      tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                      tm.tm_hour, tm.tm_min, tm.tm_sec, millis);
        result += timeBuf;
        
        // Format level as [LEVEL]
        result += " [";
        result += LevelToString(level, LevelNameStyle::Short4);
        result += "] ";
        
        // Format process and thread IDs as [PID:TID]
        char idBuf[32];
        std::snprintf(idBuf, sizeof(idBuf), "[%u:%u] ", processId, threadId);
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
 * @brief Null sink - discards all output
 * @brief 空 Sink - 丢弃所有输出
 *
 * Useful for benchmarking or disabling logging.
 * 用于基准测试或禁用日志记录。
 */
struct NullSinkType {
    void Write(std::string_view) noexcept {}
    void Flush() noexcept {}
    void Close() noexcept {}
};

/**
 * @brief Console sink - writes to stdout
 * @brief 控制台 Sink - 写入 stdout
 *
 * Writes log messages to standard output with newline.
 * 将日志消息写入标准输出，带换行符。
 */
struct ConsoleSinkType {
    void Write(std::string_view msg) noexcept {
        std::fwrite(msg.data(), 1, msg.size(), stdout);
        std::fputc('\n', stdout);
    }
    void Flush() noexcept { std::fflush(stdout); }
    void Close() noexcept {}
};

/**
 * @brief Stderr sink - writes to stderr
 * @brief 标准错误 Sink - 写入 stderr
 *
 * Writes log messages to standard error with newline.
 * 将日志消息写入标准错误，带换行符。
 */
struct StderrSinkType {
    void Write(std::string_view msg) noexcept {
        std::fwrite(msg.data(), 1, msg.size(), stderr);
        std::fputc('\n', stderr);
    }
    void Flush() noexcept { std::fflush(stderr); }
    void Close() noexcept {}
};

/**
 * @brief File sink configuration for static FileSinkType
 * @brief 静态 FileSinkType 的文件 Sink 配置
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
 * @brief File sink - writes to file with rotation support
 * @brief 文件 Sink - 写入文件，支持轮转
 *
 * Writes log messages to a file. Supports move semantics and file rotation.
 * 将日志消息写入文件。支持移动语义和文件轮转。
 *
 * _Requirements: 13.2, 13.4_
 */
class FileSinkType {
public:
    FileSinkType() = default;
    
    /// Construct with filename only (no rotation)
    /// 仅使用文件名构造（无轮转）
    explicit FileSinkType(const char* filename) 
        : m_filename(filename), m_maxSize(0), m_maxFiles(0), m_currentSize(0) {
        OpenFile();
    }
    
    /// Construct with std::string filename (no rotation)
    /// 使用 std::string 文件名构造（无轮转）
    explicit FileSinkType(const std::string& filename) 
        : m_filename(filename), m_maxSize(0), m_maxFiles(0), m_currentSize(0) {
        OpenFile();
    }
    
    /// Construct with StaticFileSinkConfig (full rotation support)
    /// 使用 StaticFileSinkConfig 构造（完整轮转支持）
    explicit FileSinkType(const StaticFileSinkConfig& config) 
        : m_filename(config.filename), m_maxSize(config.maxSize), 
          m_maxFiles(config.maxFiles), m_currentSize(0) {
        if (config.rotateOnOpen && !m_filename.empty()) {
            RotateFile();
        }
        OpenFile();
    }
    
    /// Construct with any config type that has filename, maxSize, maxFiles, rotateOnOpen fields
    /// 使用任何具有 filename、maxSize、maxFiles、rotateOnOpen 字段的配置类型构造
    /// This allows using FileSinkConfig from fast_logger_v2.hpp
    /// 这允许使用 fast_logger_v2.hpp 中的 FileSinkConfig
    template<typename ConfigT, 
             typename = std::enable_if_t<!std::is_same_v<std::decay_t<ConfigT>, StaticFileSinkConfig> &&
                                         !std::is_same_v<std::decay_t<ConfigT>, FileSinkType> &&
                                         !std::is_same_v<std::decay_t<ConfigT>, const char*> &&
                                         !std::is_same_v<std::decay_t<ConfigT>, std::string>>>
    explicit FileSinkType(const ConfigT& config) 
        : m_filename(config.filename), m_maxSize(config.maxSize), 
          m_maxFiles(config.maxFiles), m_currentSize(0) {
        if (config.rotateOnOpen && !m_filename.empty()) {
            RotateFile();
        }
        OpenFile();
    }
    
    /// Move constructor / 移动构造函数
    FileSinkType(FileSinkType&& o) noexcept 
        : m_file(o.m_file), m_filename(std::move(o.m_filename)),
          m_maxSize(o.m_maxSize), m_maxFiles(o.m_maxFiles), 
          m_currentSize(o.m_currentSize) {
        o.m_file = nullptr;
        o.m_currentSize = 0;
    }
    
    /// Move assignment operator / 移动赋值运算符
    FileSinkType& operator=(FileSinkType&& o) noexcept {
        if (this != &o) { 
            Close(); 
            m_file = o.m_file;
            m_filename = std::move(o.m_filename);
            m_maxSize = o.m_maxSize;
            m_maxFiles = o.m_maxFiles;
            m_currentSize = o.m_currentSize;
            o.m_file = nullptr;
            o.m_currentSize = 0;
        }
        return *this;
    }
    
    ~FileSinkType() { Close(); }
    
    // Non-copyable / 不可复制
    FileSinkType(const FileSinkType&) = delete;
    FileSinkType& operator=(const FileSinkType&) = delete;

    void Write(std::string_view msg) noexcept {
        if (!m_file) return;
        
        // Check if rotation is needed / 检查是否需要轮转
        if (m_maxSize > 0 && m_currentSize + msg.size() + 1 > m_maxSize) {
            RotateFile();
            OpenFile();
        }
        
        std::fwrite(msg.data(), 1, msg.size(), m_file);
        std::fputc('\n', m_file);
        m_currentSize += msg.size() + 1;
    }
    
    void Flush() noexcept { 
        if (m_file) std::fflush(m_file); 
    }
    
    void Close() noexcept { 
        if (m_file) { 
            std::fclose(m_file); 
            m_file = nullptr; 
        } 
    }
    
    /// Get current file size / 获取当前文件大小
    size_t GetCurrentSize() const noexcept { return m_currentSize; }
    
    /// Get filename / 获取文件名
    const std::string& GetFilename() const noexcept { return m_filename; }

private:
    /// Open the log file / 打开日志文件
    void OpenFile() noexcept {
        if (m_filename.empty()) return;
        
        m_file = std::fopen(m_filename.c_str(), "a");
        if (m_file) {
            // Get current file size / 获取当前文件大小
            std::fseek(m_file, 0, SEEK_END);
            m_currentSize = static_cast<size_t>(std::ftell(m_file));
        }
    }
    
    /// Rotate log files / 轮转日志文件
    void RotateFile() noexcept {
        if (m_file) {
            std::fclose(m_file);
            m_file = nullptr;
        }
        
        if (m_filename.empty()) return;
        
        // Delete oldest file if maxFiles is set / 如果设置了 maxFiles，删除最旧的文件
        if (m_maxFiles > 0) {
            std::string oldestFile = m_filename + "." + std::to_string(m_maxFiles);
            std::remove(oldestFile.c_str());
            
            // Rename existing rotated files / 重命名现有的轮转文件
            for (size_t i = m_maxFiles - 1; i >= 1; --i) {
                std::string oldName = m_filename + "." + std::to_string(i);
                std::string newName = m_filename + "." + std::to_string(i + 1);
                std::rename(oldName.c_str(), newName.c_str());
            }
        }
        
        // Rename current file to .1 / 将当前文件重命名为 .1
        std::string rotatedName = m_filename + ".1";
        std::rename(m_filename.c_str(), rotatedName.c_str());
        
        m_currentSize = 0;
    }

    FILE* m_file = nullptr;
    std::string m_filename;
    size_t m_maxSize = 0;
    size_t m_maxFiles = 0;
    size_t m_currentSize = 0;
};

// ==============================================================================
// Default SinkBindings / 默认 SinkBindings
// ==============================================================================

/// Console sink with SimpleFormat binding / 控制台 Sink 与 SimpleFormat 绑定
using DefaultConsoleSinkBinding = SinkBinding<ConsoleSinkType, SimpleFormat>;

/// Console sink with MessageOnlyFormat binding / 控制台 Sink 与 MessageOnlyFormat 绑定
using ConsoleMessageOnlyBinding = SinkBinding<ConsoleSinkType, MessageOnlyFormat>;

/// Console sink with FullFormat binding / 控制台 Sink 与 FullFormat 绑定
using ConsoleFullFormatBinding = SinkBinding<ConsoleSinkType, FullFormat>;

/// Null sink with MessageOnlyFormat binding (for benchmarking) / 空 Sink 与 MessageOnlyFormat 绑定（用于基准测试）
using NullSinkBinding = SinkBinding<NullSinkType, MessageOnlyFormat>;

/// Default SinkBindingList with console output and SimpleFormat / 默认 SinkBindingList，控制台输出和 SimpleFormat
using DefaultSinkBindings = SinkBindingList<DefaultConsoleSinkBinding>;

/// High performance SinkBindingList with NullSink / 高性能 SinkBindingList，使用 NullSink
using HighPerformanceSinkBindings = SinkBindingList<NullSinkBinding>;

}  // namespace oneplog
