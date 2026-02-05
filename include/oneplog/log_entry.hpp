/**
 * @file log_entry.hpp
 * @brief Log entry structures with compile-time configurable fields
 * @文件 log_entry.hpp
 * @简述 具有编译时可配置字段的日志条目结构
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include "binary_snapshot.hpp"
#include "common.hpp"
#include <cstdint>
#include <type_traits>
#include <variant>

namespace oneplog {

// ==============================================================================
// SourceLocation Structure / SourceLocation 结构体
// ==============================================================================

/**
 * @brief Source code location information
 * @brief 源代码位置信息
 * 
 * Captures the file, line, and function where a log statement was invoked.
 * Used in Debug builds to provide detailed debugging information.
 * 
 * 捕获日志语句被调用时的文件、行号和函数名。
 * 在 Debug 构建中使用，提供详细的调试信息。
 */
struct SourceLocation {
    /// Source file name (pointer to string literal)
    /// 源文件名（指向字符串字面量的指针）
    const char* file{nullptr};

    /// Line number in source file
    /// 源文件中的行号
    uint32_t line{0};

    /// Function name (pointer to string literal)
    /// 函数名（指向字符串字面量的指针）
    const char* function{nullptr};

    /**
     * @brief Default constructor
     * @brief 默认构造函数
     */
    constexpr SourceLocation() = default;

    /**
     * @brief Constructor with parameters
     * @brief 带参数的构造函数
     * @param f File name / 文件名
     * @param l Line number / 行号
     * @param fn Function name / 函数名
     */
    constexpr SourceLocation(const char* f, uint32_t l, const char* fn)
        : file(f), line(l), function(fn) {}

    /**
     * @brief Check if location is valid
     * @brief 检查位置是否有效
     * @return bool True if valid / 如果有效则返回 true
     */
    constexpr bool IsValid() const {
        return file != nullptr && function != nullptr;
    }

    /**
     * @brief Equality operator
     * @brief 相等运算符
     */
    constexpr bool operator==(const SourceLocation& other) const {
        return file == other.file && line == other.line && function == other.function;
    }

    /**
     * @brief Inequality operator
     * @brief 不等运算符
     */
    constexpr bool operator!=(const SourceLocation& other) const {
        return !(*this == other);
    }
};

// ==============================================================================
// LogEntry Feature Flags / LogEntry 特性标志
// ==============================================================================

/**
 * @brief Feature flags for compile-time LogEntry configuration
 * @brief 用于编译时 LogEntry 配置的特性标志
 * 
 * These flags can be combined using bitwise OR to create custom LogEntry configurations.
 * 这些标志可以通过按位或组合，创建自定义的 LogEntry 配置。
 */
enum class LogEntryFeatures : uint32_t {
    None = 0,                    ///< No optional features / 无可选特性
    Timestamp = 1 << 0,          ///< Include timestamp / 包含时间戳
    SourceLocation = 1 << 1,     ///< Include source location (file, line, function) / 包含源代码位置
    ThreadId = 1 << 2,           ///< Include thread ID / 包含线程 ID
    ProcessId = 1 << 3,          ///< Include process ID / 包含进程 ID
    
    // Predefined combinations / 预定义组合
    Minimal = None,                                          ///< Minimal: only level and snapshot / 最小配置
    NoTimestamp = ThreadId | ProcessId,                      ///< Without timestamp / 无时间戳
    Standard = Timestamp | ThreadId | ProcessId,             ///< Standard: timestamp + IDs / 标准配置
    Debug = Timestamp | SourceLocation | ThreadId | ProcessId, ///< Debug: all fields / Debug 配置
    Full = Debug                                             ///< Full: same as Debug / 完整配置
};

/**
 * @brief Bitwise OR operator for LogEntryFeatures
 * @brief LogEntryFeatures 的按位或运算符
 */
constexpr LogEntryFeatures operator|(LogEntryFeatures a, LogEntryFeatures b) noexcept {
    return static_cast<LogEntryFeatures>(
        static_cast<uint32_t>(a) | static_cast<uint32_t>(b)
    );
}

/**
 * @brief Bitwise AND operator for LogEntryFeatures
 * @brief LogEntryFeatures 的按位与运算符
 */
