/**
 * @file benchmark_array_mapping.cpp
 * @brief Performance benchmark comparing ArrayMappingTable vs ThreadModuleTable
 * @brief ArrayMappingTable 与 ThreadModuleTable 的性能对比测试
 *
 * This benchmark compares the ArrayMappingTable (linear search) performance
 * against the original ThreadModuleTable implementation. ArrayMappingTable
 * is designed for non-Linux platforms (macOS/Windows) where TIDs can be large.
 *
 * 此测试对比 ArrayMappingTable（线性搜索）性能与原 ThreadModuleTable 实现。
 * ArrayMappingTable 专为非 Linux 平台（macOS/Windows）设计，这些平台的 TID 可能很大。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include "oneplog/benchmark/performance_benchmark.hpp"
#include "oneplog/internal/array_mapping_table.hpp"
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
 * @brief Benchmark ArrayMappingTable registration
 * @brief 测试 ArrayMappingTable 注册性能
 */
BenchmarkResult BenchmarkArrayMappingRegistration(size_t numOperations, size_t numThreads = 1) {
    static ArrayMappingTable<256, 15> table;
    table.Clear();

    BenchmarkResult result;
    result.testName = "ArrayMapping_Registration";
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
            // Use large TID values typical on macOS/Windows
            // 使用 macOS/Windows 上典型的大 TID 值
            uint32_t baseTid = static_cast<uint32_t>(t * 10000 + 100000);

            for (size_t i = 0; i < opsPerThread; ++i) {
                // Cycle through a limited set of TIDs to stay within table capacity
                // 循环使用有限的 TID 集合以保持在表容量内
                uint32_t tid = baseTid + static_cast<uint32_t>(i % 32);

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
 * @brief Benchmark ArrayMappingTable lookup
 * @brief 测试 ArrayMappingTable 查找性能
 */
BenchmarkResult BenchmarkArrayMappingLookup(size_t numOperations, size_t numThreads = 1) {
    static ArrayMappingTable<256, 15> table;
    table.Clear();

    // Pre-register entries with large TID values / 使用大 TID 值预注册条目
    for (uint32_t i = 0; i < 100; ++i) {
        table.Register(100000 + i, "module_" + std::to_string(i));
    }

    BenchmarkResult result;
    result.testName = "ArrayMapping_Lookup";
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
                uint32_t tid = 100000 + static_cast<uint32_t>(i % 100);

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
    PrintHeader("ArrayMappingTable vs ThreadModuleTable Comparison");
    std::cout << "ArrayMappingTable vs ThreadModuleTable 性能对比\n";
    std::cout << "(For non-Linux platforms: macOS/Windows)\n";
    std::cout << "（适用于非 Linux 平台：macOS/Windows）\n";

    constexpr size_t kNumOperations = 100000;

    // Single-thread registration comparison
    // 单线程注册对比
    std::cout << "\n[1] Registration Benchmark (Single Thread) / 注册测试（单线程）\n";
    std::cout << std::string(60, '-') << "\n";

    auto threadModuleReg = PerformanceBenchmark::BenchmarkRegistration(kNumOperations, 1);
    auto arrayMappingReg = BenchmarkArrayMappingRegistration(kNumOperations, 1);

    PerformanceBenchmark::PrintComparisonReport(threadModuleReg, arrayMappingReg, "Registration");

    // Single-thread lookup comparison
    // 单线程查找对比
    std::cout << "\n[2] Lookup Benchmark (Single Thread) / 查找测试（单线程）\n";
    std::cout << std::string(60, '-') << "\n";

    auto threadModuleLookup = PerformanceBenchmark::BenchmarkLookup(kNumOperations, 1);
    auto arrayMappingLookup = BenchmarkArrayMappingLookup(kNumOperations, 1);

    PerformanceBenchmark::PrintComparisonReport(threadModuleLookup, arrayMappingLookup, "Lookup");

    // Multi-thread lookup comparison
    // 多线程查找对比
    std::cout << "\n[3] Lookup Benchmark (4 Threads) / 查找测试（4 线程）\n";
    std::cout << std::string(60, '-') << "\n";

    auto threadModuleLookupMT = PerformanceBenchmark::BenchmarkLookup(kNumOperations, 4);
    auto arrayMappingLookupMT = BenchmarkArrayMappingLookup(kNumOperations, 4);

    PerformanceBenchmark::PrintComparisonReport(threadModuleLookupMT, arrayMappingLookupMT, "Lookup (4 threads)");
}

/**
 * @brief Print summary of performance comparison
 * @brief 打印性能对比摘要
 */
void PrintSummary() {
    PrintHeader("Performance Summary / 性能摘要");

    constexpr size_t kNumOperations = 100000;

    auto threadModuleLookup = PerformanceBenchmark::BenchmarkLookup(kNumOperations, 1);
    auto arrayMappingLookup = BenchmarkArrayMappingLookup(kNumOperations, 1);

    double throughputDiff = 0.0;
    if (threadModuleLookup.throughputOpsPerSec > 0) {
        throughputDiff = ((arrayMappingLookup.throughputOpsPerSec - threadModuleLookup.throughputOpsPerSec)
                          / threadModuleLookup.throughputOpsPerSec) * 100.0;
    }

    double latencyDiff = 0.0;
    if (threadModuleLookup.avgLatencyNs > 0) {
        latencyDiff = ((threadModuleLookup.avgLatencyNs - arrayMappingLookup.avgLatencyNs)
                       / threadModuleLookup.avgLatencyNs) * 100.0;
    }

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\nArrayMappingTable O(n) Lookup Performance:\n";
    std::cout << "ArrayMappingTable O(n) 查找性能:\n\n";

    std::cout << "ThreadModuleTable (Baseline / 基准):\n";
    std::cout << "  Throughput: " << threadModuleLookup.throughputOpsPerSec << " ops/sec\n";
    std::cout << "  Avg Latency: " << threadModuleLookup.avgLatencyNs << " ns\n";

    std::cout << "\nArrayMappingTable (Optimized / 优化后):\n";
    std::cout << "  Throughput: " << arrayMappingLookup.throughputOpsPerSec << " ops/sec\n";
    std::cout << "  Avg Latency: " << arrayMappingLookup.avgLatencyNs << " ns\n";

    std::cout << "\nComparison / 对比:\n";
    std::cout << "  Throughput: ";
    if (throughputDiff > 0) {
        std::cout << "+" << throughputDiff << "% (higher is better)\n";
    } else if (throughputDiff < 0) {
        std::cout << throughputDiff << "% (lower than baseline)\n";
    } else {
        std::cout << "same as baseline\n";
    }
    std::cout << "  Latency: ";
    if (latencyDiff > 0) {
        std::cout << latencyDiff << "% reduction (lower is better)\n";
    } else if (latencyDiff < 0) {
        std::cout << (-latencyDiff) << "% increase\n";
    } else {
        std::cout << "same as baseline\n";
    }

    std::cout << "\nNote: ArrayMappingTable uses O(n) linear search,\n";
    std::cout << "designed for non-Linux platforms where TIDs can be large.\n";
    std::cout << "It provides better memory efficiency than DirectMappingTable\n";
    std::cout << "at the cost of lookup performance.\n";
    std::cout << "注意：ArrayMappingTable 使用 O(n) 线性搜索，\n";
    std::cout << "专为 TID 可能很大的非 Linux 平台设计。\n";
    std::cout << "它以查找性能为代价提供比 DirectMappingTable 更好的内存效率。\n";
}

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "ArrayMappingTable Performance Benchmark\n";
    std::cout << "ArrayMappingTable 性能测试\n";
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
