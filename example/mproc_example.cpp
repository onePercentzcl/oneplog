/**
 * @file mproc_example.cpp
 * @brief Multi-process mode example for onePlog
 * @brief onePlog 多进程模式示例
 *
 * This example demonstrates how to use onePlog across multiple processes.
 * Each process creates its own logger but outputs to the same console.
 * 此示例演示如何在多个进程中使用 onePlog。
 * 每个进程创建自己的日志器，但输出到同一个控制台。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <iostream>
#include <thread>
#include <chrono>

#ifndef ONEPLOG_SYNC_ONLY
#include <oneplog/oneplog.hpp>

#if defined(__unix__) || defined(__APPLE__)
#define ONEPLOG_HAS_FORK 1
#include <unistd.h>
#include <sys/wait.h>
#endif

#ifdef ONEPLOG_HAS_FORK
void RunChildProcess(int childId) {
    // Initialize with sync mode for child process
    // 子进程使用同步模式初始化
    oneplog::LoggerConfig config;
    config.mode = oneplog::Mode::Sync;
    config.level = oneplog::Level::Debug;
    config.processName = "child" + std::to_string(childId);  // Set process name / 设置进程名
    oneplog::Init(config);

    // Set module name for this process / 设置此进程的模块名
    oneplog::NameManager::SetModuleName("worker");

    std::cout << "[Child " << childId << "] PID=" << getpid() << " Starting" << std::endl;
    
    log::Info("[Child {}] Process={}, Module={}", 
              childId,
              oneplog::NameManager::GetProcessName(),
              oneplog::NameManager::GetModuleName());

    for (int i = 0; i < 5; ++i) {
        log::Info("[Child {}] Message {}", childId, i);
        std::this_thread::sleep_for(std::chrono::milliseconds(20 + childId * 10));
    }

    std::cout << "[Child " << childId << "] Complete" << std::endl;
    log::Flush();
    oneplog::Shutdown();
}
#endif

int main() {
    std::cout << "=== onePlog Multi-Process Mode Example ===" << std::endl;
    std::cout << "=== onePlog 多进程模式示例 ===" << std::endl;
    std::cout << std::endl;

#ifdef ONEPLOG_HAS_FORK
    // Initialize parent with sync mode and process name
    // 父进程使用同步模式和进程名初始化
    oneplog::LoggerConfig config;
    config.mode = oneplog::Mode::Sync;
    config.level = oneplog::Level::Debug;
    config.processName = "parent";  // Set process name / 设置进程名
    oneplog::Init(config);

    // Set module name for parent process / 设置父进程的模块名
    oneplog::NameManager::SetModuleName("main");

    std::cout << "[Parent] PID=" << getpid() << std::endl;
    std::cout << "[Parent] Process=" << oneplog::NameManager::GetProcessName() 
              << ", Module=" << oneplog::NameManager::GetModuleName() << std::endl;
    std::cout << "--- Forking child processes / 创建子进程 ---" << std::endl;

    const int numChildren = 3;
    std::vector<pid_t> childPids;

    for (int i = 0; i < numChildren; ++i) {
        pid_t pid = fork();

        if (pid < 0) {
            std::cerr << "Fork failed!" << std::endl;
            return 1;
        } else if (pid == 0) {
            // Child process - reinitialize logger and run
            // 子进程 - 重新初始化日志器并运行
            oneplog::Shutdown();  // Clean up parent's logger
            RunChildProcess(i);
            _exit(0);  // Use _exit to avoid flushing parent's buffers
        } else {
            // Parent process - record child PID
            // 父进程 - 记录子进程 PID
            childPids.push_back(pid);
        }
    }

    // Parent process logging (interleaved with children)
    // 父进程日志记录（与子进程交错）
    std::cout << "[Parent] Starting parent process logging" << std::endl;
    for (int i = 0; i < 5; ++i) {
        log::Info("[Parent] Message {}", i);
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    std::cout << "[Parent] Parent process logging complete" << std::endl;

    // Wait for all children to complete
    // 等待所有子进程完成
    for (pid_t pid : childPids) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            std::cout << "[Parent] Child PID=" << pid << " exited with status " 
                      << WEXITSTATUS(status) << std::endl;
        }
    }

    log::Flush();
    oneplog::Shutdown();

#else
    std::cout << "Multi-process mode is only supported on Unix-like systems." << std::endl;
    std::cout << "多进程模式仅在类 Unix 系统上支持。" << std::endl;
    std::cout << std::endl;

    // Demonstrate single-process usage on non-Unix systems
    // 在非 Unix 系统上演示单进程用法
    std::cout << "--- Single Process Demo / 单进程演示 ---" << std::endl;

    oneplog::LoggerConfig config;
    config.processName = "demo_app";
    oneplog::Init(config);
    
    oneplog::NameManager::SetModuleName("main");
    
    for (int i = 0; i < 5; ++i) {
        log::Info("Demo message {}", i);
    }

    oneplog::Shutdown();
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
