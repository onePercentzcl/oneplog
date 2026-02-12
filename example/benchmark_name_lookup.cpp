/**
 * @file benchmark_name_lookup.cpp
 * @brief Performance benchmark for name lookup operations
 * @brief 名称查找操作的性能测试
 *
 * This benchmark measures the performance of the current ThreadModuleTable
 * implementation as a baseline for optimization comparison.
 *
 * 此测试测量当前 ThreadModuleTable 实现的性能，作为优化对比的基准。
 *
 * Requirements: 8.5
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include "oneplog/benchmark/performance_benchmark.hpp"
#include "oneplog/name_manager.hpp"

#include <iostream>
#include <string>

using namespace oneplog;
using namespace oneplog::benchmark;

/**
 * @brief Print section header
 * @brief 打印章节标题
 */
void PrintHeader(const std::string& title) {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << title << "\n";
    std::cout << "========================================\n";
}

/**
 * @brief Run single-threaded benchmarks
 * @brief 运行单线程测试
 */
void RunSingleThreadBenchmarks() {
    PrintHeader("Single-Thread Benchmarks / 单线程测试");

    constexpr size_t kNumOperations = 100000;

    // Registration benchmark
    // 注册测试
    std::cout << "\n[1] Registration Benchmark / 注册测试\n";
    auto regResult = PerformanceBenchmark::BenchmarkRegistration(kNumOperations, 1);
    PerformanceBenchmark::PrintResult(regResult);

    // Lookup benchmark
    // 查找测试
    std::cout << "\n[2] Lookup Benchmark / 查找测试\n";
    auto lookupResult = PerformanceBenchmark::BenchmarkLookup(kNumOperations, 1);
    PerformanceBenchmark::PrintResult(lookupResult);

    // Logging benchmark (Sync mode)
    // 日志测试（同步模式）
    std::cout << "\n[3] Logging Benchmark (Sync Mode) / 日志测试（同步模式）\n";
    auto syncResult = PerformanceBenchmark::BenchmarkLogging(Mode::Sync, kNumOperations, 1);
    PerformanceBenchmark::PrintResult(syncResult);

    // Logging benchmark (Async mode)
    // 日志测试（异步模式）
    std::cout << "\n[4] Logging Benchmark (Async Mode) / 日志测试（异步模式）\n";
    auto asyncResult = PerformanceBenchmark::BenchmarkLogging(Mode::Async, kNumOperations, 1);
    PerformanceBenchmark::PrintResult(asyncResult);
}

/**
 * @brief Run multi-threaded benchmarks
 * @brief 运行多线程测试
 */
void RunMultiThreadBenchmarks() {
    PrintHeader("Multi-Thread Benchmarks / 多线程测试");

    constexpr size_t kNumOperations = 100000;
    constexpr size_t kNumThreads = 4;

    // Registration benchmark
    // 注册测试
    std::cout << "\n[1] Registration Benchmark (" << kNumThreads << " threads) / 注册测试\n";
    auto regResult = PerformanceBenchmark::BenchmarkRegistration(kNumOperations, kNumThreads);
    PerformanceBenchmark::PrintResult(regResult);

    // Lookup benchmark
    // 查找测试
    std::cout << "\n[2] Lookup Benchmark (" << kNumThreads << " threads) / 查找测试\n";
    auto lookupResult = PerformanceBenchmark::BenchmarkLookup(kNumOperations, kNumThreads);
    PerformanceBenchmark::PrintResult(lookupResult);

    // Logging benchmark (Async mode)
    // 日志测试（异步模式）
    std::cout << "\n[3] Logging Benchmark (Async Mode, " << kNumThreads << " threads) / 日志测试\n";
    auto asyncResult = PerformanceBenchmark::BenchmarkLogging(Mode::Async, kNumOperations, kNumThreads);
    PerformanceBenchmark::PrintResult(asyncResult);
}

/**
 * @brief Run scalability test with varying thread counts
 * @brief 运行不同线程数的可扩展性测试
 */
void RunScalabilityTest() {
    PrintHeader("Scalability Test / 可扩展性测试");

    constexpr size_t kNumOperations = 100000;
    std::vector<size_t> threadCounts = {1, 2, 4, 8};

    std::cout << "\nLookup throughput vs thread count:\n";
    std::cout << "查找吞吐量 vs 线程数:\n\n";

    std::cout << std::left << std::setw(10) << "Threads"
              << std::right << std::setw(20) << "Throughput (ops/s)"
              << std::setw(15) << "Avg Latency"
              << std::setw(15) << "P99 Latency"
              << "\n";
    std::cout << std::string(60, '-') << "\n";

    for (size_t numThreads : threadCounts) {
        auto result = PerformanceBenchmark::BenchmarkLookup(kNumOperations, numThreads);
        
        std::cout << std::left << std::setw(10) << numThreads
                  << std::right << std::setw(20) << std::fixed << std::setprecision(0)
                  << result.throughputOpsPerSec
                  << std::setw(15) << std::setprecision(2) << result.avgLatencyNs
                  << std::setw(15) << result.p99LatencyNs
                  << "\n";
    }
}

/**
 * @brief Print baseline summary for future comparison
 * @brief 打印基准摘要用于未来对比
 */
void PrintBaselineSummary() {
    PrintHeader("Baseline Summary / 基准摘要");

    constexpr size_t kNumOperations = 100000;

    std::cout << "\nCurrent ThreadModuleTable Performance (Baseline):\n";
    std::cout << "当前 ThreadModuleTable 性能（基准）:\n\n";

    // Single-thread lookup (most important metric)
    // 单线程查找（最重要的指标）
    auto lookupResult = PerformanceBenchmark::BenchmarkLookup(kNumOperations, 1);

    std::cout << "Single-Thread Lookup / 单线程查找:\n";
    std::cout << "  Throughput: " << std::fixed << std::setprecision(0)
              << lookupResult.throughputOpsPerSec << " ops/sec\n";
    std::cout << "  Avg Latency: " << std::setprecision(2)
              << lookupResult.avgLatencyNs << " ns\n";
    std::cout << "  P50 Latency: " << lookupResult.p50LatencyNs << " ns\n";
    std::cout << "  P99 Latency: " << lookupResult.p99LatencyNs << " ns\n";
    std::cout << "  P999 Latency: " << lookupResult.p999LatencyNs << " ns\n";

    std::cout << "\n";
    std::cout << "Save these values for comparison after optimization.\n";
    std::cout << "保存这些值用于优化后对比。\n";
}

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "onePlog Name Lookup Benchmark\n";
    std::cout << "onePlog 名称查找性能测试\n";
    std::cout << "========================================\n";

    // Check for specific test mode
    // 检查特定测试模式
    std::string mode = (argc > 1) ? argv[1] : "all";

    if (mode == "single" || mode == "all") {
        RunSingleThreadBenchmarks();
    }

    if (mode == "multi" || mode == "all") {
        RunMultiThreadBenchmarks();
    }

    if (mode == "scale" || mode == "all") {
        RunScalabilityTest();
    }

    if (mode == "summary" || mode == "all") {
        PrintBaselineSummary();
    }

    std::cout << "\n========================================\n";
    std::cout << "Benchmark Complete / 测试完成\n";
    std::cout << "========================================\n";

    return 0;
}
