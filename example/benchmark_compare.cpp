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
    auto logger = std::make_shared<oneplog::Logger>("oneplog_sync", oneplog::Mode::Sync);
    logger->SetSink(sink);
    logger->SetFormat(std::make_shared<SimpleFormat>());
    logger->SetLevel(oneplog::Level::Info);
    logger->Init();

    for (int i = 0; i < cfg.warmup; ++i) {
        logger->Info("Warmup {}", i);
    }

    std::vector<int64_t> latencies;
    latencies.reserve(cfg.iterations);

    auto start = Clock::now();
    for (int i = 0; i < cfg.iterations; ++i) {
        auto t1 = Clock::now();
        logger->Info("Message {} value {}", i, 3.14159);
        auto t2 = Clock::now();
        latencies.push_back(std::chrono::duration_cast<Duration>(t2 - t1).count());
    }
    auto end = Clock::now();

    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalcStats(latencies, totalMs);
}

Stats BenchOneplogAsync(const Config& cfg) {
    auto sink = std::make_shared<NullSink>();
    auto logger = std::make_shared<oneplog::Logger>("oneplog_async", oneplog::Mode::Async);
    logger->SetSink(sink);
    logger->SetFormat(std::make_shared<SimpleFormat>());
    logger->SetLevel(oneplog::Level::Info);
    
    oneplog::LoggerConfig logCfg;
    logCfg.mode = oneplog::Mode::Async;
    logCfg.heapRingBufferSize = 1024 * 1024;
    logger->Init(logCfg);

    for (int i = 0; i < cfg.warmup; ++i) {
        logger->Info("Warmup {}", i);
    }
    logger->Flush();

    std::vector<int64_t> latencies;
    latencies.reserve(cfg.iterations);

    auto start = Clock::now();
    for (int i = 0; i < cfg.iterations; ++i) {
        auto t1 = Clock::now();
        logger->Info("Message {} value {}", i, 3.14159);
        auto t2 = Clock::now();
        latencies.push_back(std::chrono::duration_cast<Duration>(t2 - t1).count());
    }
    auto end = Clock::now();

    logger->Flush();
    logger->Shutdown();

    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalcStats(latencies, totalMs);
}

Stats BenchOneplogAsyncMT(const Config& cfg) {
    auto sink = std::make_shared<NullSink>();
    auto logger = std::make_shared<oneplog::Logger>("oneplog_async_mt", oneplog::Mode::Async);
    logger->SetSink(sink);
    logger->SetFormat(std::make_shared<SimpleFormat>());
    logger->SetLevel(oneplog::Level::Info);
    
    oneplog::LoggerConfig logCfg;
    logCfg.mode = oneplog::Mode::Async;
    logCfg.heapRingBufferSize = 1024 * 1024;
    logger->Init(logCfg);

    for (int i = 0; i < cfg.warmup; ++i) {
        logger->Info("Warmup {}", i);
    }
    logger->Flush();

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
                logger->Info("Thread {} msg {} val {}", t, i, 3.14159);
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

    logger->Flush();
    logger->Shutdown();

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
    auto logger = std::make_shared<oneplog::Logger>("oneplog_sync_file", oneplog::Mode::Sync);
    logger->SetSink(sink);
    logger->SetFormat(std::make_shared<SimpleFormat>());
    logger->SetLevel(oneplog::Level::Info);
    logger->Init();

    for (int i = 0; i < cfg.warmup; ++i) {
        logger->Info("Warmup {}", i);
    }
    logger->Flush();

    std::vector<int64_t> latencies;
    latencies.reserve(cfg.iterations);

    auto start = Clock::now();
    for (int i = 0; i < cfg.iterations; ++i) {
        auto t1 = Clock::now();
        logger->Info("Message {} value {}", i, 3.14159);
        auto t2 = Clock::now();
        latencies.push_back(std::chrono::duration_cast<Duration>(t2 - t1).count());
    }
    auto end = Clock::now();

    logger->Flush();
    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalcStats(latencies, totalMs);
}

