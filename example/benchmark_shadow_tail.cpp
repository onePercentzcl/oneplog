/**
 * @file benchmark_shadow_tail.cpp
 * @brief Shadow tail optimization benchmark - high contention scenarios
 * @brief Shadow tail 优化基准测试 - 高竞争场景
 *
 * This benchmark specifically tests the shadow tail optimization by creating
 * scenarios with high producer-consumer contention.
 *
 * 此基准测试通过创建高生产者-消费者竞争场景来专门测试 shadow tail 优化。
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

#include <oneplog/internal/heap_memory.hpp>
#include <oneplog/internal/log_entry.hpp>

using Clock = std::chrono::high_resolution_clock;
using Duration = std::chrono::nanoseconds;

// ==============================================================================
// Test Configuration / 测试配置
// ==============================================================================

struct TestConfig {
    int iterations = 1000000;      // Total iterations / 总迭代次数
    int producerThreads = 8;       // Number of producer threads / 生产者线程数
    int warmupIterations = 10000;  // Warmup iterations / 预热迭代次数
    size_t queueSize = 1024;       // Queue size (small to create contention) / 队列大小（小以创建竞争）
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
    double throughput = 0;
    uint64_t droppedCount = 0;
};

Statistics CalculateStats(std::vector<int64_t>& latencies, double totalTimeMs, uint64_t dropped = 0) {
    Statistics stats;
    stats.droppedCount = dropped;
    
    if (latencies.empty()) {
        stats.throughput = 0;
        return stats;
    }

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

std::string FormatNum(double val, int width, int precision = 1) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << val;
    std::string s = oss.str();
    if (static_cast<int>(s.size()) < width) {
        s = std::string(width - s.size(), ' ') + s;
    }
    return s;
}

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

void PrintStats(const std::string& name, const Statistics& stats) {
    std::cout << "\n  " << name << "\n";
    std::cout << "  ----------------------------------------------------------------\n";
    std::cout << "  Throughput / 吞吐量:    " << FormatThroughput(stats.throughput) << " ops/sec\n";
    std::cout << "  Dropped / 丢弃数:       " << stats.droppedCount << "\n";
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
// Test 1: Multi-producer single-consumer (high contention)
// 测试 1：多生产者单消费者（高竞争）
// ==============================================================================

Statistics BenchmarkMPSC(const TestConfig& config) {
    oneplog::HeapRingBuffer<oneplog::LogEntry, false> buffer(config.queueSize);
    buffer.SetPolicy(oneplog::QueueFullPolicy::DropNewest);
    
    int iterPerThread = config.iterations / config.producerThreads;
    std::vector<std::vector<int64_t>> threadLatencies(config.producerThreads);
    std::atomic<bool> startFlag{false};
    std::atomic<bool> stopConsumer{false};
    std::atomic<size_t> totalProduced{0};
    std::atomic<size_t> totalConsumed{0};

    // Consumer thread / 消费者线程
    std::thread consumer([&]() {
        oneplog::LogEntry entry;
        while (!stopConsumer.load(std::memory_order_relaxed) || 
               totalConsumed.load() < totalProduced.load()) {
            if (buffer.TryPop(entry)) {
                totalConsumed.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
    });

    // Producer threads / 生产者线程
    std::vector<std::thread> producers;
    for (int t = 0; t < config.producerThreads; ++t) {
        threadLatencies[t].reserve(iterPerThread);
        producers.emplace_back([&, t]() {
            while (!startFlag.load(std::memory_order_relaxed)) {
                std::this_thread::yield();
            }
            
            for (int i = 0; i < iterPerThread; ++i) {
                oneplog::LogEntry entry;
                entry.level = oneplog::Level::Info;
                entry.timestamp = static_cast<uint64_t>(i);

                auto t1 = Clock::now();
                bool pushed = buffer.TryPush(std::move(entry));
                auto t2 = Clock::now();
                
                if (pushed) {
                    totalProduced.fetch_add(1, std::memory_order_relaxed);
                }
                threadLatencies[t].push_back(
                    std::chrono::duration_cast<Duration>(t2 - t1).count());
            }
        });
    }

    auto start = Clock::now();
    startFlag = true;

    for (auto& th : producers) {
        th.join();
    }
    
    auto end = Clock::now();
    stopConsumer = true;
    consumer.join();

    // Merge latencies / 合并延迟数据
    std::vector<int64_t> allLatencies;
    for (auto& tl : threadLatencies) {
        allLatencies.insert(allLatencies.end(), tl.begin(), tl.end());
    }

    double totalTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalculateStats(allLatencies, totalTimeMs, buffer.GetDroppedCount());
}

// ==============================================================================
// Test 2: Queue full scenario (DropNewest policy)
// 测试 2：队列满场景（DropNewest 策略）
// ==============================================================================

Statistics BenchmarkQueueFull(const TestConfig& config) {
    // Very small queue to ensure frequent full conditions
    // 非常小的队列以确保频繁的满队列情况
    oneplog::HeapRingBuffer<oneplog::LogEntry, false> buffer(64);
    buffer.SetPolicy(oneplog::QueueFullPolicy::DropNewest);
    
    int iterPerThread = config.iterations / config.producerThreads;
    std::vector<std::vector<int64_t>> threadLatencies(config.producerThreads);
    std::atomic<bool> startFlag{false};
    std::atomic<bool> stopConsumer{false};

    // Slow consumer (simulates backpressure) / 慢消费者（模拟背压）
    std::thread consumer([&]() {
        oneplog::LogEntry entry;
        while (!stopConsumer.load(std::memory_order_relaxed)) {
            if (buffer.TryPop(entry)) {
                // Simulate slow processing / 模拟慢处理
                std::this_thread::sleep_for(std::chrono::microseconds(10));
            } else {
                std::this_thread::yield();
            }
        }
        // Drain remaining / 排空剩余
        while (buffer.TryPop(entry)) {}
    });

    // Producer threads / 生产者线程
    std::vector<std::thread> producers;
    for (int t = 0; t < config.producerThreads; ++t) {
        threadLatencies[t].reserve(iterPerThread);
        producers.emplace_back([&, t]() {
            while (!startFlag.load(std::memory_order_relaxed)) {
                std::this_thread::yield();
            }
            
            for (int i = 0; i < iterPerThread; ++i) {
                oneplog::LogEntry entry;
                entry.level = oneplog::Level::Info;

                auto t1 = Clock::now();
                buffer.TryPush(std::move(entry));
                auto t2 = Clock::now();
                
                threadLatencies[t].push_back(
                    std::chrono::duration_cast<Duration>(t2 - t1).count());
            }
        });
    }

    auto start = Clock::now();
    startFlag = true;

    for (auto& th : producers) {
        th.join();
    }
    
    auto end = Clock::now();
    stopConsumer = true;
    consumer.join();

    std::vector<int64_t> allLatencies;
    for (auto& tl : threadLatencies) {
        allLatencies.insert(allLatencies.end(), tl.begin(), tl.end());
    }

    double totalTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalculateStats(allLatencies, totalTimeMs, buffer.GetDroppedCount());
}

// ==============================================================================
// Test 3: Burst traffic pattern
// 测试 3：突发流量模式
// ==============================================================================

Statistics BenchmarkBurstTraffic(const TestConfig& config) {
    oneplog::HeapRingBuffer<oneplog::LogEntry, false> buffer(config.queueSize);
    buffer.SetPolicy(oneplog::QueueFullPolicy::DropNewest);
    
    std::vector<int64_t> latencies;
    latencies.reserve(config.iterations);
    std::atomic<bool> stopConsumer{false};
    std::atomic<size_t> consumed{0};

    // Consumer thread / 消费者线程
    std::thread consumer([&]() {
        oneplog::LogEntry entry;
        while (!stopConsumer.load(std::memory_order_relaxed)) {
            if (buffer.TryPop(entry)) {
                consumed.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
        while (buffer.TryPop(entry)) {
            consumed.fetch_add(1, std::memory_order_relaxed);
        }
    });

    auto start = Clock::now();
    
    // Burst pattern: send 100 logs, pause, repeat
    // 突发模式：发送 100 条日志，暂停，重复
    int burstSize = 100;
    int totalBursts = config.iterations / burstSize;
    
    for (int burst = 0; burst < totalBursts; ++burst) {
        // Send burst / 发送突发
        for (int i = 0; i < burstSize; ++i) {
            oneplog::LogEntry entry;
            entry.level = oneplog::Level::Info;

            auto t1 = Clock::now();
            buffer.TryPush(std::move(entry));
            auto t2 = Clock::now();
            
            latencies.push_back(
                std::chrono::duration_cast<Duration>(t2 - t1).count());
        }
        
        // Small pause between bursts / 突发之间的小暂停
        if (burst % 10 == 0) {
            std::this_thread::sleep_for(std::chrono::microseconds(1));
        }
    }
    
    auto end = Clock::now();
    stopConsumer = true;
    consumer.join();

    double totalTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalculateStats(latencies, totalTimeMs, buffer.GetDroppedCount());
}

// ==============================================================================
// Test 4: Sustained high throughput
// 测试 4：持续高吞吐量
// ==============================================================================

Statistics BenchmarkSustainedThroughput(const TestConfig& config) {
    // Larger queue for sustained throughput test
    // 较大队列用于持续吞吐量测试
    oneplog::HeapRingBuffer<oneplog::LogEntry, false> buffer(65536);
    buffer.SetPolicy(oneplog::QueueFullPolicy::DropNewest);
    
    int iterPerThread = config.iterations / config.producerThreads;
    std::vector<std::vector<int64_t>> threadLatencies(config.producerThreads);
    std::atomic<bool> startFlag{false};
    std::atomic<bool> stopConsumer{false};

    // Fast consumer / 快速消费者
    std::thread consumer([&]() {
        oneplog::LogEntry entry;
        while (!stopConsumer.load(std::memory_order_relaxed)) {
            buffer.TryPop(entry);
        }
        while (buffer.TryPop(entry)) {}
    });

    std::vector<std::thread> producers;
    for (int t = 0; t < config.producerThreads; ++t) {
        threadLatencies[t].reserve(iterPerThread);
        producers.emplace_back([&, t]() {
            while (!startFlag.load(std::memory_order_relaxed)) {
                std::this_thread::yield();
            }
            
            for (int i = 0; i < iterPerThread; ++i) {
                oneplog::LogEntry entry;
                entry.level = oneplog::Level::Info;
                entry.timestamp = static_cast<uint64_t>(i);

                auto t1 = Clock::now();
                buffer.TryPush(std::move(entry));
                auto t2 = Clock::now();
                
                threadLatencies[t].push_back(
                    std::chrono::duration_cast<Duration>(t2 - t1).count());
            }
        });
    }

    auto start = Clock::now();
    startFlag = true;

    for (auto& th : producers) {
        th.join();
    }
    
    auto end = Clock::now();
    stopConsumer = true;
    consumer.join();

    std::vector<int64_t> allLatencies;
    for (auto& tl : threadLatencies) {
        allLatencies.insert(allLatencies.end(), tl.begin(), tl.end());
    }

    double totalTimeMs = std::chrono::duration<double, std::milli>(end - start).count();
    return CalculateStats(allLatencies, totalTimeMs, buffer.GetDroppedCount());
}

// ==============================================================================
// Main / 主函数
// ==============================================================================

void PrintHeader() {
    std::cout << "\n";
    std::cout << "================================================================\n";
    std::cout << "    Shadow Tail Optimization Benchmark\n";
    std::cout << "    Shadow Tail 优化基准测试\n";
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
    std::cout << "  Shadow tail interval:           " 
              << oneplog::RingBufferHeader::kShadowTailUpdateInterval << "\n";
}

void PrintConfig(const TestConfig& config) {
    std::cout << "\n";
    std::cout << "  Test Config / 测试配置\n";
    std::cout << "  ----------------------------------------------------------------\n";
    std::cout << "  Iterations / 迭代次数:          " << config.iterations << "\n";
    std::cout << "  Producer threads / 生产者线程:  " << config.producerThreads << "\n";
    std::cout << "  Queue size / 队列大小:          " << config.queueSize << "\n";
}

int main(int argc, char* argv[]) {
    PrintHeader();
    PrintSystemInfo();

    TestConfig config;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-i" && i + 1 < argc) {
            config.iterations = std::atoi(argv[++i]);
        } else if (arg == "-t" && i + 1 < argc) {
            config.producerThreads = std::atoi(argv[++i]);
        } else if (arg == "-q" && i + 1 < argc) {
            config.queueSize = std::atoi(argv[++i]);
        } else if (arg == "-h" || arg == "--help") {
            std::cout << "\nUsage / 用法: benchmark_shadow_tail [options]\n";
            std::cout << "  -i <num>    Iterations / 迭代次数 (default: 1000000)\n";
            std::cout << "  -t <num>    Producer threads / 生产者线程数 (default: 8)\n";
            std::cout << "  -q <num>    Queue size / 队列大小 (default: 1024)\n";
            std::cout << "  -h          Show help / 显示帮助\n";
            return 0;
        }
    }

    PrintConfig(config);

    std::cout << "\n================================================================\n";
    std::cout << "  Running benchmarks... / 运行基准测试...\n";
    std::cout << "================================================================\n";

    // Test 1: MPSC high contention
    std::cout << "\n>>> [1/4] MPSC High Contention / 多生产者单消费者高竞争...\n";
    auto mpscStats = BenchmarkMPSC(config);
    PrintStats("MPSC High Contention (queue=" + std::to_string(config.queueSize) + ")", mpscStats);

    // Test 2: Queue full scenario
    std::cout << "\n>>> [2/4] Queue Full Scenario / 队列满场景...\n";
    auto fullStats = BenchmarkQueueFull(config);
    PrintStats("Queue Full (DropNewest, queue=64)", fullStats);

    // Test 3: Burst traffic
    std::cout << "\n>>> [3/4] Burst Traffic / 突发流量...\n";
    auto burstStats = BenchmarkBurstTraffic(config);
    PrintStats("Burst Traffic (100 logs/burst)", burstStats);

    // Test 4: Sustained throughput
    std::cout << "\n>>> [4/4] Sustained Throughput / 持续高吞吐量...\n";
    auto sustainedStats = BenchmarkSustainedThroughput(config);
    PrintStats("Sustained Throughput (queue=65536)", sustainedStats);

    // Summary
    std::cout << "\n================================================================\n";
    std::cout << "                      Summary / 总结\n";
    std::cout << "================================================================\n";
    std::cout << std::left;
    std::cout << "  " << std::setw(35) << "MPSC High Contention" 
              << FormatThroughput(mpscStats.throughput) << " ops/sec"
              << "  (dropped: " << mpscStats.droppedCount << ")\n";
    std::cout << "  " << std::setw(35) << "Queue Full (DropNewest)" 
              << FormatThroughput(fullStats.throughput) << " ops/sec"
              << "  (dropped: " << fullStats.droppedCount << ")\n";
    std::cout << "  " << std::setw(35) << "Burst Traffic" 
              << FormatThroughput(burstStats.throughput) << " ops/sec"
              << "  (dropped: " << burstStats.droppedCount << ")\n";
    std::cout << "  " << std::setw(35) << "Sustained Throughput" 
              << FormatThroughput(sustainedStats.throughput) << " ops/sec"
              << "  (dropped: " << sustainedStats.droppedCount << ")\n";
    std::cout << "================================================================\n";

    std::cout << "\nBenchmark complete! / 基准测试完成!\n\n";

    return 0;
}
