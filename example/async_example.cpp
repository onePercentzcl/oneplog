/**
 * @file async_example.cpp
 * @brief Asynchronous mode example using simplified API
 * @brief 使用简化 API 的异步模式示例
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

    // Initialize with simplified API / 使用简化 API 初始化
    oneplog::Init();

    std::cout << "--- Basic Logging / 基本日志 ---" << std::endl;

    // Log at different levels / 不同级别的日志
    oneplog::Trace("This is a trace message");
    oneplog::Debug("This is a debug message");
    oneplog::Info("This is an info message");
    oneplog::Warn("This is a warning message");
    oneplog::Error("This is an error message");
    oneplog::Critical("This is a critical message");

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::cout << std::endl;
    std::cout << "--- Formatted Logging / 格式化日志 ---" << std::endl;

    int value = 42;
    double pi = 3.14159;
    const char* name = "onePlog";

    oneplog::Info("Integer value: {}", value);
    oneplog::Info("Double value: {}", pi);
    oneplog::Info("String value: {}", name);
    oneplog::Info("Multiple values: {} {} {}", value, pi, name);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    std::cout << std::endl;
    std::cout << "--- High Throughput Test / 高吞吐量测试 ---" << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    constexpr int kMessageCount = 1000;
    for (int i = 0; i < kMessageCount; ++i) {
        oneplog::Debug("High throughput message {}", i);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Queued " << kMessageCount << " messages in " << duration.count() << " us" << std::endl;
    std::cout << "Average: " << (duration.count() * 1000 / kMessageCount) << " ns per message" << std::endl;

    // Flush and shutdown / 刷新并关闭
    oneplog::Flush();
    oneplog::Shutdown();

    std::cout << std::endl;
    std::cout << "=== Example Complete ===" << std::endl;

    return 0;
}
