/**
 * @file example_mproc.cpp
 * @brief Multi-process mode example for onePlog
 * @brief onePlog 多进程模式示例
 *
 * This example demonstrates the multi-process logging mode of onePlog,
 * including various configurations and usage patterns.
 *
 * 此示例演示 onePlog 的多进程日志模式，包括各种配置和使用模式。
 *
 * Features demonstrated / 演示的功能:
 * - Multi-process logger basic usage / 多进程日志器基本用法
 * - Custom shared memory name / 共享内存名称自定义配置
 * - SharedRingBuffer capacity configuration / SharedRingBuffer 容量配置
 * - Process name registration (RegisterProcess) / 进程名称注册
 * - Module name registration (RegisterModule) / 模块名称注册
 * - Producer and consumer process roles / 生产者和消费者进程角色
 * - PipelineThread mechanism / PipelineThread 工作机制
 * - Cross-process log aggregation / 跨进程日志聚合
 *
 * @note Multi-process mode requires shared memory support from the OS.
 * @note 多进程模式需要操作系统的共享内存支持。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <oneplog/oneplog.hpp>

// ==============================================================================
// Example 1: Multi-process Logger Basic Usage
// 示例 1: 多进程日志器基本用法
// ==============================================================================

/**
 * @brief Demonstrates basic usage of the multi-process logger
 * @brief 演示多进程日志器的基本用法
 *
 * The MProcLogger uses shared memory to enable logging across multiple
 * processes. The first process to create the logger becomes the owner
 * (producer), and subsequent processes become consumers.
 *
 * MProcLogger 使用共享内存实现跨进程日志记录。第一个创建日志器的进程
 * 成为所有者（生产者），后续进程成为消费者。
 */
void BasicMProcLoggerExample() {
    std::cout << "\n=== Example 1: Basic MProc Logger / 基本多进程日志器 ===" << std::endl;
    
    // Create a multi-process logger with default settings
    // 使用默认设置创建多进程日志器
    oneplog::MProcLogger logger;
    
    // Check if this process is the owner (producer)
    // 检查此进程是否是所有者（生产者）
    if (logger.IsMProcOwner()) {
        std::cout << "This process is the OWNER (producer)" << std::endl;
        std::cout << "此进程是所有者（生产者）" << std::endl;
    } else {
        std::cout << "This process is a CONSUMER" << std::endl;
        std::cout << "此进程是消费者" << std::endl;
    }
    
    // Log messages (works from both producer and consumer)
    // 记录日志消息（生产者和消费者都可以）
    logger.Info("MProc logger message / 多进程日志器消息");
    logger.Debug("Debug from MProc / 来自多进程的调试消息");
    logger.Warn("Warning from MProc / 来自多进程的警告消息");
    
    // Flush and shutdown
    // 刷新并关闭
    logger.Flush();
    logger.Shutdown();
    
    std::cout << "MProc logger shutdown complete / 多进程日志器关闭完成" << std::endl;
}

// ==============================================================================
// Example 2: Custom Shared Memory Name
// 示例 2: 共享内存名称自定义配置
// ==============================================================================

/**
 * @brief Demonstrates custom shared memory name configuration
 * @brief 演示共享内存名称自定义配置
 *
 * Custom shared memory names allow multiple independent logging systems
 * to coexist on the same machine.
 *
 * 自定义共享内存名称允许多个独立的日志系统在同一台机器上共存。
 */

// Define custom shared memory name type
// 定义自定义共享内存名称类型
struct CustomSharedMemoryName {
    static constexpr const char* value = "my_app_logs";
};

