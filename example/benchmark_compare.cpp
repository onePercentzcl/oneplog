/**
 * @file benchmark_compare.cpp
 * @brief Performance comparison between onePlog and spdlog
 * @brief onePlog 与 spdlog 性能对比测试
 */

#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <vector>
#include <numeric>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <sstream>
#include <fstream>

#include <oneplog/fast_logger_v2.hpp>
// Note: We use fast_logger_v2.hpp which is the redesigned FastLogger
// 注意：我们使用 fast_logger_v2.hpp，这是重新设计的 FastLogger

#ifdef HAS_SPDLOG
#include <spdlog/spdlog.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#endif

using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::nanoseconds;

// ==============================================================================
// Configuration / 配置
// ==============================================================================

struct Config {
    int iterations = 500000;
    int threads = 4;
    int warmup = 10000;
    int runs = 100;
};

// ==============================================================================
// Statistics / 统计
// ==============================================================================

struct Stats {
    double throughput = 0;
    double meanLatency = 0;
    double p99Latency = 0;
};

Stats CalcStats(std::vector<int64_t>& latencies, double totalMs) {
    Stats s;
    if (latencies.empty()) return s;
    std::sort(latencies.begin(), latencies.end());
    size_t n = latencies.size();
    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    s.meanLatency = sum / n;
    s.p99Latency = latencies[static_cast<size_t>(n * 0.99)];
    s.throughput = (n / totalMs) * 1000.0;
    return s;
}

std::string FormatThroughput(double val) {
    std::ostringstream oss;
    if (val >= 1000000) {
        oss << std::fixed << std::setprecision(2) << (val / 1000000.0) << " M";
    } else if (val >= 1000) {
        oss << std::fixed << std::setprecision(2) << (val / 1000.0) << " K";
    } else {
        oss << std::fixed << std::setprecision(0) << val;
    }
    return oss.str();
}

struct MultiRunStats {
    double avgThroughput = 0;
    double stdDev = 0;
};

MultiRunStats CalcMultiRunStats(const std::vector<double>& throughputs) {
    MultiRunStats mrs;
    if (throughputs.empty()) return mrs;
    size_t n = throughputs.size();
    double sum = std::accumulate(throughputs.begin(), throughputs.end(), 0.0);
    mrs.avgThroughput = sum / n;
    double sqSum = 0;
    for (double t : throughputs) {
        sqSum += (t - mrs.avgThroughput) * (t - mrs.avgThroughput);
    }
    mrs.stdDev = std::sqrt(sqSum / n);
    return mrs;
}

// ==============================================================================
// onePlog Benchmarks using FastLoggerV2 / 使用 FastLoggerV2 的 onePlog 测试
// ==============================================================================

// Custom config for sync benchmark with NullSink
using SyncNullConfig = oneplog::FastLoggerConfig<
    oneplog::Mode::Sync,
    oneplog::Level::Info,
    false, true, true,
    8192, 4096, oneplog::QueueFullPolicy::DropNewest,
    oneplog::DefaultSharedMemoryName, 10,
    oneplog::SinkBindingList<oneplog::SinkBinding<oneplog::NullSinkType, oneplog::MessageOnlyFormat>>
>;

// Custom config for async benchmark with NullSink
using AsyncNullConfig = oneplog::FastLoggerConfig<
    oneplog::Mode::Async,
    oneplog::Level::Info,
    false, true, true,
    8192, 4096, oneplog::QueueFullPolicy::DropNewest,
    oneplog::DefaultSharedMemoryName, 10,
    oneplog::SinkBindingList<oneplog::SinkBinding<oneplog::NullSinkType, oneplog::MessageOnlyFormat>>
>;

// Custom config for sync benchmark with FileSink
using SyncFileConfig = oneplog::FastLoggerConfig<
    oneplog::Mode::Sync,
    oneplog::Level::Info,
    false, true, true,
    8192, 4096, oneplog::QueueFullPolicy::DropNewest,
    oneplog::DefaultSharedMemoryName, 10,
    oneplog::SinkBindingList<oneplog::SinkBinding<oneplog::FileSinkType, oneplog::MessageOnlyFormat>>
>;

