/**
 * @file writer_thread.hpp
 * @brief WriterThread for log output
 * @brief 日志输出的写入线程
 *
 * WriterThread is responsible for:
 * - Reading log entries from HeapRingBuffer (async mode) or SharedRingBuffer (multi-process mode)
 * - Formatting log entries using Format
 * - Writing formatted messages to Sink
 *
 * WriterThread 负责：
 * - 从 HeapRingBuffer（异步模式）或 SharedRingBuffer（多进程模式）读取日志条目
 * - 使用 Format 格式化日志条目
 * - 将格式化后的消息写入 Sink
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

#include "oneplog/format.hpp"
#include "oneplog/heap_ring_buffer.hpp"
#include "oneplog/log_entry.hpp"
#include "oneplog/shared_ring_buffer.hpp"
#include "oneplog/sink.hpp"

namespace oneplog {

/**
 * @brief Writer thread for log output
 * @brief 日志输出的写入线程
 *
 * Data flow / 数据流:
 * - Async mode: HeapRingBuffer → WriterThread → Sink
 * - Multi-process mode: SharedRingBuffer → WriterThread → Sink
 */
class WriterThread {
public:
    /**
     * @brief Construct a writer thread with a sink
     * @brief 使用 Sink 构造写入线程
     *
     * @param sink The sink to write to / 要写入的 Sink
     */
    explicit WriterThread(std::shared_ptr<Sink> sink)
        : m_sink(std::move(sink))
        , m_heapRingBuffer(nullptr)
        , m_sharedRingBuffer(nullptr)
        , m_running(false)
        , m_pollInterval(std::chrono::microseconds(1))
        , m_pollTimeout(std::chrono::milliseconds(10)) {}

    /**
     * @brief Destructor - stops the thread if running
     * @brief 析构函数 - 如果线程正在运行则停止
     */
    ~WriterThread() {
        Stop();
    }

    // Non-copyable, non-movable
    WriterThread(const WriterThread&) = delete;
    WriterThread& operator=(const WriterThread&) = delete;
    WriterThread(WriterThread&&) = delete;
    WriterThread& operator=(WriterThread&&) = delete;

    /**
     * @brief Set the heap ring buffer (for async mode)
     * @brief 设置堆环形队列（用于异步模式）
     *
     * @param buffer Pointer to the heap ring buffer / 堆环形队列指针
     */
    void SetHeapRingBuffer(HeapRingBuffer<LogEntry>* buffer) {
        m_heapRingBuffer = buffer;
    }

    /**
     * @brief Set the shared ring buffer (for multi-process mode)
     * @brief 设置共享环形队列（用于多进程模式）
     *
     * @param buffer Pointer to the shared ring buffer / 共享环形队列指针
     */
    void SetSharedRingBuffer(SharedRingBuffer<LogEntry>* buffer) {
        m_sharedRingBuffer = buffer;
    }

    /**
     * @brief Set the formatter
     * @brief 设置格式化器
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
     * @brief Start the writer thread
     * @brief 启动写入线程
     */
    void Start() {
        if (m_running.load(std::memory_order_acquire)) {
            return;  // Already running / 已经在运行
        }

        m_running.store(true, std::memory_order_release);
        m_thread = std::thread(&WriterThread::ThreadFunc, this);
    }

    /**
     * @brief Stop the writer thread
     * @brief 停止写入线程
     */
    void Stop() {
        if (!m_running.load(std::memory_order_acquire)) {
            return;  // Not running / 未运行
        }

        m_running.store(false, std::memory_order_release);

        // Notify to wake up from wait / 通知以从等待中唤醒
        if (m_heapRingBuffer) {
            m_heapRingBuffer->NotifyConsumer();
        }
        if (m_sharedRingBuffer) {
            m_sharedRingBuffer->NotifyConsumer();
        }

        if (m_thread.joinable()) {
            m_thread.join();
        }
    }

    /**
     * @brief Flush all pending log entries
     * @brief 刷新所有待处理的日志条目
     */
    void Flush() {
        // Process all remaining entries / 处理所有剩余条目
        LogEntry entry;
        
        if (m_heapRingBuffer) {
            while (m_heapRingBuffer->TryPop(entry)) {
                ProcessEntry(entry);
            }
        }
        
        if (m_sharedRingBuffer) {
            while (m_sharedRingBuffer->TryPop(entry)) {
                ProcessEntry(entry);
            }
        }

        // Flush the sink / 刷新 Sink
        if (m_sink) {
            m_sink->Flush();
        }
    }

    /**
     * @brief Check if the thread is running
     * @brief 检查线程是否正在运行
     */
    bool IsRunning() const {
        return m_running.load(std::memory_order_acquire);
    }

    /**
     * @brief Set the poll interval
     * @brief 设置轮询间隔
     *
     * @param interval Poll interval / 轮询间隔
     */
    void SetPollInterval(std::chrono::microseconds interval) {
        m_pollInterval = interval;
    }

