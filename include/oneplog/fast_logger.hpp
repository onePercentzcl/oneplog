/**
 * @file fast_logger.hpp
 * @brief High-performance template-based Logger with compile-time optimization
 * @brief 高性能模板化日志器，支持编译期优化
 *
 * This header can be used independently without external macro definitions.
 * All required dependencies are included automatically.
 *
 * 此头文件可独立使用，无需外部宏定义。
 * 所有必需的依赖项会自动包含。
 *
 * Features:
 * - Compile-time Format and Sink type resolution (no virtual calls)
 * - Multi-sink support via variadic templates
 * - Sync/Async mode support
 * - Zero-overhead level filtering
 * - Works with or without fmt library (ONEPLOG_USE_FMT)
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

// Standard library includes
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

// Internal dependencies - these are always required
#include "oneplog/common.hpp"
#include "oneplog/internal/log_entry.hpp"
#include "oneplog/internal/heap_memory.hpp"

// Optional fmt library support
// If ONEPLOG_USE_FMT is defined, fmt library will be used for formatting
// Otherwise, BinarySnapshot-based fallback formatting is used
#ifdef ONEPLOG_USE_FMT
#include <fmt/format.h>
#endif

// Platform-specific includes for thread/process ID
#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

namespace oneplog {

// ==============================================================================
// Compile-time Format Requirements / 编译期格式化需求
// ==============================================================================

template<bool NeedsTimestamp = true, bool NeedsLevel = true,
         bool NeedsThreadId = false, bool NeedsProcessId = false>
struct StaticFormatRequirements {
    static constexpr bool kNeedsTimestamp = NeedsTimestamp;
    static constexpr bool kNeedsLevel = NeedsLevel;
    static constexpr bool kNeedsThreadId = NeedsThreadId;
    static constexpr bool kNeedsProcessId = NeedsProcessId;
};

// ==============================================================================
// Static Format Types / 静态格式化器类型
// ==============================================================================

/// Message only format (like spdlog's %v)
/// 仅消息格式（类似 spdlog 的 %v）
struct MessageOnlyFormat {
    /// Format requirements - no metadata needed / 格式化需求 - 不需要元数据
    using Requirements = StaticFormatRequirements<false, false, false, false>;

#ifdef ONEPLOG_USE_FMT
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
#endif

    // =========================================================================
    // Non-fmt fallback implementations / 非 fmt 回退实现
    // =========================================================================
    
    /**
     * @brief Format LogEntry to string without fmt library (async mode)
     * @brief 不使用 fmt 库将 LogEntry 格式化为字符串（异步模式）
     * 
     * @param entry The log entry / 日志条目
     * @return Formatted string / 格式化后的字符串
     * 
     * _Requirements: 7.3, 7.4, 9.3_
     */
    static std::string FormatEntry(const LogEntry& entry) {
        return entry.snapshot.FormatAll();
    }
    
    /**
     * @brief Format with metadata to string without fmt library (sync mode)
     * @brief 不使用 fmt 库将带元数据的内容格式化为字符串（同步模式）
     * 
     * @param level Log level (unused) / 日志级别（未使用）
     * @param timestamp Timestamp (unused) / 时间戳（未使用）
     * @param threadId Thread ID (unused) / 线程 ID（未使用）
     * @param processId Process ID (unused) / 进程 ID（未使用）
     * @param snapshot The binary snapshot / 二进制快照
     * @return Formatted string / 格式化后的字符串
     * 
     * _Requirements: 7.3, 7.4, 9.2_
     */
    static std::string FormatEntry(Level /*level*/, uint64_t /*timestamp*/, 
                                   uint32_t /*threadId*/, uint32_t /*processId*/,
                                   const BinarySnapshot& snapshot) {
        return snapshot.FormatAll();
    }
};

/// Simple format: [HH:MM:SS] [LEVEL] message
/// 简单格式：[HH:MM:SS] [LEVEL] message
struct SimpleFormat {
    /// Format requirements - needs timestamp and level / 格式化需求 - 需要时间戳和级别
    using Requirements = StaticFormatRequirements<true, true, false, false>;

#ifdef ONEPLOG_USE_FMT
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
#endif

    // =========================================================================
    // Non-fmt fallback implementations / 非 fmt 回退实现
    // =========================================================================
    
