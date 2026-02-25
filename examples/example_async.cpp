/**
 * @file example_async.cpp
 * @brief Asynchronous mode example for onePlog
 * @brief onePlog 异步模式示例
 *
 * This example demonstrates the asynchronous logging mode of onePlog,
 * including various configurations and usage patterns.
 *
 * 此示例演示 onePlog 的异步日志模式，包括各种配置和使用模式。
 *
 * Features demonstrated / 演示的功能:
 * - Default async logger basic usage / 默认异步日志器基本用法
 * - HeapRingBuffer capacity configuration / HeapRingBuffer 容量配置
 * - QueueFullPolicy strategies / QueueFullPolicy 各策略
 * - ShadowTail optimization / ShadowTail 优化
 * - WFC (Wait For Completion) / WFC（等待完成）功能
 * - Flush and Shutdown / Flush 和 Shutdown 使用
 * - Multi-threaded concurrent logging / 多线程并发日志记录
 * - File and console sink configuration / 文件和控制台 Sink 配置
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <oneplog/oneplog.hpp>

// ==============================================================================
// Example 1: Default Async Logger Basic Usage
// 示例 1: 默认异步日志器基本用法
// ==============================================================================

/**
 * @brief Demonstrates basic usage of the default async logger
 * @brief 演示默认异步日志器的基本用法
 *
 * The AsyncLogger uses a background thread to process log messages,
 * providing non-blocking logging for high-performance applications.
 *
 * AsyncLogger 使用后台线程处理日志消息，为高性能应用提供非阻塞日志记录。
 */
void BasicAsyncLoggerExample() {
    std::cout << "\n=== Example 1: Default Async Logger / 默认异步日志器 ===" << std::endl;
    
    // Create an async logger with default settings
    // 使用默认设置创建异步日志器
    oneplog::AsyncLogger logger;
    
    // Log messages at different levels
    // 不同级别的日志消息
    logger.Trace("Async trace message / 异步跟踪消息");
    logger.Debug("Async debug message / 异步调试消息");
    logger.Info("Async info message / 异步信息消息");
    logger.Warn("Async warning message / 异步警告消息");
    logger.Error("Async error message / 异步错误消息");
    logger.Critical("Async critical message / 异步严重错误消息");
    
    // Formatted logging with arguments
    // 带参数的格式化日志
    int count = 100;
    double latency = 0.5;
    logger.Info("Processed {} requests with avg latency {:.2f}ms", count, latency);
    logger.Info("处理了 {} 个请求，平均延迟 {:.2f}ms", count, latency);
    
    // Flush to ensure all messages are processed
    // 刷新以确保所有消息都已处理
    logger.Flush();
    
    // Shutdown the logger (stops background thread)
    // 关闭日志器（停止后台线程）
    logger.Shutdown();
    
    std::cout << "Async logger shutdown complete / 异步日志器关闭完成" << std::endl;
}

// ==============================================================================
// Example 2: HeapRingBuffer Capacity Configuration
// 示例 2: HeapRingBuffer 容量配置
// ==============================================================================

/**
 * @brief Demonstrates HeapRingBuffer capacity configuration
 * @brief 演示 HeapRingBuffer 容量配置
 *
 * The HeapRingBuffer capacity determines how many log entries can be
 * queued before the queue full policy takes effect.
 *
 * HeapRingBuffer 容量决定了在队列满策略生效之前可以排队多少日志条目。
 */