Stats BenchOneplogAsyncFile(const Config& cfg, const std::string& filename) {
    std::remove(filename.c_str());
    auto sink = std::make_shared<oneplog::FileSink>(filename);
    auto logger = std::make_shared<oneplog::Logger>("oneplog_async_file", oneplog::Mode::Async);
    logger->SetSink(sink);
    logger->SetFormat(std::make_shared<SimpleFormat>());
    logger->SetLevel(oneplog::Level::Info);
    
    oneplog::LoggerConfig logCfg;
    logCfg.mode = oneplog::Mode::Async;
    logCfg.heapRingBufferSize = 1024 * 1024;
    logger->Init(logCfg);

    for (int i = 0; i < cfg.warmup; ++i) {
        logger->Info("Warmup {}", i);
    }
    logger->Flush();

    std::vector<int64_t> latencies;
    latencies.reserve(cfg.iterations);

    auto start = Clock::now();
    for (int i = 0; i < cfg.iterations; ++i) {
        auto t1 = Clock::now();
        logger->Info("Message {} value {}", i, 3.14159);
        auto t2 = Clock::now();
        latencies.push_back(std::chrono::duration_cast<Duration>(t2 - t1).count());
    }
    auto end = Clock::now();

    logger->Flush();
    logger->Shutdown();

    double totalMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalcStats(latencies, totalMs);
}

