/**
 * @file format.hpp
 * @brief Log entry formatters for onePlog
 * @brief onePlog 日志条目格式化器
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include <chrono>
#include <cstdio>
#include <cstring>
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
    std::string FormatEntry(const LogEntry& entry) override {
        std::string result;
        result.reserve(256);

        for (const auto& token : m_tokens) {
            switch (token.type) {
                case TokenType::Literal:
                    result += token.literal;
                    break;

                case TokenType::Timestamp:
                    result += FormatTimestamp(entry.timestamp, m_timestampFormat, m_includeNanos);
                    break;

                case TokenType::Level:
                    result += LevelToString(entry.level, m_levelStyle);
                    break;

                case TokenType::File:
#ifndef NDEBUG
                    if (entry.file != nullptr) {
                        // Extract just the filename from the path
                        // 从路径中提取文件名
                        const char* filename = entry.file;
                        const char* lastSlash = std::strrchr(entry.file, '/');
                        if (lastSlash != nullptr) {
                            filename = lastSlash + 1;
                        }
#ifdef _WIN32
                        const char* lastBackslash = std::strrchr(entry.file, '\\');
                        if (lastBackslash != nullptr && lastBackslash > filename) {
                            filename = lastBackslash + 1;
                        }
#endif
                        result += filename;
                    }
#endif
                    break;

                case TokenType::Line:
#ifndef NDEBUG
                    result += std::to_string(entry.line);
#else
                    result += "0";
#endif
                    break;

                case TokenType::Function:
#ifndef NDEBUG
                    if (entry.function != nullptr) {
                        result += entry.function;
                    }
#endif
                    break;

                case TokenType::ThreadId:
                    result += std::to_string(entry.threadId);
                    break;

                case TokenType::ProcessId:
                    result += std::to_string(entry.processId);
                    break;

                case TokenType::ProcessName:
                    result += m_processName;
                    break;

                case TokenType::ModuleName:
                    result += m_moduleName;
                    break;

                case TokenType::Message:
                    // Format the message using BinarySnapshot
                    // 使用 BinarySnapshot 格式化消息
                    if (!entry.snapshot.IsEmpty()) {
                        result += entry.snapshot.Format("{}");
                    }
                    break;
            }
        }

        return result;
    }

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
    void ParsePattern() {
        m_tokens.clear();
        const char* p = m_pattern.c_str();
        std::string literal;

        while (*p != '\0') {
            if (*p == '%') {
                // Save accumulated literal / 保存累积的字面文本
                if (!literal.empty()) {
                    m_tokens.push_back({TokenType::Literal, literal});
                    literal.clear();
                }

                ++p;  // Skip '%'
                if (*p == '\0') {
                    break;
                }

                switch (*p) {
                    case 't':
                        m_tokens.push_back({TokenType::Timestamp, ""});
                        break;
                    case 'l':
                        m_tokens.push_back({TokenType::Level, ""});
                        break;
                    case 'f':
                        m_tokens.push_back({TokenType::File, ""});
                        break;
                    case 'n':
                        m_tokens.push_back({TokenType::Line, ""});
                        break;
                    case 'F':
                        m_tokens.push_back({TokenType::Function, ""});
                        break;
                    case 'T':
                        m_tokens.push_back({TokenType::ThreadId, ""});
                        break;
                    case 'P':
                        m_tokens.push_back({TokenType::ProcessId, ""});
                        break;
                    case 'N':
                        m_tokens.push_back({TokenType::ProcessName, ""});
                        break;
                    case 'M':
                        m_tokens.push_back({TokenType::ModuleName, ""});
                        break;
                    case 'm':
                        m_tokens.push_back({TokenType::Message, ""});
                        break;
                    case '%':
                        literal += '%';
                        break;
                    default:
                        // Unknown specifier, treat as literal
                        // 未知说明符，作为字面文本处理
                        literal += '%';
                        literal += *p;
                        break;
                }
                ++p;
            } else {
                literal += *p++;
            }
        }

        // Save remaining literal / 保存剩余的字面文本
        if (!literal.empty()) {
            m_tokens.push_back({TokenType::Literal, literal});
        }
    }

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
    std::string FormatEntry(const LogEntry& entry) override {
        std::ostringstream oss;
        const char* indent = m_prettyPrint ? "  " : "";
        const char* newline = m_prettyPrint ? "\n" : "";
        const char* space = m_prettyPrint ? " " : "";

        oss << "{" << newline;

        // Timestamp / 时间戳
        oss << indent << "\"timestamp\":" << space << "\"" 
            << FormatTimestamp(entry.timestamp, "%Y-%m-%dT%H:%M:%S", true) << "\"," << newline;

        // Level / 日志级别
        oss << indent << "\"level\":" << space << "\"" 
            << LevelToString(entry.level, m_levelStyle) << "\"," << newline;

        // Thread ID / 线程 ID
        oss << indent << "\"threadId\":" << space << entry.threadId << "," << newline;

        // Process ID / 进程 ID
        oss << indent << "\"processId\":" << space << entry.processId;

#ifndef NDEBUG
        // Source location (Debug mode only) / 源位置（仅调试模式）
        if (m_includeLocation) {
            oss << "," << newline;

            // File / 文件
            if (entry.file != nullptr) {
                const char* filename = entry.file;
                const char* lastSlash = std::strrchr(entry.file, '/');
                if (lastSlash != nullptr) {
                    filename = lastSlash + 1;
                }
#ifdef _WIN32
                const char* lastBackslash = std::strrchr(entry.file, '\\');
                if (lastBackslash != nullptr && lastBackslash > filename) {
                    filename = lastBackslash + 1;
                }
#endif
                oss << indent << "\"file\":" << space << "\"" << EscapeJson(filename) << "\"," << newline;
            }

            // Line / 行号
            oss << indent << "\"line\":" << space << entry.line << "," << newline;

            // Function / 函数
            if (entry.function != nullptr) {
                oss << indent << "\"function\":" << space << "\"" 
                    << EscapeJson(entry.function) << "\"";
            }
        }
#endif

        // Message / 消息
        oss << "," << newline;
        std::string message;
        if (!entry.snapshot.IsEmpty()) {
            message = entry.snapshot.Format("{}");
        }
        oss << indent << "\"message\":" << space << "\"" << EscapeJson(message) << "\"" << newline;

        oss << "}";

        return oss.str();
    }

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
    static std::string EscapeJson(const std::string& str) {
        std::string result;
        result.reserve(str.size() + 16);

        for (char c : str) {
            switch (c) {
                case '"':
                    result += "\\\"";
                    break;
                case '\\':
                    result += "\\\\";
                    break;
                case '\b':
                    result += "\\b";
                    break;
                case '\f':
                    result += "\\f";
                    break;
                case '\n':
                    result += "\\n";
                    break;
                case '\r':
                    result += "\\r";
                    break;
                case '\t':
                    result += "\\t";
                    break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        // Control character, escape as \uXXXX
                        // 控制字符，转义为 \uXXXX
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x",
                                      static_cast<unsigned int>(static_cast<unsigned char>(c)));
                        result += buf;
                    } else {
                        result += c;
                    }
                    break;
            }
        }

        return result;
    }

    bool m_prettyPrint{false};                ///< Pretty print mode / 美化打印模式
    bool m_includeLocation{true};             ///< Include source location / 是否包含源位置
    LevelNameStyle m_levelStyle{LevelNameStyle::Short4};  ///< Level name style / 级别名称样式
};

}  // namespace oneplog
