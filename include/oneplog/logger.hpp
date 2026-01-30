/**
 * @file logger.hpp
 * @brief Template-based Logger class for onePlog
 * @brief onePlog 模板化日志器
 *
 * This file provides a template-based Logger implementation that uses
 * C++17 template parameters to specify operating mode, log level, and
 * WFC functionality at compile time, achieving zero-overhead abstraction.
 *
 * 本文件提供基于模板的 Logger 实现，使用 C++17 模板参数在编译时
 * 指定运行模式、日志级别和 WFC 功能，实现零开销抽象。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "oneplog/binary_snapshot.hpp"
#include "oneplog/common.hpp"
#include "oneplog/format.hpp"
#include "oneplog/heap_ring_buffer.hpp"
#include "oneplog/log_entry.hpp"
#include "oneplog/name_manager.hpp"
#include "oneplog/pipeline_thread.hpp"
#include "oneplog/shared_memory.hpp"
#include "oneplog/sink.hpp"
#include "oneplog/writer_thread.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

namespace oneplog {

// ==============================================================================
// Compile-time Default Level / 编译时默认级别
// ==============================================================================

/**
 * @brief Default log level based on build configuration
 * @brief 基于构建配置的默认日志级别
 *
 * In Debug mode (NDEBUG not defined): Level::Debug
 * In Release mode (NDEBUG defined): Level::Info
 *
 * 调试模式（未定义 NDEBUG）：Level::Debug
 * 发布模式（定义了 NDEBUG）：Level::Info
 */
#ifdef NDEBUG
constexpr Level kDefaultLevel = Level::Info;
#else
constexpr Level kDefaultLevel = Level::Debug;
#endif

// ==============================================================================
// Logger Configuration / Logger 配置
// ==============================================================================

/**
 * @brief Logger configuration structure (runtime options)
 * @brief Logger 配置结构（运行时选项）
 *
 * This structure contains runtime configuration options that do not affect
 * the compile-time mode and level settings.
 *
 * 此结构包含不影响编译时模式和级别设置的运行时配置选项。
 */
struct LoggerConfig {
    size_t heapRingBufferSize{8192};             ///< Heap ring buffer size / 堆环形队列大小
    size_t sharedRingBufferSize{4096};           ///< Shared ring buffer size / 共享环形队列大小
    QueueFullPolicy queueFullPolicy{QueueFullPolicy::DropNewest};  ///< Queue full policy / 队列满策略
    std::string sharedMemoryName;                ///< Shared memory name (for MProc mode) / 共享内存名称
    std::chrono::microseconds pollInterval{1};   ///< Poll interval / 轮询间隔
    std::chrono::milliseconds pollTimeout{10};   ///< Poll timeout / 轮询超时
    std::string processName;                     ///< Process name (optional) / 进程名（可选）
};

// ==============================================================================
// Logger Class / Logger 类
// ==============================================================================

/**
 * @brief Template-based logger class with compile-time configuration
 * @brief 具有编译时配置的模板化日志器类
 *
 * @tparam M Operating mode (Sync/Async/MProc), default: Mode::Async
 * @tparam L Compile-time minimum log level, default: kDefaultLevel
 * @tparam EnableWFC Enable WFC (Wait For Completion) functionality, default: false
 *
 * @tparam M 运行模式（Sync/Async/MProc），默认：Mode::Async
 * @tparam L 编译时最小日志级别，默认：kDefaultLevel
 * @tparam EnableWFC 启用 WFC（等待完成）功能，默认：false
 *
 * Features:
 * - Compile-time mode selection eliminates runtime branching
 * - Compile-time level filtering optimizes away disabled log calls
 * - WFC functionality can be completely disabled at compile time
 *
 * 特性：
 * - 编译时模式选择消除运行时分支
 * - 编译时级别过滤优化掉禁用的日志调用
 * - WFC 功能可在编译时完全禁用
 */
template<Mode M = Mode::Async, Level L = kDefaultLevel, bool EnableWFC = false>
class Logger {
public:
    // =========================================================================
    // Compile-time Constants / 编译时常量
    // =========================================================================

    /// Operating mode / 运行模式
    static constexpr Mode kMode = M;
    
    /// Minimum log level (compile-time) / 最小日志级别（编译时）
    static constexpr Level kMinLevel = L;
    
    /// WFC functionality enabled / WFC 功能启用
    static constexpr bool kEnableWFC = EnableWFC;

    // =========================================================================
    // Constructors and Destructor / 构造函数和析构函数
    // =========================================================================

    /**
     * @brief Construct a logger
     * @brief 构造日志器
     */
    Logger()
        : m_initialized(false) {}

    /**
     * @brief Destructor - calls Shutdown()
     * @brief 析构函数 - 调用 Shutdown()
     */
    ~Logger() {
        Shutdown();
    }

    // =========================================================================
    // Non-copyable, Movable / 不可复制，可移动
    // =========================================================================

    /// Deleted copy constructor / 删除的复制构造函数
    Logger(const Logger&) = delete;
    
    /// Deleted copy assignment / 删除的复制赋值
    Logger& operator=(const Logger&) = delete;

