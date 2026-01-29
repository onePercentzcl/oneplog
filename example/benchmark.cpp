/**
 * @file benchmark.cpp
 * @brief Performance benchmark for onePlog
 * @brief onePlog 性能测试程序
 *
 * This benchmark measures logging throughput and latency across different modes.
 * 此基准测试测量不同模式下的日志吞吐量和延迟。
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
#include <mutex>

#include <oneplog/oneplog.hpp>

#if defined(__unix__) || defined(__APPLE__)
#define ONEPLOG_HAS_FORK 1
#include <unistd.h>
#include <sys/wait.h>
#endif

using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::nanoseconds;

// ==============================================================================
// Benchmark Configuration / 基准测试配置
// ==============================================================================

struct BenchmarkConfig {
    int warmupIterations = 10000;     // Warmup iterations / 预热迭代次数
    int iterations = 1000000;         // Test iterations / 测试迭代次数
    int threads = 4;                  // Number of threads / 线程数
    int processes = 4;                // Number of processes / 进程数
    bool verbose = false;             // Verbose output / 详细输出
};

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
    
    // Mean / 平均值
    double sum = std::accumulate(latencies.begin(), latencies.end(), 0.0);
    stats.mean = sum / n;

    // Median / 中位数
    stats.median = latencies[n / 2];

    // Stddev / 标准差
    double sqSum = 0;
    for (auto v : latencies) {
        sqSum += (v - stats.mean) * (v - stats.mean);
    }
    stats.stddev = std::sqrt(sqSum / n);

    // Min/Max / 最小/最大值
    stats.min = latencies.front();
    stats.max = latencies.back();

    // Percentiles / 百分位数
    stats.p50 = latencies[static_cast<size_t>(n * 0.50)];
    stats.p90 = latencies[static_cast<size_t>(n * 0.90)];
    stats.p99 = latencies[static_cast<size_t>(n * 0.99)];
    stats.p999 = latencies[static_cast<size_t>(n * 0.999)];

    // Throughput / 吞吐量
    stats.throughput = (n / totalTimeMs) * 1000.0;

    return stats;
}

// Helper: format number with padding / 辅助函数：格式化数字并填充
std::string FormatNum(double val, int width, int precision = 1) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << val;
    std::string s = oss.str();
    if (static_cast<int>(s.size()) < width) {
        s = std::string(width - s.size(), ' ') + s;
    }
    return s;
}

// Helper: format throughput with M/K suffix / 辅助函数：使用 M/K 后缀格式化吞吐量
std::string FormatThroughput(double val, int width = 10) {
    std::ostringstream oss;
    if (val >= 1000000) {
        oss << std::fixed << std::setprecision(2) << (val / 1000000.0) << " M";
    } else if (val >= 1000) {
        oss << std::fixed << std::setprecision(2) << (val / 1000.0) << " K";
    } else {
        oss << std::fixed << std::setprecision(0) << val << "  ";
    }
    std::string s = oss.str();
    if (static_cast<int>(s.size()) < width) {
        s = std::string(width - s.size(), ' ') + s;
    }
    return s;
}

void PrintStats(const std::string& nameEn, const std::string& nameZh, const Statistics& stats) {
    std::cout << "\n";
    std::cout << "  " << nameEn << " / " << nameZh << "\n";
    std::cout << "  ----------------------------------------------------------------\n";
    std::cout << std::fixed;
    std::cout << "  Throughput / 吞吐量:    " << FormatThroughput(stats.throughput) << " ops/sec\n";
    std::cout << "  ----------------------------------------------------------------\n";
    std::cout << "  Latency / 延迟 (ns):\n";
    std::cout << "    Mean   / 平均值:      " << FormatNum(stats.mean, 12) << "\n";
    std::cout << "    Median / 中位数:      " << FormatNum(stats.median, 12) << "\n";
    std::cout << "    Stddev / 标准差:      " << FormatNum(stats.stddev, 12) << "\n";
    std::cout << "    Min    / 最小值:      " << FormatNum(stats.min, 12) << "\n";
    std::cout << "    Max    / 最大值:      " << FormatNum(stats.max, 12) << "\n";
    std::cout << "    P50:                  " << FormatNum(stats.p50, 12) << "\n";
    std::cout << "    P90:                  " << FormatNum(stats.p90, 12) << "\n";
    std::cout << "    P99:                  " << FormatNum(stats.p99, 12) << "\n";
    std::cout << "    P99.9:                " << FormatNum(stats.p999, 12) << "\n";
}

// ==============================================================================
// Null Sink (for pure logging overhead measurement)
// 空输出（用于测量纯日志开销）
// ==============================================================================

class NullSink : public oneplog::Sink {
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

// ==============================================================================
// Benchmarks / 基准测试
// ==============================================================================

/**
 * @brief Benchmark sync mode single-threaded
 * @brief 同步模式单线程基准测试
 */
