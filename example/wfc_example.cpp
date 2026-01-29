/**
 * @file wfc_example.cpp
 * @brief Wait For Completion (WFC) logging example for onePlog
 * @brief onePlog 等待完成 (WFC) 日志示例
 *
 * This example demonstrates how to use WFC logging in onePlog.
 * WFC ensures that the log message is fully written before returning,
 * which is useful for critical logs (e.g., before a crash).
 * 此示例演示如何在 onePlog 中使用 WFC 日志。
 * WFC 确保日志消息在返回前完全写入，
 * 这对于关键日志（例如崩溃前）非常有用。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <chrono>
#include <iostream>
#include <thread>
#include <oneplog/oneplog.hpp>

int main() {
    std::cout << "=== onePlog WFC (Wait For Completion) Example ===" << std::endl;
    std::cout << "=== onePlog WFC（等待完成）示例 ===" << std::endl;
    std::cout << std::endl;

    // Simple initialization / 简单初始化
    oneplog::Init();
    oneplog::SetLevel(oneplog::Level::Trace);

    std::cout << "--- Normal Async Logging / 普通异步日志 ---" << std::endl;

    // Normal async logging (returns immediately)
    // 普通异步日志（立即返回）
    auto start = std::chrono::high_resolution_clock::now();
    log::Info("Normal async message 1");
    log::Info("Normal async message 2");
    log::Info("Normal async message 3");
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Normal async logging took: " << duration.count() << " us" << std::endl;
    std::cout << "普通异步日志耗时: " << duration.count() << " 微秒" << std::endl;

    // Give some time for async processing
    // 给异步处理一些时间
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::cout << std::endl;
    std::cout << "--- WFC Logging / WFC 日志 ---" << std::endl;

    // WFC logging using log:: class (waits for completion)
    // 使用 log:: 类的 WFC 日志（等待完成）
    start = std::chrono::high_resolution_clock::now();
    log::InfoWFC("WFC message - guaranteed to be written");
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "WFC logging took: " << duration.count() << " us" << std::endl;
    std::cout << "WFC 日志耗时: " << duration.count() << " 微秒" << std::endl;

    std::cout << std::endl;
    std::cout << "--- WFC at Different Levels / 不同级别的 WFC ---" << std::endl;

    // WFC at different log levels using log:: class
    // 使用 log:: 类的不同日志级别的 WFC
    log::TraceWFC("WFC trace message");
    log::DebugWFC("WFC debug message");
    log::InfoWFC("WFC info message");
    log::WarnWFC("WFC warning message");
    log::ErrorWFC("WFC error message");
    log::CriticalWFC("WFC critical message");

    std::cout << std::endl;
    std::cout << "--- WFC Macros / WFC 宏 ---" << std::endl;

    // Using WFC macros / 使用 WFC 宏
    ONEPLOG_TRACE_WFC("Macro WFC trace");
    ONEPLOG_DEBUG_WFC("Macro WFC debug");
    ONEPLOG_INFO_WFC("Macro WFC info with value: {}", 42);
    ONEPLOG_WARN_WFC("Macro WFC warning");
    ONEPLOG_ERROR_WFC("Macro WFC error");
    ONEPLOG_CRITICAL_WFC("Macro WFC critical");

    std::cout << std::endl;
    std::cout << "--- Critical Error Scenario / 关键错误场景 ---" << std::endl;

    // Simulate a critical error scenario
    // 模拟关键错误场景
    std::cout << "Simulating critical error..." << std::endl;
    std::cout << "模拟关键错误..." << std::endl;

    // Use WFC for critical errors to ensure they are logged before potential crash
    // 对关键错误使用 WFC 以确保在潜在崩溃前记录
    log::CriticalWFC("CRITICAL: System error detected! Error code: {}", 0xDEAD);
    log::ErrorWFC("ERROR: Attempting recovery...");
    log::InfoWFC("INFO: Recovery successful");

    std::cout << "Critical error handled, all logs guaranteed written" << std::endl;
    std::cout << "关键错误已处理，所有日志保证已写入" << std::endl;

    std::cout << std::endl;
    std::cout << "--- Performance Comparison / 性能对比 ---" << std::endl;

    constexpr int kIterations = 100;

    // Measure normal async logging / 测量普通异步日志
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kIterations; ++i) {
        log::Debug("Async benchmark message {}", i);
    }
    end = std::chrono::high_resolution_clock::now();
    auto asyncDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Wait for async processing / 等待异步处理
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Measure WFC logging / 测量 WFC 日志
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < kIterations; ++i) {
        log::DebugWFC("{}", i);
    }
    end = std::chrono::high_resolution_clock::now();
    auto wfcDuration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Async: " << kIterations << " messages in " << asyncDuration.count() << " us" << std::endl;
    std::cout << "异步: " << kIterations << " 条消息耗时 " << asyncDuration.count() << " 微秒" << std::endl;
    std::cout << "WFC: " << kIterations << " messages in " << wfcDuration.count() << " us" << std::endl;
    std::cout << "WFC: " << kIterations << " 条消息耗时 " << wfcDuration.count() << " 微秒" << std::endl;

    // Shutdown / 关闭
    oneplog::Shutdown();

    std::cout << std::endl;
    std::cout << "=== Example Complete / 示例完成 ===" << std::endl;

    return 0;
}