    /**
     * @brief Move constructor
     * @brief 移动构造函数
     */
    Logger(Logger&& other) noexcept
        : m_initialized(other.m_initialized)
        , m_sinks(std::move(other.m_sinks))
        , m_format(std::move(other.m_format))
        , m_heapRingBuffer(std::move(other.m_heapRingBuffer))
        , m_sharedMemory(std::move(other.m_sharedMemory))
        , m_pipelineThread(std::move(other.m_pipelineThread))
        , m_writerThread(std::move(other.m_writerThread)) {
        other.m_initialized = false;
    }

    /**
     * @brief Move assignment operator
     * @brief 移动赋值运算符
     */
    Logger& operator=(Logger&& other) noexcept {
        if (this != &other) {
            Shutdown();
            
            m_initialized = other.m_initialized;
            m_sinks = std::move(other.m_sinks);
            m_format = std::move(other.m_format);
            m_heapRingBuffer = std::move(other.m_heapRingBuffer);
            m_sharedMemory = std::move(other.m_sharedMemory);
            m_pipelineThread = std::move(other.m_pipelineThread);
            m_writerThread = std::move(other.m_writerThread);
            
            other.m_initialized = false;
        }
        return *this;
    }

    // =========================================================================
    // Initialization and Shutdown / 初始化和关闭
    // =========================================================================

    /**
     * @brief Initialize the logger with default configuration
     * @brief 使用默认配置初始化日志器
     */
    void Init() {
        LoggerConfig config;
        InitImpl(config);
    }

    /**
     * @brief Initialize the logger with custom configuration
     * @brief 使用自定义配置初始化日志器
     *
     * @param config Logger configuration / 日志器配置
     */
    void Init(const LoggerConfig& config) {
        InitImpl(config);
    }

