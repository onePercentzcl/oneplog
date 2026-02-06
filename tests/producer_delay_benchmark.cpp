/**
 * @file producer_delay_benchmark.cpp
 * @brief Benchmark producer delay impact on consumer performance
 * @brief 测试生产者延时对消费者性能的影响
 */

#include "oneplog/ring_buffer.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

using namespace oneplog::internal;

template<int DelayNs>
void BenchmarkProducerDelay(int numProducers, size_t durationMs) {
    constexpr size_t kBufferSize = 256;
    RingBuffer<512, kBufferSize> buffer;
    buffer.Init(QueueFullPolicy::DropNewest);
    
    std::atomic<bool> stopFlag{false};
    std::atomic<uint64_t> consumerCount{0};
    std::atomic<uint64_t> producerCount{0};
    
    // Pre-fill buffer to 50%
    // 预填充缓冲区到 50%
    for (size_t i = 0; i < kBufferSize / 2; ++i) {
        uint64_t data = i;
        buffer.TryPush(&data, sizeof(data));
    }
    
    // Producer threads
    // 生产者线程
    std::vector<std::thread> producers;
    for (int i = 0; i < numProducers; ++i) {
        producers.emplace_back([&, producerId = i]() {
            uint64_t data = producerId;
            while (!stopFlag.load(std::memory_order_relaxed)) {
                if (buffer.TryPush(&data, sizeof(data))) {
                    producerCount.fetch_add(1, std::memory_order_relaxed);
                    ++data;
                    
                    // Add delay after successful push
                    // 成功推送后增加延时
                    if constexpr (DelayNs > 0) {
                        if constexpr (DelayNs < 1000) {
                            // For very short delays, use CPU pause
                            // 对于非常短的延时，使用 CPU pause
                            for (int j = 0; j < DelayNs / 10; ++j) {
                                CPU_PAUSE();
                            }
                        } else {
                            std::this_thread::sleep_for(std::chrono::nanoseconds(DelayNs));
                        }
                    }
                }
            }
        });
    }
    
    // Consumer thread
    // 消费者线程
    std::thread consumer([&]() {
        char readBuffer[512];
        size_t readSize = sizeof(readBuffer);
        while (!stopFlag.load(std::memory_order_relaxed)) {
            if (buffer.TryPop(readBuffer, readSize)) {
                consumerCount.fetch_add(1, std::memory_order_relaxed);
                readSize = sizeof(readBuffer);
            }
        }
    });
    
    std::this_thread::sleep_for(std::chrono::milliseconds(durationMs));
    stopFlag.store(true, std::memory_order_relaxed);
    
    consumer.join();
    for (auto& p : producers) {
        p.join();
    }
    
    double consumerOps = consumerCount.load() * 1000.0 / durationMs;
    double producerOps = producerCount.load() * 1000.0 / durationMs;
    uint64_t dropped = buffer.GetDroppedCount();
    
    std::cout << std::setw(12) << DelayNs
              << std::setw(12) << numProducers
              << std::setw(18) << std::fixed << std::setprecision(0) << consumerOps
              << std::setw(18) << producerOps
              << std::setw(15) << dropped
              << std::endl;
}

int main() {
    std::cout << "\n=== Producer Delay Impact Benchmark ===\n" << std::endl;
    std::cout << std::setw(12) << "Delay(ns)"
              << std::setw(12) << "Producers"
              << std::setw(18) << "Consumer ops/s"
              << std::setw(18) << "Producer ops/s"
              << std::setw(15) << "Dropped"
              << std::endl;
    std::cout << std::string(75, '-') << std::endl;
    
    constexpr size_t kDurationMs = 1000;
    
    // Test with 4 producers and different delays
    // 测试 4 个生产者和不同延时
    std::cout << "\n--- 4 Producers ---" << std::endl;
    BenchmarkProducerDelay<0>(4, kDurationMs);      // No delay / 无延时
    BenchmarkProducerDelay<10>(4, kDurationMs);     // 10ns (1 CPU pause)
    BenchmarkProducerDelay<50>(4, kDurationMs);     // 50ns (5 CPU pauses)
    BenchmarkProducerDelay<100>(4, kDurationMs);    // 100ns (10 CPU pauses)
    BenchmarkProducerDelay<500>(4, kDurationMs);    // 500ns (50 CPU pauses)
    BenchmarkProducerDelay<1000>(4, kDurationMs);   // 1us
    BenchmarkProducerDelay<5000>(4, kDurationMs);   // 5us
    BenchmarkProducerDelay<10000>(4, kDurationMs);  // 10us
    
    // Test with 8 producers and different delays
    // 测试 8 个生产者和不同延时
    std::cout << "\n--- 8 Producers ---" << std::endl;
    BenchmarkProducerDelay<0>(8, kDurationMs);      // No delay / 无延时
    BenchmarkProducerDelay<10>(8, kDurationMs);     // 10ns
    BenchmarkProducerDelay<50>(8, kDurationMs);     // 50ns
    BenchmarkProducerDelay<100>(8, kDurationMs);    // 100ns
    BenchmarkProducerDelay<500>(8, kDurationMs);    // 500ns
    BenchmarkProducerDelay<1000>(8, kDurationMs);   // 1us
    BenchmarkProducerDelay<5000>(8, kDurationMs);   // 5us
    BenchmarkProducerDelay<10000>(8, kDurationMs);  // 10us
    
    std::cout << "\n=== Benchmark Complete ===\n" << std::endl;
    return 0;
}