    /**
     * @brief Format LogEntry to string without fmt library (async mode)
     * @brief 不使用 fmt 库将 LogEntry 格式化为字符串（异步模式）
     * 
     * @param entry The log entry / 日志条目
     * @return Formatted string / 格式化后的字符串
     * 
     * _Requirements: 7.3, 7.4, 9.3_
     */
    static std::string FormatEntry(const LogEntry& entry) {
        return FormatEntry(entry.level, entry.timestamp, 0, 0, entry.snapshot);
    }
    
    /**
     * @brief Format with metadata to string without fmt library (sync mode)
     * @brief 不使用 fmt 库将带元数据的内容格式化为字符串（同步模式）
     * 
     * @param level Log level / 日志级别
     * @param timestamp Nanosecond timestamp / 纳秒时间戳
     * @param threadId Thread ID (unused) / 线程 ID（未使用）
     * @param processId Process ID (unused) / 进程 ID（未使用）
     * @param snapshot The binary snapshot / 二进制快照
     * @return Formatted string / 格式化后的字符串
     * 
     * _Requirements: 7.3, 7.4, 9.2_
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

/// Full format: [YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [PID:TID] message
/// 完整格式：[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [PID:TID] message
struct FullFormat {
    /// Format requirements - needs all metadata / 格式化需求 - 需要所有元数据
    using Requirements = StaticFormatRequirements<true, true, true, true>;

#ifdef ONEPLOG_USE_FMT
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
#endif

    // =========================================================================
    // Non-fmt fallback implementations / 非 fmt 回退实现
    // =========================================================================
    
    /**
     * @brief Format LogEntry to string without fmt library (async mode)
     * @brief 不使用 fmt 库将 LogEntry 格式化为字符串（异步模式）
     * 
     * @param entry The log entry / 日志条目
     * @return Formatted string / 格式化后的字符串
     * 
     * _Requirements: 7.3, 7.4, 9.3_
     */
    static std::string FormatEntry(const LogEntry& entry) {
        return FormatEntry(entry.level, entry.timestamp, entry.threadId, 
                          entry.processId, entry.snapshot);
    }
    
