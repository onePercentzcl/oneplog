/**
 * @file benchmark_async.cpp
 * @brief Asynchronous mode performance benchmark for onePlog
 * @brief onePlog 异步模式性能测试
 *
 * This benchmark measures the performance of onePlog's asynchronous logging mode
 * and compares it with spdlog async mode (if available).
 *
 * 此基准测试测量 onePlog 异步日志模式的性能，并与 spdlog 异步模式进行对比（如果可用）。
 *
 * Test scenarios / 测试场景:
 * - NullSink: Measures pure async logging overhead without I/O
 *   NullSink: 测量无 I/O 的纯异步日志开销
 * - FileSink: Measures async logging with file I/O
 *   FileSink: 测量带文件 I/O 的异步日志性能
 * - Single-threaded producer: One thread producing logs
 *   单线程生产者: 一个线程产生日志
 * - Multi-threaded producer: Multiple threads producing logs
 *   多线程生产者: 多个线程产生日志
 * - QueueFullPolicy: Different policies when queue is full
 *   QueueFullPolicy: 队列满时的不同策略
 *
 * Metrics / 指标:
 * - Throughput (ops/sec): Operations per second
 *   吞吐量 (ops/sec): 每秒操作数
 * - Latency: Average, P50, P99, P999 percentiles
 *   延迟: 平均值、P50、P99、P999 百分位数
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

#ifdef HAS_SPDLOG
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#endif

namespace {

// ==============================================================================
// Configuration / 配置
// ==============================================================================

/// Default number of iterations / 默认迭代次数
constexpr size_t kDefaultIterations = 100000;

/// Default warmup iterations / 默认预热次数
constexpr size_t kDefaultWarmupIterations = 10000;

/// Default number of threads for multi-threaded tests / 多线程测试的默认线程数
constexpr size_t kDefaultNumThreads = 4;

/// Default queue size / 默认队列大小
constexpr size_t kDefaultQueueSize = 8192;

/// Test message for benchmarking / 用于基准测试的测试消息
/// Using simple message format consistent with old benchmark for fair comparison
/// 使用与旧基准测试一致的简单消息格式以进行公平对比
constexpr const char* kTestMessage = "Message {} value {}";

/// Test file path for file sink benchmarks / 文件 Sink 基准测试的测试文件路径
constexpr const char* kTestFilePath = "benchmark_async_test.log";

/// spdlog test file path / spdlog 测试文件路径
constexpr const char* kSpdlogTestFilePath = "benchmark_async_spdlog.log";

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
    size_t numThreads{kDefaultNumThreads};       ///< Number of producer threads / 生产者线程数
    size_t queueSize{kDefaultQueueSize};         ///< Queue size / 队列大小
    bool runNullSink{true};                      ///< Run NullSink benchmark / 运行 NullSink 测试
    bool runFileSink{true};                      ///< Run FileSink benchmark / 运行 FileSink 测试
    bool runSingleThread{true};                  ///< Run single-thread benchmark / 运行单线程测试
    bool runMultiThread{true};                   ///< Run multi-thread benchmark / 运行多线程测试
    bool runQueuePolicyTest{true};               ///< Run queue policy test / 运行队列策略测试
    bool runSpdlogComparison{true};              ///< Run spdlog comparison / 运行 spdlog 对比
    bool verbose{false};                         ///< Verbose output / 详细输出
};

// ==============================================================================
// onePlog Async Mode Benchmarks / onePlog 异步模式基准测试
// ==============================================================================

/**
 * @brief Benchmark onePlog async mode with NullSink (single thread)
 * @brief 使用 NullSink 测试 onePlog 异步模式（单线程）
 */
