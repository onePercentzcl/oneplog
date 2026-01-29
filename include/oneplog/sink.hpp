/**
 * @file sink.hpp
 * @brief Log output sinks for onePlog
 * @brief onePlog 日志输出目标
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "oneplog/common.hpp"

namespace oneplog {

// Forward declaration
class Format;

// ==============================================================================
// Sink Base Class / Sink 基类
// ==============================================================================

/**
 * @brief Base class for log output sinks
 * @brief 日志输出目标基类
 *
 * Sink is responsible for writing formatted log messages to various destinations.
 * Sink 负责将格式化后的日志消息写入各种目标。
 */
class Sink {
public:
    /**
     * @brief Virtual destructor
     * @brief 虚析构函数
     */
    virtual ~Sink() = default;

    /**
     * @brief Set the formatter for this sink
     * @brief 设置此 Sink 的格式化器
     *
     * @param format The formatter to use / 要使用的格式化器
     */
    void SetFormat(std::shared_ptr<Format> format) {
        m_format = std::move(format);
    }

    /**
     * @brief Get the formatter
     * @brief 获取格式化器
     */
    std::shared_ptr<Format> GetFormat() const {
        return m_format;
    }

    /**
     * @brief Write a single log message
     * @brief 写入单条日志消息
     *
     * @param message The formatted message to write / 要写入的格式化消息
     */
    virtual void Write(const std::string& message) = 0;

    /**
     * @brief Write multiple log messages in batch
     * @brief 批量写入多条日志消息
     *
     * @param messages The messages to write / 要写入的消息列表
     */
    virtual void WriteBatch(const std::vector<std::string>& messages) {
        for (const auto& msg : messages) {
            Write(msg);
        }
    }

    /**
     * @brief Flush any buffered output
     * @brief 刷新所有缓冲的输出
     */
    virtual void Flush() = 0;

    /**
     * @brief Close the sink and release resources
     * @brief 关闭 Sink 并释放资源
     */
    virtual void Close() = 0;

    /**
     * @brief Check if an error has occurred
     * @brief 检查是否发生错误
     */
    virtual bool HasError() const = 0;

    /**
     * @brief Get the last error message
     * @brief 获取最后的错误消息
     */
    virtual std::string GetLastError() const = 0;

    /**
     * @brief Start the sink's background thread
     * @brief 启动 Sink 的后台线程
     */
    void StartThread() {
        if (m_threadRunning.load()) {
            return;
        }
        m_threadRunning.store(true);
        m_thread = std::thread(&Sink::ThreadFunc, this);
    }

    /**
     * @brief Stop the sink's background thread
     * @brief 停止 Sink 的后台线程
     */
    void StopThread() {
        if (!m_threadRunning.load()) {
            return;
        }
        m_threadRunning.store(false);
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    /**
     * @brief Check if the background thread is running
     * @brief 检查后台线程是否正在运行
     */
    bool IsThreadRunning() const {
        return m_threadRunning.load();
    }

protected:
    /**
     * @brief Background thread function (override in derived classes if needed)
     * @brief 后台线程函数（如需要可在派生类中重写）
     */
    virtual void ThreadFunc() {
        // Default implementation does nothing
        // 默认实现不做任何事
    }

    std::shared_ptr<Format> m_format;           ///< Formatter / 格式化器
    std::thread m_thread;                        ///< Background thread / 后台线程
    std::atomic<bool> m_threadRunning{false};   ///< Thread running flag / 线程运行标志
};

// ==============================================================================
// ConsoleSink / 控制台输出
// ==============================================================================

/**
 * @brief Console output sink
 * @brief 控制台输出 Sink
 *
 * Writes log messages to stdout or stderr with optional color support.
 * 将日志消息写入 stdout 或 stderr，支持可选的颜色输出。
 */
class ConsoleSink : public Sink {
public:
    /**
     * @brief Output stream selection
     * @brief 输出流选择
     */
    enum class Stream {
        StdOut,  ///< Standard output / 标准输出
        StdErr   ///< Standard error / 标准错误
    };

    /**
     * @brief Construct a console sink
     * @brief 构造控制台 Sink
     *
     * @param stream Output stream to use / 要使用的输出流
     */
    explicit ConsoleSink(Stream stream = Stream::StdOut)
        : m_stream(stream)
        , m_colorEnabled(true)
        , m_hasError(false) {}