constexpr LogEntryFeatures operator&(LogEntryFeatures a, LogEntryFeatures b) noexcept {
    return static_cast<LogEntryFeatures>(
        static_cast<uint32_t>(a) & static_cast<uint32_t>(b)
    );
}

/**
 * @brief Check if a feature is enabled
 * @brief 检查特性是否启用
 */
constexpr bool HasFeature(LogEntryFeatures features, LogEntryFeatures flag) noexcept {
    return (static_cast<uint32_t>(features) & static_cast<uint32_t>(flag)) != 0;
}

// ==============================================================================
// LogEntryTemplate - Compile-time Configurable LogEntry / 编译时可配置的 LogEntry
// ==============================================================================

/**
 * @brief Templated log entry with compile-time configurable fields
 * @brief 具有编译时可配置字段的模板化日志条目
 * 
 * @tparam Features Feature flags controlling which fields are included
 * @tparam Features 控制包含哪些字段的特性标志
 * 
 * This template uses C++20 features to achieve zero-overhead abstraction:
 * - Fields not enabled by Features are completely eliminated at compile time
 * - [[no_unique_address]] ensures empty types don't consume space
 * - std::monostate represents disabled fields with zero size overhead
 * 
 * 此模板使用 C++20 特性实现零开销抽象：
 * - Features 未启用的字段在编译时完全消除
 * - [[no_unique_address]] 确保空类型不占用空间
 * - std::monostate 表示禁用字段，零大小开销
 * 
 * Memory layout is optimized to avoid padding:
 * 内存布局经过优化以避免填充：
 * - 8-byte fields first (timestamp, pointers)
 * - 4-byte fields next (IDs, line)
 * - 1-byte fields last (level)
 * - Padding automatically calculated for alignment
 * 
 * Example usage:
 * 使用示例：
 * @code
 * // Minimal: only level and snapshot (no timestamp, no IDs)
 * // 最小配置：仅级别和快照（无时间戳、无 ID）
 * using MinimalEntry = LogEntryTemplate<LogEntryFeatures::Minimal>;
 * 
 * // Without timestamp: only IDs
 * // 无时间戳：仅 ID
 * using NoTimestampEntry = LogEntryTemplate<LogEntryFeatures::NoTimestamp>;
 * 
 * // Standard: timestamp + IDs (no source location)
 * // 标准配置：时间戳 + ID（无源代码位置）
 * using StandardEntry = LogEntryTemplate<LogEntryFeatures::Standard>;
 * 
 * // Debug: all fields including source location
 * // Debug 配置：包含源代码位置的所有字段
 * using DebugEntry = LogEntryTemplate<LogEntryFeatures::Debug>;
 * @endcode
 */
template<LogEntryFeatures Features = LogEntryFeatures::Standard>
struct LogEntryTemplate {
    // ==========================================================================
    // Compile-time Feature Detection / 编译时特性检测
    // ==========================================================================
    
    /// Check if timestamp is enabled / 检查是否启用时间戳
    static constexpr bool kHasTimestamp = HasFeature(Features, LogEntryFeatures::Timestamp);
    
    /// Check if source location is enabled / 检查是否启用源代码位置
    static constexpr bool kHasSourceLocation = HasFeature(Features, LogEntryFeatures::SourceLocation);
    
    /// Check if thread ID is enabled / 检查是否启用线程 ID
    static constexpr bool kHasThreadId = HasFeature(Features, LogEntryFeatures::ThreadId);
    
    /// Check if process ID is enabled / 检查是否启用进程 ID
    static constexpr bool kHasProcessId = HasFeature(Features, LogEntryFeatures::ProcessId);
    
    // ==========================================================================
    // Conditional Fields (using [[no_unique_address]]) / 条件字段
    // ==========================================================================
    
    /// Timestamp in nanoseconds (optional)
    /// 纳秒级时间戳（可选）
    [[no_unique_address]]
    std::conditional_t<kHasTimestamp, uint64_t, std::monostate> timestamp{};
    
    /// Source file name (optional, pointer to string literal)
    /// 源文件名（可选，指向字符串字面量的指针）
    [[no_unique_address]]
    std::conditional_t<kHasSourceLocation, const char*, std::monostate> file{};
    
