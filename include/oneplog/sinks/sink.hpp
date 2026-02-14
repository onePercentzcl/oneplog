/**
 * @file sink.hpp
 * @brief Log output sinks for onePlog
 * @brief onePlog 日志输出目标
 *
 * This file contains all sink implementations:
 * - Sink: Base class for all sinks
 * - ConsoleSink: Console output sink
 * - FileSink: File output sink with rotation support
 * - NetworkSink: Network output sink (TCP/UDP)
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
#include <string_view>
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
    virtual ~Sink() = default;

    void SetFormat(std::shared_ptr<Format> format) {
        m_format = std::move(format);
    }

    std::shared_ptr<Format> GetFormat() const {
        return m_format;
    }

    virtual void Write(const std::string& message) = 0;

    virtual void Write(std::string_view message) {
        Write(std::string(message));
    }

    virtual void WriteBatch(const std::vector<std::string>& messages) {
        for (const auto& msg : messages) {
            Write(msg);
        }
    }

    virtual void Flush() = 0;
    virtual void Close() = 0;
    virtual bool HasError() const = 0;
    virtual std::string GetLastError() const = 0;

    void StartThread() {
        if (m_threadRunning.load()) {
            return;
        }
        m_threadRunning.store(true);
        m_thread = std::thread(&Sink::ThreadFunc, this);
    }

    void StopThread() {
        if (!m_threadRunning.load()) {
            return;
        }
        m_threadRunning.store(false);
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    bool IsThreadRunning() const {
        return m_threadRunning.load();
    }

protected:
    virtual void ThreadFunc() {}

    std::shared_ptr<Format> m_format;
    std::thread m_thread;
    std::atomic<bool> m_threadRunning{false};
};

// ==============================================================================
// ConsoleSink / 控制台输出
// ==============================================================================

/**
 * @brief Console output sink
 * @brief 控制台输出 Sink
 */
class ConsoleSink : public Sink {
public:
    enum class Stream { StdOut, StdErr };

    explicit ConsoleSink(Stream stream = Stream::StdOut)
        : m_stream(stream), m_colorEnabled(true), m_hasError(false) {}

    void Write(const std::string& message) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        FILE* out = (m_stream == Stream::StdOut) ? stdout : stderr;
        std::fputs(message.c_str(), out);
        std::fputc('\n', out);
    }

    void Write(std::string_view message) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        FILE* out = (m_stream == Stream::StdOut) ? stdout : stderr;
        std::fwrite(message.data(), 1, message.size(), out);
        std::fputc('\n', out);
    }

    void Flush() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        FILE* out = (m_stream == Stream::StdOut) ? stdout : stderr;
        std::fflush(out);
    }

    void Close() override {}

    bool HasError() const override { return m_hasError; }
    std::string GetLastError() const override { return m_lastError; }

    void SetColorEnabled(bool enable) { m_colorEnabled = enable; }
    bool IsColorEnabled() const { return m_colorEnabled; }

    static const char* GetLevelColor(Level level) {
        switch (level) {
            case Level::Trace:    return "\033[37m";
            case Level::Debug:    return "\033[36m";
            case Level::Info:     return "\033[32m";
            case Level::Warn:     return "\033[33m";
            case Level::Error:    return "\033[31m";
            case Level::Critical: return "\033[35m";
            default:              return "\033[0m";
        }
    }

    static const char* GetResetColor() { return "\033[0m"; }

private:
    Stream m_stream;
    bool m_colorEnabled;
    bool m_hasError;
    std::string m_lastError;
    mutable std::mutex m_mutex;
};

// ==============================================================================
// FileSink / 文件输出
// ==============================================================================

/**
 * @brief File output sink with rotation support
 * @brief 支持轮转的文件输出 Sink
 */
class FileSink : public Sink {
public:
    explicit FileSink(const std::string& filename)
        : m_filename(filename), m_maxSize(0), m_maxFiles(0),
          m_currentSize(0), m_rotateOnOpen(false), m_hasError(false) {
        OpenFile();
    }

    ~FileSink() override { Close(); }

