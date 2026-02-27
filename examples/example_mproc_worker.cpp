/**
 * @file example_mproc_worker.cpp
 * @brief Multi-process mode worker (producer) process
 * @brief 多进程模式工作者（生产者）进程
 *
 * This is a WORKER process for standalone multi-process logging.
 * Run example_mproc_owner FIRST, then run this in separate terminals.
 *
 * 这是独立多进程日志记录的工作者进程。
 * 首先运行 example_mproc_owner，然后在单独的终端中运行此程序。
 *
 * Usage / 用法:
 *   Terminal 1: ./example_mproc_owner
 *   Terminal 2: ./example_mproc_worker 1
 *   Terminal 3: ./example_mproc_worker 2
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <string>
#include <oneplog/oneplog.hpp>

#ifndef _WIN32
#include <unistd.h>
#endif

int main(int argc, char* argv[]) {
    // Get worker ID from command line
    // 从命令行获取工作者 ID
    int workerId = 1;
    if (argc > 1) {
        workerId = std::atoi(argv[1]);
        if (workerId <= 0) workerId = 1;
    }
    
    std::string workerName = "Worker" + std::to_string(workerId);
    
    std::cout << "========================================" << std::endl;
    std::cout << "MProc Worker (Producer) Process #" << workerId << std::endl;
    std::cout << "多进程工作者（生产者）进程 #" << workerId << std::endl;
    std::cout << "========================================\n" << std::endl;
    
#ifndef _WIN32
    std::cout << "[" << workerName << "] PID: " << getpid() << std::endl;
#endif
    
    // Create logger - this process will attach to existing shared memory
    // 创建日志器 - 此进程将附加到现有共享内存
    oneplog::RuntimeConfig config;
    config.processName = workerName;
    
    oneplog::MProcLogger logger(config);
    
    // Use unified API to set process name (will register to shared memory automatically)
    // 使用统一 API 设置进程名（会自动注册到共享内存）
    oneplog::SetProcessName(workerName);
    
    if (logger.IsMProcOwner()) {
        std::cerr << "[" << workerName << "] WARNING: This process became the owner!" << std::endl;
        std::cerr << "[" << workerName << "] Make sure example_mproc_owner is running first." << std::endl;
        std::cerr << "[" << workerName << "] 警告：此进程成为了所有者！" << std::endl;
        std::cerr << "[" << workerName << "] 确保先运行 example_mproc_owner。" << std::endl;
        // Continue anyway for demonstration
    } else {
        std::cout << "[" << workerName << "] Attached to existing shared memory" << std::endl;
        std::cout << "[" << workerName << "] 已附加到现有共享内存" << std::endl;
    }
    
    logger.Info("=== {} started ===", workerName);
    
    // Send log messages
    // 发送日志消息
    constexpr int kMessageCount = 20;
    
    std::cout << "[" << workerName << "] Sending " << kMessageCount << " messages..." << std::endl;
    
    for (int i = 0; i < kMessageCount; ++i) {
        logger.Info("[{}] Message {}/{}", workerName, i + 1, kMessageCount);
        
        // Vary the delay to simulate real work
        // 变化延迟以模拟真实工作
        int delay = 100 + (i % 5) * 50;
        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        
        // Progress indicator
        // 进度指示器
        if ((i + 1) % 5 == 0) {
            std::cout << "[" << workerName << "] Progress: " << (i + 1) << "/" << kMessageCount << std::endl;
        }
    }
    
    logger.Info("=== {} finished ===", workerName);
    logger.Flush();
    
    std::cout << "\n[" << workerName << "] All messages sent" << std::endl;
    std::cout << "[" << workerName << "] 所有消息已发送" << std::endl;
    
    return 0;
}
