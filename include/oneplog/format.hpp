/**
 * @file format.hpp
 * @brief Log entry formatters for onePlog
 * @brief onePlog 日志条目格式化器
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include <chrono>
#include <ctime>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "oneplog/common.hpp"
#include "oneplog/log_entry.hpp"

namespace oneplog {

// Forward declaration
class Sink;

/**
 * @brief Base class for log formatters
 * @brief 日志格式化器基类
 *
 * Format is responsible for converting LogEntry objects into formatted strings.
 * Format 负责将 LogEntry 对象转换为格式化字符串。
 */
class Format {
public:
    /**
     * @brief Virtual destructor
     * @brief 虚析构函数
     */
    virtual ~Format() = default;

    /**
     * @brief Format a log entry into a string
     * @brief 将日志条目格式化为字符串
     *
     * @param entry The log entry to format / 要格式化的日志条目
     * @return Formatted string / 格式化后的字符串
     */
    virtual std::string FormatEntry(const LogEntry& entry) = 0;

    /**
     * @brief Convert log level to string representation
     * @brief 将日志级别转换为字符串表示
     *
     * @param level Log level / 日志级别
     * @param style Name style / 名称样式
     * @return String representation / 字符串表示
     */
    static const char* LevelToString(Level level, LevelNameStyle style = LevelNameStyle::Short4) {
        // Use the constexpr function from common.hpp
        // 使用 common.hpp 中的 constexpr 函数
        return oneplog::LevelToString(level, style).data();
    }

    /**
     * @brief Bind a sink to this formatter
     * @brief 将 Sink 绑定到此格式化器
     *
     * @param sink The sink to bind / 要绑定的 Sink
     */
    void BindSink(std::shared_ptr<Sink> sink) {
        m_sinks.push_back(std::move(sink));
    }

    /**
     * @brief Get bound sinks
     * @brief 获取绑定的 Sink 列表
     */
    const std::vector<std::shared_ptr<Sink>>& GetSinks() const {
        return m_sinks;
    }

    /**
     * @brief Clear all bound sinks
     * @brief 清除所有绑定的 Sink
     */
    void ClearSinks() {
        m_sinks.clear();
    }

protected:
    /**
     * @brief Format timestamp to string
     * @brief 将时间戳格式化为字符串
     *
     * @param timestamp Nanosecond timestamp / 纳秒级时间戳
     * @param format Time format string / 时间格式字符串
     * @param includeNanos Include nanoseconds / 是否包含纳秒
     * @return Formatted timestamp string / 格式化后的时间戳字符串
     */
    static std::string FormatTimestamp(uint64_t timestamp,
                                       const std::string& format = "%Y-%m-%d %H:%M:%S",
                                       bool includeNanos = true) {
        // Convert nanoseconds to seconds and remaining nanoseconds
        // 将纳秒转换为秒和剩余纳秒
        auto seconds = static_cast<time_t>(timestamp / 1000000000ULL);
        auto nanos = static_cast<uint32_t>(timestamp % 1000000000ULL);

        std::tm tm_time{};
#ifdef _WIN32
        localtime_s(&tm_time, &seconds);
#else
        localtime_r(&seconds, &tm_time);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm_time, format.c_str());

        if (includeNanos) {
            // Add milliseconds (3 digits) / 添加毫秒（3位）
            oss << '.' << std::setfill('0') << std::setw(3) << (nanos / 1000000);
        }

        return oss.str();
    }

    std::vector<std::shared_ptr<Sink>> m_sinks;  ///< Bound sinks / 绑定的 Sink 列表
};

// ==============================================================================
// PatternFormat / 模式格式化器
// ==============================================================================

/**
 * @brief Pattern-based log formatter
 * @brief 基于模式的日志格式化器
 *
 * Supports the following pattern specifiers:
 * 支持以下模式说明符：
 *
 * - %t - Timestamp / 时间戳
 * - %l - Log level / 日志级别
 * - %f - File name / 文件名
 * - %n - Line number / 行号
 * - %F - Function name / 函数名
 * - %T - Thread ID / 线程 ID
 * - %P - Process ID / 进程 ID
 * - %N - Process name / 进程名
 * - %M - Module name / 模块名
 * - %m - Message content / 消息内容
 * - %% - Literal % / 字面量 %
 *
 * Example patterns / 示例模式:
 * - "[%t] [%l] %m" -> "[2024-01-01 12:00:00.123] [INFO] Hello World"
 * - "%t %l [%T] %f:%n %m" -> "2024-01-01 12:00:00.123 INFO [12345] main.cpp:42 Hello World"
 */
class PatternFormat : public Format {
public:
    /// Default pattern / 默认模式
    static constexpr const char* kDefaultPattern = "[%t] [%l] %m";

    /**
     * @brief Construct with pattern string
     * @brief 使用模式字符串构造
     *
     * @param pattern Pattern string / 模式字符串
     */
    explicit PatternFormat(const std::string& pattern = kDefaultPattern)
        : m_pattern(pattern)
        , m_levelStyle(LevelNameStyle::Short4)
        , m_timestampFormat("%Y-%m-%d %H:%M:%S")
        , m_includeNanos(true) {
        ParsePattern();
    }

    /**
     * @brief Format a log entry using the pattern
     * @brief 使用模式格式化日志条目
     *
     * @param entry The log entry to format / 要格式化的日志条目
     * @return Formatted string / 格式化后的字符串
     */
    std::string FormatEntry(const LogEntry& entry) override;

    /**
     * @brief Set the log level display style
     * @brief 设置日志级别显示样式
     *
     * @param style Level name style / 级别名称样式
     */
    void SetLevelStyle(LevelNameStyle style) {
        m_levelStyle = style;
    }

