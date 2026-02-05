/**
 * @file common.hpp
 * @brief Common definitions (Level, Mode, ErrorCode)
 * @文件 common.hpp
 * @简述 通用定义（日志级别、运行模式、错误码）
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include <string_view>
#include <chrono>
#include <thread>
#include <vector>

// Check if the standard library supports hardware interference size (C++17) to prevent false sharing
// 检查标准库是否支持硬件干扰尺寸（C++17），用于防止伪共享
#if defined(__cpp_lib_hardware_interference_size) && __cpp_lib_hardware_interference_size >= 201703L
#include <new> // Required for std::hardware_destructive_interference_size / 包含此头文件以使用该特性
#endif

// Platform-specific includes: Handle differences between Linux and macOS (Darwin)
// 平台特定包含：处理 Linux 和 macOS (Darwin) 之间的差异
#ifdef __linux__
// Linux specific: eventfd is used for efficient inter-process/thread notification
// Linux 特定：eventfd 用于高效的进程/线程间通知
#include <sys/eventfd.h>
#include <unistd.h>
#include <poll.h>
#elif defined(__APPLE__)
// Apple specific: Use Grand Central Dispatch (GCD) for signaling on macOS/iOS
// Apple 特定：在 macOS/iOS 上使用 GCD 进行信号通知
#include <dispatch/dispatch.h>
#include <unistd.h>
#endif

namespace oneplog::internal {
// ==============================================================================
// Cache Line Size / 缓存行大小
// ==============================================================================
/**
 * @brief Log level enumeration (compatible with syslog/spdlog)
 * @brief 获取系统缓存行大小
 */
// Check if the standard library provides hardware interference size constants
// 检查标准库是否提供了硬件干扰尺寸常量
#if defined(__cpp_lib_hardware_interference_size) && __cpp_lib_hardware_interference_size >= 201703L
/**
 * @brief Use the hardware-specific cache line size to prevent false sharing
 * @brief 使用硬件特定的缓存行尺寸以防止伪共享
 */
constexpr std::size_t kCacheLineSize = std::hardware_destructive_interference_size;
#else
/**
 * @brief Fallback to 64 bytes if the compiler/platform doesn't support the C++17 constant.
 * @brief 64 bytes is the standard cache line size for most modern x86_64 and ARMv8 CPUs.
 * @brief 如果编译器或平台不支持 C++17 常量，则回退到 64 字节。
 * @brief 64 字节是大多数现代 x86_64 和 ARMv8 (如 M4 Pro/RK3588) CPU 的标准缓存行尺寸。
 */
constexpr std::size_t kCacheLineSize = 64;
#endif

// ==============================================================================
// CacheLineAligned / 缓存行对齐
// ==============================================================================

/**
 * @brief A wrapper that aligns type T to the hardware cache line boundary.
 * @brief 将类型 T 对齐到硬件缓存行边界的包装器。
 * * @details This prevents "False Sharing" where multiple threads modify different
 * variables residing on the same cache line, causing significant performance degradation.
 * @details 这可以防止“伪共享”现象，即多个线程修改位于同一缓存行上的不同变量，从而导致严重的性能下降。
 * * @tparam T The underlying type to be aligned / 需要对齐的底层类型
 */


template <typename T>
struct alignas(kCacheLineSize) CacheLineAligned {
    /**
     * @brief The actual value being stored.
     * @brief 实际存储的数值。
     */
    T value;

    /**
     * @brief Default constructor.
     * @brief 默认构造函数。
     */
    CacheLineAligned() = default;

    /**
     * @brief Construct from a constant reference.
     * @brief 通过常量引用进行构造。
     */
    explicit CacheLineAligned(const T& v) : value(v) {
    }

    /**
     * @brief Construct by moving a value.
     * @brief 通过移动语义进行构造。
     */
    explicit CacheLineAligned(T&& v) : value(std::move(v)) {
    }

    /**
     * @brief Implicit conversion to reference of T.
     * @brief 隐式转换为 T 的引用。
     */
    operator T&() noexcept { return value; }