void HeapRingBufferCapacityExample() {
    std::cout << "\n=== Example 2: HeapRingBuffer Capacity / HeapRingBuffer 容量 ===" << std::endl;
    
    // Small buffer (1024 slots) - for memory-constrained environments
    // 小缓冲区（1024 槽位）- 用于内存受限环境
    std::cout << "--- Small buffer (1024 slots) ---" << std::endl;
    {
        using SmallBufferConfig = oneplog::LoggerConfig<
            oneplog::Mode::Async,
            oneplog::Level::Debug,
            false,  // EnableWFC
            true,   // EnableShadowTail
            true,   // UseFmt
            1024,   // HeapRingBufferCapacity (small)
            4096,   // SharedRingBufferCapacity
            oneplog::QueueFullPolicy::Block,
            oneplog::DefaultSharedMemoryName,
            10,
            oneplog::DefaultSinkBindings
        >;
        
        oneplog::LoggerImpl<SmallBufferConfig> logger;
        logger.Info("Small buffer logger (1024 slots) / 小缓冲区日志器（1024 槽位）");
        logger.Flush();
        logger.Shutdown();
    }
    
    // Large buffer (65536 slots) - for high-throughput applications
    // 大缓冲区（65536 槽位）- 用于高吞吐量应用
    std::cout << "--- Large buffer (65536 slots) ---" << std::endl;
    {
        using LargeBufferConfig = oneplog::LoggerConfig<
            oneplog::Mode::Async,
            oneplog::Level::Debug,
            false, true, true,
            65536,  // HeapRingBufferCapacity (large)
            4096,
            oneplog::QueueFullPolicy::Block,
            oneplog::DefaultSharedMemoryName, 10,
            oneplog::DefaultSinkBindings
        >;
        
        oneplog::LoggerImpl<LargeBufferConfig> logger;
        logger.Info("Large buffer logger (65536 slots) / 大缓冲区日志器（65536 槽位）");
        logger.Flush();
        logger.Shutdown();
    }
    
    std::cout << "Buffer capacity affects memory usage and burst handling" << std::endl;
    std::cout << "缓冲区容量影响内存使用和突发处理能力" << std::endl;
}

// ==============================================================================
// Example 3: QueueFullPolicy Strategies
// 示例 3: QueueFullPolicy 各策略
// ==============================================================================

/**
 * @brief Demonstrates different QueueFullPolicy strategies
 * @brief 演示不同的 QueueFullPolicy 策略
 *
 * QueueFullPolicy determines what happens when the ring buffer is full:
 * - Block: Wait until space is available (may block producer)
 * - DropNewest: Discard the new message (non-blocking)
 * - DropOldest: Discard the oldest message to make room (non-blocking)
 *
 * QueueFullPolicy 决定当环形队列满时的行为：
 * - Block：等待直到有空间（可能阻塞生产者）
 * - DropNewest：丢弃新消息（非阻塞）
 * - DropOldest：丢弃最旧的消息以腾出空间（非阻塞）
 */
void QueueFullPolicyExample() {
    std::cout << "\n=== Example 3: QueueFullPolicy / 队列满策略 ===" << std::endl;
    
    // Block policy - waits for space (default)
    // Block 策略 - 等待空间（默认）
    std::cout << "--- Block Policy ---" << std::endl;
    std::cout << "Waits for space when queue is full (may block producer)" << std::endl;
    std::cout << "队列满时等待空间（可能阻塞生产者）" << std::endl;
    {
        using BlockConfig = oneplog::LoggerConfig<
            oneplog::Mode::Async,
            oneplog::Level::Debug,
            false, true, true,
            8192, 4096,
            oneplog::QueueFullPolicy::Block,  // Block when full
            oneplog::DefaultSharedMemoryName, 10,
            oneplog::DefaultSinkBindings
        >;
        
        oneplog::LoggerImpl<BlockConfig> logger;
        logger.Info("Block policy: guaranteed delivery / Block 策略：保证投递");
        logger.Flush();
        logger.Shutdown();
    }
    
    // DropNewest policy - discards new messages
    // DropNewest 策略 - 丢弃新消息
    std::cout << "\n--- DropNewest Policy ---" << std::endl;
    std::cout << "Discards new messages when queue is full (non-blocking)" << std::endl;
    std::cout << "队列满时丢弃新消息（非阻塞）" << std::endl;
    {
        using DropNewestConfig = oneplog::LoggerConfig<
            oneplog::Mode::Async,
            oneplog::Level::Debug,
            false, true, true,
            8192, 4096,
            oneplog::QueueFullPolicy::DropNewest,  // Drop new messages
            oneplog::DefaultSharedMemoryName, 10,
            oneplog::DefaultSinkBindings
        >;
        
        oneplog::LoggerImpl<DropNewestConfig> logger;
        logger.Info("DropNewest policy: non-blocking, may lose messages");
        logger.Info("DropNewest 策略：非阻塞，可能丢失消息");
        logger.Flush();
        logger.Shutdown();
    }
    
    // DropOldest policy - discards oldest messages
    // DropOldest 策略 - 丢弃最旧的消息
    std::cout << "\n--- DropOldest Policy ---" << std::endl;
    std::cout << "Discards oldest messages when queue is full (non-blocking)" << std::endl;
    std::cout << "队列满时丢弃最旧的消息（非阻塞）" << std::endl;
    {
        using DropOldestConfig = oneplog::LoggerConfig<
            oneplog::Mode::Async,
            oneplog::Level::Debug,
            false, true, true,
            8192, 4096,
            oneplog::QueueFullPolicy::DropOldest,  // Drop oldest messages
            oneplog::DefaultSharedMemoryName, 10,
            oneplog::DefaultSinkBindings
        >;
        
        oneplog::LoggerImpl<DropOldestConfig> logger;
        logger.Info("DropOldest policy: keeps recent messages");
        logger.Info("DropOldest 策略：保留最新消息");
        logger.Flush();
        logger.Shutdown();
    }
}