void CustomSharedMemoryNameExample() {
    std::cout << "\n=== Example 2: Custom Shared Memory Name / 自定义共享内存名称 ===" << std::endl;
    
    // Logger with custom shared memory name
    // 使用自定义共享内存名称的日志器
    using CustomMProcConfig = oneplog::LoggerConfig<
        oneplog::Mode::MProc,
        oneplog::Level::Debug,
        false,  // EnableWFC
        true,   // EnableShadowTail
        true,   // UseFmt
        8192,   // HeapRingBufferCapacity
        4096,   // SharedRingBufferCapacity
        oneplog::QueueFullPolicy::Block,
        CustomSharedMemoryName,  // Custom shared memory name
        10,     // PollTimeoutMs
        oneplog::DefaultSinkBindings
    >;
    
    oneplog::LoggerImpl<CustomMProcConfig> logger;
    
    std::cout << "Using custom shared memory name: " << CustomSharedMemoryName::value << std::endl;
    std::cout << "使用自定义共享内存名称: " << CustomSharedMemoryName::value << std::endl;
    
    logger.Info("Message with custom shared memory name");
    logger.Info("使用自定义共享内存名称的消息");
    
    logger.Flush();
    logger.Shutdown();
}

// ==============================================================================
// Example 3: SharedRingBuffer Capacity Configuration
// 示例 3: SharedRingBuffer 容量配置
// ==============================================================================

/**
 * @brief Demonstrates SharedRingBuffer capacity configuration
 * @brief 演示 SharedRingBuffer 容量配置
 *
 * The SharedRingBuffer capacity determines how many log entries can be
 * stored in shared memory for cross-process communication.
 *
 * SharedRingBuffer 容量决定了可以在共享内存中存储多少日志条目用于跨进程通信。
 */
void SharedRingBufferCapacityExample() {
    std::cout << "\n=== Example 3: SharedRingBuffer Capacity / SharedRingBuffer 容量 ===" << std::endl;
    
    // Small shared buffer (2048 slots)
    // 小共享缓冲区（2048 槽位）
    std::cout << "--- Small SharedRingBuffer (2048 slots) ---" << std::endl;
    {
        using SmallSharedConfig = oneplog::LoggerConfig<
            oneplog::Mode::MProc,
            oneplog::Level::Debug,
            false, true, true,
            8192,   // HeapRingBufferCapacity
            2048,   // SharedRingBufferCapacity (small)
            oneplog::QueueFullPolicy::Block,
            oneplog::DefaultSharedMemoryName, 10,
            oneplog::DefaultSinkBindings
        >;
        
        oneplog::LoggerImpl<SmallSharedConfig> logger;
        logger.Info("Small SharedRingBuffer (2048 slots)");
        logger.Info("小 SharedRingBuffer（2048 槽位）");
        logger.Flush();
        logger.Shutdown();
    }
    
    // Large shared buffer (16384 slots)
    // 大共享缓冲区（16384 槽位）
    std::cout << "--- Large SharedRingBuffer (16384 slots) ---" << std::endl;
    {
        using LargeSharedConfig = oneplog::LoggerConfig<
            oneplog::Mode::MProc,
            oneplog::Level::Debug,
            false, true, true,
            8192,   // HeapRingBufferCapacity
            16384,  // SharedRingBufferCapacity (large)
            oneplog::QueueFullPolicy::Block,
            oneplog::DefaultSharedMemoryName, 10,
            oneplog::DefaultSinkBindings
        >;
        
        oneplog::LoggerImpl<LargeSharedConfig> logger;
        logger.Info("Large SharedRingBuffer (16384 slots)");
        logger.Info("大 SharedRingBuffer（16384 槽位）");
        logger.Flush();
        logger.Shutdown();
    }
    
    std::cout << "SharedRingBuffer capacity affects cross-process throughput" << std::endl;
    std::cout << "SharedRingBuffer 容量影响跨进程吞吐量" << std::endl;
}


// ==============================================================================
// Example 4: Process Name Registration
// 示例 4: 进程名称注册
// ==============================================================================

/**
 * @brief Demonstrates process name registration
 * @brief 演示进程名称注册
 *
 * Process names help identify which process generated each log message
 * in a multi-process environment.
 *
 * 进程名称有助于在多进程环境中识别每条日志消息来自哪个进程。
 */