    /**
     * @brief Implicit conversion to constant reference of T.
     * @brief 隐式转换为 T 的常量引用。
     */
    operator const T&() const noexcept { return value; }

    /**
     * @brief Member access operator.
     * @brief 成员访问运算符。
     */
    T* operator->() noexcept { return &value; }

    /**
     * @brief Constant member access operator.
     * @brief 常量成员访问运算符。
     */
    const T* operator->() const noexcept { return &value; }

    /**
     * @brief In-place constructor.
     * @brief 原地构造函数，直接将参数转发给 T 的构造函数。
     */
    template <typename... Args>
    explicit CacheLineAligned(Args&&... args)
        : value(std::forward<Args>(args)...) {
    }

    /**
     * @brief Ensure the type T itself does not exceed a single cache line.
     * @brief 确保类型 T 本身的大小不会超过单个缓存行。
     * * @note If T is larger than kCacheLineSize, the alignment might not prevent
     * interference with adjacent data as effectively.
     * @note 如果 T 大于 kCacheLineSize，对齐可能无法像预期那样有效地防止与相邻数据的干扰。
     */
    static_assert(sizeof(T) <= kCacheLineSize, "Type T is larger than cache line size");
};

// ==============================================================================
// Max PID(only Linux) / 最大PID数值（仅Linux）
// ==============================================================================

#ifdef __linux__
#ifdef SET_MAX_PID
constexpr size_t kLinuxMaxPID = SET_MAX_PID;
#else
constexpr size_t kLinuxMaxPID = 32768;
#endif
#endif


// ==============================================================================
// Log Level / 日志级别
// ==============================================================================

/**
 * @brief Log level enumeration (compatible with syslog/spdlog)
 * @brief 日志级别枚举（兼容 syslog/spdlog 标准）
 */
enum class Level : uint8_t {
    /// Alert: Action must be taken immediately (e.g., database corruption)
    /// 警报：必须立即采取行动（如核心数据库损坏）
    Alert = 0,

    /// Critical: Critical conditions (e.g., hardware device failure)
    /// 临界：严重情况（如主存储设备失效）
    Critical = 1,

    /// Error: Runtime errors that do not require immediate action
    /// 错误：运行时错误，不需要立即采取行动但必须记录
    Error = 2,

    /// Warning: Warning messages for potential future issues
    /// 警告：警告信息，如果不处理可能导致错误
    Warning = 3,

    /// Notice: Normal but significant conditions
    /// 通知：正常但具有重大意义的情况
    Notice = 4,

    /// Informational: Routine system messages (e.g., service start/stop)
    /// 信息：普通的系统消息（如进程启动/停止）
    Informational = 5,

    /// Debugging: Detailed information for development and troubleshooting
    /// 调试：仅在开发和排查故障时使用的详细信息
    Debugging = 6,

    /// Trace: Most granular tracing information
    /// 追踪：最详细的跟踪信息
    Trace = 7,

    /// Off: Disable all logging output
    /// 关闭：不输出任何日志
    Off = 8
};

/**
 * @brief Level name display style
 * @brief 日志级别名称显示样式
 */
enum class LevelNameStyle : uint8_t {
    /// Full name style (e.g., "Critical", "Warning", "Informational")
    /// 全称样式（例如："Critical", "Warning", "Informational"）
    Full = 0,

    /// 4-character short name for aligned output (e.g., "CRIT", "WARN", "INFO")
    /// 4 字符缩写样式，方便对齐输出（例如："CRIT", "WARN", "INFO"）
    Short4 = 1,

    /// 1-character minimalist name (e.g., "C", "W", "I")
    /// 1 字符极简样式（例如："C", "W", "I"）
    Short1 = 2
};

/**
 * @brief Convert log level to string representation
 * @brief 将日志级别转换为字符串表示
 * @param level The log level to convert / 待转换的日志级别
 * @param style The display style of the level name / 级别名称的显示样式
 * @return std::string_view The string representation of the level / 级别的字符串表示
 */
