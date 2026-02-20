/**
 * @file fast_logger_example.cpp
 * @brief FastLoggerV2 usage examples
 * @brief FastLoggerV2 使用示例
 *
 * This example demonstrates the various ways to use FastLoggerV2,
 * the high-performance template-based logger in oneplog.
 *
 * 此示例演示了使用 FastLoggerV2 的各种方式，
 * FastLoggerV2 是 oneplog 中的高性能模板化日志器。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <oneplog/fast_logger_v2.hpp>

// ==============================================================================
// Example 1: Basic Usage / 基本用法
// ==============================================================================

void BasicUsageExample() {
    std::cout << "\n=== Example 1: Basic Usage / 基本用法 ===" << std::endl;
    
    // Create a sync logger with default settings
    // 使用默认设置创建同步日志器
    oneplog::SyncLogger logger;
    
    // Log at different levels
    // 不同级别的日志
    logger.Trace("This is a trace message");
    logger.Debug("This is a debug message");
    logger.Info("This is an info message");
    logger.Warn("This is a warning message");
    logger.Error("This is an error message");
    logger.Critical("This is a critical message");
    
    // Formatted logging
    // 格式化日志
    int value = 42;
    double pi = 3.14159;
    const char* name = "FastLoggerV2";
    
    logger.Info("Integer value: {}", value);
    logger.Info("Double value: {}", pi);
    logger.Info("String value: {}", name);
    logger.Info("Multiple values: {} {} {}", value, pi, name);
    
    logger.Flush();
}

// ==============================================================================
// Example 2: Preset Configurations / 预设配置
// ==============================================================================

void PresetConfigExample() {
    std::cout << "\n=== Example 2: Preset Configurations / 预设配置 ===" << std::endl;
    
    // Sync logger (direct output, no background thread)
    // 同步日志器（直接输出，无后台线程）
    std::cout << "--- SyncLogger ---" << std::endl;
    oneplog::SyncLogger syncLogger;
    syncLogger.Info("SyncLogger: Direct output");
    syncLogger.Flush();
    
    // Async logger (background thread processing)
    // 异步日志器（后台线程处理）
    std::cout << "--- AsyncLogger ---" << std::endl;
    oneplog::AsyncLogger asyncLogger;
    asyncLogger.Info("AsyncLogger: Background processing");
    asyncLogger.Flush();
    asyncLogger.Shutdown();
}

// ==============================================================================
// Example 3: Custom Configuration / 自定义配置
// ==============================================================================

void CustomConfigExample() {
    std::cout << "\n=== Example 3: Custom Configuration / 自定义配置 ===" << std::endl;
    
    // Custom logger with specific settings using FastLoggerConfig
    // 使用 FastLoggerConfig 具有特定设置的自定义日志器
    using CustomConfig = oneplog::FastLoggerConfig<
        oneplog::Mode::Sync,           // Sync mode / 同步模式
        oneplog::Level::Debug,         // Minimum level / 最小级别
        false,                         // EnableWFC
        false,                         // EnableShadowTail
        true,                          // UseFmt
        8192,                          // HeapRingBufferCapacity
        4096,                          // SharedRingBufferCapacity
        oneplog::QueueFullPolicy::DropNewest,
        oneplog::DefaultSharedMemoryName,
        10,                            // PollTimeoutMs
        oneplog::SinkBindingList<      // Custom sink bindings
            oneplog::SinkBinding<oneplog::ConsoleSinkType, oneplog::FullFormat>
        >
    >;
    
    oneplog::FastLoggerV2<CustomConfig> logger;
    logger.Debug("Custom logger with Debug level");
    logger.Info("Custom logger info message");
    logger.Flush();
}

// ==============================================================================
// Example 4: Level Filtering / 级别过滤
// ==============================================================================

void LevelFilteringExample() {
    std::cout << "\n=== Example 4: Level Filtering / 级别过滤 ===" << std::endl;
    
    // Logger with Warn minimum level
    // 最小级别为 Warn 的日志器
    using WarnConfig = oneplog::FastLoggerConfig<
        oneplog::Mode::Sync,
        oneplog::Level::Warn,          // Minimum level = Warn
        false, false, true,
        8192, 4096, oneplog::QueueFullPolicy::DropNewest,
        oneplog::DefaultSharedMemoryName, 10,
        oneplog::SinkBindingList<
            oneplog::SinkBinding<oneplog::ConsoleSinkType, oneplog::SimpleFormat>
        >
    >;
    
    oneplog::FastLoggerV2<WarnConfig> logger;
    
    std::cout << "Logging with minimum level = Warn:" << std::endl;
    logger.Debug("This Debug message won't appear (filtered at compile time)");
    logger.Info("This Info message won't appear (filtered at compile time)");
    logger.Warn("This Warn message will appear");
    logger.Error("This Error message will appear");
    logger.Critical("This Critical message will appear");
    logger.Flush();
}

// ==============================================================================
// Example 5: Multi-Sink Output / 多 Sink 输出
// ==============================================================================

void MultiSinkExample() {
    std::cout << "\n=== Example 5: Multi-Sink Output / 多 Sink 输出 ===" << std::endl;
    
    // Create a multi-sink config that outputs to both console and stderr
    // 创建同时输出到控制台和 stderr 的多 Sink 配置
    using MultiSinkConfig = oneplog::FastLoggerConfig<
        oneplog::Mode::Sync,
        oneplog::Level::Info,
        false, false, true,
        8192, 4096, oneplog::QueueFullPolicy::DropNewest,
        oneplog::DefaultSharedMemoryName, 10,
        oneplog::SinkBindingList<
            oneplog::SinkBinding<oneplog::ConsoleSinkType, oneplog::SimpleFormat>,
            oneplog::SinkBinding<oneplog::StderrSinkType, oneplog::SimpleFormat>
        >
    >;
    
    oneplog::FastLoggerV2<MultiSinkConfig> logger;
    logger.Info("This message goes to both stdout and stderr");
    logger.Flush();
}

// ==============================================================================
// Example 6: Async Mode with High Throughput / 异步模式高吞吐量
// ==============================================================================

void AsyncHighThroughputExample() {
    std::cout << "\n=== Example 6: Async High Throughput / 异步高吞吐量 ===" << std::endl;
    
    oneplog::AsyncLogger logger;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    constexpr int kMessageCount = 1000;
    for (int i = 0; i < kMessageCount; ++i) {
        logger.Debug("High throughput message {}", i);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Queued " << kMessageCount << " messages in " << duration.count() << " us" << std::endl;
    std::cout << "Average: " << (duration.count() * 1000 / kMessageCount) << " ns per message" << std::endl;
    
    // Wait for all messages to be processed
    // 等待所有消息处理完成
    logger.Flush();
    logger.Shutdown();
}

// ==============================================================================
// Example 7: Logger Template Alias / Logger 模板别名
// ==============================================================================

void LoggerAliasExample() {
    std::cout << "\n=== Example 7: Logger Template Alias / Logger 模板别名 ===" << std::endl;
    
    // Using the Logger alias (backward compatible template)
    // 使用 Logger 别名（向后兼容的模板）
    oneplog::Logger<oneplog::Mode::Sync, oneplog::Level::Debug> logger;
    
    logger.Debug("Using Logger alias for backward compatibility");
    logger.Info("This works like the old API");
    logger.Flush();
}

// ==============================================================================
// Example 8: High Performance Config / 高性能配置
// ==============================================================================

void HighPerformanceExample() {
    std::cout << "\n=== Example 8: High Performance Config / 高性能配置 ===" << std::endl;
    
    // Using the HighPerformanceConfig preset (NullSink + MessageOnlyFormat)
    // 使用 HighPerformanceConfig 预设（NullSink + MessageOnlyFormat）
    oneplog::FastLoggerV2<oneplog::HighPerformanceConfig> logger;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    constexpr int kMessageCount = 10000;
    for (int i = 0; i < kMessageCount; ++i) {
        logger.Info("High performance message {}", i);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    std::cout << "Logged " << kMessageCount << " messages in " << duration.count() << " us" << std::endl;
    std::cout << "Average: " << (duration.count() * 1000 / kMessageCount) << " ns per message" << std::endl;
    std::cout << "(Output discarded by NullSink for maximum throughput)" << std::endl;
    
    logger.Flush();
    logger.Shutdown();
}

// ==============================================================================
// Main Function / 主函数
// ==============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "FastLoggerV2 Examples / FastLoggerV2 示例" << std::endl;
    std::cout << "========================================" << std::endl;
    
    BasicUsageExample();
    PresetConfigExample();
    CustomConfigExample();
    LevelFilteringExample();
    MultiSinkExample();
    AsyncHighThroughputExample();
    LoggerAliasExample();
    HighPerformanceExample();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "All examples completed / 所有示例完成" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
