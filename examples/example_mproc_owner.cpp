/**
 * @file example_mproc_owner.cpp
 * @brief Multi-process mode owner (consumer) process
 * @brief 多进程模式所有者（消费者）进程
 *
 * This is the OWNER process for standalone multi-process logging.
 * Run this process FIRST, then run example_mproc_worker in separate terminals.
 *
 * 这是独立多进程日志记录的所有者进程。
 * 首先运行此进程，然后在单独的终端中运行 example_mproc_worker。
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
#include <csignal>
#include <atomic>
#include <oneplog/oneplog.hpp>

#ifndef _WIN32
#include <unistd.h>
#endif

// Global flag for graceful shutdown
// 用于优雅关闭的全局标志
std::atomic<bool> g_running{true};

void SignalHandler(int signal) {
    std::cout << "\n[Owner] Received signal " << signal << ", shutting down..." << std::endl;
    g_running = false;
}

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "MProc Owner (Consumer) Process" << std::endl;
    std::cout << "多进程所有者（消费者）进程" << std::endl;
    std::cout << "========================================\n" << std::endl;
    
    // Setup signal handlers
    // 设置信号处理器
    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);
    
#ifndef _WIN32
    std::cout << "[Owner] PID: " << getpid() << std::endl;
#endif
    
    // Create logger - this process will be the owner
    // 创建日志器 - 此进程将成为所有者
    oneplog::RuntimeConfig config;
    config.processName = "Owner";
    
    oneplog::MProcLogger logger(config);
    
    // Use unified API to set process name (will register to shared memory automatically)
    // 使用统一 API 设置进程名（会自动注册到共享内存）
    oneplog::SetProcessName("Owner");
    
    if (!logger.IsMProcOwner()) {
        std::cerr << "[Owner] ERROR: This process should be the owner!" << std::endl;
        std::cerr << "[Owner] Make sure no other owner process is running." << std::endl;
        std::cerr << "[Owner] 错误：此进程应该是所有者！" << std::endl;
        std::cerr << "[Owner] 确保没有其他所有者进程在运行。" << std::endl;
        return 1;
    }
    
    std::cout << "[Owner] Logger created successfully as OWNER" << std::endl;
    std::cout << "[Owner] 日志器已成功创建为所有者" << std::endl;
    std::cout << "\n[Owner] Waiting for worker processes..." << std::endl;
    std::cout << "[Owner] 等待工作进程..." << std::endl;
    std::cout << "[Owner] Run './example_mproc_worker <id>' in other terminals" << std::endl;
    std::cout << "[Owner] 在其他终端运行 './example_mproc_worker <id>'" << std::endl;
    std::cout << "[Owner] Press Ctrl+C to shutdown" << std::endl;
    std::cout << "[Owner] 按 Ctrl+C 关闭\n" << std::endl;
    
    logger.Info("=== Owner process started ===");
    
    int counter = 0;
    while (g_running) {
        // Owner also logs periodically
        // 所有者也定期记录日志
        if (counter % 50 == 0) {
            logger.Info("[Owner] Heartbeat {}", counter / 50);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ++counter;
    }
    
    logger.Info("=== Owner process shutting down ===");
    
    std::cout << "\n[Owner] Flushing and shutting down..." << std::endl;
    logger.Flush();
    
    // Give workers time to finish
    // 给工作进程时间完成
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    logger.Shutdown();
    
    std::cout << "[Owner] Shutdown complete" << std::endl;
    std::cout << "[Owner] 关闭完成" << std::endl;
    
    return 0;
}