oneplog::benchmark::BenchmarkResult BenchmarkOneplogAsyncNullSinkSingleThread(
    size_t iterations, size_t warmupIterations, size_t queueSize) {
    
    // Configure logger with NullSink and MessageOnlyFormat
    // 配置使用 NullSink 和 MessageOnlyFormat 的日志器
    using NullSinkBinding = oneplog::SinkBinding<oneplog::NullSinkType, oneplog::MessageOnlyFormat>;
    using NullSinkConfig = oneplog::LoggerConfig<
        oneplog::Mode::Async,
        oneplog::Level::Info,
        false,  // EnableWFC
        true,   // EnableShadowTail
        true,   // UseFmt
        8192,   // HeapRingBufferCapacity (will be overridden)
        4096,   // SharedRingBufferCapacity
        oneplog::QueueFullPolicy::Block,  // Use Block for fair comparison with spdlog
        oneplog::DefaultSharedMemoryName,
        10,     // PollTimeoutMs
        oneplog::SinkBindingList<NullSinkBinding>
    >;
    
    (void)queueSize;  // Queue size is compile-time in current implementation
    oneplog::LoggerImpl<NullSinkConfig> logger;
    
    // Test data / 测试数据
    int intVal = 42;
    double doubleVal = 3.14159;
    
    // Warmup phase / 预热阶段
    for (size_t i = 0; i < warmupIterations; ++i) {
        logger.Info(kTestMessage, intVal, doubleVal);
    }
    logger.Flush();
    
    // Benchmark phase / 基准测试阶段
    oneplog::benchmark::LatencyCollector collector(iterations);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < iterations; ++i) {
        auto opStart = std::chrono::high_resolution_clock::now();
        logger.Info(kTestMessage, intVal, doubleVal);
        auto opEnd = std::chrono::high_resolution_clock::now();
        
        double latencyNs = static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(opEnd - opStart).count());
        collector.AddSample(latencyNs);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logger.Flush();
    
    // Calculate results / 计算结果
    oneplog::benchmark::BenchmarkResult result;
    result.testName = "onePlog Async NullSink (1 thread)";
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
 * @brief Benchmark onePlog async mode with NullSink (multi-thread)
 * @brief 使用 NullSink 测试 onePlog 异步模式（多线程）
 */
