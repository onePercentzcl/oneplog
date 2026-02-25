/**
 * @file benchmark_mproc.cpp
 * @brief Multi-process mode performance benchmark for onePlog
 * @brief onePlog 多进程模式性能测试
 *
 * This benchmark measures the performance of onePlog's multi-process logging mode,
 * testing cross-process communication via shared memory.
 *
 * 此基准测试测量 onePlog 多进程日志模式的性能，测试通过共享内存的跨进程通信。
 *
 * Test scenarios / 测试场景:
 * - Single producer process: One process producing logs
 *   单生产者进程: 一个进程产生日志
 * - Multi producer process: Multiple processes producing logs (simulated with threads)
 *   多生产者进程: 多个进程产生日志（使用线程模拟）
 * - SharedRingBuffer read/write performance
 *   SharedRingBuffer 读写性能
 * - Different SharedRingBuffer capacities
 *   不同 SharedRingBuffer 容量
 *
 * Metrics / 指标:
 * - Throughput (ops/sec): Operations per second
 *   吞吐量 (ops/sec): 每秒操作数
 * - Latency: Average, P50, P99, P999 percentiles
 *   延迟: 平均值、P50、P99、P999 百分位数
 * - Cross-process communication latency distribution
 *   跨进程通信延迟分布
 *
 * @note This benchmark simulates multi-process behavior using threads within
 *       a single process for simplicity. For true multi-process testing,
 *       run multiple instances of this program.
 * @note 为简化起见，此基准测试使用单进程内的线程模拟多进程行为。
 *       对于真正的多进程测试，请运行此程序的多个实例。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#include "benchmark_utils.hpp"
#include <oneplog/oneplog.hpp>

namespace {

// ==============================================================================
// Configuration / 配置
// ==============================================================================

/// Default number of iterations / 默认迭代次数
constexpr size_t kDefaultIterations = 100000;

/// Default warmup iterations / 默认预热次数
constexpr size_t kDefaultWarmupIterations = 10000;

/// Default number of producer threads (simulating processes) / 默认生产者线程数（模拟进程）
constexpr size_t kDefaultNumProducers = 4;

/// Default SharedRingBuffer capacity / 默认 SharedRingBuffer 容量
constexpr size_t kDefaultSharedBufferCapacity = 4096;

/// Test message for benchmarking / 用于基准测试的测试消息
constexpr const char* kTestMessage = "MProc benchmark test: int={}, double={:.6f}, str={}";

/// Shared memory name for benchmarking / 基准测试的共享内存名称
constexpr const char* kBenchmarkSharedMemoryName = "oneplog_benchmark_mproc";

// ==============================================================================
// Benchmark Configuration Structure / 基准测试配置结构
// ==============================================================================

/**
 * @brief Benchmark configuration
 * @brief 基准测试配置
 */
struct BenchmarkConfig {
    size_t iterations{kDefaultIterations};       ///< Number of iterations / 迭代次数
    size_t warmupIterations{kDefaultWarmupIterations}; ///< Warmup iterations / 预热次数
    size_t numProducers{kDefaultNumProducers};   ///< Number of producer threads / 生产者线程数
    size_t sharedBufferCapacity{kDefaultSharedBufferCapacity}; ///< SharedRingBuffer capacity
    bool runSingleProducer{true};                ///< Run single producer benchmark / 运行单生产者测试
    bool runMultiProducer{true};                 ///< Run multi producer benchmark / 运行多生产者测试
    bool runCapacityTest{true};                  ///< Run capacity test / 运行容量测试
    bool runLatencyDistribution{true};           ///< Run latency distribution test / 运行延迟分布测试
    bool verbose{false};                         ///< Verbose output / 详细输出
};

// ==============================================================================
// Custom Shared Memory Name for Benchmarking / 基准测试的自定义共享内存名称
// ==============================================================================

/**
 * @brief Custom shared memory name for benchmarking
 * @brief 基准测试的自定义共享内存名称
 */
struct BenchmarkSharedMemoryName {
    static constexpr const char* value = kBenchmarkSharedMemoryName;
};

// ==============================================================================
// onePlog MProc Mode Benchmarks / onePlog 多进程模式基准测试
// ==============================================================================

/**
 * @brief Benchmark onePlog MProc mode with single producer
 * @brief 使用单生产者测试 onePlog 多进程模式
 *
 * This test measures the throughput and latency of logging in MProc mode
 * with a single producer thread.
 *
 * 此测试测量单生产者线程在 MProc 模式下的日志吞吐量和延迟。
 */
