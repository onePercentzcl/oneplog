/**
 * @file performance_benchmark.hpp
 * @brief Performance benchmark tool for name lookup operations
 * @brief 名称查找操作的性能测试工具
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include "oneplog/benchmark/latency_collector.hpp"
#include "oneplog/common.hpp"
#include "oneplog/name_manager.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace oneplog::benchmark {

/**
 * @brief Performance benchmark results
 * @brief 性能测试结果
 *
 * Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7
 */
struct BenchmarkResult {
    double throughputOpsPerSec{0.0};  ///< Operations per second / 每秒操作数
    double avgLatencyNs{0.0};         ///< Average latency in nanoseconds / 平均延迟（纳秒）
    double p50LatencyNs{0.0};         ///< 50th percentile latency / P50 延迟
    double p99LatencyNs{0.0};         ///< 99th percentile latency / P99 延迟
    double p999LatencyNs{0.0};        ///< 99.9th percentile latency / P999 延迟
    double minLatencyNs{0.0};         ///< Minimum latency / 最小延迟
    double maxLatencyNs{0.0};         ///< Maximum latency / 最大延迟
    double stdDevNs{0.0};             ///< Standard deviation / 标准差
    size_t totalOperations{0};        ///< Total operations performed / 总操作数
    size_t numThreads{1};             ///< Number of threads used / 使用的线程数
    double totalTimeMs{0.0};          ///< Total test time in milliseconds / 总测试时间（毫秒）
    std::string testName;             ///< Test name / 测试名称

    /**
     * @brief Convert result to string for display
     * @brief 将结果转换为显示字符串
     */
    std::string ToString() const {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2);
        oss << "Test: " << testName << "\n";
        oss << "  Threads: " << numThreads << "\n";
        oss << "  Operations: " << totalOperations << "\n";
        oss << "  Total Time: " << totalTimeMs << " ms\n";
        oss << "  Throughput: " << throughputOpsPerSec << " ops/sec\n";
        oss << "  Latency (ns):\n";
        oss << "    Avg: " << avgLatencyNs << "\n";
        oss << "    Min: " << minLatencyNs << "\n";
        oss << "    Max: " << maxLatencyNs << "\n";
        oss << "    P50: " << p50LatencyNs << "\n";
        oss << "    P99: " << p99LatencyNs << "\n";
        oss << "    P999: " << p999LatencyNs << "\n";
        oss << "    StdDev: " << stdDevNs << "\n";
        return oss.str();
    }
};

/**
 * @brief Performance benchmark tool for name lookup operations
 * @brief 名称查找操作的性能测试工具
 *
 * Provides methods to benchmark registration, lookup, and full logging path
 * with support for multi-threaded scenarios.
 *
 * 提供注册、查找和完整日志路径的基准测试方法，支持多线程场景。
 *
 * Requirements: 8.1, 8.2, 8.3, 8.4, 8.5, 8.6, 8.7
 */
class PerformanceBenchmark {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;
    using Duration = std::chrono::nanoseconds;