Statistics BenchmarkSyncSingleThread(const BenchmarkConfig& config) {
    auto nullSink = std::make_shared<NullSink>();
    auto logger = std::make_shared<oneplog::Logger>("bench_sync", oneplog::Mode::Sync);
    logger->SetSink(nullSink);
    logger->SetLevel(oneplog::Level::Info);
    logger->Init();

    // Warmup / 预热
    for (int i = 0; i < config.warmupIterations; ++i) {
        logger->Info("Warmup message {}", i);
    }
    nullSink->Reset();

    // Benchmark / 基准测试
    std::vector<int64_t> latencies;
    latencies.reserve(config.iterations);

    auto start = Clock::now();
    for (int i = 0; i < config.iterations; ++i) {
        auto t1 = Clock::now();
        logger->Info("Benchmark message {} with value {}", i, 3.14159);
        auto t2 = Clock::now();
        latencies.push_back(std::chrono::duration_cast<Duration>(t2 - t1).count());
    }
    auto end = Clock::now();

    double totalTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalculateStats(latencies, totalTimeMs);
}

/**
 * @brief Benchmark async mode single-threaded
 * @brief 异步模式单线程基准测试
 */
Statistics BenchmarkAsyncSingleThread(const BenchmarkConfig& config) {
    auto nullSink = std::make_shared<NullSink>();
    auto logger = std::make_shared<oneplog::Logger>("bench_async", oneplog::Mode::Async);
    logger->SetSink(nullSink);
    logger->SetLevel(oneplog::Level::Info);
    
    oneplog::LoggerConfig logConfig;
    logConfig.mode = oneplog::Mode::Async;
    logConfig.heapRingBufferSize = 1024 * 1024;  // 1M entries
    logger->Init(logConfig);

    // Warmup / 预热
    for (int i = 0; i < config.warmupIterations; ++i) {
        logger->Info("Warmup message {}", i);
    }
    logger->Flush();
    nullSink->Reset();

    // Benchmark / 基准测试
    std::vector<int64_t> latencies;
    latencies.reserve(config.iterations);

    auto start = Clock::now();
    for (int i = 0; i < config.iterations; ++i) {
        auto t1 = Clock::now();
        logger->Info("Benchmark message {} with value {}", i, 3.14159);
        auto t2 = Clock::now();
        latencies.push_back(std::chrono::duration_cast<Duration>(t2 - t1).count());
    }
    auto end = Clock::now();

    logger->Flush();
    logger->Shutdown();

    double totalTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalculateStats(latencies, totalTimeMs);
}

/**
 * @brief Benchmark async mode multi-threaded
 * @brief 异步模式多线程基准测试
 */
Statistics BenchmarkAsyncMultiThread(const BenchmarkConfig& config) {
    auto nullSink = std::make_shared<NullSink>();
    auto logger = std::make_shared<oneplog::Logger>("bench_async_mt", oneplog::Mode::Async);
    logger->SetSink(nullSink);
    logger->SetLevel(oneplog::Level::Info);
    
    oneplog::LoggerConfig logConfig;
    logConfig.mode = oneplog::Mode::Async;
    logConfig.heapRingBufferSize = 1024 * 1024;
    logger->Init(logConfig);

    // Warmup / 预热
    for (int i = 0; i < config.warmupIterations; ++i) {
        logger->Info("Warmup message {}", i);
    }
    logger->Flush();
    nullSink->Reset();

    int iterPerThread = config.iterations / config.threads;
    std::vector<std::vector<int64_t>> threadLatencies(config.threads);
    std::atomic<bool> startFlag{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < config.threads; ++t) {
        threadLatencies[t].reserve(iterPerThread);
        threads.emplace_back([&, t]() {
            while (!startFlag.load()) {
                std::this_thread::yield();
            }
            for (int i = 0; i < iterPerThread; ++i) {
                auto t1 = Clock::now();
                logger->Info("Thread {} message {} value {}", t, i, 3.14159);
                auto t2 = Clock::now();
                threadLatencies[t].push_back(
                    std::chrono::duration_cast<Duration>(t2 - t1).count());
            }
        });
    }

    auto start = Clock::now();
    startFlag = true;

    for (auto& th : threads) {
        th.join();
    }
    auto end = Clock::now();

    logger->Flush();
    logger->Shutdown();

    // Merge latencies / 合并延迟数据
    std::vector<int64_t> allLatencies;
    for (auto& tl : threadLatencies) {
        allLatencies.insert(allLatencies.end(), tl.begin(), tl.end());
    }

    double totalTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalculateStats(allLatencies, totalTimeMs);
}