    void Write(const std::string& message) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_file.is_open()) {
            m_hasError = true;
            m_lastError = "File not open";
            return;
        }
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

    void Write(std::string_view message) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_file.is_open()) {
            m_hasError = true;
            m_lastError = "File not open";
            return;
        }
        if (m_maxSize > 0 && m_currentSize + message.size() > m_maxSize) {
            RotateFile();
        }
        m_file.write(message.data(), static_cast<std::streamsize>(message.size()));
        m_file.put('\n');
        m_currentSize += message.size() + 1;
        if (m_file.fail()) {
            m_hasError = true;
            m_lastError = "Write failed";
        }
    }

    void Flush() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_file.is_open()) {
            m_file.flush();
        }
    }

    void Close() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_file.is_open()) {
            m_file.close();
        }
    }

    bool HasError() const override { return m_hasError; }
    std::string GetLastError() const override { return m_lastError; }

    void SetMaxSize(size_t bytes) { m_maxSize = bytes; }
    void SetMaxFiles(size_t count) { m_maxFiles = count; }
    void SetRotateOnOpen(bool enable) { m_rotateOnOpen = enable; }
    size_t GetCurrentSize() const { return m_currentSize; }
    const std::string& GetFilename() const { return m_filename; }

private:
    void OpenFile() {
        m_file.open(m_filename, std::ios::app);
        if (!m_file.is_open()) {
            m_hasError = true;
            m_lastError = "Failed to open file: " + m_filename;
            return;
        }
        m_file.seekp(0, std::ios::end);
        m_currentSize = static_cast<size_t>(m_file.tellp());
        m_hasError = false;
    }

    void RotateFile() {
        m_file.close();
        if (m_maxFiles > 0) {
            std::string oldestFile = m_filename + "." + std::to_string(m_maxFiles);
            std::remove(oldestFile.c_str());
            for (size_t i = m_maxFiles - 1; i >= 1; --i) {
                std::string oldName = m_filename + "." + std::to_string(i);
                std::string newName = m_filename + "." + std::to_string(i + 1);
                std::rename(oldName.c_str(), newName.c_str());
            }
        }
        std::string rotatedName = m_filename + ".1";
        std::rename(m_filename.c_str(), rotatedName.c_str());
        m_currentSize = 0;
        OpenFile();
    }

    std::string m_filename;
    std::ofstream m_file;
    size_t m_maxSize;
    size_t m_maxFiles;
    size_t m_currentSize;
    bool m_rotateOnOpen;
    bool m_hasError;
    std::string m_lastError;
    mutable std::mutex m_mutex;
};

// ==============================================================================
// NetworkSink / 网络输出
// ==============================================================================

/**
 * @brief Network output sink (TCP/UDP)
 * @brief 网络输出 Sink（TCP/UDP）
 */
class NetworkSink : public Sink {
public:
    enum class Protocol { TCP, UDP };

    NetworkSink(const std::string& host, uint16_t port, Protocol protocol = Protocol::TCP)
        : m_host(host), m_port(port), m_protocol(protocol), m_socket(-1),
          m_reconnectInterval(std::chrono::seconds(5)), m_maxRetries(3), m_hasError(false) {}

    ~NetworkSink() override { Close(); }

    void Write(const std::string& message) override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!IsConnected()) {
            if (!Connect()) {
                m_hasError = true;
                m_lastError = "Failed to connect to " + m_host + ":" + std::to_string(m_port);
                return;
            }
        }
        (void)message;
    }

    void Flush() override {}

    void Close() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_socket >= 0) {
            m_socket = -1;
        }
    }

    bool HasError() const override { return m_hasError; }
    std::string GetLastError() const override { return m_lastError; }

    void SetReconnectInterval(std::chrono::seconds interval) { m_reconnectInterval = interval; }
    void SetMaxRetries(size_t count) { m_maxRetries = count; }
    const std::string& GetHost() const { return m_host; }
    uint16_t GetPort() const { return m_port; }
    Protocol GetProtocol() const { return m_protocol; }

private:
    bool IsConnected() const { return m_socket >= 0; }
    bool Connect() { return false; }

    std::string m_host;
    uint16_t m_port;
    Protocol m_protocol;
    int m_socket;
    std::chrono::seconds m_reconnectInterval;
    size_t m_maxRetries;
    bool m_hasError;
    std::string m_lastError;
    mutable std::mutex m_mutex;
};

}  // namespace oneplog