void ProcessNameRegistrationExample() {
    std::cout << "\n=== Example 4: Process Name Registration / 进程名称注册 ===" << std::endl;
    
    // Create logger with RuntimeConfig containing process name
    // 使用包含进程名称的 RuntimeConfig 创建日志器
    oneplog::RuntimeConfig config;
    config.processName = "MainProcess";
    
    oneplog::MProcLogger logger(config);
    
    // Register process name (alternative method)
    // 注册进程名称（另一种方法）
    uint32_t processId = logger.RegisterProcess("MainProcess");
    if (processId > 0) {
        std::cout << "Registered process 'MainProcess' with ID: " << processId << std::endl;
        std::cout << "注册进程 'MainProcess'，ID: " << processId << std::endl;
    }
    
    // Get process name by ID
    // 通过 ID 获取进程名称
    const char* name = logger.GetProcessName(processId);
    if (name) {
        std::cout << "Process name for ID " << processId << ": " << name << std::endl;
        std::cout << "ID " << processId << " 的进程名称: " << name << std::endl;
    }
    
    logger.Info("Message from registered process / 来自已注册进程的消息");
    
    logger.Flush();
    logger.Shutdown();
}

// ==============================================================================
// Example 5: Module Name Registration
// 示例 5: 模块名称注册
// ==============================================================================

/**
 * @brief Demonstrates module name registration
 * @brief 演示模块名称注册
 *
 * Module names help identify which component or module generated each
 * log message within a process.
 *
 * 模块名称有助于识别进程内哪个组件或模块生成了每条日志消息。
 */
void ModuleNameRegistrationExample() {
    std::cout << "\n=== Example 5: Module Name Registration / 模块名称注册 ===" << std::endl;
    
    oneplog::MProcLogger logger;
    
    // Register module names
    // 注册模块名称
    uint32_t networkModuleId = logger.RegisterModule("NetworkModule");
    uint32_t databaseModuleId = logger.RegisterModule("DatabaseModule");
    uint32_t uiModuleId = logger.RegisterModule("UIModule");
    
    std::cout << "Registered modules / 已注册模块:" << std::endl;
    std::cout << "  NetworkModule ID: " << networkModuleId << std::endl;
    std::cout << "  DatabaseModule ID: " << databaseModuleId << std::endl;
    std::cout << "  UIModule ID: " << uiModuleId << std::endl;
    
    // Get module names by ID
    // 通过 ID 获取模块名称
    if (networkModuleId > 0) {
        const char* name = logger.GetModuleName(networkModuleId);
        if (name) {
            std::cout << "Module name for ID " << networkModuleId << ": " << name << std::endl;
        }
    }
    
    // Log messages from different "modules"
    // 从不同"模块"记录日志消息
    logger.Info("[Network] Connection established / 连接已建立");
    logger.Info("[Database] Query executed / 查询已执行");
    logger.Info("[UI] Window rendered / 窗口已渲染");
    
    logger.Flush();
    logger.Shutdown();
}

// ==============================================================================
// Example 6: Producer and Consumer Process Roles
// 示例 6: 生产者和消费者进程角色
// ==============================================================================

/**
 * @brief Demonstrates producer and consumer process roles
 * @brief 演示生产者和消费者进程角色
 *
 * In MProc mode:
 * - The first process to create the logger becomes the OWNER (producer)
 * - Subsequent processes become CONSUMERS
 * - The owner creates the shared memory and PipelineThread
 * - Consumers attach to existing shared memory
 *
 * 在 MProc 模式下：
 * - 第一个创建日志器的进程成为所有者（生产者）
 * - 后续进程成为消费者
 * - 所有者创建共享内存和 PipelineThread
 * - 消费者附加到现有共享内存
 */