    /**
     * @brief Benchmark name registration throughput
     * @brief 测试名称注册吞吐量
     * @param numOperations Number of operations to perform / 要执行的操作数
     * @param numThreads Number of concurrent threads / 并发线程数
     * @return Benchmark results / 测试结果
     */
    static BenchmarkResult BenchmarkRegistration(size_t numOperations, size_t numThreads = 1) {
        BenchmarkResult result;
        result.testName = "Registration";
        result.numThreads = numThreads;
        result.totalOperations = numOperations;

        // Clear existing table
        // 清空现有表
        internal::GetGlobalThreadModuleTable().Clear();

        // Prepare thread collectors
        // 准备线程收集器
        std::vector<LatencyCollector> collectors(numThreads);
        for (auto& collector : collectors) {
            collector.Reserve(numOperations / numThreads + 1);
        }

        std::atomic<bool> startFlag{false};
        std::vector<std::thread> threads;
        threads.reserve(numThreads);

        size_t opsPerThread = numOperations / numThreads;

        auto startTime = Clock::now();

        for (size_t t = 0; t < numThreads; ++t) {
            threads.emplace_back([&, t]() {
                // Wait for start signal
                // 等待开始信号
                while (!startFlag.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }

                std::string name = "thread_" + std::to_string(t);
                uint32_t baseTid = static_cast<uint32_t>(t * 1000);

                for (size_t i = 0; i < opsPerThread; ++i) {
                    uint32_t tid = baseTid + static_cast<uint32_t>(i % 100);
                    
                    auto opStart = Clock::now();
                    internal::GetGlobalThreadModuleTable().Register(tid, name);
                    auto opEnd = Clock::now();

                    double latencyNs = static_cast<double>(
                        std::chrono::duration_cast<Duration>(opEnd - opStart).count());
                    collectors[t].AddSample(latencyNs);
                }
            });
        }

        // Start all threads
        // 启动所有线程
        startFlag.store(true, std::memory_order_release);

        // Wait for completion
        // 等待完成
        for (auto& thread : threads) {
            thread.join();
        }

        auto endTime = Clock::now();

        // Merge collectors and calculate results
        // 合并收集器并计算结果
        LatencyCollector merged;
        merged.Reserve(numOperations);
        for (auto& collector : collectors) {
            for (size_t i = 0; i < collector.Size(); ++i) {
                // Re-sort to get samples - this is a workaround
                // 重新排序以获取样本 - 这是一个变通方法
            }
        }

        // Calculate from first collector for simplicity (single-thread case)
        // 为简单起见，从第一个收集器计算（单线程情况）
        // For multi-thread, we merge all samples
        // 对于多线程，我们合并所有样本
        for (auto& collector : collectors) {
            collector.Sort();
        }

        // Use first collector's stats for now (TODO: proper merge)
        // 暂时使用第一个收集器的统计数据（TODO：正确合并）
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
     * @brief Benchmark name lookup throughput
     * @brief 测试名称查找吞吐量
     * @param numOperations Number of operations to perform / 要执行的操作数
     * @param numThreads Number of concurrent threads / 并发线程数
     * @return Benchmark results / 测试结果
     */
    static BenchmarkResult BenchmarkLookup(size_t numOperations, size_t numThreads = 1) {
        BenchmarkResult result;
        result.testName = "Lookup";
        result.numThreads = numThreads;
        result.totalOperations = numOperations;

        // Pre-register some entries
        // 预先注册一些条目
        internal::GetGlobalThreadModuleTable().Clear();
        for (uint32_t i = 0; i < 100; ++i) {
            internal::GetGlobalThreadModuleTable().Register(i, "module_" + std::to_string(i));
        }

        std::vector<LatencyCollector> collectors(numThreads);
        for (auto& collector : collectors) {
            collector.Reserve(numOperations / numThreads + 1);
        }

        std::atomic<bool> startFlag{false};
        std::vector<std::thread> threads;
        threads.reserve(numThreads);

        size_t opsPerThread = numOperations / numThreads;

        auto startTime = Clock::now();

        for (size_t t = 0; t < numThreads; ++t) {
            threads.emplace_back([&, t]() {
                while (!startFlag.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }

                for (size_t i = 0; i < opsPerThread; ++i) {
                    uint32_t tid = static_cast<uint32_t>(i % 100);
                    
                    auto opStart = Clock::now();
                    volatile auto name = internal::GetGlobalThreadModuleTable().GetName(tid);
                    (void)name;
                    auto opEnd = Clock::now();

                    double latencyNs = static_cast<double>(
                        std::chrono::duration_cast<Duration>(opEnd - opStart).count());
                    collectors[t].AddSample(latencyNs);
                }
            });
        }

        startFlag.store(true, std::memory_order_release);

        for (auto& thread : threads) {
            thread.join();
        }

        auto endTime = Clock::now();

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
     * @brief Benchmark full logging path (including name lookup)
     * @brief 测试完整日志路径（包含名称查找）
     * @param mode Operating mode (Sync/Async) / 运行模式
     * @param numOperations Number of log operations / 日志操作数
     * @param numThreads Number of concurrent threads / 并发线程数
     * @return Benchmark results / 测试结果
     */
    static BenchmarkResult BenchmarkLogging(Mode mode, size_t numOperations, size_t numThreads = 1) {
        BenchmarkResult result;
        result.testName = std::string("Logging_") + std::string(ModeToString(mode));
        result.numThreads = numThreads;
        result.totalOperations = numOperations;

        // Initialize NameManager
        // 初始化 NameManager
        NameManager<false>::Initialize(mode);

        std::vector<LatencyCollector> collectors(numThreads);
        for (auto& collector : collectors) {
            collector.Reserve(numOperations / numThreads + 1);
        }

        std::atomic<bool> startFlag{false};
        std::vector<std::thread> threads;
        threads.reserve(numThreads);

        size_t opsPerThread = numOperations / numThreads;

        auto startTime = Clock::now();

        for (size_t t = 0; t < numThreads; ++t) {
            threads.emplace_back([&, t]() {
                // Set module name for this thread
                // 为此线程设置模块名
                std::string moduleName = "worker_" + std::to_string(t);
                NameManager<false>::SetModuleName(moduleName);

                while (!startFlag.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }

                for (size_t i = 0; i < opsPerThread; ++i) {
                    auto opStart = Clock::now();
                    
                    // Simulate logging operation: get process name + module name
                    // 模拟日志操作：获取进程名 + 模块名
                    volatile auto procName = NameManager<false>::GetProcessName();
                    volatile auto modName = NameManager<false>::GetModuleName();
                    (void)procName;
                    (void)modName;
                    
                    auto opEnd = Clock::now();

                    double latencyNs = static_cast<double>(
                        std::chrono::duration_cast<Duration>(opEnd - opStart).count());
                    collectors[t].AddSample(latencyNs);
                }
            });
        }

        startFlag.store(true, std::memory_order_release);

        for (auto& thread : threads) {
            thread.join();
        }

        auto endTime = Clock::now();

        NameManager<false>::Shutdown();

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
     * @brief Print comparison report (before vs after optimization)
     * @brief 打印对比报告（优化前 vs 优化后）
     * @param before Results before optimization / 优化前结果
     * @param after Results after optimization / 优化后结果
     * @param testName Name of the test / 测试名称
     */
    static void PrintComparisonReport(const BenchmarkResult& before,
                                       const BenchmarkResult& after,
                                       const std::string& testName) {
        std::cout << "\n";
        std::cout << "========================================\n";
        std::cout << "Performance Comparison: " << testName << "\n";
        std::cout << "========================================\n";
        std::cout << std::fixed << std::setprecision(2);

        auto printRow = [](const std::string& metric, double beforeVal, double afterVal,
                          const std::string& unit, bool higherIsBetter) {
            double improvement = 0.0;
            if (beforeVal > 0) {
                if (higherIsBetter) {
                    improvement = ((afterVal - beforeVal) / beforeVal) * 100.0;
                } else {
                    improvement = ((beforeVal - afterVal) / beforeVal) * 100.0;
                }
            }
            
            std::cout << std::left << std::setw(20) << metric
                      << std::right << std::setw(15) << beforeVal
                      << std::setw(15) << afterVal
                      << std::setw(10) << unit;
            
            if (improvement > 0) {
                std::cout << "  +" << improvement << "%";
            } else if (improvement < 0) {
                std::cout << "  " << improvement << "%";
            }
            std::cout << "\n";
        };

        std::cout << std::left << std::setw(20) << "Metric"
                  << std::right << std::setw(15) << "Before"
                  << std::setw(15) << "After"
                  << std::setw(10) << "Unit"
                  << "  Change\n";
        std::cout << std::string(70, '-') << "\n";

        printRow("Throughput", before.throughputOpsPerSec, after.throughputOpsPerSec, "ops/s", true);
        printRow("Avg Latency", before.avgLatencyNs, after.avgLatencyNs, "ns", false);
        printRow("P50 Latency", before.p50LatencyNs, after.p50LatencyNs, "ns", false);
        printRow("P99 Latency", before.p99LatencyNs, after.p99LatencyNs, "ns", false);
        printRow("P999 Latency", before.p999LatencyNs, after.p999LatencyNs, "ns", false);
        printRow("Min Latency", before.minLatencyNs, after.minLatencyNs, "ns", false);
        printRow("Max Latency", before.maxLatencyNs, after.maxLatencyNs, "ns", false);

        std::cout << "========================================\n\n";
    }

    /**
     * @brief Print single benchmark result
     * @brief 打印单个测试结果
     * @param result Benchmark result to print / 要打印的测试结果
     */
    static void PrintResult(const BenchmarkResult& result) {
        std::cout << result.ToString();
    }

    /**
     * @brief Run a custom benchmark with the given function
     * @brief 使用给定函数运行自定义测试
     * @param testName Name of the test / 测试名称
     * @param numOperations Number of operations / 操作数
     * @param numThreads Number of threads / 线程数
     * @param operation Operation to benchmark / 要测试的操作
     * @return Benchmark results / 测试结果
     */
    static BenchmarkResult RunCustomBenchmark(
        const std::string& testName,
        size_t numOperations,
        size_t numThreads,
        std::function<void()> operation) {
        
        BenchmarkResult result;
        result.testName = testName;
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

        auto startTime = Clock::now();

        for (size_t t = 0; t < numThreads; ++t) {
            threads.emplace_back([&, t]() {
                while (!startFlag.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }

                for (size_t i = 0; i < opsPerThread; ++i) {
                    auto opStart = Clock::now();
                    operation();
                    auto opEnd = Clock::now();

                    double latencyNs = static_cast<double>(
                        std::chrono::duration_cast<Duration>(opEnd - opStart).count());
                    collectors[t].AddSample(latencyNs);
                }
            });
        }

        startFlag.store(true, std::memory_order_release);

        for (auto& thread : threads) {
            thread.join();
        }

        auto endTime = Clock::now();

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
};

}  // namespace oneplog::benchmark