oneplog::benchmark::BenchmarkResult BenchmarkOneplogMProcSingleProducer(
    size_t iterations, size_t warmupIterations, size_t sharedBufferCapacity) {
    
    // Configure logger with MProc mode and NullSink
    // 配置使用 MProc 模式和 NullSink 的日志器
    using NullSinkBinding = oneplog::SinkBinding<oneplog::NullSinkType, oneplog::MessageOnlyFormat>;
    using MProcConfig = oneplog::LoggerConfig<
        oneplog::Mode::MProc,
        oneplog::Level::Info,
        false,  // EnableWFC
        true,   // EnableShadowTail
        true,   // UseFmt
        8192,   // HeapRingBufferCapacity
        4096,   // SharedRingBufferCapacity (compile-time)
        oneplog::QueueFullPolicy::Block,
        BenchmarkSharedMemoryName,
        10,     // PollTimeoutMs
        oneplog::SinkBindingList<NullSinkBinding>
    >;
    
    (void)sharedBufferCapacity;  // Capacity is compile-time in current implementation
    
    oneplog::RuntimeConfig runtimeConfig;
    runtimeConfig.processName = "BenchmarkProducer";
    
    oneplog::LoggerImpl<MProcConfig> logger(runtimeConfig);
    
    // Test data / 测试数据
    int intVal = 42;
    double doubleVal = 3.14159265358979;
    const char* strVal = "mproc_benchmark";
    
    // Warmup phase / 预热阶段
    for (size_t i = 0; i < warmupIterations; ++i) {
        logger.Info(kTestMessage, intVal, doubleVal, strVal);
    }
    logger.Flush();
    
    // Benchmark phase / 基准测试阶段
    oneplog::benchmark::LatencyCollector collector(iterations);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < iterations; ++i) {
        auto opStart = std::chrono::high_resolution_clock::now();
        logger.Info(kTestMessage, intVal, doubleVal, strVal);
        auto opEnd = std::chrono::high_resolution_clock::now();
        
        double latencyNs = static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(opEnd - opStart).count());
        collector.AddSample(latencyNs);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logger.Flush();
    
    // Calculate results / 计算结果
    oneplog::benchmark::BenchmarkResult result;
    result.testName = "onePlog MProc (1 producer)";
    result.numThreads = 1;
    result.totalOperations = iterations;
    result.totalTimeMs = static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count()) / 1000.0;
    result.throughputOpsPerSec = static_cast<double>(iterations) / (result.totalTimeMs / 1000.0);
    result.avgLatencyNs = collector.GetAverage();
    result.p50LatencyNs = collector.GetP50();
    result.p99LatencyNs = collector.GetP99();
    result.p999LatencyNs = collector.GetP999();
    result.minLatencyNs = collector.GetMin();
    result.maxLatencyNs = collector.GetMax();
    result.stdDevNs = collector.GetStdDev();
    
    return result;
}

/**
 * @brief Benchmark onePlog MProc mode with multiple producers (simulated)
 * @brief 使用多生产者测试 onePlog 多进程模式（模拟）
 *
 * This test simulates multiple producer processes using threads.
 * Each thread acts as a separate producer writing to the shared ring buffer.
 *
 * 此测试使用线程模拟多个生产者进程。
 * 每个线程作为独立的生产者写入共享环形队列。
 */