void ProducerConsumerRolesExample() {
    std::cout << "\n=== Example 6: Producer/Consumer Roles / 生产者/消费者角色 ===" << std::endl;
    
    oneplog::MProcLogger logger;
    
    std::cout << "Process role detection / 进程角色检测:" << std::endl;
    
    if (logger.IsMProcOwner()) {
        std::cout << "  Role: OWNER (Producer)" << std::endl;
        std::cout << "  角色: 所有者（生产者）" << std::endl;
        std::cout << "  - Created shared memory" << std::endl;
        std::cout << "  - 创建了共享内存" << std::endl;
        std::cout << "  - Started PipelineThread" << std::endl;
        std::cout << "  - 启动了 PipelineThread" << std::endl;
        std::cout << "  - Responsible for writing to sinks" << std::endl;
        std::cout << "  - 负责写入 Sink" << std::endl;
    } else {
        std::cout << "  Role: CONSUMER" << std::endl;
        std::cout << "  角色: 消费者" << std::endl;
        std::cout << "  - Attached to existing shared memory" << std::endl;
        std::cout << "  - 附加到现有共享内存" << std::endl;
        std::cout << "  - Sends logs to shared buffer" << std::endl;
        std::cout << "  - 将日志发送到共享缓冲区" << std::endl;
    }
    
    logger.Info("Message from {} process", logger.IsMProcOwner() ? "owner" : "consumer");
    logger.Info("来自{}进程的消息", logger.IsMProcOwner() ? "所有者" : "消费者");
    
    logger.Flush();
    logger.Shutdown();
}

// ==============================================================================
// Example 7: PipelineThread Mechanism
// 示例 7: PipelineThread 工作机制
// ==============================================================================

/**
 * @brief Demonstrates PipelineThread mechanism
 * @brief 演示 PipelineThread 工作机制
 *
 * PipelineThread transfers log entries from the local HeapRingBuffer
 * to the SharedRingBuffer for cross-process communication.
 *
 * PipelineThread 将日志条目从本地 HeapRingBuffer 传输到 SharedRingBuffer
 * 以实现跨进程通信。
 *
 * Data flow / 数据流:
 * Producer Thread -> HeapRingBuffer -> PipelineThread -> SharedRingBuffer -> Consumer
 */
void PipelineThreadExample() {
    std::cout << "\n=== Example 7: PipelineThread Mechanism / PipelineThread 机制 ===" << std::endl;
    
    std::cout << "MProc mode data flow / MProc 模式数据流:" << std::endl;
    std::cout << "  1. Producer thread logs message" << std::endl;
    std::cout << "     生产者线程记录消息" << std::endl;
    std::cout << "  2. Message goes to HeapRingBuffer (local)" << std::endl;
    std::cout << "     消息进入 HeapRingBuffer（本地）" << std::endl;
    std::cout << "  3. PipelineThread transfers to SharedRingBuffer" << std::endl;
    std::cout << "     PipelineThread 传输到 SharedRingBuffer" << std::endl;
    std::cout << "  4. Consumer reads from SharedRingBuffer" << std::endl;
    std::cout << "     消费者从 SharedRingBuffer 读取" << std::endl;
    std::cout << "  5. Consumer writes to Sink" << std::endl;
    std::cout << "     消费者写入 Sink" << std::endl;
    
    oneplog::MProcLogger logger;
    
    // Log multiple messages to demonstrate pipeline
    // 记录多条消息以演示管道
    for (int i = 0; i < 10; ++i) {
        logger.Info("Pipeline message {} / 管道消息 {}", i, i);
    }
    
    // Small delay to allow pipeline processing
    // 小延迟以允许管道处理
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    logger.Flush();
    logger.Shutdown();
    
    std::cout << "Pipeline processing complete / 管道处理完成" << std::endl;
}


// ==============================================================================
// Example 8: Cross-process Log Aggregation
// 示例 8: 跨进程日志聚合
// ==============================================================================