oneplog::benchmark::BenchmarkResult BenchmarkOneplogAsyncNullSinkMultiThread(
    size_t iterations, size_t warmupIterations, size_t numThreads, size_t queueSize) {
    
    // Configure logger with NullSink and MessageOnlyFormat
    // 配置使用 NullSink 和 MessageOnlyFormat 的日志器
    using NullSinkBinding = oneplog::SinkBinding<oneplog::NullSinkType, oneplog::MessageOnlyFormat>;
    using NullSinkConfig = oneplog::LoggerConfig<
        oneplog::Mode::Async,
        oneplog::Level::Info,
        false,  // EnableWFC
        true,   // EnableShadowTail
        true,   // UseFmt
        8192,   // HeapRingBufferCapacity
        4096,   // SharedRingBufferCapacity
        oneplog::QueueFullPolicy::Block,  // Use Block for fair comparison with spdlog
        oneplog::DefaultSharedMemoryName,
        10,     // PollTimeoutMs
        oneplog::SinkBindingList<NullSinkBinding>
    >;
    
    (void)queueSize;
    oneplog::LoggerImpl<NullSinkConfig> logger;
    
    // Test data / 测试数据
    int intVal = 42;
    double doubleVal = 3.14159;
    
    // Warmup phase (single thread) / 预热阶段（单线程）
    for (size_t i = 0; i < warmupIterations; ++i) {
        logger.Info(kTestMessage, intVal, doubleVal);
    }
    logger.Flush();
    
    // Benchmark phase (multi-thread) / 基准测试阶段（多线程）
    std::vector<oneplog::benchmark::LatencyCollector> collectors(numThreads);
    for (auto& collector : collectors) {
        collector.Reserve(iterations / numThreads + 1);
    }
    
    std::atomic<bool> startFlag{false};
    std::vector<std::thread> threads;
    threads.reserve(numThreads);
    
    size_t opsPerThread = iterations / numThreads;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (size_t t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            while (!startFlag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            
            for (size_t i = 0; i < opsPerThread; ++i) {
                auto opStart = std::chrono::high_resolution_clock::now();
                logger.Info(kTestMessage, intVal, doubleVal);
                auto opEnd = std::chrono::high_resolution_clock::now();
                
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
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logger.Flush();
    
    // Merge results from all collectors / 合并所有收集器的结果
    oneplog::benchmark::LatencyCollector merged;
    merged.Reserve(iterations);
    double totalLatency = 0.0;
    double minLatency = std::numeric_limits<double>::max();
    double maxLatency = 0.0;
    
    for (auto& collector : collectors) {
        totalLatency += collector.GetAverage() * static_cast<double>(collector.Size());
        minLatency = std::min(minLatency, collector.GetMin());
        maxLatency = std::max(maxLatency, collector.GetMax());
    }
    
    // Use first collector for percentiles (approximation)
    // 使用第一个收集器的百分位数（近似值）
    auto& firstCollector = collectors[0];
    
    // Calculate results / 计算结果
    oneplog::benchmark::BenchmarkResult result;
    result.testName = "onePlog Async NullSink (" + std::to_string(numThreads) + " threads)";
    result.numThreads = numThreads;
    result.totalOperations = opsPerThread * numThreads;
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
 * @brief Benchmark onePlog async mode with FileSink (single thread)
 * @brief 使用 FileSink 测试 onePlog 异步模式（单线程）
 */
oneplog::benchmark::BenchmarkResult BenchmarkOneplogAsyncFileSinkSingleThread(
    size_t iterations, size_t warmupIterations, size_t queueSize) {
    
    // Remove existing test file / 删除现有测试文件
    std::remove(kTestFilePath);
    
    // Configure logger with FileSink and MessageOnlyFormat for fair comparison
    // 配置使用 FileSink 和 MessageOnlyFormat 的日志器以进行公平对比
    using FileSinkBinding = oneplog::SinkBinding<oneplog::FileSinkType, oneplog::MessageOnlyFormat>;
    using FileSinkConfig = oneplog::LoggerConfig<
        oneplog::Mode::Async,
        oneplog::Level::Info,
        false,  // EnableWFC
        true,   // EnableShadowTail
        true,   // UseFmt
        8192,   // HeapRingBufferCapacity
        4096,   // SharedRingBufferCapacity
        oneplog::QueueFullPolicy::Block,  // Use Block for fair comparison with spdlog
        oneplog::DefaultSharedMemoryName,
        10,     // PollTimeoutMs
        oneplog::SinkBindingList<FileSinkBinding>
    >;
    
    (void)queueSize;
    oneplog::SinkBindingList<FileSinkBinding> bindings{
        FileSinkBinding{oneplog::FileSinkType{kTestFilePath}}
    };
    oneplog::LoggerImpl<FileSinkConfig> logger{std::move(bindings)};
    
    // Test data / 测试数据
    int intVal = 42;
    double doubleVal = 3.14159;
    
    // Warmup phase / 预热阶段
    for (size_t i = 0; i < warmupIterations; ++i) {
        logger.Info(kTestMessage, intVal, doubleVal);
    }
    logger.Flush();
    
    // Benchmark phase / 基准测试阶段
    oneplog::benchmark::LatencyCollector collector(iterations);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < iterations; ++i) {
        auto opStart = std::chrono::high_resolution_clock::now();
        logger.Info(kTestMessage, intVal, doubleVal);
        auto opEnd = std::chrono::high_resolution_clock::now();
        
        double latencyNs = static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(opEnd - opStart).count());
        collector.AddSample(latencyNs);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logger.Flush();
    
    // Calculate results / 计算结果
    oneplog::benchmark::BenchmarkResult result;
    result.testName = "onePlog Async FileSink (1 thread)";
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
 * @brief Benchmark onePlog async mode with FileSink (multi-thread)
 * @brief 使用 FileSink 测试 onePlog 异步模式（多线程）
 */
oneplog::benchmark::BenchmarkResult BenchmarkOneplogAsyncFileSinkMultiThread(
    size_t iterations, size_t warmupIterations, size_t numThreads, size_t queueSize) {
    
    // Remove existing test file / 删除现有测试文件
    std::remove(kTestFilePath);
    
    // Configure logger with FileSink and MessageOnlyFormat for fair comparison
    // 配置使用 FileSink 和 MessageOnlyFormat 的日志器以进行公平对比
    using FileSinkBinding = oneplog::SinkBinding<oneplog::FileSinkType, oneplog::MessageOnlyFormat>;
    using FileSinkConfig = oneplog::LoggerConfig<
        oneplog::Mode::Async,
        oneplog::Level::Info,
        false,  // EnableWFC
        true,   // EnableShadowTail
        true,   // UseFmt
        8192,   // HeapRingBufferCapacity
        4096,   // SharedRingBufferCapacity
        oneplog::QueueFullPolicy::Block,  // Use Block for fair comparison with spdlog
        oneplog::DefaultSharedMemoryName,
        10,     // PollTimeoutMs
        oneplog::SinkBindingList<FileSinkBinding>
    >;
    
    (void)queueSize;
    oneplog::SinkBindingList<FileSinkBinding> bindings{
        FileSinkBinding{oneplog::FileSinkType{kTestFilePath}}
    };
    oneplog::LoggerImpl<FileSinkConfig> logger{std::move(bindings)};
    
    // Test data / 测试数据
    int intVal = 42;
    double doubleVal = 3.14159;
    
    // Warmup phase / 预热阶段
    for (size_t i = 0; i < warmupIterations; ++i) {
        logger.Info(kTestMessage, intVal, doubleVal);
    }
    logger.Flush();
    
    // Benchmark phase (multi-thread) / 基准测试阶段（多线程）
    std::vector<oneplog::benchmark::LatencyCollector> collectors(numThreads);
    for (auto& collector : collectors) {
        collector.Reserve(iterations / numThreads + 1);
    }
    
    std::atomic<bool> startFlag{false};
    std::vector<std::thread> threads;
    threads.reserve(numThreads);
    
    size_t opsPerThread = iterations / numThreads;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (size_t t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            while (!startFlag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            
            for (size_t i = 0; i < opsPerThread; ++i) {
                auto opStart = std::chrono::high_resolution_clock::now();
                logger.Info(kTestMessage, intVal, doubleVal);
                auto opEnd = std::chrono::high_resolution_clock::now();
                
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
    result.testName = "onePlog Async FileSink (" + std::to_string(numThreads) + " threads)";
    result.numThreads = numThreads;
    result.totalOperations = opsPerThread * numThreads;
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
 * @brief Benchmark onePlog async mode with different QueueFullPolicy
 * @brief 测试 onePlog 异步模式的不同 QueueFullPolicy 策略
 */
void BenchmarkQueueFullPolicies(size_t iterations, size_t warmupIterations) {
    std::cout << "\n--- QueueFullPolicy Benchmark / QueueFullPolicy 基准测试 ---\n";
    
    // Test data / 测试数据
    int intVal = 42;
    double doubleVal = 3.14159;
    
    // Block policy / Block 策略
    {
        using NullSinkBinding = oneplog::SinkBinding<oneplog::NullSinkType, oneplog::MessageOnlyFormat>;
        using BlockConfig = oneplog::LoggerConfig<
            oneplog::Mode::Async,
            oneplog::Level::Info,
            false, true, true,
            1024,  // Small queue to test policy / 小队列以测试策略
            4096,
            oneplog::QueueFullPolicy::Block,
            oneplog::DefaultSharedMemoryName, 10,
            oneplog::SinkBindingList<NullSinkBinding>
        >;
        
        oneplog::LoggerImpl<BlockConfig> logger;
        
        // Warmup / 预热
        for (size_t i = 0; i < warmupIterations; ++i) {
            logger.Info(kTestMessage, intVal, doubleVal);
        }
        logger.Flush();
        
        // Benchmark / 基准测试
        auto startTime = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            logger.Info(kTestMessage, intVal, doubleVal);
        }
        logger.Flush();
        auto endTime = std::chrono::high_resolution_clock::now();
        
        double totalTimeMs = static_cast<double>(
            std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count()) / 1000.0;
        double throughput = static_cast<double>(iterations) / (totalTimeMs / 1000.0);
        
        std::cout << "QueueFullPolicy::Block:\n";
        std::cout << "  Throughput: " << std::fixed << std::setprecision(2) << throughput << " ops/sec\n";
        std::cout << "  Total Time: " << totalTimeMs << " ms\n";
    }
    
    // DropNewest policy / DropNewest 策略
    {
        using NullSinkBinding = oneplog::SinkBinding<oneplog::NullSinkType, oneplog::MessageOnlyFormat>;
        using DropNewestConfig = oneplog::LoggerConfig<
            oneplog::Mode::Async,
            oneplog::Level::Info,
            false, true, true,
            1024,
            4096,
            oneplog::QueueFullPolicy::DropNewest,
            oneplog::DefaultSharedMemoryName, 10,
            oneplog::SinkBindingList<NullSinkBinding>
        >;
        
        oneplog::LoggerImpl<DropNewestConfig> logger;
        
        // Warmup / 预热
        for (size_t i = 0; i < warmupIterations; ++i) {
            logger.Info(kTestMessage, intVal, doubleVal);
        }
        logger.Flush();
        
        // Benchmark / 基准测试
        auto startTime = std::chrono::high_resolution_clock::now();
        for (size_t i = 0; i < iterations; ++i) {
            logger.Info(kTestMessage, intVal, doubleVal);
        }
        logger.Flush();
        auto endTime = std::chrono::high_resolution_clock::now();
        
        double totalTimeMs = static_cast<double>(
            std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count()) / 1000.0;
        double throughput = static_cast<double>(iterations) / (totalTimeMs / 1000.0);
        
        std::cout << "QueueFullPolicy::DropNewest:\n";
        std::cout << "  Throughput: " << std::fixed << std::setprecision(2) << throughput << " ops/sec\n";
        std::cout << "  Total Time: " << totalTimeMs << " ms\n";
    }
    
    // DropOldest policy / DropOldest 策略
    // NOTE: DropOldest has a known issue that may cause infinite loop when queue is full
    // 注意: DropOldest 策略在队列满时可能导致死循环，暂时跳过
    std::cout << "QueueFullPolicy::DropOldest:\n";
    std::cout << "  Skipped (known issue) / 跳过（已知问题）\n";
}

// ==============================================================================
// spdlog Async Mode Benchmarks / spdlog 异步模式基准测试
// ==============================================================================

#ifdef HAS_SPDLOG

/**
 * @brief Benchmark spdlog async mode with null_sink (single thread)
 * @brief 使用 null_sink 测试 spdlog 异步模式（单线程）
 */
oneplog::benchmark::BenchmarkResult BenchmarkSpdlogAsyncNullSinkSingleThread(
    size_t iterations, size_t warmupIterations, size_t queueSize) {
    
    // Initialize spdlog thread pool / 初始化 spdlog 线程池
    spdlog::init_thread_pool(queueSize, 1);
    
    // Create spdlog async logger with null_sink
    // 创建使用 null_sink 的 spdlog 异步日志器
    auto nullSink = std::make_shared<spdlog::sinks::null_sink_mt>();
    auto logger = std::make_shared<spdlog::async_logger>(
        "spdlog_async_null", nullSink, spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);
    logger->set_level(spdlog::level::info);
    logger->set_pattern("%v");
    
    // Test data / 测试数据
    int intVal = 42;
    double doubleVal = 3.14159;
    
    // Warmup phase / 预热阶段
    for (size_t i = 0; i < warmupIterations; ++i) {
        logger->info(kTestMessage, intVal, doubleVal);
    }
    logger->flush();
    
    // Benchmark phase / 基准测试阶段
    oneplog::benchmark::LatencyCollector collector(iterations);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < iterations; ++i) {
        auto opStart = std::chrono::high_resolution_clock::now();
        logger->info(kTestMessage, intVal, doubleVal);
        auto opEnd = std::chrono::high_resolution_clock::now();
        
        double latencyNs = static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(opEnd - opStart).count());
        collector.AddSample(latencyNs);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logger->flush();
    
    // Shutdown spdlog / 关闭 spdlog
    spdlog::drop_all();
    spdlog::shutdown();
    
    // Calculate results / 计算结果
    oneplog::benchmark::BenchmarkResult result;
    result.testName = "spdlog Async NullSink (1 thread)";
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
 * @brief Benchmark spdlog async mode with null_sink (multi-thread)
 * @brief 使用 null_sink 测试 spdlog 异步模式（多线程）
 */
oneplog::benchmark::BenchmarkResult BenchmarkSpdlogAsyncNullSinkMultiThread(
    size_t iterations, size_t warmupIterations, size_t numThreads, size_t queueSize) {
    
    // Initialize spdlog thread pool / 初始化 spdlog 线程池
    spdlog::init_thread_pool(queueSize, 1);
    
    // Create spdlog async logger with null_sink
    // 创建使用 null_sink 的 spdlog 异步日志器
    auto nullSink = std::make_shared<spdlog::sinks::null_sink_mt>();
    auto logger = std::make_shared<spdlog::async_logger>(
        "spdlog_async_null_mt", nullSink, spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);
    logger->set_level(spdlog::level::info);
    logger->set_pattern("%v");
    
    // Test data / 测试数据
    int intVal = 42;
    double doubleVal = 3.14159;
    
    // Warmup phase / 预热阶段
    for (size_t i = 0; i < warmupIterations; ++i) {
        logger->info(kTestMessage, intVal, doubleVal);
    }
    logger->flush();
    
    // Benchmark phase (multi-thread) / 基准测试阶段（多线程）
    std::vector<oneplog::benchmark::LatencyCollector> collectors(numThreads);
    for (auto& collector : collectors) {
        collector.Reserve(iterations / numThreads + 1);
    }
    
    std::atomic<bool> startFlag{false};
    std::vector<std::thread> threads;
    threads.reserve(numThreads);
    
    size_t opsPerThread = iterations / numThreads;
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (size_t t = 0; t < numThreads; ++t) {
        threads.emplace_back([&, t]() {
            while (!startFlag.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            
            for (size_t i = 0; i < opsPerThread; ++i) {
                auto opStart = std::chrono::high_resolution_clock::now();
                logger->info(kTestMessage, intVal, doubleVal);
                auto opEnd = std::chrono::high_resolution_clock::now();
                
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
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logger->flush();
    
    // Shutdown spdlog / 关闭 spdlog
    spdlog::drop_all();
    spdlog::shutdown();
    
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
    result.testName = "spdlog Async NullSink (" + std::to_string(numThreads) + " threads)";
    result.numThreads = numThreads;
    result.totalOperations = opsPerThread * numThreads;
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
 * @brief Benchmark spdlog async mode with basic_file_sink (single thread)
 * @brief 使用 basic_file_sink 测试 spdlog 异步模式（单线程）
 */
oneplog::benchmark::BenchmarkResult BenchmarkSpdlogAsyncFileSinkSingleThread(
    size_t iterations, size_t warmupIterations, size_t queueSize) {
    
    // Remove existing test file / 删除现有测试文件
    std::remove(kSpdlogTestFilePath);
    
    // Initialize spdlog thread pool / 初始化 spdlog 线程池
    spdlog::init_thread_pool(queueSize, 1);
    
    // Create spdlog async logger with basic_file_sink
    // 创建使用 basic_file_sink 的 spdlog 异步日志器
    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(kSpdlogTestFilePath, true);
    auto logger = std::make_shared<spdlog::async_logger>(
        "spdlog_async_file", fileSink, spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);
    logger->set_level(spdlog::level::info);
    logger->set_pattern("%v");  // Message only for fair comparison / 仅消息以进行公平对比
    
    // Test data / 测试数据
    int intVal = 42;
    double doubleVal = 3.14159;
    
    // Warmup phase / 预热阶段
    for (size_t i = 0; i < warmupIterations; ++i) {
        logger->info(kTestMessage, intVal, doubleVal);
    }
    logger->flush();
    
    // Benchmark phase / 基准测试阶段
    oneplog::benchmark::LatencyCollector collector(iterations);
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < iterations; ++i) {
        auto opStart = std::chrono::high_resolution_clock::now();
        logger->info(kTestMessage, intVal, doubleVal);
        auto opEnd = std::chrono::high_resolution_clock::now();
        
        double latencyNs = static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(opEnd - opStart).count());
        collector.AddSample(latencyNs);
    }
    
    auto endTime = std::chrono::high_resolution_clock::now();
    logger->flush();
    
    // Shutdown spdlog / 关闭 spdlog
    spdlog::drop_all();
    spdlog::shutdown();
    
    // Calculate results / 计算结果
    oneplog::benchmark::BenchmarkResult result;
    result.testName = "spdlog Async FileSink (1 thread)";
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

#endif  // HAS_SPDLOG

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
    std::cout << "  Threads:       " << result.numThreads << "\n";
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

/**
 * @brief Print comparison between two results
 * @brief 打印两个结果的对比
 */
void PrintComparison(const oneplog::benchmark::BenchmarkResult& oneplogResult,
                     const oneplog::benchmark::BenchmarkResult& spdlogResult,
                     const std::string& testType) {
    std::cout << "\n========================================\n";
    std::cout << "Comparison: " << testType << "\n";
    std::cout << "对比: " << testType << "\n";
    std::cout << "========================================\n";
    std::cout << std::fixed << std::setprecision(2);
    
    auto printRow = [](const std::string& metric, double oneplogVal, double spdlogVal,
                       const std::string& unit, bool higherIsBetter) {
        double ratio = spdlogVal > 0 ? oneplogVal / spdlogVal : 0;
        std::string comparison;
        if (higherIsBetter) {
            comparison = ratio > 1.0 ? "faster" : "slower";
        } else {
            comparison = ratio < 1.0 ? "faster" : "slower";
        }
        
        std::cout << std::left << std::setw(15) << metric
                  << std::right << std::setw(15) << oneplogVal
                  << std::setw(15) << spdlogVal
                  << std::setw(10) << unit;
        
        if (higherIsBetter) {
            std::cout << "  " << ratio << "x (" << comparison << ")";
        } else {
            std::cout << "  " << (1.0/ratio) << "x (" << comparison << ")";
        }
        std::cout << "\n";
    };
    
    std::cout << std::left << std::setw(15) << "Metric"
              << std::right << std::setw(15) << "onePlog"
              << std::setw(15) << "spdlog"
              << std::setw(10) << "Unit"
              << "  Ratio\n";
    std::cout << std::string(70, '-') << "\n";
    
    printRow("Throughput", oneplogResult.throughputOpsPerSec, 
             spdlogResult.throughputOpsPerSec, "ops/s", true);
    printRow("Avg Latency", oneplogResult.avgLatencyNs, 
             spdlogResult.avgLatencyNs, "ns", false);
    printRow("P50 Latency", oneplogResult.p50LatencyNs, 
             spdlogResult.p50LatencyNs, "ns", false);
    printRow("P99 Latency", oneplogResult.p99LatencyNs, 
             spdlogResult.p99LatencyNs, "ns", false);
    printRow("P999 Latency", oneplogResult.p999LatencyNs, 
             spdlogResult.p999LatencyNs, "ns", false);
    
    std::cout << "========================================\n";
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
    std::cout << "  -t, --threads <N>       Number of producer threads (default: " 
              << kDefaultNumThreads << ")\n";
    std::cout << "                          生产者线程数（默认: " << kDefaultNumThreads << "）\n";
    std::cout << "  -q, --queue-size <N>    Queue size (default: " 
              << kDefaultQueueSize << ")\n";
    std::cout << "                          队列大小（默认: " << kDefaultQueueSize << "）\n";
    std::cout << "  --null                  Run NullSink benchmark only\n";
    std::cout << "                          仅运行 NullSink 测试\n";
    std::cout << "  --file                  Run FileSink benchmark only\n";
    std::cout << "                          仅运行 FileSink 测试\n";
    std::cout << "  --single                Run single-thread benchmark only\n";
    std::cout << "                          仅运行单线程测试\n";
    std::cout << "  --multi                 Run multi-thread benchmark only\n";
    std::cout << "                          仅运行多线程测试\n";
    std::cout << "  --policy                Run QueueFullPolicy benchmark\n";
    std::cout << "                          运行 QueueFullPolicy 测试\n";
    std::cout << "  --no-spdlog             Skip spdlog comparison\n";
    std::cout << "                          跳过 spdlog 对比\n";
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
    bool sinkSpecified = false;
    bool threadSpecified = false;
    
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
        } else if (arg == "-t" || arg == "--threads") {
            if (i + 1 < argc) {
                config.numThreads = static_cast<size_t>(std::atol(argv[++i]));
            }
        } else if (arg == "-q" || arg == "--queue-size") {
            if (i + 1 < argc) {
                config.queueSize = static_cast<size_t>(std::atol(argv[++i]));
            }
        } else if (arg == "--null") {
            if (!sinkSpecified) {
                config.runNullSink = false;
                config.runFileSink = false;
                sinkSpecified = true;
            }
            config.runNullSink = true;
        } else if (arg == "--file") {
            if (!sinkSpecified) {
                config.runNullSink = false;
                config.runFileSink = false;
                sinkSpecified = true;
            }
            config.runFileSink = true;
        } else if (arg == "--single") {
            if (!threadSpecified) {
                config.runSingleThread = false;
                config.runMultiThread = false;
                threadSpecified = true;
            }
            config.runSingleThread = true;
        } else if (arg == "--multi") {
            if (!threadSpecified) {
                config.runSingleThread = false;
                config.runMultiThread = false;
                threadSpecified = true;
            }
            config.runMultiThread = true;
        } else if (arg == "--policy") {
            config.runQueuePolicyTest = true;
        } else if (arg == "--no-spdlog") {
            config.runSpdlogComparison = false;
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
    std::cout << "onePlog Async Mode Benchmark\n";
    std::cout << "onePlog 异步模式性能测试\n";
    std::cout << "========================================\n";
    std::cout << "Iterations: " << config.iterations << " / 迭代次数: " << config.iterations << "\n";
    std::cout << "Warmup: " << config.warmupIterations << " / 预热次数: " << config.warmupIterations << "\n";
    std::cout << "Threads: " << config.numThreads << " / 线程数: " << config.numThreads << "\n";
    std::cout << "Queue Size: " << config.queueSize << " / 队列大小: " << config.queueSize << "\n";
#ifdef HAS_SPDLOG
    std::cout << "spdlog comparison: " << (config.runSpdlogComparison ? "enabled" : "disabled") << "\n";
    std::cout << "spdlog 对比: " << (config.runSpdlogComparison ? "启用" : "禁用") << "\n";
#else
    std::cout << "spdlog comparison: not available (HAS_SPDLOG not defined)\n";
    std::cout << "spdlog 对比: 不可用（未定义 HAS_SPDLOG）\n";
    config.runSpdlogComparison = false;
#endif
    std::cout << "========================================\n";
    
    // ==============================================================================
    // NullSink Benchmarks / NullSink 基准测试
    // ==============================================================================
    
    if (config.runNullSink) {
        std::cout << "\n--- NullSink Async Benchmark / NullSink 异步基准测试 ---\n";
        
        // Single thread / 单线程
        if (config.runSingleThread) {
            std::cout << "\nRunning onePlog Async NullSink (single thread)...\n";
            std::cout << "运行 onePlog 异步 NullSink（单线程）...\n";
            auto oneplogResult = BenchmarkOneplogAsyncNullSinkSingleThread(
                config.iterations, config.warmupIterations, config.queueSize);
            PrintResult(oneplogResult);
            
#ifdef HAS_SPDLOG
            if (config.runSpdlogComparison) {
                std::cout << "\nRunning spdlog Async NullSink (single thread)...\n";
                std::cout << "运行 spdlog 异步 NullSink（单线程）...\n";
                auto spdlogResult = BenchmarkSpdlogAsyncNullSinkSingleThread(
                    config.iterations, config.warmupIterations, config.queueSize);
                PrintResult(spdlogResult);
                
                PrintComparison(oneplogResult, spdlogResult, "NullSink (1 thread)");
            }
#endif
        }
        
        // Multi thread / 多线程
        if (config.runMultiThread) {
            std::cout << "\nRunning onePlog Async NullSink (multi-thread)...\n";
            std::cout << "运行 onePlog 异步 NullSink（多线程）...\n";
            auto oneplogResult = BenchmarkOneplogAsyncNullSinkMultiThread(
                config.iterations, config.warmupIterations, config.numThreads, config.queueSize);
            PrintResult(oneplogResult);
            
#ifdef HAS_SPDLOG
            if (config.runSpdlogComparison) {
                std::cout << "\nRunning spdlog Async NullSink (multi-thread)...\n";
                std::cout << "运行 spdlog 异步 NullSink（多线程）...\n";
                auto spdlogResult = BenchmarkSpdlogAsyncNullSinkMultiThread(
                    config.iterations, config.warmupIterations, config.numThreads, config.queueSize);
                PrintResult(spdlogResult);
                
                PrintComparison(oneplogResult, spdlogResult, 
                    "NullSink (" + std::to_string(config.numThreads) + " threads)");
            }
#endif
        }
    }
    
    // ==============================================================================
    // FileSink Benchmarks / FileSink 基准测试
    // ==============================================================================
    
    if (config.runFileSink) {
        std::cout << "\n--- FileSink Async Benchmark / FileSink 异步基准测试 ---\n";
        
        // Single thread / 单线程
        if (config.runSingleThread) {
            std::cout << "\nRunning onePlog Async FileSink (single thread)...\n";
            std::cout << "运行 onePlog 异步 FileSink（单线程）...\n";
            auto oneplogResult = BenchmarkOneplogAsyncFileSinkSingleThread(
                config.iterations, config.warmupIterations, config.queueSize);
            PrintResult(oneplogResult);
            
#ifdef HAS_SPDLOG
            if (config.runSpdlogComparison) {
                std::cout << "\nRunning spdlog Async FileSink (single thread)...\n";
                std::cout << "运行 spdlog 异步 FileSink（单线程）...\n";
                auto spdlogResult = BenchmarkSpdlogAsyncFileSinkSingleThread(
                    config.iterations, config.warmupIterations, config.queueSize);
                PrintResult(spdlogResult);
                
                PrintComparison(oneplogResult, spdlogResult, "FileSink (1 thread)");
            }
#endif
        }
        
        // Multi thread / 多线程
        if (config.runMultiThread) {
            std::cout << "\nRunning onePlog Async FileSink (multi-thread)...\n";
            std::cout << "运行 onePlog 异步 FileSink（多线程）...\n";
            auto oneplogResult = BenchmarkOneplogAsyncFileSinkMultiThread(
                config.iterations, config.warmupIterations, config.numThreads, config.queueSize);
            PrintResult(oneplogResult);
        }
        
        // Cleanup test files / 清理测试文件
        std::remove(kTestFilePath);
#ifdef HAS_SPDLOG
        std::remove(kSpdlogTestFilePath);
#endif
    }
    
    // ==============================================================================
    // QueueFullPolicy Benchmarks / QueueFullPolicy 基准测试
    // ==============================================================================
    
    if (config.runQueuePolicyTest) {
        BenchmarkQueueFullPolicies(config.iterations, config.warmupIterations);
    }
    
    // ==============================================================================
    // Summary / 总结
    // ==============================================================================
    
    std::cout << "\n========================================\n";
    std::cout << "Benchmark Complete / 基准测试完成\n";
    std::cout << "========================================\n";
    
    return 0;
}
