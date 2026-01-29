/**
 * @file multithread_example.cpp
 * @brief Multi-process multi-thread example for onePlog
 * @brief onePlog 多进程多线程示例
 *
 * This example demonstrates:
 * - 4 processes (1 parent + 3 children)
 * - Each process has 4 threads
 * - Each thread simulates a different module (network, database, cache, worker)
 * - Module name inheritance using ThreadWithModuleName
 *
 * 此示例演示：
 * - 4 个进程（1 个父进程 + 3 个子进程）
 * - 每个进程有 4 个线程
 * - 每个线程模拟不同的模块（network, database, cache, worker）
 * - 使用 ThreadWithModuleName 实现模块名继承
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>

#ifndef ONEPLOG_SYNC_ONLY
#include <oneplog/oneplog.hpp>

#if defined(__unix__) || defined(__APPLE__)
#define ONEPLOG_HAS_FORK 1
#include <unistd.h>
#include <sys/wait.h>
#endif

// 模块名称列表 / Module name list
const char* kModuleNames[] = {"network", "database", "cache", "worker"};
constexpr int kThreadsPerProcess = 4;
constexpr int kMessagesPerThread = 5;

/**
 * @brief Thread worker function - simulates a module
 * @brief 线程工作函数 - 模拟一个模块
 */
void ThreadWorker([[maybe_unused]] int processId, int threadId, const std::string& processName) {
    const char* moduleName = kModuleNames[threadId % kThreadsPerProcess];
    
    // Set module name for this thread using NameManager
    // 使用 NameManager 设置此线程的模块名
    oneplog::NameManager::SetModuleName(moduleName);
    
    for (int i = 0; i < kMessagesPerThread; ++i) {
        // Log with process and module info / 记录带有进程和模块信息的日志
        log::Info("[{}-T{}] [{}] Processing task {}", processName, threadId, moduleName, i);
        
        // 模拟不同模块的工作时间 / Simulate different module work times
        int delay = 10 + threadId * 5;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
    }
    
    // WFC 确保最后一条消息写入 / WFC ensures last message is written
    log::InfoWFC("[{}-T{}] [{}] Module completed", processName, threadId, moduleName);
}

/**
 * @brief Demonstrate module name inheritance
 * @brief 演示模块名继承
 */
void DemoModuleNameInheritance(const std::string& processName) {
    std::cout << std::endl;
    std::cout << "--- Module Name Inheritance Demo / 模块名继承演示 ---" << std::endl;
    
    // Set parent module name / 设置父模块名
    oneplog::NameManager::SetModuleName("supervisor");
    log::Info("[{}] Parent thread module: {}", processName, oneplog::NameManager::GetModuleName());
    
    // Create child thread that inherits module name
    // 创建继承模块名的子线程
    auto childThread = oneplog::ThreadWithModuleName::Create([&processName]() {
        // Child inherits "supervisor" module name / 子线程继承 "supervisor" 模块名
        log::Info("[{}] Child thread inherited module: {}", 
                  processName, oneplog::NameManager::GetModuleName());
        
        // Create grandchild thread / 创建孙线程
        auto grandchildThread = oneplog::ThreadWithModuleName::Create([&processName]() {
            log::Info("[{}] Grandchild thread inherited module: {}", 
                      processName, oneplog::NameManager::GetModuleName());
        });
        grandchildThread.join();
    });
    childThread.join();
    
    // Create thread with explicit module name / 创建具有显式模块名的线程
    auto namedThread = oneplog::ThreadWithModuleName::CreateWithName("explicit_module", [&processName]() {
        log::Info("[{}] Thread with explicit module: {}", 
                  processName, oneplog::NameManager::GetModuleName());
    });
    namedThread.join();
}

/**
 * @brief Run process with multiple threads
 * @brief 运行带有多个线程的进程
 */
