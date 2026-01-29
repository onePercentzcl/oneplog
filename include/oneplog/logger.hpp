/**
 * @file logger.hpp
 * @brief Logger class for onePlog
 * @brief onePlog 日志管理器
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "oneplog/binary_snapshot.hpp"
#include "oneplog/common.hpp"
#include "oneplog/format.hpp"
#include "oneplog/heap_ring_buffer.hpp"
#include "oneplog/log_entry.hpp"
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
// Logger Configuration / Logger 配置
// ==============================================================================

/**
 * @brief Logger configuration structure
 * @brief Logger 配置结构
 */
struct LoggerConfig {
    Mode mode{Mode::Async};                      ///< Operating mode / 运行模式
    Level level{Level::Info};                    ///< Log level / 日志级别
    size_t heapRingBufferSize{8192};             ///< Heap ring buffer size / 堆环形队列大小
    size_t sharedRingBufferSize{4096};           ///< Shared ring buffer size / 共享环形队列大小
    QueueFullPolicy queueFullPolicy{QueueFullPolicy::DropNewest};  ///< Queue full policy / 队列满策略
    std::string sharedMemoryName;                ///< Shared memory name (for MProc mode) / 共享内存名称
    std::chrono::microseconds pollInterval{1};   ///< Poll interval / 轮询间隔
    std::chrono::milliseconds pollTimeout{10};   ///< Poll timeout / 轮询超时
};

// ==============================================================================
// Logger Class / Logger 类
// ==============================================================================

/**
 * @brief Main logger class
 * @brief 主日志类
 *
 * Supports three operating modes:
 * 支持三种运行模式：
 * - Sync: Direct output in calling thread / 同步：在调用线程中直接输出
 * - Async: Output via background thread / 异步：通过后台线程输出
 * - MProc: Multi-process logging via shared memory / 多进程：通过共享内存记录日志
 */
class Logger {
public:
    /**
     * @brief Construct a logger
     * @brief 构造日志器
     *
     * @param name Logger name / 日志器名称
     * @param mode Operating mode / 运行模式
     */
    explicit Logger(const std::string& name = "", Mode mode = Mode::Async)
        : m_name(name)
        , m_mode(mode)
        , m_level(Level::Info)
        , m_initialized(false) {}

    /**
     * @brief Destructor
     * @brief 析构函数
     */
    ~Logger() {
        Shutdown();
    }

    // Non-copyable
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    // =========================================================================
    // Name and Mode / 名称和模式
    // =========================================================================

    /**
     * @brief Get logger name
     * @brief 获取日志器名称
     */
    const std::string& Name() const { return m_name; }

    /**
     * @brief Get operating mode
     * @brief 获取运行模式
     */
    Mode GetMode() const { return m_mode; }

    // =========================================================================
    // Initialization / 初始化
    // =========================================================================

    /**
     * @brief Initialize the logger with default configuration
     * @brief 使用默认配置初始化日志器
     */
    void Init() {
        LoggerConfig config;
        config.mode = m_mode;
        config.level = m_level;
        Init(config);
    }