    /**
     * @brief Write a message to the console
     * @brief 将消息写入控制台
     */
    void Write(const std::string& message) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        FILE* out = (m_stream == Stream::StdOut) ? stdout : stderr;
        std::fputs(message.c_str(), out);
        std::fputc('\n', out);
    }

    /**
     * @brief Flush the console output
     * @brief 刷新控制台输出
     */
    void Flush() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        FILE* out = (m_stream == Stream::StdOut) ? stdout : stderr;
        std::fflush(out);
    }

    /**
     * @brief Close the console sink (no-op)
     * @brief 关闭控制台 Sink（无操作）
     */
    void Close() override {
        // Nothing to close for console
        // 控制台无需关闭
    }

    /**
     * @brief Check if an error has occurred
     * @brief 检查是否发生错误
     */
    bool HasError() const override {
        return m_hasError;
    }

    /**
     * @brief Get the last error message
     * @brief 获取最后的错误消息
     */
    std::string GetLastError() const override {
        return m_lastError;
    }

    /**
     * @brief Enable or disable color output
     * @brief 启用或禁用颜色输出
     *
     * @param enable Enable color / 是否启用颜色
     */
    void SetColorEnabled(bool enable) {
        m_colorEnabled = enable;
    }

    /**
     * @brief Check if color is enabled
     * @brief 检查是否启用颜色
     */
    bool IsColorEnabled() const {
        return m_colorEnabled;
    }

    /**
     * @brief Get ANSI color code for a log level
     * @brief 获取日志级别的 ANSI 颜色代码
     */
    static const char* GetLevelColor(Level level) {
        switch (level) {
            case Level::Trace:
                return "\033[37m";      // White / 白色
            case Level::Debug:
                return "\033[36m";      // Cyan / 青色
            case Level::Info:
                return "\033[32m";      // Green / 绿色
            case Level::Warn:
                return "\033[33m";      // Yellow / 黄色
            case Level::Error:
                return "\033[31m";      // Red / 红色
            case Level::Critical:
                return "\033[35m";      // Magenta / 品红
            default:
                return "\033[0m";       // Reset / 重置
        }
    }

    /**
     * @brief Get ANSI reset code
     * @brief 获取 ANSI 重置代码
     */
    static const char* GetResetColor() {
        return "\033[0m";
    }

private:
    Stream m_stream;                    ///< Output stream / 输出流
    bool m_colorEnabled;                ///< Color enabled / 是否启用颜色
    bool m_hasError;                    ///< Error flag / 错误标志
    std::string m_lastError;            ///< Last error message / 最后的错误消息
    mutable std::mutex m_mutex;         ///< Mutex for thread safety / 线程安全互斥锁
};

// ==============================================================================
// FileSink / 文件输出
// ==============================================================================

/**
 * @brief File output sink with rotation support
 * @brief 支持轮转的文件输出 Sink
 *
 * Writes log messages to a file with optional size-based rotation.
 * 将日志消息写入文件，支持可选的基于大小的轮转。
 */
class FileSink : public Sink {
public:
    /**
     * @brief Construct a file sink
     * @brief 构造文件 Sink
     *
     * @param filename Path to the log file / 日志文件路径
     */
    explicit FileSink(const std::string& filename)
        : m_filename(filename)
        , m_maxSize(0)
        , m_maxFiles(0)
        , m_currentSize(0)
        , m_rotateOnOpen(false)
        , m_hasError(false) {
        OpenFile();
    }

    /**
     * @brief Destructor - closes the file
     * @brief 析构函数 - 关闭文件
     */
    ~FileSink() override {
        Close();
    }

