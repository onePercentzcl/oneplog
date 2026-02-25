/**
 * @file common.hpp
 * @brief Common definitions for onePlog (Level, Mode, ErrorCode)
 * @brief onePlog 通用定义（日志级别、运行模式、错误码）
 *
 * This file contains fundamental types and constants used throughout the library:
 * - Level: Log severity levels (Trace, Debug, Info, Warn, Error, Critical, Off)
 * - Mode: Operating modes (Sync, Async, MProc)
 * - ErrorCode: Error codes for various failure scenarios
 *
 * 此文件包含整个库使用的基本类型和常量：
 * - Level：日志严重级别（Trace、Debug、Info、Warn、Error、Critical、Off）
 * - Mode：运行模式（Sync、Async、MProc）
 * - ErrorCode：各种失败场景的错误码
 *
 * @note All types in this file are designed to be lightweight and constexpr-friendly.
 * @note 此文件中的所有类型都设计为轻量级且支持 constexpr。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#ifndef ONEPLOG_COMMON_HPP
#define ONEPLOG_COMMON_HPP

#include <cstdint>
#include <string_view>

namespace oneplog {

// ==============================================================================
// Log Level / 日志级别
// ==============================================================================

/**
 * @brief Log level enumeration (compatible with spdlog)
 * @brief 日志级别枚举（兼容 spdlog）
 *
 * Log levels are ordered by severity from lowest (Trace) to highest (Critical).
 * The Off level is used to disable logging entirely.
 *
 * 日志级别按严重程度从低（Trace）到高（Critical）排序。
 * Off 级别用于完全禁用日志记录。
 *
 * @note Level values are designed to be compatible with spdlog for easy migration.
 * @note 级别值设计为与 spdlog 兼容，便于迁移。
 */
enum class Level : uint8_t {
    Trace = 0,     ///< Most detailed tracing info (verbose debugging) / 最详细的跟踪信息（详细调试）
    Debug = 1,     ///< Debug information (development use) / 调试信息（开发使用）
    Info = 2,      ///< General information (normal operation) / 一般信息（正常运行）
    Warn = 3,      ///< Warning messages (potential issues) / 警告信息（潜在问题）
    Error = 4,     ///< Error messages (recoverable errors) / 错误信息（可恢复错误）
    Critical = 5,  ///< Critical errors (severe failures) / 严重错误（严重故障）
    Off = 6        ///< Logging disabled / 关闭日志
};

/**
 * @brief Level name display style
 * @brief 日志级别名称显示样式
 */
enum class LevelNameStyle : uint8_t {
    Full,    ///< Full name: "trace", "debug", "info", "warn", "error", "critical"
    Short4,  ///< 4-char name: "TRAC", "DBUG", "INFO", "WARN", "ERRO", "CRIT"
    Short1   ///< 1-char name: "T", "D", "I", "W", "E", "C"
};

/**
 * @brief Convert log level to string representation
 * @brief 将日志级别转换为字符串表示
 */
