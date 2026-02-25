/**
 * @file example_sync.cpp
 * @brief Synchronous mode example for onePlog
 * @brief onePlog 同步模式示例
 *
 * This example demonstrates the synchronous logging mode of onePlog,
 * including various configurations and usage patterns.
 *
 * 此示例演示 onePlog 的同步日志模式，包括各种配置和使用模式。
 *
 * Features demonstrated / 演示的功能:
 * - Default sync logger basic usage / 默认同步日志器基本用法
 * - Custom log level configuration / 自定义日志级别配置
 * - Console and file sink configuration / 控制台和文件 Sink 配置
 * - File rotation / 文件轮转
 * - Multi-sink output / 多 Sink 同时输出
 * - Various format types / 各种格式化器
 * - RuntimeConfig usage / RuntimeConfig 运行时配置
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <iostream>
#include <oneplog/oneplog.hpp>

// ==============================================================================
// Example 1: Default Sync Logger Basic Usage
// 示例 1: 默认同步日志器基本用法
// ==============================================================================

/**
 * @brief Demonstrates basic usage of the default sync logger
 * @brief 演示默认同步日志器的基本用法
 *
 * The SyncLogger is the simplest way to use onePlog. It writes logs
 * directly to the console without any background thread.
 *
 * SyncLogger 是使用 onePlog 最简单的方式。它直接将日志写入控制台，
 * 不使用任何后台线程。
 */
void BasicSyncLoggerExample() {
    std::cout << "\n=== Example 1: Default Sync Logger / 默认同步日志器 ===" << std::endl;
    
    // Create a sync logger with default settings
    // 使用默认设置创建同步日志器
    oneplog::SyncLogger logger;
    
    // Log messages at different levels
    // 不同级别的日志消息
    logger.Trace("This is a trace message / 这是一条跟踪消息");
    logger.Debug("This is a debug message / 这是一条调试消息");
    logger.Info("This is an info message / 这是一条信息消息");
    logger.Warn("This is a warning message / 这是一条警告消息");
    logger.Error("This is an error message / 这是一条错误消息");
    logger.Critical("This is a critical message / 这是一条严重错误消息");
    
    // Formatted logging with arguments
    // 带参数的格式化日志
    int count = 42;
    double value = 3.14159;
    const char* name = "onePlog";
    
    logger.Info("Integer: {}", count);
    logger.Info("Double: {:.2f}", value);
    logger.Info("String: {}", name);
    logger.Info("Multiple values: count={}, value={:.3f}, name={}", count, value, name);
    
    // Flush to ensure all output is written
    // 刷新以确保所有输出都已写入
    logger.Flush();
}

// ==============================================================================
// Example 2: Custom Log Level Configuration
// 示例 2: 自定义日志级别配置
// ==============================================================================

/**
 * @brief Demonstrates compile-time log level filtering
 * @brief 演示编译期日志级别过滤
 *
 * Log level filtering is done at compile time, so filtered messages
 * have zero runtime overhead.
 *
 * 日志级别过滤在编译期完成，因此被过滤的消息没有运行时开销。
 */
