/**
 * @file logger_config.hpp
 * @brief Compile-time configuration infrastructure for Logger
 * @brief Logger 的编译期配置基础设施
 *
 * This file provides the compile-time configuration structures for Logger:
 * - StaticFormatRequirements: Specifies metadata requirements for formatters
 * - SinkBinding: Binds a Sink type with a Format type
 * - SinkBindingList: Manages multiple SinkBindings with union of requirements
 * - LoggerConfig: Aggregates all compile-time configuration
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include <chrono>
#include <cstddef>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

#include "oneplog/common.hpp"
#include "oneplog/internal/log_entry.hpp"
#include "oneplog/internal/heap_memory.hpp"
#include <fmt/format.h>

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
 */
#ifndef ONEPLOG_DEFAULT_LEVEL_DEFINED
#define ONEPLOG_DEFAULT_LEVEL_DEFINED
#ifdef NDEBUG
constexpr Level kDefaultLevel = Level::Info;
#else
constexpr Level kDefaultLevel = Level::Debug;
#endif
#endif

// ==============================================================================
// StaticFormatRequirements - 编译期格式化需求
// ==============================================================================

/**
 * @brief Compile-time format requirements specification
 * @brief 编译期格式化需求规范
 *
 * Defines which metadata fields a formatter needs. FastLogger uses these
 * flags to conditionally acquire only the required metadata at compile time.
 *
 * 定义格式化器需要哪些元数据字段。FastLogger 使用这些标志在编译期
 * 有条件地只获取所需的元数据。
 *
 * @tparam NeedsTimestamp Whether timestamp is required / 是否需要时间戳
 * @tparam NeedsLevel Whether log level is required / 是否需要日志级别
 * @tparam NeedsThreadId Whether thread ID is required / 是否需要线程 ID
 * @tparam NeedsProcessId Whether process ID is required / 是否需要进程 ID
 * @tparam NeedsSourceLocation Whether source location is required / 是否需要源位置
 *
 * _Requirements: 9.1_
 */
template<bool NeedsTimestamp = true, 
         bool NeedsLevel = true,
         bool NeedsThreadId = false, 
         bool NeedsProcessId = false,
         bool NeedsSourceLocation = false>
struct StaticFormatRequirements {
    /// Whether timestamp is required / 是否需要时间戳
    static constexpr bool kNeedsTimestamp = NeedsTimestamp;
    
    /// Whether log level is required / 是否需要日志级别
    static constexpr bool kNeedsLevel = NeedsLevel;
    
    /// Whether thread ID is required / 是否需要线程 ID
    static constexpr bool kNeedsThreadId = NeedsThreadId;
    
    /// Whether process ID is required / 是否需要进程 ID
    static constexpr bool kNeedsProcessId = NeedsProcessId;
    
    /// Whether source location is required / 是否需要源位置
    static constexpr bool kNeedsSourceLocation = NeedsSourceLocation;
};

// Common requirement presets / 常用需求预设
using NoRequirements = StaticFormatRequirements<false, false, false, false, false>;
using TimestampOnlyRequirements = StaticFormatRequirements<true, false, false, false, false>;
using BasicRequirements = StaticFormatRequirements<true, true, false, false, false>;
using ThreadRequirements = StaticFormatRequirements<true, true, true, false, false>;
using FullRequirements = StaticFormatRequirements<true, true, true, true, false>;
using DebugRequirements = StaticFormatRequirements<true, true, true, true, true>;


// ==============================================================================
// SinkBinding - Sink 与 Format 绑定
// ==============================================================================

/**
 * @brief Binds a Sink type with a Format type
 * @brief 将 Sink 类型与 Format 类型绑定
 *
 * SinkBinding pairs a Sink with its corresponding Format. Each Sink can have
 * its own independent Format, allowing different output formats for different
 * destinations (e.g., console vs file).
 *
 * SinkBinding 将 Sink 与其对应的 Format 配对。每个 Sink 可以有自己独立的
 * Format，允许不同目标使用不同的输出格式（例如控制台 vs 文件）。
 *
 * @tparam SinkT The Sink type / Sink 类型
 * @tparam FormatT The Format type / Format 类型
 *
 * _Requirements: 10.4_
 */
template<typename SinkT, typename FormatT>
struct SinkBinding {
    /// Sink type alias / Sink 类型别名
    using Sink = SinkT;
    
    /// Format type alias / Format 类型别名
    using Format = FormatT;
    
