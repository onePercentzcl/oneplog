/**
 * @file test_async_exact.cpp
 * @brief Exact copy of FastLogger async implementation for debugging
 */

#include <iostream>
#include <thread>
#include <atomic>
#include <memory>
#include <oneplog/internal/heap_memory.hpp>
#include <oneplog/internal/log_entry.hpp>
#include <oneplog/common.hpp>

#ifdef ONEPLOG_USE_FMT
#include <fmt/format.h>
#endif

// Copy of NullSinkType
struct NullSinkType {
    void Write(std::string_view) noexcept {}
    void Flush() noexcept {}
    void Close() noexcept {}
};

// Exact copy of FastLogger async implementation
template<bool EnableWFC = false, bool EnableShadowTail = false>
class ExactFastLogger {
public:
    ExactFastLogger() : m_sink{} { 
        InitAsync(8192, oneplog::QueueFullPolicy::Block);
    }

    ~ExactFastLogger() { 
        // Must stop worker before any member is destroyed
        StopWorker();
        m_sink.Close();
        // Note: m_ringBuffer will be destroyed after this
    }

    void Info(const char* fmt, int arg1, int arg2) noexcept {
        LogImplAsync(fmt, arg1, arg2);
    }

    void Flush() noexcept {
        // Wait for ring buffer to drain
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

    void LogImplAsync(const char* fmt, int arg1, int arg2) noexcept {
        if (!m_ringBuffer) return;

        oneplog::LogEntry entry;
        entry.timestamp = 12345;
        entry.level = oneplog::Level::Info;
        entry.threadId = 1;
        entry.processId = 1;

        // Capture format string and arguments to BinarySnapshot
        entry.snapshot.CaptureStringView(std::string_view(fmt));
        entry.snapshot.Capture(arg1, arg2);

        m_ringBuffer->TryPush(std::move(entry));
        m_ringBuffer->NotifyConsumer();
    }

    void StartWorker() {
        // Check if already running
        if (m_running.load(std::memory_order_acquire)) {
            return;
        }
        
        m_running.store(true, std::memory_order_release);
        m_worker = std::thread([this] {
            // Worker thread main loop - follows WriterThread pattern
            while (m_running.load(std::memory_order_acquire)) {
                bool hasData = false;
                oneplog::LogEntry entry;
                
                // Process all available entries
                while (m_running.load(std::memory_order_relaxed) &&
                       m_ringBuffer && m_ringBuffer->TryPop(entry)) {
#ifdef ONEPLOG_USE_FMT
                    fmt::memory_buffer buffer;
                    std::string msg = entry.snapshot.FormatAll();
                    buffer.append(std::string_view(msg));
                    m_sink.Write(std::string_view(buffer.data(), buffer.size()));
#endif
                    hasData = true;
                }
                
                // No data available, wait for notification
                if (!hasData && m_running.load(std::memory_order_relaxed)) {
                    if (m_ringBuffer) {
                        m_ringBuffer->WaitForData(std::chrono::microseconds(100), 
                                                  std::chrono::milliseconds(10));
                    }
                }
            }
            
            // Drain remaining entries before exit
            if (m_ringBuffer) {
                oneplog::LogEntry entry;
                while (m_ringBuffer->TryPop(entry)) {
#ifdef ONEPLOG_USE_FMT
                    fmt::memory_buffer buffer;
                    std::string msg = entry.snapshot.FormatAll();
                    buffer.append(std::string_view(msg));
                    m_sink.Write(std::string_view(buffer.data(), buffer.size()));
#endif
                }
            }
            
            m_sink.Flush();
        });
    }

    void StopWorker() {
        // Check if worker is running
        if (!m_running.load(std::memory_order_acquire)) {
            return;
        }
        
        // Signal worker to stop
        m_running.store(false, std::memory_order_release);
        
        // Wake up worker if it's waiting
        if (m_ringBuffer) {
            m_ringBuffer->NotifyConsumer();
        }
        
        // Wait for worker to finish
        if (m_worker.joinable()) {
            m_worker.join();
        }
        
        // Drain any remaining entries after worker has stopped
        // (following the pattern from original WriterThread)
        if (m_ringBuffer) {
            oneplog::LogEntry entry;
            while (m_ringBuffer->TryPop(entry)) {
#ifdef ONEPLOG_USE_FMT
                fmt::memory_buffer buffer;
                std::string msg = entry.snapshot.FormatAll();
                buffer.append(std::string_view(msg));
                m_sink.Write(std::string_view(buffer.data(), buffer.size()));
#endif
            }
        }
    }

    // EXACT same member order as FastLogger
    std::unique_ptr<oneplog::internal::HeapRingBuffer<oneplog::LogEntry, EnableWFC, EnableShadowTail>> m_ringBuffer;
    std::thread m_worker;
    std::atomic<bool> m_running{false};
    NullSinkType m_sink{};
};

int main() {
    std::cout << "Testing ExactFastLogger (exact copy of FastLogger)...\n";
    
    for (int run = 0; run < 20; ++run) {
        std::cout << "Run " << run << " starting...\n";
        
        {
            ExactFastLogger<false, false> logger;
            
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
