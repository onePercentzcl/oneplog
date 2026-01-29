/**
 * @file common.hpp
 * @brief Common definitions (Level, Mode, ErrorCode)
 * @brief 通用定义（日志级别、运行模式、错误码）
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
 */
enum class Level : uint8_t {
    Trace = 0,     ///< Most detailed tracing info / 最详细的跟踪信息
    Debug = 1,     ///< Debug information / 调试信息
    Info = 2,      ///< General information / 一般信息
    Warn = 3,      ///< Warning messages / 警告信息
    Error = 4,     ///< Error messages / 错误信息
    Critical = 5,  ///< Critical errors / 严重错误
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
 */
enum class Mode : uint8_t {
    Sync = 0,   ///< Synchronous mode / 同步模式
    Async = 1,  ///< Asynchronous mode / 异步模式
    MProc = 2   ///< Multi-process mode / 多进程模式
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
 */
enum class ErrorCode : int32_t {
    Success = 0,

    // Memory errors (100-199) / 内存错误
    MemoryAllocationFailed = 100,
    MemoryPoolExhausted = 101,
    BufferOverflow = 102,
    BufferUnderflow = 103,

    // Queue errors (200-299) / 队列错误
    QueueFull = 200,
    QueueEmpty = 201,
    QueueSlotBusy = 202,
    QueueTimeout = 203,
    QueueClosed = 204,

    // File errors (300-399) / 文件错误
    FileOpenFailed = 300,
    FileWriteFailed = 301,
    FileReadFailed = 302,
    FileRotateFailed = 303,
    FileCloseFailed = 304,
    FileFlushFailed = 305,

    // Network errors (400-499) / 网络错误
    NetworkConnectFailed = 400,
    NetworkSendFailed = 401,
    NetworkReceiveFailed = 402,
    NetworkDisconnected = 403,
    NetworkTimeout = 404,
    NetworkBindFailed = 405,

    // Shared memory errors (500-599) / 共享内存错误
    SharedMemoryCreateFailed = 500,
    SharedMemoryAttachFailed = 501,
    SharedMemoryDetachFailed = 502,
    SharedMemoryCorrupted = 503,
    SharedMemoryVersionMismatch = 504,

    // Configuration errors (600-699) / 配置错误
    ConfigParseError = 600,
    ConfigInvalidValue = 601,
    ConfigMissingRequired = 602,
    ConfigFileNotFound = 603,

    // Thread errors (700-799) / 线程错误
    ThreadCreateFailed = 700,
    ThreadJoinFailed = 701,
    ThreadAlreadyRunning = 702,
    ThreadNotRunning = 703,

    // General errors (900-999) / 通用错误
    InvalidArgument = 900,
    NotInitialized = 901,
    AlreadyInitialized = 902,
    NotSupported = 903,
    InternalError = 999
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
