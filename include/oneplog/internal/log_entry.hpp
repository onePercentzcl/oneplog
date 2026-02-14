/**
 * @file log_entry.hpp
 * @brief Log entry structures for onePlog
 * @brief onePlog 日志条目结构
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include <cstdint>

#include "oneplog/internal/binary_snapshot.hpp"
#include "oneplog/common.hpp"

namespace oneplog {

// ==============================================================================
// Source Location / 源位置
// ==============================================================================

/**
 * @brief Source code location information
 * @brief 源代码位置信息
 *
 * Captures file name, line number, and function name for debugging.
 * 捕获文件名、行号和函数名用于调试。
 */
struct SourceLocation {
    const char* file;      ///< Source file name / 源文件名
    uint32_t line;         ///< Line number / 行号
    const char* function;  ///< Function name / 函数名

    /**
     * @brief Default constructor
     * @brief 默认构造函数
     */
    constexpr SourceLocation() noexcept
        : file(nullptr), line(0), function(nullptr) {}

    /**
     * @brief Construct with location info
     * @brief 使用位置信息构造
     */
    constexpr SourceLocation(const char* f, uint32_t l, const char* fn) noexcept
        : file(f), line(l), function(fn) {}

    /**
     * @brief Check if location is valid
     * @brief 检查位置是否有效
     */
    constexpr bool IsValid() const noexcept {
        return file != nullptr && line > 0;
    }
};


// ==============================================================================
// Log Entry Structures / 日志条目结构
// ==============================================================================

/**
 * @brief Debug mode log entry (with full location info)
 * @brief 调试模式日志条目（包含完整位置信息）
 *
 * Memory layout (optimized, no unnecessary padding):
 * 内存布局（优化后，无不必要填充）：
 *
 * +------------------+
 * | timestamp (8B)   |  Nanosecond timestamp / 纳秒级时间戳
 * +------------------+
 * | file* (8B)       |  File name pointer / 文件名指针
 * +------------------+
 * | function* (8B)   |  Function name pointer / 函数名指针
 * +------------------+
 * | threadId (4B)    |  Thread ID / 线程 ID
 * +------------------+
 * | processId (4B)   |  Process ID / 进程 ID
 * +------------------+
 * | line (4B)        |  Line number / 行号
 * +------------------+
 * | level (1B)       |  Log level / 日志级别
 * +------------------+
 * | reserved (3B)    |  Reserved bytes / 保留字节
 * +------------------+
 * | snapshot (256B)  |  BinarySnapshot data / BinarySnapshot 数据
 * +------------------+
 * Total: 296 bytes / 总计: 296 字节
 */
struct LogEntryDebug {
    uint64_t timestamp;           ///< Nanosecond timestamp / 纳秒级时间戳
    const char* file;             ///< File name / 文件名
    const char* function;         ///< Function name / 函数名
    uint32_t threadId;            ///< Thread ID / 线程 ID
    uint32_t processId;           ///< Process ID / 进程 ID
    uint32_t line;                ///< Line number / 行号
    Level level;                  ///< Log level / 日志级别
    uint8_t reserved[3];          ///< Reserved bytes / 保留字节
    BinarySnapshot snapshot;      ///< Argument snapshot / 参数快照

    /**
     * @brief Default constructor
     * @brief 默认构造函数
     */
    LogEntryDebug() noexcept
        : timestamp(0)
        , file(nullptr)
        , function(nullptr)
        , threadId(0)
        , processId(0)
        , line(0)
        , level(Level::Info)
        , reserved{0, 0, 0}
        , snapshot() {}

    /**
     * @brief Construct with all fields
     * @brief 使用所有字段构造
     */
    LogEntryDebug(uint64_t ts, const char* f, const char* fn,
                  uint32_t tid, uint32_t pid, uint32_t ln,
                  Level lvl, BinarySnapshot&& snap) noexcept
        : timestamp(ts)
        , file(f)
        , function(fn)
        , threadId(tid)
        , processId(pid)
        , line(ln)
        , level(lvl)
        , reserved{0, 0, 0}
        , snapshot(std::move(snap)) {}