void CustomLogLevelExample() {
    std::cout << "\n=== Example 2: Custom Log Level / 自定义日志级别 ===" << std::endl;
    
    // Logger with minimum level = Warn (filters out Trace, Debug, Info)
    // 最小级别为 Warn 的日志器（过滤掉 Trace、Debug、Info）
    using WarnLevelConfig = oneplog::LoggerConfig<
        oneplog::Mode::Sync,
        oneplog::Level::Warn,  // Minimum level / 最小级别
        false,                 // EnableWFC
        false,                 // EnableShadowTail
        true,                  // UseFmt
        8192,                  // HeapRingBufferCapacity
        4096,                  // SharedRingBufferCapacity
        oneplog::QueueFullPolicy::Block,
        oneplog::DefaultSharedMemoryName,
        10,                    // PollTimeoutMs
        oneplog::DefaultSinkBindings
    >;
    
    oneplog::LoggerImpl<WarnLevelConfig> warnLogger;
    
    std::cout << "Logger with minimum level = Warn:" << std::endl;
    std::cout << "日志器最小级别 = Warn:" << std::endl;
    
    // These messages will be filtered at compile time (no output)
    // 这些消息将在编译期被过滤（无输出）
    warnLogger.Trace("Trace: filtered / 跟踪: 已过滤");
    warnLogger.Debug("Debug: filtered / 调试: 已过滤");
    warnLogger.Info("Info: filtered / 信息: 已过滤");
    
    // These messages will be output
    // 这些消息将被输出
    warnLogger.Warn("Warn: visible / 警告: 可见");
    warnLogger.Error("Error: visible / 错误: 可见");
    warnLogger.Critical("Critical: visible / 严重: 可见");
    
    warnLogger.Flush();
    
    // Logger with minimum level = Error
    // 最小级别为 Error 的日志器
    using ErrorLevelConfig = oneplog::LoggerConfig<
        oneplog::Mode::Sync,
        oneplog::Level::Error,
        false, false, true,
        8192, 4096, oneplog::QueueFullPolicy::Block,
        oneplog::DefaultSharedMemoryName, 10,
        oneplog::DefaultSinkBindings
    >;
    
    oneplog::LoggerImpl<ErrorLevelConfig> errorLogger;
    
    std::cout << "\nLogger with minimum level = Error:" << std::endl;
    std::cout << "日志器最小级别 = Error:" << std::endl;
    
    errorLogger.Warn("Warn: filtered / 警告: 已过滤");
    errorLogger.Error("Error: visible / 错误: 可见");
    errorLogger.Critical("Critical: visible / 严重: 可见");
    
    errorLogger.Flush();
}

// ==============================================================================
// Example 3: Console Sink Configuration
// 示例 3: 控制台 Sink 配置
// ==============================================================================

/**
 * @brief Demonstrates console sink with different formats
 * @brief 演示使用不同格式的控制台 Sink
 */
void ConsoleSinkExample() {
    std::cout << "\n=== Example 3: Console Sink / 控制台 Sink ===" << std::endl;
    
    // Console with SimpleFormat (default)
    // 使用 SimpleFormat 的控制台（默认）
    std::cout << "--- SimpleFormat ---" << std::endl;
    {
        using SimpleConfig = oneplog::LoggerConfig<
            oneplog::Mode::Sync,
            oneplog::Level::Debug,
            false, false, true,
            8192, 4096, oneplog::QueueFullPolicy::Block,
            oneplog::DefaultSharedMemoryName, 10,
            oneplog::SinkBindingList<
                oneplog::SinkBinding<oneplog::ConsoleSinkType, oneplog::SimpleFormat>
            >
        >;
        oneplog::LoggerImpl<SimpleConfig> logger;
        logger.Info("SimpleFormat: [HH:MM:SS] [LEVEL] message");
        logger.Flush();
    }
    
    // Console with FullFormat (includes PID:TID)
    // 使用 FullFormat 的控制台（包含 PID:TID）
    std::cout << "--- FullFormat ---" << std::endl;
    {
        using FullConfig = oneplog::LoggerConfig<
            oneplog::Mode::Sync,
            oneplog::Level::Debug,
            false, false, true,
            8192, 4096, oneplog::QueueFullPolicy::Block,
            oneplog::DefaultSharedMemoryName, 10,
            oneplog::SinkBindingList<
                oneplog::SinkBinding<oneplog::ConsoleSinkType, oneplog::FullFormat>
            >
        >;
        oneplog::LoggerImpl<FullConfig> logger;
        logger.Info("FullFormat: [YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [PID:TID] message");
        logger.Flush();
    }
    
    // Console with MessageOnlyFormat (no metadata)
    // 使用 MessageOnlyFormat 的控制台（无元数据）
    std::cout << "--- MessageOnlyFormat ---" << std::endl;
    {
        using MessageOnlyConfig = oneplog::LoggerConfig<
            oneplog::Mode::Sync,
            oneplog::Level::Debug,
            false, false, true,
            8192, 4096, oneplog::QueueFullPolicy::Block,
            oneplog::DefaultSharedMemoryName, 10,
            oneplog::SinkBindingList<
                oneplog::SinkBinding<oneplog::ConsoleSinkType, oneplog::MessageOnlyFormat>
            >
        >;
        oneplog::LoggerImpl<MessageOnlyConfig> logger;
        logger.Info("MessageOnlyFormat: just the message, no metadata");
        logger.Flush();
    }
    
    // Stderr sink
    // 标准错误 Sink
    std::cout << "--- StderrSink ---" << std::endl;
    {
        using StderrConfig = oneplog::LoggerConfig<
            oneplog::Mode::Sync,
            oneplog::Level::Debug,
            false, false, true,
            8192, 4096, oneplog::QueueFullPolicy::Block,
            oneplog::DefaultSharedMemoryName, 10,
            oneplog::SinkBindingList<
                oneplog::SinkBinding<oneplog::StderrSinkType, oneplog::SimpleFormat>
            >
        >;
        oneplog::LoggerImpl<StderrConfig> logger;
        logger.Error("This message goes to stderr / 此消息输出到 stderr");
        logger.Flush();
    }
}