    /**
     * @brief Initialize the logger with custom configuration
     * @brief 使用自定义配置初始化日志器
     *
     * @param config Logger configuration / 日志器配置
     */
    void Init(const LoggerConfig& config) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_initialized) {
            return;  // Already initialized / 已经初始化
        }

        m_mode = config.mode;
        m_level = config.level;

        switch (m_mode) {
            case Mode::Sync:
                // Sync mode: no background threads needed
                // 同步模式：不需要后台线程
                break;

            case Mode::Async:
                // Async mode: create heap ring buffer and writer thread
                // 异步模式：创建堆环形队列和写入线程
                m_heapRingBuffer = std::make_unique<HeapRingBuffer<LogEntry>>(
                    config.heapRingBufferSize, config.queueFullPolicy);
                
                if (!m_sinks.empty()) {
                    m_writerThread = std::make_unique<WriterThread>(m_sinks[0]);
                    m_writerThread->SetHeapRingBuffer(m_heapRingBuffer.get());
                    m_writerThread->SetPollInterval(config.pollInterval);
                    m_writerThread->SetPollTimeout(config.pollTimeout);
                    if (m_format) {
                        m_writerThread->SetFormat(m_format);
                    }
                    m_writerThread->Start();
                }
                break;

            case Mode::MProc:
                // Multi-process mode: create shared memory, heap ring buffer, pipeline, and writer
                // 多进程模式：创建共享内存、堆环形队列、管道和写入器
                if (!config.sharedMemoryName.empty()) {
                    m_sharedMemory = SharedMemory::Create(
                        config.sharedMemoryName,
                        config.sharedRingBufferSize,
                        config.queueFullPolicy);
                }

                m_heapRingBuffer = std::make_unique<HeapRingBuffer<LogEntry>>(
                    config.heapRingBufferSize, config.queueFullPolicy);

                if (m_sharedMemory) {
                    m_pipelineThread = std::make_unique<PipelineThread>(
                        *m_heapRingBuffer, *m_sharedMemory);
                    m_pipelineThread->SetPollInterval(config.pollInterval);
                    m_pipelineThread->SetPollTimeout(config.pollTimeout);
                    m_pipelineThread->Start();

                    if (!m_sinks.empty()) {
                        m_writerThread = std::make_unique<WriterThread>(m_sinks[0]);
                        m_writerThread->SetSharedRingBuffer(m_sharedMemory->GetRingBuffer());
                        m_writerThread->SetPollInterval(config.pollInterval);
                        m_writerThread->SetPollTimeout(config.pollTimeout);
                        if (m_format) {
                            m_writerThread->SetFormat(m_format);
                        }
                        m_writerThread->Start();
                    }
                }
                break;
        }

        m_initialized = true;
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
     * @brief Set log level
     * @brief 设置日志级别
     */
    void SetLevel(Level level) {
        m_level.store(level, std::memory_order_release);
    }

    /**
     * @brief Get log level
     * @brief 获取日志级别
     */
    Level GetLevel() const {
        return m_level.load(std::memory_order_acquire);
    }

    /**
     * @brief Check if a level should be logged
     * @brief 检查是否应该记录某个级别
     */
    bool ShouldLog(Level level) const {
        return static_cast<uint8_t>(level) >= static_cast<uint8_t>(GetLevel());
    }

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
     * @brief Log a message at the specified level
     * @brief 以指定级别记录消息
     */
    template<typename... Args>
    void Log(Level level, const char* fmt, Args&&... args) {
        if (!ShouldLog(level)) {
            return;
        }
        LogImpl(level, ONEPLOG_CURRENT_LOCATION, fmt, std::forward<Args>(args)...);
    }

    /**
     * @brief Log a message with source location
     * @brief 使用源位置记录消息
     */
    template<typename... Args>
    void Log(Level level, const SourceLocation& loc, const char* fmt, Args&&... args) {
        if (!ShouldLog(level)) {
            return;
        }
        LogImpl(level, loc, fmt, std::forward<Args>(args)...);
    }

    // Convenience methods / 便捷方法
    template<typename... Args>
    void Trace(const char* fmt, Args&&... args) {
        Log(Level::Trace, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Debug(const char* fmt, Args&&... args) {
        Log(Level::Debug, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Info(const char* fmt, Args&&... args) {
        Log(Level::Info, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Warn(const char* fmt, Args&&... args) {
        Log(Level::Warn, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Error(const char* fmt, Args&&... args) {
        Log(Level::Error, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void Critical(const char* fmt, Args&&... args) {
        Log(Level::Critical, fmt, std::forward<Args>(args)...);
    }

    // =========================================================================
    // WFC Logging Methods / WFC 日志记录方法
    // =========================================================================

    template<typename... Args>
    void TraceWFC(const char* fmt, Args&&... args) {
        LogWFC(Level::Trace, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void DebugWFC(const char* fmt, Args&&... args) {
        LogWFC(Level::Debug, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void InfoWFC(const char* fmt, Args&&... args) {
        LogWFC(Level::Info, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void WarnWFC(const char* fmt, Args&&... args) {
        LogWFC(Level::Warn, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void ErrorWFC(const char* fmt, Args&&... args) {
        LogWFC(Level::Error, fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void CriticalWFC(const char* fmt, Args&&... args) {
        LogWFC(Level::Critical, fmt, std::forward<Args>(args)...);
    }

    // =========================================================================
    // Lifecycle / 生命周期
    // =========================================================================

    /**
     * @brief Flush all pending log entries
     * @brief 刷新所有待处理的日志条目
     */
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

    /**
     * @brief Shutdown the logger
     * @brief 关闭日志器
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

        m_initialized = false;
    }

private:
    /**
     * @brief Internal log implementation
     * @brief 内部日志实现
     */
    template<typename... Args>
    void LogImpl(Level level, const SourceLocation& loc, const char* fmt, Args&&... args) {
        // Create log entry / 创建日志条目
        LogEntry entry;
        entry.timestamp = GetNanosecondTimestamp();
        entry.level = level;
        entry.threadId = GetCurrentThreadId();
        entry.processId = GetCurrentProcessId();

#ifndef NDEBUG
        entry.file = loc.file;
        entry.function = loc.function;
        entry.line = loc.line;
#else
        (void)loc;  // Suppress unused warning in release mode
#endif

        // Capture format string and arguments / 捕获格式字符串和参数
        // Format string is captured as the first argument
        // 格式字符串作为第一个参数捕获
        if constexpr (sizeof...(Args) == 0) {
            // No arguments, just capture the format string as message
            // 没有参数，只捕获格式字符串作为消息
            entry.snapshot.CaptureStringView(std::string_view(fmt));
        } else {
            // Capture format string followed by arguments
            // 捕获格式字符串，然后是参数
            entry.snapshot.CaptureStringView(std::string_view(fmt));
            entry.snapshot.Capture(std::forward<Args>(args)...);
        }

        // Process based on mode / 根据模式处理
        switch (m_mode) {
            case Mode::Sync:
                ProcessEntrySync(entry);
                break;

            case Mode::Async:
            case Mode::MProc:
                if (m_heapRingBuffer) {
                    m_heapRingBuffer->TryPush(std::move(entry));
                    m_heapRingBuffer->NotifyConsumer();
                }
                break;
        }
    }

    /**
     * @brief WFC log implementation
     * @brief WFC 日志实现
     */
    template<typename... Args>
    void LogWFC(Level level, const char* fmt, Args&&... args) {
        if (!ShouldLog(level)) {
            return;
        }

        // For sync mode, WFC is the same as normal log
        // 对于同步模式，WFC 与普通日志相同
        if (m_mode == Mode::Sync) {
            LogImpl(level, ONEPLOG_CURRENT_LOCATION, fmt, std::forward<Args>(args)...);
            return;
        }

        // Create log entry / 创建日志条目
        LogEntry entry;
        entry.timestamp = GetNanosecondTimestamp();
        entry.level = level;
        entry.threadId = GetCurrentThreadId();
        entry.processId = GetCurrentProcessId();

#ifndef NDEBUG
        auto loc = ONEPLOG_CURRENT_LOCATION;
        entry.file = loc.file;
        entry.function = loc.function;
        entry.line = loc.line;
#endif

        // Capture arguments / 捕获参数
        entry.snapshot.Capture(std::forward<Args>(args)...);

        // Push with WFC and wait / 使用 WFC 推送并等待
        if (m_heapRingBuffer) {
            m_heapRingBuffer->TryPushWFC(std::move(entry));
        }
    }

    /**
     * @brief Process entry in sync mode
     * @brief 在同步模式下处理条目
     */
    void ProcessEntrySync(const LogEntry& entry) {
        std::string message;
        
        if (m_format) {
            message = m_format->FormatEntry(entry);
        } else {
            message = DefaultFormat(entry);
        }

        for (auto& sink : m_sinks) {
            if (sink) {
                sink->Write(message);
            }
        }
    }

    /**
     * @brief Default format for log entries (uses ConsoleFormat)
     * @brief 日志条目的默认格式（使用 ConsoleFormat）
     */
    std::string DefaultFormat(const LogEntry& entry) const {
        // Use a static ConsoleFormat for default formatting
        // 使用静态 ConsoleFormat 进行默认格式化
        static ConsoleFormat defaultFormat;
        return defaultFormat.FormatEntry(entry);
    }

    /**
     * @brief Get nanosecond timestamp
     * @brief 获取纳秒级时间戳
     */
    static uint64_t GetNanosecondTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto duration = now.time_since_epoch();
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count());
    }

    /**
     * @brief Get current thread ID
     * @brief 获取当前线程 ID
     */
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

    /**
     * @brief Get current process ID
     * @brief 获取当前进程 ID
     */
    static uint32_t GetCurrentProcessId() {
#ifdef _WIN32
        return static_cast<uint32_t>(::GetCurrentProcessId());
#else
        return static_cast<uint32_t>(::getpid());
#endif
    }

    std::string m_name;                                    ///< Logger name / 日志器名称
    Mode m_mode;                                           ///< Operating mode / 运行模式
    std::atomic<Level> m_level;                            ///< Log level / 日志级别
    bool m_initialized;                                    ///< Initialization flag / 初始化标志
    std::mutex m_mutex;                                    ///< Mutex for thread safety / 线程安全互斥锁

    std::vector<std::shared_ptr<Sink>> m_sinks;            ///< Output sinks / 输出 Sink
    std::shared_ptr<Format> m_format;                      ///< Formatter / 格式化器

    std::unique_ptr<HeapRingBuffer<LogEntry>> m_heapRingBuffer;  ///< Heap ring buffer / 堆环形队列
    std::unique_ptr<SharedMemory> m_sharedMemory;          ///< Shared memory / 共享内存
    std::unique_ptr<PipelineThread> m_pipelineThread;      ///< Pipeline thread / 管道线程
    std::unique_ptr<WriterThread> m_writerThread;          ///< Writer thread / 写入线程
};

// ==============================================================================
// Global Logger / 全局日志器
// ==============================================================================

namespace detail {

/**
 * @brief Get the default logger instance
 * @brief 获取默认日志器实例
 */
inline std::shared_ptr<Logger>& GetDefaultLoggerPtr() {
    static std::shared_ptr<Logger> defaultLogger;
    return defaultLogger;
}

inline std::mutex& GetDefaultLoggerMutex() {
    static std::mutex mutex;
    return mutex;
}

}  // namespace detail

/**
 * @brief Get the default logger
 * @brief 获取默认日志器
 */
inline std::shared_ptr<Logger> DefaultLogger() {
    std::lock_guard<std::mutex> lock(detail::GetDefaultLoggerMutex());
    return detail::GetDefaultLoggerPtr();
}

/**
 * @brief Set the default logger
 * @brief 设置默认日志器
 */
inline void SetDefaultLogger(std::shared_ptr<Logger> logger) {
    std::lock_guard<std::mutex> lock(detail::GetDefaultLoggerMutex());
    detail::GetDefaultLoggerPtr() = std::move(logger);
}

// ==============================================================================
// Global Convenience Functions / 全局便捷函数
// ==============================================================================

/**
 * @brief Initialize the default logger
 * @brief 初始化默认日志器
 */
inline void Init(const LoggerConfig& config = LoggerConfig{}) {
    auto logger = std::make_shared<Logger>("default", config.mode);
    logger->SetLevel(config.level);
    logger->Init(config);
    SetDefaultLogger(logger);
}

/**
 * @brief Initialize producer for multi-process mode (child process)
 * @brief 初始化多进程模式的生产者（子进程）
 */
inline void InitProducer(const std::string& shmName) {
    // Connect to existing shared memory / 连接到已存在的共享内存
    auto sharedMemory = SharedMemory::Connect(shmName);
    if (!sharedMemory) {
        return;
    }
    // Producer initialization would go here
    // 生产者初始化代码在这里
}

// Global logging functions / 全局日志函数
template<typename... Args>
void Trace(const char* fmt, Args&&... args) {
    auto logger = DefaultLogger();
    if (logger) {
        logger->Trace(fmt, std::forward<Args>(args)...);
    }
}

template<typename... Args>
void Debug(const char* fmt, Args&&... args) {
    auto logger = DefaultLogger();
    if (logger) {
        logger->Debug(fmt, std::forward<Args>(args)...);
    }
}

template<typename... Args>
void Info(const char* fmt, Args&&... args) {
    auto logger = DefaultLogger();
    if (logger) {
        logger->Info(fmt, std::forward<Args>(args)...);
    }
}

template<typename... Args>
void Warn(const char* fmt, Args&&... args) {
    auto logger = DefaultLogger();
    if (logger) {
        logger->Warn(fmt, std::forward<Args>(args)...);
    }
}

template<typename... Args>
void Error(const char* fmt, Args&&... args) {
    auto logger = DefaultLogger();
    if (logger) {
        logger->Error(fmt, std::forward<Args>(args)...);
    }
}

template<typename... Args>
void Critical(const char* fmt, Args&&... args) {
    auto logger = DefaultLogger();
    if (logger) {
        logger->Critical(fmt, std::forward<Args>(args)...);
    }
}

// Global WFC logging functions / 全局 WFC 日志函数
template<typename... Args>
void TraceWFC(const char* fmt, Args&&... args) {
    auto logger = DefaultLogger();
    if (logger) {
        logger->TraceWFC(fmt, std::forward<Args>(args)...);
    }
}

template<typename... Args>
void DebugWFC(const char* fmt, Args&&... args) {
    auto logger = DefaultLogger();
    if (logger) {
        logger->DebugWFC(fmt, std::forward<Args>(args)...);
    }
}

template<typename... Args>
void InfoWFC(const char* fmt, Args&&... args) {
    auto logger = DefaultLogger();
    if (logger) {
        logger->InfoWFC(fmt, std::forward<Args>(args)...);
    }
}

template<typename... Args>
void WarnWFC(const char* fmt, Args&&... args) {
    auto logger = DefaultLogger();
    if (logger) {
        logger->WarnWFC(fmt, std::forward<Args>(args)...);
    }
}

template<typename... Args>
void ErrorWFC(const char* fmt, Args&&... args) {
    auto logger = DefaultLogger();
    if (logger) {
        logger->ErrorWFC(fmt, std::forward<Args>(args)...);
    }
}

template<typename... Args>
void CriticalWFC(const char* fmt, Args&&... args) {
    auto logger = DefaultLogger();
    if (logger) {
        logger->CriticalWFC(fmt, std::forward<Args>(args)...);
    }
}

// Global utility functions / 全局工具函数
inline void SetLevel(Level level) {
    auto logger = DefaultLogger();
    if (logger) {
        logger->SetLevel(level);
    }
}

inline void Flush() {
    auto logger = DefaultLogger();
    if (logger) {
        logger->Flush();
    }
}

inline void Shutdown() {
    auto logger = DefaultLogger();
    if (logger) {
        logger->Shutdown();
    }
    SetDefaultLogger(nullptr);
}

// ==============================================================================
// Global Name Settings / 全局名称设置
// ==============================================================================

namespace detail {

inline std::string& GetProcessNameRef() {
    static std::string processName;
    return processName;
}

inline std::string& GetModuleNameRef() {
    static std::string moduleName;
    return moduleName;
}

}  // namespace detail

/**
 * @brief Set the process name for logging
 * @brief 设置日志的进程名称
 *
 * @param name Process name / 进程名称
 */
inline void SetProcessName(const std::string& name) {
    detail::GetProcessNameRef() = name;
}

/**
 * @brief Get the process name
 * @brief 获取进程名称
 */
inline const std::string& GetProcessName() {
    return detail::GetProcessNameRef();
}

/**
 * @brief Set the module name for logging
 * @brief 设置日志的模块名称
 *
 * @param name Module name / 模块名称
 */
inline void SetModuleName(const std::string& name) {
    detail::GetModuleNameRef() = name;
}

/**
 * @brief Get the module name
 * @brief 获取模块名称
 */
inline const std::string& GetModuleName() {
    return detail::GetModuleNameRef();
}

}  // namespace oneplog
