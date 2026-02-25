/**
 * @file logger.hpp
 * @brief High-performance template-based Logger with compile-time configuration
 * @brief 高性能模板化日志器，支持编译期配置
 *
 * This is the main Logger implementation that uses LoggerConfig for all
 * compile-time configuration. It supports:
 * - Multi-sink binding via SinkBindingList
 * - Three operating modes: Sync, Async, MProc
 * - Compile-time level filtering
 * - WFC (Wait For Completion) support
 * - Proper resource management and move semantics
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

#include "oneplog/common.hpp"
#include "oneplog/internal/logger_config.hpp"
#include "oneplog/internal/log_entry.hpp"
#include "oneplog/internal/heap_memory.hpp"
#include "oneplog/internal/shared_memory.hpp"
#include "oneplog/internal/pipeline_thread.hpp"
#include "oneplog/internal/static_formats.hpp"
#include <fmt/format.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

namespace oneplog {

// ==============================================================================
// Forward Declarations / 前向声明
// ==============================================================================

// Forward declare default configs (defined later)
struct DefaultSyncConfig;
struct DefaultAsyncConfig;
struct DefaultMProcConfig;
struct HighPerformanceConfig;

// ==============================================================================
// Runtime Configuration / 运行时配置
// ==============================================================================

/**
 * @brief Runtime configuration for Logger
 * @brief Logger 的运行时配置
 *
 * Contains configuration options that can be set at runtime.
 * 包含可在运行时设置的配置选项。
 *
 * _Requirements: 13.3_
 */
struct RuntimeConfig {
    /// Process name for log identification / 用于日志标识的进程名称
    std::string processName;
    
    /// Poll interval for async mode / 异步模式的轮询间隔
    std::chrono::microseconds pollInterval{1};
    
    /// Enable colored output / 启用彩色输出
    bool colorEnabled{true};
    
    /// Enable dynamic name resolution (reserved for future use) / 启用动态名称解析（保留字段，暂未使用）
    bool dynamicNameResolution{true};
};

/**
 * @brief File sink configuration
 * @brief 文件 Sink 配置
 *
 * Configuration for file-based sinks with rotation support.
 * 支持轮转的文件 Sink 配置。
 *
 * _Requirements: 13.4_
 */
struct FileSinkConfig {
    /// File path / 文件路径
    std::string filename;
    
    /// Maximum file size before rotation (0 = no limit) / 轮转前的最大文件大小（0 = 无限制）
    size_t maxSize{0};
    
    /// Maximum number of rotated files (0 = no rotation) / 轮转文件的最大数量（0 = 不轮转）
    size_t maxFiles{0};
    
    /// Rotate on open / 打开时轮转
    bool rotateOnOpen{false};
};

// ==============================================================================
// Helper Functions / 辅助函数
// ==============================================================================

namespace internal {

/**
 * @brief Get current nanosecond timestamp
 * @brief 获取当前纳秒时间戳
 */
inline uint64_t GetNanosecondTimestamp() noexcept {
    auto now = std::chrono::system_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());
}

/**
 * @brief Get current thread ID
 * @brief 获取当前线程 ID
 */
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

/**
 * @brief Get current process ID
 * @brief 获取当前进程 ID
 */
inline uint32_t GetCurrentProcessId() noexcept {
#ifdef _WIN32
    return static_cast<uint32_t>(::GetCurrentProcessId());
#else
    return static_cast<uint32_t>(::getpid());
#endif
}

}  // namespace internal

// ==============================================================================
// Logger Class / 日志器类
// ==============================================================================

/**
 * @brief High-performance template-based logger with compile-time configuration
 * @brief 使用编译期配置的高性能模板化日志器
 *
 * Logger is the main logging class that provides:
 * - Compile-time type resolution for zero virtual call overhead
 * - Multi-sink support via SinkBindingList
 * - Three operating modes: Sync, Async, MProc
 * - Compile-time log level filtering
 * - WFC (Wait For Completion) support
 *
 * Logger 是主要的日志类，提供：
 * - 编译期类型解析，实现零虚函数调用开销
 * - 通过 SinkBindingList 支持多 Sink
 * - 三种运行模式：Sync、Async、MProc
 * - 编译期日志级别过滤
 * - WFC（等待完成）支持
 *
 * @tparam Config The compile-time configuration type (LoggerConfig)
 *
 * _Requirements: 2.1, 2.2_
 */
template<typename Config = LoggerConfig<>>
class LoggerImpl {
public:
    // =========================================================================
    // Type Aliases and Compile-time Constants / 类型别名和编译期常量
    // =========================================================================
    
    /// The configuration type / 配置类型
    using ConfigType = Config;
    
    /// The SinkBindings type from config / 来自配置的 SinkBindings 类型
    using SinkBindings = typename Config::SinkBindings;
    
    /// Operating mode / 运行模式
    static constexpr Mode kMode = Config::kMode;
    
    /// Minimum log level / 最小日志级别
    static constexpr Level kMinLevel = Config::kLevel;
    
    /// WFC enabled flag / WFC 启用标志
    static constexpr bool kEnableWFC = Config::kEnableWFC;
    
    /// Shadow tail optimization enabled / 影子尾指针优化启用
    static constexpr bool kEnableShadowTail = Config::kEnableShadowTail;
    
    /// Use fmt library for formatting / 使用 fmt 库格式化
    static constexpr bool kUseFmt = Config::kUseFmt;
    
    /// Heap ring buffer capacity / 堆环形队列容量
    static constexpr size_t kHeapRingBufferCapacity = Config::kHeapRingBufferCapacity;
    
    /// Queue full policy / 队列满策略
    static constexpr QueueFullPolicy kQueueFullPolicy = Config::kQueueFullPolicy;
    
    // =========================================================================
    // Metadata Requirements from SinkBindings / 来自 SinkBindings 的元数据需求
    // =========================================================================
    
    /// Whether timestamp is needed by any format / 是否有任何格式需要时间戳
    static constexpr bool kNeedsTimestamp = SinkBindings::kNeedsTimestamp;
    
    /// Whether level is needed by any format / 是否有任何格式需要级别
    static constexpr bool kNeedsLevel = SinkBindings::kNeedsLevel;
    
    /// Whether thread ID is needed by any format / 是否有任何格式需要线程 ID
    static constexpr bool kNeedsThreadId = SinkBindings::kNeedsThreadId;
    
    /// Whether process ID is needed by any format / 是否有任何格式需要进程 ID
    static constexpr bool kNeedsProcessId = SinkBindings::kNeedsProcessId;
    
    /// Whether source location is needed by any format / 是否有任何格式需要源位置
    static constexpr bool kNeedsSourceLocation = SinkBindings::kNeedsSourceLocation;

    // =========================================================================
    // Constructors / 构造函数
    // =========================================================================
    
    /**
     * @brief Default constructor
     * @brief 默认构造函数
     *
     * Creates a FastLogger with default-constructed SinkBindings and
     * default RuntimeConfig.
     *
     * 使用默认构造的 SinkBindings 和默认 RuntimeConfig 创建 FastLogger。
     *
     * _Requirements: 11.1, 13.1_
     */
    LoggerImpl() : m_sinkBindings{}, m_runtimeConfig{} {
        Initialize();
    }
    