Stats BenchOneplogAsyncFileMT(const Config& cfg, const std::string& filename) {
    std::remove(filename.c_str());
    auto sink = std::make_shared<oneplog::FileSink>(filename);
    auto logger = std::make_shared<oneplog::Logger>("oneplog_async_file_mt", oneplog::Mode::Async);
    logger->SetSink(sink);
    logger->SetFormat(std::make_shared<SimpleFormat>());
    logger->SetLevel(oneplog::Level::Info);
    
    oneplog::LoggerConfig logCfg;
    logCfg.mode = oneplog::Mode::Async;
    logCfg.heapRingBufferSize = 1024 * 1024;
    logger->Init(logCfg);

    for (int i = 0; i < cfg.warmup; ++i) {
        logger->Info("Warmup {}", i);
    }
    logger->Flush();

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
                logger->Info("Thread {} msg {} val {}", t, i, 3.14159);
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

    logger->Flush();
    logger->Shutdown();

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

void PrintResult(const std::string& lib, const std::string& mode, const Stats& s) {
    std::cout << "  " << std::left << std::setw(12) << lib
              << std::setw(20) << mode
              << std::right << std::setw(12) << FormatThroughput(s.throughput) << " ops/sec"
              << std::setw(10) << std::fixed << std::setprecision(0) << s.meanLatency << " ns"
              << std::setw(10) << s.p99Latency << " ns\n";
}

int main(int argc, char* argv[]) {
    Config cfg;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-i" && i + 1 < argc) {
            cfg.iterations = std::atoi(argv[++i]);
        } else if (arg == "-t" && i + 1 < argc) {
            cfg.threads = std::atoi(argv[++i]);
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
    std::cout << "\n";

#ifndef HAS_SPDLOG
    std::cout << "  WARNING: spdlog not found! Only running onePlog benchmarks.\n";
    std::cout << "  警告: 未找到 spdlog！仅运行 onePlog 测试。\n";
    std::cout << "  Install with: xmake require spdlog\n\n";
#endif

    std::cout << "  " << std::left << std::setw(12) << "Library"
              << std::setw(20) << "Mode"
              << std::right << std::setw(18) << "Throughput"
              << std::setw(10) << "Mean"
              << std::setw(10) << "P99\n";
    std::cout << "  " << std::string(70, '-') << "\n";

    // onePlog tests
    std::cout << "\n  [onePlog]\n";
    
    std::cout << "  Running Sync...          \r" << std::flush;
    auto oneplogSync = BenchOneplogSync(cfg);
    PrintResult("onePlog", "Sync (1 Thread)", oneplogSync);
    
    std::cout << "  Running Async...         \r" << std::flush;
    auto oneplogAsync = BenchOneplogAsync(cfg);
    PrintResult("onePlog", "Async (1 Thread)", oneplogAsync);
    
    std::cout << "  Running Async MT...      \r" << std::flush;
    auto oneplogAsyncMT = BenchOneplogAsyncMT(cfg);
    PrintResult("onePlog", "Async (" + std::to_string(cfg.threads) + " Threads)", oneplogAsyncMT);

    // File output tests / 文件输出测试
    std::cout << "\n  [onePlog - File Output / 文件输出]\n";
    
    std::cout << "  Running Sync File...     \r" << std::flush;
    auto oneplogSyncFile = BenchOneplogSyncFile(cfg, "/tmp/oneplog_sync.log");
    PrintResult("onePlog", "Sync File", oneplogSyncFile);
    
    std::cout << "  Running Async File...    \r" << std::flush;
    auto oneplogAsyncFile = BenchOneplogAsyncFile(cfg, "/tmp/oneplog_async.log");
    PrintResult("onePlog", "Async File", oneplogAsyncFile);
    
    std::cout << "  Running Async File MT... \r" << std::flush;
    auto oneplogAsyncFileMT = BenchOneplogAsyncFileMT(cfg, "/tmp/oneplog_async_mt.log");
    PrintResult("onePlog", "Async File (" + std::to_string(cfg.threads) + "T)", oneplogAsyncFileMT);

#ifdef HAS_SPDLOG
    // spdlog tests
    std::cout << "\n  [spdlog]\n";
    
    std::cout << "  Running Sync...          \r" << std::flush;
    auto spdlogSync = BenchSpdlogSync(cfg);
    PrintResult("spdlog", "Sync (1 Thread)", spdlogSync);
    
    std::cout << "  Running Async...         \r" << std::flush;
    auto spdlogAsync = BenchSpdlogAsync(cfg);
    PrintResult("spdlog", "Async (1 Thread)", spdlogAsync);
    
    std::cout << "  Running Async MT...      \r" << std::flush;
    auto spdlogAsyncMT = BenchSpdlogAsyncMT(cfg);
    PrintResult("spdlog", "Async (" + std::to_string(cfg.threads) + " Threads)", spdlogAsyncMT);

    // File output tests / 文件输出测试
    std::cout << "\n  [spdlog - File Output / 文件输出]\n";
    
    std::cout << "  Running Sync File...     \r" << std::flush;
    auto spdlogSyncFile = BenchSpdlogSyncFile(cfg, "/tmp/spdlog_sync.log");
    PrintResult("spdlog", "Sync File", spdlogSyncFile);
    
    std::cout << "  Running Async File...    \r" << std::flush;
    auto spdlogAsyncFile = BenchSpdlogAsyncFile(cfg, "/tmp/spdlog_async.log");
    PrintResult("spdlog", "Async File", spdlogAsyncFile);
    
    std::cout << "  Running Async File MT... \r" << std::flush;
    auto spdlogAsyncFileMT = BenchSpdlogAsyncFileMT(cfg, "/tmp/spdlog_async_mt.log");
    PrintResult("spdlog", "Async File (" + std::to_string(cfg.threads) + "T)", spdlogAsyncFileMT);

    // Summary comparison
    std::cout << "\n";
    std::cout << "  " << std::string(70, '=') << "\n";
    std::cout << "  Summary / 总结:\n";
    std::cout << "  " << std::string(70, '-') << "\n";
    
    auto ratio = [](double a, double b) {
        if (b == 0) return 0.0;
        return (a / b - 1.0) * 100.0;
    };
    
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "  Sync Mode:   onePlog " 
              << (oneplogSync.throughput >= spdlogSync.throughput ? "faster" : "slower")
              << " by " << std::abs(ratio(oneplogSync.throughput, spdlogSync.throughput)) << "%\n";
    std::cout << "  Async Mode:  onePlog "
              << (oneplogAsync.throughput >= spdlogAsync.throughput ? "faster" : "slower")
              << " by " << std::abs(ratio(oneplogAsync.throughput, spdlogAsync.throughput)) << "%\n";
    std::cout << "  Async " << cfg.threads << "T:   onePlog "
              << (oneplogAsyncMT.throughput >= spdlogAsyncMT.throughput ? "faster" : "slower")
              << " by " << std::abs(ratio(oneplogAsyncMT.throughput, spdlogAsyncMT.throughput)) << "%\n";
    
    std::cout << "\n  File Output / 文件输出:\n";
    std::cout << "  Sync File:   onePlog "
              << (oneplogSyncFile.throughput >= spdlogSyncFile.throughput ? "faster" : "slower")
              << " by " << std::abs(ratio(oneplogSyncFile.throughput, spdlogSyncFile.throughput)) << "%\n";
    std::cout << "  Async File:  onePlog "
              << (oneplogAsyncFile.throughput >= spdlogAsyncFile.throughput ? "faster" : "slower")
              << " by " << std::abs(ratio(oneplogAsyncFile.throughput, spdlogAsyncFile.throughput)) << "%\n";
    std::cout << "  Async File " << cfg.threads << "T: onePlog "
              << (oneplogAsyncFileMT.throughput >= spdlogAsyncFileMT.throughput ? "faster" : "slower")
              << " by " << std::abs(ratio(oneplogAsyncFileMT.throughput, spdlogAsyncFileMT.throughput)) << "%\n";
#endif

    std::cout << "\n================================================================\n";
    std::cout << "  Benchmark complete! / 测试完成!\n";
    std::cout << "================================================================\n\n";

    return 0;
}