oneplog::benchmark::BenchmarkResult BenchmarkOneplogMProcMultiProducer(
    size_t iterations, size_t warmupIterations, size_t numProducers, size_t sharedBufferCapacity) {
    
    // Configure logger with MProc mode and NullSink
    // 配置使用 MProc 模式和 NullSink 的日志器
    using NullSinkBinding = oneplog::SinkBinding<oneplog::NullSinkType, oneplog::MessageOnlyFormat>;
    using MProcConfig = oneplog::LoggerConfig<
        oneplog::Mode::MProc,
        oneplog::Level::Info,
        false,  // EnableWFC
        true,   // EnableShadowTail
        true,   // UseFmt
        8192,   // HeapRingBufferCapacity
        4096,   // SharedRingBufferCapacity
        oneplog::QueueFullPolicy::Block,
        BenchmarkSharedMemoryName,
        10,     // PollTimeoutMs
        oneplog::SinkBindingList<NullSinkBinding>
    >;
    
    (void)sharedBufferCapacity;
    
    oneplog::RuntimeConfig runtimeConfig;
    runtimeConfig.processName = "BenchmarkMultiProducer";
    
    oneplog::LoggerImpl<MProcConfig> logger(runtimeConfig);
    
    // Test data / 测试数据
    int intVal = 42;
    double doubleVal = 3.14159265358979;
    const char* strVal = "mproc_benchmark";
    
    // Warmup phase (single thread) / 预热阶段（单线程）
    for (size_t i = 0; i < warmupIterations; ++i) {
        logger.Info(kTestMessage, intVal, doubleVal, strVal);
    }
    logger.Flush();
    
    // Benchmark phase (multi-thread simulating multi-process) / 基准测试阶段（多线程模拟多进程）
    std::vector<oneplog::benchmark::LatencyCollector> collectors(numProducers);
    for (auto& collector : collectors) {
        collector.Reserve(iterations / numProducers + 1);
    }
    
    std::atomic<bool> startFlag{false};
    std::vector<std::thread> threads;
    threads.reserve(numProducers);
    
    size_t opsPerProducer = iterations / numProducers;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (size_t p = 0; p < numProducers; ++p) {
        threads.emplace_back([&, p]() {
            while (!startFlag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            
            for (size_t i = 0; i < opsPerProducer; ++i) {
                auto opStart = std::chrono::high_resolution_clock::now();
                logger.Info(kTestMessage, intVal, doubleVal, strVal);
                auto opEnd = std::chrono::high_resolution_clock::now();
                
                double latencyNs = static_cast<double>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(opEnd - opStart).count());
                collectors[p].AddSample(latencyNs);
            }
        });
    }
    
    startFlag.store(true, std::memory_order_release);
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logger.Flush();
    
    // Merge results / 合并结果
    double totalLatency = 0.0;
    double minLatency = std::numeric_limits<double>::max();
    double maxLatency = 0.0;
    
    for (auto& collector : collectors) {
        totalLatency += collector.GetAverage() * static_cast<double>(collector.Size());
        minLatency = std::min(minLatency, collector.GetMin());
        maxLatency = std::max(maxLatency, collector.GetMax());
    }
    
    auto& firstCollector = collectors[0];
    
    // Calculate results / 计算结果
    oneplog::benchmark::BenchmarkResult result;
    result.testName = "onePlog MProc (" + std::to_string(numProducers) + " producers)";
    result.numThreads = numProducers;
    result.totalOperations = opsPerProducer * numProducers;
    result.totalTimeMs = static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count()) / 1000.0;
    result.throughputOpsPerSec = static_cast<double>(result.totalOperations) / (result.totalTimeMs / 1000.0);
    result.avgLatencyNs = totalLatency / static_cast<double>(result.totalOperations);
    result.p50LatencyNs = firstCollector.GetP50();
    result.p99LatencyNs = firstCollector.GetP99();
    result.p999LatencyNs = firstCollector.GetP999();
    result.minLatencyNs = minLatency;
    result.maxLatencyNs = maxLatency;
    result.stdDevNs = firstCollector.GetStdDev();
    
    return result;
}

/**
 * @brief Benchmark SharedRingBuffer with different capacities
 * @brief 测试不同容量的 SharedRingBuffer
 *
 * This test measures how SharedRingBuffer capacity affects performance.
 * Note: In the current implementation, capacity is compile-time, so this
 * test uses different logger configurations.
 *
 * 此测试测量 SharedRingBuffer 容量如何影响性能。
 * 注意：在当前实现中，容量是编译期确定的，因此此测试使用不同的日志器配置。
 */