    /// Format requirements from the Format type / 来自 Format 类型的格式化需求
    using Requirements = typename FormatT::Requirements;
    
    /// The sink instance / Sink 实例
    Sink sink;
    
    /// Default constructor / 默认构造函数
    SinkBinding() = default;
    
    /// Construct with sink instance / 使用 sink 实例构造
    explicit SinkBinding(Sink s) : sink(std::move(s)) {}
    
    /**
     * @brief Write in sync mode - format and write directly
     * @brief 同步模式写入 - 直接格式化并写入
     *
     * @tparam UseFmt Whether to use fmt library / 是否使用 fmt 库
     * @tparam Args Argument types / 参数类型
     * @param level Log level / 日志级别
     * @param timestamp Nanosecond timestamp / 纳秒时间戳
     * @param threadId Thread ID / 线程 ID
     * @param processId Process ID / 进程 ID
     * @param fmt Format string / 格式字符串
     * @param args Format arguments / 格式化参数
     */
    template<bool UseFmt = true, typename... Args>
    void WriteSync(Level level, uint64_t timestamp, 
                   uint32_t threadId, uint32_t processId,
                   const char* fmt, Args&&... args) noexcept {
        try {
            if constexpr (UseFmt) {
                fmt::memory_buffer buffer;
                FormatT::FormatTo(buffer, level, timestamp, threadId, processId,
                                  fmt, std::forward<Args>(args)...);
                sink.Write(std::string_view(buffer.data(), buffer.size()));
            } else {
                // Non-fmt fallback using BinarySnapshot
                // 非 fmt 回退实现，使用 BinarySnapshot
                BinarySnapshot snapshot;
                snapshot.CaptureStringView(std::string_view(fmt));
                if constexpr (sizeof...(Args) > 0) {
                    snapshot.Capture(std::forward<Args>(args)...);
                }
                std::string msg = FormatT::FormatEntry(level, timestamp, threadId, 
                                                       processId, snapshot);
                sink.Write(msg);
            }
        } catch (...) {
            // Silently ignore errors in logging
        }
    }
    
    /**
     * @brief Write in async mode - format from LogEntry and write
     * @brief 异步模式写入 - 从 LogEntry 格式化并写入
     *
     * @tparam UseFmt Whether to use fmt library / 是否使用 fmt 库
     * @param entry The log entry to format and write / 要格式化和写入的日志条目
     */
    template<bool UseFmt = true>
    void WriteAsync(const LogEntry& entry) noexcept {
        try {
            if constexpr (UseFmt) {
                fmt::memory_buffer buffer;
                FormatT::FormatEntryTo(buffer, entry);
                sink.Write(std::string_view(buffer.data(), buffer.size()));
            } else {
                // Non-fmt fallback
                // 非 fmt 回退实现
                std::string msg = FormatT::FormatEntry(entry);
                sink.Write(msg);
            }
        } catch (...) {
            // Silently ignore errors in logging
        }
    }
    
    /// Flush the sink / 刷新 Sink
    void Flush() noexcept {
        try {
            sink.Flush();
        } catch (...) {}
    }
    
    /// Close the sink / 关闭 Sink
    void Close() noexcept {
        try {
            sink.Close();
        } catch (...) {}
    }
};


// ==============================================================================
// SinkBindingList - 多 Sink 绑定列表
// ==============================================================================

/**
 * @brief Manages multiple SinkBindings with compile-time requirement union
 * @brief 管理多个 SinkBinding，并计算编译期需求并集
 *
 * SinkBindingList holds multiple SinkBinding instances and computes the union
 * of all their format requirements at compile time. This allows FastLogger to
 * acquire only the metadata needed by at least one formatter.
 *
 * SinkBindingList 持有多个 SinkBinding 实例，并在编译期计算所有格式化需求的
 * 并集。这允许 FastLogger 只获取至少一个格式化器需要的元数据。
 *
 * @tparam Bindings The SinkBinding types / SinkBinding 类型列表
 *
 * _Requirements: 1.6, 2.2_
 */
template<typename... Bindings>
struct SinkBindingList {
    /// Tuple of all bindings / 所有绑定的元组
    std::tuple<Bindings...> bindings;
    
    /// Number of bindings / 绑定数量
    static constexpr size_t kBindingCount = sizeof...(Bindings);
    
    // =========================================================================
    // Compile-time requirement union / 编译期需求并集
    // =========================================================================
    
    /// Union of timestamp requirements / 时间戳需求并集
    static constexpr bool kNeedsTimestamp = (Bindings::Requirements::kNeedsTimestamp || ...);
    