    /**
     * @brief Format with metadata to string without fmt library (sync mode)
     * @brief 不使用 fmt 库将带元数据的内容格式化为字符串（同步模式）
     * 
     * @param level Log level / 日志级别
     * @param timestamp Nanosecond timestamp / 纳秒时间戳
     * @param threadId Thread ID / 线程 ID
     * @param processId Process ID / 进程 ID
     * @param snapshot The binary snapshot / 二进制快照
     * @return Formatted string / 格式化后的字符串
     * 
     * _Requirements: 7.3, 7.4, 9.2_
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

/// Null sink - discards all output
struct NullSinkType {
    void Write(std::string_view) noexcept {}
    void Flush() noexcept {}
    void Close() noexcept {}
};

/// Console sink - writes to stdout
struct ConsoleSinkType {
    void Write(std::string_view msg) noexcept {
        std::fwrite(msg.data(), 1, msg.size(), stdout);
        std::fputc('\n', stdout);
    }
    void Flush() noexcept { std::fflush(stdout); }
    void Close() noexcept {}
};

/// Stderr sink - writes to stderr
struct StderrSinkType {
    void Write(std::string_view msg) noexcept {
        std::fwrite(msg.data(), 1, msg.size(), stderr);
        std::fputc('\n', stderr);
    }
    void Flush() noexcept { std::fflush(stderr); }
    void Close() noexcept {}
};

/// File sink - writes to file
class FileSinkType {
public:
    FileSinkType() = default;
    explicit FileSinkType(const char* filename) { m_file = std::fopen(filename, "a"); }
    FileSinkType(FileSinkType&& o) noexcept : m_file(o.m_file) { o.m_file = nullptr; }
    FileSinkType& operator=(FileSinkType&& o) noexcept {
        if (this != &o) { Close(); m_file = o.m_file; o.m_file = nullptr; }
        return *this;
    }
    ~FileSinkType() { Close(); }

    void Write(std::string_view msg) noexcept {
        if (m_file) { std::fwrite(msg.data(), 1, msg.size(), m_file); std::fputc('\n', m_file); }
    }
    void Flush() noexcept { if (m_file) std::fflush(m_file); }
    void Close() noexcept { if (m_file) { std::fclose(m_file); m_file = nullptr; } }

private:
    FILE* m_file = nullptr;
};

// ==============================================================================
// Multi-Sink Wrapper / 多 Sink 包装器
// ==============================================================================

template<typename... Sinks>
class MultiSink {
public:
    MultiSink() = default;
    explicit MultiSink(Sinks... sinks) : m_sinks(std::move(sinks)...) {}

    void Write(std::string_view msg) noexcept {
        std::apply([&msg](auto&... sinks) { (sinks.Write(msg), ...); }, m_sinks);
    }
    void Flush() noexcept {
        std::apply([](auto&... sinks) { (sinks.Flush(), ...); }, m_sinks);
    }
    void Close() noexcept {
        std::apply([](auto&... sinks) { (sinks.Close(), ...); }, m_sinks);
    }

    template<size_t I>
    auto& Get() noexcept { return std::get<I>(m_sinks); }

private:
    std::tuple<Sinks...> m_sinks;
};

// Helper to create multi-sink
template<typename... Sinks>
MultiSink<Sinks...> MakeSinks(Sinks... sinks) {
    return MultiSink<Sinks...>(std::move(sinks)...);
}

// ==============================================================================
// Helper Functions / 辅助函数
// ==============================================================================

namespace internal {

inline uint64_t GetNanosecondTimestamp() noexcept {
    auto now = std::chrono::system_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());
}

inline uint32_t GetCurrentThreadId() noexcept {
#ifdef _WIN32
    return static_cast<uint32_t>(::GetCurrentThreadId());
#elif defined(__APPLE__)
    uint64_t tid;
    pthread_threadid_np(nullptr, &tid);
    return static_cast<uint32_t>(tid);
#else
    return static_cast<uint32_t>(pthread_self());
#endif
}

inline uint32_t GetCurrentProcessId() noexcept {
#ifdef _WIN32
    return static_cast<uint32_t>(::GetCurrentProcessId());
#else
    return static_cast<uint32_t>(::getpid());
#endif
}

}  // namespace internal

// ==============================================================================
// FastLogger Class / 快速日志器类
// ==============================================================================

/**
 * @brief High-performance template-based logger
 * @brief 高性能模板化日志器
 *
 * @tparam M Operating mode (Sync/Async)
 * @tparam FormatType Static format type
 * @tparam SinkType Static sink type (can be MultiSink for multiple sinks)
 * @tparam L Compile-time minimum log level
 * @tparam EnableWFC Enable Wait-Free-Capture (for async mode)
 * @tparam EnableShadowTail Enable shadow tail optimization (for async mode)
 */
template<Mode M = Mode::Sync,
         typename FormatType = MessageOnlyFormat,
         typename SinkType = NullSinkType,
         Level L = Level::Info,
         bool EnableWFC = false,
         bool EnableShadowTail = false>
class FastLogger {
public:
    static constexpr Mode kMode = M;
    static constexpr Level kMinLevel = L;
    using Format = FormatType;
    using Sink = SinkType;
    using Req = typename FormatType::Requirements;

    // =========================================================================
    // Constructors / 构造函数
    // =========================================================================

    FastLogger() : m_sink{} { 
        if constexpr (M == Mode::Async) {
            InitAsync(8192, QueueFullPolicy::Block);
        }
    }
    
    explicit FastLogger(SinkType sink) : m_sink(std::move(sink)) {
        if constexpr (M == Mode::Async) {
            InitAsync(8192, QueueFullPolicy::Block);
        }
    }
    
    // Async mode constructor with buffer size
    FastLogger(SinkType sink, size_t bufferSize, QueueFullPolicy policy = QueueFullPolicy::Block) 
        : m_sink(std::move(sink)) {
        if constexpr (M == Mode::Async) {
            InitAsync(bufferSize, policy);
        }
    }

    ~FastLogger() { 
        // Must stop worker before any member is destroyed
        if constexpr (M == Mode::Async) {
            StopWorker();
        }
        m_sink.Close();
        // Note: m_ringBuffer will be destroyed after this
    }