#ifdef ONEPLOG_HAS_FORK
#include <sys/mman.h>

// Shared memory structure for collecting latencies from child processes
// 用于从子进程收集延迟数据的共享内存结构
struct SharedLatencyData {
    std::atomic<size_t> count{0};
    std::atomic<bool> ready[16];  // Max 16 processes
    int64_t latencies[16][50000]; // Max 50000 samples per process
    size_t latencyCounts[16];
};

/**
 * @brief Benchmark multi-process mode
 * @brief 多进程模式基准测试
 */
Statistics BenchmarkMultiProcess(const BenchmarkConfig& config) {
    // Limit iterations per process for shared memory size
    // 限制每个进程的迭代次数以适应共享内存大小
    int maxIterPerProcess = 50000;
    int iterPerProcess = std::min(config.iterations / config.processes, maxIterPerProcess);
    int actualProcesses = std::min(config.processes, 16);
    
    // Create shared memory for collecting latencies
    // 创建共享内存收集延迟数据
    size_t shmSize = sizeof(SharedLatencyData);
    void* shmPtr = mmap(nullptr, shmSize, PROT_READ | PROT_WRITE, 
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shmPtr == MAP_FAILED) {
        std::cerr << "mmap failed!" << std::endl;
        Statistics stats;
        return stats;
    }
    
    auto* sharedData = new (shmPtr) SharedLatencyData();
    for (int i = 0; i < 16; ++i) {
        sharedData->ready[i].store(false);
        sharedData->latencyCounts[i] = 0;
    }
    
    std::vector<pid_t> childPids;
    auto start = Clock::now();
    
    for (int p = 0; p < actualProcesses; ++p) {
        pid_t pid = fork();
        
        if (pid < 0) {
            std::cerr << "Fork failed!" << std::endl;
            break;
        } else if (pid == 0) {
            // Child process / 子进程
            auto nullSink = std::make_shared<NullSink>();
            auto logger = std::make_shared<oneplog::Logger>("bench_mproc", oneplog::Mode::Sync);
            logger->SetSink(nullSink);
            logger->SetLevel(oneplog::Level::Info);
            logger->Init();
            
            // Collect latencies / 收集延迟数据
            size_t idx = 0;
            for (int i = 0; i < iterPerProcess; ++i) {
                auto t1 = Clock::now();
                logger->Info("Process {} message {} value {}", p, i, 3.14159);
                auto t2 = Clock::now();
                sharedData->latencies[p][idx++] = 
                    std::chrono::duration_cast<Duration>(t2 - t1).count();
            }
            
            sharedData->latencyCounts[p] = idx;
            sharedData->ready[p].store(true);
            
            logger->Flush();
            _exit(0);
        } else {
            childPids.push_back(pid);
        }
    }
    
    // Wait for all children / 等待所有子进程
    for (pid_t pid : childPids) {
        int status;
        waitpid(pid, &status, 0);
    }
    
    auto end = Clock::now();
    
    // Collect all latencies from shared memory
    // 从共享内存收集所有延迟数据
    std::vector<int64_t> allLatencies;
    for (int p = 0; p < actualProcesses; ++p) {
        if (sharedData->ready[p].load()) {
            size_t count = sharedData->latencyCounts[p];
            for (size_t i = 0; i < count; ++i) {
                allLatencies.push_back(sharedData->latencies[p][i]);
            }
        }
    }
    
    double totalTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    // Cleanup shared memory / 清理共享内存
    munmap(shmPtr, shmSize);
    
    if (allLatencies.empty()) {
        Statistics stats;
        stats.throughput = (iterPerProcess * actualProcesses / totalTimeMs) * 1000.0;
        return stats;
    }
    
    return CalculateStats(allLatencies, totalTimeMs);
}
#endif

