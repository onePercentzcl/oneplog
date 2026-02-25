/**
 * @file benchmark_sync.cpp
 * @brief Synchronous mode performance benchmark for onePlog
 * @brief onePlog 同步模式性能测试
 *
 * This benchmark measures the performance of onePlog's synchronous logging mode
 * and compares it with spdlog (if available).
 *
 * 此基准测试测量 onePlog 同步日志模式的性能，并与 spdlog 进行对比（如果可用）。
 *
 * Test scenarios / 测试场景:
 * - NullSink: Measures pure logging overhead without I/O
 *   NullSink: 测量无 I/O 的纯日志开销
 * - FileSink: Measures logging with file I/O
 *   FileSink: 测量带文件 I/O 的日志性能
 * - ConsoleSink: Measures logging with console I/O
 *   ConsoleSink: 测量带控制台 I/O 的日志性能
 *
 * Metrics / 指标:
 * - Throughput (ops/sec): Operations per second
 *   吞吐量 (ops/sec): 每秒操作数
 * - Latency: Average, P50, P99, P999 percentiles
 *   延迟: 平均值、P50、P99、P999 百分位数
 *
 * @copyright Copyright (c) 2024 onePlog
 */

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
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>
#endif

namespace {

// ==============================================================================
// Configuration / 配置
// ==============================================================================

/// Default number of iterations / 默认迭代次数
constexpr size_t kDefaultIterations = 100000;

/// Default warmup iterations / 默认预热次数
constexpr size_t kDefaultWarmupIterations = 10000;

/// Test message for benchmarking / 用于基准测试的测试消息
/// Using simple message format consistent with old benchmark for fair comparison
/// 使用与旧基准测试一致的简单消息格式以进行公平对比
constexpr const char* kTestMessage = "Message {} value {}";

/// Test file path for file sink benchmarks / 文件 Sink 基准测试的测试文件路径
constexpr const char* kTestFilePath = "benchmark_sync_test.log";

/// spdlog test file path / spdlog 测试文件路径
constexpr const char* kSpdlogTestFilePath = "benchmark_sync_spdlog.log";

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
    bool runNullSink{true};                      ///< Run NullSink benchmark / 运行 NullSink 测试
    bool runFileSink{true};                      ///< Run FileSink benchmark / 运行 FileSink 测试
    bool runConsoleSink{false};                  ///< Run ConsoleSink benchmark / 运行 ConsoleSink 测试
    bool runSpdlogComparison{true};              ///< Run spdlog comparison / 运行 spdlog 对比
    bool verbose{false};                         ///< Verbose output / 详细输出
};

// ==============================================================================
// onePlog Sync Mode Benchmarks / onePlog 同步模式基准测试
// ==============================================================================

/**
 * @brief Benchmark onePlog sync mode with NullSink
 * @brief 使用 NullSink 测试 onePlog 同步模式
 *
 * NullSink discards all output, measuring pure logging overhead.
 * NullSink 丢弃所有输出，测量纯日志开销。
 */