// ==============================================================================
// Example 4: ShadowTail Optimization
// 示例 4: ShadowTail 优化
// ==============================================================================

/**
 * @brief Demonstrates ShadowTail optimization
 * @brief 演示 ShadowTail 优化
 *
 * ShadowTail is a producer-side optimization that caches the tail pointer
 * to reduce atomic operations and improve throughput.
 *
 * ShadowTail 是一种生产者侧优化，通过缓存尾指针来减少原子操作并提高吞吐量。
 */
void ShadowTailExample() {
    std::cout << "\n=== Example 4: ShadowTail Optimization / ShadowTail 优化 ===" << std::endl;
    
    // With ShadowTail enabled (default for async)
    // 启用 ShadowTail（异步模式默认启用）
    std::cout << "--- ShadowTail Enabled ---" << std::endl;
    {
        using ShadowTailConfig = oneplog::LoggerConfig<
            oneplog::Mode::Async,
            oneplog::Level::Debug,
            false,
            true,   // EnableShadowTail = true
            true,
            8192, 4096, oneplog::QueueFullPolicy::Block,
            oneplog::DefaultSharedMemoryName, 10,
            oneplog::DefaultSinkBindings
        >;
        
        oneplog::LoggerImpl<ShadowTailConfig> logger;
        
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 10000; ++i) {
            logger.Debug("ShadowTail enabled message {}", i);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        logger.Flush();
        logger.Shutdown();
        
        std::cout << "ShadowTail enabled: " << duration.count() << " us for 10000 messages" << std::endl;
        std::cout << "ShadowTail 启用: " << duration.count() << " us 处理 10000 条消息" << std::endl;
    }
    
    // Without ShadowTail
    // 禁用 ShadowTail
    std::cout << "\n--- ShadowTail Disabled ---" << std::endl;
    {
        using NoShadowTailConfig = oneplog::LoggerConfig<
            oneplog::Mode::Async,
            oneplog::Level::Debug,
            false,
            false,  // EnableShadowTail = false
            true,
            8192, 4096, oneplog::QueueFullPolicy::Block,
            oneplog::DefaultSharedMemoryName, 10,
            oneplog::DefaultSinkBindings
        >;
        
        oneplog::LoggerImpl<NoShadowTailConfig> logger;
        
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 10000; ++i) {
            logger.Debug("ShadowTail disabled message {}", i);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        logger.Flush();
        logger.Shutdown();
        
        std::cout << "ShadowTail disabled: " << duration.count() << " us for 10000 messages" << std::endl;
        std::cout << "ShadowTail 禁用: " << duration.count() << " us 处理 10000 条消息" << std::endl;
    }
    
    std::cout << "\nShadowTail improves throughput by reducing atomic operations" << std::endl;
    std::cout << "ShadowTail 通过减少原子操作来提高吞吐量" << std::endl;
}