    /**
     * @brief Shutdown the logger
     * @brief 关闭日志器
     *
     * Stops background threads, flushes and closes sinks, releases resources.
     * 停止后台线程，刷新并关闭 Sink，释放资源。
     */
    void Shutdown() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_initialized) {
            return;
        }

        // Stop threads / 停止线程
        if (m_writerThread) {
            m_writerThread->Stop();
            m_writerThread.reset();
        }

        if (m_pipelineThread) {
            m_pipelineThread->Stop();
            m_pipelineThread.reset();
        }

        // Close sinks / 关闭 Sink
        for (auto& sink : m_sinks) {
            if (sink) {
                sink->Flush();
                sink->Close();
            }
        }

        // Release resources / 释放资源
        m_heapRingBuffer.reset();
        m_sharedMemory.reset();

        // Shutdown NameManager / 关闭 NameManager
        NameManager<EnableWFC>::Shutdown();

        m_initialized = false;
    }

    /**
     * @brief Check if logger is initialized
     * @brief 检查日志器是否已初始化
     */
    bool IsInitialized() const { return m_initialized; }

    // =========================================================================
    // Configuration / 配置
    // =========================================================================

    /**
     * @brief Set the primary sink
     * @brief 设置主 Sink
     */
    void SetSink(std::shared_ptr<Sink> sink) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_sinks.clear();
        if (sink) {
            m_sinks.push_back(std::move(sink));
        }
    }

    /**
     * @brief Add a sink
     * @brief 添加 Sink
     */
    void AddSink(std::shared_ptr<Sink> sink) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (sink) {
            m_sinks.push_back(std::move(sink));
        }
    }

    /**
     * @brief Get sinks
     * @brief 获取 Sink 列表
     */
    const std::vector<std::shared_ptr<Sink>>& GetSinks() const {
        return m_sinks;
    }

    /**
     * @brief Set the formatter
     * @brief 设置格式化器
     */
    void SetFormat(std::shared_ptr<Format> format) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_format = std::move(format);
        if (m_writerThread) {
            m_writerThread->SetFormat(m_format);
        }
    }

    /**
     * @brief Get the formatter
     * @brief 获取格式化器
     */
    std::shared_ptr<Format> GetFormat() const {
        return m_format;
    }

    // =========================================================================
    // Logging Methods / 日志记录方法
    // =========================================================================

    /**
     * @brief Log a trace message (compile-time filtered)
     * @brief 记录跟踪消息（编译时过滤）
     */
    template<typename... Args>
    void Trace(const char* fmt, Args&&... args) {
        if constexpr (static_cast<uint8_t>(Level::Trace) >= static_cast<uint8_t>(L)) {
            LogImpl<Level::Trace>(ONEPLOG_CURRENT_LOCATION, fmt, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Log a debug message (compile-time filtered)
     * @brief 记录调试消息（编译时过滤）
     */
    template<typename... Args>
    void Debug(const char* fmt, Args&&... args) {
        if constexpr (static_cast<uint8_t>(Level::Debug) >= static_cast<uint8_t>(L)) {
            LogImpl<Level::Debug>(ONEPLOG_CURRENT_LOCATION, fmt, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Log an info message (compile-time filtered)
     * @brief 记录信息消息（编译时过滤）
     */
    template<typename... Args>
    void Info(const char* fmt, Args&&... args) {
        if constexpr (static_cast<uint8_t>(Level::Info) >= static_cast<uint8_t>(L)) {
            LogImpl<Level::Info>(ONEPLOG_CURRENT_LOCATION, fmt, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Log a warning message (compile-time filtered)
     * @brief 记录警告消息（编译时过滤）
     */
    template<typename... Args>
    void Warn(const char* fmt, Args&&... args) {
        if constexpr (static_cast<uint8_t>(Level::Warn) >= static_cast<uint8_t>(L)) {
            LogImpl<Level::Warn>(ONEPLOG_CURRENT_LOCATION, fmt, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Log an error message (compile-time filtered)
     * @brief 记录错误消息（编译时过滤）
     */
    template<typename... Args>
    void Error(const char* fmt, Args&&... args) {
        if constexpr (static_cast<uint8_t>(Level::Error) >= static_cast<uint8_t>(L)) {
            LogImpl<Level::Error>(ONEPLOG_CURRENT_LOCATION, fmt, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Log a critical message (compile-time filtered)
     * @brief 记录严重错误消息（编译时过滤）
     */
    template<typename... Args>
    void Critical(const char* fmt, Args&&... args) {
        if constexpr (static_cast<uint8_t>(Level::Critical) >= static_cast<uint8_t>(L)) {
            LogImpl<Level::Critical>(ONEPLOG_CURRENT_LOCATION, fmt, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Log a message with runtime level selection
     * @brief 使用运行时级别选择记录消息
     */
    template<typename... Args>
    void Log(Level level, const char* fmt, Args&&... args) {
        if (static_cast<uint8_t>(level) < static_cast<uint8_t>(L)) {
            return;
        }
        
        uint64_t timestamp = GetNanosecondTimestamp();
        
        if constexpr (M == Mode::Sync) {
            ProcessEntrySyncDirect(level, timestamp, ONEPLOG_CURRENT_LOCATION, 
                                   fmt, std::forward<Args>(args)...);
        } else {
            ProcessEntryAsync(level, timestamp, ONEPLOG_CURRENT_LOCATION, 
                              fmt, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Log a message with runtime level and source location
     * @brief 使用运行时级别和源位置记录消息
     */
    template<typename... Args>
    void Log(Level level, const SourceLocation& loc, const char* fmt, Args&&... args) {
        if (static_cast<uint8_t>(level) < static_cast<uint8_t>(L)) {
            return;
        }
        
        uint64_t timestamp = GetNanosecondTimestamp();
        
        if constexpr (M == Mode::Sync) {
            ProcessEntrySyncDirect(level, timestamp, loc, fmt, std::forward<Args>(args)...);
        } else {
            ProcessEntryAsync(level, timestamp, loc, fmt, std::forward<Args>(args)...);
        }
    }

    // =========================================================================
    // WFC Logging Methods / WFC 日志记录方法
    // =========================================================================

    /**
     * @brief Log a trace message with WFC (Wait For Completion)
     * @brief 使用 WFC（等待完成）记录跟踪消息
     */
    template<typename... Args>
    void TraceWFC(const char* fmt, Args&&... args) {
        if constexpr (EnableWFC) {
            if constexpr (static_cast<uint8_t>(Level::Trace) >= static_cast<uint8_t>(L)) {
                LogWFCImpl<Level::Trace>(fmt, std::forward<Args>(args)...);
            }
        } else {
            Trace(fmt, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Log a debug message with WFC
     * @brief 使用 WFC 记录调试消息
     */
    template<typename... Args>
    void DebugWFC(const char* fmt, Args&&... args) {
        if constexpr (EnableWFC) {
            if constexpr (static_cast<uint8_t>(Level::Debug) >= static_cast<uint8_t>(L)) {
                LogWFCImpl<Level::Debug>(fmt, std::forward<Args>(args)...);
            }
        } else {
            Debug(fmt, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Log an info message with WFC
     * @brief 使用 WFC 记录信息消息
     */
    template<typename... Args>
    void InfoWFC(const char* fmt, Args&&... args) {
        if constexpr (EnableWFC) {
            if constexpr (static_cast<uint8_t>(Level::Info) >= static_cast<uint8_t>(L)) {
                LogWFCImpl<Level::Info>(fmt, std::forward<Args>(args)...);
            }
        } else {
            Info(fmt, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Log a warning message with WFC
     * @brief 使用 WFC 记录警告消息
     */
    template<typename... Args>
    void WarnWFC(const char* fmt, Args&&... args) {
        if constexpr (EnableWFC) {
            if constexpr (static_cast<uint8_t>(Level::Warn) >= static_cast<uint8_t>(L)) {
                LogWFCImpl<Level::Warn>(fmt, std::forward<Args>(args)...);
            }
        } else {
            Warn(fmt, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Log an error message with WFC
     * @brief 使用 WFC 记录错误消息
     */
    template<typename... Args>
    void ErrorWFC(const char* fmt, Args&&... args) {
        if constexpr (EnableWFC) {
            if constexpr (static_cast<uint8_t>(Level::Error) >= static_cast<uint8_t>(L)) {
                LogWFCImpl<Level::Error>(fmt, std::forward<Args>(args)...);
            }
        } else {
            Error(fmt, std::forward<Args>(args)...);
        }
    }

    /**
     * @brief Log a critical message with WFC
     * @brief 使用 WFC 记录严重错误消息
     */
    template<typename... Args>
    void CriticalWFC(const char* fmt, Args&&... args) {
        if constexpr (EnableWFC) {
            if constexpr (static_cast<uint8_t>(Level::Critical) >= static_cast<uint8_t>(L)) {
                LogWFCImpl<Level::Critical>(fmt, std::forward<Args>(args)...);
            }
        } else {
            Critical(fmt, std::forward<Args>(args)...);
        }
    }

    // =========================================================================
    // Query Methods / 查询方法
    // =========================================================================

    static constexpr Mode GetMode() { return kMode; }
    static constexpr Level GetMinLevel() { return kMinLevel; }
    static constexpr bool IsWFCEnabled() { return kEnableWFC; }

    // =========================================================================
    // Flush / 刷新
    // =========================================================================

    void Flush() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_writerThread) {
            m_writerThread->Flush();
        }
        
        for (auto& sink : m_sinks) {
            if (sink) {
                sink->Flush();
            }
        }
    }

private:
    // =========================================================================
    // Internal Implementation / 内部实现
    // =========================================================================

    void InitImpl(const LoggerConfig& config) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_initialized) {
            return;
        }

        // Initialize NameManager for name resolution / 初始化 NameManager 用于名称解析
        NameManager<EnableWFC>::Initialize(M);
        
        // Set process name if provided / 如果提供了进程名则设置
        if (!config.processName.empty()) {
            NameManager<EnableWFC>::SetProcessName(config.processName);
        }

        if constexpr (M == Mode::Sync) {
            // Sync mode: no background threads needed
        }
        else if constexpr (M == Mode::Async) {
            // Async mode: create heap ring buffer and writer thread
            m_heapRingBuffer = std::make_unique<HeapRingBuffer<LogEntry, EnableWFC>>(
                config.heapRingBufferSize, config.queueFullPolicy);
            
            if (!m_sinks.empty()) {
                m_writerThread = std::make_unique<WriterThread<EnableWFC>>(m_sinks[0]);
                m_writerThread->SetHeapRingBuffer(m_heapRingBuffer.get());
                m_writerThread->SetPollInterval(config.pollInterval);
                m_writerThread->SetPollTimeout(config.pollTimeout);
                if (m_format) {
                    m_writerThread->SetFormat(m_format);
                }
                m_writerThread->Start();
            }
        }
        else if constexpr (M == Mode::MProc) {
            // Multi-process mode
            if (!config.sharedMemoryName.empty()) {
                m_sharedMemory = SharedMemory<EnableWFC>::Create(
                    config.sharedMemoryName,
                    config.sharedRingBufferSize,
                    config.queueFullPolicy);
            }

            m_heapRingBuffer = std::make_unique<HeapRingBuffer<LogEntry, EnableWFC>>(
                config.heapRingBufferSize, config.queueFullPolicy);

            if (m_sharedMemory) {
                m_pipelineThread = std::make_unique<PipelineThread<EnableWFC>>(
                    *m_heapRingBuffer, *m_sharedMemory);
                m_pipelineThread->SetPollInterval(config.pollInterval);
                m_pipelineThread->SetPollTimeout(config.pollTimeout);
                m_pipelineThread->Start();

                if (!m_sinks.empty()) {
                    m_writerThread = std::make_unique<WriterThread<EnableWFC>>(m_sinks[0]);
                    m_writerThread->SetSharedRingBuffer(m_sharedMemory->GetRingBuffer());
                    m_writerThread->SetPollInterval(config.pollInterval);
                    m_writerThread->SetPollTimeout(config.pollTimeout);
                    if (m_format) {
                        m_writerThread->SetFormat(m_format);
                    }
                    m_writerThread->Start();
                }
            }
        }

        m_initialized = true;
    }

    template<Level LogLevel, typename... Args>
    void LogImpl(const SourceLocation& loc, const char* fmt, Args&&... args) noexcept {
        try {
            uint64_t timestamp = GetNanosecondTimestamp();
            
            if constexpr (M == Mode::Sync) {
                ProcessEntrySyncDirect(LogLevel, timestamp, loc, fmt, std::forward<Args>(args)...);
            } else {
                ProcessEntryAsync(LogLevel, timestamp, loc, fmt, std::forward<Args>(args)...);
            }
        } catch (const std::exception& e) {
            FallbackToStderr(LogLevel, fmt, e.what());
        } catch (...) {
            FallbackToStderr(LogLevel, fmt, "unknown exception");
        }
    }

    template<Level LogLevel, typename... Args>
    void LogWFCImpl(const char* fmt, Args&&... args) noexcept {
        try {
            if constexpr (EnableWFC) {
                if constexpr (M == Mode::Sync) {
                    LogImpl<LogLevel>(ONEPLOG_CURRENT_LOCATION, fmt, std::forward<Args>(args)...);
                } else {
                    LogEntry entry;
                    entry.timestamp = GetNanosecondTimestamp();
                    entry.level = LogLevel;
                    entry.threadId = GetCurrentThreadId();
                    entry.processId = GetCurrentProcessId();

#ifndef NDEBUG
                    auto loc = ONEPLOG_CURRENT_LOCATION;
                    entry.file = loc.file;
                    entry.function = loc.function;
                    entry.line = loc.line;
#endif

                    if constexpr (sizeof...(Args) == 0) {
                        entry.snapshot.CaptureStringView(std::string_view(fmt));
                    } else {
                        entry.snapshot.CaptureStringView(std::string_view(fmt));
                        entry.snapshot.Capture(std::forward<Args>(args)...);
                    }

                    if (m_heapRingBuffer) {
                        m_heapRingBuffer->TryPushWFC(std::move(entry));
                    }
                }
            }
        } catch (const std::exception& e) {
            FallbackToStderr(LogLevel, fmt, e.what());
        } catch (...) {
            FallbackToStderr(LogLevel, fmt, "unknown exception");
        }
    }

    template<typename... Args>
    void ProcessEntrySyncDirect(Level level, uint64_t timestamp,
                                const SourceLocation& loc,
                                const char* fmt, Args&&... args) {
        FormatRequirements req;
        if (m_format) {
            req = m_format->GetRequirements();
        }

        uint32_t threadId = req.needsThreadId ? GetCurrentThreadId() : 0;
        uint32_t processId = req.needsProcessId ? GetCurrentProcessId() : 0;

#ifdef ONEPLOG_USE_FMT
        fmt::memory_buffer msgBuffer;
        
        if constexpr (sizeof...(Args) == 0) {
            msgBuffer.append(std::string_view(fmt));
        } else {
            fmt::format_to(std::back_inserter(msgBuffer), fmt::runtime(fmt), 
                           std::forward<Args>(args)...);
        }
        std::string_view message(msgBuffer.data(), msgBuffer.size());

        fmt::memory_buffer outputBuffer;
        if (m_format) {
            m_format->FormatDirectToBuffer(outputBuffer, level, timestamp, 
                                           threadId, processId, message);
        } else {
            DefaultFormatDirectToBuffer(outputBuffer, level, timestamp, 
                                        threadId, processId, message);
        }

        std::string_view output(outputBuffer.data(), outputBuffer.size());
        for (auto& sink : m_sinks) {
            if (sink) {
                sink->Write(output);
            }
        }
#else
        std::string message;
        if constexpr (sizeof...(Args) == 0) {
            message = fmt;
        } else {
            BinarySnapshot snapshot;
            snapshot.CaptureStringView(std::string_view(fmt));
            snapshot.Capture(std::forward<Args>(args)...);
            message = snapshot.FormatAll();
        }

        std::string output;
        if (m_format) {
            output = m_format->FormatDirect(level, timestamp, threadId, processId, message);
        } else {
            output = DefaultFormatDirect(level, timestamp, threadId, processId, message);
        }

        for (auto& sink : m_sinks) {
            if (sink) {
                sink->Write(output);
            }
        }
#endif

        (void)loc;
    }

    template<typename... Args>
    void ProcessEntryAsync(Level level, uint64_t timestamp,
                           const SourceLocation& loc,
                           const char* fmt, Args&&... args) {
        FormatRequirements req;
        if (m_format) {
            req = m_format->GetRequirements();
        } else {
            req.needsThreadId = true;
            req.needsProcessId = true;
        }

        LogEntry entry;
        entry.timestamp = timestamp;
        entry.level = level;
        entry.threadId = req.needsThreadId ? GetCurrentThreadId() : 0;
        entry.processId = req.needsProcessId ? GetCurrentProcessId() : 0;

#ifndef NDEBUG
        if (req.needsSourceLocation) {
            entry.file = loc.file;
            entry.function = loc.function;
            entry.line = loc.line;
        }
#else
        (void)loc;
#endif

        if constexpr (sizeof...(Args) == 0) {
            entry.snapshot.CaptureStringView(std::string_view(fmt));
        } else {
            entry.snapshot.CaptureStringView(std::string_view(fmt));
            entry.snapshot.Capture(std::forward<Args>(args)...);
        }

        if (m_heapRingBuffer) {
            m_heapRingBuffer->TryPush(std::move(entry));
            m_heapRingBuffer->NotifyConsumer();
        }
    }

    std::string DefaultFormatDirect(Level level, uint64_t timestamp,
                                    uint32_t threadId, uint32_t processId,
                                    const std::string& message) const {
        static ConsoleFormat defaultFormat;
        return defaultFormat.FormatDirect(level, timestamp, threadId, processId, message);
    }

#ifdef ONEPLOG_USE_FMT
    void DefaultFormatDirectToBuffer(fmt::memory_buffer& buffer,
                                     Level level, uint64_t timestamp,
                                     uint32_t threadId, uint32_t processId,
                                     std::string_view message) const {
        static ConsoleFormat defaultFormat;
        defaultFormat.FormatDirectToBuffer(buffer, level, timestamp, 
                                           threadId, processId, message);
    }
#endif

    // =========================================================================
    // Helper Functions / 辅助函数
    // =========================================================================

    /**
     * @brief Fallback to stderr when logging fails
     * @brief 日志记录失败时降级到 stderr
     *
     * This ensures the program doesn't crash due to logging errors.
     * 这确保程序不会因日志错误而崩溃。
     */
    static void FallbackToStderr(Level level, const char* fmt, const char* error) noexcept {
        try {
            // Use fprintf for maximum safety (no exceptions)
            // 使用 fprintf 以获得最大安全性（无异常）
            std::fprintf(stderr, "[oneplog] LOGGING FAILED [%s]: format=\"%s\", error=\"%s\"\n",
                         LevelToString(level, LevelNameStyle::Short4).data(),
                         fmt ? fmt : "(null)",
                         error ? error : "(null)");
            std::fflush(stderr);
        } catch (...) {
            // Last resort: ignore all errors
            // 最后手段：忽略所有错误
        }
    }

    static uint64_t GetNanosecondTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
    }

    static uint32_t GetCurrentThreadId() {
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

    static uint32_t GetCurrentProcessId() {
#ifdef _WIN32
        return static_cast<uint32_t>(::GetCurrentProcessId());
#else
        return static_cast<uint32_t>(::getpid());
#endif
    }

    // =========================================================================
    // Member Variables / 成员变量
    // =========================================================================

    bool m_initialized;
    std::mutex m_mutex;

    std::vector<std::shared_ptr<Sink>> m_sinks;
    std::shared_ptr<Format> m_format;

    std::unique_ptr<HeapRingBuffer<LogEntry, EnableWFC>> m_heapRingBuffer;
    std::unique_ptr<SharedMemory<EnableWFC>> m_sharedMemory;
    std::unique_ptr<PipelineThread<EnableWFC>> m_pipelineThread;
    std::unique_ptr<WriterThread<EnableWFC>> m_writerThread;
};

// ==============================================================================
// Type Aliases / 类型别名
// ==============================================================================

template<Level L = kDefaultLevel, bool EnableWFC = false>
using SyncLogger = Logger<Mode::Sync, L, EnableWFC>;

template<Level L = kDefaultLevel, bool EnableWFC = false>
using AsyncLogger = Logger<Mode::Async, L, EnableWFC>;

template<Level L = kDefaultLevel, bool EnableWFC = false>
using MProcLogger = Logger<Mode::MProc, L, EnableWFC>;

using DebugLogger = Logger<Mode::Async, Level::Debug, false>;
using ReleaseLogger = Logger<Mode::Async, Level::Info, false>;
using DebugLoggerWFC = Logger<Mode::Async, Level::Debug, true>;
using ReleaseLoggerWFC = Logger<Mode::Async, Level::Info, true>;

// ==============================================================================
// Global Logger / 全局日志器
// ==============================================================================

namespace detail {

template<Mode M = Mode::Async, Level L = kDefaultLevel, bool EnableWFC = false>
inline std::shared_ptr<Logger<M, L, EnableWFC>>& GetDefaultLoggerPtr() {
    static std::shared_ptr<Logger<M, L, EnableWFC>> defaultLogger;
    return defaultLogger;
}

template<Mode M = Mode::Async, Level L = kDefaultLevel, bool EnableWFC = false>
inline std::mutex& GetDefaultLoggerMutex() {
    static std::mutex mutex;
    return mutex;
}

}  // namespace detail

template<Mode M = Mode::Async, Level L = kDefaultLevel, bool EnableWFC = false>
inline std::shared_ptr<Logger<M, L, EnableWFC>> DefaultLogger() {
    std::lock_guard<std::mutex> lock(detail::GetDefaultLoggerMutex<M, L, EnableWFC>());
    return detail::GetDefaultLoggerPtr<M, L, EnableWFC>();
}

template<Mode M = Mode::Async, Level L = kDefaultLevel, bool EnableWFC = false>
inline void SetDefaultLogger(std::shared_ptr<Logger<M, L, EnableWFC>> logger) {
    std::lock_guard<std::mutex> lock(detail::GetDefaultLoggerMutex<M, L, EnableWFC>());
    detail::GetDefaultLoggerPtr<M, L, EnableWFC>() = std::move(logger);
}

// ==============================================================================
// Default Logger Type / 默认日志器类型
// ==============================================================================

/**
 * @brief Default logger type for simplified API
 * @brief 简化 API 的默认日志器类型
 *
 * The default logger uses:
 * - Mode::Async for best performance
 * - kDefaultLevel (Debug in debug builds, Info in release)
 * - EnableWFC=false (WFC disabled by default for zero overhead)
 *
 * 默认日志器使用：
 * - Mode::Async 以获得最佳性能
 * - kDefaultLevel（调试构建为 Debug，发布构建为 Info）
 * - EnableWFC=false（默认禁用 WFC 以实现零开销）
 *
 * Users can customize by defining macros before including oneplog:
 * 用户可以在包含 oneplog 之前定义宏来自定义：
 * @code
 * #define ONEPLOG_DEFAULT_MODE oneplog::Mode::MProc
 * #define ONEPLOG_DEFAULT_LEVEL oneplog::Level::Debug
 * #define ONEPLOG_DEFAULT_ENABLE_WFC true
 * #include <oneplog/oneplog.hpp>
 * @endcode
 */

#ifndef ONEPLOG_DEFAULT_MODE
#define ONEPLOG_DEFAULT_MODE Mode::Async
#endif

#ifndef ONEPLOG_DEFAULT_LEVEL
#define ONEPLOG_DEFAULT_LEVEL kDefaultLevel
#endif

#ifndef ONEPLOG_DEFAULT_ENABLE_WFC
#define ONEPLOG_DEFAULT_ENABLE_WFC false
#endif

using DefaultLoggerType = Logger<ONEPLOG_DEFAULT_MODE, ONEPLOG_DEFAULT_LEVEL, ONEPLOG_DEFAULT_ENABLE_WFC>;

// ==============================================================================
// Global Logger Storage / 全局日志器存储
// ==============================================================================

namespace detail {

/**
 * @brief Global default logger instance (type-erased for simplified API)
 * @brief 全局默认日志器实例（类型擦除以简化 API）
 */
inline std::shared_ptr<DefaultLoggerType>& GetGlobalLoggerPtr() {
    static std::shared_ptr<DefaultLoggerType> globalLogger;
    return globalLogger;
}

inline std::mutex& GetGlobalLoggerMutex() {
    static std::mutex mutex;
    return mutex;
}

}  // namespace detail

// ==============================================================================
// Global Convenience Functions / 全局便捷函数
// ==============================================================================

// Legacy template-based functions (for backward compatibility)
// 旧版模板函数（向后兼容）
template<Mode M = Mode::Async, Level L = kDefaultLevel, bool EnableWFC = false>
inline void InitLogger(const LoggerConfig& config = LoggerConfig{}) {
    auto logger = std::make_shared<Logger<M, L, EnableWFC>>();
    logger->SetSink(std::make_shared<ConsoleSink>());
    logger->SetFormat(std::make_shared<ConsoleFormat>());
    logger->Init(config);
    SetDefaultLogger<M, L, EnableWFC>(logger);
}

template<Mode M = Mode::Async, Level L = kDefaultLevel, bool EnableWFC = false>
inline void ShutdownLogger() {
    auto logger = DefaultLogger<M, L, EnableWFC>();
    if (logger) {
        logger->Shutdown();
    }
    SetDefaultLogger<M, L, EnableWFC>(nullptr);
}

// ==============================================================================
// Simplified Public API / 简化公共 API
// ==============================================================================

/**
 * @brief Initialize the global logger (consumer mode - writes logs)
 * @brief 初始化全局日志器（消费者模式 - 写入日志）
 *
 * This is the main initialization function for single-process applications
 * or the consumer process in multi-process mode.
 *
 * 这是单进程应用程序或多进程模式中消费者进程的主要初始化函数。
 *
 * @param config Logger configuration / 日志器配置
 *
 * Usage / 用法:
 * @code
 * oneplog::Init();  // Use defaults
 * // or
 * oneplog::LoggerConfig config;
 * config.sharedMemoryName = "/myapp_log";
 * oneplog::Init(config);
 * @endcode
 */
inline void Init(const LoggerConfig& config = LoggerConfig{}) {
    std::lock_guard<std::mutex> lock(detail::GetGlobalLoggerMutex());
    
    auto logger = std::make_shared<DefaultLoggerType>();
    logger->SetSink(std::make_shared<ConsoleSink>());
    logger->SetFormat(std::make_shared<ConsoleFormat>());
    logger->Init(config);
    
    detail::GetGlobalLoggerPtr() = logger;
}

/**
 * @brief Initialize the global logger for producer mode (MProc only)
 * @brief 初始化全局日志器为生产者模式（仅限 MProc）
 *
 * In multi-process mode, producer processes only push logs to shared memory.
 * They don't need a writer thread or sink.
 *
 * 在多进程模式下，生产者进程只将日志推送到共享内存。
 * 它们不需要写入线程或 Sink。
 *
 * @param config Logger configuration (sharedMemoryName required)
 *
 * Usage / 用法:
 * @code
 * oneplog::LoggerConfig config;
 * config.sharedMemoryName = "/myapp_log";
 * oneplog::InitProducer(config);
 * @endcode
 */
inline void InitProducer(const LoggerConfig& config) {
    std::lock_guard<std::mutex> lock(detail::GetGlobalLoggerMutex());
    
    auto logger = std::make_shared<DefaultLoggerType>();
    // Producer mode: no sink needed, logs go to shared memory
    // 生产者模式：不需要 Sink，日志进入共享内存
    logger->Init(config);
    
    detail::GetGlobalLoggerPtr() = logger;
}

/**
 * @brief Shutdown the global logger
 * @brief 关闭全局日志器
 */
inline void Shutdown() {
    std::lock_guard<std::mutex> lock(detail::GetGlobalLoggerMutex());
    
    auto& logger = detail::GetGlobalLoggerPtr();
    if (logger) {
        logger->Shutdown();
        logger.reset();
    }
}

/**
 * @brief Flush the global logger
 * @brief 刷新全局日志器
 */
inline void Flush() {
    std::lock_guard<std::mutex> lock(detail::GetGlobalLoggerMutex());
    
    auto& logger = detail::GetGlobalLoggerPtr();
    if (logger) {
        logger->Flush();
    }
}

/**
 * @brief Get the global logger instance
 * @brief 获取全局日志器实例
 */
inline std::shared_ptr<DefaultLoggerType> GetLogger() {
    std::lock_guard<std::mutex> lock(detail::GetGlobalLoggerMutex());
    return detail::GetGlobalLoggerPtr();
}

/**
 * @brief Set a custom logger as the global logger
 * @brief 设置自定义日志器为全局日志器
 */
inline void SetLogger(std::shared_ptr<DefaultLoggerType> logger) {
    std::lock_guard<std::mutex> lock(detail::GetGlobalLoggerMutex());
    detail::GetGlobalLoggerPtr() = std::move(logger);
}

// ==============================================================================
// Global Logging Functions / 全局日志函数
// ==============================================================================

/**
 * @brief Log at TRACE level
 * @brief 以 TRACE 级别记录日志
 */
template<typename... Args>
inline void Trace(const char* fmt, Args&&... args) {
    auto logger = GetLogger();
    if (logger) {
        logger->Trace(fmt, std::forward<Args>(args)...);
    }
}

/**
 * @brief Log at DEBUG level
 * @brief 以 DEBUG 级别记录日志
 */
template<typename... Args>
inline void Debug(const char* fmt, Args&&... args) {
    auto logger = GetLogger();
    if (logger) {
        logger->Debug(fmt, std::forward<Args>(args)...);
    }
}

/**
 * @brief Log at INFO level
 * @brief 以 INFO 级别记录日志
 */
template<typename... Args>
inline void Info(const char* fmt, Args&&... args) {
    auto logger = GetLogger();
    if (logger) {
        logger->Info(fmt, std::forward<Args>(args)...);
    }
}

/**
 * @brief Log at WARN level
 * @brief 以 WARN 级别记录日志
 */
template<typename... Args>
inline void Warn(const char* fmt, Args&&... args) {
    auto logger = GetLogger();
    if (logger) {
        logger->Warn(fmt, std::forward<Args>(args)...);
    }
}

/**
 * @brief Log at ERROR level
 * @brief 以 ERROR 级别记录日志
 */
template<typename... Args>
inline void Error(const char* fmt, Args&&... args) {
    auto logger = GetLogger();
    if (logger) {
        logger->Error(fmt, std::forward<Args>(args)...);
    }
}

/**
 * @brief Log at CRITICAL level
 * @brief 以 CRITICAL 级别记录日志
 */
template<typename... Args>
inline void Critical(const char* fmt, Args&&... args) {
    auto logger = GetLogger();
    if (logger) {
        logger->Critical(fmt, std::forward<Args>(args)...);
    }
}

// ==============================================================================
// Global WFC Logging Functions / 全局 WFC 日志函数
// ==============================================================================

/**
 * @brief Log at TRACE level with WFC (Wait For Completion)
 * @brief 以 TRACE 级别记录 WFC 日志
 */
template<typename... Args>
inline void TraceWFC(const char* fmt, Args&&... args) {
    auto logger = GetLogger();
    if (logger) {
        logger->TraceWFC(fmt, std::forward<Args>(args)...);
    }
}

/**
 * @brief Log at DEBUG level with WFC
 * @brief 以 DEBUG 级别记录 WFC 日志
 */
template<typename... Args>
inline void DebugWFC(const char* fmt, Args&&... args) {
    auto logger = GetLogger();
    if (logger) {
        logger->DebugWFC(fmt, std::forward<Args>(args)...);
    }
}

/**
 * @brief Log at INFO level with WFC
 * @brief 以 INFO 级别记录 WFC 日志
 */
template<typename... Args>
inline void InfoWFC(const char* fmt, Args&&... args) {
    auto logger = GetLogger();
    if (logger) {
        logger->InfoWFC(fmt, std::forward<Args>(args)...);
    }
}

/**
 * @brief Log at WARN level with WFC
 * @brief 以 WARN 级别记录 WFC 日志
 */
template<typename... Args>
inline void WarnWFC(const char* fmt, Args&&... args) {
    auto logger = GetLogger();
    if (logger) {
        logger->WarnWFC(fmt, std::forward<Args>(args)...);
    }
}

/**
 * @brief Log at ERROR level with WFC
 * @brief 以 ERROR 级别记录 WFC 日志
 */
template<typename... Args>
inline void ErrorWFC(const char* fmt, Args&&... args) {
    auto logger = GetLogger();
    if (logger) {
        logger->ErrorWFC(fmt, std::forward<Args>(args)...);
    }
}

/**
 * @brief Log at CRITICAL level with WFC
 * @brief 以 CRITICAL 级别记录 WFC 日志
 */
template<typename... Args>
inline void CriticalWFC(const char* fmt, Args&&... args) {
    auto logger = GetLogger();
    if (logger) {
        logger->CriticalWFC(fmt, std::forward<Args>(args)...);
    }
}

}  // namespace oneplog

// ==============================================================================
// Static Logger Class (Legacy) / 静态日志类（旧版）
// ==============================================================================

/**
 * @brief Static logger class for convenient logging (legacy API)
 * @brief 静态日志类，提供便捷的日志记录方式（旧版 API）
 *
 * @deprecated Use oneplog::Info(), oneplog::Error() etc. instead
 * @deprecated 请使用 oneplog::Info(), oneplog::Error() 等替代
 *
 * Usage / 用法:
 * @code
 * oneplog::Init();
 * log::Info("Hello, {}!", "world");
 * log::ErrorWFC("Critical error: {}", code);
 * @endcode
 */
class log {
public:
    template<typename... Args>
    static void Trace(const char* fmt, Args&&... args) {
        oneplog::Trace(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Debug(const char* fmt, Args&&... args) {
        oneplog::Debug(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Info(const char* fmt, Args&&... args) {
        oneplog::Info(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Warn(const char* fmt, Args&&... args) {
        oneplog::Warn(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Error(const char* fmt, Args&&... args) {
        oneplog::Error(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void Critical(const char* fmt, Args&&... args) {
        oneplog::Critical(fmt, std::forward<Args>(args)...);
    }

    // WFC logging methods
    template<typename... Args>
    static void TraceWFC(const char* fmt, Args&&... args) {
        oneplog::TraceWFC(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void DebugWFC(const char* fmt, Args&&... args) {
        oneplog::DebugWFC(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void InfoWFC(const char* fmt, Args&&... args) {
        oneplog::InfoWFC(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void WarnWFC(const char* fmt, Args&&... args) {
        oneplog::WarnWFC(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void ErrorWFC(const char* fmt, Args&&... args) {
        oneplog::ErrorWFC(fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    static void CriticalWFC(const char* fmt, Args&&... args) {
        oneplog::CriticalWFC(fmt, std::forward<Args>(args)...);
    }

    static void Flush() {
        oneplog::Flush();
    }

    static void Shutdown() {
        oneplog::Shutdown();
    }
};

