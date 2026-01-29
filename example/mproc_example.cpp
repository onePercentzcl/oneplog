/**
 * @file mproc_example.cpp
 * @brief Multi-process mode example for onePlog
 * @brief onePlog 多进程模式示例
 *
 * This example demonstrates how to use onePlog in multi-process mode.
 * In multi-process mode, multiple processes share the same log output
 * through shared memory.
 * 此示例演示如何在多进程模式下使用 onePlog。
 * 在多进程模式下，多个进程通过共享内存共享同一个日志输出。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <iostream>
#include <thread>
#include <chrono>

#ifndef ONEPLOG_SYNC_ONLY
#include <oneplog/oneplog.hpp>

#ifdef __unix__
#include <unistd.h>
#include <sys/wait.h>
#endif

void RunParentProcess(std::shared_ptr<oneplog::Logger>& logger) {
    std::cout << "[Parent] Starting parent process logging" << std::endl;
    std::cout << "[父进程] 开始父进程日志记录" << std::endl;

    for (int i = 0; i < 5; ++i) {
        logger->Info("[Parent] Message {}", i);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::cout << "[Parent] Parent process logging complete" << std::endl;
    std::cout << "[父进程] 父进程日志记录完成" << std::endl;
}

#ifdef __unix__
void RunChildProcess(const std::string& shmName) {
    std::cout << "[Child] Starting child process" << std::endl;
    std::cout << "[子进程] 启动子进程" << std::endl;

    // Initialize as producer (connect to existing shared memory)
    // 作为生产者初始化（连接到现有共享内存）
    oneplog::InitProducer(shmName, 0);

    // Set process name for identification
    // 设置进程名称以便识别
    oneplog::SetProcessName("child_process");

    for (int i = 0; i < 5; ++i) {
        oneplog::Info("[Child] Message {}", i);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    std::cout << "[Child] Child process logging complete" << std::endl;
    std::cout << "[子进程] 子进程日志记录完成" << std::endl;
}
#endif

int main() {
    std::cout << "=== onePlog Multi-Process Mode Example ===" << std::endl;
    std::cout << "=== onePlog 多进程模式示例 ===" << std::endl;
    std::cout << std::endl;

#ifdef __unix__
    // Create a logger in multi-process mode
    // 创建多进程模式的日志器
    auto logger = std::make_shared<oneplog::Logger>("mproc_logger", oneplog::Mode::MProc);

    // Create a console sink
    // 创建控制台输出
    auto consoleSink = std::make_shared<oneplog::ConsoleSink>();
    logger->SetSink(consoleSink);

    // Set log level
    // 设置日志级别
    logger->SetLevel(oneplog::Level::Debug);

    // Configure with custom settings
    // 使用自定义设置配置
    oneplog::LoggerConfig config;
    config.mode = oneplog::Mode::MProc;
    config.sharedMemoryName = "/oneplog_mproc_example";
    config.sharedRingBufferSize = 1024;

    // Initialize the logger (creates shared memory)
    // 初始化日志器（创建共享内存）
    logger->Init(config);

    // Set as default logger
    // 设置为默认日志器
    oneplog::SetDefaultLogger(logger);

    // Set process name
    // 设置进程名称
    oneplog::SetProcessName("parent_process");

    std::cout << "--- Forking child process / 创建子进程 ---" << std::endl;

    pid_t pid = fork();

    if (pid < 0) {
        std::cerr << "Fork failed!" << std::endl;
        return 1;
    } else if (pid == 0) {
        // Child process
        // 子进程
        RunChildProcess(config.sharedMemoryName);
        return 0;
    } else {
        // Parent process
        // 父进程
        RunParentProcess(logger);

        // Wait for child to complete
        // 等待子进程完成
        int status;
        waitpid(pid, &status, 0);

        std::cout << "[Parent] Child process exited with status " << status << std::endl;
        std::cout << "[父进程] 子进程退出，状态码 " << status << std::endl;
    }

    // Flush and shutdown
    // 刷新并关闭
    logger->Flush();
    logger->Shutdown();

#else
    std::cout << "Multi-process mode is only supported on Unix-like systems." << std::endl;
    std::cout << "多进程模式仅在类 Unix 系统上支持。" << std::endl;
    std::cout << std::endl;

    // Demonstrate single-process usage on non-Unix systems
    // 在非 Unix 系统上演示单进程用法
    std::cout << "--- Single Process Demo / 单进程演示 ---" << std::endl;

    auto logger = std::make_shared<oneplog::Logger>("demo_logger", oneplog::Mode::Async);
    auto consoleSink = std::make_shared<oneplog::ConsoleSink>();
    logger->SetSink(consoleSink);
    logger->SetLevel(oneplog::Level::Debug);
    logger->Init();
    oneplog::SetDefaultLogger(logger);

    for (int i = 0; i < 5; ++i) {
        logger->Info("Demo message {}", i);
    }

    logger->Flush();
    logger->Shutdown();
#endif

    std::cout << std::endl;
    std::cout << "=== Example Complete / 示例完成 ===" << std::endl;

    return 0;
}

#else  // ONEPLOG_SYNC_ONLY

int main() {
    std::cout << "Multi-process mode is disabled when ONEPLOG_SYNC_ONLY is defined." << std::endl;
    std::cout << "当定义 ONEPLOG_SYNC_ONLY 时，多进程模式被禁用。" << std::endl;
    return 0;
}

#endif  // ONEPLOG_SYNC_ONLY