void BenchmarkSharedRingBufferCapacities(size_t iterations, size_t warmupIterations) {
    std::cout << "\n--- SharedRingBuffer Capacity Benchmark / SharedRingBuffer 容量基准测试 ---\n";
    std::cout << "Note: Testing with compile-time configured capacities.\n";
    std::cout << "注意: 使用编译期配置的容量进行测试。\n\n";
    
    // Test data / 测试数据
    int intVal = 42;
    double doubleVal = 3.14159265358979;
    const char* strVal = "capacity_test";
    
    // Small capacity (1024) / 小容量 (1024)
    {
        using NullSinkBinding = oneplog::SinkBinding<oneplog::NullSinkType, oneplog::MessageOnlyFormat>;
        using SmallCapConfig = oneplog::LoggerConfig<
            oneplog::Mode::MProc,
            oneplog::Level::Info,
            false, true, true,
            8192,
            1024,  // Small SharedRingBuffer capacity / 小 SharedRingBuffer 容量
            oneplog::QueueFullPolicy::Block,
            BenchmarkSharedMemoryName, 10,
            oneplog::SinkBindingList<NullSinkBinding>
        >;
        
        oneplog::LoggerImpl<SmallCapConfig> logger;
        
        // Warmup / 预热
        for (size_t i = 0; i < warmupIterations; ++i) {
            logger.Info(kTestMessage, intVal, doubleVal, strVal);
        }
        logger.Flush();
        
        // Benchmark / 基准测试
        auto startTime = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            logger.Info(kTestMessage, intVal, doubleVal, strVal);
        }
        logger.Flush();
        auto endTime = std::chrono::high_resolution_clock::now();
        
        double totalTimeMs = static_cast<double>(
            std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count()) / 1000.0;
        double throughput = static_cast<double>(iterations) / (totalTimeMs / 1000.0);
        
        std::cout << "SharedRingBuffer Capacity: 1024\n";
        std::cout << "  Throughput: " << std::fixed << std::setprecision(2) << throughput << " ops/sec\n";
        std::cout << "  Total Time: " << totalTimeMs << " ms\n\n";
    }
    
    // Medium capacity (4096) / 中等容量 (4096)
    {
        using NullSinkBinding = oneplog::SinkBinding<oneplog::NullSinkType, oneplog::MessageOnlyFormat>;
        using MediumCapConfig = oneplog::LoggerConfig<
            oneplog::Mode::MProc,
            oneplog::Level::Info,
            false, true, true,
            8192,
            4096,  // Medium SharedRingBuffer capacity / 中等 SharedRingBuffer 容量
            oneplog::QueueFullPolicy::Block,
            BenchmarkSharedMemoryName, 10,
            oneplog::SinkBindingList<NullSinkBinding>
        >;
        
        oneplog::LoggerImpl<MediumCapConfig> logger;
        
        // Warmup / 预热
        for (size_t i = 0; i < warmupIterations; ++i) {
            logger.Info(kTestMessage, intVal, doubleVal, strVal);
        }
        logger.Flush();
        
        // Benchmark / 基准测试
        auto startTime = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            logger.Info(kTestMessage, intVal, doubleVal, strVal);
        }
        logger.Flush();
        auto endTime = std::chrono::high_resolution_clock::now();
        
        double totalTimeMs = static_cast<double>(
            std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count()) / 1000.0;
        double throughput = static_cast<double>(iterations) / (totalTimeMs / 1000.0);
        
        std::cout << "SharedRingBuffer Capacity: 4096\n";
        std::cout << "  Throughput: " << std::fixed << std::setprecision(2) << throughput << " ops/sec\n";
        std::cout << "  Total Time: " << totalTimeMs << " ms\n\n";
    }
    
    // Large capacity (8192) / 大容量 (8192)
    {
        using NullSinkBinding = oneplog::SinkBinding<oneplog::NullSinkType, oneplog::MessageOnlyFormat>;
        using LargeCapConfig = oneplog::LoggerConfig<
            oneplog::Mode::MProc,
            oneplog::Level::Info,
            false, true, true,
            8192,
            8192,  // Large SharedRingBuffer capacity / 大 SharedRingBuffer 容量
            oneplog::QueueFullPolicy::Block,
            BenchmarkSharedMemoryName, 10,
            oneplog::SinkBindingList<NullSinkBinding>
        >;
        
        oneplog::LoggerImpl<LargeCapConfig> logger;
        
        // Warmup / 预热
        for (size_t i = 0; i < warmupIterations; ++i) {
            logger.Info(kTestMessage, intVal, doubleVal, strVal);
        }
        logger.Flush();
        
        // Benchmark / 基准测试
        auto startTime = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            logger.Info(kTestMessage, intVal, doubleVal, strVal);
        }
        logger.Flush();
        auto endTime = std::chrono::high_resolution_clock::now();
        
        double totalTimeMs = static_cast<double>(
            std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count()) / 1000.0;
        double throughput = static_cast<double>(iterations) / (totalTimeMs / 1000.0);
        
        std::cout << "SharedRingBuffer Capacity: 8192\n";
        std::cout << "  Throughput: " << std::fixed << std::setprecision(2) << throughput << " ops/sec\n";
        std::cout << "  Total Time: " << totalTimeMs << " ms\n";
    }
}

