/**
 * @file sync_example.cpp
 * @brief Synchronous mode example for onePlog
 * @brief onePlog 同步模式示例
 *
 * This example demonstrates how to use onePlog in synchronous mode.
 * In sync mode, logs are written directly in the calling thread.
 * 此示例演示如何在同步模式下使用 onePlog。
 * 在同步模式下，日志直接在调用线程中写入。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <iostream>
#include <oneplog/oneplog.hpp>

int main() {
    std::cout << "=== onePlog Sync Mode Example ===" << std::endl;
    std::cout << "=== onePlog 同步模式示例 ===" << std::endl;
    std::cout << std::endl;

    // Create a logger in sync mode
    // 创建同步模式的日志器
    auto logger = std::make_shared<oneplog::Logger>("sync_logger", oneplog::Mode::Sync);

    // Create a console sink
    // 创建控制台输出
    auto consoleSink = std::make_shared<oneplog::ConsoleSink>();
    logger->SetSink(consoleSink);

    // Set log level to Trace to see all logs
    // 设置日志级别为 Trace 以查看所有日志
    logger->SetLevel(oneplog::Level::Trace);

    // Initialize the logger
    // 初始化日志器
    logger->Init();

    // Set as default logger
    // 设置为默认日志器
    oneplog::SetDefaultLogger(logger);

    std::cout << "--- Basic Logging / 基本日志 ---" << std::endl;

    // Log at different levels
    // 不同级别的日志
    logger->Trace("This is a trace message");
    logger->Debug("This is a debug message");
    logger->Info("This is an info message");
    logger->Warn("This is a warning message");
    logger->Error("This is an error message");
    logger->Critical("This is a critical message");

    std::cout << std::endl;
    std::cout << "--- Formatted Logging / 格式化日志 ---" << std::endl;

    // Log with format arguments
    // 带格式化参数的日志
    int value = 42;
    double pi = 3.14159;
    const char* name = "onePlog";

    logger->Info("Integer value: {}", value);
    logger->Info("Double value: {}", pi);
    logger->Info("String value: {}", name);
    logger->Info("Multiple values: {} {} {}", value, pi, name);

    std::cout << std::endl;
    std::cout << "--- Using Macros / 使用宏 ---" << std::endl;

    // Use logging macros
    // 使用日志宏
    ONEPLOG_TRACE("Macro trace message");
    ONEPLOG_DEBUG("Macro debug message");
    ONEPLOG_INFO("Macro info message with value: {}", 123);
    ONEPLOG_WARN("Macro warning message");
    ONEPLOG_ERROR("Macro error message");
    ONEPLOG_CRITICAL("Macro critical message");

    std::cout << std::endl;
    std::cout << "--- Conditional Logging / 条件日志 ---" << std::endl;

    // Conditional logging
    // 条件日志
    bool condition = true;
    ONEPLOG_INFO_IF(condition, "This message is logged because condition is true");
    ONEPLOG_INFO_IF(!condition, "This message is NOT logged because condition is false");

    std::cout << std::endl;
    std::cout << "--- Level Filtering / 级别过滤 ---" << std::endl;

    // Change log level to filter messages
    // 更改日志级别以过滤消息
    logger->SetLevel(oneplog::Level::Warn);
    std::cout << "Log level set to Warn" << std::endl;

    logger->Trace("This trace message will NOT be logged");
    logger->Debug("This debug message will NOT be logged");
    logger->Info("This info message will NOT be logged");
    logger->Warn("This warning message WILL be logged");
    logger->Error("This error message WILL be logged");

    // Shutdown the logger
    // 关闭日志器
    logger->Shutdown();

    std::cout << std::endl;
    std::cout << "=== Example Complete / 示例完成 ===" << std::endl;

    return 0;
}