constexpr std::string_view LevelToString(Level level,
                                         const LevelNameStyle style = LevelNameStyle::Short4) noexcept {
    // Standard full names for each level
    // 各个级别的标准全称
    constexpr std::string_view kFullNames[] = {
        "Alert", "Critical", "Error", "Warning", "Notice", "Informational",
        "Debugging", "Trace", "Off"
    };

    // 4-character aligned names for consistent formatting
    // 4 字符对齐名称，用于保持格式一致性
    constexpr std::string_view kShort4Names[] = {
        "ALER", "CRIT", "ERRO", "WARN", "NOTI", "INFO", "DBUG", "TRAC", "OFF"
    };

    // Minimalist 1-character names for high-density logging
    // 极简 1 字符名称，用于高密度日志场景
    constexpr std::string_view kShort1Names[] = {
        "A", "C", "E", "W", "N", "I", "D", "T", "O"
    };

    const auto idx = static_cast<size_t>(level);

    // Handle invalid level index by returning 'Unknown' in requested style
    // 处理无效的级别索引，按请求的样式返回“未知”标识
    if (idx > static_cast<size_t>(Level::Off)) {
        switch (style) {
            case LevelNameStyle::Full:
                return "Unknown";
            case LevelNameStyle::Short4:
                return "UNKN";
            case LevelNameStyle::Short1:
                return "U";
            default:
                return "UNKN";
        }
    }

    // Return the corresponding string constant based on the style
    // 根据指定的样式返回对应的字符串常量
    switch (style) {
        case LevelNameStyle::Full:
            return kFullNames[idx];
        case LevelNameStyle::Short4:
            return kShort4Names[idx];
        case LevelNameStyle::Short1:
            return kShort1Names[idx];
        default:
            return kShort4Names[idx];
    }
}

/**
 * @brief Convert string to log level
 * @brief 将字符串转换为日志级别
 * @param name The string representation of the level / 级别的字符串表示
 * @return Level The corresponding log level / 对应的日志级别
 */
constexpr Level StringToLevel(const std::string_view name) noexcept {
    if (name == "alert" || name == "ALERT" || name == "ALER" || name == "A") {
        return Level::Alert;
    }
    if (name == "critical" || name == "CRITICAL" || name == "CRIT" || name == "C") {
        return Level::Critical;
    }
    if (name == "error" || name == "ERROR" || name == "ERRO" || name == "E") {
        return Level::Error;
    }
    if (name == "warning" || name == "WARNING" || name == "WARN" || name == "W") {
        return Level::Warning;
    }
    if (name == "notice" || name == "NOTICE" || name == "NOTI" || name == "N") {
        return Level::Notice;
    }
    if (name == "informational" || name == "INFORMATIONAL" || name == "INFO" || name == "I") {
        return Level::Informational;
    }
    if (name == "debugging" || name == "DEBUGGING" || name == "DBUG" || name == "D") {
        return Level::Debugging;
    }
    if (name == "trace" || name == "TRACE" || name == "TRAC" || name == "T") {
        return Level::Trace;
    }
    return Level::Off;
}

/**
 * @brief Check if a log level should be logged (compile-time)
 * @brief 检查日志级别是否应该记录（编译时）
 * @tparam logLevel The level of the current log message / 当前日志消息的级别
 * @tparam currentLevel The current threshold level of the logger / 日志器当前的阈值级别
 * @return bool True if it should be logged / 如果应该记录则返回 true
 */
template <Level logLevel, Level currentLevel>
constexpr bool ShouldLog() noexcept {
    return static_cast<uint8_t>(logLevel) <= static_cast<uint8_t>(currentLevel);
}

constexpr size_t kLevelCount        = 8; ///< Number of log levels (excluding Off) / 日志级别数量（不含 Off）
constexpr size_t kLevelCountWithOff = 9; ///< Total number of log levels (including Off) / 总日志级别数量（含 Off）


// ==============================================================================
// Logger Operating Mode / 日志运行模式
// ==============================================================================

/**
 * @brief Operating mode enumeration
 * @brief 运行模式枚举
 */