    /**
     * @brief Write a message to the file
     * @brief 将消息写入文件
     */
    void Write(const std::string& message) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_file.is_open()) {
            m_hasError = true;
            m_lastError = "File not open";
            return;
        }

        // Check if rotation is needed / 检查是否需要轮转
        if (m_maxSize > 0 && m_currentSize + message.size() > m_maxSize) {
            RotateFile();
        }

        m_file << message << '\n';
        m_currentSize += message.size() + 1;

        if (m_file.fail()) {
            m_hasError = true;
            m_lastError = "Write failed";
        }
    }

    /**
     * @brief Flush the file buffer
     * @brief 刷新文件缓冲区
     */
    void Flush() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_file.is_open()) {
            m_file.flush();
        }
    }

    /**
     * @brief Close the file
     * @brief 关闭文件
     */
    void Close() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_file.is_open()) {
            m_file.close();
        }
    }

    /**
     * @brief Check if an error has occurred
     * @brief 检查是否发生错误
     */
    bool HasError() const override {
        return m_hasError;
    }

    /**
     * @brief Get the last error message
     * @brief 获取最后的错误消息
     */
    std::string GetLastError() const override {
        return m_lastError;
    }

    /**
     * @brief Set maximum file size before rotation
     * @brief 设置轮转前的最大文件大小
     *
     * @param bytes Maximum size in bytes (0 = no limit) / 最大字节数（0 = 无限制）
     */
    void SetMaxSize(size_t bytes) {
        m_maxSize = bytes;
    }

    /**
     * @brief Set maximum number of rotated files to keep
     * @brief 设置保留的最大轮转文件数
     *
     * @param count Maximum files (0 = no limit) / 最大文件数（0 = 无限制）
     */
    void SetMaxFiles(size_t count) {
        m_maxFiles = count;
    }

    /**
     * @brief Set whether to rotate on open
     * @brief 设置是否在打开时轮转
     *
     * @param enable Enable rotate on open / 是否启用打开时轮转
     */
    void SetRotateOnOpen(bool enable) {
        m_rotateOnOpen = enable;
    }

    /**
     * @brief Get the current file size
     * @brief 获取当前文件大小
     */
    size_t GetCurrentSize() const {
        return m_currentSize;
    }

    /**
     * @brief Get the filename
     * @brief 获取文件名
     */
    const std::string& GetFilename() const {
        return m_filename;
    }

private:
    /**
     * @brief Open the log file
     * @brief 打开日志文件
     */
    void OpenFile() {
        m_file.open(m_filename, std::ios::app);
        if (!m_file.is_open()) {
            m_hasError = true;
            m_lastError = "Failed to open file: " + m_filename;
            return;
        }

        // Get current file size / 获取当前文件大小
        m_file.seekp(0, std::ios::end);
        m_currentSize = static_cast<size_t>(m_file.tellp());
        m_hasError = false;
    }

    /**
     * @brief Rotate the log file
     * @brief 轮转日志文件
     */
    void RotateFile() {
        m_file.close();

        // Delete oldest file if max files exceeded / 如果超过最大文件数则删除最旧的文件
        if (m_maxFiles > 0) {
            std::string oldestFile = m_filename + "." + std::to_string(m_maxFiles);
            std::remove(oldestFile.c_str());

            // Rename existing rotated files / 重命名现有的轮转文件
            for (size_t i = m_maxFiles - 1; i >= 1; --i) {
                std::string oldName = m_filename + "." + std::to_string(i);
                std::string newName = m_filename + "." + std::to_string(i + 1);
                std::rename(oldName.c_str(), newName.c_str());
            }
        }

        // Rename current file to .1 / 将当前文件重命名为 .1
        std::string rotatedName = m_filename + ".1";
        std::rename(m_filename.c_str(), rotatedName.c_str());

        // Open new file / 打开新文件
        m_currentSize = 0;
        OpenFile();
    }

    std::string m_filename;             ///< Log file path / 日志文件路径
    std::ofstream m_file;               ///< File stream / 文件流
    size_t m_maxSize;                   ///< Max file size / 最大文件大小
    size_t m_maxFiles;                  ///< Max rotated files / 最大轮转文件数
    size_t m_currentSize;               ///< Current file size / 当前文件大小
    bool m_rotateOnOpen;                ///< Rotate on open / 打开时轮转
    bool m_hasError;                    ///< Error flag / 错误标志
    std::string m_lastError;            ///< Last error message / 最后的错误消息
    mutable std::mutex m_mutex;         ///< Mutex for thread safety / 线程安全互斥锁
};

// ==============================================================================
// NetworkSink / 网络输出
// ==============================================================================

/**
 * @brief Network output sink (TCP/UDP)
 * @brief 网络输出 Sink（TCP/UDP）
 *
 * Writes log messages to a network destination.
 * 将日志消息写入网络目标。
 */
class NetworkSink : public Sink {
public:
    /**
     * @brief Network protocol selection
     * @brief 网络协议选择
     */
    enum class Protocol {
        TCP,  ///< TCP protocol / TCP 协议
        UDP   ///< UDP protocol / UDP 协议
    };