// ==============================================================================
// Example 4: File Sink Configuration with Rotation
// 示例 4: 文件 Sink 配置（包括文件轮转）
// ==============================================================================

/**
 * @brief Demonstrates file sink with rotation support
 * @brief 演示支持轮转的文件 Sink
 *
 * File rotation helps manage log file sizes by automatically
 * creating new files when the current one reaches a size limit.
 *
 * 文件轮转通过在当前文件达到大小限制时自动创建新文件来帮助管理日志文件大小。
 */
void FileSinkExample() {
    std::cout << "\n=== Example 4: File Sink with Rotation / 文件 Sink 与轮转 ===" << std::endl;
    
    // Basic file sink (no rotation)
    // 基本文件 Sink（无轮转）
    std::cout << "Creating basic file sink: sync_example.log" << std::endl;
    std::cout << "创建基本文件 Sink: sync_example.log" << std::endl;
    {
        // Create file sink binding with SimpleFormat
        // 创建使用 SimpleFormat 的文件 Sink 绑定
        using FileSinkBinding = oneplog::SinkBinding<oneplog::FileSinkType, oneplog::SimpleFormat>;
        using FileConfig = oneplog::LoggerConfig<
            oneplog::Mode::Sync,
            oneplog::Level::Debug,
            false, false, true,
            8192, 4096, oneplog::QueueFullPolicy::Block,
            oneplog::DefaultSharedMemoryName, 10,
            oneplog::SinkBindingList<FileSinkBinding>
        >;
        
        // Create logger with file sink
        // 创建带文件 Sink 的日志器
        oneplog::SinkBindingList<FileSinkBinding> bindings(
            FileSinkBinding(oneplog::FileSinkType("sync_example.log"))
        );
        oneplog::LoggerImpl<FileConfig> logger(std::move(bindings));
        
        logger.Info("Basic file sink example / 基本文件 Sink 示例");
        logger.Debug("Debug message to file / 调试消息写入文件");
        logger.Warn("Warning message to file / 警告消息写入文件");
        logger.Flush();
    }
    std::cout << "Check sync_example.log for output" << std::endl;
    std::cout << "检查 sync_example.log 查看输出" << std::endl;
    
    // File sink with rotation
    // 带轮转的文件 Sink
    std::cout << "\nCreating file sink with rotation: sync_rotate.log" << std::endl;
    std::cout << "创建带轮转的文件 Sink: sync_rotate.log" << std::endl;
    {
        // Configure file sink with rotation
        // 配置带轮转的文件 Sink
        oneplog::StaticFileSinkConfig fileConfig;
        fileConfig.filename = "sync_rotate.log";
        fileConfig.maxSize = 1024;  // 1KB for demo (use larger in production)
        fileConfig.maxFiles = 3;    // Keep 3 rotated files
        fileConfig.rotateOnOpen = false;
        
        using FileSinkBinding = oneplog::SinkBinding<oneplog::FileSinkType, oneplog::SimpleFormat>;
        using FileConfig = oneplog::LoggerConfig<
            oneplog::Mode::Sync,
            oneplog::Level::Debug,
            false, false, true,
            8192, 4096, oneplog::QueueFullPolicy::Block,
            oneplog::DefaultSharedMemoryName, 10,
            oneplog::SinkBindingList<FileSinkBinding>
        >;
        
        oneplog::SinkBindingList<FileSinkBinding> bindings{
            FileSinkBinding{oneplog::FileSinkType{fileConfig}}
        };
        oneplog::LoggerImpl<FileConfig> logger{std::move(bindings)};
        
        // Write enough messages to trigger rotation
        // 写入足够多的消息以触发轮转
        for (int i = 0; i < 50; ++i) {
            logger.Info("Rotation test message {} / 轮转测试消息 {}", i, i);
        }
        logger.Flush();
    }
    std::cout << "Check sync_rotate.log and sync_rotate.log.1, .2, .3 for rotated files" << std::endl;
    std::cout << "检查 sync_rotate.log 和 sync_rotate.log.1, .2, .3 查看轮转文件" << std::endl;
}

