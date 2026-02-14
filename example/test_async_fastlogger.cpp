/**
 * @file test_async_fastlogger.cpp
 * @brief Test FastLogger async mode step by step
 */

#include <iostream>
#include <thread>
#include <atomic>
#include <memory>
#include <oneplog/internal/heap_memory.hpp>
#include <oneplog/internal/log_entry.hpp>

#ifdef ONEPLOG_USE_FMT
#include <fmt/format.h>
#endif

// Simulate FastLogger's worker thread pattern
class TestAsyncLogger {
public:
    TestAsyncLogger() {
        m_ringBuffer = std::make_unique<oneplog::internal::HeapRingBuffer<oneplog::LogEntry, false, false>>(
            8192, oneplog::QueueFullPolicy::Block);
        StartWorker();
    }
    
    ~TestAsyncLogger() {
        StopWorker();
    }
    
    void Log(const char* fmt, int arg1, int arg2) {
        if (!m_ringBuffer) return;
        
        oneplog::LogEntry entry;
        entry.timestamp = 12345;
        entry.level = oneplog::Level::Info;
        entry.threadId = 1;
        entry.processId = 1;
        
        // Capture format string and arguments
        entry.snapshot.CaptureStringView(std::string_view(fmt));
        entry.snapshot.Capture(arg1, arg2);
        
        m_ringBuffer->TryPush(std::move(entry));
        m_ringBuffer->NotifyConsumer();
    }
    
    void Flush() {
        if (m_ringBuffer) {
            while (!m_ringBuffer->IsEmpty()) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    }
    
private:
    void StartWorker() {
        if (m_running.load(std::memory_order_acquire)) return;
        
        m_running.store(true, std::memory_order_release);
        m_worker = std::thread([this] {
            while (m_running.load(std::memory_order_acquire)) {
                bool hasData = false;
                oneplog::LogEntry entry;
                
                while (m_running.load(std::memory_order_relaxed) &&
                       m_ringBuffer && m_ringBuffer->TryPop(entry)) {
#ifdef ONEPLOG_USE_FMT
                    fmt::memory_buffer buffer;
                    // Format the entry - this is where the crash might happen
                    std::string msg = entry.snapshot.FormatAll();
                    buffer.append(std::string_view(msg));
#endif
                    hasData = true;
                }
                
                if (!hasData && m_running.load(std::memory_order_relaxed)) {
                    if (m_ringBuffer) {
                        m_ringBuffer->WaitForData(std::chrono::microseconds(100),
                                                  std::chrono::milliseconds(10));
                    }
                }
            }
            
            // Drain remaining
            if (m_ringBuffer) {
                oneplog::LogEntry entry;
                while (m_ringBuffer->TryPop(entry)) {
#ifdef ONEPLOG_USE_FMT
                    fmt::memory_buffer buffer;
                    std::string msg = entry.snapshot.FormatAll();
                    buffer.append(std::string_view(msg));
#endif
                }
            }
        });
    }
    
    void StopWorker() {
        if (!m_running.load(std::memory_order_acquire)) return;
        
        m_running.store(false, std::memory_order_release);
        
        if (m_ringBuffer) {
            m_ringBuffer->NotifyConsumer();
        }
        
        if (m_worker.joinable()) {
            m_worker.join();
        }
    }
    
    std::unique_ptr<oneplog::internal::HeapRingBuffer<oneplog::LogEntry, false, false>> m_ringBuffer;
    std::thread m_worker;
    std::atomic<bool> m_running{false};
};

int main() {
    std::cout << "Testing TestAsyncLogger (simulates FastLogger)...\n";
    
    for (int run = 0; run < 20; ++run) {
        std::cout << "Run " << run << " starting...\n";
        
        {
            TestAsyncLogger logger;
            
            for (int i = 0; i < 100000; ++i) {
                logger.Log("Message {} value {}", i, i * 2);
            }
            
            logger.Flush();
        }
        
        std::cout << "Run " << run << " complete.\n";
    }
    
    std::cout << "All runs complete!\n";
    return 0;
}