enum class Mode : uint8_t {
    Sync = 0,  ///< Synchronous mode / 同步模式
    Async = 1, ///< Asynchronous mode / 异步模式
    MProc = 2  ///< Multi-process mode / 多进程模式
};

/**
 * @brief Convert mode to string representation
 * @brief 将模式转换为字符串表示
 * @param mode The operating mode / 运行模式
 * @return std::string_view The string representation of the mode / 模式的字符串表示
 */
constexpr std::string_view ModeToString(const Mode mode) noexcept {
    switch (mode) {
        case Mode::Sync:
            return "sync";
        case Mode::Async:
            return "async";
        case Mode::MProc:
            return "mproc";
        default:
            return "unknown";
    }
}

/**
 * @brief Convert string to mode
 * @brief 将字符串转换为模式
 * @param name The string representation of the mode / 模式的字符串表示
 * @return Mode The corresponding operating mode / 对应的运行模式
 */
constexpr Mode StringToMode(std::string_view name) noexcept {
    if (name == "sync" || name == "Sync" || name == "SYNC") {
        return Mode::Sync;
    }
    if (name == "async" || name == "Async" || name == "ASYNC") {
        return Mode::Async;
    }
    if (name == "mproc" || name == "MProc" || name == "MPROC" || name == "multiprocess" ||
        name == "MultiProcess" || name == "MULTIPROCESS") {
        return Mode::MProc;
    }
    return Mode::Async;
}

// ==============================================================================
// Queue Full Policy / 队列满策略
// ==============================================================================

/**
 * @brief Queue full policy enumeration
 * @brief 队列满策略枚举
 * 
 * Defines the behavior when the ring buffer queue is full.
 * 定义当环形缓冲区队列满时的行为。
 */
enum class QueueFullPolicy : uint8_t {
    /// Block: Block the producer until space is available
    /// 阻塞：阻塞生产者直到有可用空间
    Block = 0,

    /// DropNewest: Drop the newest log entry and increment dropped count
    /// 丢弃最新：丢弃新日志条目并增加丢弃计数
    DropNewest = 1,

    /// DropOldest: Drop the oldest log entry and increment dropped count
    /// 丢弃最旧：丢弃最旧日志条目并增加丢弃计数
    DropOldest = 2
};

/**
 * @brief Convert queue full policy to string representation
 * @brief 将队列满策略转换为字符串表示
 * @param policy The queue full policy / 队列满策略
 * @return std::string_view The string representation of the policy / 策略的字符串表示
 */
constexpr std::string_view QueueFullPolicyToString(const QueueFullPolicy policy) noexcept {
    switch (policy) {
        case QueueFullPolicy::Block:
            return "Block";
        case QueueFullPolicy::DropNewest:
            return "DropNewest";
        case QueueFullPolicy::DropOldest:
            return "DropOldest";
        default:
            return "Unknown";
    }
}

/**
 * @brief Convert string to queue full policy
 * @brief 将字符串转换为队列满策略
 * @param name The string representation of the policy / 策略的字符串表示
 * @return QueueFullPolicy The corresponding queue full policy / 对应的队列满策略
 */
constexpr QueueFullPolicy StringToQueueFullPolicy(std::string_view name) noexcept {
    if (name == "block" || name == "Block" || name == "BLOCK") {
        return QueueFullPolicy::Block;
    }
    if (name == "dropnewest" || name == "DropNewest" || name == "DROPNEWEST" ||
        name == "drop_newest" || name == "DROP_NEWEST") {
        return QueueFullPolicy::DropNewest;
    }
    if (name == "dropoldest" || name == "DropOldest" || name == "DROPOLDEST" ||
        name == "drop_oldest" || name == "DROP_OLDEST") {
        return QueueFullPolicy::DropOldest;
    }
    return QueueFullPolicy::DropNewest; // Default policy
}

// ==============================================================================
// Slot State / 槽位状态
// ==============================================================================

