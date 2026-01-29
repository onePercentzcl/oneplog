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

#include <oneplog/oneplog.hpp>

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
    int runs = 100;  // 运行次数 / Number of runs
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

// ==============================================================================
// Multi-run Statistics / 多次运行统计
// ==============================================================================

struct MultiRunStats {
    double avgThroughput = 0;
    double minThroughput = 0;
    double maxThroughput = 0;
    double stdDev = 0;
};

MultiRunStats CalcMultiRunStats(const std::vector<double>& throughputs) {
    MultiRunStats mrs;
    if (throughputs.empty()) return mrs;
    
    size_t n = throughputs.size();
    double sum = std::accumulate(throughputs.begin(), throughputs.end(), 0.0);
    mrs.avgThroughput = sum / n;
    
    mrs.minThroughput = *std::min_element(throughputs.begin(), throughputs.end());
    mrs.maxThroughput = *std::max_element(throughputs.begin(), throughputs.end());
    
    // Calculate standard deviation / 计算标准差
    double sqSum = 0;
    for (double t : throughputs) {
        sqSum += (t - mrs.avgThroughput) * (t - mrs.avgThroughput);
    }
    mrs.stdDev = std::sqrt(sqSum / n);
    
    return mrs;
}

// ==============================================================================
// Null Sink for onePlog / onePlog 空输出
// ==============================================================================

class NullSink : public oneplog::Sink {
public:
    void Write(const std::string&) override {}
    void Write(std::string_view) override {}
    void WriteBatch(const std::vector<std::string>&) override {}
    void Flush() override {}
    void Close() override {}
    bool HasError() const override { return false; }
    std::string GetLastError() const override { return ""; }
};

// ==============================================================================
// Simple Format (message only, like spdlog's %v)
// 简单格式化器（仅消息，类似 spdlog 的 %v）
// ==============================================================================

class SimpleFormat : public oneplog::Format {
public:
    std::string FormatEntry(const oneplog::LogEntry& entry) override {
        return entry.snapshot.FormatAll();
    }

    std::string FormatDirect(oneplog::Level /*level*/, uint64_t /*timestamp*/,
                             uint32_t /*threadId*/, uint32_t /*processId*/,
                             const std::string& message) override {
        return message;
    }

#ifdef ONEPLOG_USE_FMT
    void FormatDirectToBuffer(fmt::memory_buffer& buffer,
                              oneplog::Level /*level*/, uint64_t /*timestamp*/,
                              uint32_t /*threadId*/, uint32_t /*processId*/,
                              std::string_view message) override {
        // Zero-copy: just append message to buffer
        // 零拷贝：直接将消息追加到缓冲区
        buffer.append(message);
    }
#endif

    oneplog::FormatRequirements GetRequirements() const override {
        oneplog::FormatRequirements req;
        req.needsTimestamp = false;
        req.needsLevel = false;
        req.needsThreadId = false;
        req.needsProcessId = false;
        req.needsSourceLocation = false;
        return req;
    }
};

// ==============================================================================
// onePlog Benchmarks / onePlog 测试
// ==============================================================================

