/**
 * @file benchmark_contention_compare.cpp
 * @brief High contention benchmark: onePlog vs spdlog
 * @brief 高竞争场景基准测试：onePlog 与 spdlog 对比
 *
 * Tests shadow tail optimization effectiveness under high contention scenarios.
 * 测试 shadow tail 优化在高竞争场景下的效果。
 *
 * @copyright Copyright (c) 2024 onePlog
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

#include <oneplog/oneplog.hpp>

#ifdef HAS_SPDLOG
#include <spdlog/spdlog.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/async.h>
#endif

using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::nanoseconds;

// ==============================================================================
// Configuration / 配置
// ==============================================================================

struct Config {
    int iterations = 500000;
    int threads = 8;
    int warmup = 1000;
    int runs = 5;
    size_t queueSize = 1024;  // Small queue for contention / 小队列以产生竞争
};

// ==============================================================================
// Statistics / 统计
// ==============================================================================

struct Stats {
    double throughput = 0;
    double meanLatency = 0;
    double medianLatency = 0;
    double p99Latency = 0;
    double p999Latency = 0;
    uint64_t dropped = 0;
};

Stats CalcStats(std::vector<int64_t>& latencies, double totalMs, uint64_t dropped = 0) {
    Stats s;
    s.dropped = dropped;
    if (latencies.empty()) return s;
    
    std::sort(latencies.begin(), latencies.end());
    size_t n = latencies.size();
    
    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    s.meanLatency = sum / n;
    s.medianLatency = latencies[n / 2];
    s.p99Latency = latencies[static_cast<size_t>(n * 0.99)];
    s.p999Latency = latencies[static_cast<size_t>(n * 0.999)];
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

std::string FormatNum(double val, int width = 8) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1) << val;
    std::string s = oss.str();
    if (static_cast<int>(s.size()) < width) {
        s = std::string(width - s.size(), ' ') + s;
    }
    return s;
}

// ==============================================================================
// Multi-run Statistics / 多次运行统计
// ==============================================================================

struct MultiRunStats {
    double avgThroughput = 0;
    double stdDev = 0;
    double avgMedianLatency = 0;
    double avgP99Latency = 0;
    uint64_t totalDropped = 0;
};

MultiRunStats CalcMultiRunStats(const std::vector<Stats>& results) {
    MultiRunStats mrs;
    if (results.empty()) return mrs;
    
    size_t n = results.size();
    
    double sumThroughput = 0;
    double sumMedian = 0;
    double sumP99 = 0;
    
    for (const auto& s : results) {
        sumThroughput += s.throughput;
        sumMedian += s.medianLatency;
        sumP99 += s.p99Latency;
        mrs.totalDropped += s.dropped;
    }
    
    mrs.avgThroughput = sumThroughput / n;
    mrs.avgMedianLatency = sumMedian / n;
    mrs.avgP99Latency = sumP99 / n;
    
    double sqSum = 0;
    for (const auto& s : results) {
        sqSum += (s.throughput - mrs.avgThroughput) * (s.throughput - mrs.avgThroughput);
    }
    mrs.stdDev = std::sqrt(sqSum / n);
    
    return mrs;
}

// ==============================================================================
// Null Sink / 空输出
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

class SimpleFormat : public oneplog::Format {
public:
    std::string FormatEntry(const oneplog::LogEntry& entry) override {
        return entry.snapshot.FormatAll();
    }
    std::string FormatDirect(oneplog::Level, uint64_t, uint32_t, uint32_t,
                             const std::string& message) override {
        return message;
    }
#ifdef ONEPLOG_USE_FMT
    void FormatDirectToBuffer(fmt::memory_buffer& buffer, oneplog::Level, uint64_t,
                              uint32_t, uint32_t, std::string_view message) override {
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
// Test 1: MPSC High Contention (DropNewest)
// 测试 1：多生产者单消费者高竞争（丢弃最新）
// ==============================================================================

Stats BenchOneplogMPSC_DropNewest(const Config& cfg) {
    auto sink = std::make_shared<NullSink>();
    oneplog::Logger<oneplog::Mode::Async, oneplog::Level::Info, false> logger;
    logger.SetSink(sink);
    logger.SetFormat(std::make_shared<SimpleFormat>());
    
    oneplog::LoggerConfig logCfg;
    logCfg.heapRingBufferSize = cfg.queueSize;
    logCfg.queueFullPolicy = oneplog::QueueFullPolicy::DropNewest;
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
                logger.Info("T{} msg {} val {}", t, i, 3.14159);
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
    // Note: dropped count not easily accessible from Logger API
    return CalcStats(all, totalMs, 0);
}

Stats BenchOneplogMPSC_Block(const Config& cfg) {
    auto sink = std::make_shared<NullSink>();
    oneplog::Logger<oneplog::Mode::Async, oneplog::Level::Info, false> logger;
    logger.SetSink(sink);
    logger.SetFormat(std::make_shared<SimpleFormat>());
    
    oneplog::LoggerConfig logCfg;
    logCfg.heapRingBufferSize = cfg.queueSize;
    logCfg.queueFullPolicy = oneplog::QueueFullPolicy::Block;
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
                logger.Info("T{} msg {} val {}", t, i, 3.14159);
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
    return CalcStats(all, totalMs, 0);
}

#ifdef HAS_SPDLOG

Stats BenchSpdlogMPSC_DropNewest(const Config& cfg) {
    // spdlog uses overrun_oldest by default, we use discard for drop newest behavior
    spdlog::init_thread_pool(cfg.queueSize, 1);
    auto sink = std::make_shared<spdlog::sinks::null_sink_mt>();
    auto logger = std::make_shared<spdlog::async_logger>(
        "spdlog_drop", sink, spdlog::thread_pool(),
        spdlog::async_overflow_policy::overrun_oldest);  // Closest to DropNewest
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
                logger->info("T{} msg {} val {}", t, i, 3.14159);
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
    auto stats = CalcStats(all, totalMs, 0);  // spdlog doesn't expose drop count easily
    
    spdlog::shutdown();
    return stats;
}

Stats BenchSpdlogMPSC_Block(const Config& cfg) {
    spdlog::init_thread_pool(cfg.queueSize, 1);
    auto sink = std::make_shared<spdlog::sinks::null_sink_mt>();
    auto logger = std::make_shared<spdlog::async_logger>(
        "spdlog_block", sink, spdlog::thread_pool(),
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
                logger->info("T{} msg {} val {}", t, i, 3.14159);
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
    auto stats = CalcStats(all, totalMs, 0);
    
    spdlog::shutdown();
    return stats;
}

#endif

// ==============================================================================
// Test 2: Large Queue (normal operation)
// 测试 2：大队列（正常操作）
// ==============================================================================

Stats BenchOneplogLargeQueue(const Config& cfg) {
    auto sink = std::make_shared<NullSink>();
    oneplog::Logger<oneplog::Mode::Async, oneplog::Level::Info, false> logger;
    logger.SetSink(sink);
    logger.SetFormat(std::make_shared<SimpleFormat>());
    
    oneplog::LoggerConfig logCfg;
    logCfg.heapRingBufferSize = 1024 * 1024;  // 1M entries
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
                logger.Info("T{} msg {} val {}", t, i, 3.14159);
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
    return CalcStats(all, totalMs, 0);
}

#ifdef HAS_SPDLOG

Stats BenchSpdlogLargeQueue(const Config& cfg) {
    spdlog::init_thread_pool(1024 * 1024, 1);
    auto sink = std::make_shared<spdlog::sinks::null_sink_mt>();
    auto logger = std::make_shared<spdlog::async_logger>(
        "spdlog_large", sink, spdlog::thread_pool(),
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
                logger->info("T{} msg {} val {}", t, i, 3.14159);
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
    auto stats = CalcStats(all, totalMs, 0);
    
    spdlog::shutdown();
    return stats;
}

#endif

// ==============================================================================
// Main / 主函数
// ==============================================================================

template<typename BenchFunc>
MultiRunStats RunMultiple(BenchFunc func, int runs) {
    std::vector<Stats> results;
    results.reserve(runs);
    
    for (int r = 0; r < runs; ++r) {
        results.push_back(func());
    }
    
    return CalcMultiRunStats(results);
}

void PrintResult(const std::string& lib, const std::string& scenario, 
                 const MultiRunStats& mrs, bool showDropped = false) {
    std::cout << "  " << std::left << std::setw(10) << lib
              << std::setw(25) << scenario
              << std::right << std::setw(12) << FormatThroughput(mrs.avgThroughput)
              << " ± " << std::setw(8) << FormatThroughput(mrs.stdDev)
              << "  P50:" << FormatNum(mrs.avgMedianLatency, 6) << "ns"
              << "  P99:" << FormatNum(mrs.avgP99Latency, 8) << "ns";
    if (showDropped) {
        std::cout << "  dropped:" << mrs.totalDropped;
    }
    std::cout << "\n";
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
        } else if (arg == "-q" && i + 1 < argc) {
            cfg.queueSize = std::atoi(argv[++i]);
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "\nUsage: benchmark_contention_compare [options]\n";
            std::cout << "  -i <num>    Iterations (default: 500000)\n";
            std::cout << "  -t <num>    Threads (default: 8)\n";
            std::cout << "  -r <num>    Runs (default: 5)\n";
            std::cout << "  -q <num>    Queue size for contention tests (default: 1024)\n";
            return 0;
        }
    }

    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "  High Contention Benchmark: onePlog vs spdlog\n";
    std::cout << "  高竞争场景基准测试：onePlog 与 spdlog 对比\n";
    std::cout << "================================================================\n";
    std::cout << "\n";
    std::cout << "  Config / 配置:\n";
    std::cout << "    Iterations / 迭代次数:     " << cfg.iterations << "\n";
    std::cout << "    Threads / 线程数:          " << cfg.threads << "\n";
    std::cout << "    Queue size / 队列大小:     " << cfg.queueSize << "\n";
    std::cout << "    Runs / 运行次数:           " << cfg.runs << "\n";
    std::cout << "    Shadow tail interval:      " 
              << oneplog::RingBufferHeader::kShadowTailUpdateInterval << "\n";
    std::cout << "\n";

#ifndef HAS_SPDLOG
    std::cout << "  WARNING: spdlog not found! Only running onePlog benchmarks.\n";
    std::cout << "  Install with: xmake require spdlog\n\n";
#endif

    std::cout << "  " << std::string(100, '=') << "\n";
    std::cout << "  Test 1: Small Queue High Contention (queue=" << cfg.queueSize << ")\n";
    std::cout << "  测试 1：小队列高竞争场景\n";
    std::cout << "  " << std::string(100, '-') << "\n";

    // onePlog DropNewest
    std::cout << "\n  Running onePlog (DropNewest)...\n";
    auto oneplogDrop = RunMultiple([&]() { return BenchOneplogMPSC_DropNewest(cfg); }, cfg.runs);
    PrintResult("onePlog", "DropNewest", oneplogDrop, true);

    // onePlog Block
    std::cout << "  Running onePlog (Block)...\n";
    auto oneplogBlock = RunMultiple([&]() { return BenchOneplogMPSC_Block(cfg); }, cfg.runs);
    PrintResult("onePlog", "Block", oneplogBlock);

#ifdef HAS_SPDLOG
    // spdlog overrun_oldest (similar to DropNewest)
    std::cout << "  Running spdlog (overrun_oldest)...\n";
    auto spdlogDrop = RunMultiple([&]() { return BenchSpdlogMPSC_DropNewest(cfg); }, cfg.runs);
    PrintResult("spdlog", "overrun_oldest", spdlogDrop);

    // spdlog Block
    std::cout << "  Running spdlog (Block)...\n";
    auto spdlogBlock = RunMultiple([&]() { return BenchSpdlogMPSC_Block(cfg); }, cfg.runs);
    PrintResult("spdlog", "Block", spdlogBlock);
#endif

    std::cout << "\n  " << std::string(100, '=') << "\n";
    std::cout << "  Test 2: Large Queue Normal Operation (queue=1M)\n";
    std::cout << "  测试 2：大队列正常操作场景\n";
    std::cout << "  " << std::string(100, '-') << "\n";

    std::cout << "\n  Running onePlog (Large Queue)...\n";
    auto oneplogLarge = RunMultiple([&]() { return BenchOneplogLargeQueue(cfg); }, cfg.runs);
    PrintResult("onePlog", "Large Queue (1M)", oneplogLarge);

#ifdef HAS_SPDLOG
    std::cout << "  Running spdlog (Large Queue)...\n";
    auto spdlogLarge = RunMultiple([&]() { return BenchSpdlogLargeQueue(cfg); }, cfg.runs);
    PrintResult("spdlog", "Large Queue (1M)", spdlogLarge);
#endif

    // Summary
    std::cout << "\n  " << std::string(100, '=') << "\n";
    std::cout << "  Summary / 总结\n";
    std::cout << "  " << std::string(100, '-') << "\n";

#ifdef HAS_SPDLOG
    auto ratio = [](double a, double b) -> double {
        if (b == 0) return 0;
        return (a / b - 1.0) * 100.0;
    };

    std::cout << std::fixed << std::setprecision(1);
    
    std::cout << "\n  High Contention (Small Queue) / 高竞争（小队列）:\n";
    std::cout << "    DropNewest: onePlog is " 
              << std::abs(ratio(oneplogDrop.avgThroughput, spdlogDrop.avgThroughput)) << "% "
              << (oneplogDrop.avgThroughput >= spdlogDrop.avgThroughput ? "faster" : "slower")
              << " (P99: " << FormatNum(oneplogDrop.avgP99Latency) << " vs " 
              << FormatNum(spdlogDrop.avgP99Latency) << " ns)\n";
    
    std::cout << "    Block:      onePlog is "
              << std::abs(ratio(oneplogBlock.avgThroughput, spdlogBlock.avgThroughput)) << "% "
              << (oneplogBlock.avgThroughput >= spdlogBlock.avgThroughput ? "faster" : "slower")
              << " (P99: " << FormatNum(oneplogBlock.avgP99Latency) << " vs "
              << FormatNum(spdlogBlock.avgP99Latency) << " ns)\n";

    std::cout << "\n  Normal Operation (Large Queue) / 正常操作（大队列）:\n";
    std::cout << "    onePlog is "
              << std::abs(ratio(oneplogLarge.avgThroughput, spdlogLarge.avgThroughput)) << "% "
              << (oneplogLarge.avgThroughput >= spdlogLarge.avgThroughput ? "faster" : "slower")
              << " (P99: " << FormatNum(oneplogLarge.avgP99Latency) << " vs "
              << FormatNum(spdlogLarge.avgP99Latency) << " ns)\n";
#else
    std::cout << "\n  onePlog Results (spdlog not available for comparison):\n";
    std::cout << "    DropNewest: " << FormatThroughput(oneplogDrop.avgThroughput) << " ops/sec\n";
    std::cout << "    Block:      " << FormatThroughput(oneplogBlock.avgThroughput) << " ops/sec\n";
    std::cout << "    Large Queue:" << FormatThroughput(oneplogLarge.avgThroughput) << " ops/sec\n";
#endif

    std::cout << "\n================================================================\n";
    std::cout << "  Benchmark complete! / 测试完成!\n";
    std::cout << "================================================================\n\n";

    return 0;
}