    /// Union of level requirements / 日志级别需求并集
    static constexpr bool kNeedsLevel = (Bindings::Requirements::kNeedsLevel || ...);
    
    /// Union of thread ID requirements / 线程 ID 需求并集
    static constexpr bool kNeedsThreadId = (Bindings::Requirements::kNeedsThreadId || ...);
    
    /// Union of process ID requirements / 进程 ID 需求并集
    static constexpr bool kNeedsProcessId = (Bindings::Requirements::kNeedsProcessId || ...);
    
    /// Union of source location requirements / 源位置需求并集
    static constexpr bool kNeedsSourceLocation = (Bindings::Requirements::kNeedsSourceLocation || ...);
    
    // =========================================================================
    // Constructors / 构造函数
    // =========================================================================
    
    /// Default constructor / 默认构造函数
    SinkBindingList() = default;
    
    /// Construct with binding instances / 使用绑定实例构造
    explicit SinkBindingList(Bindings... b) : bindings(std::move(b)...) {}
    
    // =========================================================================
    // Sync mode operations / 同步模式操作
    // =========================================================================
    
    /**
     * @brief Write to all sinks in sync mode
     * @brief 同步模式下写入所有 Sink
     *
     * @tparam UseFmt Whether to use fmt library / 是否使用 fmt 库
     * @tparam Args Argument types / 参数类型
     */
    template<bool UseFmt = true, typename... Args>
    void WriteAllSync(Level level, uint64_t timestamp,
                      uint32_t threadId, uint32_t processId,
                      const char* fmt, Args&&... args) noexcept {
        std::apply([&](auto&... binding) {
            (binding.template WriteSync<UseFmt>(level, timestamp, threadId, processId,
                                                fmt, std::forward<Args>(args)...), ...);
        }, bindings);
    }
    
    // =========================================================================
    // Async mode operations / 异步模式操作
    // =========================================================================
    
    /**
     * @brief Write to all sinks in async mode
     * @brief 异步模式下写入所有 Sink
     *
     * @tparam UseFmt Whether to use fmt library / 是否使用 fmt 库
     * @param entry The log entry to format and write / 要格式化和写入的日志条目
     */
    template<bool UseFmt = true>
    void WriteAllAsync(const LogEntry& entry) noexcept {
        std::apply([&](auto&... binding) {
            (binding.template WriteAsync<UseFmt>(entry), ...);
        }, bindings);
    }
    
    // =========================================================================
    // Control operations / 控制操作
    // =========================================================================
    
    /**
     * @brief Flush all sinks
     * @brief 刷新所有 Sink
     */
    void FlushAll() noexcept {
        std::apply([](auto&... binding) {
            (binding.Flush(), ...);
        }, bindings);
    }
    
    /**
     * @brief Close all sinks
     * @brief 关闭所有 Sink
     */
    void CloseAll() noexcept {
        std::apply([](auto&... binding) {
            (binding.Close(), ...);
        }, bindings);
    }
    
    // =========================================================================
    // Accessors / 访问器
    // =========================================================================
    
    /**
     * @brief Get a specific binding by index
     * @brief 通过索引获取特定绑定
     */
    template<size_t I>
    auto& Get() noexcept {
        return std::get<I>(bindings);
    }
    
    /**
     * @brief Get a specific binding by index (const)
     * @brief 通过索引获取特定绑定（常量）
     */
    template<size_t I>
    const auto& Get() const noexcept {
        return std::get<I>(bindings);
    }
};

/// Empty SinkBindingList specialization / 空 SinkBindingList 特化
template<>
struct SinkBindingList<> {
    std::tuple<> bindings;
    
    static constexpr size_t kBindingCount = 0;
    static constexpr bool kNeedsTimestamp = false;
    static constexpr bool kNeedsLevel = false;
    static constexpr bool kNeedsThreadId = false;
    static constexpr bool kNeedsProcessId = false;
    static constexpr bool kNeedsSourceLocation = false;
    
    template<bool UseFmt = true, typename... Args>
    void WriteAllSync(Level, uint64_t, uint32_t, uint32_t, const char*, Args&&...) noexcept {}
    
    template<bool UseFmt = true>
    void WriteAllAsync(const LogEntry&) noexcept {}
    
    void FlushAll() noexcept {}
    void CloseAll() noexcept {}
};


// ==============================================================================
// Default Shared Memory Name / 默认共享内存名称
// ==============================================================================

/**
 * @brief Default shared memory name type
 * @brief 默认共享内存名称类型
 */
