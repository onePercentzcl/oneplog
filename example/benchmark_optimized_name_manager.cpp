/**
 * @file benchmark_optimized_name_manager.cpp
 * @brief Performance comparison: OptimizedNameManager vs NameManager
 * @brief 性能对比：OptimizedNameManager vs NameManager
 *
 * This benchmark compares the performance of the new OptimizedNameManager
 * against the original NameManager implementation.
 *
 * 此测试对比新的 OptimizedNameManager 与原 NameManager 实现的性能。
 *
 * Requirements: 8.3
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include "oneplog/benchmark/latency_collector.hpp"
#include "oneplog/benchmark/performance_benchmark.hpp"
#include "oneplog/name_manager.hpp"
#include "oneplog/internal/optimized_name_manager.hpp"
#include "oneplog/oneplog.hpp"

#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <algorithm>
#include <numeric>
#include <cmath>

#if defined(__unix__) || defined(__APPLE__)
#define ONEPLOG_HAS_FORK 1
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#endif

using namespace oneplog;
using namespace oneplog::benchmark;

using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::nanoseconds;

// Type aliases for the two implementations
using OriginalNM = NameManager<false>;
using OptimizedNM = OptimizedNameManager<false, 31>;

// ==============================================================================
// Statistics / 统计数据
// ==============================================================================

struct Statistics {
    double mean = 0;
    double median = 0;
    double stddev = 0;
    double min = 0;
    double max = 0;
    double p50 = 0;
    double p90 = 0;
    double p99 = 0;
    double p999 = 0;
    double throughput = 0;  // ops/sec
};

Statistics CalculateStats(std::vector<int64_t>& latencies, double totalTimeMs) {
    Statistics stats;
    if (latencies.empty()) return stats;

    std::sort(latencies.begin(), latencies.end());

    size_t n = latencies.size();
    
    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    stats.mean = sum / n;
    stats.median = latencies[n / 2];

    double sqSum = 0;
    for (auto v : latencies) {
        sqSum += (v - stats.mean) * (v - stats.mean);
    }
    stats.stddev = std::sqrt(sqSum / n);

    stats.min = latencies.front();
    stats.max = latencies.back();
    stats.p50 = latencies[static_cast<size_t>(n * 0.50)];
    stats.p90 = latencies[static_cast<size_t>(n * 0.90)];
    stats.p99 = latencies[static_cast<size_t>(n * 0.99)];
    stats.p999 = latencies[static_cast<size_t>(n * 0.999)];
    stats.throughput = (n / totalTimeMs) * 1000.0;

    return stats;
}

// ==============================================================================
// Null Sink (for pure logging overhead measurement)
// 空输出（用于测量纯日志开销）
// ==============================================================================

class NullSink : public Sink {
public:
    void Write(const std::string&) override { m_count++; }
    void WriteBatch(const std::vector<std::string>& messages) override { 
        m_count += messages.size(); 
    }
    void Flush() override {}
    void Close() override {}
    bool HasError() const override { return false; }
    std::string GetLastError() const override { return ""; }
    size_t GetCount() const { return m_count.load(); }
    void Reset() { m_count = 0; }
private:
    std::atomic<size_t> m_count{0};
};

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
 * @brief Benchmark original NameManager logging
 * @brief 测试原 NameManager 日志性能
 */