/**
 * @brief Benchmark cross-process communication latency distribution
 * @brief 测试跨进程通信延迟分布
 *
 * This test measures the latency distribution of cross-process communication
 * through the SharedRingBuffer.
 *
 * 此测试测量通过 SharedRingBuffer 的跨进程通信延迟分布。
 */
void BenchmarkLatencyDistribution(size_t iterations, size_t warmupIterations) {
    std::cout << "\n--- Latency Distribution Benchmark / 延迟分布基准测试 ---\n";
    
    // Configure logger with MProc mode and NullSink
    // 配置使用 MProc 模式和 NullSink 的日志器
    using NullSinkBinding = oneplog::SinkBinding<oneplog::NullSinkType, oneplog::MessageOnlyFormat>;
    using MProcConfig = oneplog::LoggerConfig<
        oneplog::Mode::MProc,
        oneplog::Level::Info,
        false,  // EnableWFC
        true,   // EnableShadowTail
        true,   // UseFmt
        8192,   // HeapRingBufferCapacity
        4096,   // SharedRingBufferCapacity
        oneplog::QueueFullPolicy::Block,
        BenchmarkSharedMemoryName,
        10,     // PollTimeoutMs
        oneplog::SinkBindingList<NullSinkBinding>
    >;
    
    oneplog::LoggerImpl<MProcConfig> logger;
    
    // Test data / 测试数据
    int intVal = 42;
    double doubleVal = 3.14159265358979;
    const char* strVal = "latency_test";
    
    // Warmup phase / 预热阶段
    for (size_t i = 0; i < warmupIterations; ++i) {
        logger.Info(kTestMessage, intVal, doubleVal, strVal);
    }
    logger.Flush();
    
    // Collect latency samples / 收集延迟样本
    oneplog::benchmark::LatencyCollector collector(iterations);
    
    for (size_t i = 0; i < iterations; ++i) {
        auto opStart = std::chrono::high_resolution_clock::now();
        logger.Info(kTestMessage, intVal, doubleVal, strVal);
        auto opEnd = std::chrono::high_resolution_clock::now();
        
        double latencyNs = static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(opEnd - opStart).count());
        collector.AddSample(latencyNs);
    }
    
    logger.Flush();
    
    // Print latency distribution / 打印延迟分布
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Latency Distribution (ns):\n";
    std::cout << "  Min:     " << collector.GetMin() << "\n";
    std::cout << "  P10:     " << collector.GetPercentile(0.10) << "\n";
    std::cout << "  P25:     " << collector.GetPercentile(0.25) << "\n";
    std::cout << "  P50:     " << collector.GetP50() << "\n";
    std::cout << "  P75:     " << collector.GetPercentile(0.75) << "\n";
    std::cout << "  P90:     " << collector.GetPercentile(0.90) << "\n";
    std::cout << "  P95:     " << collector.GetPercentile(0.95) << "\n";
    std::cout << "  P99:     " << collector.GetP99() << "\n";
    std::cout << "  P999:    " << collector.GetP999() << "\n";
    std::cout << "  Max:     " << collector.GetMax() << "\n";
    std::cout << "  Average: " << collector.GetAverage() << "\n";
    std::cout << "  StdDev:  " << collector.GetStdDev() << "\n";
    
    // Print histogram buckets / 打印直方图桶
    std::cout << "\nLatency Histogram:\n";
    
    // Define histogram buckets (in nanoseconds) / 定义直方图桶（纳秒）
    const std::vector<double> buckets = {100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000, 100000};
    std::vector<size_t> counts(buckets.size() + 1, 0);
    
    collector.Sort();
    
    // Count samples in each bucket / 统计每个桶中的样本数
    for (size_t i = 0; i < collector.Size(); ++i) {
        double latency = collector.GetPercentile(static_cast<double>(i + 1) / static_cast<double>(collector.Size()));
        
        size_t bucketIdx = 0;
        for (size_t b = 0; b < buckets.size(); ++b) {
            if (latency <= buckets[b]) {
                bucketIdx = b;
                break;
            }
            bucketIdx = b + 1;
        }
        counts[bucketIdx]++;
    }
    
    // Print histogram / 打印直方图
    std::cout << "  <= 100ns:    " << counts[0] << " (" 
              << (100.0 * static_cast<double>(counts[0]) / static_cast<double>(iterations)) << "%)\n";
    for (size_t b = 1; b < buckets.size(); ++b) {
        std::cout << "  <= " << std::setw(6) << buckets[b] << "ns: " << counts[b] << " ("
                  << (100.0 * static_cast<double>(counts[b]) / static_cast<double>(iterations)) << "%)\n";
    }
    std::cout << "  > " << std::setw(6) << buckets.back() << "ns:  " << counts.back() << " ("
              << (100.0 * static_cast<double>(counts.back()) / static_cast<double>(iterations)) << "%)\n";
}