// ==============================================================================
// Example 5: WFC (Wait For Completion)
// 示例 5: WFC（等待完成）功能
// ==============================================================================

/**
 * @brief Demonstrates WFC (Wait For Completion) functionality
 * @brief 演示 WFC（等待完成）功能
 *
 * WFC allows waiting for a specific log message to be fully processed
 * before continuing. Useful for critical messages that must be persisted.
 *
 * WFC 允许等待特定日志消息完全处理后再继续。适用于必须持久化的关键消息。
 */
void WFCExample() {
    std::cout << "\n=== Example 5: WFC (Wait For Completion) / WFC（等待完成）===" << std::endl;
    
    // Logger with WFC enabled
    // 启用 WFC 的日志器
    using WFCConfig = oneplog::LoggerConfig<
        oneplog::Mode::Async,
        oneplog::Level::Debug,
        true,   // EnableWFC = true
        true,   // EnableShadowTail
        true,   // UseFmt
        8192, 4096, oneplog::QueueFullPolicy::Block,
        oneplog::DefaultSharedMemoryName, 10,
        oneplog::DefaultSinkBindings
    >;
    
    oneplog::LoggerImpl<WFCConfig> logger;
    
    // Regular async logging (non-blocking)
    // 常规异步日志（非阻塞）
    std::cout << "Regular async logging (non-blocking):" << std::endl;
    std::cout << "常规异步日志（非阻塞）:" << std::endl;
    logger.Info("Regular message 1 / 常规消息 1");
    logger.Info("Regular message 2 / 常规消息 2");
    
    // WFC logging (waits for completion)
    // WFC 日志（等待完成）
    std::cout << "\nWFC logging (waits for completion):" << std::endl;
    std::cout << "WFC 日志（等待完成）:" << std::endl;
    logger.InfoWFC("Critical message - waiting for completion");
    logger.InfoWFC("关键消息 - 等待完成");
    std::cout << "WFC messages have been processed / WFC 消息已处理完成" << std::endl;
    
    // All WFC methods available
    // 所有可用的 WFC 方法
    std::cout << "\nAll WFC methods / 所有 WFC 方法:" << std::endl;
    logger.TraceWFC("TraceWFC message");
    logger.DebugWFC("DebugWFC message");
    logger.InfoWFC("InfoWFC message");
    logger.WarnWFC("WarnWFC message");
    logger.ErrorWFC("ErrorWFC message");
    logger.CriticalWFC("CriticalWFC message");
    
    logger.Flush();
    logger.Shutdown();
    
    std::cout << "\nWFC is useful for critical messages that must be persisted" << std::endl;
    std::cout << "WFC 适用于必须持久化的关键消息" << std::endl;
}

// ==============================================================================
// Example 6: Flush and Shutdown
// 示例 6: Flush 和 Shutdown 使用
// ==============================================================================

/**
 * @brief Demonstrates proper Flush and Shutdown usage
 * @brief 演示正确的 Flush 和 Shutdown 使用方式
 *
 * Flush waits for all queued messages to be processed.
 * Shutdown stops the background thread and closes all sinks.
 *
 * Flush 等待所有排队的消息被处理。
 * Shutdown 停止后台线程并关闭所有 Sink。
 */