/**
 * @brief Benchmark BinarySnapshot capture overhead
 * @brief BinarySnapshot 捕获开销基准测试
 */
Statistics BenchmarkSnapshotCapture(const BenchmarkConfig& config) {
    std::vector<int64_t> latencies;
    latencies.reserve(config.iterations);

    // Warmup / 预热
    for (int i = 0; i < config.warmupIterations; ++i) {
        oneplog::BinarySnapshot snapshot;
        snapshot.Capture(i, 3.14159, "test string", true);
    }

    auto start = Clock::now();
    for (int i = 0; i < config.iterations; ++i) {
        auto t1 = Clock::now();
        oneplog::BinarySnapshot snapshot;
        snapshot.Capture(i, 3.14159, "test string", true);
        auto t2 = Clock::now();
        latencies.push_back(std::chrono::duration_cast<Duration>(t2 - t1).count());
    }
    auto end = Clock::now();

    double totalTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalculateStats(latencies, totalTimeMs);
}

/**
 * @brief Benchmark HeapRingBuffer push/pop
 * @brief HeapRingBuffer 入队/出队基准测试
 */
Statistics BenchmarkRingBuffer(const BenchmarkConfig& config) {
    oneplog::HeapRingBuffer<oneplog::LogEntry> buffer(65536);
    std::vector<int64_t> latencies;
    latencies.reserve(config.iterations);

    // Warmup / 预热
    for (int i = 0; i < config.warmupIterations; ++i) {
        oneplog::LogEntry entry;
        entry.level = oneplog::Level::Info;
        buffer.TryPush(std::move(entry));
        oneplog::LogEntry out;
        buffer.TryPop(out);
    }

    auto start = Clock::now();
    for (int i = 0; i < config.iterations; ++i) {
        oneplog::LogEntry entry;
        entry.level = oneplog::Level::Info;
        entry.timestamp = static_cast<uint64_t>(i);

        auto t1 = Clock::now();
        buffer.TryPush(std::move(entry));
        oneplog::LogEntry out;
        buffer.TryPop(out);
        auto t2 = Clock::now();

        latencies.push_back(std::chrono::duration_cast<Duration>(t2 - t1).count());
    }
    auto end = Clock::now();

    double totalTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalculateStats(latencies, totalTimeMs);
}

/**
 * @brief Benchmark format overhead
 * @brief 格式化开销基准测试
 */
Statistics BenchmarkFormat(const BenchmarkConfig& config) {
    oneplog::ConsoleFormat format;
    format.SetProcessName("bench");
    format.SetModuleName("test");
    format.SetColorEnabled(false);

    oneplog::LogEntry entry;
    entry.level = oneplog::Level::Info;
    entry.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    entry.processId = 12345;
    entry.threadId = 67890;
    entry.snapshot.Capture("Test message with value {} and string {}", 42, "hello");

    std::vector<int64_t> latencies;
    latencies.reserve(config.iterations);

    // Warmup / 预热
    for (int i = 0; i < config.warmupIterations; ++i) {
        auto result = format.FormatEntry(entry);
        (void)result;
    }

    auto start = Clock::now();
    for (int i = 0; i < config.iterations; ++i) {
        auto t1 = Clock::now();
        auto result = format.FormatEntry(entry);
        auto t2 = Clock::now();
        (void)result;
        latencies.push_back(std::chrono::duration_cast<Duration>(t2 - t1).count());
    }
    auto end = Clock::now();

    double totalTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalculateStats(latencies, totalTimeMs);
}

/**
 * @brief Benchmark async WFC (Wait For Completion) mode
 * @brief 异步 WFC（等待完成）模式基准测试
 */