// ==============================================================================
// Result Printing / 结果打印
// ==============================================================================

/**
 * @brief Print benchmark result in a formatted table
 * @brief 以格式化表格打印基准测试结果
 */
void PrintResult(const oneplog::benchmark::BenchmarkResult& result) {
    std::cout << "\n" << result.testName << ":\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Producers:     " << result.numThreads << "\n";
    std::cout << "  Throughput:    " << result.throughputOpsPerSec << " ops/sec\n";
    std::cout << "  Total Time:    " << result.totalTimeMs << " ms\n";
    std::cout << "  Latency (ns):\n";
    std::cout << "    Average:     " << result.avgLatencyNs << "\n";
    std::cout << "    Min:         " << result.minLatencyNs << "\n";
    std::cout << "    Max:         " << result.maxLatencyNs << "\n";
    std::cout << "    P50:         " << result.p50LatencyNs << "\n";
    std::cout << "    P99:         " << result.p99LatencyNs << "\n";
    std::cout << "    P999:        " << result.p999LatencyNs << "\n";
    std::cout << "    StdDev:      " << result.stdDevNs << "\n";
}

// ==============================================================================
// Command Line Parsing / 命令行解析
// ==============================================================================

/**
 * @brief Print usage information
 * @brief 打印使用说明
 */
void PrintUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n";
    std::cout << "用法: " << programName << " [选项]\n\n";
    std::cout << "Options / 选项:\n";
    std::cout << "  -n, --iterations <N>    Number of iterations (default: " 
              << kDefaultIterations << ")\n";
    std::cout << "                          迭代次数（默认: " << kDefaultIterations << "）\n";
    std::cout << "  -w, --warmup <N>        Warmup iterations (default: " 
              << kDefaultWarmupIterations << ")\n";
    std::cout << "                          预热次数（默认: " << kDefaultWarmupIterations << "）\n";
    std::cout << "  -p, --producers <N>     Number of producer threads (default: " 
              << kDefaultNumProducers << ")\n";
    std::cout << "                          生产者线程数（默认: " << kDefaultNumProducers << "）\n";
    std::cout << "  --single                Run single producer benchmark only\n";
    std::cout << "                          仅运行单生产者测试\n";
    std::cout << "  --multi                 Run multi producer benchmark only\n";
    std::cout << "                          仅运行多生产者测试\n";
    std::cout << "  --capacity              Run capacity benchmark\n";
    std::cout << "                          运行容量测试\n";
    std::cout << "  --latency               Run latency distribution benchmark\n";
    std::cout << "                          运行延迟分布测试\n";
    std::cout << "  -v, --verbose           Verbose output\n";
    std::cout << "                          详细输出\n";
    std::cout << "  -h, --help              Show this help message\n";
    std::cout << "                          显示此帮助信息\n";
}

/**
 * @brief Parse command line arguments
 * @brief 解析命令行参数
 */
