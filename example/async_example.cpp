/**
 * @file async_example.cpp
 * @brief Asynchronous mode example for onePlog
 * @brief onePlog 异步模式示例
 *
 * This example demonstrates how to use onePlog in asynchronous mode.
 * In async mode, logs are passed to a background thread via lock-free queue.
 * 此示例演示如何在异步模式下使用 onePlog。
 * 在异步模式下，日志通过无锁队列传递给后台线程。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <chrono>
#include <iostream>
#include <thread>
#include <oneplog/oneplog.hpp>

int main() {
    std::cout << "=== onePlog Async Mode Example ===" << std::endl;
    std::cout << "=== onePlog 异步模式示例 ===" << std::endl;
    std::cout << std::endl;

    // Create a logger in async mode (default)
    // 创建异步模式的日志器（默认）
    auto logger = std::make_shared<oneplog::Logger>("async_logger", oneplog::Mode::Async);

    // Create a console sink
    // 创建控制台输出
    auto consoleSink = std::make_shared<oneplog::ConsoleSink>();
    logger->SetSink(consoleSink);

    // Set log level
    // 设置日志级别
    logger->SetLevel(oneplog::Level::Debug);

    // Configure with custom settings
    // 使用自定义设置配置
    oneplog::LoggerConfig config;
    config.mode = oneplog::Mode::Async;
    config.heapRingBufferSize = 1024;  // Buffer size / 缓冲区大小

    // Initialize the logger
    // 初始化日志器
    logger->Init(config);

    // Set as default logger
    // 设置为默认日志器
    oneplog::SetDefaultLogger(logger);

    std::cout << "--- Async Logging / 异步日志 ---" << std::endl;

    // Log messages (they will be processed asynchronously)
    // 记录日志（它们将被异步处理）
    for (int i = 0; i < 10; ++i) {
        logger->Info("Async message {}", i);
    }

    std::cout << "All messages queued, waiting for processing..." << std::endl;
    std::cout << "所有消息已入队，等待处理..." << std::endl;

    // Give some time for async processing
    // 给异步处理一些时间
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << std::endl;
    std::cout << "--- High Throughput Test / 高吞吐量测试 ---" << std::endl;

    // High throughput logging
    // 高吞吐量日志
    auto start = std::chrono::high_resolution_clock::now();

    constexpr int kMessageCount = 1000;
    for (int i = 0; i < kMessageCount; ++i) {
        logger->Debug("High throughput message {}", i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Queued " << kMessageCount << " messages in " << duration.count() << " us" << std::endl;
    std::cout << "入队 " << kMessageCount << " 条消息耗时 " << duration.count() << " 微秒" << std::endl;
    std::cout << "Average: " << (duration.count() / kMessageCount) << " us per message" << std::endl;
    std::cout << "平均: 每条消息 " << (duration.count() / kMessageCount) << " 微秒" << std::endl;

    std::cout << std::endl;
    std::cout << "--- Multi-threaded Logging / 多线程日志 ---" << std::endl;

    // Multi-threaded logging
    // 多线程日志
    constexpr int kThreadCount = 4;
    constexpr int kMessagesPerThread = 100;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreadCount; ++t) {
        threads.emplace_back([&logger, t]() {
            for (int i = 0; i < kMessagesPerThread; ++i) {
                logger->Info("Thread {} message {}", t, i);
            }
        });
    }

    // Wait for all threads to complete
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "All threads completed" << std::endl;
    std::cout << "所有线程已完成" << std::endl;

    // Flush to ensure all messages are written
    // 刷新以确保所有消息都被写入
    logger->Flush();

    std::cout << std::endl;
    std::cout << "--- Using Global Functions / 使用全局函数 ---" << std::endl;

    // Use global convenience functions
    // 使用全局便捷函数
    oneplog::Info("Global info message");
    oneplog::Warn("Global warning message");
    oneplog::Error("Global error message");

    // Flush and shutdown
    // 刷新并关闭
    oneplog::Flush();
    oneplog::Shutdown();

    std::cout << std::endl;
    std::cout << "=== Example Complete / 示例完成 ===" << std::endl;

    return 0;
}