/**
 * @brief Slot state enumeration for ring buffer
 * @brief 环形缓冲区槽位状态枚举
 * 
 * Defines the state of each slot in the ring buffer for lock-free operations.
 * 定义环形缓冲区中每个槽位的状态，用于无锁操作。
 */
enum class SlotState : uint8_t {
    /// Empty: Slot is available for writing
    /// 空闲：槽位可用于写入
    Empty = 0,

    /// Writing: Slot is being written by a producer
    /// 写入中：槽位正在被生产者写入
    Writing = 1,

    /// Ready: Slot contains data ready for reading
    /// 就绪：槽位包含可供读取的数据
    Ready = 2,

    /// Reading: Slot is being read by a consumer
    /// 读取中：槽位正在被消费者读取
    Reading = 3
};

/**
 * @brief Convert slot state to string representation
 * @brief 将槽位状态转换为字符串表示
 * @param state The slot state / 槽位状态
 * @return std::string_view The string representation of the state / 状态的字符串表示
 */
constexpr std::string_view SlotStateToString(const SlotState state) noexcept {
    switch (state) {
        case SlotState::Empty:
            return "Empty";
        case SlotState::Writing:
            return "Writing";
        case SlotState::Ready:
            return "Ready";
        case SlotState::Reading:
            return "Reading";
        default:
            return "Unknown";
    }
}

// ==============================================================================
// WFC State / WFC 状态
// ==============================================================================

/**
 * @brief WFC (Wait For Completion) state enumeration
 * @brief WFC（等待完成）状态枚举
 * 
 * Defines the state of WFC mechanism for each log entry.
 * 定义每个日志条目的 WFC 机制状态。
 */
enum class WFCState : uint8_t {
    /// None: WFC is not enabled for this entry
    /// 无：此条目未启用 WFC
    None = 0,

    /// Enabled: WFC is enabled, waiting for completion
    /// 启用：WFC 已启用，等待完成
    Enabled = 1,

    /// Completed: WFC operation has completed
    /// 完成：WFC 操作已完成
    Completed = 2
};

/**
 * @brief Convert WFC state to string representation
 * @brief 将 WFC 状态转换为字符串表示
 * @param state The WFC state / WFC 状态
 * @return std::string_view The string representation of the state / 状态的字符串表示
 */
constexpr std::string_view WFCStateToString(const WFCState state) noexcept {
    switch (state) {
        case WFCState::None:
            return "None";
        case WFCState::Enabled:
            return "Enabled";
        case WFCState::Completed:
            return "Completed";
        default:
            return "Unknown";
    }
}

// ==============================================================================
// Consumer State / 消费者状态
// ==============================================================================

/**
 * @brief Consumer state enumeration for writer/pipeline threads
 * @brief 输出/管道线程的消费者状态枚举
 * 
 * Defines the state of consumer threads for polling and backoff strategy.
 * 定义消费者线程的状态，用于轮询和回避策略。
 */
enum class ConsumerState : uint8_t {
    /// Active: Consumer is actively processing entries
    /// 活跃：消费者正在积极处理条目
    Active = 0,

    /// Waiting: Consumer is waiting for notification
    /// 等待：消费者正在等待通知
    Waiting = 1
};

/**
 * @brief Convert consumer state to string representation
 * @brief 将消费者状态转换为字符串表示
 * @param state The consumer state / 消费者状态
 * @return std::string_view The string representation of the state / 状态的字符串表示
 */
constexpr std::string_view ConsumerStateToString(const ConsumerState state) noexcept {
    switch (state) {
        case ConsumerState::Active:
            return "Active";
        case ConsumerState::Waiting:
            return "Waiting";
        default:
            return "Unknown";
    }
}

// ==============================================================================
// Error Code / 错误码
// ==============================================================================

/**
 * @brief Error code enumeration
 * @brief 错误码枚举
 */
enum class ErrorCode : int32_t {
    /// Success / 成功
    Success = 0,