// ==============================================================================
// Example 5: Multi-Sink Output
// 示例 5: 多 Sink 同时输出
// ==============================================================================

/**
 * @brief Demonstrates logging to multiple sinks simultaneously
 * @brief 演示同时输出到多个 Sink
 *
 * Multi-sink output allows sending logs to different destinations
 * with potentially different formats.
 *
 * 多 Sink 输出允许将日志发送到不同的目标，可以使用不同的格式。
 */
void MultiSinkExample() {
    std::cout << "\n=== Example 5: Multi-Sink Output / 多 Sink 输出 ===" << std::endl;
    
    // Console + Stderr (different formats)
    // 控制台 + 标准错误（不同格式）
    std::cout << "--- Console (SimpleFormat) + Stderr (FullFormat) ---" << std::endl;
    {
        using MultiSinkConfig = oneplog::LoggerConfig<
            oneplog::Mode::Sync,
            oneplog::Level::Debug,
            false, false, true,
            8192, 4096, oneplog::QueueFullPolicy::Block,
            oneplog::DefaultSharedMemoryName, 10,
            oneplog::SinkBindingList<
                oneplog::SinkBinding<oneplog::ConsoleSinkType, oneplog::SimpleFormat>,
                oneplog::SinkBinding<oneplog::StderrSinkType, oneplog::FullFormat>
            >
        >;
        
        oneplog::LoggerImpl<MultiSinkConfig> logger;
        logger.Info("This goes to both stdout and stderr / 同时输出到 stdout 和 stderr");
        logger.Flush();
    }
    
    // Console + File
    // 控制台 + 文件
    std::cout << "\n--- Console + File ---" << std::endl;
    {
        using ConsoleSinkBinding = oneplog::SinkBinding<oneplog::ConsoleSinkType, oneplog::SimpleFormat>;
        using FileSinkBinding = oneplog::SinkBinding<oneplog::FileSinkType, oneplog::FullFormat>;
        
        using MultiSinkConfig = oneplog::LoggerConfig<
            oneplog::Mode::Sync,
            oneplog::Level::Debug,
            false, false, true,
            8192, 4096, oneplog::QueueFullPolicy::Block,
            oneplog::DefaultSharedMemoryName, 10,
            oneplog::SinkBindingList<ConsoleSinkBinding, FileSinkBinding>
        >;
        
        oneplog::SinkBindingList<ConsoleSinkBinding, FileSinkBinding> bindings(
            ConsoleSinkBinding(),
            FileSinkBinding(oneplog::FileSinkType("sync_multi.log"))
        );
        
        oneplog::LoggerImpl<MultiSinkConfig> logger(std::move(bindings));
        logger.Info("Console: SimpleFormat, File: FullFormat");
        logger.Debug("Debug message to both sinks / 调试消息输出到两个 Sink");
        logger.Flush();
    }
    std::cout << "Check sync_multi.log for file output" << std::endl;
    std::cout << "检查 sync_multi.log 查看文件输出" << std::endl;
}

// ==============================================================================
// Example 6: Various Format Types
// 示例 6: 各种格式化器
// ==============================================================================

/**
 * @brief Demonstrates all available format types
 * @brief 演示所有可用的格式化器类型
 */