    /**
     * @brief Construct with SourceLocation
     * @brief 使用 SourceLocation 构造
     */
    LogEntryDebug(uint64_t ts, const SourceLocation& loc,
                  uint32_t tid, uint32_t pid,
                  Level lvl, BinarySnapshot&& snap) noexcept
        : timestamp(ts)
        , file(loc.file)
        , function(loc.function)
        , threadId(tid)
        , processId(pid)
        , line(loc.line)
        , level(lvl)
        , reserved{0, 0, 0}
        , snapshot(std::move(snap)) {}

    /**
     * @brief Get source location
     * @brief 获取源位置
     */
    SourceLocation GetLocation() const noexcept {
        return SourceLocation(file, line, function);
    }
};


/**
 * @brief Release mode log entry (compact version)
 * @brief 发布模式日志条目（精简版）
 *
 * Memory layout (optimized):
 * 内存布局（优化后）：
 *
 * +------------------+
 * | timestamp (8B)   |  Nanosecond timestamp / 纳秒级时间戳
 * +------------------+
 * | threadId (4B)    |  Thread ID / 线程 ID
 * +------------------+
 * | processId (4B)   |  Process ID / 进程 ID
 * +------------------+
 * | level (1B)       |  Log level / 日志级别
 * +------------------+
 * | reserved (7B)    |  Reserved bytes / 保留字节
 * +------------------+
 * | snapshot (256B)  |  BinarySnapshot data / BinarySnapshot 数据
 * +------------------+
 * Total: 280 bytes / 总计: 280 字节
 */
struct LogEntryRelease {
    uint64_t timestamp;           ///< Nanosecond timestamp / 纳秒级时间戳
    uint32_t threadId;            ///< Thread ID / 线程 ID
    uint32_t processId;           ///< Process ID / 进程 ID
    Level level;                  ///< Log level / 日志级别
    uint8_t reserved[7];          ///< Reserved bytes / 保留字节
    BinarySnapshot snapshot;      ///< Argument snapshot / 参数快照

    /**
     * @brief Default constructor
     * @brief 默认构造函数
     */
    LogEntryRelease() noexcept
        : timestamp(0)
        , threadId(0)
        , processId(0)
        , level(Level::Info)
        , reserved{0, 0, 0, 0, 0, 0, 0}
        , snapshot() {}

    /**
     * @brief Construct with all fields
     * @brief 使用所有字段构造
     */
    LogEntryRelease(uint64_t ts, uint32_t tid, uint32_t pid,
                    Level lvl, BinarySnapshot&& snap) noexcept
        : timestamp(ts)
        , threadId(tid)
        , processId(pid)
        , level(lvl)
        , reserved{0, 0, 0, 0, 0, 0, 0}
        , snapshot(std::move(snap)) {}
};

// ==============================================================================
// Compile-time selection / 编译时选择
// ==============================================================================

#ifdef NDEBUG
    /// LogEntry type alias (Release mode) / LogEntry 类型别名（发布模式）
    using LogEntry = LogEntryRelease;
#else
    /// LogEntry type alias (Debug mode) / LogEntry 类型别名（调试模式）
    using LogEntry = LogEntryDebug;
#endif

// ==============================================================================
// Source Location Macro / 源位置宏
// ==============================================================================

/**
 * @brief Macro to capture current source location
 * @brief 捕获当前源位置的宏
 *
 * Usage / 用法:
 * @code
 * auto loc = ONEPLOG_CURRENT_LOCATION;
 * @endcode
 */
#define ONEPLOG_CURRENT_LOCATION \
    ::oneplog::SourceLocation(__FILE__, static_cast<uint32_t>(__LINE__), __FUNCTION__)

}  // namespace oneplog