void RunProcess(int processId, const std::string& processName) {
    // 初始化日志器 / Initialize logger
    oneplog::LoggerConfig config;
    config.mode = oneplog::Mode::Async;
    config.level = oneplog::Level::Debug;
    config.processName = processName;  // Set process name via config / 通过配置设置进程名
    oneplog::Init(config);
    
    // Set initial module name / 设置初始模块名
    oneplog::NameManager::SetModuleName("main");
    
#ifdef ONEPLOG_HAS_FORK
    log::Info("[{}] Started with PID={}, launching {} threads", 
              processName, getpid(), kThreadsPerProcess);
#else
    log::Info("[{}] Started, launching {} threads", processName, kThreadsPerProcess);
#endif
    
    log::Info("[{}] Process={}, Module={}", 
              processName, 
              oneplog::NameManager::GetProcessName(),
              oneplog::NameManager::GetModuleName());
    log::Info("[{}] Modules: network(T0), database(T1), cache(T2), worker(T3)", processName);
    
    // 创建线程 / Create threads
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreadsPerProcess; ++t) {
        threads.emplace_back(ThreadWorker, processId, t, processName);
    }
    
    // 等待所有线程完成 / Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    log::InfoWFC("[{}] All {} threads completed", processName, kThreadsPerProcess);
    
    // Demonstrate module name inheritance (only in parent process)
    // 演示模块名继承（仅在父进程中）
    if (processId == 0) {
        DemoModuleNameInheritance(processName);
    }
    
    log::Flush();
    oneplog::Shutdown();
}

#ifdef ONEPLOG_HAS_FORK
/**
 * @brief Run as child process
 * @brief 作为子进程运行
 */
void RunChildProcess(int childId) {
    std::string processName = "child" + std::to_string(childId);
    RunProcess(childId, processName);
}
#endif

int main() {
    std::cout << "=== onePlog Multi-Process Multi-Thread Example ===" << std::endl;
    std::cout << "=== onePlog 多进程多线程示例 ===" << std::endl;
    std::cout << std::endl;
    std::cout << "Configuration / 配置:" << std::endl;
    std::cout << "  Processes / 进程数: 4 (1 parent + 3 children)" << std::endl;
    std::cout << "  Threads per process / 每进程线程数: " << kThreadsPerProcess << std::endl;
    std::cout << "  Modules / 模块: network(T0), database(T1), cache(T2), worker(T3)" << std::endl;
    std::cout << std::endl;

#ifdef ONEPLOG_HAS_FORK
    constexpr int kNumChildren = 3;
    std::vector<pid_t> childPids;
    
    // Fork 子进程 / Fork child processes
    for (int i = 0; i < kNumChildren; ++i) {
        pid_t pid = fork();
        
        if (pid < 0) {
            std::cerr << "Fork failed!" << std::endl;
            return 1;
        } else if (pid == 0) {
            // 子进程 / Child process
            RunChildProcess(i + 1);  // childId: 1, 2, 3
            _exit(0);
        } else {
            // 父进程记录子进程 PID / Parent records child PID
            childPids.push_back(pid);
        }
    }
    
    // 父进程也运行多线程 / Parent process also runs multi-threaded
    RunProcess(0, "parent");
    
    // 等待所有子进程完成 / Wait for all child processes
    std::cout << std::endl;
    std::cout << "--- Waiting for child processes / 等待子进程 ---" << std::endl;
    for (pid_t pid : childPids) {
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            std::cout << "Child PID=" << pid << " exited with status " 
                      << WEXITSTATUS(status) << std::endl;
        }
    }
    
#else
    // 非 Unix 系统：只运行单进程多线程
    // Non-Unix systems: run single process multi-threaded only
    std::cout << "Fork not available, running single process with multiple threads" << std::endl;
    std::cout << "Fork 不可用，运行单进程多线程" << std::endl;
    std::cout << std::endl;
    
    RunProcess(0, "main");
#endif

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