struct DefaultSharedMemoryName {
    static constexpr const char* value = "oneplog_shared";
};

// ==============================================================================
// LoggerConfig - 编译期配置聚合
// ==============================================================================

/**
 * @brief Compile-time configuration aggregation for Logger
 * @brief Logger 的编译期配置聚合
 *
 * LoggerConfig aggregates all compile-time configuration options into a
 * single template structure. This allows Logger to be configured entirely
 * at compile time, enabling maximum optimization.
 *
 * LoggerConfig 将所有编译期配置选项聚合到单一模板结构中。这允许
 * Logger 完全在编译期配置，实现最大程度的优化。
 *
 * @tparam M Operating mode (Sync/Async/MProc) / 运行模式
 * @tparam L Minimum log level / 最小日志级别
 * @tparam EnableWFC Enable WFC functionality / 启用 WFC 功能
 * @tparam EnableShadowTail Enable shadow tail optimization / 启用影子尾指针优化
 * @tparam UseFmt Use fmt library for formatting / 使用 fmt 库格式化
 * @tparam HeapRingBufferCapacity Heap ring buffer slot count / 堆环形队列槽位数量
 * @tparam SharedRingBufferCapacity Shared ring buffer slot count / 共享环形队列槽位数量
 * @tparam Policy Queue full policy / 队列满策略
 * @tparam SharedMemoryNameT Shared memory name type / 共享内存名称类型
 * @tparam PollTimeoutMs Poll timeout in milliseconds / 轮询超时（毫秒）
 * @tparam SinkBindingsT SinkBindingList type / SinkBindingList 类型
 *
 * _Requirements: 12.1, 12.2, 12.3, 12.4, 12.5_
 */
template<
    Mode M = Mode::Sync,
    Level L = kDefaultLevel,
    bool EnableWFC = false,
    bool EnableShadowTail = true,
    bool UseFmt = true,
    size_t HeapRingBufferCapacity = 8192,
    size_t SharedRingBufferCapacity = 8192,
    QueueFullPolicy Policy = QueueFullPolicy::Block,
    typename SharedMemoryNameT = DefaultSharedMemoryName,
    int64_t PollTimeoutMs = 10,
    typename SinkBindingsT = SinkBindingList<>
>
struct LoggerConfig {
    // =========================================================================
    // Mode configuration / 模式配置
    // =========================================================================
    
    /// Operating mode / 运行模式
    static constexpr Mode kMode = M;
    
    /// Minimum log level / 最小日志级别
    static constexpr Level kLevel = L;
    
    /// WFC functionality enabled / WFC 功能启用
    static constexpr bool kEnableWFC = EnableWFC;
    
    /// Shadow tail optimization enabled / 影子尾指针优化启用
    static constexpr bool kEnableShadowTail = EnableShadowTail;
    
    // =========================================================================
    // Formatting configuration / 格式化配置
    // =========================================================================
    
    /// Use fmt library for formatting / 使用 fmt 库格式化
    static constexpr bool kUseFmt = UseFmt;
    
    // =========================================================================
    // Queue configuration / 队列配置
    // =========================================================================
    
    /// Heap ring buffer capacity (slot count) / 堆环形队列容量（槽位数量）
    static constexpr size_t kHeapRingBufferCapacity = HeapRingBufferCapacity;
    
    /// Shared ring buffer capacity (slot count) / 共享环形队列容量（槽位数量）
    static constexpr size_t kSharedRingBufferCapacity = SharedRingBufferCapacity;
    
    /// Queue full policy / 队列满策略
    static constexpr QueueFullPolicy kQueueFullPolicy = Policy;
    
    // =========================================================================
    // Multi-process configuration / 多进程配置
    // =========================================================================
    
    /// Shared memory name type / 共享内存名称类型
    using SharedMemoryName = SharedMemoryNameT;
    
    /// Poll timeout / 轮询超时
    static constexpr std::chrono::milliseconds kPollTimeout{PollTimeoutMs};
    
    // =========================================================================
    // Sink configuration / Sink 配置
    // =========================================================================
    
    /// SinkBindingList type / SinkBindingList 类型
    using SinkBindings = SinkBindingsT;
};

// ==============================================================================
// Configuration Helpers / 配置辅助
// ==============================================================================

/**
 * @brief Helper to create a LoggerConfig with custom SinkBindings
 * @brief 创建带有自定义 SinkBindings 的 LoggerConfig 的辅助模板
 */
