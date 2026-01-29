/**
 * @file sync_example.cpp
 * @brief Synchronous mode example using Logger instance
 * @brief 使用 Logger 实例的同步模式示例
 *
 * This example shows how to use a custom Logger instance
 * when you need non-default configuration (Sync mode).
 *
 * 此示例展示当需要非默认配置（同步模式）时如何使用自定义 Logger 实例。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <iostream>
#include <thread>
#include <oneplog/oneplog.hpp>

int main() {
    std::cout << "=== onePlog Sync Mode Example ===" << std::endl;
    std::cout << "=== onePlog 同步模式示例 ===" << std::endl;
    std::cout << std::endl;

    // Create sync logger instance (non-default mode)
    // 创建同步日志器实例（非默认模式）
    // Using kDefaultLevel: Debug in debug builds, Info in release builds
    // 使用 kDefaultLevel：调试构建为 Debug，发布构建为 Info
    oneplog::Logger<oneplog::Mode::Sync, oneplog::kDefaultLevel, false> logger;
    logger.SetSink(std::make_shared<oneplog::ConsoleSink>());
    logger.SetFormat(std::make_shared<oneplog::ConsoleFormat>());
    logger.Init();

    std::cout << "--- Basic Logging / 基本日志 ---" << std::endl;

    // Log at different levels / 不同级别的日志
    logger.Trace("This is a trace message");
    logger.Debug("This is a debug message");
    logger.Info("This is an info message");
    logger.Warn("This is a warning message");
    logger.Error("This is an error message");
    logger.Critical("This is a critical message");

    std::cout << std::endl;
    std::cout << "--- Multi-thread Demo / 多线程演示 ---" << std::endl;

    std::thread worker1([&logger]() {
        logger.Info("Worker 1 processing task");
        logger.Debug("Worker 1 debug info");
    });

    std::thread worker2([&logger]() {
        logger.Info("Worker 2 processing task");
        logger.Debug("Worker 2 debug info");
    });

    worker1.join();
    worker2.join();

    std::cout << std::endl;
    std::cout << "--- Formatted Logging / 格式化日志 ---" << std::endl;

    int value = 42;
    double pi = 3.14159;
    const char* name = "onePlog";

    logger.Info("Integer value: {}", value);
    logger.Info("Double value: {}", pi);
    logger.Info("String value: {}", name);
    logger.Info("Multiple values: {} {} {}", value, pi, name);

    // Shutdown / 关闭
    logger.Shutdown();

    std::cout << std::endl;
    std::cout << "=== Example Complete ===" << std::endl;

    return 0;
}
