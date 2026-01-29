/**
 * @file exec_example.cpp
 * @brief Example of using onePlog with exec'd child processes
 * @brief 使用 exec 启动子进程时使用 onePlog 的示例
 *
 * This example demonstrates how to use onePlog when launching child processes
 * via exec (not fork). Each process creates its own logger independently.
 * 此示例演示如何在通过 exec（非 fork）启动子进程时使用 onePlog。
 * 每个进程独立创建自己的日志器。
 *
 * Usage / 用法:
 *   ./exec_example           # Run as parent / 作为父进程运行
 *   ./exec_example child N   # Run as child N / 作为子进程 N 运行
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <cstdlib>

#ifndef ONEPLOG_SYNC_ONLY
#include <oneplog/oneplog.hpp>

#if defined(__unix__) || defined(__APPLE__)
#define ONEPLOG_HAS_POSIX 1
#include <unistd.h>
#include <sys/wait.h>
#include <spawn.h>

extern char** environ;
#endif

/**
 * @brief Run as child process
 * @brief 作为子进程运行
 */
void RunAsChild(int childId) {
    // Create a new logger for this child process
    // 为子进程创建新的日志器
    auto logger = std::make_shared<oneplog::Logger>("child_logger", oneplog::Mode::Sync);
    auto consoleSink = std::make_shared<oneplog::ConsoleSink>();
    logger->SetSink(consoleSink);
    logger->SetLevel(oneplog::Level::Debug);
    logger->Init();

    // Set process name for identification
    // 设置进程名称以便识别
    std::string procName = "child" + std::to_string(childId);
    auto format = std::make_shared<oneplog::ConsoleFormat>();
    format->SetProcessName(procName);
    logger->SetFormat(format);

    oneplog::SetDefaultLogger(logger);

#ifdef ONEPLOG_HAS_POSIX
    logger->Info("[Child {}] Started, PID={}", childId, getpid());
#else
    logger->Info("[Child {}] Started", childId);
#endif

    // Simulate some work
    // 模拟一些工作
    for (int i = 0; i < 5; ++i) {
        logger->Info("[Child {}] Working... step {}", childId, i);
        std::this_thread::sleep_for(std::chrono::milliseconds(50 + childId * 20));
    }

    logger->Info("[Child {}] Completed", childId);
    logger->Flush();
}

/**
 * @brief Run as parent process
 * @brief 作为父进程运行
 */
void RunAsParent(const char* programPath) {
    // Create parent logger
    // 创建父进程日志器
    auto logger = std::make_shared<oneplog::Logger>("parent_logger", oneplog::Mode::Sync);
    auto consoleSink = std::make_shared<oneplog::ConsoleSink>();
    logger->SetSink(consoleSink);
    logger->SetLevel(oneplog::Level::Debug);
    logger->Init();

    auto format = std::make_shared<oneplog::ConsoleFormat>();
    format->SetProcessName("parent");
    logger->SetFormat(format);

    oneplog::SetDefaultLogger(logger);

#ifdef ONEPLOG_HAS_POSIX
    logger->Info("[Parent] Starting, PID={}", getpid());
#else
    logger->Info("[Parent] Starting");
#endif

#ifdef ONEPLOG_HAS_POSIX
    const int numChildren = 3;
    std::vector<pid_t> childPids;

    logger->Info("[Parent] Spawning {} child processes...", numChildren);

    for (int i = 0; i < numChildren; ++i) {
        pid_t pid;
        
        // Prepare arguments for child process
        // 准备子进程参数
        std::string childIdStr = std::to_string(i);
        char* argv[] = {
            const_cast<char*>(programPath),
            const_cast<char*>("child"),
            const_cast<char*>(childIdStr.c_str()),
            nullptr
        };

        // Spawn child process using posix_spawn
        // 使用 posix_spawn 启动子进程
        int status = posix_spawn(&pid, programPath, nullptr, nullptr, argv, environ);
        
        if (status == 0) {
            logger->Info("[Parent] Spawned child {} with PID={}", i, pid);
            childPids.push_back(pid);
        } else {
            logger->Error("[Parent] Failed to spawn child {}: {}", i, strerror(status));
        }
    }

    // Parent does some work while children are running
    // 父进程在子进程运行时做一些工作
    logger->Info("[Parent] Doing parent work...");
    for (int i = 0; i < 3; ++i) {
        logger->Info("[Parent] Parent working... step {}", i);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // Wait for all children to complete
    // 等待所有子进程完成
    logger->Info("[Parent] Waiting for children to complete...");
    for (pid_t pid : childPids) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            logger->Info("[Parent] Child PID={} exited with status {}", 
                        pid, WEXITSTATUS(status));
        }
    }

#else
    logger->Info("[Parent] posix_spawn not available on this platform");
    logger->Info("[Parent] Running single process demo instead");
    
    for (int i = 0; i < 5; ++i) {
        logger->Info("[Parent] Demo message {}", i);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
#endif

    logger->Info("[Parent] All done!");
    logger->Flush();
}

int main(int argc, char* argv[]) {
    std::cout << "=== onePlog Exec Child Process Example ===" << std::endl;
    std::cout << "=== onePlog Exec 子进程示例 ===" << std::endl;
    std::cout << std::endl;

    if (argc >= 3 && strcmp(argv[1], "child") == 0) {
        // Running as child process
        // 作为子进程运行
        int childId = atoi(argv[2]);
        RunAsChild(childId);
    } else {
        // Running as parent process
        // 作为父进程运行
        RunAsParent(argv[0]);
    }

    std::cout << std::endl;
    std::cout << "=== Example Complete / 示例完成 ===" << std::endl;

    return 0;
}

#else  // ONEPLOG_SYNC_ONLY

int main() {
    std::cout << "This example requires async mode support." << std::endl;
    std::cout << "此示例需要异步模式支持。" << std::endl;
    return 0;
}

#endif  // ONEPLOG_SYNC_ONLY