void FormatTypesExample() {
    std::cout << "\n=== Example 6: Format Types / 格式化器类型 ===" << std::endl;
    
    std::cout << "Available formats / 可用格式:" << std::endl;
    std::cout << "1. MessageOnlyFormat: message only (no metadata)" << std::endl;
    std::cout << "   MessageOnlyFormat: 仅消息（无元数据）" << std::endl;
    std::cout << "2. SimpleFormat: [HH:MM:SS] [LEVEL] message" << std::endl;
    std::cout << "   SimpleFormat: [时:分:秒] [级别] 消息" << std::endl;
    std::cout << "3. FullFormat: [YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [PID:TID] message" << std::endl;
    std::cout << "   FullFormat: [年-月-日 时:分:秒.毫秒] [级别] [进程ID:线程ID] 消息" << std::endl;
    
    std::cout << "\n--- Comparison / 对比 ---" << std::endl;
    
    // MessageOnlyFormat
    {
        using Config = oneplog::LoggerConfig<
            oneplog::Mode::Sync, oneplog::Level::Debug,
            false, false, true, 8192, 4096, oneplog::QueueFullPolicy::Block,
            oneplog::DefaultSharedMemoryName, 10,
            oneplog::SinkBindingList<
                oneplog::SinkBinding<oneplog::ConsoleSinkType, oneplog::MessageOnlyFormat>
            >
        >;
        oneplog::LoggerImpl<Config> logger;
        logger.Info("MessageOnlyFormat output");
        logger.Flush();
    }
    
    // SimpleFormat
    {
        using Config = oneplog::LoggerConfig<
            oneplog::Mode::Sync, oneplog::Level::Debug,
            false, false, true, 8192, 4096, oneplog::QueueFullPolicy::Block,
            oneplog::DefaultSharedMemoryName, 10,
            oneplog::SinkBindingList<
                oneplog::SinkBinding<oneplog::ConsoleSinkType, oneplog::SimpleFormat>
            >
        >;
        oneplog::LoggerImpl<Config> logger;
        logger.Info("SimpleFormat output");
        logger.Flush();
    }
    
    // FullFormat
    {
        using Config = oneplog::LoggerConfig<
            oneplog::Mode::Sync, oneplog::Level::Debug,
            false, false, true, 8192, 4096, oneplog::QueueFullPolicy::Block,
            oneplog::DefaultSharedMemoryName, 10,
            oneplog::SinkBindingList<
                oneplog::SinkBinding<oneplog::ConsoleSinkType, oneplog::FullFormat>
            >
        >;
        oneplog::LoggerImpl<Config> logger;
        logger.Info("FullFormat output");
        logger.Flush();
    }
}

// ==============================================================================
// Example 7: RuntimeConfig Usage
// 示例 7: RuntimeConfig 运行时配置
// ==============================================================================

/**
 * @brief Demonstrates RuntimeConfig for runtime settings
 * @brief 演示 RuntimeConfig 运行时设置
 *
 * RuntimeConfig allows setting options that can be changed at runtime,
 * such as process name and poll interval.
 *
 * RuntimeConfig 允许设置可在运行时更改的选项，如进程名称和轮询间隔。
 */
void RuntimeConfigExample() {
    std::cout << "\n=== Example 7: RuntimeConfig / 运行时配置 ===" << std::endl;
    
    // Create RuntimeConfig
    // 创建 RuntimeConfig
    oneplog::RuntimeConfig config;
    config.processName = "SyncExample";
    config.colorEnabled = true;
    config.pollInterval = std::chrono::microseconds(1);
    
    // Create logger with RuntimeConfig
    // 使用 RuntimeConfig 创建日志器
    oneplog::SyncLogger logger(config);
    
    logger.Info("Logger with RuntimeConfig / 使用 RuntimeConfig 的日志器");
    logger.Info("Process name: {} / 进程名称: {}", config.processName, config.processName);
    
    // Access and modify RuntimeConfig
    // 访问和修改 RuntimeConfig
    auto& runtimeConfig = logger.GetRuntimeConfig();
    std::cout << "Current process name: " << runtimeConfig.processName << std::endl;
    std::cout << "当前进程名称: " << runtimeConfig.processName << std::endl;
    
    // Modify at runtime
    // 运行时修改
    runtimeConfig.processName = "ModifiedName";
    logger.Info("Modified process name: {} / 修改后的进程名称: {}", 
                runtimeConfig.processName, runtimeConfig.processName);
    
    logger.Flush();
}

// ==============================================================================
// Main Function / 主函数
// ==============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "onePlog Sync Mode Examples" << std::endl;
    std::cout << "onePlog 同步模式示例" << std::endl;
    std::cout << "========================================" << std::endl;
    
    BasicSyncLoggerExample();
    CustomLogLevelExample();
    ConsoleSinkExample();
    FileSinkExample();
    MultiSinkExample();
    FormatTypesExample();
    RuntimeConfigExample();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "All sync mode examples completed!" << std::endl;
    std::cout << "所有同步模式示例完成！" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
