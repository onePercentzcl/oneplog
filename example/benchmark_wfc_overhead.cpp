/**
 * @file benchmark_wfc_overhead.cpp
 * @brief Benchmark to compare performance with WFC enabled vs disabled
 * @brief 比较启用 WFC 和禁用 WFC 时的性能基准测试
 *
 * This benchmark tests the overhead of having WFC enabled at compile time,
 * even when not using WFC logging methods. It compares:
 * - Async mode: TemplateLogger<Mode::Async, ...>
 * - MProc mode: TemplateLogger<Mode::MProc, ...>
 *
 * Both loggers use normal Info() calls, not InfoWFC() calls.
 *
 * 此基准测试测试在编译时启用 WFC 的开销，即使不使用 WFC 日志方法。
 * 它比较：
 * - 异步模式：TemplateLogger<Mode::Async, ...>
 * - 多进程模式：TemplateLogger<Mode::MProc, ...>
 *
 * 两个日志器都使用普通的 Info() 调用，而不是 InfoWFC() 调用。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <thread>
#include <vector>

#include "oneplog/logger.hpp"
#include "oneplog/sink.hpp"
#include "oneplog/format.hpp"

using namespace oneplog;

// ==============================================================================
// Configuration / 配置
// ==============================================================================

struct BenchmarkConfig {
    size_t iterations = 500000;
    size_t warmup = 10000;
    size_t runs = 100;  // 默认 100 次运行
    size_t threads = 4;
    std::string sharedMemoryName = "/oneplog_wfc_bench";
};

// ==============================================================================
// Statistics / 统计
// ==============================================================================

struct Stats {
    double mean;
    double stddev;
    double min;
    double max;
};

Stats CalculateStats(const std::vector<double>& values) {
    Stats stats{};
    if (values.empty()) return stats;
    
    stats.mean = std::accumulate(values.begin(), values.end(), 0.0) / values.size();
    stats.min = *std::min_element(values.begin(), values.end());
    stats.max = *std::max_element(values.begin(), values.end());
    
    double sq_sum = 0.0;
    for (double v : values) {
        sq_sum += (v - stats.mean) * (v - stats.mean);
    }
    stats.stddev = std::sqrt(sq_sum / values.size());
    
    return stats;
}

// ==============================================================================
// Null Sink (discards all output) / 空 Sink（丢弃所有输出）
// ==============================================================================

class NullSink : public Sink {
public:
    void Write(const std::string&) override {}
    void Write(std::string_view) override {}
    void Flush() override {}
    void Close() override {}
    bool HasError() const override { return false; }
    std::string GetLastError() const override { return ""; }
};

// ==============================================================================
// Async Mode Benchmark Functions / 异步模式基准测试函数
// ==============================================================================

// Single-threaded async benchmark with WFC disabled
// 单线程异步基准测试（禁用 WFC）
double BenchmarkAsyncWFCDisabled(size_t iterations) {
    Logger<Mode::Async, Level::Debug, false> logger;
    logger.SetSink(std::make_shared<NullSink>());
    logger.SetFormat(std::make_shared<ConsoleFormat>());
    logger.Init();
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < iterations; ++i) {
        logger.Info("Test message {}", static_cast<int>(i % 1000000));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    logger.Shutdown();
    
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return static_cast<double>(iterations) / (duration.count() / 1e9);
}

// Single-threaded async benchmark with WFC enabled
// 单线程异步基准测试（启用 WFC）
double BenchmarkAsyncWFCEnabled(size_t iterations) {
    Logger<Mode::Async, Level::Debug, true> logger;
    logger.SetSink(std::make_shared<NullSink>());
    logger.SetFormat(std::make_shared<ConsoleFormat>());
    logger.Init();
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < iterations; ++i) {
        logger.Info("Test message {}", static_cast<int>(i % 1000000));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    logger.Shutdown();
    
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return static_cast<double>(iterations) / (duration.count() / 1e9);
}

// Multi-threaded async benchmark with WFC disabled
// 多线程异步基准测试（禁用 WFC）
double BenchmarkAsyncWFCDisabledMT(size_t iterations, size_t numThreads) {
    Logger<Mode::Async, Level::Debug, false> logger;
    logger.SetSink(std::make_shared<NullSink>());
    logger.SetFormat(std::make_shared<ConsoleFormat>());
    logger.Init();
    
    size_t perThread = iterations / numThreads;
    std::vector<std::thread> threads;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t t = 0; t < numThreads; ++t) {
        threads.emplace_back([&logger, perThread, t]() {
            for (size_t i = 0; i < perThread; ++i) {
                logger.Info("Thread {} message {}", static_cast<int>(t), static_cast<int>(i % 1000000));
            }
        });
    }
    
    for (auto& th : threads) {
        th.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    logger.Shutdown();
    
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return static_cast<double>(iterations) / (duration.count() / 1e9);
}

// Multi-threaded async benchmark with WFC enabled
// 多线程异步基准测试（启用 WFC）
double BenchmarkAsyncWFCEnabledMT(size_t iterations, size_t numThreads) {
    Logger<Mode::Async, Level::Debug, true> logger;
    logger.SetSink(std::make_shared<NullSink>());
    logger.SetFormat(std::make_shared<ConsoleFormat>());
    logger.Init();
    
    size_t perThread = iterations / numThreads;
    std::vector<std::thread> threads;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t t = 0; t < numThreads; ++t) {
        threads.emplace_back([&logger, perThread, t]() {
            for (size_t i = 0; i < perThread; ++i) {
                logger.Info("Thread {} message {}", static_cast<int>(t), static_cast<int>(i % 1000000));
            }
        });
    }
    
    for (auto& th : threads) {
        th.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    logger.Shutdown();
    
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return static_cast<double>(iterations) / (duration.count() / 1e9);
}

// ==============================================================================
// MProc Mode Benchmark Functions / 多进程模式基准测试函数
// ==============================================================================

// Single-threaded MProc benchmark with WFC disabled
// 单线程多进程基准测试（禁用 WFC）
double BenchmarkMProcWFCDisabled(size_t iterations, const std::string& shmName) {
    Logger<Mode::MProc, Level::Debug, false> logger;
    logger.SetSink(std::make_shared<NullSink>());
    logger.SetFormat(std::make_shared<ConsoleFormat>());
    
    LoggerConfig config;
    config.sharedMemoryName = shmName + "_disabled";
    logger.Init(config);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < iterations; ++i) {
        logger.Info("Test message {}", static_cast<int>(i % 1000000));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    logger.Shutdown();
    
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return static_cast<double>(iterations) / (duration.count() / 1e9);
}

// Single-threaded MProc benchmark with WFC enabled
// 单线程多进程基准测试（启用 WFC）
double BenchmarkMProcWFCEnabled(size_t iterations, const std::string& shmName) {
    Logger<Mode::MProc, Level::Debug, true> logger;
    logger.SetSink(std::make_shared<NullSink>());
    logger.SetFormat(std::make_shared<ConsoleFormat>());
    
    LoggerConfig config;
    config.sharedMemoryName = shmName + "_enabled";
    logger.Init(config);
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < iterations; ++i) {
        logger.Info("Test message {}", static_cast<int>(i % 1000000));
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    logger.Shutdown();
    
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return static_cast<double>(iterations) / (duration.count() / 1e9);
}

// Multi-threaded MProc benchmark with WFC disabled
// 多线程多进程基准测试（禁用 WFC）
double BenchmarkMProcWFCDisabledMT(size_t iterations, size_t numThreads, const std::string& shmName) {
    Logger<Mode::MProc, Level::Debug, false> logger;
    logger.SetSink(std::make_shared<NullSink>());
    logger.SetFormat(std::make_shared<ConsoleFormat>());
    
    LoggerConfig config;
    config.sharedMemoryName = shmName + "_disabled_mt";
    logger.Init(config);
    
    size_t perThread = iterations / numThreads;
    std::vector<std::thread> threads;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t t = 0; t < numThreads; ++t) {
        threads.emplace_back([&logger, perThread, t]() {
            for (size_t i = 0; i < perThread; ++i) {
                logger.Info("Thread {} message {}", static_cast<int>(t), static_cast<int>(i % 1000000));
            }
        });
    }
    
    for (auto& th : threads) {
        th.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    logger.Shutdown();
    
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return static_cast<double>(iterations) / (duration.count() / 1e9);
}

// Multi-threaded MProc benchmark with WFC enabled
// 多线程多进程基准测试（启用 WFC）
double BenchmarkMProcWFCEnabledMT(size_t iterations, size_t numThreads, const std::string& shmName) {
    Logger<Mode::MProc, Level::Debug, true> logger;
    logger.SetSink(std::make_shared<NullSink>());
    logger.SetFormat(std::make_shared<ConsoleFormat>());
    
    LoggerConfig config;
    config.sharedMemoryName = shmName + "_enabled_mt";
    logger.Init(config);
    
    size_t perThread = iterations / numThreads;
    std::vector<std::thread> threads;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t t = 0; t < numThreads; ++t) {
        threads.emplace_back([&logger, perThread, t]() {
            for (size_t i = 0; i < perThread; ++i) {
                logger.Info("Thread {} message {}", static_cast<int>(t), static_cast<int>(i % 1000000));
            }
        });
    }
    
    for (auto& th : threads) {
        th.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    logger.Shutdown();
    
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    return static_cast<double>(iterations) / (duration.count() / 1e9);
}

// ==============================================================================
// Helper Functions / 辅助函数
// ==============================================================================

void PrintHeader() {
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "     WFC Compile-Time Overhead Benchmark\n";
    std::cout << "     WFC 编译时开销基准测试\n";
    std::cout << "================================================================\n";
}

void PrintConfig(const BenchmarkConfig& config) {
    std::cout << "\n  Config / 配置:\n";
    std::cout << "    Iterations / 迭代次数: " << config.iterations << "\n";
    std::cout << "    Threads / 线程数:      " << config.threads << "\n";
    std::cout << "    Warmup / 预热:         " << config.warmup << "\n";
    std::cout << "    Runs / 运行次数:       " << config.runs << "\n";
    std::cout << "\n";
}

std::string FormatThroughput(double ops) {
    std::ostringstream oss;
    if (ops >= 1e6) {
        oss << std::fixed << std::setprecision(2) << (ops / 1e6) << " M";
    } else if (ops >= 1e3) {
        oss << std::fixed << std::setprecision(2) << (ops / 1e3) << " K";
    } else {
        oss << std::fixed << std::setprecision(2) << ops;
    }
    return oss.str();
}

void PrintResult(const std::string& name, const Stats& stats) {
    std::cout << "  " << std::left << std::setw(30) << name 
              << FormatThroughput(stats.mean) << " ± " 
              << FormatThroughput(stats.stddev) << " ops/sec\n";
}

void PrintComparison(const std::string& name, double disabled, double enabled) {
    double diff = ((enabled - disabled) / disabled) * 100.0;
    std::string comparison;
    if (std::abs(diff) < 1.0) {
        comparison = "no significant difference";
    } else if (diff > 0) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << "+" << diff << "% (WFC enabled faster)";
        comparison = oss.str();
    } else {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << diff << "% (WFC enabled slower)";
        comparison = oss.str();
    }
    std::cout << "  " << std::left << std::setw(30) << name << comparison << "\n";
}

// ==============================================================================
// Main / 主函数
// ==============================================================================

int main(int argc, char* argv[]) {
    BenchmarkConfig config;
    
    // Parse command line arguments / 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-i" && i + 1 < argc) {
            config.iterations = std::stoull(argv[++i]);
        } else if (arg == "-t" && i + 1 < argc) {
            config.threads = std::stoull(argv[++i]);
        } else if (arg == "-r" && i + 1 < argc) {
            config.runs = std::stoull(argv[++i]);
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "Usage: " << argv[0] << " [options]\n";
            std::cout << "  -i <iterations>  Number of iterations (default: 500000)\n";
            std::cout << "  -t <threads>     Number of threads (default: 4)\n";
            std::cout << "  -r <runs>        Number of runs (default: 100)\n";
            return 0;
        }
    }
    
    PrintHeader();
    PrintConfig(config);
    
    std::vector<double> asyncDisabled, asyncEnabled;
    std::vector<double> asyncMTDisabled, asyncMTEnabled;
    std::vector<double> mprocDisabled, mprocEnabled;
    std::vector<double> mprocMTDisabled, mprocMTEnabled;
    
    std::cout << "  Running benchmarks... / 运行基准测试...\n\n";
    
    // Warmup / 预热
    std::cout << "  [Warmup] / [预热]...\n";
    BenchmarkAsyncWFCDisabled(config.warmup);
    BenchmarkAsyncWFCEnabled(config.warmup);
    BenchmarkMProcWFCDisabled(config.warmup, config.sharedMemoryName);
    BenchmarkMProcWFCEnabled(config.warmup, config.sharedMemoryName);
    
    // Run benchmarks / 运行基准测试
    for (size_t run = 0; run < config.runs; ++run) {
        std::cout << "  [Run " << (run + 1) << "/" << config.runs << "]...          \r" << std::flush;
        
        // Async mode / 异步模式
        asyncDisabled.push_back(BenchmarkAsyncWFCDisabled(config.iterations));
        asyncEnabled.push_back(BenchmarkAsyncWFCEnabled(config.iterations));
        asyncMTDisabled.push_back(BenchmarkAsyncWFCDisabledMT(config.iterations, config.threads));
        asyncMTEnabled.push_back(BenchmarkAsyncWFCEnabledMT(config.iterations, config.threads));
        
        // MProc mode / 多进程模式
        mprocDisabled.push_back(BenchmarkMProcWFCDisabled(config.iterations, config.sharedMemoryName));
        mprocEnabled.push_back(BenchmarkMProcWFCEnabled(config.iterations, config.sharedMemoryName));
        mprocMTDisabled.push_back(BenchmarkMProcWFCDisabledMT(config.iterations, config.threads, config.sharedMemoryName));
        mprocMTEnabled.push_back(BenchmarkMProcWFCEnabledMT(config.iterations, config.threads, config.sharedMemoryName));
    }
    
    std::cout << "\n";
    
    // Calculate statistics / 计算统计数据
    Stats asyncDisabledStats = CalculateStats(asyncDisabled);
    Stats asyncEnabledStats = CalculateStats(asyncEnabled);
    Stats asyncMTDisabledStats = CalculateStats(asyncMTDisabled);
    Stats asyncMTEnabledStats = CalculateStats(asyncMTEnabled);
    Stats mprocDisabledStats = CalculateStats(mprocDisabled);
    Stats mprocEnabledStats = CalculateStats(mprocEnabled);
    Stats mprocMTDisabledStats = CalculateStats(mprocMTDisabled);
    Stats mprocMTEnabledStats = CalculateStats(mprocMTEnabled);
    
    // Print results / 打印结果
    std::cout << "  ============================================================\n";
    std::cout << "  Results / 结果:\n";
    std::cout << "  ------------------------------------------------------------\n";
    
    std::cout << "\n  [Async Mode (1 Thread) / 异步模式 (单线程)]\n";
    PrintResult("WFC Disabled (EnableWFC=false)", asyncDisabledStats);
    PrintResult("WFC Enabled (EnableWFC=true)", asyncEnabledStats);
    
    std::cout << "\n  [Async Mode (" << config.threads << " Threads) / 异步模式 (" 
              << config.threads << " 线程)]\n";
    PrintResult("WFC Disabled (EnableWFC=false)", asyncMTDisabledStats);
    PrintResult("WFC Enabled (EnableWFC=true)", asyncMTEnabledStats);
    
    std::cout << "\n  [MProc Mode (1 Thread) / 多进程模式 (单线程)]\n";
    PrintResult("WFC Disabled (EnableWFC=false)", mprocDisabledStats);
    PrintResult("WFC Enabled (EnableWFC=true)", mprocEnabledStats);
    
    std::cout << "\n  [MProc Mode (" << config.threads << " Threads) / 多进程模式 (" 
              << config.threads << " 线程)]\n";
    PrintResult("WFC Disabled (EnableWFC=false)", mprocMTDisabledStats);
    PrintResult("WFC Enabled (EnableWFC=true)", mprocMTEnabledStats);
    
    // Print comparison / 打印对比
    std::cout << "\n  ============================================================\n";
    std::cout << "  Overhead Analysis / 开销分析:\n";
    std::cout << "  ------------------------------------------------------------\n";
    PrintComparison("Async Mode (1T):", asyncDisabledStats.mean, asyncEnabledStats.mean);
    PrintComparison("Async Mode (MT):", asyncMTDisabledStats.mean, asyncMTEnabledStats.mean);
    PrintComparison("MProc Mode (1T):", mprocDisabledStats.mean, mprocEnabledStats.mean);
    PrintComparison("MProc Mode (MT):", mprocMTDisabledStats.mean, mprocMTEnabledStats.mean);
    
    std::cout << "\n  ============================================================\n";
    std::cout << "  Conclusion / 结论:\n";
    std::cout << "  ------------------------------------------------------------\n";
    
    double avgOverhead = (
        ((asyncEnabledStats.mean - asyncDisabledStats.mean) / asyncDisabledStats.mean) +
        ((asyncMTEnabledStats.mean - asyncMTDisabledStats.mean) / asyncMTDisabledStats.mean) +
        ((mprocEnabledStats.mean - mprocDisabledStats.mean) / mprocDisabledStats.mean) +
        ((mprocMTEnabledStats.mean - mprocMTDisabledStats.mean) / mprocMTDisabledStats.mean)
    ) / 4.0 * 100.0;
    
    if (std::abs(avgOverhead) < 2.0) {
        std::cout << "  WFC compile-time flag has negligible overhead when not using WFC methods.\n";
        std::cout << "  WFC 编译时标志在不使用 WFC 方法时开销可忽略不计。\n";
    } else if (avgOverhead < 0) {
        std::cout << "  WFC enabled shows slightly better performance (likely noise).\n";
        std::cout << "  启用 WFC 显示略好的性能（可能是噪声）。\n";
    } else {
        std::cout << "  WFC enabled shows ~" << std::fixed << std::setprecision(1) 
                  << avgOverhead << "% overhead.\n";
        std::cout << "  启用 WFC 显示约 " << std::fixed << std::setprecision(1) 
                  << avgOverhead << "% 的开销。\n";
    }
    
    std::cout << "\n================================================================\n";
    std::cout << "  Benchmark complete! / 测试完成!\n";
    std::cout << "================================================================\n\n";
    
    return 0;
}