    // Memory errors (100-199) / 内存错误
    MemoryAllocationFailed = 100, ///< Memory allocation failed / 内存分配失败
    MemoryPoolExhausted = 101,    ///< Memory pool exhausted / 内存池耗尽
    BufferOverflow = 102,         ///< Buffer overflow / 缓冲区溢出
    BufferUnderflow = 103,        ///< Buffer underflow / 缓冲区下溢

    // Queue errors (200-299) / 队列错误
    QueueFull = 200,     ///< Queue is full / 队列已满
    QueueEmpty = 201,    ///< Queue is empty / 队列为空
    QueueSlotBusy = 202, ///< Queue slot is busy / 队列槽位忙
    QueueTimeout = 203,  ///< Queue operation timeout / 队列操作超时
    QueueClosed = 204,   ///< Queue is closed / 队列已关闭

    // File errors (300-399) / 文件错误
    FileOpenFailed = 300,   ///< Failed to open file / 打开文件失败
    FileWriteFailed = 301,  ///< Failed to write to file / 写入文件失败
    FileReadFailed = 302,   ///< Failed to read from file / 读取文件失败
    FileRotateFailed = 303, ///< Failed to rotate file / 文件滚动失败
    FileCloseFailed = 304,  ///< Failed to close file / 关闭文件失败
    FileFlushFailed = 305,  ///< Failed to flush file / 刷新文件缓冲区失败

    // Network errors (400-499) / 网络错误
    NetworkConnectFailed = 400, ///< Failed to connect to network / 网络连接失败
    NetworkSendFailed = 401,    ///< Failed to send data / 发送数据失败
    NetworkReceiveFailed = 402, ///< Failed to receive data / 接收数据失败
    NetworkDisconnected = 403,  ///< Network disconnected / 网络已断开
    NetworkTimeout = 404,       ///< Network operation timeout / 网络操作超时
    NetworkBindFailed = 405,    ///< Failed to bind network address / 绑定网络地址失败

    // Shared memory errors (500-599) / 共享内存错误
    SharedMemoryCreateFailed = 500,    ///< Failed to create shared memory / 创建共享内存失败
    SharedMemoryAttachFailed = 501,    ///< Failed to attach shared memory / 挂载共享内存失败
    SharedMemoryDetachFailed = 502,    ///< Failed to detach shared memory / 卸载共享内存失败
    SharedMemoryCorrupted = 503,       ///< Shared memory corrupted / 共享内存损坏
    SharedMemoryVersionMismatch = 504, ///< Shared memory version mismatch / 共享内存版本不匹配

    // Configuration errors (600-699) / 配置错误
    ConfigParseError = 600,      ///< Failed to parse configuration / 解析配置失败
    ConfigInvalidValue = 601,    ///< Invalid configuration value / 无效的配置值
    ConfigMissingRequired = 602, ///< Missing required configuration / 缺少必要的配置项
    ConfigFileNotFound = 603,    ///< Configuration file not found / 找不到配置文件

    // Thread errors (700-799) / 线程错误
    ThreadCreateFailed = 700,   ///< Failed to create thread / 创建线程失败
    ThreadJoinFailed = 701,     ///< Failed to join thread / 等待线程结束失败
    ThreadAlreadyRunning = 702, ///< Thread is already running / 线程已在运行中
    ThreadNotRunning = 703,     ///< Thread is not running / 线程未运行

    // General errors (900-999) / 通用错误
    InvalidArgument = 900,    ///< Invalid argument / 参数无效
    NotInitialized = 901,     ///< System not initialized / 系统未初始化
    AlreadyInitialized = 902, ///< System already initialized / 系统已初始化
    NotSupported = 903,       ///< Feature not supported / 功能不支持
    InternalError = 999       ///< Internal system error / 系统内部错误
};

/**
 * @brief Convert error code to string representation
 * @brief 将错误码转换为字符串表示
 * @param code The error code / 错误码
 * @return std::string_view The string representation of the error code / 错误码的字符串表示
 */
