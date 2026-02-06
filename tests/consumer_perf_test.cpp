/**
 * @file consumer_perf_test.cpp
 * @brief 单线程消费者极限性能测试
 */

#include "oneplog/ring_buffer.hpp"
#include <chrono>
#include <iostream>

using namespace oneplog::internal;

int main() {
    constexpr size_t kBufferSize = 1024;
    constexpr size_t kIterations = 1000000;
    
    RingBuffer<512, kBufferSize> buffer;
    buffer.Init(QueueFullPolicy::DropNewest);
    
    // 预填充一半
    for (size_t i = 0; i < kBufferSize / 2; ++i) {
        uint64_t data = i;
        buffer.TryPush(&data, sizeof(data));
    }
    
    char readBuffer[512];
    size_t readSize;
    uint64_t popCount = 0;
    uint64_t pushCount = 0;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < kIterations; ++i) {
        readSize = sizeof(readBuffer);
        if (buffer.TryPop(readBuffer, readSize)) {
            ++popCount;
            // 立即补充
            uint64_t data = i;
            if (buffer.TryPush(&data, sizeof(data))) {
                ++pushCount;
            }
        }
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double opsPerSec = popCount * 1000000.0 / duration.count();
    
    std::cout << "单线程消费者极限性能测试:" << std::endl;
    std::cout << "- 迭代次数: " << kIterations << std::endl;
    std::cout << "- Pop 成功: " << popCount << std::endl;
    std::cout << "- 耗时: " << duration.count() << " us" << std::endl;
    std::cout << "- 吞吐量: " << opsPerSec << " ops/sec" << std::endl;
    std::cout << "- 平均延迟: " << (duration.count() * 1000.0 / popCount) << " ns/op" << std::endl;
    
    return 0;
}