    // Non-copyable, movable
    FastLogger(const FastLogger&) = delete;
    FastLogger& operator=(const FastLogger&) = delete;
    FastLogger(FastLogger&& o) noexcept : m_sink(std::move(o.m_sink)) {
        if constexpr (M == Mode::Async) {
            o.StopWorker();
            m_ringBuffer = std::move(o.m_ringBuffer);
            StartWorker();
        }
    }

    // =========================================================================
    // Logging Methods / 日志记录方法
    // =========================================================================

    template<typename... Args>
    void Trace(const char* fmt, Args&&... args) noexcept {
        if constexpr (static_cast<uint8_t>(Level::Trace) >= static_cast<uint8_t>(L)) {
            LogImpl<Level::Trace>(fmt, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    void Debug(const char* fmt, Args&&... args) noexcept {
        if constexpr (static_cast<uint8_t>(Level::Debug) >= static_cast<uint8_t>(L)) {
            LogImpl<Level::Debug>(fmt, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    void Info(const char* fmt, Args&&... args) noexcept {
        if constexpr (static_cast<uint8_t>(Level::Info) >= static_cast<uint8_t>(L)) {
            LogImpl<Level::Info>(fmt, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    void Warn(const char* fmt, Args&&... args) noexcept {
        if constexpr (static_cast<uint8_t>(Level::Warn) >= static_cast<uint8_t>(L)) {
            LogImpl<Level::Warn>(fmt, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    void Error(const char* fmt, Args&&... args) noexcept {
        if constexpr (static_cast<uint8_t>(Level::Error) >= static_cast<uint8_t>(L)) {
            LogImpl<Level::Error>(fmt, std::forward<Args>(args)...);
        }
    }

    template<typename... Args>
    void Critical(const char* fmt, Args&&... args) noexcept {
        if constexpr (static_cast<uint8_t>(Level::Critical) >= static_cast<uint8_t>(L)) {
            LogImpl<Level::Critical>(fmt, std::forward<Args>(args)...);
        }
    }

    // =========================================================================
    // Control Methods / 控制方法
    // =========================================================================

    void Flush() noexcept {
        if constexpr (M == Mode::Async) {
            // Wait for ring buffer to drain
            if (m_ringBuffer) {
                while (!m_ringBuffer->IsEmpty()) {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
        }
        m_sink.Flush();
    }

    void Shutdown() noexcept {
        if constexpr (M == Mode::Async) {
            StopWorker();
        }
        m_sink.Close();
    }

    SinkType& GetSink() noexcept { return m_sink; }
    const SinkType& GetSink() const noexcept { return m_sink; }

private:
    // =========================================================================
    // Internal Implementation / 内部实现
    // =========================================================================

    void InitAsync(size_t bufferSize, QueueFullPolicy policy) {
        if constexpr (M == Mode::Async) {
            m_ringBuffer = std::make_unique<internal::HeapRingBuffer<LogEntry, EnableWFC, EnableShadowTail>>(
                bufferSize, policy);
            StartWorker();
        }
    }

    template<Level LogLevel, typename... Args>
    void LogImpl(const char* fmt, Args&&... args) noexcept {
        if constexpr (M == Mode::Sync) {
            // Sync mode: format and write directly
            LogImplSync<LogLevel>(fmt, std::forward<Args>(args)...);
        } else {
            // Async mode: capture to LogEntry and push to ring buffer
            LogImplAsync<LogLevel>(fmt, std::forward<Args>(args)...);
        }
    }

    template<Level LogLevel, typename... Args>
    void LogImplSync(const char* fmt, Args&&... args) noexcept {
        [[maybe_unused]] uint64_t timestamp = 0;
        [[maybe_unused]] uint32_t threadId = 0;
        [[maybe_unused]] uint32_t processId = 0;

        // Compile-time conditional: only get what format needs
        if constexpr (Req::kNeedsTimestamp) timestamp = internal::GetNanosecondTimestamp();
        if constexpr (Req::kNeedsThreadId) threadId = internal::GetCurrentThreadId();
        if constexpr (Req::kNeedsProcessId) processId = internal::GetCurrentProcessId();

#ifdef ONEPLOG_USE_FMT
        fmt::memory_buffer buffer;
        FormatType::FormatTo(buffer, LogLevel, timestamp, threadId, processId,
                             fmt, std::forward<Args>(args)...);
        m_sink.Write(std::string_view(buffer.data(), buffer.size()));
#else
        // Non-fmt fallback: use BinarySnapshot for formatting
        BinarySnapshot snapshot;
        snapshot.CaptureStringView(std::string_view(fmt));
        if constexpr (sizeof...(Args) > 0) {
            snapshot.Capture(std::forward<Args>(args)...);
        }
        std::string msg = FormatType::FormatEntry(LogLevel, timestamp, threadId, 
                                                   processId, snapshot);
        m_sink.Write(msg);
#endif
    }

    template<Level LogLevel, typename... Args>
    void LogImplAsync(const char* fmt, Args&&... args) noexcept {
        if (!m_ringBuffer) return;

        LogEntry entry;
        entry.timestamp = internal::GetNanosecondTimestamp();
        entry.level = LogLevel;
        
        // Compile-time conditional: only get what format needs
        if constexpr (Req::kNeedsThreadId) {
            entry.threadId = internal::GetCurrentThreadId();
        }
        if constexpr (Req::kNeedsProcessId) {
            entry.processId = internal::GetCurrentProcessId();
        }

        // Capture format string and arguments to BinarySnapshot
        if constexpr (sizeof...(Args) == 0) {
            entry.snapshot.CaptureStringView(std::string_view(fmt));
        } else {
            entry.snapshot.CaptureStringView(std::string_view(fmt));
            entry.snapshot.Capture(std::forward<Args>(args)...);
        }

        m_ringBuffer->TryPush(std::move(entry));
        m_ringBuffer->NotifyConsumer();
    }

    void StartWorker() {
        if constexpr (M == Mode::Async) {
            // Check if already running
            if (m_running.load(std::memory_order_acquire)) {
                return;
            }
            
            m_running.store(true, std::memory_order_release);
            m_worker = std::thread([this] {
                // Worker thread main loop - follows WriterThread pattern
                while (m_running.load(std::memory_order_acquire)) {
                    bool hasData = false;
                    LogEntry entry;
                    
                    // Process all available entries
                    while (m_running.load(std::memory_order_relaxed) &&
                           m_ringBuffer && m_ringBuffer->TryPop(entry)) {
#ifdef ONEPLOG_USE_FMT
                        fmt::memory_buffer buffer;
                        FormatType::FormatEntryTo(buffer, entry);
                        m_sink.Write(std::string_view(buffer.data(), buffer.size()));
#else
                        // Non-fmt fallback: use FormatEntry method
                        std::string msg = FormatType::FormatEntry(entry);
                        m_sink.Write(msg);
#endif
                        hasData = true;
                    }
                    
                    // No data available, wait for notification
                    if (!hasData && m_running.load(std::memory_order_relaxed)) {
                        if (m_ringBuffer) {
                            m_ringBuffer->WaitForData(std::chrono::microseconds(100), 
                                                      std::chrono::milliseconds(10));
                        }
                    }
                }
                
                // Drain remaining entries before exit
                if (m_ringBuffer) {
                    LogEntry entry;
                    while (m_ringBuffer->TryPop(entry)) {
#ifdef ONEPLOG_USE_FMT
                        fmt::memory_buffer buffer;
                        FormatType::FormatEntryTo(buffer, entry);
                        m_sink.Write(std::string_view(buffer.data(), buffer.size()));
#else
                        // Non-fmt fallback: use FormatEntry method
                        std::string msg = FormatType::FormatEntry(entry);
                        m_sink.Write(msg);
#endif
                    }
                }
                
                m_sink.Flush();
            });
        }
    }

    void StopWorker() {
        if constexpr (M == Mode::Async) {
            // Check if worker is running
            if (!m_running.load(std::memory_order_acquire)) {
                return;
            }
            
            // Signal worker to stop
            m_running.store(false, std::memory_order_release);
            
            // Wake up worker if it's waiting
            if (m_ringBuffer) {
                m_ringBuffer->NotifyConsumer();
            }
            
            // Wait for worker to finish
            if (m_worker.joinable()) {
                m_worker.join();
            }
            
            // Drain any remaining entries after worker has stopped
            // (following the pattern from original WriterThread)
            if (m_ringBuffer) {
                LogEntry entry;
                while (m_ringBuffer->TryPop(entry)) {
#ifdef ONEPLOG_USE_FMT
                    fmt::memory_buffer buffer;
                    FormatType::FormatEntryTo(buffer, entry);
                    m_sink.Write(std::string_view(buffer.data(), buffer.size()));
#else
                    // Non-fmt fallback: use FormatEntry method
                    std::string msg = FormatType::FormatEntry(entry);
                    m_sink.Write(msg);
#endif
                }
            }
        }
    }

    // =========================================================================
    // Member Variables / 成员变量
    // =========================================================================
    
    // IMPORTANT: Declaration order matters for destruction order!
    // 重要：声明顺序决定析构顺序！
    // 
    // Destruction order (reverse of declaration):
    // 1. m_sink - destroyed last (after worker is stopped)
    // 2. m_running - atomic flag
    // 3. m_worker - thread (should be joined before ringBuffer is destroyed)
    // 4. m_ringBuffer - destroyed first
    //
    // But we need ringBuffer to be valid while worker is running!
    // So we must stop worker in destructor BEFORE members are destroyed.
    
    // Async mode only - using original HeapRingBuffer
    std::unique_ptr<internal::HeapRingBuffer<LogEntry, EnableWFC, EnableShadowTail>> m_ringBuffer;
    std::thread m_worker;
    std::atomic<bool> m_running{false};

    // Sink is declared last so it's destroyed first (after worker is stopped)
    // Sink 最后声明，这样它会最先被析构（在 worker 停止之后）
    SinkType m_sink{};
};

// ==============================================================================
// Type Aliases / 类型别名
// ==============================================================================

// Sync loggers
template<Level L = Level::Info>
using FastSyncLogger = FastLogger<Mode::Sync, SimpleFormat, ConsoleSinkType, L>;

template<Level L = Level::Info>
using FastSyncFileLogger = FastLogger<Mode::Sync, SimpleFormat, FileSinkType, L>;

// Async loggers
template<Level L = Level::Info>
using FastAsyncLogger = FastLogger<Mode::Async, SimpleFormat, ConsoleSinkType, L>;

template<Level L = Level::Info>
using FastAsyncFileLogger = FastLogger<Mode::Async, SimpleFormat, FileSinkType, L>;

// Benchmark logger (fastest)
using BenchmarkLogger = FastLogger<Mode::Sync, MessageOnlyFormat, NullSinkType, Level::Info>;

// ==============================================================================
// Backward Compatibility Type Aliases / 向后兼容类型别名
// ==============================================================================

/**
 * @brief Simplified type aliases for common use cases
 * @brief 常用场景的简化类型别名
 *
 * These aliases provide backward compatibility with existing code and
 * simplify the migration to the new FastLogger API.
 *
 * 这些别名提供与现有代码的向后兼容性，并简化向新 FastLogger API 的迁移。
 *
 * _Requirements: 1.7, 14.5_
 */

/// Sync logger with console output / 同步日志器，控制台输出
using SyncLogger = FastLogger<Mode::Sync, SimpleFormat, ConsoleSinkType, Level::Info>;

/// Async logger with console output / 异步日志器，控制台输出
using AsyncLogger = FastLogger<Mode::Async, SimpleFormat, ConsoleSinkType, Level::Info>;

/// Sync logger with full format / 同步日志器，完整格式
using SyncFullLogger = FastLogger<Mode::Sync, FullFormat, ConsoleSinkType, Level::Info>;

/// Async logger with full format / 异步日志器，完整格式
using AsyncFullLogger = FastLogger<Mode::Async, FullFormat, ConsoleSinkType, Level::Info>;

/// High performance logger (null sink, message only) / 高性能日志器（空 Sink，仅消息）
using HighPerformanceLogger = FastLogger<Mode::Async, MessageOnlyFormat, NullSinkType, Level::Info>;

/**
 * @brief Default compile-time log level based on build configuration
 * @brief 基于构建配置的默认编译期日志级别
 *
 * In Debug mode (NDEBUG not defined): Level::Debug
 * In Release mode (NDEBUG defined): Level::Info
 */
#ifndef ONEPLOG_DEFAULT_LEVEL_DEFINED_FAST
#define ONEPLOG_DEFAULT_LEVEL_DEFINED_FAST
#ifdef NDEBUG
constexpr Level kFastDefaultLevel = Level::Info;
#else
constexpr Level kFastDefaultLevel = Level::Debug;
#endif
#endif

/**
 * @brief Generic Logger type alias for backward compatibility
 * @brief 用于向后兼容的通用 Logger 类型别名
 *
 * This template alias allows existing code using Logger<Mode, Level, ...>
 * to continue working with the new FastLogger implementation.
 *
 * 此模板别名允许使用 Logger<Mode, Level, ...> 的现有代码
 * 继续使用新的 FastLogger 实现。
 *
 * @tparam M Operating mode (Sync/Async) / 运行模式
 * @tparam L Minimum log level / 最小日志级别
 * @tparam EnableWFC Enable WFC functionality (unused in this version) / 启用 WFC 功能（此版本未使用）
 * @tparam EnableShadowTail Enable shadow tail optimization (unused in this version) / 启用影子尾指针优化（此版本未使用）
 *
 * _Requirements: 14.5_
 */
template<Mode M = Mode::Async, 
         Level L = kFastDefaultLevel,
         bool EnableWFC = false, 
         bool EnableShadowTail = true>
using Logger = FastLogger<M, SimpleFormat, ConsoleSinkType, L, EnableWFC, EnableShadowTail>;

// ==============================================================================
// Default FastLogger / 默认快速日志器
// ==============================================================================

#ifndef ONEPLOG_FAST_DEFAULT_MODE
#define ONEPLOG_FAST_DEFAULT_MODE Mode::Sync
#endif

#ifndef ONEPLOG_FAST_DEFAULT_LEVEL
#ifdef NDEBUG
#define ONEPLOG_FAST_DEFAULT_LEVEL Level::Info
#else
#define ONEPLOG_FAST_DEFAULT_LEVEL Level::Debug
#endif
#endif

using DefaultFastLogger = FastLogger<ONEPLOG_FAST_DEFAULT_MODE, SimpleFormat, ConsoleSinkType, ONEPLOG_FAST_DEFAULT_LEVEL>;

namespace fast {

namespace internal {
inline std::unique_ptr<DefaultFastLogger>& GetGlobalLogger() {
    static std::unique_ptr<DefaultFastLogger> logger;
    return logger;
}
inline std::mutex& GetGlobalMutex() {
    static std::mutex mtx;
    return mtx;
}
}  // namespace internal

// ==============================================================================
// Global API / 全局 API
// ==============================================================================

inline void Init() {
    std::lock_guard<std::mutex> lock(internal::GetGlobalMutex());
    internal::GetGlobalLogger() = std::make_unique<DefaultFastLogger>();
}

inline void Shutdown() {
    std::lock_guard<std::mutex> lock(internal::GetGlobalMutex());
    if (internal::GetGlobalLogger()) {
        internal::GetGlobalLogger()->Shutdown();
        internal::GetGlobalLogger().reset();
    }
}

inline void Flush() {
    std::lock_guard<std::mutex> lock(internal::GetGlobalMutex());
    if (internal::GetGlobalLogger()) internal::GetGlobalLogger()->Flush();
}

template<typename... Args>
inline void Trace(const char* fmt, Args&&... args) {
    if (internal::GetGlobalLogger()) internal::GetGlobalLogger()->Trace(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void Debug(const char* fmt, Args&&... args) {
    if (internal::GetGlobalLogger()) internal::GetGlobalLogger()->Debug(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void Info(const char* fmt, Args&&... args) {
    if (internal::GetGlobalLogger()) internal::GetGlobalLogger()->Info(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void Warn(const char* fmt, Args&&... args) {
    if (internal::GetGlobalLogger()) internal::GetGlobalLogger()->Warn(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void Error(const char* fmt, Args&&... args) {
    if (internal::GetGlobalLogger()) internal::GetGlobalLogger()->Error(fmt, std::forward<Args>(args)...);
}

template<typename... Args>
inline void Critical(const char* fmt, Args&&... args) {
    if (internal::GetGlobalLogger()) internal::GetGlobalLogger()->Critical(fmt, std::forward<Args>(args)...);
}

}  // namespace fast

}  // namespace oneplog