    /**
     * @brief Construct with RuntimeConfig
     * @brief 使用 RuntimeConfig 构造
     *
     * @param config Runtime configuration / 运行时配置
     *
     * _Requirements: 11.1, 13.1_
     */
    explicit LoggerImpl(const RuntimeConfig& config)
        : m_sinkBindings{}
        , m_runtimeConfig(config) {
        Initialize();
    }
    
    /**
     * @brief Construct with SinkBindings instance
     * @brief 使用 SinkBindings 实例构造
     *
     * @param bindings Pre-configured SinkBindings / 预配置的 SinkBindings
     *
     * _Requirements: 11.1, 13.2_
     */
    explicit LoggerImpl(SinkBindings bindings)
        : m_sinkBindings(std::move(bindings))
        , m_runtimeConfig{} {
        Initialize();
    }
    
    /**
     * @brief Construct with SinkBindings and RuntimeConfig
     * @brief 使用 SinkBindings 和 RuntimeConfig 构造
     *
     * @param bindings Pre-configured SinkBindings / 预配置的 SinkBindings
     * @param config Runtime configuration / 运行时配置
     *
     * _Requirements: 11.1, 13.1, 13.2_
     */
    LoggerImpl(SinkBindings bindings, const RuntimeConfig& config)
        : m_sinkBindings(std::move(bindings))
        , m_runtimeConfig(config) {
        Initialize();
    }

    /**
     * @brief Destructor
     * @brief 析构函数
     *
     * Ensures proper resource cleanup order:
     * 1. Stop worker thread (if running)
     * 2. Drain remaining log entries
     * 3. Close all sinks
     *
     * 确保正确的资源清理顺序：
     * 1. 停止工作线程（如果正在运行）
     * 2. 排空剩余日志条目
     * 3. 关闭所有 Sink
     *
     * _Requirements: 11.2, 13.1, 13.2_
     */
    ~LoggerImpl() {
        // Must stop worker before any member is destroyed
        if constexpr (kMode == Mode::MProc) {
            StopMProcWorker();
        } else if constexpr (kMode == Mode::Async) {
            StopWorker();
        }
        
        // Close all sinks
        m_sinkBindings.CloseAll();
        
        // Note: m_ringBuffer, m_sharedMemory, m_pipelineThread will be destroyed after this
    }
    
    // =========================================================================
    // Move Semantics / 移动语义
    // =========================================================================
    
    /// Deleted copy constructor / 删除的拷贝构造函数
    LoggerImpl(const LoggerImpl&) = delete;
    
    /// Deleted copy assignment / 删除的拷贝赋值
    LoggerImpl& operator=(const LoggerImpl&) = delete;
    
    /**
     * @brief Move constructor
     * @brief 移动构造函数
     *
     * Transfers ownership of all resources from the source logger.
     * The source logger is left in a valid but unspecified state.
     *
     * 从源日志器转移所有资源的所有权。
     * 源日志器处于有效但未指定的状态。
     *
     * @param other Source logger to move from / 要移动的源日志器
     *
     * _Requirements: 11.3_
     */
    LoggerImpl(LoggerImpl&& other) noexcept
        : m_sinkBindings(std::move(other.m_sinkBindings))
        , m_runtimeConfig(std::move(other.m_runtimeConfig))
        , m_ringBuffer(nullptr)
        , m_sharedMemory(nullptr)
        , m_pipelineThread(nullptr)
        , m_running{false}
        , m_isMProcOwner{false} {
        if constexpr (kMode == Mode::MProc) {
            // Stop the other's MProc workers first
            other.StopMProcWorker();
            
            // Transfer ownership
            m_ringBuffer = std::move(other.m_ringBuffer);
            m_sharedMemory = std::move(other.m_sharedMemory);
            m_pipelineThread = std::move(other.m_pipelineThread);
            m_isMProcOwner = other.m_isMProcOwner;
            other.m_isMProcOwner = false;
            
            // Restart workers if we have shared memory
            if (m_sharedMemory) {
                if (m_isMProcOwner && m_pipelineThread) {
                    m_pipelineThread->Start();
                }
                StartMProcWorker();
            }
        } else if constexpr (kMode == Mode::Async) {
            // Stop the other's worker first
            other.StopWorker();
            
            // Transfer ring buffer ownership
            m_ringBuffer = std::move(other.m_ringBuffer);
            
            // Start our own worker
            if (m_ringBuffer) {
                StartWorker();
            }
        }
    }
    
    /**
     * @brief Move assignment operator
     * @brief 移动赋值运算符
     *
     * Transfers ownership of all resources from the source logger.
     * The current logger's resources are properly cleaned up first.
     *
     * 从源日志器转移所有资源的所有权。
     * 首先正确清理当前日志器的资源。
     *
     * @param other Source logger to move from / 要移动的源日志器
     * @return Reference to this logger / 对此日志器的引用
     *
     * _Requirements: 11.4_
     */
    LoggerImpl& operator=(LoggerImpl&& other) noexcept {
        if (this != &other) {
            // Stop our workers and clean up
            if constexpr (kMode == Mode::MProc) {
                StopMProcWorker();
            } else if constexpr (kMode == Mode::Async) {
                StopWorker();
            }
            m_sinkBindings.CloseAll();
            
            // Move resources
            m_sinkBindings = std::move(other.m_sinkBindings);
            m_runtimeConfig = std::move(other.m_runtimeConfig);
            
            if constexpr (kMode == Mode::MProc) {
                // Stop the other's MProc workers
                other.StopMProcWorker();
                
                // Transfer ownership
                m_ringBuffer = std::move(other.m_ringBuffer);
                m_sharedMemory = std::move(other.m_sharedMemory);
                m_pipelineThread = std::move(other.m_pipelineThread);
                m_isMProcOwner = other.m_isMProcOwner;
                other.m_isMProcOwner = false;
                
                // Restart workers if we have shared memory
                if (m_sharedMemory) {
                    if (m_isMProcOwner && m_pipelineThread) {
                        m_pipelineThread->Start();
                    }
                    StartMProcWorker();
                }
            } else if constexpr (kMode == Mode::Async) {
                // Stop the other's worker
                other.StopWorker();
                
                // Transfer ring buffer
                m_ringBuffer = std::move(other.m_ringBuffer);
                
                // Start our worker
                if (m_ringBuffer) {
                    StartWorker();
                }
            }
        }
        return *this;
    }

    // =========================================================================
    // Logging Methods / 日志记录方法
    // =========================================================================
    
    /**
     * @brief Log a trace message
     * @brief 记录跟踪消息
     *
     * @tparam Args Argument types / 参数类型
     * @param fmt Format string / 格式字符串
     * @param args Format arguments / 格式化参数
     *
     * _Requirements: 1.2, 2.4, 14.1_
     */
    template<typename... Args>
    void Trace(const char* fmt, Args&&... args) noexcept {
        if constexpr (static_cast<uint8_t>(Level::Trace) >= static_cast<uint8_t>(kMinLevel)) {
            LogImpl<Level::Trace>(fmt, std::forward<Args>(args)...);
        }
    }
    