    /**
     * @brief Get the current level style
     * @brief 获取当前级别样式
     */
    LevelNameStyle GetLevelStyle() const {
        return m_levelStyle;
    }

    /**
     * @brief Set the timestamp format
     * @brief 设置时间戳格式
     *
     * @param format strftime-compatible format string / strftime 兼容的格式字符串
     */
    void SetTimestampFormat(const std::string& format) {
        m_timestampFormat = format;
    }

    /**
     * @brief Get the current timestamp format
     * @brief 获取当前时间戳格式
     */
    const std::string& GetTimestampFormat() const {
        return m_timestampFormat;
    }

    /**
     * @brief Set whether to include nanoseconds in timestamp
     * @brief 设置是否在时间戳中包含纳秒
     *
     * @param include Include nanoseconds / 是否包含纳秒
     */
    void SetIncludeNanos(bool include) {
        m_includeNanos = include;
    }

    /**
     * @brief Get the pattern string
     * @brief 获取模式字符串
     */
    const std::string& GetPattern() const {
        return m_pattern;
    }

    /**
     * @brief Set a new pattern string
     * @brief 设置新的模式字符串
     *
     * @param pattern New pattern string / 新的模式字符串
     */
    void SetPattern(const std::string& pattern) {
        m_pattern = pattern;
        ParsePattern();
    }

    /**
     * @brief Set process name (for %N specifier)
     * @brief 设置进程名（用于 %N 说明符）
     */
    void SetProcessName(const std::string& name) {
        m_processName = name;
    }

    /**
     * @brief Set module name (for %M specifier)
     * @brief 设置模块名（用于 %M 说明符）
     */
    void SetModuleName(const std::string& name) {
        m_moduleName = name;
    }

private:
    /**
     * @brief Token type for parsed pattern
     * @brief 解析后模式的标记类型
     */
    enum class TokenType {
        Literal,     ///< Literal text / 字面文本
        Timestamp,   ///< %t - Timestamp / 时间戳
        Level,       ///< %l - Log level / 日志级别
        File,        ///< %f - File name / 文件名
        Line,        ///< %n - Line number / 行号
        Function,    ///< %F - Function name / 函数名
        ThreadId,    ///< %T - Thread ID / 线程 ID
        ProcessId,   ///< %P - Process ID / 进程 ID
        ProcessName, ///< %N - Process name / 进程名
        ModuleName,  ///< %M - Module name / 模块名
        Message      ///< %m - Message content / 消息内容
    };

    /**
     * @brief Token structure for parsed pattern
     * @brief 解析后模式的标记结构
     */
    struct Token {
        TokenType type;
        std::string literal;  // Only used for Literal type / 仅用于 Literal 类型
    };

    /**
     * @brief Parse the pattern string into tokens
     * @brief 将模式字符串解析为标记
     */
    void ParsePattern();

    std::string m_pattern;                    ///< Pattern string / 模式字符串
    LevelNameStyle m_levelStyle;              ///< Level name style / 级别名称样式
    std::string m_timestampFormat;            ///< Timestamp format / 时间戳格式
    bool m_includeNanos;                      ///< Include nanoseconds / 是否包含纳秒
    std::vector<Token> m_tokens;              ///< Parsed tokens / 解析后的标记
    std::string m_processName;                ///< Process name / 进程名
    std::string m_moduleName;                 ///< Module name / 模块名
};

// ==============================================================================
// JsonFormat / JSON 格式化器
// ==============================================================================

/**
 * @brief JSON log formatter
 * @brief JSON 日志格式化器
 *
 * Formats log entries as JSON objects.
 * 将日志条目格式化为 JSON 对象。
 *
 * Example output / 示例输出:
 * {"timestamp":"2024-01-01T12:00:00.123","level":"INFO","message":"Hello World"}
 */
class JsonFormat : public Format {
public:
    /**
     * @brief Default constructor
     * @brief 默认构造函数
     */
    JsonFormat() = default;

    /**
     * @brief Format a log entry as JSON
     * @brief 将日志条目格式化为 JSON
     *
     * @param entry The log entry to format / 要格式化的日志条目
     * @return JSON formatted string / JSON 格式化后的字符串
     */
    std::string FormatEntry(const LogEntry& entry) override;

    /**
     * @brief Set pretty print mode
     * @brief 设置美化打印模式
     *
     * @param enable Enable pretty print / 是否启用美化打印
     */
    void SetPrettyPrint(bool enable) {
        m_prettyPrint = enable;
    }

    /**
     * @brief Get pretty print mode
     * @brief 获取美化打印模式
     */
    bool GetPrettyPrint() const {
        return m_prettyPrint;
    }

    /**
     * @brief Set whether to include source location
     * @brief 设置是否包含源位置
     *
     * @param enable Include location / 是否包含位置
     */
    void SetIncludeLocation(bool enable) {
        m_includeLocation = enable;
    }

    /**
     * @brief Get include location setting
     * @brief 获取包含位置设置
     */
    bool GetIncludeLocation() const {
        return m_includeLocation;
    }

    /**
     * @brief Set level name style for JSON output
     * @brief 设置 JSON 输出的级别名称样式
     */
    void SetLevelStyle(LevelNameStyle style) {
        m_levelStyle = style;
    }

private:
    /**
     * @brief Escape a string for JSON
     * @brief 为 JSON 转义字符串
     */
    static std::string EscapeJson(const std::string& str);

    bool m_prettyPrint{false};                ///< Pretty print mode / 美化打印模式
    bool m_includeLocation{true};             ///< Include source location / 是否包含源位置
    LevelNameStyle m_levelStyle{LevelNameStyle::Short4};  ///< Level name style / 级别名称样式
};

}  // namespace oneplog