constexpr std::string_view LevelToString(Level level,
                                          LevelNameStyle style = LevelNameStyle::Short4) noexcept {
    constexpr std::string_view kFullNames[] = {"trace", "debug", "info",  "warn",
                                                "error", "critical", "off"};
    constexpr std::string_view kShort4Names[] = {"TRAC", "DBUG", "INFO", "WARN",
                                                  "ERRO", "CRIT", "OFF"};
    constexpr std::string_view kShort1Names[] = {"T", "D", "I", "W", "E", "C", "O"};

    const auto idx = static_cast<size_t>(level);
    if (idx > static_cast<size_t>(Level::Off)) {
        return "UNKN";
    }

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
 */
constexpr Level StringToLevel(std::string_view name) noexcept {
    if (name == "trace" || name == "TRACE" || name == "TRAC" || name == "T") {
        return Level::Trace;
    }
    if (name == "debug" || name == "DEBUG" || name == "DBUG" || name == "D") {
        return Level::Debug;
    }
    if (name == "info" || name == "INFO" || name == "I") {
        return Level::Info;
    }
    if (name == "warn" || name == "WARN" || name == "warning" || name == "WARNING" || name == "W") {
        return Level::Warn;
    }
    if (name == "error" || name == "ERROR" || name == "ERRO" || name == "E") {
        return Level::Error;
    }
    if (name == "critical" || name == "CRITICAL" || name == "CRIT" || name == "C") {
        return Level::Critical;
    }
    if (name == "off" || name == "OFF" || name == "O") {
        return Level::Off;
    }
    return Level::Off;
}

/**
 * @brief Check if a log level should be logged (compile-time)
 * @brief 检查日志级别是否应该记录（编译时）
 */
template <Level logLevel, Level currentLevel>
constexpr bool ShouldLog() noexcept {
    return static_cast<uint8_t>(logLevel) >= static_cast<uint8_t>(currentLevel);
}

constexpr size_t kLevelCount = 6;         ///< Number of log levels (excluding Off)
constexpr size_t kLevelCountWithOff = 7;  ///< Total number of log levels (including Off)

// ==============================================================================
// Operating Mode / 运行模式
// ==============================================================================

/**
 * @brief Operating mode enumeration
 * @brief 运行模式枚举
 *
 * Defines the three operating modes supported by onePlog:
 * - Sync: Direct synchronous logging (lowest latency, blocking)
 * - Async: Asynchronous logging with background thread (non-blocking)
 * - MProc: Multi-process logging via shared memory (cross-process)
 *
 * 定义 onePlog 支持的三种运行模式：
 * - Sync：直接同步日志（最低延迟，阻塞）
 * - Async：带后台线程的异步日志（非阻塞）
 * - MProc：通过共享内存的多进程日志（跨进程）
 *
 * @note Mode selection affects performance characteristics and resource usage.
 * @note 模式选择影响性能特性和资源使用。
 */
enum class Mode : uint8_t {
    Sync = 0,   ///< Synchronous mode - logs directly in calling thread / 同步模式 - 在调用线程中直接记录
    Async = 1,  ///< Asynchronous mode - logs via background thread / 异步模式 - 通过后台线程记录
    MProc = 2   ///< Multi-process mode - logs via shared memory / 多进程模式 - 通过共享内存记录
};

/**
 * @brief Convert mode to string representation
 * @brief 将模式转换为字符串表示
 */
constexpr std::string_view ModeToString(Mode mode) noexcept {
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
// Error Code / 错误码
// ==============================================================================

/**
 * @brief Error code enumeration
 * @brief 错误码枚举
 *
 * @note Currently unused - reserved for future error handling implementation.
 * @note 当前未使用 - 预留给将来的错误处理实现。
 *
 * Comprehensive error codes organized by category:
 * - 0: Success
 * - 100-199: Memory errors (allocation, pool, buffer)
 * - 200-299: Queue errors (full, empty, timeout)
 * - 300-399: File errors (open, write, rotate)
 * - 400-499: Network errors (connect, send, receive)
 * - 500-599: Shared memory errors (create, attach, corrupt)
 * - 600-699: Configuration errors (parse, invalid, missing)
 * - 700-799: Thread errors (create, join, state)
 * - 900-999: General errors (argument, state, internal)
 *
 * 按类别组织的综合错误码：
 * - 0：成功
 * - 100-199：内存错误（分配、池、缓冲区）
 * - 200-299：队列错误（满、空、超时）
 * - 300-399：文件错误（打开、写入、轮转）
 * - 400-499：网络错误（连接、发送、接收）
 * - 500-599：共享内存错误（创建、附加、损坏）
 * - 600-699：配置错误（解析、无效、缺失）
 * - 700-799：线程错误（创建、加入、状态）
 * - 900-999：通用错误（参数、状态、内部）
 */
enum class ErrorCode : int32_t {
    Success = 0,  ///< Operation completed successfully / 操作成功完成

    // Memory errors (100-199) / 内存错误
    MemoryAllocationFailed = 100,  ///< Failed to allocate memory / 内存分配失败
    MemoryPoolExhausted = 101,     ///< Memory pool has no available blocks / 内存池无可用块
    BufferOverflow = 102,          ///< Buffer capacity exceeded / 缓冲区容量超出
    BufferUnderflow = 103,         ///< Buffer underflow (read from empty) / 缓冲区下溢（从空读取）

    // Queue errors (200-299) / 队列错误
    QueueFull = 200,      ///< Queue is full, cannot push / 队列已满，无法推入
    QueueEmpty = 201,     ///< Queue is empty, cannot pop / 队列为空，无法弹出
    QueueSlotBusy = 202,  ///< Queue slot is being written / 队列槽位正在写入
    QueueTimeout = 203,   ///< Queue operation timed out / 队列操作超时
    QueueClosed = 204,    ///< Queue has been closed / 队列已关闭

    // File errors (300-399) / 文件错误
    FileOpenFailed = 300,    ///< Failed to open file / 文件打开失败
    FileWriteFailed = 301,   ///< Failed to write to file / 文件写入失败
    FileReadFailed = 302,    ///< Failed to read from file / 文件读取失败
    FileRotateFailed = 303,  ///< Failed to rotate log file / 日志文件轮转失败
    FileCloseFailed = 304,   ///< Failed to close file / 文件关闭失败
    FileFlushFailed = 305,   ///< Failed to flush file buffer / 文件缓冲区刷新失败

    // Network errors (400-499) / 网络错误
    NetworkConnectFailed = 400,   ///< Failed to connect to server / 连接服务器失败
    NetworkSendFailed = 401,      ///< Failed to send data / 发送数据失败
    NetworkReceiveFailed = 402,   ///< Failed to receive data / 接收数据失败
    NetworkDisconnected = 403,    ///< Network connection lost / 网络连接断开
    NetworkTimeout = 404,         ///< Network operation timed out / 网络操作超时
    NetworkBindFailed = 405,      ///< Failed to bind to address / 绑定地址失败

    // Shared memory errors (500-599) / 共享内存错误
    SharedMemoryCreateFailed = 500,    ///< Failed to create shared memory / 创建共享内存失败
    SharedMemoryAttachFailed = 501,    ///< Failed to attach to shared memory / 附加共享内存失败
    SharedMemoryDetachFailed = 502,    ///< Failed to detach from shared memory / 分离共享内存失败
    SharedMemoryCorrupted = 503,       ///< Shared memory data is corrupted / 共享内存数据损坏
    SharedMemoryVersionMismatch = 504, ///< Shared memory version mismatch / 共享内存版本不匹配

    // Configuration errors (600-699) / 配置错误
    ConfigParseError = 600,      ///< Failed to parse configuration / 配置解析失败
    ConfigInvalidValue = 601,    ///< Invalid configuration value / 配置值无效
    ConfigMissingRequired = 602, ///< Required configuration missing / 缺少必需配置
    ConfigFileNotFound = 603,    ///< Configuration file not found / 配置文件未找到

    // Thread errors (700-799) / 线程错误
    ThreadCreateFailed = 700,    ///< Failed to create thread / 创建线程失败
    ThreadJoinFailed = 701,      ///< Failed to join thread / 加入线程失败
    ThreadAlreadyRunning = 702,  ///< Thread is already running / 线程已在运行
    ThreadNotRunning = 703,      ///< Thread is not running / 线程未运行

    // General errors (900-999) / 通用错误
    InvalidArgument = 900,     ///< Invalid argument provided / 提供的参数无效
    NotInitialized = 901,      ///< Component not initialized / 组件未初始化
    AlreadyInitialized = 902,  ///< Component already initialized / 组件已初始化
    NotSupported = 903,        ///< Operation not supported / 操作不支持
    InternalError = 999        ///< Internal error (unexpected) / 内部错误（意外）
};

/**
 * @brief Convert error code to string representation
 * @brief 将错误码转换为字符串表示
 */
constexpr std::string_view ErrorCodeToString(ErrorCode code) noexcept {
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
 */
constexpr bool IsSuccess(ErrorCode code) noexcept {
    return code == ErrorCode::Success;
}

/**
 * @brief Check if error code indicates failure
 * @brief 检查错误码是否表示失败
 */
constexpr bool IsError(ErrorCode code) noexcept {
    return code != ErrorCode::Success;
}

}  // namespace oneplog

#endif  // ONEPLOG_COMMON_HPP
