/**
 * @file wfc_example.cpp
 * @brief WFC (Wait For Completion) example
 * @brief WFC（等待完成）示例
 *
 * This example shows how to use WFC logging with a custom Logger instance.
 * WFC logs are never dropped and block until written.
 *
 * 此示例展示如何使用自定义 Logger 实例进行 WFC 日志记录。
 * WFC 日志永不丢弃，会阻塞直到写入完成。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <oneplog/oneplog.hpp>

int main() {
    std::cout << "=== onePlog WFC Example ===" << std::endl;
    std::cout << "=== onePlog WFC 示例 ===" << std::endl;
    std::cout << std::endl;

    // Create async logger with WFC enabled (EnableWFC=true)
    // 创建启用 WFC 的异步日志器（EnableWFC=true）
    oneplog::Logger<oneplog::Mode::Async, oneplog::Level::Debug, true> logger;
    logger.SetSink(std::make_shared<oneplog::ConsoleSink>());
    logger.SetFormat(std::make_shared<oneplog::ConsoleFormat>());
    logger.Init();

    std::cout << "--- Normal vs WFC Logging / 普通日志 vs WFC 日志 ---" << std::endl;

    // Normal logging (may be dropped if queue is full)
    // 普通日志（队列满时可能被丢弃）
    logger.Info("Normal log message 1");
    logger.Info("Normal log message 2");

    // WFC logging (never dropped, blocks until written)
    // WFC 日志（永不丢弃，阻塞直到写入）
    logger.InfoWFC("WFC log message - guaranteed delivery");
    logger.ErrorWFC("WFC error - critical info never lost");

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::cout << std::endl;
    std::cout << "--- WFC Use Case: Critical Events / WFC 用例：关键事件 ---" << std::endl;

    // Simulate critical events that must not be lost
    // 模拟不能丢失的关键事件
    for (int i = 0; i < 5; ++i) {
        logger.Debug("Regular debug message {}", i);
        
        if (i == 2) {
            // Critical event - use WFC to ensure it's logged
            // 关键事件 - 使用 WFC 确保被记录
            logger.CriticalWFC("CRITICAL: System state changed at iteration {}", i);
        }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::cout << std::endl;
    std::cout << "--- All WFC Levels / 所有 WFC 级别 ---" << std::endl;

    logger.TraceWFC("Trace WFC message");
    logger.DebugWFC("Debug WFC message");
    logger.InfoWFC("Info WFC message");
    logger.WarnWFC("Warn WFC message");
    logger.ErrorWFC("Error WFC message");
    logger.CriticalWFC("Critical WFC message");

    // Shutdown / 关闭
    logger.Flush();
    logger.Shutdown();

    std::cout << std::endl;
    std::cout << "=== Example Complete ===" << std::endl;

    return 0;
}