Stats BenchOneplogSync(const Config& cfg) {
    auto sink = std::make_shared<NullSink>();
    oneplog::Logger<oneplog::Mode::Sync, oneplog::Level::Info, false> logger;
    logger.SetSink(sink);
    logger.SetFormat(std::make_shared<SimpleFormat>());
    logger.Init();

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

Stats BenchOneplogAsync(const Config& cfg) {
    auto sink = std::make_shared<NullSink>();
    oneplog::Logger<oneplog::Mode::Async, oneplog::Level::Info, false> logger;
    logger.SetSink(sink);
    logger.SetFormat(std::make_shared<SimpleFormat>());
    
    oneplog::LoggerConfig logCfg;
    logCfg.heapRingBufferSize = 1024 * 1024;
    logger.Init(logCfg);

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

Stats BenchOneplogAsyncMT(const Config& cfg) {
    auto sink = std::make_shared<NullSink>();
    oneplog::Logger<oneplog::Mode::Async, oneplog::Level::Info, false> logger;
    logger.SetSink(sink);
    logger.SetFormat(std::make_shared<SimpleFormat>());
    
    oneplog::LoggerConfig logCfg;
    logCfg.heapRingBufferSize = 1024 * 1024;
    logger.Init(logCfg);

    for (int i = 0; i < cfg.warmup; ++i) {
        logger.Info("Warmup {}", i);
    }
    logger.Flush();

    int iterPerThread = cfg.iterations / cfg.threads;
    std::vector<std::vector<int64_t>> threadLatencies(cfg.threads);
    std::atomic<bool> go{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < cfg.threads; ++t) {
        threadLatencies[t].reserve(iterPerThread);
        threads.emplace_back([&, t]() {
            while (!go.load()) std::this_thread::yield();
            for (int i = 0; i < iterPerThread; ++i) {
                auto t1 = Clock::now();
                logger.Info("Thread {} msg {} val {}", t, i, 3.14159);
                auto t2 = Clock::now();
                threadLatencies[t].push_back(
                    std::chrono::duration_cast<Duration>(t2 - t1).count());
            }
        });
    }

    auto start = Clock::now();
    go = true;
    for (auto& th : threads) th.join();
    auto end = Clock::now();

    logger.Flush();
    logger.Shutdown();

    std::vector<int64_t> all;
    for (auto& tl : threadLatencies) {
        all.insert(all.end(), tl.begin(), tl.end());
    }

    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalcStats(all, totalMs);
}

// ==============================================================================
// File Output Benchmarks / 文件输出测试
// ==============================================================================

Stats BenchOneplogSyncFile(const Config& cfg, const std::string& filename) {
    std::remove(filename.c_str());
    auto sink = std::make_shared<oneplog::FileSink>(filename);
    oneplog::Logger<oneplog::Mode::Sync, oneplog::Level::Info, false> logger;
    logger.SetSink(sink);
    logger.SetFormat(std::make_shared<SimpleFormat>());
    logger.Init();

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

Stats BenchOneplogAsyncFile(const Config& cfg, const std::string& filename) {
    std::remove(filename.c_str());
    auto sink = std::make_shared<oneplog::FileSink>(filename);
    oneplog::Logger<oneplog::Mode::Async, oneplog::Level::Info, false> logger;
    logger.SetSink(sink);
    logger.SetFormat(std::make_shared<SimpleFormat>());
    
    oneplog::LoggerConfig logCfg;
    logCfg.heapRingBufferSize = 1024 * 1024;
    logger.Init(logCfg);

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

Stats BenchOneplogAsyncFileMT(const Config& cfg, const std::string& filename) {
    std::remove(filename.c_str());
    auto sink = std::make_shared<oneplog::FileSink>(filename);
    oneplog::Logger<oneplog::Mode::Async, oneplog::Level::Info, false> logger;
    logger.SetSink(sink);
    logger.SetFormat(std::make_shared<SimpleFormat>());
    
    oneplog::LoggerConfig logCfg;
    logCfg.heapRingBufferSize = 1024 * 1024;
    logger.Init(logCfg);

    for (int i = 0; i < cfg.warmup; ++i) {
        logger.Info("Warmup {}", i);
    }
    logger.Flush();

    int iterPerThread = cfg.iterations / cfg.threads;
    std::vector<std::vector<int64_t>> threadLatencies(cfg.threads);
    std::atomic<bool> go{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < cfg.threads; ++t) {
        threadLatencies[t].reserve(iterPerThread);
        threads.emplace_back([&, t]() {
            while (!go.load()) std::this_thread::yield();
            for (int i = 0; i < iterPerThread; ++i) {
                auto t1 = Clock::now();
                logger.Info("Thread {} msg {} val {}", t, i, 3.14159);
                auto t2 = Clock::now();
                threadLatencies[t].push_back(
                    std::chrono::duration_cast<Duration>(t2 - t1).count());
            }
        });
    }

    auto start = Clock::now();
    go = true;
    for (auto& th : threads) th.join();
    auto end = Clock::now();

    logger.Flush();
    logger.Shutdown();

    std::vector<int64_t> all;
    for (auto& tl : threadLatencies) {
        all.insert(all.end(), tl.begin(), tl.end());
    }

    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalcStats(all, totalMs);
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

Stats BenchSpdlogAsync(const Config& cfg) {
    spdlog::init_thread_pool(1024 * 1024, 1);
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

    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    auto stats = CalcStats(latencies, totalMs);
    
    spdlog::shutdown();
    return stats;
}

Stats BenchSpdlogAsyncMT(const Config& cfg) {
    spdlog::init_thread_pool(1024 * 1024, 1);
    auto sink = std::make_shared<spdlog::sinks::null_sink_mt>();
    auto logger = std::make_shared<spdlog::async_logger>(
        "spdlog_async_mt", sink, spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);
    logger->set_level(spdlog::level::info);
    logger->set_pattern("%v");

    for (int i = 0; i < cfg.warmup; ++i) {
        logger->info("Warmup {}", i);
    }
    logger->flush();

    int iterPerThread = cfg.iterations / cfg.threads;
    std::vector<std::vector<int64_t>> threadLatencies(cfg.threads);
    std::atomic<bool> go{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < cfg.threads; ++t) {
        threadLatencies[t].reserve(iterPerThread);
        threads.emplace_back([&, t]() {
            while (!go.load()) std::this_thread::yield();
            for (int i = 0; i < iterPerThread; ++i) {
                auto t1 = Clock::now();
                logger->info("Thread {} msg {} val {}", t, i, 3.14159);
                auto t2 = Clock::now();
                threadLatencies[t].push_back(
                    std::chrono::duration_cast<Duration>(t2 - t1).count());
            }
        });
    }

    auto start = Clock::now();
    go = true;
    for (auto& th : threads) th.join();
    auto end = Clock::now();

    logger->flush();

    std::vector<int64_t> all;
    for (auto& tl : threadLatencies) {
        all.insert(all.end(), tl.begin(), tl.end());
    }

    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    auto stats = CalcStats(all, totalMs);
    
    spdlog::shutdown();
    return stats;
}

// File output benchmarks / 文件输出测试
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

Stats BenchSpdlogAsyncFile(const Config& cfg, const std::string& filename) {
    std::remove(filename.c_str());
    spdlog::init_thread_pool(1024 * 1024, 1);
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
    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    auto stats = CalcStats(latencies, totalMs);
    
    spdlog::shutdown();
    return stats;
}

Stats BenchSpdlogAsyncFileMT(const Config& cfg, const std::string& filename) {
    std::remove(filename.c_str());
    spdlog::init_thread_pool(1024 * 1024, 1);
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename, true);
    auto logger = std::make_shared<spdlog::async_logger>(
        "spdlog_async_file_mt", sink, spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);
    logger->set_level(spdlog::level::info);
    logger->set_pattern("%v");

    for (int i = 0; i < cfg.warmup; ++i) {
        logger->info("Warmup {}", i);
    }
    logger->flush();

    int iterPerThread = cfg.iterations / cfg.threads;
    std::vector<std::vector<int64_t>> threadLatencies(cfg.threads);
    std::atomic<bool> go{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < cfg.threads; ++t) {
        threadLatencies[t].reserve(iterPerThread);
        threads.emplace_back([&, t]() {
            while (!go.load()) std::this_thread::yield();
            for (int i = 0; i < iterPerThread; ++i) {
                auto t1 = Clock::now();
                logger->info("Thread {} msg {} val {}", t, i, 3.14159);
                auto t2 = Clock::now();
                threadLatencies[t].push_back(
                    std::chrono::duration_cast<Duration>(t2 - t1).count());
            }
        });
    }

    auto start = Clock::now();
    go = true;
    for (auto& th : threads) th.join();
    auto end = Clock::now();

    logger->flush();

    std::vector<int64_t> all;
    for (auto& tl : threadLatencies) {
        all.insert(all.end(), tl.begin(), tl.end());
    }

    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    auto stats = CalcStats(all, totalMs);
    
    spdlog::shutdown();
    return stats;
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

// Run benchmark multiple times and collect stats / 多次运行测试并收集统计
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
        } else if (arg == "-t" && i + 1 < argc) {
            cfg.threads = std::atoi(argv[++i]);
        } else if (arg == "-r" && i + 1 < argc) {
            cfg.runs = std::atoi(argv[++i]);
        }
    }

    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "     onePlog vs spdlog Performance Comparison\n";
    std::cout << "     onePlog 与 spdlog 性能对比\n";
    std::cout << "================================================================\n";
    std::cout << "\n";
    std::cout << "  Config / 配置:\n";
    std::cout << "    Iterations / 迭代次数: " << cfg.iterations << "\n";
    std::cout << "    Threads / 线程数:      " << cfg.threads << "\n";
    std::cout << "    Warmup / 预热:         " << cfg.warmup << "\n";
    std::cout << "    Runs / 运行次数:       " << cfg.runs << "\n";
    std::cout << "\n";

#ifndef HAS_SPDLOG
    std::cout << "  WARNING: spdlog not found! Only running onePlog benchmarks.\n";
    std::cout << "  警告: 未找到 spdlog！仅运行 onePlog 测试。\n";
    std::cout << "  Install with: xmake require spdlog\n\n";
#endif

    std::cout << "  " << std::left << std::setw(12) << "Library"
              << std::setw(20) << "Mode"
              << std::right << std::setw(24) << "Throughput (avg ± stddev)\n";
    std::cout << "  " << std::string(60, '-') << "\n";

    // onePlog tests
    std::cout << "\n  [onePlog]\n";
    
    auto oneplogSync = RunMultiple([&]() { return BenchOneplogSync(cfg); }, cfg.runs);
    PrintResult("onePlog", "Sync (1 Thread)", oneplogSync);
    
    auto oneplogAsync = RunMultiple([&]() { return BenchOneplogAsync(cfg); }, cfg.runs);
    PrintResult("onePlog", "Async (1 Thread)", oneplogAsync);
    
    auto oneplogAsyncMT = RunMultiple([&]() { return BenchOneplogAsyncMT(cfg); }, cfg.runs);
    PrintResult("onePlog", "Async (" + std::to_string(cfg.threads) + " Threads)", oneplogAsyncMT);

    // File output tests / 文件输出测试
    std::cout << "\n  [onePlog - File Output / 文件输出]\n";
    
    auto oneplogSyncFile = RunMultipleWithArg([&](const std::string& f) { return BenchOneplogSyncFile(cfg, f); }, 
                                               cfg.runs, "/tmp/oneplog_sync.log");
    PrintResult("onePlog", "Sync File", oneplogSyncFile);
    
    auto oneplogAsyncFile = RunMultipleWithArg([&](const std::string& f) { return BenchOneplogAsyncFile(cfg, f); },
                                                cfg.runs, "/tmp/oneplog_async.log");
    PrintResult("onePlog", "Async File", oneplogAsyncFile);
    
    auto oneplogAsyncFileMT = RunMultipleWithArg([&](const std::string& f) { return BenchOneplogAsyncFileMT(cfg, f); },
                                                  cfg.runs, "/tmp/oneplog_async_mt.log");
    PrintResult("onePlog", "Async File (" + std::to_string(cfg.threads) + "T)", oneplogAsyncFileMT);

#ifdef HAS_SPDLOG
    // spdlog tests
    std::cout << "\n  [spdlog]\n";
    
    auto spdlogSync = RunMultiple([&]() { return BenchSpdlogSync(cfg); }, cfg.runs);
    PrintResult("spdlog", "Sync (1 Thread)", spdlogSync);
    
    auto spdlogAsync = RunMultiple([&]() { return BenchSpdlogAsync(cfg); }, cfg.runs);
    PrintResult("spdlog", "Async (1 Thread)", spdlogAsync);
    
    auto spdlogAsyncMT = RunMultiple([&]() { return BenchSpdlogAsyncMT(cfg); }, cfg.runs);
    PrintResult("spdlog", "Async (" + std::to_string(cfg.threads) + " Threads)", spdlogAsyncMT);

    // File output tests / 文件输出测试
    std::cout << "\n  [spdlog - File Output / 文件输出]\n";
    
    auto spdlogSyncFile = RunMultipleWithArg([&](const std::string& f) { return BenchSpdlogSyncFile(cfg, f); },
                                              cfg.runs, "/tmp/spdlog_sync.log");
    PrintResult("spdlog", "Sync File", spdlogSyncFile);
    
    auto spdlogAsyncFile = RunMultipleWithArg([&](const std::string& f) { return BenchSpdlogAsyncFile(cfg, f); },
                                               cfg.runs, "/tmp/spdlog_async.log");
    PrintResult("spdlog", "Async File", spdlogAsyncFile);
    
    auto spdlogAsyncFileMT = RunMultipleWithArg([&](const std::string& f) { return BenchSpdlogAsyncFileMT(cfg, f); },
                                                 cfg.runs, "/tmp/spdlog_async_mt.log");
    PrintResult("spdlog", "Async File (" + std::to_string(cfg.threads) + "T)", spdlogAsyncFileMT);

    // Summary comparison
    std::cout << "\n";
    std::cout << "  " << std::string(60, '=') << "\n";
    std::cout << "  Summary / 总结 (based on " << cfg.runs << " runs average):\n";
    std::cout << "  " << std::string(60, '-') << "\n";
    
    auto ratio = [](double a, double b) {
        if (b == 0) return 0.0;
        return (a / b - 1.0) * 100.0;
    };
    
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "  Sync Mode:   onePlog " 
              << (oneplogSync.avgThroughput >= spdlogSync.avgThroughput ? "faster" : "slower")
              << " by " << std::abs(ratio(oneplogSync.avgThroughput, spdlogSync.avgThroughput)) << "%\n";
    std::cout << "  Async Mode:  onePlog "
              << (oneplogAsync.avgThroughput >= spdlogAsync.avgThroughput ? "faster" : "slower")
              << " by " << std::abs(ratio(oneplogAsync.avgThroughput, spdlogAsync.avgThroughput)) << "%\n";
    std::cout << "  Async " << cfg.threads << "T:   onePlog "
              << (oneplogAsyncMT.avgThroughput >= spdlogAsyncMT.avgThroughput ? "faster" : "slower")
              << " by " << std::abs(ratio(oneplogAsyncMT.avgThroughput, spdlogAsyncMT.avgThroughput)) << "%\n";
    
    std::cout << "\n  File Output / 文件输出:\n";
    std::cout << "  Sync File:   onePlog "
              << (oneplogSyncFile.avgThroughput >= spdlogSyncFile.avgThroughput ? "faster" : "slower")
              << " by " << std::abs(ratio(oneplogSyncFile.avgThroughput, spdlogSyncFile.avgThroughput)) << "%\n";
    std::cout << "  Async File:  onePlog "
              << (oneplogAsyncFile.avgThroughput >= spdlogAsyncFile.avgThroughput ? "faster" : "slower")
              << " by " << std::abs(ratio(oneplogAsyncFile.avgThroughput, spdlogAsyncFile.avgThroughput)) << "%\n";
    std::cout << "  Async File " << cfg.threads << "T: onePlog "
              << (oneplogAsyncFileMT.avgThroughput >= spdlogAsyncFileMT.avgThroughput ? "faster" : "slower")
              << " by " << std::abs(ratio(oneplogAsyncFileMT.avgThroughput, spdlogAsyncFileMT.avgThroughput)) << "%\n";
#endif

    std::cout << "\n================================================================\n";
    std::cout << "  Benchmark complete! / 测试完成!\n";
    std::cout << "================================================================\n\n";

    return 0;
}