    /**
     * @brief Log a debug message
     * @brief 记录调试消息
     *
     * @tparam Args Argument types / 参数类型
     * @param fmt Format string / 格式字符串
     * @param args Format arguments / 格式化参数
     *
     * _Requirements: 1.2, 2.4, 14.1_
     */
    template<typename... Args>
    void Debug(const char* fmt, Args&&... args) noexcept {
        if constexpr (static_cast<uint8_t>(Level::Debug) >= static_cast<uint8_t>(kMinLevel)) {
            LogImpl<Level::Debug>(fmt, std::forward<Args>(args)...);
        }
    }
    
    /**
     * @brief Log an info message
     * @brief 记录信息消息
     *
     * @tparam Args Argument types / 参数类型
     * @param fmt Format string / 格式字符串
     * @param args Format arguments / 格式化参数
     *
     * _Requirements: 1.2, 2.4, 14.1_
     */
    template<typename... Args>
    void Info(const char* fmt, Args&&... args) noexcept {
        if constexpr (static_cast<uint8_t>(Level::Info) >= static_cast<uint8_t>(kMinLevel)) {
            LogImpl<Level::Info>(fmt, std::forward<Args>(args)...);
        }
    }
    
    /**
     * @brief Log a warning message
     * @brief 记录警告消息
     *
     * @tparam Args Argument types / 参数类型
     * @param fmt Format string / 格式字符串
     * @param args Format arguments / 格式化参数
     *
     * _Requirements: 1.2, 2.4, 14.1_
     */
    template<typename... Args>
    void Warn(const char* fmt, Args&&... args) noexcept {
        if constexpr (static_cast<uint8_t>(Level::Warn) >= static_cast<uint8_t>(kMinLevel)) {
            LogImpl<Level::Warn>(fmt, std::forward<Args>(args)...);
        }
    }
    
    /**
     * @brief Log an error message
     * @brief 记录错误消息
     *
     * @tparam Args Argument types / 参数类型
     * @param fmt Format string / 格式字符串
     * @param args Format arguments / 格式化参数
     *
     * _Requirements: 1.2, 2.4, 14.1_
     */
    template<typename... Args>
    void Error(const char* fmt, Args&&... args) noexcept {
        if constexpr (static_cast<uint8_t>(Level::Error) >= static_cast<uint8_t>(kMinLevel)) {
            LogImpl<Level::Error>(fmt, std::forward<Args>(args)...);
        }
    }
    
    /**
     * @brief Log a critical message
     * @brief 记录严重错误消息
     *
     * @tparam Args Argument types / 参数类型
     * @param fmt Format string / 格式字符串
     * @param args Format arguments / 格式化参数
     *
     * _Requirements: 1.2, 2.4, 14.1_
     */
    template<typename... Args>
    void Critical(const char* fmt, Args&&... args) noexcept {
        if constexpr (static_cast<uint8_t>(Level::Critical) >= static_cast<uint8_t>(kMinLevel)) {
            LogImpl<Level::Critical>(fmt, std::forward<Args>(args)...);
        }
    }

    // =========================================================================
    // WFC Logging Methods / WFC 日志记录方法
    // =========================================================================
    
    /**
     * @brief Log a trace message with WFC (Wait For Completion)
     * @brief 使用 WFC（等待完成）记录跟踪消息
     *
     * Only available when EnableWFC is true in the configuration.
     * 仅当配置中 EnableWFC 为 true 时可用。
     *
     * @tparam Args Argument types / 参数类型
     * @param fmt Format string / 格式字符串
     * @param args Format arguments / 格式化参数
     *
     * _Requirements: 1.3, 14.2_
     */
    template<typename... Args>
    void TraceWFC(const char* fmt, Args&&... args) noexcept {
        static_assert(kEnableWFC, "WFC methods require EnableWFC=true in config");
        if constexpr (static_cast<uint8_t>(Level::Trace) >= static_cast<uint8_t>(kMinLevel)) {
            LogImplWFC<Level::Trace>(fmt, std::forward<Args>(args)...);
        }
    }
    
    /**
     * @brief Log a debug message with WFC
     * @brief 使用 WFC 记录调试消息
     *
     * _Requirements: 1.3, 14.2_
     */
    template<typename... Args>
    void DebugWFC(const char* fmt, Args&&... args) noexcept {
        static_assert(kEnableWFC, "WFC methods require EnableWFC=true in config");
        if constexpr (static_cast<uint8_t>(Level::Debug) >= static_cast<uint8_t>(kMinLevel)) {
            LogImplWFC<Level::Debug>(fmt, std::forward<Args>(args)...);
        }
    }
    
    /**
     * @brief Log an info message with WFC
     * @brief 使用 WFC 记录信息消息
     *
     * _Requirements: 1.3, 14.2_
     */
    template<typename... Args>
    void InfoWFC(const char* fmt, Args&&... args) noexcept {
        static_assert(kEnableWFC, "WFC methods require EnableWFC=true in config");
        if constexpr (static_cast<uint8_t>(Level::Info) >= static_cast<uint8_t>(kMinLevel)) {
            LogImplWFC<Level::Info>(fmt, std::forward<Args>(args)...);
        }
    }
    
    /**
     * @brief Log a warning message with WFC
     * @brief 使用 WFC 记录警告消息
     *
     * _Requirements: 1.3, 14.2_
     */
    template<typename... Args>
    void WarnWFC(const char* fmt, Args&&... args) noexcept {
        static_assert(kEnableWFC, "WFC methods require EnableWFC=true in config");
        if constexpr (static_cast<uint8_t>(Level::Warn) >= static_cast<uint8_t>(kMinLevel)) {
            LogImplWFC<Level::Warn>(fmt, std::forward<Args>(args)...);
        }
    }
    
    /**
     * @brief Log an error message with WFC
     * @brief 使用 WFC 记录错误消息
     *
     * _Requirements: 1.3, 14.2_
     */
    template<typename... Args>
    void ErrorWFC(const char* fmt, Args&&... args) noexcept {
        static_assert(kEnableWFC, "WFC methods require EnableWFC=true in config");
        if constexpr (static_cast<uint8_t>(Level::Error) >= static_cast<uint8_t>(kMinLevel)) {
            LogImplWFC<Level::Error>(fmt, std::forward<Args>(args)...);
        }
    }
    
    /**
     * @brief Log a critical message with WFC
     * @brief 使用 WFC 记录严重错误消息
     *
     * _Requirements: 1.3, 14.2_
     */
    template<typename... Args>
    void CriticalWFC(const char* fmt, Args&&... args) noexcept {
        static_assert(kEnableWFC, "WFC methods require EnableWFC=true in config");
        if constexpr (static_cast<uint8_t>(Level::Critical) >= static_cast<uint8_t>(kMinLevel)) {
            LogImplWFC<Level::Critical>(fmt, std::forward<Args>(args)...);
        }
    }

    // =========================================================================
    // Control Methods / 控制方法
    // =========================================================================
    
