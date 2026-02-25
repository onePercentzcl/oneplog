/**
 * @file benchmark_utils.hpp
 * @brief Performance benchmark utilities for oneplog
 * @brief oneplog 性能测试工具集
 *
 * This file consolidates benchmark utilities including latency collection
 * and performance measurement tools.
 *
 * 此文件整合了基准测试工具，包括延迟收集和性能测量工具。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <functional>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace oneplog::benchmark {

// ==============================================================================
// LatencyCollector - Latency sample collector with percentile calculation
// LatencyCollector - 支持百分位数计算的延迟样本收集器
// ==============================================================================

/**
 * @brief Latency sample collector with percentile calculation
 * @brief 支持百分位数计算的延迟样本收集器
 *
 * Collects latency samples and provides statistical analysis including
 * average, percentiles (P50, P99, P999), min, max, and standard deviation.
 *
 * 收集延迟样本并提供统计分析，包括平均值、百分位数（P50、P99、P999）、
 * 最小值、最大值和标准差。
 */
class LatencyCollector {
public:
    /**
     * @brief Construct a latency collector with optional pre-allocation
     * @brief 构造延迟收集器，可选预分配
     * @param reserveSize Number of samples to pre-allocate / 预分配的样本数
     */
    explicit LatencyCollector(size_t reserveSize = 10000) {
        m_samples.reserve(reserveSize);
    }

    /**
     * @brief Add a latency sample
     * @brief 添加延迟样本
     * @param latencyNs Latency in nanoseconds / 延迟（纳秒）
     */
    void AddSample(double latencyNs) {
        m_samples.push_back(latencyNs);
        m_sorted = false;
    }

    /**
     * @brief Sort samples for percentile calculation
     * @brief 排序样本以计算百分位数
     */
    void Sort() {
        if (!m_sorted && !m_samples.empty()) {
            std::sort(m_samples.begin(), m_samples.end());
            m_sorted = true;
        }
    }

    /**
     * @brief Get percentile value
     * @brief 获取百分位数值
     * @param p Percentile (0.0 to 1.0, e.g., 0.99 for P99) / 百分位数（0.0 到 1.0）
     * @return Latency at the specified percentile / 指定百分位数的延迟
     */
    double GetPercentile(double p) {
        if (m_samples.empty()) return 0.0;
        Sort();
        size_t idx = static_cast<size_t>(std::ceil(p * static_cast<double>(m_samples.size()))) - 1;
        if (idx >= m_samples.size()) idx = m_samples.size() - 1;
        return m_samples[idx];
    }

    double GetP50() { return GetPercentile(0.50); }
    double GetP99() { return GetPercentile(0.99); }
    double GetP999() { return GetPercentile(0.999); }

    double GetAverage() const {
        if (m_samples.empty()) return 0.0;
        return std::accumulate(m_samples.begin(), m_samples.end(), 0.0) / 
               static_cast<double>(m_samples.size());
    }

    double GetMin() const {
        if (m_samples.empty()) return 0.0;
        return *std::min_element(m_samples.begin(), m_samples.end());
    }

    double GetMax() const {
        if (m_samples.empty()) return 0.0;
        return *std::max_element(m_samples.begin(), m_samples.end());
    }

    double GetStdDev() const {
        if (m_samples.size() < 2) return 0.0;
        double avg = GetAverage();
        double sumSquaredDiff = 0.0;
        for (double sample : m_samples) {
            double diff = sample - avg;
            sumSquaredDiff += diff * diff;
        }
        return std::sqrt(sumSquaredDiff / static_cast<double>(m_samples.size() - 1));
    }

    size_t Size() const { return m_samples.size(); }
    bool Empty() const { return m_samples.empty(); }
    void Clear() { m_samples.clear(); m_sorted = false; }
    void Reserve(size_t capacity) { m_samples.reserve(capacity); }

private:
    std::vector<double> m_samples;
    bool m_sorted{false};
};

// ==============================================================================
// BenchmarkResult - Performance benchmark results
// BenchmarkResult - 性能测试结果
// ==============================================================================

/**
 * @brief Performance benchmark results
 * @brief 性能测试结果
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

// ==============================================================================
// BenchmarkRunner - Generic benchmark runner
// BenchmarkRunner - 通用基准测试运行器
// ==============================================================================

/**
 * @brief Generic benchmark runner for performance testing
 * @brief 用于性能测试的通用基准测试运行器
 */
class BenchmarkRunner {
public:
    using Clock = std::chrono::high_resolution_clock;
    using Duration = std::chrono::nanoseconds;

    /**
     * @brief Run a custom benchmark with the given function
     * @brief 使用给定函数运行自定义测试
     */
    static BenchmarkResult Run(
        const std::string& testName,
        size_t numOperations,
        size_t numThreads,
        std::function<void()> operation,
        std::function<void(size_t)> threadSetup = nullptr) {
        
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
                if (threadSetup) threadSetup(t);
                
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

        // Merge results from all collectors
        // 合并所有收集器的结果
        LatencyCollector merged;
        merged.Reserve(numOperations);
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
     * @brief Print benchmark result
     * @brief 打印测试结果
     */
    static void PrintResult(const BenchmarkResult& result) {
        std::cout << result.ToString();
    }

    /**
     * @brief Print comparison report (before vs after)
     * @brief 打印对比报告（优化前 vs 优化后）
     */
    static void PrintComparison(const BenchmarkResult& before,
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
                improvement = higherIsBetter 
                    ? ((afterVal - beforeVal) / beforeVal) * 100.0
                    : ((beforeVal - afterVal) / beforeVal) * 100.0;
            }
            
            std::cout << std::left << std::setw(20) << metric
                      << std::right << std::setw(15) << beforeVal
                      << std::setw(15) << afterVal
                      << std::setw(10) << unit;
            
            if (improvement > 0) std::cout << "  +" << improvement << "%";
            else if (improvement < 0) std::cout << "  " << improvement << "%";
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

        std::cout << "========================================\n\n";
    }
};

}  // namespace oneplog::benchmark