void FlushShutdownExample() {
    std::cout << "\n=== Example 6: Flush and Shutdown / Flush 和 Shutdown ===" << std::endl;
    
    oneplog::AsyncLogger logger;
    
    // Queue some messages
    // 排队一些消息
    std::cout << "Queueing messages..." << std::endl;
    std::cout << "排队消息中..." << std::endl;
    for (int i = 0; i < 100; ++i) {
        logger.Debug("Message {} / 消息 {}", i, i);
    }
    
    // Flush - wait for all messages to be processed
    // Flush - 等待所有消息被处理
    std::cout << "Calling Flush()..." << std::endl;
    std::cout << "调用 Flush()..." << std::endl;
    logger.Flush();
    std::cout << "Flush complete - all messages processed" << std::endl;
    std::cout << "Flush 完成 - 所有消息已处理" << std::endl;
    
    // Can continue logging after Flush
    // Flush 后可以继续日志记录
    logger.Info("Message after Flush / Flush 后的消息");
    
    // Shutdown - stops background thread
    // Shutdown - 停止后台线程
    std::cout << "\nCalling Shutdown()..." << std::endl;
    std::cout << "调用 Shutdown()..." << std::endl;
    logger.Shutdown();
    std::cout << "Shutdown complete - background thread stopped" << std::endl;
    std::cout << "Shutdown 完成 - 后台线程已停止" << std::endl;
    
    // Note: After Shutdown, logging may not work as expected
    // 注意：Shutdown 后，日志记录可能无法正常工作
    std::cout << "\nBest practice: Call Flush() before Shutdown()" << std::endl;
    std::cout << "最佳实践：在 Shutdown() 之前调用 Flush()" << std::endl;
}


// ==============================================================================
// Example 7: Multi-threaded Concurrent Logging
// 示例 7: 多线程并发日志记录
// ==============================================================================

/**
 * @brief Demonstrates multi-threaded concurrent logging
 * @brief 演示多线程并发日志记录
 *
 * The async logger is thread-safe and can be used from multiple threads
 * simultaneously without external synchronization.
 *
 * 异步日志器是线程安全的，可以从多个线程同时使用，无需外部同步。
 */