Statistics BenchmarkAsyncWFC(const BenchmarkConfig& config) {
    auto nullSink = std::make_shared<NullSink>();
    auto logger = std::make_shared<oneplog::Logger>("bench_wfc", oneplog::Mode::Async);
    logger->SetSink(nullSink);
    logger->SetLevel(oneplog::Level::Info);
    
    oneplog::LoggerConfig logConfig;
    logConfig.mode = oneplog::Mode::Async;
    logConfig.heapRingBufferSize = 1024 * 1024;
    logger->Init(logConfig);

    // Use fewer iterations for WFC (it's slower)
    // WFC 使用较少的迭代次数（因为它更慢）
    int wfcIterations = std::min(config.iterations, 10000);

    // Warmup / 预热
    for (int i = 0; i < 100; ++i) {
        logger->InfoWFC("Warmup WFC message {}", i);
    }
    nullSink->Reset();

    // Benchmark / 基准测试
    std::vector<int64_t> latencies;
    latencies.reserve(wfcIterations);

    auto start = Clock::now();
    for (int i = 0; i < wfcIterations; ++i) {
        auto t1 = Clock::now();
        logger->InfoWFC("WFC benchmark message {} with value {}", i, 3.14159);
        auto t2 = Clock::now();
        latencies.push_back(std::chrono::duration_cast<Duration>(t2 - t1).count());
    }
    auto end = Clock::now();

    logger->Flush();
    logger->Shutdown();

    double totalTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalculateStats(latencies, totalTimeMs);
}

/**
 * @brief Benchmark async log call overhead from worker threads (measures call duration only)
 * @brief 工作线程异步日志调用开销基准测试（仅测量调用耗时）
 * 
 * This measures how long it takes for a worker thread to call the async log API.
 * 测量工作线程调用异步日志 API 所需的时间。
 */
Statistics BenchmarkWorkerThreadLatency(const BenchmarkConfig& config) {
    auto nullSink = std::make_shared<NullSink>();
    auto logger = std::make_shared<oneplog::Logger>("bench_worker", oneplog::Mode::Async);
    logger->SetSink(nullSink);
    logger->SetLevel(oneplog::Level::Info);
    
    oneplog::LoggerConfig logConfig;
    logConfig.mode = oneplog::Mode::Async;
    logConfig.heapRingBufferSize = 1024 * 1024;
    logger->Init(logConfig);

    // Warmup / 预热
    for (int i = 0; i < config.warmupIterations; ++i) {
        logger->Info("Warmup message {}", i);
    }
    logger->Flush();
    nullSink->Reset();

    // Benchmark: measure call duration / 基准测试：测量调用耗时
    std::vector<int64_t> latencies;
    latencies.reserve(config.iterations);

    auto start = Clock::now();
    for (int i = 0; i < config.iterations; ++i) {
        auto t1 = Clock::now();
        logger->Info("Worker thread test message {} value {}", i, 3.14159);
        auto t2 = Clock::now();
        latencies.push_back(std::chrono::duration_cast<Duration>(t2 - t1).count());
    }
    auto end = Clock::now();

    logger->Flush();
    logger->Shutdown();

    double totalTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalculateStats(latencies, totalTimeMs);
}

/**
 * @brief Benchmark multi-threaded async log call overhead
 * @brief 多线程异步日志调用开销基准测试
 * 
 * This measures how long it takes for multiple worker threads to call the async log API.
 * 测量多个工作线程调用异步日志 API 所需的时间。
 */
Statistics BenchmarkWorkerThreadLatencyMT(const BenchmarkConfig& config) {
    auto nullSink = std::make_shared<NullSink>();
    auto logger = std::make_shared<oneplog::Logger>("bench_worker_mt", oneplog::Mode::Async);
    logger->SetSink(nullSink);
    logger->SetLevel(oneplog::Level::Info);
    
    oneplog::LoggerConfig logConfig;
    logConfig.mode = oneplog::Mode::Async;
    logConfig.heapRingBufferSize = 1024 * 1024;
    logger->Init(logConfig);

    int iterPerThread = config.iterations / config.threads;

    // Warmup / 预热
    for (int i = 0; i < config.warmupIterations; ++i) {
        logger->Info("Warmup message {}", i);
    }
    logger->Flush();
    nullSink->Reset();

    std::vector<std::vector<int64_t>> threadLatencies(config.threads);
    std::atomic<bool> startFlag{false};

    std::vector<std::thread> threads;
    for (int t = 0; t < config.threads; ++t) {
        threadLatencies[t].reserve(iterPerThread);
        threads.emplace_back([&, t]() {
            while (!startFlag.load()) {
                std::this_thread::yield();
            }
            for (int i = 0; i < iterPerThread; ++i) {
                auto t1 = Clock::now();
                logger->Info("Thread {} worker test {} value {}", t, i, 3.14159);
                auto t2 = Clock::now();
                threadLatencies[t].push_back(
                    std::chrono::duration_cast<Duration>(t2 - t1).count());
            }
        });
    }

    auto start = Clock::now();
    startFlag = true;

    for (auto& th : threads) {
        th.join();
    }
    auto end = Clock::now();

    logger->Flush();
    logger->Shutdown();

    // Merge latencies / 合并延迟数据
    std::vector<int64_t> allLatencies;
    for (auto& tl : threadLatencies) {
        allLatencies.insert(allLatencies.end(), tl.begin(), tl.end());
    }

    double totalTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalculateStats(allLatencies, totalTimeMs);
}