    /// Function name (optional, pointer to string literal)
    /// 函数名（可选，指向字符串字面量的指针）
    [[no_unique_address]]
    std::conditional_t<kHasSourceLocation, const char*, std::monostate> function{};
    
    /// Thread ID (optional)
    /// 线程 ID（可选）
    [[no_unique_address]]
    std::conditional_t<kHasThreadId, uint32_t, std::monostate> threadId{};
    
    /// Process ID (optional)
    /// 进程 ID（可选）
    [[no_unique_address]]
    std::conditional_t<kHasProcessId, uint32_t, std::monostate> processId{};
    
    /// Line number (optional)
    /// 行号（可选）
    [[no_unique_address]]
    std::conditional_t<kHasSourceLocation, uint32_t, std::monostate> line{};
    
    // ==========================================================================
    // Required Fields / 必需字段
    // ==========================================================================
    
    /// Log level (always present)
    /// 日志级别（始终存在）
    internal::Level level{internal::Level::Info};
    
    // ==========================================================================
    // Padding Calculation / 填充计算
    // ==========================================================================
    
private:
    /// Calculate required padding to align snapshot to 8 bytes
    /// 计算所需的填充以将 snapshot 对齐到 8 字节
    static constexpr size_t CalculatePadding() {
        // Count bytes used by enabled fields
        // With [[no_unique_address]], std::monostate takes 0 bytes
        size_t usedBytes = 0;
        if constexpr (kHasTimestamp) usedBytes += sizeof(uint64_t);
        if constexpr (kHasSourceLocation) usedBytes += sizeof(const char*) * 2 + sizeof(uint32_t);
        if constexpr (kHasThreadId) usedBytes += sizeof(uint32_t);
        if constexpr (kHasProcessId) usedBytes += sizeof(uint32_t);
        usedBytes += sizeof(internal::Level);  // level is always present
        
        // Calculate padding needed to reach 8-byte alignment
        return (8 - (usedBytes % 8)) % 8;
    }
    
public:
    /// Padding bytes for alignment (size depends on enabled features)
    /// 对齐填充字节（大小取决于启用的特性）
    uint8_t padding[CalculatePadding()]{};
    
    // ==========================================================================
    // Binary Snapshot / 二进制快照
    // ==========================================================================
    
    /// Binary snapshot of log arguments (always present)
    /// 日志参数的二进制快照（始终存在）
    BinarySnapshot snapshot{};
    
    // ==========================================================================
    // Constructors / 构造函数
    // ==========================================================================
    
    /**
     * @brief Default constructor
     * @brief 默认构造函数
     */
    LogEntryTemplate() = default;
    
    // ==========================================================================
    // Accessor Methods with Compile-time Checks / 带编译时检查的访问器方法
    // ==========================================================================
    
    /**
     * @brief Get timestamp (only available if enabled)
     * @brief 获取时间戳（仅在启用时可用）
     */
    template<typename T = void>
    uint64_t GetTimestamp() const {
        static_assert(kHasTimestamp, "Timestamp feature not enabled. Use LogEntryFeatures::Timestamp.");
        if constexpr (kHasTimestamp) {
            return timestamp;
        }
        return 0;
    }
    
    /**
     * @brief Set timestamp (only available if enabled)
     * @brief 设置时间戳（仅在启用时可用）
     */
    template<typename T = void>
    void SetTimestamp(uint64_t ts) {
        static_assert(kHasTimestamp, "Timestamp feature not enabled. Use LogEntryFeatures::Timestamp.");
        if constexpr (kHasTimestamp) {
            timestamp = ts;
        }
    }
    
    /**
     * @brief Get source location (only available if enabled)
     * @brief 获取源代码位置（仅在启用时可用）
     */
    template<typename T = void>
    SourceLocation GetLocation() const {
        static_assert(kHasSourceLocation, "SourceLocation feature not enabled. Use LogEntryFeatures::SourceLocation.");
        if constexpr (kHasSourceLocation) {
            return SourceLocation{file, line, function};
        }
        return SourceLocation{};
    }
    
