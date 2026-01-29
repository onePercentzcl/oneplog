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
#include <thread>
#include <oneplog/oneplog.hpp>

int main() {
    std::cout << "=== onePlog Sync Mode Example ===" << std::endl;
    std::cout << "=== onePlog 同步模式示例 ===" << std::endl;
    std::cout << std::endl;

    // Initialize with sync mode and process name
    // 使用同步模式和进程名初始化
    oneplog::LoggerConfig config;
    config.mode = oneplog::Mode::Sync;
    config.level = oneplog::Level::Trace;
    config.processName = "sync_app";  // Set process name / 设置进程名
    oneplog::Init(config);

    std::cout << "--- Process/Module Name Demo / 进程名/模块名演示 ---" << std::endl;

    // Set module name for main thread / 设置主线程的模块名
    oneplog::NameManager::SetModuleName("main");
    
    log::Info("Main thread started with process={}, module={}", 
              oneplog::NameManager::GetProcessName(),
              oneplog::NameManager::GetModuleName());

    std::cout << std::endl;
    std::cout << "--- Basic Logging / 基本日志 ---" << std::endl;

    // Use global functions / 使用全局函数
    oneplog::Trace("This is a trace message");
    oneplog::Debug("This is a debug message");
    oneplog::Info("This is an info message");
    oneplog::Warn("This is a warning message");
    oneplog::Error("This is an error message");
    oneplog::Critical("This is a critical message");

    std::cout << std::endl;
    std::cout << "--- Multi-thread with Module Names / 多线程模块名 ---" << std::endl;

    // Create threads with different module names
    // 创建具有不同模块名的线程
    std::thread worker1([]() {
        oneplog::NameManager::SetModuleName("worker1");
        log::Info("Worker 1 processing task");
        log::Debug("Worker 1 debug info");
    });

    std::thread worker2([]() {
        oneplog::NameManager::SetModuleName("worker2");
        log::Info("Worker 2 processing task");
        log::Debug("Worker 2 debug info");
    });

    worker1.join();
    worker2.join();

    std::cout << std::endl;
    std::cout << "--- Thread Inheritance Demo / 线程继承演示 ---" << std::endl;

    // Demonstrate module name inheritance using ThreadWithModuleName
    // 使用 ThreadWithModuleName 演示模块名继承
    oneplog::NameManager::SetModuleName("parent");
    log::Info("Parent thread module name set");

    auto childThread = oneplog::ThreadWithModuleName::Create([]() {
        // Child thread inherits parent's module name
        // 子线程继承父线程的模块名
        log::Info("Child thread inherited module name: {}", 
                  oneplog::NameManager::GetModuleName());
    });
    childThread.join();

    std::cout << std::endl;
    std::cout << "--- Formatted Logging / 格式化日志 ---" << std::endl;

    // Log with format arguments / 带格式化参数的日志
    int value = 42;
    double pi = 3.14159;
    const char* name = "onePlog";

    log::Info("Integer value: {}", value);
    log::Info("Double value: {}", pi);
    log::Info("String value: {}", name);
    log::Info("Multiple values: {} {} {}", value, pi, name);

    std::cout << std::endl;
    std::cout << "--- Using Macros / 使用宏 ---" << std::endl;

    // Use logging macros / 使用日志宏
    ONEPLOG_TRACE("Macro trace message");
    ONEPLOG_DEBUG("Macro debug message");
    ONEPLOG_INFO("Macro info message with value: {}", 123);
    ONEPLOG_WARN("Macro warning message");
    ONEPLOG_ERROR("Macro error message");
    ONEPLOG_CRITICAL("Macro critical message");

    std::cout << std::endl;
    std::cout << "--- Conditional Logging / 条件日志 ---" << std::endl;

    // Conditional logging / 条件日志
    bool condition = true;
    ONEPLOG_INFO_IF(condition, "This message is logged because condition is true");
    ONEPLOG_INFO_IF(!condition, "This message is NOT logged because condition is false");

    std::cout << std::endl;
    std::cout << "--- Level Filtering / 级别过滤 ---" << std::endl;

    // Change log level to filter messages / 更改日志级别以过滤消息
    log::SetLevel(oneplog::Level::Warn);
    std::cout << "Log level set to Warn" << std::endl;

    log::Trace("This trace message will NOT be logged");
    log::Debug("This debug message will NOT be logged");
    log::Info("This info message will NOT be logged");
    log::Warn("This warning message WILL be logged");
    log::Error("This error message WILL be logged");

    // Shutdown / 关闭
    oneplog::Shutdown();

    std::cout << std::endl;
    std::cout << "=== Example Complete / 示例完成 ===" << std::endl;

    return 0;
}