// ==============================================================================
// Main / 主函数
// ==============================================================================

void PrintHeader() {
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "        onePlog Performance Benchmark\n";
    std::cout << "        onePlog 性能基准测试\n";
    std::cout << "================================================================\n";
}

void PrintSystemInfo() {
    std::cout << "\n";
    std::cout << "  System Info / 系统信息\n";
    std::cout << "  ----------------------------------------------------------------\n";
    std::cout << "  Hardware threads / 硬件线程:    " << std::thread::hardware_concurrency() << "\n";
#ifdef NDEBUG
    std::cout << "  Build mode / 构建模式:          Release\n";
#else
    std::cout << "  Build mode / 构建模式:          Debug\n";
#endif
#ifdef ONEPLOG_USE_FMT
    std::cout << "  fmt library / fmt 库:           Enabled / 启用\n";
#else
    std::cout << "  fmt library / fmt 库:           Disabled / 禁用\n";
#endif
}

void PrintConfig(const BenchmarkConfig& config) {
    std::cout << "\n";
    std::cout << "  Benchmark Config / 测试配置\n";
    std::cout << "  ----------------------------------------------------------------\n";
    std::cout << "  Iterations / 迭代次数:          " << config.iterations << "\n";
    std::cout << "  Threads / 线程数:               " << config.threads << "\n";
    std::cout << "  Processes / 进程数:             " << config.processes << "\n";
    std::cout << "  Warmup / 预热次数:              " << config.warmupIterations << "\n";
}

void PrintProgress(int current, int total, const std::string& nameEn, const std::string& nameZh) {
    std::cout << "\n>>> [" << current << "/" << total << "] " 
              << nameEn << " / " << nameZh << "...\n";
    std::cout.flush();
}

void PrintSummary(const std::vector<std::pair<std::string, double>>& results) {
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "                      Summary / 总结\n";
    std::cout << "================================================================\n";
    for (const auto& [name, throughput] : results) {
        std::cout << "  " << std::left << std::setw(40) << name 
                  << FormatThroughput(throughput) << " ops/sec\n";
    }
    std::cout << "================================================================\n";
}

int main(int argc, char* argv[]) {
    PrintHeader();
    PrintSystemInfo();

    BenchmarkConfig config;
    
    // Parse command line arguments / 解析命令行参数
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-v" || arg == "--verbose") {
            config.verbose = true;
        } else if (arg == "-i" && i + 1 < argc) {
            config.iterations = std::atoi(argv[++i]);
        } else if (arg == "-t" && i + 1 < argc) {
            config.threads = std::atoi(argv[++i]);
        } else if (arg == "-p" && i + 1 < argc) {
            config.processes = std::atoi(argv[++i]);
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "\nUsage / 用法: benchmark [options]\n";
            std::cout << "  -i <num>    Iterations / 迭代次数 (default: 1000000)\n";
            std::cout << "  -t <num>    Threads / 线程数 (default: 4)\n";
            std::cout << "  -p <num>    Processes / 进程数 (default: 4)\n";
            std::cout << "  -v          Verbose output / 详细输出\n";
            std::cout << "  -h          Show help / 显示帮助\n";
            return 0;
        }
    }

    PrintConfig(config);

#ifdef ONEPLOG_HAS_FORK
    const int totalTests = 11;
#else
    const int totalTests = 10;