template<typename SinkBindingsT,
         Mode M = Mode::Sync,
         Level L = kDefaultLevel,
         bool EnableWFC = false,
         bool EnableShadowTail = true,
         bool UseFmt = true>
using LoggerConfigWithSinks = LoggerConfig<
    M, L, EnableWFC, EnableShadowTail, UseFmt,
    8192, 8192, QueueFullPolicy::Block,
    DefaultSharedMemoryName, 10, SinkBindingsT
>;

/**
 * @brief Helper to create an async LoggerConfig
 * @brief 创建异步 LoggerConfig 的辅助模板
 */
template<typename SinkBindingsT,
         Level L = kDefaultLevel,
         bool EnableWFC = false,
         bool EnableShadowTail = true,
         size_t BufferCapacity = 8192>
using AsyncLoggerConfig = LoggerConfig<
    Mode::Async, L, EnableWFC, EnableShadowTail, true,
    BufferCapacity, 8192, QueueFullPolicy::Block,
    DefaultSharedMemoryName, 10, SinkBindingsT
>;

/**
 * @brief Helper to create a sync LoggerConfig
 * @brief 创建同步 LoggerConfig 的辅助模板
 */
template<typename SinkBindingsT,
         Level L = kDefaultLevel>
using SyncLoggerConfig = LoggerConfig<
    Mode::Sync, L, false, false, true,
    8192, 8192, QueueFullPolicy::Block,
    DefaultSharedMemoryName, 10, SinkBindingsT
>;

/**
 * @brief Helper to create a multi-process LoggerConfig
 * @brief 创建多进程 LoggerConfig 的辅助模板
 */
template<typename SinkBindingsT,
         typename SharedMemoryNameT,
         Level L = kDefaultLevel,
         bool EnableWFC = false,
         bool EnableShadowTail = true>
using MProcLoggerConfig = LoggerConfig<
    Mode::MProc, L, EnableWFC, EnableShadowTail, true,
    8192, 8192, QueueFullPolicy::Block,
    SharedMemoryNameT, 10, SinkBindingsT
>;

// ==============================================================================
// Backward Compatibility Aliases / 向后兼容别名
// ==============================================================================

/// @deprecated Use LoggerConfig instead
template<
    Mode M = Mode::Sync,
    Level L = kDefaultLevel,
    bool EnableWFC = false,
    bool EnableShadowTail = true,
    bool UseFmt = true,
    size_t HeapRingBufferCapacity = 8192,
    size_t SharedRingBufferCapacity = 8192,
    QueueFullPolicy Policy = QueueFullPolicy::Block,
    typename SharedMemoryNameT = DefaultSharedMemoryName,
    int64_t PollTimeoutMs = 10,
    typename SinkBindingsT = SinkBindingList<>
>
using FastLoggerConfig = LoggerConfig<M, L, EnableWFC, EnableShadowTail, UseFmt,
    HeapRingBufferCapacity, SharedRingBufferCapacity, Policy,
    SharedMemoryNameT, PollTimeoutMs, SinkBindingsT>;

/// @deprecated Use LoggerConfigWithSinks instead
template<typename SinkBindingsT,
         Mode M = Mode::Sync,
         Level L = kDefaultLevel,
         bool EnableWFC = false,
         bool EnableShadowTail = true,
         bool UseFmt = true>
using FastLoggerConfigWithSinks = LoggerConfigWithSinks<SinkBindingsT, M, L, EnableWFC, EnableShadowTail, UseFmt>;

/// @deprecated Use AsyncLoggerConfig instead
template<typename SinkBindingsT,
         Level L = kDefaultLevel,
         bool EnableWFC = false,
         bool EnableShadowTail = true,
         size_t BufferCapacity = 8192>
using AsyncFastLoggerConfig = AsyncLoggerConfig<SinkBindingsT, L, EnableWFC, EnableShadowTail, BufferCapacity>;

/// @deprecated Use SyncLoggerConfig instead
template<typename SinkBindingsT,
         Level L = kDefaultLevel>
using SyncFastLoggerConfig = SyncLoggerConfig<SinkBindingsT, L>;

/// @deprecated Use MProcLoggerConfig instead
template<typename SinkBindingsT,
         typename SharedMemoryNameT,
         Level L = kDefaultLevel,
         bool EnableWFC = false,
         bool EnableShadowTail = true>
using MProcFastLoggerConfig = MProcLoggerConfig<SinkBindingsT, SharedMemoryNameT, L, EnableWFC, EnableShadowTail>;

}  // namespace oneplog