    /**
     * @brief Set source location (only available if enabled)
     * @brief 设置源代码位置（仅在启用时可用）
     */
    template<typename T = void>
    void SetLocation(const SourceLocation& loc) {
        static_assert(kHasSourceLocation, "SourceLocation feature not enabled. Use LogEntryFeatures::SourceLocation.");
        if constexpr (kHasSourceLocation) {
            file = loc.file;
            line = loc.line;
            function = loc.function;
        }
    }
    
    /**
     * @brief Get thread ID (only available if enabled)
     * @brief 获取线程 ID（仅在启用时可用）
     */
    template<typename T = void>
    uint32_t GetThreadId() const {
        static_assert(kHasThreadId, "ThreadId feature not enabled. Use LogEntryFeatures::ThreadId.");
        if constexpr (kHasThreadId) {
            return threadId;
        }
        return 0;
    }
    
    /**
     * @brief Set thread ID (only available if enabled)
     * @brief 设置线程 ID（仅在启用时可用）
     */
    template<typename T = void>
    void SetThreadId(uint32_t tid) {
        static_assert(kHasThreadId, "ThreadId feature not enabled. Use LogEntryFeatures::ThreadId.");
        if constexpr (kHasThreadId) {
            threadId = tid;
        }
    }
    
    /**
     * @brief Get process ID (only available if enabled)
     * @brief 获取进程 ID（仅在启用时可用）
     */
    template<typename T = void>
    uint32_t GetProcessId() const {
        static_assert(kHasProcessId, "ProcessId feature not enabled. Use LogEntryFeatures::ProcessId.");
        if constexpr (kHasProcessId) {
            return processId;
        }
        return 0;
    }
    
    /**
     * @brief Set process ID (only available if enabled)
     * @brief 设置进程 ID（仅在启用时可用）
     */
    template<typename T = void>
    void SetProcessId(uint32_t pid) {
        static_assert(kHasProcessId, "ProcessId feature not enabled. Use LogEntryFeatures::ProcessId.");
        if constexpr (kHasProcessId) {
            processId = pid;
        }
    }
    
    // ==========================================================================
    // Validation / 验证
    // ==========================================================================
    
    /**
     * @brief Check if entry is valid
     * @brief 检查条目是否有效
     * @return bool True if valid / 如果有效则返回 true
     * 
     * Validation rules depend on enabled features:
     * 验证规则取决于启用的特性：
     * - If timestamp enabled: must be non-zero
     * - If source location enabled: file and function must be non-null
     * - Level must be valid
     */
    bool IsValid() const {
        // Check timestamp if enabled
        if constexpr (kHasTimestamp) {
            if (timestamp == 0) {
                return false;
            }
        }
        
        // Check source location if enabled
        if constexpr (kHasSourceLocation) {
            if (file == nullptr || function == nullptr) {
                return false;
            }
        }
        
        // Level is always required
        return static_cast<uint8_t>(level) <= static_cast<uint8_t>(internal::Level::Off);
    }
};

// ==============================================================================
// Type Aliases for Common Configurations / 常用配置的类型别名
// ==============================================================================

/**
 * @brief Minimal log entry: only level and snapshot
 * @brief 最小日志条目：仅级别和快照
 * 
 * Use case: When you only need the log message and level, no metadata
 * 使用场景：只需要日志消息和级别，不需要元数据
 * Size: ~264 bytes (level + reserved + snapshot)
 */
using LogEntryMinimal = LogEntryTemplate<LogEntryFeatures::Minimal>;

/**
 * @brief Log entry without timestamp: only IDs
 * @brief 无时间戳的日志条目：仅 ID
 * 
 * Use case: When external system provides timestamps, or timestamps not needed
 * 使用场景：外部系统提供时间戳，或不需要时间戳
 * Size: ~272 bytes (IDs + level + reserved + snapshot)
 */
using LogEntryNoTimestamp = LogEntryTemplate<LogEntryFeatures::NoTimestamp>;

/**
 * @brief Standard log entry: timestamp + IDs (no source location)
 * @brief 标准日志条目：时间戳 + ID（无源代码位置）
 * 
 * Use case: Production logging with timestamp and process/thread tracking
 * 使用场景：生产环境日志，包含时间戳和进程/线程跟踪
 * Size: ~280 bytes (timestamp + IDs + level + reserved + snapshot)
 */