// Custom config for async benchmark with FileSink
using AsyncFileConfig = oneplog::FastLoggerConfig<
    oneplog::Mode::Async,
    oneplog::Level::Info,
    false, true, true,
    8192, 4096, oneplog::QueueFullPolicy::DropNewest,
    oneplog::DefaultSharedMemoryName, 10,
    oneplog::SinkBindingList<oneplog::SinkBinding<oneplog::FileSinkType, oneplog::MessageOnlyFormat>>
>;

// Custom config for MProc benchmark with NullSink
struct BenchMProcSharedMemoryName {
    static constexpr const char* value = "/oneplog_bench_mproc";
};

using MProcNullConfig = oneplog::FastLoggerConfig<
    oneplog::Mode::MProc,
    oneplog::Level::Info,
    false, true, true,
    8192, 65536, oneplog::QueueFullPolicy::Block,
    BenchMProcSharedMemoryName, 10,
    oneplog::SinkBindingList<oneplog::SinkBinding<oneplog::NullSinkType, oneplog::MessageOnlyFormat>>
>;

Stats BenchOneplogSync(const Config& cfg) {
    // Use FastLoggerV2 with NullSink for sync benchmark
    // 使用 FastLoggerV2 + NullSink 进行同步测试
    oneplog::FastLoggerV2<SyncNullConfig> logger;

    for (int i = 0; i < cfg.warmup; ++i) {
        logger.Info("Warmup {}", i);
    }

    std::vector<int64_t> latencies;
    latencies.reserve(cfg.iterations);

    auto start = Clock::now();
    for (int i = 0; i < cfg.iterations; ++i) {
        auto t1 = Clock::now();
        logger.Info("Message {} value {}", i, 3.14159);
        auto t2 = Clock::now();
        latencies.push_back(std::chrono::duration_cast<Duration>(t2 - t1).count());
    }
    auto end = Clock::now();

    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalcStats(latencies, totalMs);
}

Stats BenchOneplogSyncFile(const Config& cfg, const std::string& filename) {
    std::remove(filename.c_str());
    // Use FastLoggerV2 with FileSink for file benchmark
    // 使用 FastLoggerV2 + FileSink 进行文件测试
    oneplog::SinkBindingList<oneplog::SinkBinding<oneplog::FileSinkType, oneplog::MessageOnlyFormat>> sinks(
        oneplog::SinkBinding<oneplog::FileSinkType, oneplog::MessageOnlyFormat>(
            oneplog::FileSinkType(filename.c_str())
        )
    );
    oneplog::FastLoggerV2<SyncFileConfig> logger(std::move(sinks));

    for (int i = 0; i < cfg.warmup; ++i) {
        logger.Info("Warmup {}", i);
    }
    logger.Flush();

    std::vector<int64_t> latencies;
    latencies.reserve(cfg.iterations);

    auto start = Clock::now();
    for (int i = 0; i < cfg.iterations; ++i) {
        auto t1 = Clock::now();
        logger.Info("Message {} value {}", i, 3.14159);
        auto t2 = Clock::now();
        latencies.push_back(std::chrono::duration_cast<Duration>(t2 - t1).count());
    }
    auto end = Clock::now();

    logger.Flush();
    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalcStats(latencies, totalMs);
}

Stats BenchOneplogAsync(const Config& cfg) {
    // Use FastLoggerV2 with NullSink for async benchmark
    // 使用 FastLoggerV2 + NullSink 进行异步测试
    oneplog::FastLoggerV2<AsyncNullConfig> logger;

    for (int i = 0; i < cfg.warmup; ++i) {
        logger.Info("Warmup {} {}", i, 3.14159);
    }
    logger.Flush();

    std::vector<int64_t> latencies;
    latencies.reserve(cfg.iterations);

    auto start = Clock::now();
    for (int i = 0; i < cfg.iterations; ++i) {
        auto t1 = Clock::now();
        logger.Info("Message {} value {}", i, 3.14159);
        auto t2 = Clock::now();
        latencies.push_back(std::chrono::duration_cast<Duration>(t2 - t1).count());
    }
    auto end = Clock::now();

    logger.Flush();
    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalcStats(latencies, totalMs);
}

