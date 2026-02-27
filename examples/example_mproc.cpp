/**
 * @file example_mproc.cpp
 * @brief Multi-process mode API demonstration
 * @brief 多进程模式 API 演示
 *
 * This example demonstrates the MProc mode API in a single process.
 * For actual multi-process examples, see:
 * - example_mproc_fork.cpp: Using fork() to create child processes
 * - example_mproc_owner.cpp + example_mproc_worker.cpp: Standalone processes
 *
 * 此示例在单进程中演示 MProc 模式 API。
 * 实际的多进程示例请参见：
 * - example_mproc_fork.cpp：使用 fork() 创建子进程
 * - example_mproc_owner.cpp + example_mproc_worker.cpp：独立进程
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <oneplog/oneplog.hpp>

// ==============================================================================
// Example 1: Basic MProc Logger API
// 示例 1: 基本 MProc 日志器 API
// ==============================================================================

void BasicAPIExample() {
    std::cout << "\n=== Example 1: Basic MProc Logger API ===" << std::endl;
    std::cout << "=== 示例 1: 基本 MProc 日志器 API ===\n" << std::endl;
    
    // Create MProc logger with RuntimeConfig
    // 使用 RuntimeConfig 创建 MProc 日志器
    oneplog::RuntimeConfig config;
    config.processName = "MainApp";
    
    oneplog::MProcLogger logger(config);
    
    // Check ownership
    // 检查所有权
    std::cout << "Is owner: " << (logger.IsMProcOwner() ? "yes" : "no") << std::endl;
    std::cout << "是否为所有者: " << (logger.IsMProcOwner() ? "是" : "否") << std::endl;
    
    // Basic logging
    // 基本日志记录
    logger.Trace("Trace message");
    logger.Debug("Debug message");
    logger.Info("Info message");
    logger.Warn("Warning message");
    logger.Error("Error message");
    logger.Critical("Critical message");
    
    logger.Flush();
    logger.Shutdown();
}

// ==============================================================================
// Example 2: Process and Module Registration
// 示例 2: 进程和模块注册
// ==============================================================================

void RegistrationExample() {
    std::cout << "\n=== Example 2: Process and Module Registration ===" << std::endl;
    std::cout << "=== 示例 2: 进程和模块注册 ===\n" << std::endl;
    
    oneplog::MProcLogger logger;
    
    // Register process
    // 注册进程
    uint32_t procId = logger.RegisterProcess("MyProcess");
    std::cout << "Registered process ID: " << procId << std::endl;
    
    // Register modules
    // 注册模块
    uint32_t mod1 = logger.RegisterModule("Network");
    uint32_t mod2 = logger.RegisterModule("Database");
    uint32_t mod3 = logger.RegisterModule("UI");
    
    std::cout << "Registered module IDs: " << mod1 << ", " << mod2 << ", " << mod3 << std::endl;
    
    // Lookup names
    // 查找名称
    if (procId > 0) {
        const char* name = logger.GetProcessName(procId);
        if (name) std::cout << "Process name: " << name << std::endl;
    }
    
    if (mod1 > 0) {
        const char* name = logger.GetModuleName(mod1);
        if (name) std::cout << "Module 1 name: " << name << std::endl;
    }
    
    logger.Info("Message after registration");
    logger.Flush();
    logger.Shutdown();
}

// ==============================================================================
// Example 3: Custom Configuration
// 示例 3: 自定义配置
// ==============================================================================

// Custom shared memory name
// 自定义共享内存名称
struct MyAppSharedMemory {
    static constexpr const char* value = "/my_app_logs";
};

void CustomConfigExample() {
    std::cout << "\n=== Example 3: Custom Configuration ===" << std::endl;
    std::cout << "=== 示例 3: 自定义配置 ===\n" << std::endl;
    
    // Custom MProc configuration
    // 自定义 MProc 配置
    using CustomConfig = oneplog::LoggerConfig<
        oneplog::Mode::MProc,
        oneplog::Level::Debug,
        false,  // EnableWFC
        true,   // EnableShadowTail
        true,   // UseFmt
        8192,   // HeapRingBufferCapacity
        4096,   // SharedRingBufferCapacity
        oneplog::QueueFullPolicy::Block,
        MyAppSharedMemory,  // Custom shared memory name
        10,     // PollTimeoutMs
        oneplog::DefaultSinkBindings
    >;
    
    oneplog::LoggerImpl<CustomConfig> logger;
    
    std::cout << "Using shared memory: " << MyAppSharedMemory::value << std::endl;
    std::cout << "使用共享内存: " << MyAppSharedMemory::value << std::endl;
    
    logger.Info("Custom config message");
    logger.Flush();
    logger.Shutdown();
}

// ==============================================================================
// Example 4: MProc with File Sink
// 示例 4: 带文件 Sink 的 MProc
// ==============================================================================

void FileSinkExample() {
    std::cout << "\n=== Example 4: MProc with File Sink ===" << std::endl;
    std::cout << "=== 示例 4: 带文件 Sink 的 MProc ===\n" << std::endl;
    
    using FileSinkBinding = oneplog::SinkBinding<oneplog::FileSinkType, oneplog::SimpleFormat>;
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
    
    logger.Info("File sink message 1");
    logger.Info("File sink message 2");
    logger.Warn("File sink warning");
    
    logger.Flush();
    logger.Shutdown();
    
    std::cout << "Check mproc_example.log for output" << std::endl;
    std::cout << "检查 mproc_example.log 查看输出" << std::endl;
}

// ==============================================================================
// Example 5: Multi-threaded Logging (simulating multi-process)
// 示例 5: 多线程日志记录（模拟多进程）
// ==============================================================================

void MultiThreadExample() {
    std::cout << "\n=== Example 5: Multi-threaded Logging ===" << std::endl;
    std::cout << "=== 示例 5: 多线程日志记录 ===\n" << std::endl;
    
    std::cout << "Note: This simulates multi-process with threads." << std::endl;
    std::cout << "注意：这用线程模拟多进程。" << std::endl;
    std::cout << "For real multi-process, see example_mproc_fork or example_mproc_owner/worker.\n" << std::endl;
    
    oneplog::MProcLogger logger;
    
    std::vector<std::thread> threads;
    
    for (int t = 0; t < 3; ++t) {
        threads.emplace_back([&logger, t]() {
            for (int i = 0; i < 5; ++i) {
                logger.Info("[Thread {}] Message {}", t, i);
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        });
    }
    
    for (auto& th : threads) {
        th.join();
    }
    
    logger.Flush();
    logger.Shutdown();
    
    std::cout << "Multi-threaded logging complete" << std::endl;
}

// ==============================================================================
// Main
// ==============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "onePlog MProc Mode API Examples" << std::endl;
    std::cout << "onePlog 多进程模式 API 示例" << std::endl;
    std::cout << "========================================" << std::endl;
    
    std::cout << "\nFor actual multi-process examples:" << std::endl;
    std::cout << "  - example_mproc_fork: fork() based" << std::endl;
    std::cout << "  - example_mproc_owner + example_mproc_worker: standalone" << std::endl;
    std::cout << "\n实际多进程示例:" << std::endl;
    std::cout << "  - example_mproc_fork: 基于 fork()" << std::endl;
    std::cout << "  - example_mproc_owner + example_mproc_worker: 独立进程" << std::endl;
    
    BasicAPIExample();
    RegistrationExample();
    CustomConfigExample();
    FileSinkExample();
    MultiThreadExample();
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "All API examples completed!" << std::endl;
    std::cout << "所有 API 示例完成！" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