    /**
     * @brief Get the poll interval
     * @brief 获取轮询间隔
     */
    std::chrono::microseconds GetPollInterval() const {
        return m_pollInterval;
    }

    /**
     * @brief Set the poll timeout
     * @brief 设置轮询超时
     *
     * @param timeout Poll timeout / 轮询超时
     */
    void SetPollTimeout(std::chrono::milliseconds timeout) {
        m_pollTimeout = timeout;
    }

    /**
     * @brief Get the poll timeout
     * @brief 获取轮询超时
     */
    std::chrono::milliseconds GetPollTimeout() const {
        return m_pollTimeout;
    }

    /**
     * @brief Get the sink
     * @brief 获取 Sink
     */
    std::shared_ptr<Sink> GetSink() const {
        return m_sink;
    }

private:
    /**
     * @brief Main thread function
     * @brief 主线程函数
     */
    void ThreadFunc() {
        while (m_running.load(std::memory_order_acquire)) {
            bool hasData = false;
            LogEntry entry;

            // Try to pop from heap ring buffer / 尝试从堆环形队列弹出
            if (m_heapRingBuffer) {
                while (m_running.load(std::memory_order_relaxed) &&
                       m_heapRingBuffer->TryPop(entry)) {
                    ProcessEntry(entry);
                    hasData = true;
                }
            }

            // Try to pop from shared ring buffer / 尝试从共享环形队列弹出
            if (m_sharedRingBuffer) {
                while (m_running.load(std::memory_order_relaxed) &&
                       m_sharedRingBuffer->TryPop(entry)) {
                    ProcessEntry(entry);
                    hasData = true;
                }
            }

            // No data available, wait for notification / 无数据可用，等待通知
            if (!hasData && m_running.load(std::memory_order_relaxed)) {
                WaitForData();
            }
        }

        // Drain remaining entries before exit / 退出前排空剩余条目
        Flush();
    }

    /**
     * @brief Wait for data to be available
     * @brief 等待数据可用
     */
    void WaitForData() {
        if (m_heapRingBuffer) {
            m_heapRingBuffer->WaitForData(m_pollInterval, m_pollTimeout);
        } else if (m_sharedRingBuffer) {
            m_sharedRingBuffer->WaitForData(m_pollInterval, m_pollTimeout);
        } else {
            // No buffer configured, just sleep / 未配置缓冲区，只是休眠
            std::this_thread::sleep_for(m_pollTimeout);
        }
    }

    /**
     * @brief Process a single log entry
     * @brief 处理单个日志条目
     *
     * @param entry The log entry to process / 要处理的日志条目
     */
    void ProcessEntry(const LogEntry& entry) {
        if (!m_sink) {
            return;
        }

        // Format the entry / 格式化条目
        std::string message;
        if (m_format) {
            message = m_format->FormatEntry(entry);
        } else {
            // Default format if no formatter set / 如果未设置格式化器则使用默认格式
            message = DefaultFormat(entry);
        }

        // Write to sink / 写入 Sink
        m_sink->Write(message);
    }

    /**
     * @brief Default format for log entries
     * @brief 日志条目的默认格式
     */
    std::string DefaultFormat(const LogEntry& entry) const {
        std::string result;
        result.reserve(256);

        // [timestamp] [level] message
        result += "[";
        result += FormatTimestamp(entry.timestamp);
        result += "] [";
        result += std::string(LevelToString(entry.level, LevelNameStyle::Short4));
        result += "] ";

        // Format message from snapshot / 从快照格式化消息
        if (!entry.snapshot.IsEmpty()) {
            result += entry.snapshot.FormatAll();
        }

        return result;
    }

    /**
     * @brief Format timestamp
     * @brief 格式化时间戳
     */
    static std::string FormatTimestamp(uint64_t timestamp) {
        auto seconds = static_cast<time_t>(timestamp / 1000000000ULL);
        auto millis = static_cast<uint32_t>((timestamp % 1000000000ULL) / 1000000);

        std::tm tm_time{};
#ifdef _WIN32
        localtime_s(&tm_time, &seconds);
#else
        localtime_r(&seconds, &tm_time);
#endif

        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_time);

        std::string result(buf);
        result += '.';
        if (millis < 100) result += '0';
        if (millis < 10) result += '0';
        result += std::to_string(millis);

        return result;
    }

    std::shared_ptr<Sink> m_sink;                 ///< Output sink / 输出 Sink
    std::shared_ptr<Format> m_format;             ///< Formatter / 格式化器
    HeapRingBuffer<LogEntry>* m_heapRingBuffer;   ///< Heap ring buffer / 堆环形队列
    SharedRingBuffer<LogEntry>* m_sharedRingBuffer;  ///< Shared ring buffer / 共享环形队列
    std::thread m_thread;                         ///< Worker thread / 工作线程
    std::atomic<bool> m_running;                  ///< Running flag / 运行标志
    std::chrono::microseconds m_pollInterval;     ///< Poll interval / 轮询间隔
    std::chrono::milliseconds m_pollTimeout;      ///< Poll timeout / 轮询超时
};

}  // namespace oneplog