Stats BenchOneplogAsyncFile(const Config& cfg, const std::string& filename) {
    std::remove(filename.c_str());
    // Use FastLoggerV2 with FileSink for async file benchmark
    // 使用 FastLoggerV2 + FileSink 进行异步文件测试
    oneplog::SinkBindingList<oneplog::SinkBinding<oneplog::FileSinkType, oneplog::MessageOnlyFormat>> sinks(
        oneplog::SinkBinding<oneplog::FileSinkType, oneplog::MessageOnlyFormat>(
            oneplog::FileSinkType(filename.c_str())
        )
    );
    oneplog::FastLoggerV2<AsyncFileConfig> logger(std::move(sinks));

    for (int i = 0; i < cfg.warmup; ++i) {
        logger.Info("Warmup {}", i);
    }
    logger.Flush();

    std::vector<int64_t> latencies;
    latencies.reserve(cfg.iterations);

    auto start = Clock::now();
    for (int i = 0; i < cfg.iterations; ++i) {
        auto t1 = Clock::now();
        logger.Info("Message {} value {}", i, 3.14159);
        auto t2 = Clock::now();
        latencies.push_back(std::chrono::duration_cast<Duration>(t2 - t1).count());
    }
    auto end = Clock::now();

    logger.Flush();
    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalcStats(latencies, totalMs);
}

// ==============================================================================
// onePlog MProc Benchmarks using FastLoggerV2 / 使用 FastLoggerV2 的 onePlog 多进程模式测试
// ==============================================================================

Stats BenchOneplogMProc(const Config& cfg) {
    // Use FastLoggerV2 with MProc mode for benchmark
    // 使用 FastLoggerV2 的 MProc 模式进行测试
    oneplog::FastLoggerV2<MProcNullConfig> logger;

    for (int i = 0; i < cfg.warmup; ++i) {
        logger.Info("Warmup {}", i);
    }
    logger.Flush();

    std::vector<int64_t> latencies;
    latencies.reserve(cfg.iterations);

    auto start = Clock::now();
    for (int i = 0; i < cfg.iterations; ++i) {
        auto t1 = Clock::now();
        logger.Info("Message {} value {}", i, 3.14159);
        auto t2 = Clock::now();
        latencies.push_back(std::chrono::duration_cast<Duration>(t2 - t1).count());
    }
    auto end = Clock::now();

    logger.Flush();
    logger.Shutdown();
    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalcStats(latencies, totalMs);
}

// ==============================================================================
// spdlog Benchmarks / spdlog 测试
// ==============================================================================

#ifdef HAS_SPDLOG

Stats BenchSpdlogSync(const Config& cfg) {
    auto sink = std::make_shared<spdlog::sinks::null_sink_mt>();
    auto logger = std::make_shared<spdlog::logger>("spdlog_sync", sink);
    logger->set_level(spdlog::level::info);
    logger->set_pattern("%v");

    for (int i = 0; i < cfg.warmup; ++i) {
        logger->info("Warmup {}", i);
    }

    std::vector<int64_t> latencies;
    latencies.reserve(cfg.iterations);

    auto start = Clock::now();
    for (int i = 0; i < cfg.iterations; ++i) {
        auto t1 = Clock::now();
        logger->info("Message {} value {}", i, 3.14159);
        auto t2 = Clock::now();
        latencies.push_back(std::chrono::duration_cast<Duration>(t2 - t1).count());
    }
    auto end = Clock::now();

    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalcStats(latencies, totalMs);
}

Stats BenchSpdlogSyncFile(const Config& cfg, const std::string& filename) {
    std::remove(filename.c_str());
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename, true);
    auto logger = std::make_shared<spdlog::logger>("spdlog_sync_file", sink);
    logger->set_level(spdlog::level::info);
    logger->set_pattern("%v");

    for (int i = 0; i < cfg.warmup; ++i) {
        logger->info("Warmup {}", i);
    }
    logger->flush();

    std::vector<int64_t> latencies;
    latencies.reserve(cfg.iterations);

    auto start = Clock::now();
    for (int i = 0; i < cfg.iterations; ++i) {
        auto t1 = Clock::now();
        logger->info("Message {} value {}", i, 3.14159);
        auto t2 = Clock::now();
        latencies.push_back(std::chrono::duration_cast<Duration>(t2 - t1).count());
    }
    auto end = Clock::now();

    logger->flush();
    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalcStats(latencies, totalMs);
}