#endif

    std::cout << "\n================================================================\n";
    std::cout << "  Running benchmarks... / 运行基准测试...\n";
    std::cout << "================================================================\n";

    std::vector<std::pair<std::string, double>> summaryResults;
    int testNum = 1;

    // Run benchmarks / 运行基准测试
    PrintProgress(testNum++, totalTests, "BinarySnapshot Capture", "BinarySnapshot 捕获");
    auto snapshotStats = BenchmarkSnapshotCapture(config);
    PrintStats("BinarySnapshot Capture", "BinarySnapshot 捕获", snapshotStats);
    summaryResults.emplace_back("BinarySnapshot Capture", snapshotStats.throughput);

    PrintProgress(testNum++, totalTests, "HeapRingBuffer Push/Pop", "HeapRingBuffer 入队出队");
    auto ringBufferStats = BenchmarkRingBuffer(config);
    PrintStats("HeapRingBuffer Push/Pop", "HeapRingBuffer 入队出队", ringBufferStats);
    summaryResults.emplace_back("HeapRingBuffer Push/Pop", ringBufferStats.throughput);

    PrintProgress(testNum++, totalTests, "ConsoleFormat", "控制台格式化");
    auto formatStats = BenchmarkFormat(config);
    PrintStats("ConsoleFormat", "控制台格式化", formatStats);
    summaryResults.emplace_back("ConsoleFormat", formatStats.throughput);

    PrintProgress(testNum++, totalTests, "Sync Mode (1 Thread)", "同步模式 (单线程)");
    auto syncStats = BenchmarkSyncSingleThread(config);
    PrintStats("Sync Mode (1 Thread)", "同步模式 (单线程)", syncStats);
    summaryResults.emplace_back("Sync Mode (1 Thread)", syncStats.throughput);

    PrintProgress(testNum++, totalTests, "Async Mode (1 Thread)", "异步模式 (单线程)");
    auto asyncStats = BenchmarkAsyncSingleThread(config);
    PrintStats("Async Mode (1 Thread)", "异步模式 (单线程)", asyncStats);
    summaryResults.emplace_back("Async Mode (1 Thread)", asyncStats.throughput);

    PrintProgress(testNum++, totalTests, "Async Mode (Multi Thread)", "异步模式 (多线程)");
    auto asyncMtStats = BenchmarkAsyncMultiThread(config);
    PrintStats("Async Mode (" + std::to_string(config.threads) + " Threads)", 
               "异步模式 (" + std::to_string(config.threads) + " 线程)", asyncMtStats);
    summaryResults.emplace_back("Async Mode (" + std::to_string(config.threads) + " Threads)", 
                                asyncMtStats.throughput);

    PrintProgress(testNum++, totalTests, "Async WFC Mode", "异步 WFC 模式");
    auto wfcStats = BenchmarkAsyncWFC(config);
    PrintStats("Async WFC Mode", "异步 WFC 模式", wfcStats);
    summaryResults.emplace_back("Async WFC Mode", wfcStats.throughput);

    PrintProgress(testNum++, totalTests, "Async Call Overhead (1 Thread)", "异步调用开销 (单线程)");
    auto workerStats = BenchmarkWorkerThreadLatency(config);
    PrintStats("Async Call Overhead (1 Thread)", "异步调用开销 (单线程)", workerStats);
    summaryResults.emplace_back("Async Call Overhead (1 Thread)", workerStats.throughput);

    PrintProgress(testNum++, totalTests, "Async Call Overhead (Multi Thread)", "异步调用开销 (多线程)");
    auto workerMtStats = BenchmarkWorkerThreadLatencyMT(config);
    PrintStats("Async Call Overhead (" + std::to_string(config.threads) + " Threads)", 
               "异步调用开销 (" + std::to_string(config.threads) + " 线程)", workerMtStats);
    summaryResults.emplace_back("Async Call Overhead (" + std::to_string(config.threads) + " Threads)", 
                                workerMtStats.throughput);

#ifdef ONEPLOG_HAS_FORK
    PrintProgress(testNum++, totalTests, "Multi-Process Mode", "多进程模式");
    auto mprocStats = BenchmarkMultiProcess(config);
    PrintStats("Multi-Process (" + std::to_string(config.processes) + " Processes)", 
               "多进程模式 (" + std::to_string(config.processes) + " 进程)", mprocStats);
    summaryResults.emplace_back("Multi-Process (" + std::to_string(config.processes) + " Processes)", 
                                mprocStats.throughput);
#endif

    // Print summary / 打印总结
    PrintSummary(summaryResults);

    std::cout << "\nBenchmark complete! / 基准测试完成!\n\n";

    return 0;
}