BenchmarkConfig ParseArgs(int argc, char* argv[]) {
    BenchmarkConfig config;
    bool testSpecified = false;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "-n" || arg == "--iterations") {
            if (i + 1 < argc) {
                config.iterations = static_cast<size_t>(std::atol(argv[++i]));
            }
        } else if (arg == "-w" || arg == "--warmup") {
            if (i + 1 < argc) {
                config.warmupIterations = static_cast<size_t>(std::atol(argv[++i]));
            }
        } else if (arg == "-p" || arg == "--producers") {
            if (i + 1 < argc) {
                config.numProducers = static_cast<size_t>(std::atol(argv[++i]));
            }
        } else if (arg == "--single") {
            if (!testSpecified) {
                config.runSingleProducer = false;
                config.runMultiProducer = false;
                config.runCapacityTest = false;
                config.runLatencyDistribution = false;
                testSpecified = true;
            }
            config.runSingleProducer = true;
        } else if (arg == "--multi") {
            if (!testSpecified) {
                config.runSingleProducer = false;
                config.runMultiProducer = false;
                config.runCapacityTest = false;
                config.runLatencyDistribution = false;
                testSpecified = true;
            }
            config.runMultiProducer = true;
        } else if (arg == "--capacity") {
            if (!testSpecified) {
                config.runSingleProducer = false;
                config.runMultiProducer = false;
                config.runCapacityTest = false;
                config.runLatencyDistribution = false;
                testSpecified = true;
            }
            config.runCapacityTest = true;
        } else if (arg == "--latency") {
            if (!testSpecified) {
                config.runSingleProducer = false;
                config.runMultiProducer = false;
                config.runCapacityTest = false;
                config.runLatencyDistribution = false;
                testSpecified = true;
            }
            config.runLatencyDistribution = true;
        } else if (arg == "-v" || arg == "--verbose") {
            config.verbose = true;
        } else if (arg == "-h" || arg == "--help") {
            PrintUsage(argv[0]);
            std::exit(0);
        }
    }
    
    return config;
}

}  // anonymous namespace

// ==============================================================================
// Main Function / 主函数
// ==============================================================================

int main(int argc, char* argv[]) {
    BenchmarkConfig config = ParseArgs(argc, argv);
    
    std::cout << "========================================\n";
    std::cout << "onePlog MProc Mode Benchmark\n";
    std::cout << "onePlog 多进程模式性能测试\n";
    std::cout << "========================================\n";
    std::cout << "Iterations: " << config.iterations << " / 迭代次数: " << config.iterations << "\n";
    std::cout << "Warmup: " << config.warmupIterations << " / 预热次数: " << config.warmupIterations << "\n";
    std::cout << "Producers: " << config.numProducers << " / 生产者数: " << config.numProducers << "\n";
    std::cout << "SharedRingBuffer Capacity: " << config.sharedBufferCapacity 
              << " / SharedRingBuffer 容量: " << config.sharedBufferCapacity << "\n";
    std::cout << "========================================\n";
    std::cout << "\nNote: Multi-process behavior is simulated using threads.\n";
    std::cout << "注意: 多进程行为使用线程模拟。\n";
    std::cout << "For true multi-process testing, run multiple instances.\n";
    std::cout << "对于真正的多进程测试，请运行多个实例。\n";
    
    // ==============================================================================
    // Single Producer Benchmark / 单生产者基准测试
    // ==============================================================================
    
    if (config.runSingleProducer) {
        std::cout << "\n--- Single Producer Benchmark / 单生产者基准测试 ---\n";
        
        std::cout << "Running onePlog MProc (single producer)...\n";
        std::cout << "运行 onePlog MProc（单生产者）...\n";
        auto result = BenchmarkOneplogMProcSingleProducer(
            config.iterations, config.warmupIterations, config.sharedBufferCapacity);
        PrintResult(result);
    }
    
    // ==============================================================================
    // Multi Producer Benchmark / 多生产者基准测试
    // ==============================================================================
    
    if (config.runMultiProducer) {
        std::cout << "\n--- Multi Producer Benchmark / 多生产者基准测试 ---\n";
        
        std::cout << "Running onePlog MProc (multi-producer)...\n";
        std::cout << "运行 onePlog MProc（多生产者）...\n";
        auto result = BenchmarkOneplogMProcMultiProducer(
            config.iterations, config.warmupIterations, config.numProducers, config.sharedBufferCapacity);
        PrintResult(result);
    }
    
    // ==============================================================================
    // SharedRingBuffer Capacity Benchmark / SharedRingBuffer 容量基准测试
    // ==============================================================================
    
    if (config.runCapacityTest) {
        BenchmarkSharedRingBufferCapacities(config.iterations, config.warmupIterations);
    }
    
    // ==============================================================================
    // Latency Distribution Benchmark / 延迟分布基准测试
    // ==============================================================================
    
    if (config.runLatencyDistribution) {
        BenchmarkLatencyDistribution(config.iterations, config.warmupIterations);
    }
    
    // ==============================================================================
    // Summary / 总结
    // ==============================================================================
    
    std::cout << "\n========================================\n";
    std::cout << "Benchmark Complete / 基准测试完成\n";
    std::cout << "========================================\n";
    
    return 0;
}