using LogEntryStandard = LogEntryTemplate<LogEntryFeatures::Standard>;

/**
 * @brief Debug log entry: all fields including source location
 * @brief Debug 日志条目：包含源代码位置的所有字段
 * 
 * Use case: Development and debugging with full context
 * 使用场景：开发和调试，包含完整上下文
 * Size: ~296 bytes (all fields + snapshot)
 */
using LogEntryDebug = LogEntryTemplate<LogEntryFeatures::Debug>;

/**
 * @brief Full log entry: same as Debug
 * @brief 完整日志条目：与 Debug 相同
 */
using LogEntryFull = LogEntryDebug;

// ==============================================================================
// Backward Compatibility Aliases / 向后兼容别名
// ==============================================================================

/**
 * @brief Release mode log entry (for backward compatibility)
 * @brief Release 模式日志条目（向后兼容）
 * 
 * Equivalent to LogEntryStandard: timestamp + IDs, no source location
 * 等同于 LogEntryStandard：时间戳 + ID，无源代码位置
 */
using LogEntryRelease = LogEntryStandard;

// ==============================================================================
// Compile-time Selection / 编译时选择
// ==============================================================================

/**
 * @brief LogEntry type alias selected at compile time based on build mode
 * @brief 根据构建模式在编译时选择的 LogEntry 类型别名
 * 
 * In Debug builds (NDEBUG not defined): Uses LogEntryDebug with full source location
 * In Release builds (NDEBUG defined): Uses LogEntryRelease (Standard) for optimal performance
 * 
 * 在 Debug 构建中（未定义 NDEBUG）：使用包含完整源代码位置的 LogEntryDebug
 * 在 Release 构建中（定义了 NDEBUG）：使用 LogEntryRelease（标准）以获得最佳性能
 */
#ifdef NDEBUG
using LogEntry = LogEntryRelease;
#else
using LogEntry = LogEntryDebug;
#endif

// ==============================================================================
// Helper Functions / 辅助函数
// ==============================================================================

/**
 * @brief Get the size of LogEntry at compile time
 * @brief 在编译时获取 LogEntry 的大小
 * @return constexpr size_t The size in bytes / 大小（字节）
 */
constexpr size_t GetLogEntrySize() {
    return sizeof(LogEntry);
}

/**
 * @brief Get the size of a specific LogEntry configuration
 * @brief 获取特定 LogEntry 配置的大小
 * @tparam Features Feature flags / 特性标志
 * @return constexpr size_t The size in bytes / 大小（字节）
 */
template<LogEntryFeatures Features>
constexpr size_t GetLogEntrySizeFor() {
    return sizeof(LogEntryTemplate<Features>);
}

/**
 * @brief Check if current build is Debug mode
 * @brief 检查当前构建是否为 Debug 模式
 * @return constexpr bool True if Debug mode / 如果是 Debug 模式则返回 true
 */
constexpr bool IsDebugBuild() {
#ifdef NDEBUG
    return false;
#else
    return true;
#endif
}

/**
 * @brief Check if current build is Release mode
 * @brief 检查当前构建是否为 Release 模式
 * @return constexpr bool True if Release mode / 如果是 Release 模式则返回 true
 */
constexpr bool IsReleaseBuild() {
    return !IsDebugBuild();
}

/**
 * @brief Get feature description string
 * @brief 获取特性描述字符串
 * @param features Feature flags / 特性标志
 * @return const char* Description string / 描述字符串
 */
constexpr const char* GetFeatureDescription(LogEntryFeatures features) {
    if (features == LogEntryFeatures::Minimal) {
        return "Minimal (level + snapshot only)";
    } else if (features == LogEntryFeatures::NoTimestamp) {
        return "NoTimestamp (IDs only)";
    } else if (features == LogEntryFeatures::Standard) {
        return "Standard (timestamp + IDs)";
    } else if (features == LogEntryFeatures::Debug) {
        return "Debug (all fields)";
    } else {
        return "Custom";
    }
}

} // namespace oneplog

