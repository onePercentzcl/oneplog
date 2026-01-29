/**
 * @file pipeline_thread.hpp
 * @brief PipelineThread for multi-process logging
 * @brief 多进程日志的管道线程
 *
 * PipelineThread is responsible for:
 * - Reading log entries from HeapRingBuffer
 * - Converting pointer data to inline data (for cross-process transfer)
 * - Adding process ID to log entries
 * - Writing converted entries to SharedRingBuffer
 *
 * PipelineThread 负责：
 * - 从 HeapRingBuffer 读取日志条目
 * - 将指针数据转换为内联数据（用于跨进程传输）
 * - 向日志条目添加进程 ID
 * - 将转换后的条目写入 SharedRingBuffer
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include <atomic>
#include <chrono>
#include <thread>

#include "oneplog/heap_ring_buffer.hpp"
#include "oneplog/log_entry.hpp"
#include "oneplog/shared_memory.hpp"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace oneplog {

/**
 * @brief Pipeline thread for multi-process logging
 * @brief 多进程日志的管道线程
 *
 * Data flow / 数据流:
 * Source → HeapRingBuffer → PipelineThread → SharedRingBuffer → WriterThread
 */
class PipelineThread {
public:
    /**
     * @brief Construct a pipeline thread
     * @brief 构造管道线程
     *
     * @param heapRingBuffer Reference to the heap ring buffer / 堆环形队列引用
     * @param sharedMemory Reference to the shared memory manager / 共享内存管理器引用
     */
    PipelineThread(HeapRingBuffer<LogEntry>& heapRingBuffer, SharedMemory& sharedMemory)
        : m_heapRingBuffer(heapRingBuffer)
        , m_sharedMemory(sharedMemory)
        , m_running(false)
        , m_pollInterval(std::chrono::microseconds(1))
        , m_pollTimeout(std::chrono::milliseconds(10))
        , m_processId(GetCurrentProcessId()) {}

    /**
     * @brief Destructor - stops the thread if running
     * @brief 析构函数 - 如果线程正在运行则停止
     */
    ~PipelineThread() {
        Stop();
    }

    // Non-copyable, non-movable
    PipelineThread(const PipelineThread&) = delete;
    PipelineThread& operator=(const PipelineThread&) = delete;
    PipelineThread(PipelineThread&&) = delete;
    PipelineThread& operator=(PipelineThread&&) = delete;

    /**
     * @brief Start the pipeline thread
     * @brief 启动管道线程
     */
    void Start() {
        if (m_running.load(std::memory_order_acquire)) {
            return;  // Already running / 已经在运行
        }

        m_running.store(true, std::memory_order_release);
        m_thread = std::thread(&PipelineThread::ThreadFunc, this);
    }

    /**
     * @brief Stop the pipeline thread
     * @brief 停止管道线程
     */
    void Stop() {
        if (!m_running.load(std::memory_order_acquire)) {
            return;  // Not running / 未运行
        }

        m_running.store(false, std::memory_order_release);
        
        // Notify to wake up from wait / 通知以从等待中唤醒
        m_heapRingBuffer.NotifyConsumer();

        if (m_thread.joinable()) {
            m_thread.join();
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

private:
    /**
     * @brief Main thread function
     * @brief 主线程函数
     *
     * Workflow / 工作流程:
     * 1. Try to read log entry (process immediately if data available)
     * 2. After reading one entry, automatically read next
     * 3. If next is empty or being written, poll wait
     * 4. After poll timeout, enter notification wait state
     */
    void ThreadFunc() {
        while (m_running.load(std::memory_order_acquire)) {
            LogEntry entry;
            
            // Try to pop entries / 尝试弹出条目
            while (m_running.load(std::memory_order_relaxed) && 
                   m_heapRingBuffer.TryPop(entry)) {
                ProcessEntry(entry);
            }

            // No data available, wait for notification / 无数据可用，等待通知
            if (m_running.load(std::memory_order_relaxed)) {
                m_heapRingBuffer.WaitForData(m_pollInterval, m_pollTimeout);
            }
        }

        // Drain remaining entries before exit / 退出前排空剩余条目
        LogEntry entry;
        while (m_heapRingBuffer.TryPop(entry)) {
            ProcessEntry(entry);
        }
    }

    /**
     * @brief Process a single log entry
     * @brief 处理单个日志条目
     *
     * @param entry The log entry to process / 要处理的日志条目
     */
    void ProcessEntry(LogEntry& entry) {
        // Convert pointer data to inline data / 将指针数据转换为内联数据
        ConvertPointers(entry);

        // Add process ID / 添加进程 ID
        AddProcessId(entry);

        // Write to shared ring buffer / 写入共享环形队列
        auto* ringBuffer = m_sharedMemory.GetRingBuffer();
        if (ringBuffer) {
            // Check if this is a WFC entry / 检查是否是 WFC 条目
            // For WFC, we need to wait until the entry is consumed
            // 对于 WFC，我们需要等待条目被消费
            ringBuffer->TryPush(std::move(entry));
            
            // Notify consumer / 通知消费者
            m_sharedMemory.NotifyConsumer();
        }
    }

    /**
     * @brief Convert pointer data to inline data
     * @brief 将指针数据转换为内联数据
     *
     * This is necessary for cross-process transfer because pointers
     * are not valid across process boundaries.
     * 这对于跨进程传输是必要的，因为指针在进程边界之间无效。
     *
     * @param entry The log entry to convert / 要转换的日志条目
     */
    void ConvertPointers(LogEntry& entry) {
        // Convert BinarySnapshot pointer data / 转换 BinarySnapshot 指针数据
        entry.snapshot.ConvertPointersToData();
    }

    /**
     * @brief Add process ID to the log entry
     * @brief 向日志条目添加进程 ID
     *
     * @param entry The log entry to modify / 要修改的日志条目
     */
    void AddProcessId(LogEntry& entry) {
        entry.processId = m_processId;
    }

    /**
     * @brief Get current process ID
     * @brief 获取当前进程 ID
     */
    static uint32_t GetCurrentProcessId() {
#ifdef _WIN32
        return static_cast<uint32_t>(::GetCurrentProcessId());
#else
        return static_cast<uint32_t>(::getpid());
#endif
    }

    HeapRingBuffer<LogEntry>& m_heapRingBuffer;  ///< Heap ring buffer reference / 堆环形队列引用
    SharedMemory& m_sharedMemory;                 ///< Shared memory reference / 共享内存引用
    std::thread m_thread;                         ///< Worker thread / 工作线程
    std::atomic<bool> m_running;                  ///< Running flag / 运行标志
    std::chrono::microseconds m_pollInterval;     ///< Poll interval / 轮询间隔
    std::chrono::milliseconds m_pollTimeout;      ///< Poll timeout / 轮询超时
    uint32_t m_processId;                         ///< Current process ID / 当前进程 ID
};

}  // namespace oneplog
