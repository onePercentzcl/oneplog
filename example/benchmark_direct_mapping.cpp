/**
 * @file benchmark_direct_mapping.cpp
 * @brief Performance benchmark comparing DirectMappingTable vs ThreadModuleTable
 * @brief DirectMappingTable 与 ThreadModuleTable 的性能对比测试
 *
 * This benchmark compares the O(1) DirectMappingTable lookup performance
 * against the original ThreadModuleTable linear search implementation.
 *
 * 此测试对比 O(1) DirectMappingTable 查找性能与原 ThreadModuleTable 线性搜索实现。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include "oneplog/benchmark/performance_benchmark.hpp"
#include "oneplog/internal/direct_mapping_table.hpp"
#include "oneplog/name_manager.hpp"

#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace oneplog;
using namespace oneplog::benchmark;
using namespace oneplog::internal;

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
 * @brief Benchmark DirectMappingTable registration
 * @brief 测试 DirectMappingTable 注册性能
 */
BenchmarkResult BenchmarkDirectMappingRegistration(size_t numOperations, size_t numThreads = 1) {
    // Use smaller MaxTid for testing to avoid huge memory allocation
    // 使用较小的 MaxTid 进行测试以避免巨大的内存分配
    static DirectMappingTable<4096, 15> table;
    table.Clear();

    BenchmarkResult result;
    result.testName = "DirectMapping_Registration";
    result.numThreads = numThreads;
    result.totalOperations = numOperations;

    std::vector<LatencyCollector> collectors(numThreads);
    for (auto& collector : collectors) {
        collector.Reserve(numOperations / numThreads + 1);
    }

    std::atomic<bool> startFlag{false};
    std::vector<std::thread> threads;
    threads.reserve(numThreads);

    size_t opsPerThread = numOperations / numThreads;

    auto startTime = PerformanceBenchmark::Clock::now();

    for (size_t t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            while (!startFlag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            std::string name = "thread_" + std::to_string(t);
            uint32_t baseTid = static_cast<uint32_t>(t * 100);

            for (size_t i = 0; i < opsPerThread; ++i) {
                uint32_t tid = baseTid + static_cast<uint32_t>(i % 100);
                if (tid >= 4096) tid = tid % 4096;

                auto opStart = PerformanceBenchmark::Clock::now();
                table.Register(tid, name);
                auto opEnd = PerformanceBenchmark::Clock::now();

                double latencyNs = static_cast<double>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(opEnd - opStart).count());
                collectors[t].AddSample(latencyNs);
            }
        });
    }

    startFlag.store(true, std::memory_order_release);

    for (auto& thread : threads) {
        thread.join();
    }

    auto endTime = PerformanceBenchmark::Clock::now();

    for (auto& collector : collectors) {
        collector.Sort();
    }

    if (!collectors.empty() && collectors[0].Size() > 0) {
        result.avgLatencyNs = collectors[0].GetAverage();
        result.p50LatencyNs = collectors[0].GetP50();
        result.p99LatencyNs = collectors[0].GetP99();
        result.p999LatencyNs = collectors[0].GetP999();
        result.minLatencyNs = collectors[0].GetMin();
        result.maxLatencyNs = collectors[0].GetMax();
        result.stdDevNs = collectors[0].GetStdDev();
    }

    result.totalTimeMs = static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count()) / 1000.0;

    if (result.totalTimeMs > 0) {
        result.throughputOpsPerSec = static_cast<double>(numOperations) / (result.totalTimeMs / 1000.0);
    }

    return result;
}

/**
 * @brief Benchmark DirectMappingTable lookup
 * @brief 测试 DirectMappingTable 查找性能
 */
BenchmarkResult BenchmarkDirectMappingLookup(size_t numOperations, size_t numThreads = 1) {
    static DirectMappingTable<4096, 15> table;
    table.Clear();

    // Pre-register entries / 预注册条目
    for (uint32_t i = 0; i < 100; ++i) {
        table.Register(i, "module_" + std::to_string(i));
    }

    BenchmarkResult result;
    result.testName = "DirectMapping_Lookup";
    result.numThreads = numThreads;
    result.totalOperations = numOperations;

    std::vector<LatencyCollector> collectors(numThreads);
    for (auto& collector : collectors) {
        collector.Reserve(numOperations / numThreads + 1);
    }

    std::atomic<bool> startFlag{false};
    std::vector<std::thread> threads;
    threads.reserve(numThreads);

    size_t opsPerThread = numOperations / numThreads;

    auto startTime = PerformanceBenchmark::Clock::now();

    for (size_t t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            while (!startFlag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (size_t i = 0; i < opsPerThread; ++i) {
                uint32_t tid = static_cast<uint32_t>(i % 100);

                auto opStart = PerformanceBenchmark::Clock::now();
                volatile auto name = table.GetName(tid);
                (void)name;
                auto opEnd = PerformanceBenchmark::Clock::now();

                double latencyNs = static_cast<double>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(opEnd - opStart).count());
                collectors[t].AddSample(latencyNs);
            }
        });
    }

    startFlag.store(true, std::memory_order_release);

    for (auto& thread : threads) {
        thread.join();
    }

    auto endTime = PerformanceBenchmark::Clock::now();

    for (auto& collector : collectors) {
        collector.Sort();
    }

    if (!collectors.empty() && collectors[0].Size() > 0) {
        result.avgLatencyNs = collectors[0].GetAverage();
        result.p50LatencyNs = collectors[0].GetP50();
        result.p99LatencyNs = collectors[0].GetP99();
        result.p999LatencyNs = collectors[0].GetP999();
        result.minLatencyNs = collectors[0].GetMin();
        result.maxLatencyNs = collectors[0].GetMax();
        result.stdDevNs = collectors[0].GetStdDev();
    }

    result.totalTimeMs = static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count()) / 1000.0;

    if (result.totalTimeMs > 0) {
        result.throughputOpsPerSec = static_cast<double>(numOperations) / (result.totalTimeMs / 1000.0);
    }

    return result;
}

