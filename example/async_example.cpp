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

    // Initialize with async mode and process name
    // 使用异步模式和进程名初始化
    oneplog::LoggerConfig config;
    config.mode = oneplog::Mode::Async;
    config.level = oneplog::Level::Debug;
    config.processName = "async_app";  // Set process name / 设置进程名
    oneplog::Init(config);

    std::cout << "--- Process/Module Name Demo / 进程名/模块名演示 ---" << std::endl;

    // Set module name for main thread / 设置主线程的模块名
    oneplog::NameManager::SetModuleName("main");
    
    log::Info("Async mode initialized with process={}, module={}", 
              oneplog::NameManager::GetProcessName(),
              oneplog::NameManager::GetModuleName());

    std::cout << std::endl;
    std::cout << "--- Async Logging / 异步日志 ---" << std::endl;

    // Log messages using log:: class
    // 使用 log:: 类记录日志
    for (int i = 0; i < 10; ++i) {
        log::Info("Async message {}", i);
    }

    std::cout << "All messages queued, waiting for processing..." << std::endl;
    std::cout << "所有消息已入队，等待处理..." << std::endl;

    // Give some time for async processing
    // 给异步处理一些时间
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << std::endl;
    std::cout << "--- Multi-threaded with Module Names / 多线程模块名 ---" << std::endl;

    // Multi-threaded logging with different module names
    // 使用不同模块名的多线程日志
    constexpr int kThreadCount = 4;
    constexpr int kMessagesPerThread = 10;
    const char* moduleNames[] = {"network", "database", "cache", "worker"};

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreadCount; ++t) {
        threads.emplace_back([t, &moduleNames]() {
            // Set module name for this thread / 设置此线程的模块名
            oneplog::NameManager::SetModuleName(moduleNames[t]);
            
            for (int i = 0; i < kMessagesPerThread; ++i) {
                log::Info("[{}] Processing task {}", moduleNames[t], i);
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
    log::Flush();

    std::cout << std::endl;
    std::cout << "--- High Throughput Test / 高吞吐量测试 ---" << std::endl;

    // High throughput logging
    // 高吞吐量日志
    auto start = std::chrono::high_resolution_clock::now();

    constexpr int kMessageCount = 1000;
    for (int i = 0; i < kMessageCount; ++i) {
        log::Debug("High throughput message {}", i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Queued " << kMessageCount << " messages in " << duration.count() << " us" << std::endl;
    std::cout << "入队 " << kMessageCount << " 条消息耗时 " << duration.count() << " 微秒" << std::endl;
    std::cout << "Average: " << (duration.count() / kMessageCount) << " us per message" << std::endl;
    std::cout << "平均: 每条消息 " << (duration.count() / kMessageCount) << " 微秒" << std::endl;

    std::cout << std::endl;
    std::cout << "--- Using Global Functions / 使用全局函数 ---" << std::endl;

    // Use global convenience functions
    // 使用全局便捷函数
    oneplog::Info("Global info message");
    oneplog::Warn("Global warning message");
    oneplog::Error("Global error message");

    // Shutdown / 关闭
    oneplog::Shutdown();

    std::cout << std::endl;
    std::cout << "=== Example Complete / 示例完成 ===" << std::endl;

    return 0;
}