void MultiThreadedExample() {
    std::cout << "\n=== Example 7: Multi-threaded Logging / 多线程日志记录 ===" << std::endl;
    
    oneplog::AsyncLogger logger;
    
    constexpr int kThreadCount = 4;
    constexpr int kMessagesPerThread = 1000;
    
    std::atomic<int> totalMessages{0};
    std::vector<std::thread> threads;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Create multiple producer threads
    // 创建多个生产者线程
    for (int t = 0; t < kThreadCount; ++t) {
        threads.emplace_back([&logger, &totalMessages, t]() {
            for (int i = 0; i < kMessagesPerThread; ++i) {
                logger.Debug("Thread {} message {} / 线程 {} 消息 {}", t, i, t, i);
                totalMessages.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    
    // Wait for all threads to complete
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // Flush and shutdown
    // 刷新并关闭
    logger.Flush();
    logger.Shutdown();
    
    std::cout << "Threads: " << kThreadCount << std::endl;
    std::cout << "线程数: " << kThreadCount << std::endl;
    std::cout << "Messages per thread: " << kMessagesPerThread << std::endl;
    std::cout << "每线程消息数: " << kMessagesPerThread << std::endl;
    std::cout << "Total messages: " << totalMessages.load() << std::endl;
    std::cout << "总消息数: " << totalMessages.load() << std::endl;
    std::cout << "Total time: " << duration.count() << " us" << std::endl;
    std::cout << "总时间: " << duration.count() << " us" << std::endl;
    std::cout << "Throughput: " << (totalMessages.load() * 1000000 / duration.count()) << " msg/s" << std::endl;
    std::cout << "吞吐量: " << (totalMessages.load() * 1000000 / duration.count()) << " msg/s" << std::endl;
}

// ==============================================================================
// Example 8: File and Console Sink Configuration
// 示例 8: 文件和控制台 Sink 配置
// ==============================================================================

/**
 * @brief Demonstrates file and console sink configuration in async mode
 * @brief 演示异步模式下的文件和控制台 Sink 配置
 */
void AsyncSinkConfigExample() {
    std::cout << "\n=== Example 8: Async Sink Configuration / 异步 Sink 配置 ===" << std::endl;
    
    // Async logger with file sink
    // 带文件 Sink 的异步日志器
    std::cout << "--- Async File Sink ---" << std::endl;
    {
        using FileSinkBinding = oneplog::SinkBinding<oneplog::FileSinkType, oneplog::FullFormat>;
        using AsyncFileConfig = oneplog::LoggerConfig<
            oneplog::Mode::Async,
            oneplog::Level::Debug,
            false, true, true,
            8192, 4096, oneplog::QueueFullPolicy::Block,
            oneplog::DefaultSharedMemoryName, 10,
            oneplog::SinkBindingList<FileSinkBinding>
        >;
        
        oneplog::SinkBindingList<FileSinkBinding> bindings(
            FileSinkBinding(oneplog::FileSinkType("async_example.log"))
        );
        oneplog::LoggerImpl<AsyncFileConfig> logger(std::move(bindings));
        
        logger.Info("Async file logging example / 异步文件日志示例");
        logger.Debug("Debug message to file / 调试消息写入文件");
        logger.Flush();
        logger.Shutdown();
    }
    std::cout << "Check async_example.log for output" << std::endl;
    std::cout << "检查 async_example.log 查看输出" << std::endl;
    
    // Async logger with console + file (multi-sink)
    // 带控制台 + 文件的异步日志器（多 Sink）
    std::cout << "\n--- Async Console + File ---" << std::endl;
    {
        using ConsoleSinkBinding = oneplog::SinkBinding<oneplog::ConsoleSinkType, oneplog::SimpleFormat>;
        using FileSinkBinding = oneplog::SinkBinding<oneplog::FileSinkType, oneplog::FullFormat>;
        
        using AsyncMultiConfig = oneplog::LoggerConfig<
            oneplog::Mode::Async,
            oneplog::Level::Debug,
            false, true, true,
            8192, 4096, oneplog::QueueFullPolicy::Block,
            oneplog::DefaultSharedMemoryName, 10,
            oneplog::SinkBindingList<ConsoleSinkBinding, FileSinkBinding>
        >;
        
        oneplog::SinkBindingList<ConsoleSinkBinding, FileSinkBinding> bindings(
            ConsoleSinkBinding(),
            FileSinkBinding(oneplog::FileSinkType("async_multi.log"))
        );
        oneplog::LoggerImpl<AsyncMultiConfig> logger(std::move(bindings));
        
        logger.Info("Async multi-sink: console + file");
        logger.Info("异步多 Sink: 控制台 + 文件");
        logger.Flush();
        logger.Shutdown();
    }
    std::cout << "Check async_multi.log for file output" << std::endl;
    std::cout << "检查 async_multi.log 查看文件输出" << std::endl;
    
    // High performance async logger (NullSink for benchmarking)
    // 高性能异步日志器（NullSink 用于基准测试）
    std::cout << "\n--- High Performance (NullSink) ---" << std::endl;
    {
        oneplog::LoggerImpl<oneplog::HighPerformanceConfig> logger;
        
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 100000; ++i) {
            logger.Info("High performance message {}", i);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        logger.Flush();
        logger.Shutdown();
        
        std::cout << "100000 messages in " << duration.count() << " us" << std::endl;
        std::cout << "100000 条消息用时 " << duration.count() << " us" << std::endl;
        std::cout << "Throughput: " << (100000LL * 1000000LL / duration.count()) << " msg/s" << std::endl;
        std::cout << "吞吐量: " << (100000LL * 1000000LL / duration.count()) << " msg/s" << std::endl;
    }
}

// ==============================================================================
// Main Function / 主函数
// ==============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "onePlog Async Mode Examples" << std::endl;
    std::cout << "onePlog 异步模式示例" << std::endl;
    std::cout << "========================================" << std::endl;
    
    BasicAsyncLoggerExample();
    HeapRingBufferCapacityExample();
    QueueFullPolicyExample();
    ShadowTailExample();
    WFCExample();
    FlushShutdownExample();
    MultiThreadedExample();
    AsyncSinkConfigExample();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "All async mode examples completed!" << std::endl;
    std::cout << "所有异步模式示例完成！" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