BenchmarkResult BenchmarkOriginalLogging(Mode mode, size_t numOperations, size_t numThreads) {
    BenchmarkResult result;
    result.testName = std::string("Original_") + std::string(ModeToString(mode));
    result.numThreads = numThreads;
    result.totalOperations = numOperations;

    // Initialize original NameManager
    OriginalNM::Initialize(mode);

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
            std::string moduleName = "worker_" + std::to_string(t);
            OriginalNM::SetModuleName(moduleName);

            while (!startFlag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (size_t i = 0; i < opsPerThread; ++i) {
                auto opStart = Clock::now();
                
                volatile auto procName = OriginalNM::GetProcessName();
                volatile auto modName = OriginalNM::GetModuleName();
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

    OriginalNM::Shutdown();

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
 * @brief Benchmark optimized NameManager logging
 * @brief 测试优化后 NameManager 日志性能
 */
BenchmarkResult BenchmarkOptimizedLogging(Mode mode, size_t numOperations, size_t numThreads) {
    BenchmarkResult result;
    result.testName = std::string("Optimized_") + std::string(ModeToString(mode));
    result.numThreads = numThreads;
    result.totalOperations = numOperations;

    // Initialize optimized NameManager
    OptimizedNM::Initialize(mode);
    OptimizedNM::ClearLookupTable();

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
            std::string moduleName = "worker_" + std::to_string(t);
            OptimizedNM::SetModuleName(moduleName);

            while (!startFlag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (size_t i = 0; i < opsPerThread; ++i) {
                auto opStart = Clock::now();
                
                volatile auto procName = OptimizedNM::GetProcessName();
                volatile auto modName = OptimizedNM::GetModuleName();
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

    OptimizedNM::Shutdown();

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
 * @brief Benchmark original lookup table
 * @brief 测试原查找表性能
 */
BenchmarkResult BenchmarkOriginalLookup(size_t numOperations, size_t numThreads) {
    BenchmarkResult result;
    result.testName = "Original_Lookup";
    result.numThreads = numThreads;
    result.totalOperations = numOperations;

    // Pre-register entries
    detail::GetGlobalThreadModuleTable().Clear();
    for (uint32_t i = 0; i < 100; ++i) {
        detail::GetGlobalThreadModuleTable().Register(i, "module_" + std::to_string(i));
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
                volatile auto name = detail::GetGlobalThreadModuleTable().GetName(tid);
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
 * @brief Benchmark optimized lookup table (platform-specific)
 * @brief 测试优化后查找表性能（平台特定）
 */
BenchmarkResult BenchmarkOptimizedLookup(size_t numOperations, size_t numThreads) {
    BenchmarkResult result;
    result.testName = "Optimized_Lookup";
    result.numThreads = numThreads;
    result.totalOperations = numOperations;

    // Initialize and pre-register entries
    OptimizedNM::Initialize(Mode::Async);
    OptimizedNM::ClearLookupTable();
    
    // Use the internal lookup table directly for fair comparison
    using LookupTable = typename internal::PlatformLookupTableSelector<31>::Type;
    static LookupTable lookupTable;
    lookupTable.Clear();
    
    for (uint32_t i = 0; i < 100; ++i) {
        lookupTable.Register(i, "module_" + std::to_string(i));
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
                volatile auto name = lookupTable.GetName(tid);
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

    OptimizedNM::Shutdown();

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
    constexpr size_t kNumOperations = 100000;

    PrintHeader("Lookup Table Comparison / 查找表对比");
    
    std::cout << "\nPlatform: " << internal::GetPlatformName() << "\n";
    std::cout << "Lookup Table Type: " << internal::GetLookupTableTypeName() << "\n";
    std::cout << "Lookup Complexity: " << internal::GetLookupComplexity() << "\n\n";

    // Single-thread lookup comparison
    std::cout << "[1] Single-Thread Lookup / 单线程查找\n";
    auto origLookup = BenchmarkOriginalLookup(kNumOperations, 1);
    auto optLookup = BenchmarkOptimizedLookup(kNumOperations, 1);
    PerformanceBenchmark::PrintComparisonReport(origLookup, optLookup, "Single-Thread Lookup");

    // Multi-thread lookup comparison
    std::cout << "[2] Multi-Thread Lookup (4 threads) / 多线程查找\n";
    auto origLookupMT = BenchmarkOriginalLookup(kNumOperations, 4);
    auto optLookupMT = BenchmarkOptimizedLookup(kNumOperations, 4);
    PerformanceBenchmark::PrintComparisonReport(origLookupMT, optLookupMT, "Multi-Thread Lookup (4 threads)");

    PrintHeader("Logging Path Comparison / 日志路径对比");

    // Sync mode comparison
    std::cout << "[3] Sync Mode Logging / 同步模式日志\n";
    auto origSync = BenchmarkOriginalLogging(Mode::Sync, kNumOperations, 1);
    auto optSync = BenchmarkOptimizedLogging(Mode::Sync, kNumOperations, 1);
    PerformanceBenchmark::PrintComparisonReport(origSync, optSync, "Sync Mode Logging");

    // Async mode comparison
    std::cout << "[4] Async Mode Logging / 异步模式日志\n";
    auto origAsync = BenchmarkOriginalLogging(Mode::Async, kNumOperations, 1);
    auto optAsync = BenchmarkOptimizedLogging(Mode::Async, kNumOperations, 1);
    PerformanceBenchmark::PrintComparisonReport(origAsync, optAsync, "Async Mode Logging");

    // Multi-thread async comparison
    std::cout << "[5] Multi-Thread Async Logging (4 threads) / 多线程异步日志\n";
    auto origAsyncMT = BenchmarkOriginalLogging(Mode::Async, kNumOperations, 4);
    auto optAsyncMT = BenchmarkOptimizedLogging(Mode::Async, kNumOperations, 4);
    PerformanceBenchmark::PrintComparisonReport(origAsyncMT, optAsyncMT, "Multi-Thread Async Logging (4 threads)");
}

/**
 * @brief Print summary of improvements
 * @brief 打印改进摘要
 */
void PrintSummary() {
    PrintHeader("Performance Summary / 性能摘要");

    constexpr size_t kNumOperations = 100000;

    std::cout << "\nKey Metrics Comparison:\n";
    std::cout << "关键指标对比:\n\n";

    // Single-thread lookup (most important for async mode)
    auto origLookup = BenchmarkOriginalLookup(kNumOperations, 1);
    auto optLookup = BenchmarkOptimizedLookup(kNumOperations, 1);

    double lookupImprovement = 0.0;
    if (origLookup.avgLatencyNs > 0) {
        lookupImprovement = ((origLookup.avgLatencyNs - optLookup.avgLatencyNs) / origLookup.avgLatencyNs) * 100.0;
    }

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Lookup Latency Improvement: " << lookupImprovement << "%\n";
    std::cout << "  Original: " << origLookup.avgLatencyNs << " ns\n";
    std::cout << "  Optimized: " << optLookup.avgLatencyNs << " ns\n\n";

    // Sync mode logging
    auto origSync = BenchmarkOriginalLogging(Mode::Sync, kNumOperations, 1);
    auto optSync = BenchmarkOptimizedLogging(Mode::Sync, kNumOperations, 1);

    double syncImprovement = 0.0;
    if (origSync.avgLatencyNs > 0) {
        syncImprovement = ((origSync.avgLatencyNs - optSync.avgLatencyNs) / origSync.avgLatencyNs) * 100.0;
    }

    std::cout << "Sync Mode Logging Improvement: " << syncImprovement << "%\n";
    std::cout << "  Original: " << origSync.avgLatencyNs << " ns\n";
    std::cout << "  Optimized: " << optSync.avgLatencyNs << " ns\n\n";

    std::cout << "Platform: " << internal::GetPlatformName() << "\n";
    std::cout << "Lookup Table: " << internal::GetLookupTableTypeName() << "\n";
    std::cout << "Complexity: " << internal::GetLookupComplexity() << "\n";
}

// ==============================================================================
// Full Logging Path Benchmarks / 完整日志路径测试
// ==============================================================================

/**
 * @brief Benchmark full logging with original NameManager (Sync mode)
 * @brief 使用原 NameManager 的完整日志测试（同步模式）
 */
Statistics BenchmarkFullLoggingOriginalSync(size_t numOperations, size_t numThreads) {
    auto nullSink = std::make_shared<NullSink>();
    Logger<Mode::Sync, Level::Info, false> logger;
    logger.SetSink(nullSink);
    logger.Init();

    // 使用原 NameManager
    OriginalNM::Initialize(Mode::Sync);
    OriginalNM::SetProcessName("benchmark");

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
            std::string moduleName = "worker_" + std::to_string(t);
            OriginalNM::SetModuleName(moduleName);

            while (!startFlag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (size_t i = 0; i < opsPerThread; ++i) {
                auto opStart = Clock::now();
                logger.Info("Test message {} from thread {} value {}", static_cast<int>(i), static_cast<int>(t), 3.14159);
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

    logger.Flush();
    OriginalNM::Shutdown();

    // 计算统计数据
    std::vector<int64_t> allLatencies;
    for (auto& collector : collectors) {
        collector.Sort();
        // 简化：只使用第一个线程的数据
    }

    double totalTimeMs = static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count()) / 1000.0;

    Statistics stats;
    if (!collectors.empty() && collectors[0].Size() > 0) {
        stats.mean = collectors[0].GetAverage();
        stats.p50 = collectors[0].GetP50();
        stats.p99 = collectors[0].GetP99();
        stats.p999 = collectors[0].GetP999();
        stats.min = collectors[0].GetMin();
        stats.max = collectors[0].GetMax();
    }
    stats.throughput = (numOperations / totalTimeMs) * 1000.0;

    return stats;
}

/**
 * @brief Benchmark full logging with OptimizedNameManager (Sync mode)
 * @brief 使用 OptimizedNameManager 的完整日志测试（同步模式）
 */
Statistics BenchmarkFullLoggingOptimizedSync(size_t numOperations, size_t numThreads) {
    auto nullSink = std::make_shared<NullSink>();
    Logger<Mode::Sync, Level::Info, false> logger;
    logger.SetSink(nullSink);
    logger.Init();

    // 使用优化后的 NameManager
    OptimizedNM::Initialize(Mode::Sync);
    OptimizedNM::SetProcessName("benchmark");
    OptimizedNM::ClearLookupTable();

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
            std::string moduleName = "worker_" + std::to_string(t);
            OptimizedNM::SetModuleName(moduleName);

            while (!startFlag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (size_t i = 0; i < opsPerThread; ++i) {
                auto opStart = Clock::now();
                logger.Info("Test message {} from thread {} value {}", static_cast<int>(i), static_cast<int>(t), 3.14159);
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

    logger.Flush();
    OptimizedNM::Shutdown();

    double totalTimeMs = static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count()) / 1000.0;

    Statistics stats;
    if (!collectors.empty() && collectors[0].Size() > 0) {
        stats.mean = collectors[0].GetAverage();
        stats.p50 = collectors[0].GetP50();
        stats.p99 = collectors[0].GetP99();
        stats.p999 = collectors[0].GetP999();
        stats.min = collectors[0].GetMin();
        stats.max = collectors[0].GetMax();
    }
    stats.throughput = (numOperations / totalTimeMs) * 1000.0;

    return stats;
}

/**
 * @brief Benchmark full logging with original NameManager (Async mode)
 * @brief 使用原 NameManager 的完整日志测试（异步模式）
 */
Statistics BenchmarkFullLoggingOriginalAsync(size_t numOperations, size_t numThreads) {
    auto nullSink = std::make_shared<NullSink>();
    Logger<Mode::Async, Level::Info, false> logger;
    logger.SetSink(nullSink);
    
    LoggerConfig logConfig;
    logConfig.heapRingBufferSize = 1024 * 1024;
    logger.Init(logConfig);

    OriginalNM::Initialize(Mode::Async);
    OriginalNM::SetProcessName("benchmark");

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
            std::string moduleName = "worker_" + std::to_string(t);
            OriginalNM::SetModuleName(moduleName);

            while (!startFlag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (size_t i = 0; i < opsPerThread; ++i) {
                auto opStart = Clock::now();
                logger.Info("Test message {} from thread {} value {}", static_cast<int>(i), static_cast<int>(t), 3.14159);
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

    logger.Flush();
    logger.Shutdown();
    OriginalNM::Shutdown();

    double totalTimeMs = static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count()) / 1000.0;

    Statistics stats;
    if (!collectors.empty() && collectors[0].Size() > 0) {
        stats.mean = collectors[0].GetAverage();
        stats.p50 = collectors[0].GetP50();
        stats.p99 = collectors[0].GetP99();
        stats.p999 = collectors[0].GetP999();
        stats.min = collectors[0].GetMin();
        stats.max = collectors[0].GetMax();
    }
    stats.throughput = (numOperations / totalTimeMs) * 1000.0;

    return stats;
}

/**
 * @brief Benchmark full logging with OptimizedNameManager (Async mode)
 * @brief 使用 OptimizedNameManager 的完整日志测试（异步模式）
 */
Statistics BenchmarkFullLoggingOptimizedAsync(size_t numOperations, size_t numThreads) {
    auto nullSink = std::make_shared<NullSink>();
    Logger<Mode::Async, Level::Info, false> logger;
    logger.SetSink(nullSink);
    
    LoggerConfig logConfig;
    logConfig.heapRingBufferSize = 1024 * 1024;
    logger.Init(logConfig);

    OptimizedNM::Initialize(Mode::Async);
    OptimizedNM::SetProcessName("benchmark");
    OptimizedNM::ClearLookupTable();

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
            std::string moduleName = "worker_" + std::to_string(t);
            OptimizedNM::SetModuleName(moduleName);

            while (!startFlag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }

            for (size_t i = 0; i < opsPerThread; ++i) {
                auto opStart = Clock::now();
                logger.Info("Test message {} from thread {} value {}", static_cast<int>(i), static_cast<int>(t), 3.14159);
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

    logger.Flush();
    logger.Shutdown();
    OptimizedNM::Shutdown();

    double totalTimeMs = static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count()) / 1000.0;

    Statistics stats;
    if (!collectors.empty() && collectors[0].Size() > 0) {
        stats.mean = collectors[0].GetAverage();
        stats.p50 = collectors[0].GetP50();
        stats.p99 = collectors[0].GetP99();
        stats.p999 = collectors[0].GetP999();
        stats.min = collectors[0].GetMin();
        stats.max = collectors[0].GetMax();
    }
    stats.throughput = (numOperations / totalTimeMs) * 1000.0;

    return stats;
}

/**
 * @brief Print statistics comparison
 * @brief 打印统计对比
 */
void PrintStatsComparison(const std::string& testName, const Statistics& before, const Statistics& after) {
    std::cout << "\n";
    std::cout << "========================================\n";
    std::cout << testName << "\n";
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

    printRow("Throughput", before.throughput, after.throughput, "ops/s", true);
    printRow("Avg Latency", before.mean, after.mean, "ns", false);
    printRow("P50 Latency", before.p50, after.p50, "ns", false);
    printRow("P99 Latency", before.p99, after.p99, "ns", false);
    printRow("P999 Latency", before.p999, after.p999, "ns", false);
    printRow("Min Latency", before.min, after.min, "ns", false);
    printRow("Max Latency", before.max, after.max, "ns", false);

    std::cout << "========================================\n";
}

/**
 * @brief Run full logging path benchmarks
 * @brief 运行完整日志路径测试
 */
void RunFullLoggingBenchmarks() {
    constexpr size_t kNumOperations = 100000;

    PrintHeader("Full Logging Path Comparison / 完整日志路径对比");

    std::cout << "\n说明：此测试测量完整的日志调用路径，包括：\n";
    std::cout << "  - 名称查找（进程名、模块名）\n";
    std::cout << "  - 日志格式化\n";
    std::cout << "  - 写入 Sink\n\n";

    // 同步模式单线程
    std::cout << "[1] Sync Mode (1 Thread) / 同步模式（单线程）\n";
    auto origSyncST = BenchmarkFullLoggingOriginalSync(kNumOperations, 1);
    auto optSyncST = BenchmarkFullLoggingOptimizedSync(kNumOperations, 1);
    PrintStatsComparison("Sync Mode (1 Thread) / 同步模式（单线程）", origSyncST, optSyncST);

    // 同步模式多线程
    std::cout << "[2] Sync Mode (4 Threads) / 同步模式（4线程）\n";
    auto origSyncMT = BenchmarkFullLoggingOriginalSync(kNumOperations, 4);
    auto optSyncMT = BenchmarkFullLoggingOptimizedSync(kNumOperations, 4);
    PrintStatsComparison("Sync Mode (4 Threads) / 同步模式（4线程）", origSyncMT, optSyncMT);

    // 异步模式单线程
    std::cout << "[3] Async Mode (1 Thread) / 异步模式（单线程）\n";
    auto origAsyncST = BenchmarkFullLoggingOriginalAsync(kNumOperations, 1);
    auto optAsyncST = BenchmarkFullLoggingOptimizedAsync(kNumOperations, 1);
    PrintStatsComparison("Async Mode (1 Thread) / 异步模式（单线程）", origAsyncST, optAsyncST);

    // 异步模式多线程
    std::cout << "[4] Async Mode (4 Threads) / 异步模式（4线程）\n";
    auto origAsyncMT = BenchmarkFullLoggingOriginalAsync(kNumOperations, 4);
    auto optAsyncMT = BenchmarkFullLoggingOptimizedAsync(kNumOperations, 4);
    PrintStatsComparison("Async Mode (4 Threads) / 异步模式（4线程）", origAsyncMT, optAsyncMT);
}

#ifdef ONEPLOG_HAS_FORK
/**
 * @brief Run multi-process benchmarks
 * @brief 运行多进程测试
 */
void RunMultiProcessBenchmarks() {
    PrintHeader("Multi-Process Mode Comparison / 多进程模式对比");

    std::cout << "\n说明：此测试测量多进程场景下的日志性能\n";
    std::cout << "  - 每个进程独立运行日志\n";
    std::cout << "  - 测量进程间的性能差异\n\n";

    constexpr size_t kNumOperations = 50000;
    constexpr int kNumProcesses = 4;

    // 原实现多进程测试
    std::cout << "[1] Original NameManager Multi-Process / 原 NameManager 多进程\n";
    
    std::vector<pid_t> childPids;
    auto startOrig = Clock::now();
    
    for (int p = 0; p < kNumProcesses; ++p) {
        pid_t pid = fork();
        
        if (pid < 0) {
            std::cerr << "Fork failed!" << std::endl;
            break;
        } else if (pid == 0) {
            // 子进程
            auto nullSink = std::make_shared<NullSink>();
            Logger<Mode::Sync, Level::Info, false> logger;
            logger.SetSink(nullSink);
            logger.Init();
            
            OriginalNM::Initialize(Mode::Sync);
            OriginalNM::SetProcessName("process_" + std::to_string(p));
            OriginalNM::SetModuleName("worker");
            
            for (size_t i = 0; i < kNumOperations; ++i) {
                logger.Info("Process {} message {} value {}", p, i, 3.14159);
            }
            
            logger.Flush();
            _exit(0);
        } else {
            childPids.push_back(pid);
        }
    }
    
    for (pid_t pid : childPids) {
        int status;
        waitpid(pid, &status, 0);
    }
    
    auto endOrig = Clock::now();
    double origTimeMs = static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(endOrig - startOrig).count()) / 1000.0;
    double origThroughput = (kNumOperations * kNumProcesses / origTimeMs) * 1000.0;
    
    std::cout << "  Original throughput: " << std::fixed << std::setprecision(0) 
              << origThroughput << " ops/sec\n";
    std::cout << "  Total time: " << std::setprecision(2) << origTimeMs << " ms\n\n";

    // 优化后实现多进程测试
    std::cout << "[2] Optimized NameManager Multi-Process / 优化后 NameManager 多进程\n";
    
    childPids.clear();
    auto startOpt = Clock::now();
    
    for (int p = 0; p < kNumProcesses; ++p) {
        pid_t pid = fork();
        
        if (pid < 0) {
            std::cerr << "Fork failed!" << std::endl;
            break;
        } else if (pid == 0) {
            // 子进程
            auto nullSink = std::make_shared<NullSink>();
            Logger<Mode::Sync, Level::Info, false> logger;
            logger.SetSink(nullSink);
            logger.Init();
            
            OptimizedNM::Initialize(Mode::Sync);
            OptimizedNM::SetProcessName("process_" + std::to_string(p));
            OptimizedNM::SetModuleName("worker");
            
            for (size_t i = 0; i < kNumOperations; ++i) {
                logger.Info("Process {} message {} value {}", p, i, 3.14159);
            }
            
            logger.Flush();
            _exit(0);
        } else {
            childPids.push_back(pid);
        }
    }
    
    for (pid_t pid : childPids) {
        int status;
        waitpid(pid, &status, 0);
    }
    
    auto endOpt = Clock::now();
    double optTimeMs = static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(endOpt - startOpt).count()) / 1000.0;
    double optThroughput = (kNumOperations * kNumProcesses / optTimeMs) * 1000.0;
    
    std::cout << "  Optimized throughput: " << std::fixed << std::setprecision(0) 
              << optThroughput << " ops/sec\n";
    std::cout << "  Total time: " << std::setprecision(2) << optTimeMs << " ms\n\n";

    // 对比结果
    double improvement = 0.0;
    if (origThroughput > 0) {
        improvement = ((optThroughput - origThroughput) / origThroughput) * 100.0;
    }
    
    std::cout << "========================================\n";
    std::cout << "Multi-Process Summary / 多进程摘要\n";
    std::cout << "========================================\n";
    std::cout << "  Processes: " << kNumProcesses << "\n";
    std::cout << "  Operations per process: " << kNumOperations << "\n";
    std::cout << "  Original throughput: " << std::fixed << std::setprecision(0) 
              << origThroughput << " ops/sec\n";
    std::cout << "  Optimized throughput: " << optThroughput << " ops/sec\n";
    std::cout << "  Improvement: " << std::setprecision(2);
    if (improvement > 0) {
        std::cout << "+" << improvement << "%\n";
    } else {
        std::cout << improvement << "%\n";
    }
    std::cout << "========================================\n";
}
#endif

int main(int argc, char* argv[]) {
    std::cout << "========================================\n";
    std::cout << "OptimizedNameManager Performance Benchmark\n";
    std::cout << "OptimizedNameManager 性能测试\n";
    std::cout << "========================================\n";

    std::string mode = (argc > 1) ? argv[1] : "all";

    if (mode == "compare" || mode == "all") {
        RunComparisonBenchmarks();
    }

    if (mode == "logging" || mode == "all") {
        RunFullLoggingBenchmarks();
    }

#ifdef ONEPLOG_HAS_FORK
    if (mode == "mproc" || mode == "all") {
        RunMultiProcessBenchmarks();
    }
#endif

    if (mode == "summary" || mode == "all") {
        PrintSummary();
    }

    std::cout << "\n========================================\n";
    std::cout << "Benchmark Complete / 测试完成\n";
    std::cout << "========================================\n";

    return 0;
}
