/**
 * @file example_mproc_fork.cpp
 * @brief Multi-process mode example using fork()
 * @brief 使用 fork() 的多进程模式示例
 *
 * This example demonstrates MProc logging with fork().
 * The parent creates the shared memory (owner), then forks children
 * that connect to the same shared memory.
 *
 * 此示例演示使用 fork() 的 MProc 日志记录。
 * 父进程创建共享内存（所有者），然后 fork 子进程连接到同一共享内存。
 *
 * @note This example only works on POSIX systems (Linux, macOS).
 * @copyright Copyright (c) 2024 onePlog
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <oneplog/oneplog.hpp>

#ifndef _WIN32
#include <unistd.h>
#include <sys/wait.h>
#endif

#ifndef _WIN32

// Custom shared memory name for this example
// 此示例的自定义共享内存名称
struct ForkExampleSharedMemory {
    static constexpr const char* value = "/fork_example_shm";
};

// MProc config with custom shared memory name
// 使用自定义共享内存名称的 MProc 配置
using ForkMProcConfig = oneplog::LoggerConfig<
    oneplog::Mode::MProc,
    oneplog::Level::Debug,
    false, true, true,
    8192, 4096, oneplog::QueueFullPolicy::Block,
    ForkExampleSharedMemory, 10,
    oneplog::DefaultSinkBindings
>;

using ForkMProcLogger = oneplog::LoggerImpl<ForkMProcConfig>;

void ChildProcess(int childId) {
    // Small delay to ensure parent's shared memory is ready
    // 小延迟确保父进程的共享内存已准备好
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Child creates its own logger - will connect to existing shared memory
    // 子进程创建自己的日志器 - 将连接到现有共享内存
    oneplog::RuntimeConfig config;
    config.processName = "Child" + std::to_string(childId);
    
    ForkMProcLogger logger(config);
    
    if (logger.IsMProcOwner()) {
        std::cerr << "[Child " << childId << "] WARNING: Became owner unexpectedly\n";
    } else {
        std::cout << "[Child " << childId << "] Connected to shared memory\n";
    }
    
    // Log messages
    for (int i = 0; i < 5; ++i) {
        logger.Info("[Child {}] Message {} from PID {}", childId, i, getpid());
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    
    logger.Info("[Child {}] Done", childId);
    logger.Flush();
    
    std::cout << "[Child " << childId << "] Finished\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Fork-based MProc Example\n";
    std::cout << "基于 fork 的 MProc 示例\n";
    std::cout << "========================================\n\n";
    
    constexpr int kNumChildren = 3;
    
    // Parent creates the logger first (becomes owner)
    // 父进程首先创建日志器（成为所有者）
    oneplog::RuntimeConfig config;
    config.processName = "Parent";
    
    ForkMProcLogger logger(config);
    
    if (!logger.IsMProcOwner()) {
        std::cerr << "[Parent] ERROR: Should be owner but isn't!\n";
        std::cerr << "[Parent] Another process may be using the same shared memory.\n";
        return 1;
    }
    
    std::cout << "[Parent] PID=" << getpid() << ", created shared memory as owner\n";
    logger.Info("=== Fork MProc example started ===");
    
    // Fork children
    std::vector<pid_t> children;
    for (int i = 0; i < kNumChildren; ++i) {
        pid_t pid = fork();
        
        if (pid < 0) {
            std::cerr << "[Parent] fork() failed\n";
            break;
        } else if (pid == 0) {
            // Child process - run child logic and exit
            ChildProcess(i);
            _exit(0);
        } else {
            // Parent - record child PID
            children.push_back(pid);
            std::cout << "[Parent] Forked child " << i << " with PID " << pid << "\n";
        }
    }
    
    // Parent continues logging
    for (int i = 0; i < 3; ++i) {
        logger.Info("[Parent] Message {} from PID {}", i, getpid());
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // Wait for all children
    std::cout << "[Parent] Waiting for children...\n";
    for (pid_t pid : children) {
        int status;
        waitpid(pid, &status, 0);
        std::cout << "[Parent] Child " << pid << " exited\n";
    }
    
    // Give time for all messages to be processed
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    logger.Info("=== Fork MProc example completed ===");
    logger.Flush();
    logger.Shutdown();
    
    std::cout << "\n[Parent] All done!\n";
    return 0;
}

#else
int main() {
    std::cout << "fork() not available on Windows.\n";
    std::cout << "Use example_mproc_owner + example_mproc_worker instead.\n";
    return 1;
}
#endif
