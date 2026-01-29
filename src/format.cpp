/**
 * @file format.cpp
 * @brief Log entry formatters implementation
 * @brief 日志条目格式化器实现
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include "oneplog/format.hpp"

#include <cstring>

namespace oneplog {

// ==============================================================================
// PatternFormat Implementation / PatternFormat 实现
// ==============================================================================

void PatternFormat::ParsePattern() {
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

std::string PatternFormat::FormatEntry(const LogEntry& entry) {
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
                // Note: The format string should be stored separately
                // 注意：格式字符串应该单独存储
                // For now, we just output the snapshot data
                // 目前，我们只输出快照数据
                if (!entry.snapshot.IsEmpty()) {
                    // The snapshot contains the formatted message
                    // 快照包含格式化后的消息
                    result += entry.snapshot.Format("{}");
                }
                break;
        }
    }

    return result;
}

// ==============================================================================
// JsonFormat Implementation / JsonFormat 实现
// ==============================================================================

std::string JsonFormat::EscapeJson(const std::string& str) {
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

std::string JsonFormat::FormatEntry(const LogEntry& entry) {
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

}  // namespace oneplog
