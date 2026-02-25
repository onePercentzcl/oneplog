/**
 * @file test_async_format.cpp
 * @brief Test with real MessageOnlyFormat pattern
 */

#include <iostream>
#include <thread>
#include <atomic>
#include <memory>
#include <oneplog/internal/heap_memory.hpp>
#include <oneplog/internal/log_entry.hpp>
#include <oneplog/common.hpp>
#include <fmt/format.h>

// Copy of NullSinkType
struct NullSinkType {
    void Write(std::string_view) noexcept {}
    void Flush() noexcept {}
    void Close() noexcept {}
};

// Test with real MessageOnlyFormat
template<bool EnableWFC = false, bool EnableShadowTail = false>
class FormatFastLogger {
public:
    FormatFastLogger() : m_sink{} { 
        InitAsync(8192, oneplog::QueueFullPolicy::Block);
    }

    ~FormatFastLogger() { 
        StopWorker();
        m_sink.Close();
    }

    template<typename... Args>
    void Info(const char* fmt, Args&&... args) noexcept {
        LogImplAsync(fmt, std::forward<Args>(args)...);
    }

    void Flush() noexcept {
        if (m_ringBuffer) {
            while (!m_ringBuffer->IsEmpty()) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
        m_sink.Flush();
    }

private:
    void InitAsync(size_t bufferSize, oneplog::QueueFullPolicy policy) {
        m_ringBuffer = std::make_unique<oneplog::internal::HeapRingBuffer<oneplog::LogEntry, EnableWFC, EnableShadowTail>>(
            bufferSize, policy);
        StartWorker();
    }

    template<typename... Args>
    void LogImplAsync(const char* fmt, Args&&... args) noexcept {
        if (!m_ringBuffer) return;

        oneplog::LogEntry entry;
        entry.timestamp = 12345;
        entry.level = oneplog::Level::Info;
        entry.threadId = 1;
        entry.processId = 1;

        // Capture format string and arguments to BinarySnapshot
        if constexpr (sizeof...(Args) == 0) {
            entry.snapshot.CaptureStringView(std::string_view(fmt));
        } else {
            entry.snapshot.CaptureStringView(std::string_view(fmt));
            entry.snapshot.Capture(std::forward<Args>(args)...);
        }

        m_ringBuffer->TryPush(std::move(entry));
        m_ringBuffer->NotifyConsumer();
    }

    void StartWorker() {
        if (m_running.load(std::memory_order_acquire)) {
            return;
        }
        
        m_running.store(true, std::memory_order_release);
        m_worker = std::thread([this] {
            while (m_running.load(std::memory_order_acquire)) {
                bool hasData = false;
                oneplog::LogEntry entry;
                
                while (m_running.load(std::memory_order_relaxed) &&
                       m_ringBuffer && m_ringBuffer->TryPop(entry)) {
                    fmt::memory_buffer buffer;
                    // Same pattern as MessageOnlyFormat::FormatEntryTo
                    std::string msg = entry.snapshot.FormatAll();
                    buffer.append(std::string_view(msg));
                    m_sink.Write(std::string_view(buffer.data(), buffer.size()));
                    hasData = true;
                }
                
                if (!hasData && m_running.load(std::memory_order_relaxed)) {
                    if (m_ringBuffer) {
                        m_ringBuffer->WaitForData(std::chrono::microseconds(100), 
                                                  std::chrono::milliseconds(10));
                    }
                }
            }
            
            if (m_ringBuffer) {
                oneplog::LogEntry entry;
                while (m_ringBuffer->TryPop(entry)) {
                    fmt::memory_buffer buffer;
                    std::string msg = entry.snapshot.FormatAll();
                    buffer.append(std::string_view(msg));
                    m_sink.Write(std::string_view(buffer.data(), buffer.size()));
                }
            }
            
            m_sink.Flush();
        });
    }

    void StopWorker() {
        if (!m_running.load(std::memory_order_acquire)) {
            return;
        }
        
        m_running.store(false, std::memory_order_release);
        
        if (m_ringBuffer) {
            m_ringBuffer->NotifyConsumer();
        }
        
        if (m_worker.joinable()) {
            m_worker.join();
        }
        
        if (m_ringBuffer) {
            oneplog::LogEntry entry;
            while (m_ringBuffer->TryPop(entry)) {
                fmt::memory_buffer buffer;
                std::string msg = entry.snapshot.FormatAll();
                buffer.append(std::string_view(msg));
                m_sink.Write(std::string_view(buffer.data(), buffer.size()));
            }
        }
    }

    std::unique_ptr<oneplog::internal::HeapRingBuffer<oneplog::LogEntry, EnableWFC, EnableShadowTail>> m_ringBuffer;
    std::thread m_worker;
    std::atomic<bool> m_running{false};
    NullSinkType m_sink{};
};

int main() {
    std::cout << "Testing FormatFastLogger (with real MessageOnlyFormat)...\n";
    
    for (int run = 0; run < 20; ++run) {
        std::cout << "Run " << run << " starting...\n";
        
        {
            FormatFastLogger<false, false> logger;
            
            for (int i = 0; i < 100000; ++i) {
                logger.Info("Message {} value {}", i, i * 2);
            }
            
            logger.Flush();
        }
        
        std::cout << "Run " << run << " complete.\n";
    }
    
    std::cout << "All runs complete!\n";
    return 0;
}