constexpr std::string_view ErrorCodeToString(const ErrorCode code) noexcept {
    switch (code) {
        case ErrorCode::Success:
            return "Success";
        case ErrorCode::MemoryAllocationFailed:
            return "MemoryAllocationFailed";
        case ErrorCode::MemoryPoolExhausted:
            return "MemoryPoolExhausted";
        case ErrorCode::BufferOverflow:
            return "BufferOverflow";
        case ErrorCode::BufferUnderflow:
            return "BufferUnderflow";
        case ErrorCode::QueueFull:
            return "QueueFull";
        case ErrorCode::QueueEmpty:
            return "QueueEmpty";
        case ErrorCode::QueueSlotBusy:
            return "QueueSlotBusy";
        case ErrorCode::QueueTimeout:
            return "QueueTimeout";
        case ErrorCode::QueueClosed:
            return "QueueClosed";
        case ErrorCode::FileOpenFailed:
            return "FileOpenFailed";
        case ErrorCode::FileWriteFailed:
            return "FileWriteFailed";
        case ErrorCode::FileReadFailed:
            return "FileReadFailed";
        case ErrorCode::FileRotateFailed:
            return "FileRotateFailed";
        case ErrorCode::FileCloseFailed:
            return "FileCloseFailed";
        case ErrorCode::FileFlushFailed:
            return "FileFlushFailed";
        case ErrorCode::NetworkConnectFailed:
            return "NetworkConnectFailed";
        case ErrorCode::NetworkSendFailed:
            return "NetworkSendFailed";
        case ErrorCode::NetworkReceiveFailed:
            return "NetworkReceiveFailed";
        case ErrorCode::NetworkDisconnected:
            return "NetworkDisconnected";
        case ErrorCode::NetworkTimeout:
            return "NetworkTimeout";
        case ErrorCode::NetworkBindFailed:
            return "NetworkBindFailed";
        case ErrorCode::SharedMemoryCreateFailed:
            return "SharedMemoryCreateFailed";
        case ErrorCode::SharedMemoryAttachFailed:
            return "SharedMemoryAttachFailed";
        case ErrorCode::SharedMemoryDetachFailed:
            return "SharedMemoryDetachFailed";
        case ErrorCode::SharedMemoryCorrupted:
            return "SharedMemoryCorrupted";
        case ErrorCode::SharedMemoryVersionMismatch:
            return "SharedMemoryVersionMismatch";
        case ErrorCode::ConfigParseError:
            return "ConfigParseError";
        case ErrorCode::ConfigInvalidValue:
            return "ConfigInvalidValue";
        case ErrorCode::ConfigMissingRequired:
            return "ConfigMissingRequired";
        case ErrorCode::ConfigFileNotFound:
            return "ConfigFileNotFound";
        case ErrorCode::ThreadCreateFailed:
            return "ThreadCreateFailed";
        case ErrorCode::ThreadJoinFailed:
            return "ThreadJoinFailed";
        case ErrorCode::ThreadAlreadyRunning:
            return "ThreadAlreadyRunning";
        case ErrorCode::ThreadNotRunning:
            return "ThreadNotRunning";
        case ErrorCode::InvalidArgument:
            return "InvalidArgument";
        case ErrorCode::NotInitialized:
            return "NotInitialized";
        case ErrorCode::AlreadyInitialized:
            return "AlreadyInitialized";
        case ErrorCode::NotSupported:
            return "NotSupported";
        case ErrorCode::InternalError:
            return "InternalError";
        default:
            return "UnknownError";
    }
}

/**
 * @brief Check if error code indicates success
 * @brief 检查错误码是否表示成功
 * @param code The error code to check / 待检查的错误码
 * @return bool True if successful / 如果成功则返回 true
 */
constexpr bool IsSuccess(const ErrorCode code) noexcept {
    return code == ErrorCode::Success;
}

/**
 * @brief Check if error code indicates failure
 * @brief 检查错误码是否表示失败
 * @param code The error code to check / 待检查的错误码
 * @return bool True if it is an error / 如果是错误则返回 true
 */
constexpr bool IsError(const ErrorCode code) noexcept {
    return code != ErrorCode::Success;
}
} // namespace oneplog