oneplog::benchmark::BenchmarkResult BenchmarkOneplogSyncNullSink(
    size_t iterations, size_t warmupIterations) {
    
    // Configure logger with NullSink and MessageOnlyFormat for fair comparison
    // 配置使用 NullSink 和 MessageOnlyFormat 的日志器以进行公平对比
    using NullSinkBinding = oneplog::SinkBinding<oneplog::NullSinkType, oneplog::MessageOnlyFormat>;
    using NullSinkConfig = oneplog::LoggerConfig<
        oneplog::Mode::Sync,
        oneplog::Level::Info,
        false,  // EnableWFC
        true,   // EnableShadowTail
        true,   // UseFmt
        8192,   // HeapRingBufferCapacity
        4096,   // SharedRingBufferCapacity
        oneplog::QueueFullPolicy::DropNewest,
        oneplog::DefaultSharedMemoryName,
        10,     // PollTimeoutMs
        oneplog::SinkBindingList<NullSinkBinding>
    >;
    
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
    result.testName = "onePlog Sync NullSink";
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
 * @brief Benchmark onePlog sync mode with FileSink
 * @brief 使用 FileSink 测试 onePlog 同步模式
 *
 * FileSink writes to a file, measuring logging with file I/O overhead.
 * FileSink 写入文件，测量带文件 I/O 开销的日志性能。
 */
oneplog::benchmark::BenchmarkResult BenchmarkOneplogSyncFileSink(
    size_t iterations, size_t warmupIterations) {
    
    // Remove existing test file / 删除现有测试文件
    std::remove(kTestFilePath);
    
    // Configure logger with FileSink and MessageOnlyFormat for fair comparison
    // 配置使用 FileSink 和 MessageOnlyFormat 的日志器以进行公平对比
    using FileSinkBinding = oneplog::SinkBinding<oneplog::FileSinkType, oneplog::MessageOnlyFormat>;
    using FileSinkConfig = oneplog::LoggerConfig<
        oneplog::Mode::Sync,
        oneplog::Level::Info,
        false,  // EnableWFC
        true,   // EnableShadowTail
        true,   // UseFmt
        8192,   // HeapRingBufferCapacity
        4096,   // SharedRingBufferCapacity
        oneplog::QueueFullPolicy::DropNewest,
        oneplog::DefaultSharedMemoryName,
        10,     // PollTimeoutMs
        oneplog::SinkBindingList<FileSinkBinding>
    >;
    
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
    result.testName = "onePlog Sync FileSink";
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
 * @brief Benchmark onePlog sync mode with ConsoleSink
 * @brief 使用 ConsoleSink 测试 onePlog 同步模式
 *
 * ConsoleSink writes to stdout, measuring logging with console I/O overhead.
 * ConsoleSink 写入 stdout，测量带控制台 I/O 开销的日志性能。
 *
 * Note: Console output is redirected to /dev/null for accurate timing.
 * 注意：控制台输出重定向到 /dev/null 以获得准确的计时。
 */
oneplog::benchmark::BenchmarkResult BenchmarkOneplogSyncConsoleSink(
    size_t iterations, size_t warmupIterations) {
    
    // Configure logger with ConsoleSink and SimpleFormat
    // 配置使用 ConsoleSink 和 SimpleFormat 的日志器
    using ConsoleSinkBinding = oneplog::SinkBinding<oneplog::ConsoleSinkType, oneplog::SimpleFormat>;
    using ConsoleSinkConfig = oneplog::LoggerConfig<
        oneplog::Mode::Sync,
        oneplog::Level::Info,
        false,  // EnableWFC
        false,  // EnableShadowTail
        true,   // UseFmt
        8192,   // HeapRingBufferCapacity
        4096,   // SharedRingBufferCapacity
        oneplog::QueueFullPolicy::Block,
        oneplog::DefaultSharedMemoryName,
        10,     // PollTimeoutMs
        oneplog::SinkBindingList<ConsoleSinkBinding>
    >;
    
    oneplog::LoggerImpl<ConsoleSinkConfig> logger;
    
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
    result.testName = "onePlog Sync ConsoleSink";
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

// ==============================================================================
// spdlog Sync Mode Benchmarks / spdlog 同步模式基准测试
// ==============================================================================

#ifdef HAS_SPDLOG

/**
 * @brief Benchmark spdlog sync mode with null_sink
 * @brief 使用 null_sink 测试 spdlog 同步模式
 */
oneplog::benchmark::BenchmarkResult BenchmarkSpdlogSyncNullSink(
    size_t iterations, size_t warmupIterations) {
    
    // Create spdlog logger with null_sink (use _mt for fair comparison with oneplog)
    // 创建使用 null_sink 的 spdlog 日志器（使用 _mt 版本以与 oneplog 公平对比）
    auto nullSink = std::make_shared<spdlog::sinks::null_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("spdlog_null", nullSink);
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
    
    // Calculate results / 计算结果
    oneplog::benchmark::BenchmarkResult result;
    result.testName = "spdlog Sync NullSink";
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
 * @brief Benchmark spdlog sync mode with basic_file_sink
 * @brief 使用 basic_file_sink 测试 spdlog 同步模式
 */
oneplog::benchmark::BenchmarkResult BenchmarkSpdlogSyncFileSink(
    size_t iterations, size_t warmupIterations) {
    
    // Remove existing test file / 删除现有测试文件
    std::remove(kSpdlogTestFilePath);
    
    // Create spdlog logger with basic_file_sink (use _mt for fair comparison)
    // 创建使用 basic_file_sink 的 spdlog 日志器（使用 _mt 版本以公平对比）
    auto fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(kSpdlogTestFilePath, true);
    auto logger = std::make_shared<spdlog::logger>("spdlog_file", fileSink);
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
    
    // Calculate results / 计算结果
    oneplog::benchmark::BenchmarkResult result;
    result.testName = "spdlog Sync FileSink";
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
 * @brief Benchmark spdlog sync mode with stdout_sink
 * @brief 使用 stdout_sink 测试 spdlog 同步模式
 */
oneplog::benchmark::BenchmarkResult BenchmarkSpdlogSyncConsoleSink(
    size_t iterations, size_t warmupIterations) {
    
    // Create spdlog logger with stdout_sink (use _mt for fair comparison)
    // 创建使用 stdout_sink 的 spdlog 日志器（使用 _mt 版本以公平对比）
    auto consoleSink = std::make_shared<spdlog::sinks::stdout_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("spdlog_console", consoleSink);
    logger->set_level(spdlog::level::info);
    logger->set_pattern("[%H:%M:%S] [%l] %v");  // SimpleFormat equivalent / SimpleFormat 等效格式
    
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
    
    // Calculate results / 计算结果
    oneplog::benchmark::BenchmarkResult result;
    result.testName = "spdlog Sync ConsoleSink";
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
                     const std::string& sinkType) {
    std::cout << "\n========================================\n";
    std::cout << "Comparison: " << sinkType << "\n";
    std::cout << "对比: " << sinkType << "\n";
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
    std::cout << "  --null                  Run NullSink benchmark only\n";
    std::cout << "                          仅运行 NullSink 测试\n";
    std::cout << "  --file                  Run FileSink benchmark only\n";
    std::cout << "                          仅运行 FileSink 测试\n";
    std::cout << "  --console               Run ConsoleSink benchmark only\n";
    std::cout << "                          仅运行 ConsoleSink 测试\n";
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
        } else if (arg == "--null") {
            if (!sinkSpecified) {
                config.runNullSink = false;
                config.runFileSink = false;
                config.runConsoleSink = false;
                sinkSpecified = true;
            }
            config.runNullSink = true;
        } else if (arg == "--file") {
            if (!sinkSpecified) {
                config.runNullSink = false;
                config.runFileSink = false;
                config.runConsoleSink = false;
                sinkSpecified = true;
            }
            config.runFileSink = true;
        } else if (arg == "--console") {
            if (!sinkSpecified) {
                config.runNullSink = false;
                config.runFileSink = false;
                config.runConsoleSink = false;
                sinkSpecified = true;
            }
            config.runConsoleSink = true;
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
    std::cout << "onePlog Sync Mode Benchmark\n";
    std::cout << "onePlog 同步模式性能测试\n";
    std::cout << "========================================\n";
    std::cout << "Iterations: " << config.iterations << " / 迭代次数: " << config.iterations << "\n";
    std::cout << "Warmup: " << config.warmupIterations << " / 预热次数: " << config.warmupIterations << "\n";
#ifdef HAS_SPDLOG
    std::cout << "spdlog comparison: " << (config.runSpdlogComparison ? "enabled" : "disabled") << "\n";
    std::cout << "spdlog 对比: " << (config.runSpdlogComparison ? "启用" : "禁用") << "\n";
#else
    std::cout << "spdlog comparison: not available (HAS_SPDLOG not defined)\n";
    std::cout << "spdlog 对比: 不可用（未定义 HAS_SPDLOG）\n";
    config.runSpdlogComparison = false;
#endif
    std::cout << "========================================\n";
    
    // Store results for comparison / 存储结果以进行对比
    oneplog::benchmark::BenchmarkResult oneplogNullResult;
    oneplog::benchmark::BenchmarkResult oneplogFileResult;
    oneplog::benchmark::BenchmarkResult oneplogConsoleResult;
    
#ifdef HAS_SPDLOG
    oneplog::benchmark::BenchmarkResult spdlogNullResult;
    oneplog::benchmark::BenchmarkResult spdlogFileResult;
    oneplog::benchmark::BenchmarkResult spdlogConsoleResult;
#endif
    
    // ==============================================================================
    // NullSink Benchmarks / NullSink 基准测试
    // ==============================================================================
    
    if (config.runNullSink) {
        std::cout << "\n--- NullSink Benchmark / NullSink 基准测试 ---\n";
        
        std::cout << "Running onePlog NullSink benchmark...\n";
        std::cout << "运行 onePlog NullSink 基准测试...\n";
        oneplogNullResult = BenchmarkOneplogSyncNullSink(
            config.iterations, config.warmupIterations);
        PrintResult(oneplogNullResult);
        
#ifdef HAS_SPDLOG
        if (config.runSpdlogComparison) {
            std::cout << "\nRunning spdlog NullSink benchmark...\n";
            std::cout << "运行 spdlog NullSink 基准测试...\n";
            spdlogNullResult = BenchmarkSpdlogSyncNullSink(
                config.iterations, config.warmupIterations);
            PrintResult(spdlogNullResult);
            
            PrintComparison(oneplogNullResult, spdlogNullResult, "NullSink");
        }
#endif
    }
    
    // ==============================================================================
    // FileSink Benchmarks / FileSink 基准测试
    // ==============================================================================
    
    if (config.runFileSink) {
        std::cout << "\n--- FileSink Benchmark / FileSink 基准测试 ---\n";
        
        std::cout << "Running onePlog FileSink benchmark...\n";
        std::cout << "运行 onePlog FileSink 基准测试...\n";
        oneplogFileResult = BenchmarkOneplogSyncFileSink(
            config.iterations, config.warmupIterations);
        PrintResult(oneplogFileResult);
        
#ifdef HAS_SPDLOG
        if (config.runSpdlogComparison) {
            std::cout << "\nRunning spdlog FileSink benchmark...\n";
            std::cout << "运行 spdlog FileSink 基准测试...\n";
            spdlogFileResult = BenchmarkSpdlogSyncFileSink(
                config.iterations, config.warmupIterations);
            PrintResult(spdlogFileResult);
            
            PrintComparison(oneplogFileResult, spdlogFileResult, "FileSink");
        }
#endif
        
        // Cleanup test files / 清理测试文件
        std::remove(kTestFilePath);
#ifdef HAS_SPDLOG
        std::remove(kSpdlogTestFilePath);
#endif
    }
    
    // ==============================================================================
    // ConsoleSink Benchmarks / ConsoleSink 基准测试
    // ==============================================================================
    
    if (config.runConsoleSink) {
        std::cout << "\n--- ConsoleSink Benchmark / ConsoleSink 基准测试 ---\n";
        std::cout << "Note: Console output will be visible during benchmark.\n";
        std::cout << "注意: 基准测试期间控制台输出将可见。\n";
        
        std::cout << "Running onePlog ConsoleSink benchmark...\n";
        std::cout << "运行 onePlog ConsoleSink 基准测试...\n";
        oneplogConsoleResult = BenchmarkOneplogSyncConsoleSink(
            config.iterations, config.warmupIterations);
        PrintResult(oneplogConsoleResult);
        
#ifdef HAS_SPDLOG
        if (config.runSpdlogComparison) {
            std::cout << "\nRunning spdlog ConsoleSink benchmark...\n";
            std::cout << "运行 spdlog ConsoleSink 基准测试...\n";
            spdlogConsoleResult = BenchmarkSpdlogSyncConsoleSink(
                config.iterations, config.warmupIterations);
            PrintResult(spdlogConsoleResult);
            
            PrintComparison(oneplogConsoleResult, spdlogConsoleResult, "ConsoleSink");
        }
#endif
    }
    
    // ==============================================================================
    // Summary / 总结
    // ==============================================================================
    
    std::cout << "\n========================================\n";
    std::cout << "Benchmark Complete / 基准测试完成\n";
    std::cout << "========================================\n";
    
    return 0;
}