Stats BenchSpdlogAsync(const Config& cfg) {
    spdlog::init_thread_pool(8192, 1);
    auto sink = std::make_shared<spdlog::sinks::null_sink_mt>();
    auto logger = std::make_shared<spdlog::async_logger>(
        "spdlog_async", sink, spdlog::thread_pool(), 
        spdlog::async_overflow_policy::block);
    logger->set_level(spdlog::level::info);
    logger->set_pattern("%v");

    for (int i = 0; i < cfg.warmup; ++i) {
        logger->info("Warmup {}", i);
    }
    logger->flush();

    std::vector<int64_t> latencies;
    latencies.reserve(cfg.iterations);

    auto start = Clock::now();
    for (int i = 0; i < cfg.iterations; ++i) {
        auto t1 = Clock::now();
        logger->info("Message {} value {}", i, 3.14159);
        auto t2 = Clock::now();
        latencies.push_back(std::chrono::duration_cast<Duration>(t2 - t1).count());
    }
    auto end = Clock::now();

    logger->flush();
    spdlog::shutdown();
    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalcStats(latencies, totalMs);
}

Stats BenchSpdlogAsyncFile(const Config& cfg, const std::string& filename) {
    std::remove(filename.c_str());
    spdlog::init_thread_pool(8192, 1);
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename, true);
    auto logger = std::make_shared<spdlog::async_logger>(
        "spdlog_async_file", sink, spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);
    logger->set_level(spdlog::level::info);
    logger->set_pattern("%v");

    for (int i = 0; i < cfg.warmup; ++i) {
        logger->info("Warmup {}", i);
    }
    logger->flush();

    std::vector<int64_t> latencies;
    latencies.reserve(cfg.iterations);

    auto start = Clock::now();
    for (int i = 0; i < cfg.iterations; ++i) {
        auto t1 = Clock::now();
        logger->info("Message {} value {}", i, 3.14159);
        auto t2 = Clock::now();
        latencies.push_back(std::chrono::duration_cast<Duration>(t2 - t1).count());
    }
    auto end = Clock::now();

    logger->flush();
    spdlog::shutdown();
    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalcStats(latencies, totalMs);
}

#endif

// ==============================================================================
// Main / 主函数
// ==============================================================================

void PrintResult(const std::string& lib, const std::string& mode, const MultiRunStats& mrs) {
    std::cout << "  " << std::left << std::setw(12) << lib
              << std::setw(20) << mode
              << std::right << std::setw(10) << FormatThroughput(mrs.avgThroughput)
              << " ± " << std::setw(6) << FormatThroughput(mrs.stdDev) << " ops/sec\n";
}

template<typename BenchFunc>
MultiRunStats RunMultiple(BenchFunc func, int runs) {
    std::vector<double> throughputs;
    throughputs.reserve(runs);
    for (int r = 0; r < runs; ++r) {
        Stats s = func();
        throughputs.push_back(s.throughput);
    }
    return CalcMultiRunStats(throughputs);
}

template<typename BenchFunc>
MultiRunStats RunMultipleWithArg(BenchFunc func, int runs, const std::string& arg) {
    std::vector<double> throughputs;
    throughputs.reserve(runs);
    for (int r = 0; r < runs; ++r) {
        Stats s = func(arg);
        throughputs.push_back(s.throughput);
    }
    return CalcMultiRunStats(throughputs);
}

int main(int argc, char* argv[]) {
    Config cfg;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-i" && i + 1 < argc) {
            cfg.iterations = std::atoi(argv[++i]);
        } else if (arg == "-r" && i + 1 < argc) {
            cfg.runs = std::atoi(argv[++i]);
        }
    }

    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "     onePlog FastLogger vs spdlog Performance Comparison\n";
    std::cout << "     onePlog FastLogger 与 spdlog 性能对比\n";
    std::cout << "================================================================\n";
    std::cout << "\n";
    std::cout << "  Config / 配置:\n";
    std::cout << "    Iterations / 迭代次数: " << cfg.iterations << "\n";
    std::cout << "    Warmup / 预热:         " << cfg.warmup << "\n";
    std::cout << "    Runs / 运行次数:       " << cfg.runs << "\n";
    std::cout << "\n";

#ifndef HAS_SPDLOG
    std::cout << "  WARNING: spdlog not found! Only running onePlog benchmarks.\n";
    std::cout << "  警告: 未找到 spdlog！仅运行 onePlog 测试。\n\n";
