/**
 * @file stress_test.cpp
 * @brief Stress test for onePlog
 * @brief onePlog 压力测试
 *
 * This program tests onePlog under high load conditions:
 * - Multiple threads writing logs concurrently
 * - High message throughput
 * - Long duration testing
 * - Memory stability
 *
 * 此程序在高负载条件下测试 onePlog：
 * - 多线程并发写入日志
 * - 高消息吞吐量
 * - 长时间运行测试
 * - 内存稳定性
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <iomanip>
#include <cstring>
#include <oneplog/oneplog.hpp>

// ==============================================================================
// Configuration / 配置
// ==============================================================================

struct StressTestConfig {
    int numThreads = 8;              // Number of producer threads / 生产者线程数
    int messagesPerThread = 100000;  // Messages per thread / 每线程消息数
    int durationSeconds = 0;         // Duration in seconds (0 = use message count) / 持续时间秒数
    bool useNullSink = true;         // Use null sink for pure throughput test / 使用空 Sink 进行纯吞吐量测试
    bool verbose = false;            // Print progress / 打印进度
    int reportInterval = 10000;      // Report interval / 报告间隔
    size_t queueSize = 8192;         // Ring buffer queue size / 环形队列大小
    bool blockOnFull = false;        // Block when queue is full / 队列满时阻塞
};

// ==============================================================================
// Null Sink for throughput testing / 用于吞吐量测试的空 Sink
// ==============================================================================

class NullSink : public oneplog::Sink {
public:
    void Write(const std::string&) override {
        m_count.fetch_add(1, std::memory_order_relaxed);
    }
    void Write(std::string_view) override {
        m_count.fetch_add(1, std::memory_order_relaxed);
    }
    void Flush() override {}
    void Close() override {}
    bool HasError() const override { return false; }
    std::string GetLastError() const override { return ""; }
    
    uint64_t GetCount() const { return m_count.load(std::memory_order_relaxed); }
    void Reset() { m_count.store(0, std::memory_order_relaxed); }
    
private:
    std::atomic<uint64_t> m_count{0};
};

// ==============================================================================
// Statistics / 统计数据
// ==============================================================================

struct Statistics {
    std::atomic<uint64_t> totalMessages{0};
    std::atomic<uint64_t> droppedMessages{0};
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point endTime;
    
    void Reset() {
        totalMessages.store(0, std::memory_order_relaxed);
        droppedMessages.store(0, std::memory_order_relaxed);
    }
    
    double GetDurationSeconds() const {
        return std::chrono::duration<double>(endTime - startTime).count();
    }
    
    double GetThroughput() const {
        double duration = GetDurationSeconds();
        if (duration > 0) {
            return totalMessages.load(std::memory_order_relaxed) / duration;
        }
        return 0;
    }
};

// ==============================================================================
// Global variables / 全局变量
// ==============================================================================

static std::atomic<bool> g_running{true};
static Statistics g_stats;
static std::shared_ptr<NullSink> g_nullSink;

// ==============================================================================
// Worker thread function / 工作线程函数
// ==============================================================================

void WorkerThread(int threadId, const StressTestConfig& config,
                  oneplog::Logger<oneplog::Mode::Async, oneplog::Level::Debug, false>& logger) {
    // Set module name for this thread
    oneplog::SetModuleName("worker" + std::to_string(threadId));
    
    int messageCount = 0;
    int localReportInterval = config.reportInterval;
    
    while (g_running.load(std::memory_order_relaxed)) {
        // Log message with varying content
        logger.Info("Thread {} message {} - stress test payload data 1234567890", 
                   threadId, messageCount);
        
        messageCount++;
        g_stats.totalMessages.fetch_add(1, std::memory_order_relaxed);
        
        // Check if we've reached the message limit (if not using duration mode)
        if (config.durationSeconds == 0 && messageCount >= config.messagesPerThread) {
            break;
        }
        
        // Progress report
        if (config.verbose && messageCount % localReportInterval == 0) {
            std::cout << "Thread " << threadId << ": " << messageCount << " messages" << std::endl;
        }
    }
}

// ==============================================================================
// Duration-based worker thread / 基于时间的工作线程
// ==============================================================================

void DurationWorkerThread(int threadId, const StressTestConfig& config,
                          oneplog::Logger<oneplog::Mode::Async, oneplog::Level::Debug, false>& logger) {
    oneplog::SetModuleName("worker" + std::to_string(threadId));
    
    int messageCount = 0;
    
    while (g_running.load(std::memory_order_relaxed)) {
        logger.Info("Thread {} message {} - stress test payload", threadId, messageCount);
        messageCount++;
        g_stats.totalMessages.fetch_add(1, std::memory_order_relaxed);
    }
}

// ==============================================================================
// Run stress test / 运行压力测试
// ==============================================================================

void RunStressTest(const StressTestConfig& config) {
    std::cout << "\n=== onePlog Stress Test / 压力测试 ===" << std::endl;
    std::cout << "Configuration / 配置:" << std::endl;
    std::cout << "  Threads / 线程数: " << config.numThreads << std::endl;
    if (config.durationSeconds > 0) {
        std::cout << "  Duration / 持续时间: " << config.durationSeconds << " seconds" << std::endl;
    } else {
        std::cout << "  Messages per thread / 每线程消息数: " << config.messagesPerThread << std::endl;
        std::cout << "  Total messages / 总消息数: " 
                  << (static_cast<uint64_t>(config.numThreads) * config.messagesPerThread) << std::endl;
    }
    std::cout << "  Queue size / 队列大小: " << config.queueSize << std::endl;
    std::cout << "  Queue full policy / 队列满策略: " 
              << (config.blockOnFull ? "Block (no drop)" : "DropNewest") << std::endl;
    std::cout << "  Sink: " << (config.useNullSink ? "NullSink (throughput test)" : "ConsoleSink") << std::endl;
    std::cout << std::endl;

    // Create logger with custom config
    oneplog::Logger<oneplog::Mode::Async, oneplog::Level::Debug, false> logger;
    
    if (config.useNullSink) {
        g_nullSink = std::make_shared<NullSink>();
        logger.SetSink(g_nullSink);
    } else {
        logger.SetSink(std::make_shared<oneplog::ConsoleSink>());
    }
    
    logger.SetFormat(std::make_shared<oneplog::ConsoleFormat>());
    
    // Configure queue size and policy
    oneplog::LoggerConfig loggerConfig;
    loggerConfig.heapRingBufferSize = config.queueSize;
    loggerConfig.queueFullPolicy = config.blockOnFull ? 
        oneplog::QueueFullPolicy::Block : oneplog::QueueFullPolicy::DropNewest;
    logger.Init(loggerConfig);
    
    // Reset statistics
    g_stats.Reset();
    g_running.store(true, std::memory_order_relaxed);
    
    // Create worker threads
    std::vector<std::thread> threads;
    threads.reserve(config.numThreads);
    
    std::cout << "Starting stress test..." << std::endl;
    g_stats.startTime = std::chrono::steady_clock::now();
    
    for (int i = 0; i < config.numThreads; ++i) {
        if (config.durationSeconds > 0) {
            threads.emplace_back(DurationWorkerThread, i, std::ref(config), std::ref(logger));
        } else {
            threads.emplace_back(WorkerThread, i, std::ref(config), std::ref(logger));
        }
    }
    
    // If duration mode, wait for specified time then signal stop
    if (config.durationSeconds > 0) {
        std::this_thread::sleep_for(std::chrono::seconds(config.durationSeconds));
        g_running.store(false, std::memory_order_relaxed);
    }
    
    // Wait for all threads to complete
    for (auto& t : threads) {
        t.join();
    }
    
    g_stats.endTime = std::chrono::steady_clock::now();
    
    // Flush and shutdown
    logger.Flush();
    logger.Shutdown();
    
    // Print results
    std::cout << "\n=== Results / 结果 ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Duration / 持续时间: " << g_stats.GetDurationSeconds() << " seconds" << std::endl;
    std::cout << "  Total messages / 总消息数: " << g_stats.totalMessages.load() << std::endl;
    std::cout << "  Throughput / 吞吐量: " << (g_stats.GetThroughput() / 1e6) << " M msgs/sec" << std::endl;
    std::cout << "  Per-thread throughput / 每线程吞吐量: " 
              << (g_stats.GetThroughput() / config.numThreads / 1e6) << " M msgs/sec" << std::endl;
    
    if (g_nullSink) {
        uint64_t total = g_stats.totalMessages.load();
        uint64_t received = g_nullSink->GetCount();
        uint64_t dropped = total > received ? total - received : 0;
        double dropRate = total > 0 ? (static_cast<double>(dropped) / total * 100.0) : 0.0;
        
        std::cout << "  Sink received / Sink 接收: " << received << " messages" << std::endl;
        std::cout << "  Dropped / 丢弃: " << dropped << " messages (" << dropRate << "%)" << std::endl;
        
        // Analysis
        std::cout << "\n=== Analysis / 分析 ===" << std::endl;
        if (dropRate > 50) {
            std::cout << "  ⚠️  High drop rate! / 高丢弃率！" << std::endl;
            std::cout << "  Suggestions / 建议:" << std::endl;
            std::cout << "    - Increase queue size (-q option) / 增加队列大小" << std::endl;
            std::cout << "    - Use Block policy (-B option) / 使用阻塞策略" << std::endl;
            std::cout << "    - Reduce producer threads / 减少生产者线程" << std::endl;
        } else if (dropRate > 10) {
            std::cout << "  ⚡ Moderate drop rate / 中等丢弃率" << std::endl;
            std::cout << "  Consider increasing queue size / 考虑增加队列大小" << std::endl;
        } else if (dropRate > 0) {
            std::cout << "  ✓ Low drop rate / 低丢弃率" << std::endl;
        } else {
            std::cout << "  ✓ No drops! / 无丢弃！" << std::endl;
        }
        
        // Effective throughput (messages actually processed)
        double effectiveThroughput = received / g_stats.GetDurationSeconds();
        std::cout << "  Effective throughput / 有效吞吐量: " 
                  << (effectiveThroughput / 1e6) << " M msgs/sec" << std::endl;
    }
    
    std::cout << std::endl;
}

// ==============================================================================
// Run latency test / 运行延迟测试
// ==============================================================================

void RunLatencyTest(int iterations) {
    std::cout << "\n=== Latency Test / 延迟测试 ===" << std::endl;
    std::cout << "Iterations / 迭代次数: " << iterations << std::endl;
    
    oneplog::Logger<oneplog::Mode::Async, oneplog::Level::Debug, false> logger;
    auto nullSink = std::make_shared<NullSink>();
    logger.SetSink(nullSink);
    logger.SetFormat(std::make_shared<oneplog::ConsoleFormat>());
    logger.Init();
    
    std::vector<int64_t> latencies;
    latencies.reserve(iterations);
    
    // Warm up
    for (int i = 0; i < 1000; ++i) {
        logger.Info("Warmup message {}", i);
    }
    logger.Flush();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Measure latency
    for (int i = 0; i < iterations; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        logger.Info("Latency test message {}", i);
        auto end = std::chrono::high_resolution_clock::now();
        
        latencies.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }
    
    logger.Flush();
    logger.Shutdown();
    
    // Calculate statistics
    std::sort(latencies.begin(), latencies.end());
    
    int64_t sum = 0;
    for (auto lat : latencies) {
        sum += lat;
    }
    
    double avg = static_cast<double>(sum) / iterations;
    int64_t p50 = latencies[iterations / 2];
    int64_t p90 = latencies[iterations * 90 / 100];
    int64_t p99 = latencies[iterations * 99 / 100];
    int64_t p999 = latencies[iterations * 999 / 1000];
    int64_t minLat = latencies.front();
    int64_t maxLat = latencies.back();
    
    std::cout << "\n=== Latency Results / 延迟结果 ===" << std::endl;
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "  Min / 最小: " << minLat << " ns" << std::endl;
    std::cout << "  Avg / 平均: " << avg << " ns" << std::endl;
    std::cout << "  P50 / 中位数: " << p50 << " ns" << std::endl;
    std::cout << "  P90: " << p90 << " ns" << std::endl;
    std::cout << "  P99: " << p99 << " ns" << std::endl;
    std::cout << "  P99.9: " << p999 << " ns" << std::endl;
    std::cout << "  Max / 最大: " << maxLat << " ns" << std::endl;
    std::cout << std::endl;
}

// ==============================================================================
// Run burst test / 运行突发测试
// ==============================================================================

void RunBurstTest(int burstSize, int numBursts, int intervalMs) {
    std::cout << "\n=== Burst Test / 突发测试 ===" << std::endl;
    std::cout << "Burst size / 突发大小: " << burstSize << std::endl;
    std::cout << "Number of bursts / 突发次数: " << numBursts << std::endl;
    std::cout << "Interval / 间隔: " << intervalMs << " ms" << std::endl;
    
    oneplog::Logger<oneplog::Mode::Async, oneplog::Level::Debug, false> logger;
    auto nullSink = std::make_shared<NullSink>();
    logger.SetSink(nullSink);
    logger.SetFormat(std::make_shared<oneplog::ConsoleFormat>());
    logger.Init();
    
    auto startTime = std::chrono::steady_clock::now();
    
    for (int burst = 0; burst < numBursts; ++burst) {
        // Send burst of messages
        for (int i = 0; i < burstSize; ++i) {
            logger.Info("Burst {} message {}", burst, i);
        }
        
        // Wait between bursts
        if (burst < numBursts - 1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
        }
    }
    
    logger.Flush();
    
    auto endTime = std::chrono::steady_clock::now();
    double duration = std::chrono::duration<double>(endTime - startTime).count();
    uint64_t totalMessages = static_cast<uint64_t>(burstSize) * numBursts;
    
    logger.Shutdown();
    
    std::cout << "\n=== Burst Test Results / 突发测试结果 ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Duration / 持续时间: " << duration << " seconds" << std::endl;
    std::cout << "  Total messages / 总消息数: " << totalMessages << std::endl;
    std::cout << "  Sink received / Sink 接收: " << nullSink->GetCount() << std::endl;
    std::cout << "  Effective throughput / 有效吞吐量: " 
              << (totalMessages / duration / 1e6) << " M msgs/sec" << std::endl;
    std::cout << std::endl;
}

// ==============================================================================
// Print usage / 打印用法
// ==============================================================================

void PrintUsage(const char* programName) {
    std::cout << "Usage / 用法: " << programName << " [options]" << std::endl;
    std::cout << std::endl;
    std::cout << "Options / 选项:" << std::endl;
    std::cout << "  -t <threads>     Number of threads (default: 8) / 线程数" << std::endl;
    std::cout << "  -m <messages>    Messages per thread (default: 100000) / 每线程消息数" << std::endl;
    std::cout << "  -d <seconds>     Duration in seconds (overrides -m) / 持续时间秒数" << std::endl;
    std::cout << "  -q <size>        Queue size (default: 8192) / 队列大小" << std::endl;
    std::cout << "  -B               Block when queue full (default: drop) / 队列满时阻塞" << std::endl;
    std::cout << "  -c               Use ConsoleSink instead of NullSink / 使用 ConsoleSink" << std::endl;
    std::cout << "  -v               Verbose output / 详细输出" << std::endl;
    std::cout << "  -l <iterations>  Run latency test / 运行延迟测试" << std::endl;
    std::cout << "  -b <size>        Run burst test with given burst size / 运行突发测试" << std::endl;
    std::cout << "  -h               Show this help / 显示帮助" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples / 示例:" << std::endl;
    std::cout << "  " << programName << " -t 16 -m 500000    # 16 threads, 500K msgs each" << std::endl;
    std::cout << "  " << programName << " -t 8 -d 30        # 8 threads for 30 seconds" << std::endl;
    std::cout << "  " << programName << " -q 65536 -B       # Large queue with blocking" << std::endl;
    std::cout << "  " << programName << " -l 100000         # Latency test with 100K iterations" << std::endl;
    std::cout << "  " << programName << " -b 10000          # Burst test with 10K messages per burst" << std::endl;
}

// ==============================================================================
// Main / 主函数
// ==============================================================================

int main(int argc, char* argv[]) {
    StressTestConfig config;
    bool runLatencyTest = false;
    int latencyIterations = 100000;
    bool runBurstTest = false;
    int burstSize = 10000;
    
    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            config.numThreads = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            config.messagesPerThread = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            config.durationSeconds = std::atoi(argv[++i]);
        } else if (strcmp(argv[i], "-q") == 0 && i + 1 < argc) {
            config.queueSize = static_cast<size_t>(std::atoi(argv[++i]));
        } else if (strcmp(argv[i], "-B") == 0) {
            config.blockOnFull = true;
        } else if (strcmp(argv[i], "-c") == 0) {
            config.useNullSink = false;
        } else if (strcmp(argv[i], "-v") == 0) {
            config.verbose = true;
        } else if (strcmp(argv[i], "-l") == 0) {
            runLatencyTest = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                latencyIterations = std::atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "-b") == 0) {
            runBurstTest = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                burstSize = std::atoi(argv[++i]);
            }
        } else if (strcmp(argv[i], "-h") == 0) {
            PrintUsage(argv[0]);
            return 0;
        }
    }
    
    std::cout << "=== onePlog Stress Test Suite / 压力测试套件 ===" << std::endl;
    std::cout << "Platform / 平台: ";
#ifdef __APPLE__
    std::cout << "macOS";
#elif defined(__linux__)
    std::cout << "Linux";
#elif defined(_WIN32)
    std::cout << "Windows";
#else
    std::cout << "Unknown";
#endif
    std::cout << std::endl;
    std::cout << "Hardware threads / 硬件线程数: " << std::thread::hardware_concurrency() << std::endl;
    
    if (runLatencyTest) {
        RunLatencyTest(latencyIterations);
    }
    
    if (runBurstTest) {
        RunBurstTest(burstSize, 100, 10);  // 100 bursts, 10ms interval
    }
    
    if (!runLatencyTest && !runBurstTest) {
        // Run main stress test
        RunStressTest(config);
    }
    
    std::cout << "=== All tests completed / 所有测试完成 ===" << std::endl;
    
    return 0;
}