    /**
     * @brief Flush all pending log entries
     * @brief 刷新所有待处理的日志条目
     *
     * In async mode, waits for the ring buffer to drain before flushing sinks.
     * In MProc mode, waits for both HeapRingBuffer and SharedRingBuffer to drain.
     * 在异步模式下，等待环形队列排空后再刷新 Sink。
     * 在多进程模式下，等待 HeapRingBuffer 和 SharedRingBuffer 都排空。
     *
     * _Requirements: 1.4, 14.3_
     */
    void Flush() noexcept {
        if constexpr (kMode == Mode::MProc) {
            // Wait for HeapRingBuffer to drain (producer side)
            if (m_ringBuffer) {
                while (!m_ringBuffer->IsEmpty()) {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
            // Wait for SharedRingBuffer to drain
            if (m_sharedMemory) {
                auto* sharedRingBuffer = m_sharedMemory->GetRingBuffer();
                if (sharedRingBuffer) {
                    while (!sharedRingBuffer->IsEmpty()) {
                        std::this_thread::sleep_for(std::chrono::microseconds(100));
                    }
                }
            }
        } else if constexpr (kMode == Mode::Async) {
            // Wait for ring buffer to drain
            if (m_ringBuffer) {
                while (!m_ringBuffer->IsEmpty()) {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
        }
        m_sinkBindings.FlushAll();
    }
    
    /**
     * @brief Shutdown the logger
     * @brief 关闭日志器
     *
     * Stops the worker thread (if running) and closes all sinks.
     * 停止工作线程（如果正在运行）并关闭所有 Sink。
     *
     * _Requirements: 1.4, 14.3_
     */
    void Shutdown() noexcept {
        if constexpr (kMode == Mode::MProc) {
            StopMProcWorker();
        } else if constexpr (kMode == Mode::Async) {
            StopWorker();
        }
        m_sinkBindings.CloseAll();
    }
    
    // =========================================================================
    // Accessors / 访问器
    // =========================================================================
    
    /**
     * @brief Get the SinkBindings
     * @brief 获取 SinkBindings
     */
    SinkBindings& GetSinkBindings() noexcept { return m_sinkBindings; }
    
    /**
     * @brief Get the SinkBindings (const)
     * @brief 获取 SinkBindings（常量）
     */
    const SinkBindings& GetSinkBindings() const noexcept { return m_sinkBindings; }
    
    /**
     * @brief Get the RuntimeConfig
     * @brief 获取 RuntimeConfig
     */
    RuntimeConfig& GetRuntimeConfig() noexcept { return m_runtimeConfig; }
    
    /**
     * @brief Get the RuntimeConfig (const)
     * @brief 获取 RuntimeConfig（常量）
     */
    const RuntimeConfig& GetRuntimeConfig() const noexcept { return m_runtimeConfig; }
    
    /**
     * @brief Check if the logger is running (async mode)
     * @brief 检查日志器是否正在运行（异步模式）
     */
    bool IsRunning() const noexcept {
        return m_running.load(std::memory_order_acquire);
    }
    
    // =========================================================================
    // MProc Mode Accessors / 多进程模式访问器
    // =========================================================================
    
    /**
     * @brief Check if this logger is the owner (producer) in MProc mode
     * @brief 检查此日志器是否是 MProc 模式下的所有者（生产者）
     *
     * _Requirements: 5.1, 5.2_
     */
    bool IsMProcOwner() const noexcept {
        return m_isMProcOwner;
    }
    
    /**
     * @brief Get the SharedLoggerConfig (MProc mode only)
     * @brief 获取 SharedLoggerConfig（仅 MProc 模式）
     *
     * Returns nullptr if not in MProc mode or SharedMemory is not initialized.
     * 如果不在 MProc 模式或 SharedMemory 未初始化，则返回 nullptr。
     *
     * _Requirements: 5.6_
     */
    SharedLoggerConfig* GetSharedConfig() noexcept {
        if constexpr (kMode == Mode::MProc) {
            return m_sharedMemory ? m_sharedMemory->GetConfig() : nullptr;
        }
        return nullptr;
    }
    
    /**
     * @brief Get the SharedLoggerConfig (const, MProc mode only)
     * @brief 获取 SharedLoggerConfig（常量，仅 MProc 模式）
     */
    const SharedLoggerConfig* GetSharedConfig() const noexcept {
        if constexpr (kMode == Mode::MProc) {
            return m_sharedMemory ? m_sharedMemory->GetConfig() : nullptr;
        }
        return nullptr;
    }
    
    /**
     * @brief Get the ProcessThreadNameTable (MProc mode only)
     * @brief 获取 ProcessThreadNameTable（仅 MProc 模式）
     *
     * Returns nullptr if not in MProc mode or SharedMemory is not initialized.
     * 如果不在 MProc 模式或 SharedMemory 未初始化，则返回 nullptr。
     *
     * _Requirements: 5.7_
     */
    ProcessThreadNameTable* GetNameTable() noexcept {
        if constexpr (kMode == Mode::MProc) {
            return m_sharedMemory ? m_sharedMemory->GetNameTable() : nullptr;
        }
        return nullptr;
    }
    
    /**
     * @brief Get the ProcessThreadNameTable (const, MProc mode only)
     * @brief 获取 ProcessThreadNameTable（常量，仅 MProc 模式）
     */
    const ProcessThreadNameTable* GetNameTable() const noexcept {
        if constexpr (kMode == Mode::MProc) {
            return m_sharedMemory ? m_sharedMemory->GetNameTable() : nullptr;
        }
        return nullptr;
    }
    
    /**
     * @brief Register a process name in the shared name table (MProc mode only)
     * @brief 在共享名称表中注册进程名称（仅 MProc 模式）
     *
     * @param name Process name to register / 要注册的进程名称
     * @return Registered process ID, or 0 if failed / 注册的进程 ID，失败返回 0
     *
     * _Requirements: 5.7_
     */
    uint32_t RegisterProcess(const std::string& name) noexcept {
        if constexpr (kMode == Mode::MProc) {
            return m_sharedMemory ? m_sharedMemory->RegisterProcess(name) : 0;
        }
        return 0;
    }
    
    /**
     * @brief Register a module name in the shared name table (MProc mode only)
     * @brief 在共享名称表中注册模块名称（仅 MProc 模式）
     *
     * @param name Module name to register / 要注册的模块名称
     * @return Registered module ID, or 0 if failed / 注册的模块 ID，失败返回 0
     *
     * _Requirements: 5.7_
     */
    uint32_t RegisterModule(const std::string& name) noexcept {
        if constexpr (kMode == Mode::MProc) {
            return m_sharedMemory ? m_sharedMemory->RegisterThread(name) : 0;
        }
        return 0;
    }
    
    /**
     * @brief Get process name by ID from the shared name table (MProc mode only)
     * @brief 从共享名称表中通过 ID 获取进程名称（仅 MProc 模式）
     *
     * @param id Process ID / 进程 ID
     * @return Process name, or nullptr if not found / 进程名称，未找到返回 nullptr
     *
     * _Requirements: 5.7_
     */
    const char* GetProcessName(uint32_t id) const noexcept {
        if constexpr (kMode == Mode::MProc) {
            return m_sharedMemory ? m_sharedMemory->GetProcessName(id) : nullptr;
        }
        return nullptr;
    }
    
    /**
     * @brief Get module name by ID from the shared name table (MProc mode only)
     * @brief 从共享名称表中通过 ID 获取模块名称（仅 MProc 模式）
     *
     * @param id Module ID / 模块 ID
     * @return Module name, or nullptr if not found / 模块名称，未找到返回 nullptr
     *
     * _Requirements: 5.7_
     */
    const char* GetModuleName(uint32_t id) const noexcept {
        if constexpr (kMode == Mode::MProc) {
            return m_sharedMemory ? m_sharedMemory->GetThreadName(id) : nullptr;
        }
        return nullptr;
    }
    
    // =========================================================================
    // Deprecated Methods (for backward compatibility) / 已弃用方法（向后兼容）
    // =========================================================================
    
    /**
     * @brief Register a thread name in the shared name table (MProc mode only)
     * @brief 在共享名称表中注册线程名称（仅 MProc 模式）
     *
     * @deprecated Use RegisterModule() instead. This method actually registers module names.
     * @deprecated 请使用 RegisterModule()。此方法实际上注册的是模块名称。
     *
     * @param name Thread name to register / 要注册的线程名称
     * @return Registered thread ID, or 0 if failed / 注册的线程 ID，失败返回 0
     *
     * _Requirements: 5.7_
     */
    [[deprecated("Use RegisterModule() instead")]]
    uint32_t RegisterThread(const std::string& name) noexcept {
        return RegisterModule(name);
    }
    
    /**
     * @brief Get thread name by ID from the shared name table (MProc mode only)
     * @brief 从共享名称表中通过 ID 获取线程名称（仅 MProc 模式）
     *
     * @deprecated Use GetModuleName(uint32_t) instead. This method actually returns module names.
     * @deprecated 请使用 GetModuleName(uint32_t)。此方法实际上返回的是模块名称。
     *
     * @param id Thread ID / 线程 ID
     * @return Thread name, or nullptr if not found / 线程名称，未找到返回 nullptr
     *
     * _Requirements: 5.7_
     */
    [[deprecated("Use GetModuleName(uint32_t) instead")]]
    const char* GetThreadName(uint32_t id) const noexcept {
        return GetModuleName(id);
    }

private:
    // =========================================================================
    // Initialization / 初始化
    // =========================================================================
    
    /**
     * @brief Initialize the logger based on mode
     * @brief 根据模式初始化日志器
     */
    void Initialize() {
        if constexpr (kMode == Mode::Async) {
            InitAsync();
        } else if constexpr (kMode == Mode::MProc) {
            InitMProc();
        }
        // Sync mode needs no initialization
    }
    
    /**
     * @brief Initialize async mode
     * @brief 初始化异步模式
     *
     * Creates the HeapRingBuffer and starts the worker thread.
     * 创建 HeapRingBuffer 并启动工作线程。
     *
     * _Requirements: 4.1, 4.2_
     */
    void InitAsync() {
        m_ringBuffer = std::make_unique<
            internal::HeapRingBuffer<LogEntry, kEnableWFC, kEnableShadowTail>
        >(kHeapRingBufferCapacity, kQueueFullPolicy);
        
        StartWorker();
    }
    
    /**
     * @brief Initialize multi-process mode
     * @brief 初始化多进程模式
     *
     * Creates or connects to SharedMemory, creates HeapRingBuffer and
     * PipelineThread (for producer), and creates WriterThread (for consumer).
     *
     * 创建或连接 SharedMemory，创建 HeapRingBuffer 和 PipelineThread（生产者），
     * 以及创建 WriterThread（消费者）。
     *
     * _Requirements: 5.1, 5.2, 5.3_
     */
    void InitMProc() {
        // Try to create shared memory (as owner/producer)
        m_sharedMemory = internal::SharedMemory<kEnableWFC, kEnableShadowTail>::Create(
            Config::SharedMemoryName::value,
            Config::kSharedRingBufferCapacity,
            kQueueFullPolicy);
        
        if (m_sharedMemory) {
            // Successfully created - we are the owner (producer)
            m_isMProcOwner = true;
            
            // Create HeapRingBuffer for local buffering
            m_ringBuffer = std::make_unique<
                internal::HeapRingBuffer<LogEntry, kEnableWFC, kEnableShadowTail>
            >(kHeapRingBufferCapacity, kQueueFullPolicy);
            
            // Create and start PipelineThread to transfer from HeapRingBuffer to SharedRingBuffer
            m_pipelineThread = std::make_unique<PipelineThread<kEnableWFC, kEnableShadowTail>>(
                *m_ringBuffer, *m_sharedMemory);
            m_pipelineThread->SetPollInterval(m_runtimeConfig.pollInterval);
            m_pipelineThread->SetPollTimeout(Config::kPollTimeout);
            m_pipelineThread->Start();
            
            // Register process name if provided
            if (!m_runtimeConfig.processName.empty()) {
                m_sharedMemory->RegisterProcess(m_runtimeConfig.processName);
            }
            
            // Start WriterThread to consume from SharedRingBuffer and write to sinks
            StartMProcWorker();
        } else {
            // Failed to create - try to connect as consumer
            m_sharedMemory = internal::SharedMemory<kEnableWFC, kEnableShadowTail>::Connect(
                Config::SharedMemoryName::value);
            
            if (m_sharedMemory) {
                // Successfully connected - we are a consumer
                m_isMProcOwner = false;
                
                // Register process name if provided
                if (!m_runtimeConfig.processName.empty()) {
                    m_sharedMemory->RegisterProcess(m_runtimeConfig.processName);
                }
                
                // Start WriterThread to consume from SharedRingBuffer
                StartMProcWorker();
            } else {
                // Failed to connect - fall back to async mode
                std::fprintf(stderr, "[oneplog] SharedMemory creation/connection failed, "
                            "falling back to Async mode\n");
                InitAsync();
            }
        }
    }

    // =========================================================================
    // Internal Logging Implementation / 内部日志实现
    // =========================================================================
    
    /**
     * @brief Internal log implementation - dispatches to mode-specific method
     * @brief 内部日志实现 - 分发到特定模式的方法
     */
    template<Level LogLevel, typename... Args>
    void LogImpl(const char* fmt, Args&&... args) noexcept {
        if constexpr (kMode == Mode::Sync) {
            LogImplSync<LogLevel>(fmt, std::forward<Args>(args)...);
        } else {
            LogImplAsync<LogLevel>(fmt, std::forward<Args>(args)...);
        }
    }
    
    /**
     * @brief Sync mode log implementation
     * @brief 同步模式日志实现
     *
     * Formats and writes directly to all sinks.
     * 直接格式化并写入所有 Sink。
     *
     * _Requirements: 3.1, 9.4_
     */
    template<Level LogLevel, typename... Args>
    void LogImplSync(const char* fmt, Args&&... args) noexcept {
        // Compile-time conditional: only get what formats need
        [[maybe_unused]] uint64_t timestamp = 0;
        [[maybe_unused]] uint32_t threadId = 0;
        [[maybe_unused]] uint32_t processId = 0;
        
        if constexpr (kNeedsTimestamp) {
            timestamp = internal::GetNanosecondTimestamp();
        }
        if constexpr (kNeedsThreadId) {
            threadId = internal::GetCurrentThreadId();
        }
        if constexpr (kNeedsProcessId) {
            processId = internal::GetCurrentProcessId();
        }
        
        // Format and write to all sinks
        m_sinkBindings.template WriteAllSync<kUseFmt>(
            LogLevel, timestamp, threadId, processId,
            fmt, std::forward<Args>(args)...);
    }
    
    /**
     * @brief Async mode log implementation
     * @brief 异步模式日志实现
     *
     * Captures format string and arguments to LogEntry and pushes to ring buffer.
     * 将格式字符串和参数捕获到 LogEntry 并推入环形队列。
     *
     * _Requirements: 4.3, 4.4_
     */
    template<Level LogLevel, typename... Args>
    void LogImplAsync(const char* fmt, Args&&... args) noexcept {
        if (!m_ringBuffer) return;
        
        LogEntry entry;
        
        // Always capture timestamp for async mode (needed for ordering)
        entry.timestamp = internal::GetNanosecondTimestamp();
        entry.level = LogLevel;
        
        // Compile-time conditional: only get what formats need
        if constexpr (kNeedsThreadId) {
            entry.threadId = internal::GetCurrentThreadId();
        }
        if constexpr (kNeedsProcessId) {
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
        m_ringBuffer->NotifyConsumerIfWaiting();  // Only notify if consumer is waiting
    }
    
    /**
     * @brief WFC log implementation
     * @brief WFC 日志实现
     *
     * Similar to async but uses TryPushWFC and waits for completion.
     * 类似于异步但使用 TryPushWFC 并等待完成。
     *
     * _Requirements: 1.3, 14.2_
     */
    template<Level LogLevel, typename... Args>
    void LogImplWFC(const char* fmt, Args&&... args) noexcept {
        if constexpr (!kEnableWFC) {
            // WFC not enabled, fall back to regular async
            LogImplAsync<LogLevel>(fmt, std::forward<Args>(args)...);
            return;
        }
        
        if constexpr (kMode == Mode::Sync) {
            // Sync mode doesn't need WFC, just log directly
            LogImplSync<LogLevel>(fmt, std::forward<Args>(args)...);
            return;
        }
        
        if (!m_ringBuffer) return;
        
        LogEntry entry;
        entry.timestamp = internal::GetNanosecondTimestamp();
        entry.level = LogLevel;
        
        if constexpr (kNeedsThreadId) {
            entry.threadId = internal::GetCurrentThreadId();
        }
        if constexpr (kNeedsProcessId) {
            entry.processId = internal::GetCurrentProcessId();
        }
        
        // Capture format string and arguments
        if constexpr (sizeof...(Args) == 0) {
            entry.snapshot.CaptureStringView(std::string_view(fmt));
        } else {
            entry.snapshot.CaptureStringView(std::string_view(fmt));
            entry.snapshot.Capture(std::forward<Args>(args)...);
        }
        
        // Push with WFC and wait for completion
        int64_t slot = m_ringBuffer->TryPushWFC(std::move(entry));
        if (slot >= 0) {
            m_ringBuffer->NotifyConsumer();
            // Wait for the entry to be processed
            m_ringBuffer->WaitForCompletion(slot, Config::kPollTimeout);
        }
    }

    // =========================================================================
    // Worker Thread Management / 工作线程管理
    // =========================================================================
    
    /**
     * @brief Start the worker thread
     * @brief 启动工作线程
     *
     * _Requirements: 4.2_
     */
    void StartWorker() {
        if constexpr (kMode == Mode::Sync) {
            return;  // Sync mode doesn't need a worker
        }
        
        // Check if already running
        if (m_running.load(std::memory_order_acquire)) {
            return;
        }
        
        m_running.store(true, std::memory_order_release);
        m_worker = std::thread([this] {
            WorkerLoop();
        });
    }
    
    /**
     * @brief Stop the worker thread
     * @brief 停止工作线程
     *
     * Ensures proper shutdown:
     * 1. Signal worker to stop
     * 2. Wake up worker if waiting
     * 3. Wait for worker to finish
     * 4. Drain remaining entries
     *
     * 确保正确关闭：
     * 1. 通知工作线程停止
     * 2. 如果工作线程在等待则唤醒它
     * 3. 等待工作线程完成
     * 4. 排空剩余条目
     *
     * _Requirements: 4.6, 8.1_
     */
    void StopWorker() {
        if constexpr (kMode == Mode::Sync) {
            return;  // Sync mode doesn't have a worker
        }
        
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
        DrainRemainingEntries();
    }
    
    /**
     * @brief Worker thread main loop
     * @brief 工作线程主循环
     *
     * _Requirements: 4.5_
     */
    void WorkerLoop() {
        while (m_running.load(std::memory_order_acquire)) {
            bool hasData = false;
            LogEntry entry;
            
            // Process all available entries
            if constexpr (kEnableWFC) {
                // Use TryPopForWFC to get slot index for WFC completion
                int64_t wfcSlot = -1;
                while (m_running.load(std::memory_order_relaxed) &&
                       m_ringBuffer && m_ringBuffer->TryPopForWFC(entry, wfcSlot)) {
                    ProcessEntry(entry);
                    // Mark WFC complete after processing
                    if (wfcSlot >= 0) {
                        m_ringBuffer->MarkWFCComplete(wfcSlot);
                    }
                    hasData = true;
                }
            } else {
                while (m_running.load(std::memory_order_relaxed) &&
                       m_ringBuffer && m_ringBuffer->TryPop(entry)) {
                    ProcessEntry(entry);
                    hasData = true;
                }
            }
            
            // No data available, wait for notification
            if (!hasData && m_running.load(std::memory_order_relaxed)) {
                if (m_ringBuffer) {
                    m_ringBuffer->WaitForData(
                        m_runtimeConfig.pollInterval,
                        Config::kPollTimeout);
                }
            }
        }
        
        // Drain remaining entries before exit
        DrainRemainingEntries();
        
        m_sinkBindings.FlushAll();
    }
    
    /**
     * @brief Process a single log entry
     * @brief 处理单个日志条目
     */
    void ProcessEntry(const LogEntry& entry) {
        m_sinkBindings.template WriteAllAsync<kUseFmt>(entry);
    }
    
    /**
     * @brief Drain remaining entries from the ring buffer
     * @brief 从环形队列排空剩余条目
     */
    void DrainRemainingEntries() {
        if (!m_ringBuffer) return;
        
        LogEntry entry;
        if constexpr (kEnableWFC) {
            int64_t wfcSlot = -1;
            while (m_ringBuffer->TryPopForWFC(entry, wfcSlot)) {
                ProcessEntry(entry);
                if (wfcSlot >= 0) {
                    m_ringBuffer->MarkWFCComplete(wfcSlot);
                }
            }
        } else {
            while (m_ringBuffer->TryPop(entry)) {
                ProcessEntry(entry);
            }
        }
    }

    // =========================================================================
    // MProc Mode Support / 多进程模式支持
    // =========================================================================
    
    /**
     * @brief Start the MProc worker thread (consumes from SharedRingBuffer)
     * @brief 启动 MProc 工作线程（从 SharedRingBuffer 消费）
     *
     * _Requirements: 5.3_
     */
    void StartMProcWorker() {
        if (m_running.load(std::memory_order_acquire)) {
            return;  // Already running
        }
        
        m_running.store(true, std::memory_order_release);
        m_worker = std::thread([this] {
            MProcWorkerLoop();
        });
    }
    
    /**
     * @brief Stop the MProc worker and pipeline threads
     * @brief 停止 MProc 工作线程和管道线程
     *
     * Ensures proper shutdown order:
     * 1. Stop pipeline thread first (stops producing to SharedRingBuffer)
     * 2. Stop worker thread (stops consuming from SharedRingBuffer)
     * 3. Drain remaining entries
     *
     * _Requirements: 5.1, 5.2, 5.3_
     */
    void StopMProcWorker() {
        // Stop pipeline thread first (if we are the owner/producer)
        if (m_pipelineThread) {
            m_pipelineThread->Stop();
        }
        
        // Signal worker to stop
        if (!m_running.load(std::memory_order_acquire)) {
            return;
        }
        
        m_running.store(false, std::memory_order_release);
        
        // Wake up worker if it's waiting
        if (m_sharedMemory) {
            m_sharedMemory->NotifyConsumer();
        }
        
        // Wait for worker to finish
        if (m_worker.joinable()) {
            m_worker.join();
        }
        
        // Drain remaining entries from SharedRingBuffer
        DrainSharedRingBuffer();
        
        // Also drain HeapRingBuffer if we are the owner
        if (m_isMProcOwner && m_ringBuffer) {
            DrainRemainingEntries();
        }
    }
    
    /**
     * @brief MProc worker thread main loop (consumes from SharedRingBuffer)
     * @brief MProc 工作线程主循环（从 SharedRingBuffer 消费）
     *
     * _Requirements: 5.3_
     */
    void MProcWorkerLoop() {
        if (!m_sharedMemory) return;
        
        auto* sharedRingBuffer = m_sharedMemory->GetRingBuffer();
        if (!sharedRingBuffer) return;
        
        while (m_running.load(std::memory_order_acquire)) {
            bool hasData = false;
            LogEntry entry;
            
            // Process all available entries from SharedRingBuffer
            if constexpr (kEnableWFC) {
                int64_t wfcSlot = -1;
                while (m_running.load(std::memory_order_relaxed) &&
                       sharedRingBuffer->TryPopForWFC(entry, wfcSlot)) {
                    ProcessEntry(entry);
                    if (wfcSlot >= 0) {
                        sharedRingBuffer->MarkWFCComplete(wfcSlot);
                    }
                    hasData = true;
                }
            } else {
                while (m_running.load(std::memory_order_relaxed) &&
                       sharedRingBuffer->TryPop(entry)) {
                    ProcessEntry(entry);
                    hasData = true;
                }
            }
            
            // No data available, wait for notification
            if (!hasData && m_running.load(std::memory_order_relaxed)) {
                m_sharedMemory->WaitForData(
                    m_runtimeConfig.pollInterval,
                    Config::kPollTimeout);
            }
        }
        
        // Drain remaining entries before exit
        DrainSharedRingBuffer();
        
        m_sinkBindings.FlushAll();
    }
    
    /**
     * @brief Drain remaining entries from the SharedRingBuffer
     * @brief 从 SharedRingBuffer 排空剩余条目
     */
    void DrainSharedRingBuffer() {
        if (!m_sharedMemory) return;
        
        auto* sharedRingBuffer = m_sharedMemory->GetRingBuffer();
        if (!sharedRingBuffer) return;
        
        LogEntry entry;
        if constexpr (kEnableWFC) {
            int64_t wfcSlot = -1;
            while (sharedRingBuffer->TryPopForWFC(entry, wfcSlot)) {
                ProcessEntry(entry);
                if (wfcSlot >= 0) {
                    sharedRingBuffer->MarkWFCComplete(wfcSlot);
                }
            }
        } else {
            while (sharedRingBuffer->TryPop(entry)) {
                ProcessEntry(entry);
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
    // For Async mode:
    //   1. m_worker - thread (should be joined before ringBuffer is destroyed)
    //   2. m_running - atomic flag
    //   3. m_ringBuffer - destroyed after worker is stopped
    //   4. m_runtimeConfig - runtime configuration
    //   5. m_sinkBindings - destroyed last (after all logging is done)
    //
    // For MProc mode:
    //   1. m_worker - consumer thread (should be joined first)
    //   2. m_pipelineThread - pipeline thread (should be stopped after worker)
    //   3. m_running - atomic flag
    //   4. m_sharedMemory - shared memory (destroyed after threads stopped)
    //   5. m_ringBuffer - heap ring buffer (destroyed after pipeline thread)
    //   6. m_runtimeConfig - runtime configuration
    //   7. m_sinkBindings - destroyed last (after all logging is done)
    
    /// Sink bindings (destroyed last)
    SinkBindings m_sinkBindings;
    
    /// Runtime configuration
    RuntimeConfig m_runtimeConfig;
    
    /// Ring buffer for async/mproc modes (producer side in MProc)
    std::unique_ptr<internal::HeapRingBuffer<LogEntry, kEnableWFC, kEnableShadowTail>> m_ringBuffer;
    
    /// Shared memory for MProc mode
    std::unique_ptr<internal::SharedMemory<kEnableWFC, kEnableShadowTail>> m_sharedMemory;
    
    /// Pipeline thread for MProc mode (transfers from HeapRingBuffer to SharedRingBuffer)
    std::unique_ptr<PipelineThread<kEnableWFC, kEnableShadowTail>> m_pipelineThread;
    
    /// Running flag for worker thread
    std::atomic<bool> m_running{false};
    
    /// Worker thread (consumer in Async mode, SharedRingBuffer consumer in MProc mode)
    std::thread m_worker;
    
    /// Flag to indicate if this is the owner (producer) in MProc mode
    bool m_isMProcOwner{false};
};

// ==============================================================================
// Default Configuration Presets / 默认配置预设
// ==============================================================================

/**
 * @brief Default sync configuration preset
 * @brief 默认同步配置预设
 *
 * Sync mode + Console output + SimpleFormat
 * 同步模式 + 控制台输出 + SimpleFormat
 *
 * Default values:
 * - HeapRingBufferCapacity: 8192 (unused in sync mode)
 * - SharedRingBufferCapacity: 4096 (unused in sync mode)
 * - QueueFullPolicy: DropNewest
 * - PollInterval: 1 microsecond
 * - PollTimeout: 10 milliseconds
 * - Level: Debug (debug mode) / Info (release mode)
 *
 * _Requirements: 16.1, 16.5_
 */
struct DefaultSyncConfig : LoggerConfig<
    Mode::Sync,
    kDefaultLevel,
    false,  // EnableWFC
    false,  // EnableShadowTail (not needed for sync)
    true,   // UseFmt
    8192,   // HeapRingBufferCapacity (unused in sync)
    4096,   // SharedRingBufferCapacity (unused in sync)
    QueueFullPolicy::Block,
    DefaultSharedMemoryName,
    10,     // PollTimeoutMs
    DefaultSinkBindings  // Console + SimpleFormat
> {};

/**
 * @brief Default async configuration preset
 * @brief 默认异步配置预设
 *
 * Async mode + Console output + SimpleFormat
 * 异步模式 + 控制台输出 + SimpleFormat
 *
 * Default values:
 * - HeapRingBufferCapacity: 8192
 * - SharedRingBufferCapacity: 4096
 * - QueueFullPolicy: DropNewest
 * - PollInterval: 1 microsecond
 * - PollTimeout: 10 milliseconds
 * - Level: Debug (debug mode) / Info (release mode)
 *
 * _Requirements: 16.2, 16.5_
 */
struct DefaultAsyncConfig : LoggerConfig<
    Mode::Async,
    kDefaultLevel,
    false,  // EnableWFC
    true,   // EnableShadowTail
    true,   // UseFmt
    8192,   // HeapRingBufferCapacity
    4096,   // SharedRingBufferCapacity
    QueueFullPolicy::Block,
    DefaultSharedMemoryName,
    10,     // PollTimeoutMs
    DefaultSinkBindings  // Console + SimpleFormat
> {};

/**
 * @brief Default multi-process configuration preset
 * @brief 默认多进程配置预设
 *
 * MProc mode + Console output + SimpleFormat + default shared memory configuration
 * 多进程模式 + 控制台输出 + SimpleFormat + 默认共享内存配置
 *
 * Default values:
 * - HeapRingBufferCapacity: 8192
 * - SharedRingBufferCapacity: 4096
 * - QueueFullPolicy: DropNewest
 * - PollInterval: 1 microsecond
 * - PollTimeout: 10 milliseconds
 * - SharedMemoryName: "oneplog_shared"
 * - Level: Debug (debug mode) / Info (release mode)
 *
 * _Requirements: 16.3, 16.5_
 */
struct DefaultMProcConfig : LoggerConfig<
    Mode::MProc,
    kDefaultLevel,
    false,  // EnableWFC
    true,   // EnableShadowTail
    true,   // UseFmt
    8192,   // HeapRingBufferCapacity
    4096,   // SharedRingBufferCapacity
    QueueFullPolicy::Block,
    DefaultSharedMemoryName,
    10,     // PollTimeoutMs
    DefaultSinkBindings  // Console + SimpleFormat
> {};

/**
 * @brief High performance configuration preset
 * @brief 高性能配置预设
 *
 * Async mode + NullSink + MessageOnlyFormat (for benchmarking)
 * 异步模式 + NullSink + MessageOnlyFormat（用于基准测试）
 *
 * Optimized for maximum throughput:
 * - NullSink discards output (no I/O overhead)
 * - MessageOnlyFormat skips metadata formatting
 * - Level::Info filters out Trace/Debug
 *
 * Default values:
 * - HeapRingBufferCapacity: 8192
 * - SharedRingBufferCapacity: 4096
 * - QueueFullPolicy: DropNewest
 * - PollInterval: 1 microsecond
 * - PollTimeout: 10 milliseconds
 *
 * _Requirements: 16.4, 16.5_
 */
struct HighPerformanceConfig : LoggerConfig<
    Mode::Async,
    Level::Info,
    false,  // EnableWFC
    true,   // EnableShadowTail
    true,   // UseFmt
    8192,   // HeapRingBufferCapacity
    4096,   // SharedRingBufferCapacity
    QueueFullPolicy::Block,
    DefaultSharedMemoryName,
    10,     // PollTimeoutMs
    HighPerformanceSinkBindings  // NullSink + MessageOnlyFormat
> {};

// ==============================================================================
// Type Aliases / 类型别名
// ==============================================================================

/**
 * @brief Sync logger type alias (V2 suffix for explicit versioning)
 * @brief 同步日志器类型别名（V2 后缀用于显式版本控制）
 *
 * _Requirements: 1.7, 14.5_
 */
using SyncLoggerV2 = LoggerImpl<DefaultSyncConfig>;

/**
 * @brief Async logger type alias (V2 suffix for explicit versioning)
 * @brief 异步日志器类型别名（V2 后缀用于显式版本控制）
 *
 * _Requirements: 1.7, 14.5_
 */
using AsyncLoggerV2 = LoggerImpl<DefaultAsyncConfig>;

/**
 * @brief Multi-process logger type alias (V2 suffix for explicit versioning)
 * @brief 多进程日志器类型别名（V2 后缀用于显式版本控制）
 *
 * _Requirements: 1.7, 14.5_
 */
using MProcLoggerV2 = LoggerImpl<DefaultMProcConfig>;

/**
 * @brief Sync logger type alias (without V2 suffix for convenience)
 * @brief 同步日志器类型别名（无 V2 后缀，便于使用）
 *
 * _Requirements: 1.7, 14.5_
 */
using SyncLogger = LoggerImpl<DefaultSyncConfig>;

/**
 * @brief Async logger type alias (without V2 suffix for convenience)
 * @brief 异步日志器类型别名（无 V2 后缀，便于使用）
 *
 * _Requirements: 1.7, 14.5_
 */
using AsyncLogger = LoggerImpl<DefaultAsyncConfig>;

/**
 * @brief Multi-process logger type alias (without V2 suffix for convenience)
 * @brief 多进程日志器类型别名（无 V2 后缀，便于使用）
 *
 * _Requirements: 1.7, 14.5_
 */
using MProcLogger = LoggerImpl<DefaultMProcConfig>;

/**
 * @brief Backward-compatible Logger type alias
 * @brief 向后兼容的 Logger 类型别名
 *
 * This template alias provides backward compatibility with the old Logger API.
 * It maps the old template parameters to the new LoggerImpl configuration.
 *
 * 此模板别名提供与旧 Logger API 的向后兼容性。
 * 它将旧的模板参数映射到新的 LoggerImpl 配置。
 *
 * @tparam M Operating mode (default: Async)
 * @tparam L Minimum log level (default: kDefaultLevel)
 * @tparam EnableWFC Enable WFC functionality (default: false)
 * @tparam EnableShadowTail Enable shadow tail optimization (default: true)
 *
 * _Requirements: 1.7, 14.5_
 */
template<Mode M = Mode::Async, 
         Level L = kDefaultLevel,
         bool EnableWFC = false, 
         bool EnableShadowTail = true>
using Logger = LoggerImpl<LoggerConfig<
    M, L, EnableWFC, EnableShadowTail, true,
    8192, 8192, QueueFullPolicy::Block,
    DefaultSharedMemoryName, 10, DefaultSinkBindings
>>;

// ==============================================================================
// Backward Compatibility Aliases / 向后兼容别名
// ==============================================================================

/**
 * @brief Backward-compatible FastLoggerV2 type alias
 * @brief 向后兼容的 FastLoggerV2 类型别名
 *
 * @deprecated Use LoggerImpl instead
 */
template<typename Config = LoggerConfig<>>
using FastLoggerV2 = LoggerImpl<Config>;

}  // namespace oneplog