#endif

    std::cout << "  " << std::left << std::setw(12) << "Library"
              << std::setw(20) << "Mode"
              << std::right << std::setw(24) << "Throughput (avg ± stddev)\n";
    std::cout << "  " << std::string(60, '-') << "\n";

    // onePlog FastLogger tests
    std::cout << "\n  [onePlog FastLogger - Sync]\n";
    
    auto oneplogSync = RunMultiple([&]() { return BenchOneplogSync(cfg); }, cfg.runs);
    PrintResult("onePlog", "Sync (NullSink)", oneplogSync);
    
    auto oneplogSyncFile = RunMultipleWithArg(
        [&](const std::string& f) { return BenchOneplogSyncFile(cfg, f); }, 
        cfg.runs, "/tmp/oneplog_sync.log");
    PrintResult("onePlog", "Sync (FileSink)", oneplogSyncFile);

    std::cout << "\n  [onePlog FastLogger - Async]\n";
    
    auto oneplogAsync = RunMultiple([&]() { return BenchOneplogAsync(cfg); }, cfg.runs);
    PrintResult("onePlog", "Async (NullSink)", oneplogAsync);
    
    auto oneplogAsyncFile = RunMultipleWithArg(
        [&](const std::string& f) { return BenchOneplogAsyncFile(cfg, f); },
        cfg.runs, "/tmp/oneplog_async.log");
    PrintResult("onePlog", "Async (FileSink)", oneplogAsyncFile);

    std::cout << "\n  [onePlog Logger - MProc]\n";
    
    auto oneplogMProc = RunMultiple([&]() { return BenchOneplogMProc(cfg); }, cfg.runs);
    PrintResult("onePlog", "MProc (NullSink)", oneplogMProc);

#ifdef HAS_SPDLOG
    // spdlog tests
    std::cout << "\n  [spdlog - Sync]\n";
    
    auto spdlogSync = RunMultiple([&]() { return BenchSpdlogSync(cfg); }, cfg.runs);
    PrintResult("spdlog", "Sync (NullSink)", spdlogSync);
    
    auto spdlogSyncFile = RunMultipleWithArg(
        [&](const std::string& f) { return BenchSpdlogSyncFile(cfg, f); },
        cfg.runs, "/tmp/spdlog_sync.log");
    PrintResult("spdlog", "Sync (FileSink)", spdlogSyncFile);

    std::cout << "\n  [spdlog - Async]\n";
    
    auto spdlogAsync = RunMultiple([&]() { return BenchSpdlogAsync(cfg); }, cfg.runs);
    PrintResult("spdlog", "Async (NullSink)", spdlogAsync);
    
    auto spdlogAsyncFile = RunMultipleWithArg(
        [&](const std::string& f) { return BenchSpdlogAsyncFile(cfg, f); },
        cfg.runs, "/tmp/spdlog_async.log");
    PrintResult("spdlog", "Async (FileSink)", spdlogAsyncFile);

    // Summary
    std::cout << "\n  " << std::string(60, '=') << "\n";
    std::cout << "  Summary / 总结:\n";
    std::cout << "  " << std::string(60, '-') << "\n";
    
    auto ratio = [](double a, double b) {
        if (b == 0) return 0.0;
        return (a / b - 1.0) * 100.0;
    };
    
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "  Sync NullSink:   onePlog " 
              << (oneplogSync.avgThroughput >= spdlogSync.avgThroughput ? "faster" : "slower")
              << " by " << std::abs(ratio(oneplogSync.avgThroughput, spdlogSync.avgThroughput)) << "%\n";
    std::cout << "  Sync FileSink:   onePlog "
              << (oneplogSyncFile.avgThroughput >= spdlogSyncFile.avgThroughput ? "faster" : "slower")
              << " by " << std::abs(ratio(oneplogSyncFile.avgThroughput, spdlogSyncFile.avgThroughput)) << "%\n";
    std::cout << "  Async NullSink:  onePlog "
              << (oneplogAsync.avgThroughput >= spdlogAsync.avgThroughput ? "faster" : "slower")
              << " by " << std::abs(ratio(oneplogAsync.avgThroughput, spdlogAsync.avgThroughput)) << "%\n";
    std::cout << "  Async FileSink:  onePlog "
              << (oneplogAsyncFile.avgThroughput >= spdlogAsyncFile.avgThroughput ? "faster" : "slower")
              << " by " << std::abs(ratio(oneplogAsyncFile.avgThroughput, spdlogAsyncFile.avgThroughput)) << "%\n";
#endif

    std::cout << "\n================================================================\n";
    std::cout << "  Benchmark complete! / 测试完成!\n";
    std::cout << "================================================================\n\n";

    return 0;
}