/**
 * @brief Demonstrates cross-process log aggregation
 * @brief 演示跨进程日志聚合
 *
 * This example shows how multiple processes can send logs to a single
 * aggregated output through shared memory.
 *
 * 此示例展示多个进程如何通过共享内存将日志发送到单一聚合输出。
 *
 * @note In a real multi-process scenario, you would run this program
 *       multiple times in separate processes.
 * @note 在真实的多进程场景中，您需要在不同进程中多次运行此程序。
 */
void CrossProcessAggregationExample() {
    std::cout << "\n=== Example 8: Cross-process Aggregation / 跨进程聚合 ===" << std::endl;
    
    std::cout << "Cross-process logging architecture / 跨进程日志架构:" << std::endl;
    std::cout << "  +-------------+     +------------------+     +--------+" << std::endl;
    std::cout << "  | Process A   | --> |                  |     |        |" << std::endl;
    std::cout << "  +-------------+     |  SharedMemory    | --> |  Sink  |" << std::endl;
    std::cout << "  +-------------+     |  (RingBuffer)    |     |        |" << std::endl;
    std::cout << "  | Process B   | --> |                  |     +--------+" << std::endl;
    std::cout << "  +-------------+     +------------------+" << std::endl;
    
    // Simulate multiple "processes" using threads (for demonstration)
    // 使用线程模拟多个"进程"（用于演示）
    std::cout << "\nSimulating multi-process logging with threads..." << std::endl;
    std::cout << "使用线程模拟多进程日志记录..." << std::endl;
    
    // In a real scenario, each process would create its own logger
    // 在真实场景中，每个进程会创建自己的日志器
    oneplog::MProcLogger logger;
    
    // Simulate messages from different "processes"
    // 模拟来自不同"进程"的消息
    std::vector<std::thread> threads;
    
    for (int p = 0; p < 3; ++p) {
        threads.emplace_back([&logger, p]() {
            for (int i = 0; i < 5; ++i) {
                logger.Info("[Process {}] Message {} / [进程 {}] 消息 {}", p, i, p, i);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }
    
    // Wait for all "processes" to complete
    // 等待所有"进程"完成
    for (auto& t : threads) {
        t.join();
    }
    
    logger.Flush();
    logger.Shutdown();
    
    std::cout << "\nAll messages aggregated to single output" << std::endl;
    std::cout << "所有消息聚合到单一输出" << std::endl;
}

// ==============================================================================
// Example 9: MProc with File Sink
// 示例 9: 带文件 Sink 的多进程模式
// ==============================================================================

/**
 * @brief Demonstrates MProc mode with file sink
 * @brief 演示带文件 Sink 的多进程模式
 */
void MProcFileSinkExample() {
    std::cout << "\n=== Example 9: MProc with File Sink / 带文件 Sink 的多进程 ===" << std::endl;
    
    using FileSinkBinding = oneplog::SinkBinding<oneplog::FileSinkType, oneplog::FullFormat>;
    using MProcFileConfig = oneplog::LoggerConfig<
        oneplog::Mode::MProc,
        oneplog::Level::Debug,
        false, true, true,
        8192, 4096, oneplog::QueueFullPolicy::Block,
        oneplog::DefaultSharedMemoryName, 10,
        oneplog::SinkBindingList<FileSinkBinding>
    >;
    
    oneplog::SinkBindingList<FileSinkBinding> bindings(
        FileSinkBinding(oneplog::FileSinkType("mproc_example.log"))
    );
    oneplog::LoggerImpl<MProcFileConfig> logger(std::move(bindings));
    
    logger.Info("MProc file logging example / 多进程文件日志示例");
    logger.Debug("Debug message to file / 调试消息写入文件");
    logger.Warn("Warning message to file / 警告消息写入文件");
    
    logger.Flush();
    logger.Shutdown();
    
    std::cout << "Check mproc_example.log for output" << std::endl;
    std::cout << "检查 mproc_example.log 查看输出" << std::endl;
}

// ==============================================================================
// Example 10: Complete MProc Workflow
// 示例 10: 完整的多进程工作流程
// ==============================================================================

/**
 * @brief Demonstrates a complete MProc workflow
 * @brief 演示完整的多进程工作流程
 *
 * This example shows the recommended way to use MProc mode in a
 * production environment.
 *
 * 此示例展示在生产环境中使用 MProc 模式的推荐方式。
 */
void CompleteMProcWorkflowExample() {
    std::cout << "\n=== Example 10: Complete MProc Workflow / 完整多进程工作流程 ===" << std::endl;
    
    std::cout << "Recommended MProc workflow / 推荐的多进程工作流程:" << std::endl;
    std::cout << "  1. Start owner process first (creates shared memory)" << std::endl;
    std::cout << "     首先启动所有者进程（创建共享内存）" << std::endl;
    std::cout << "  2. Start consumer processes (attach to shared memory)" << std::endl;
    std::cout << "     启动消费者进程（附加到共享内存）" << std::endl;
    std::cout << "  3. All processes can log messages" << std::endl;
    std::cout << "     所有进程都可以记录日志消息" << std::endl;
    std::cout << "  4. Owner process writes to sinks" << std::endl;
    std::cout << "     所有者进程写入 Sink" << std::endl;
    std::cout << "  5. Shutdown consumer processes first" << std::endl;
    std::cout << "     首先关闭消费者进程" << std::endl;
    std::cout << "  6. Shutdown owner process last" << std::endl;
    std::cout << "     最后关闭所有者进程" << std::endl;
    
    // Create RuntimeConfig with process name
    // 使用进程名称创建 RuntimeConfig
    oneplog::RuntimeConfig config;
    config.processName = "MainApp";
    config.pollInterval = std::chrono::microseconds(1);
    
    // Create MProc logger
    // 创建多进程日志器
    oneplog::MProcLogger logger(config);
    
    // Register process and modules
    // 注册进程和模块
    logger.RegisterProcess(config.processName);
    logger.RegisterModule("Core");
    logger.RegisterModule("Network");
    logger.RegisterModule("Storage");
    
    // Log application lifecycle
    // 记录应用程序生命周期
    logger.Info("Application starting / 应用程序启动");
    logger.Info("Initializing modules / 初始化模块");
    logger.Debug("Core module initialized / Core 模块已初始化");
    logger.Debug("Network module initialized / Network 模块已初始化");
    logger.Debug("Storage module initialized / Storage 模块已初始化");
    logger.Info("Application ready / 应用程序就绪");
    
    // Simulate some work
    // 模拟一些工作
    for (int i = 0; i < 5; ++i) {
        logger.Info("Processing request {} / 处理请求 {}", i, i);
    }
    
    // Proper shutdown sequence
    // 正确的关闭顺序
    logger.Info("Application shutting down / 应用程序关闭中");
    logger.Flush();  // Ensure all messages are processed
    logger.Shutdown();
    
    std::cout << "\nMProc workflow complete / 多进程工作流程完成" << std::endl;
}

// ==============================================================================
// Main Function / 主函数
// ==============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "onePlog MProc Mode Examples" << std::endl;
    std::cout << "onePlog 多进程模式示例" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::cout << "\nNote: MProc mode is designed for multi-process scenarios." << std::endl;
    std::cout << "注意: MProc 模式专为多进程场景设计。" << std::endl;
    std::cout << "In this single-process demo, the logger acts as owner." << std::endl;
    std::cout << "在此单进程演示中，日志器作为所有者运行。" << std::endl;
    
    BasicMProcLoggerExample();
    CustomSharedMemoryNameExample();
    SharedRingBufferCapacityExample();
    ProcessNameRegistrationExample();
    ModuleNameRegistrationExample();
    ProducerConsumerRolesExample();
    PipelineThreadExample();
    CrossProcessAggregationExample();
    MProcFileSinkExample();
    CompleteMProcWorkflowExample();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "All MProc mode examples completed!" << std::endl;
    std::cout << "所有多进程模式示例完成！" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