/**
 * @brief Run comparison benchmarks
 * @brief 运行对比测试
 */
void RunComparisonBenchmarks() {
    PrintHeader("DirectMappingTable vs ThreadModuleTable Comparison");
    std::cout << "DirectMappingTable vs ThreadModuleTable 性能对比\n";

    constexpr size_t kNumOperations = 100000;

    // Single-thread registration comparison
    // 单线程注册对比
    std::cout << "\n[1] Registration Benchmark (Single Thread) / 注册测试（单线程）\n";
    std::cout << std::string(60, '-') << "\n";

    auto threadModuleReg = PerformanceBenchmark::BenchmarkRegistration(kNumOperations, 1);
    auto directMappingReg = BenchmarkDirectMappingRegistration(kNumOperations, 1);

    PerformanceBenchmark::PrintComparisonReport(threadModuleReg, directMappingReg, "Registration");

    // Single-thread lookup comparison
    // 单线程查找对比
    std::cout << "\n[2] Lookup Benchmark (Single Thread) / 查找测试（单线程）\n";
    std::cout << std::string(60, '-') << "\n";

    auto threadModuleLookup = PerformanceBenchmark::BenchmarkLookup(kNumOperations, 1);
    auto directMappingLookup = BenchmarkDirectMappingLookup(kNumOperations, 1);

    PerformanceBenchmark::PrintComparisonReport(threadModuleLookup, directMappingLookup, "Lookup");

    // Multi-thread lookup comparison
    // 多线程查找对比
    std::cout << "\n[3] Lookup Benchmark (4 Threads) / 查找测试（4 线程）\n";
    std::cout << std::string(60, '-') << "\n";

    auto threadModuleLookupMT = PerformanceBenchmark::BenchmarkLookup(kNumOperations, 4);
    auto directMappingLookupMT = BenchmarkDirectMappingLookup(kNumOperations, 4);

    PerformanceBenchmark::PrintComparisonReport(threadModuleLookupMT, directMappingLookupMT, "Lookup (4 threads)");
}

/**
 * @brief Print summary of performance improvements
 * @brief 打印性能提升摘要
 */
void PrintSummary() {
    PrintHeader("Performance Summary / 性能摘要");

    constexpr size_t kNumOperations = 100000;

    auto threadModuleLookup = PerformanceBenchmark::BenchmarkLookup(kNumOperations, 1);
    auto directMappingLookup = BenchmarkDirectMappingLookup(kNumOperations, 1);

    double throughputImprovement = 0.0;
    if (threadModuleLookup.throughputOpsPerSec > 0) {
        throughputImprovement = ((directMappingLookup.throughputOpsPerSec - threadModuleLookup.throughputOpsPerSec)
                                 / threadModuleLookup.throughputOpsPerSec) * 100.0;
    }

    double latencyImprovement = 0.0;
    if (threadModuleLookup.avgLatencyNs > 0) {
        latencyImprovement = ((threadModuleLookup.avgLatencyNs - directMappingLookup.avgLatencyNs)
                              / threadModuleLookup.avgLatencyNs) * 100.0;
    }

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\nDirectMappingTable O(1) Lookup Performance:\n";
    std::cout << "DirectMappingTable O(1) 查找性能:\n\n";

    std::cout << "ThreadModuleTable (Baseline / 基准):\n";
    std::cout << "  Throughput: " << threadModuleLookup.throughputOpsPerSec << " ops/sec\n";
    std::cout << "  Avg Latency: " << threadModuleLookup.avgLatencyNs << " ns\n";

    std::cout << "\nDirectMappingTable (Optimized / 优化后):\n";
    std::cout << "  Throughput: " << directMappingLookup.throughputOpsPerSec << " ops/sec\n";
    std::cout << "  Avg Latency: " << directMappingLookup.avgLatencyNs << " ns\n";

    std::cout << "\nImprovement / 提升:\n";
    std::cout << "  Throughput: ";
    if (throughputImprovement > 0) {
        std::cout << "+" << throughputImprovement << "% (higher is better)\n";
    } else {
        std::cout << throughputImprovement << "% (regression)\n";
    }
    std::cout << "  Latency: ";
    if (latencyImprovement > 0) {
        std::cout << latencyImprovement << "% reduction (lower is better)\n";
    } else {
        std::cout << (-latencyImprovement) << "% increase (regression)\n";
    }

    std::cout << "\nNote: DirectMappingTable uses O(1) direct array indexing,\n";
    std::cout << "while ThreadModuleTable uses O(n) linear search.\n";
    std::cout << "注意：DirectMappingTable 使用 O(1) 直接数组索引，\n";
    std::cout << "而 ThreadModuleTable 使用 O(n) 线性搜索。\n";
}

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "DirectMappingTable Performance Benchmark\n";
    std::cout << "DirectMappingTable 性能测试\n";
    std::cout << "========================================\n";

    std::string mode = (argc > 1) ? argv[1] : "all";

    if (mode == "compare" || mode == "all") {
        RunComparisonBenchmarks();
    }

    if (mode == "summary" || mode == "all") {
        PrintSummary();
    }

    std::cout << "\n========================================\n";
    std::cout << "Benchmark Complete / 测试完成\n";
    std::cout << "========================================\n";

    return 0;
}
