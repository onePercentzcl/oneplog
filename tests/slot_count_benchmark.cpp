/**
 * @file slot_count_benchmark.cpp
 * @brief Benchmark different slot counts for RingBuffer
 * @brief 测试不同槽位数量对 RingBuffer 性能的影响
 */

#include "oneplog/ring_buffer.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

using namespace oneplog::internal;

template<size_t SlotCount>
void BenchmarkSlotCount(int numProducers, size_t durationMs) {
    RingBuffer<512, SlotCount> buffer;
    buffer.Init(QueueFullPolicy::DropNewest);
    
    std::atomic<bool> stopFlag{false};
    std::atomic<uint64_t> consumerCount{0};
    std::atomic<uint64_t> producerCount{0};
    
    if (numProducers == 0) {
        // Baseline: single thread push+pop
        // 基线：单线程 push+pop
        auto start = std::chrono::high_resolution_clock::now();
        uint64_t count = 0;
        uint64_t data = 0;
        char readBuffer[512];
        size_t readSize;
        
        while (true) {
            auto now = std::chrono::high_resolution_clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count() >= (int64_t)durationMs) {
                break;
            }
            
            if (buffer.TryPush(&data, sizeof(data))) {
                readSize = sizeof(readBuffer);
                if (buffer.TryPop(readBuffer, readSize)) {
                    ++count;
                }
                ++data;
            }
        }
        
        double ops = count * 1000.0 / durationMs;
        std::cout << std::setw(10) << SlotCount
                  << std::setw(12) << 0
                  << std::setw(18) << std::fixed << std::setprecision(0) << ops
                  << std::setw(18) << ops
                  << std::setw(15) << 0
                  << std::endl;
        return;
    }
    
    // Pre-fill buffer to 50%
    for (size_t i = 0; i < SlotCount / 2; ++i) {
        uint64_t data = i;
        buffer.TryPush(&data, sizeof(data));
    }
    
    // Producer threads
    std::vector<std::thread> producers;
    for (int i = 0; i < numProducers; ++i) {
        producers.emplace_back([&, producerId = i]() {
            uint64_t data = producerId;
            while (!stopFlag.load(std::memory_order_relaxed)) {
                if (buffer.TryPush(&data, sizeof(data))) {
                    producerCount.fetch_add(1, std::memory_order_relaxed);
                    ++data;
                }
            }
        });
    }
    
    // Consumer thread
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
    
    std::cout << std::setw(10) << SlotCount
              << std::setw(12) << numProducers
              << std::setw(18) << std::fixed << std::setprecision(0) << consumerOps
              << std::setw(18) << producerOps
              << std::setw(15) << dropped
              << std::endl;
}

int main() {
    std::cout << "\n=== Slot Count Performance Benchmark ===\n" << std::endl;
    std::cout << std::setw(10) << "SlotCount"
              << std::setw(12) << "Producers"
              << std::setw(18) << "Consumer ops/s"
              << std::setw(18) << "Producer ops/s"
              << std::setw(15) << "Dropped"
              << std::endl;
    std::cout << std::string(73, '-') << std::endl;
    
    constexpr size_t kDurationMs = 1000;
    
    // Test with 0 producers (baseline)
    std::cout << "\n--- Baseline (0 producers) ---" << std::endl;
    BenchmarkSlotCount<64>(0, kDurationMs);
    BenchmarkSlotCount<128>(0, kDurationMs);
    BenchmarkSlotCount<256>(0, kDurationMs);
    BenchmarkSlotCount<512>(0, kDurationMs);
    BenchmarkSlotCount<1024>(0, kDurationMs);
    BenchmarkSlotCount<2048>(0, kDurationMs);
    BenchmarkSlotCount<4096>(0, kDurationMs);
    
    // Test with 4 producers
    std::cout << "\n--- 4 Producers ---" << std::endl;
    BenchmarkSlotCount<64>(4, kDurationMs);
    BenchmarkSlotCount<128>(4, kDurationMs);
    BenchmarkSlotCount<256>(4, kDurationMs);
    BenchmarkSlotCount<512>(4, kDurationMs);
    BenchmarkSlotCount<1024>(4, kDurationMs);
    BenchmarkSlotCount<2048>(4, kDurationMs);
    BenchmarkSlotCount<4096>(4, kDurationMs);
    
    // Test with 8 producers
    std::cout << "\n--- 8 Producers ---" << std::endl;
    BenchmarkSlotCount<64>(8, kDurationMs);
    BenchmarkSlotCount<128>(8, kDurationMs);
    BenchmarkSlotCount<256>(8, kDurationMs);
    BenchmarkSlotCount<512>(8, kDurationMs);
    BenchmarkSlotCount<1024>(8, kDurationMs);
    BenchmarkSlotCount<2048>(8, kDurationMs);
    BenchmarkSlotCount<4096>(8, kDurationMs);
    
    std::cout << "\n=== Benchmark Complete ===\n" << std::endl;
    return 0;
}