    /**
     * @brief Construct a network sink
     * @brief 构造网络 Sink
     *
     * @param host Target host / 目标主机
     * @param port Target port / 目标端口
     * @param protocol Network protocol / 网络协议
     */
    NetworkSink(const std::string& host, uint16_t port, 
                Protocol protocol = Protocol::TCP)
        : m_host(host)
        , m_port(port)
        , m_protocol(protocol)
        , m_socket(-1)
        , m_reconnectInterval(std::chrono::seconds(5))
        , m_maxRetries(3)
        , m_hasError(false) {
        // Connection will be established on first write
        // 连接将在首次写入时建立
    }

    /**
     * @brief Destructor - closes the connection
     * @brief 析构函数 - 关闭连接
     */
    ~NetworkSink() override {
        Close();
    }

    /**
     * @brief Write a message to the network
     * @brief 将消息写入网络
     */
    void Write(const std::string& message) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // Network implementation is platform-specific
        // 网络实现是平台特定的
        // For now, just store the message (placeholder)
        // 目前只存储消息（占位符）
        
        if (!IsConnected()) {
            if (!Connect()) {
                m_hasError = true;
                m_lastError = "Failed to connect to " + m_host + ":" + std::to_string(m_port);
                return;
            }
        }

        // Actual send would go here / 实际发送代码在这里
        // This is a placeholder implementation / 这是一个占位符实现
        (void)message;  // Suppress unused warning / 抑制未使用警告
    }

    /**
     * @brief Flush (no-op for network)
     * @brief 刷新（网络无操作）
     */
    void Flush() override {
        // Network sends are typically immediate
        // 网络发送通常是即时的
    }

    /**
     * @brief Close the network connection
     * @brief 关闭网络连接
     */
    void Close() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_socket >= 0) {
            // Close socket (platform-specific)
            // 关闭套接字（平台特定）
            m_socket = -1;
        }
    }

    /**
     * @brief Check if an error has occurred
     * @brief 检查是否发生错误
     */
    bool HasError() const override {
        return m_hasError;
    }

    /**
     * @brief Get the last error message
     * @brief 获取最后的错误消息
     */
    std::string GetLastError() const override {
        return m_lastError;
    }

    /**
     * @brief Set reconnection interval
     * @brief 设置重连间隔
     *
     * @param interval Reconnection interval / 重连间隔
     */
    void SetReconnectInterval(std::chrono::seconds interval) {
        m_reconnectInterval = interval;
    }

    /**
     * @brief Set maximum retry count
     * @brief 设置最大重试次数
     *
     * @param count Maximum retries / 最大重试次数
     */
    void SetMaxRetries(size_t count) {
        m_maxRetries = count;
    }

    /**
     * @brief Get the target host
     * @brief 获取目标主机
     */
    const std::string& GetHost() const {
        return m_host;
    }

    /**
     * @brief Get the target port
     * @brief 获取目标端口
     */
    uint16_t GetPort() const {
        return m_port;
    }

    /**
     * @brief Get the protocol
     * @brief 获取协议
     */
    Protocol GetProtocol() const {
        return m_protocol;
    }

private:
    /**
     * @brief Check if connected
     * @brief 检查是否已连接
     */
    bool IsConnected() const {
        return m_socket >= 0;
    }

    /**
     * @brief Establish connection
     * @brief 建立连接
     */
    bool Connect() {
        // Platform-specific connection code would go here
        // 平台特定的连接代码在这里
        // This is a placeholder / 这是一个占位符
        return false;
    }

    std::string m_host;                         ///< Target host / 目标主机
    uint16_t m_port;                            ///< Target port / 目标端口
    Protocol m_protocol;                        ///< Network protocol / 网络协议
    int m_socket;                               ///< Socket descriptor / 套接字描述符
    std::chrono::seconds m_reconnectInterval;   ///< Reconnect interval / 重连间隔
    size_t m_maxRetries;                        ///< Max retries / 最大重试次数
    bool m_hasError;                            ///< Error flag / 错误标志
    std::string m_lastError;                    ///< Last error message / 最后的错误消息
    mutable std::mutex m_mutex;                 ///< Mutex for thread safety / 线程安全互斥锁
};

}  // namespace oneplog
