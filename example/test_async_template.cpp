/**
 * @file test_async_template.cpp
 * @brief Test FastLogger template instantiation
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

// Copy of StaticFormatRequirements
template<bool NeedsTimestamp = true, bool NeedsLevel = true,
         bool NeedsThreadId = false, bool NeedsProcessId = false>
struct StaticFormatRequirements {
    static constexpr bool kNeedsTimestamp = NeedsTimestamp;
    static constexpr bool kNeedsLevel = NeedsLevel;
    static constexpr bool kNeedsThreadId = NeedsThreadId;
    static constexpr bool kNeedsProcessId = NeedsProcessId;
};

// Copy of MessageOnlyFormat
struct MessageOnlyFormat {
    using Requirements = StaticFormatRequirements<false, false, false, false>;

#ifdef ONEPLOG_USE_FMT
    template<typename... Args>
    static void FormatTo(fmt::memory_buffer& buffer, oneplog::Level, uint64_t, uint32_t, uint32_t,
                         const char* fmt, Args&&... args) {
        if constexpr (sizeof...(Args) == 0) {
            buffer.append(std::string_view(fmt));
        } else {
            fmt::format_to(std::back_inserter(buffer), fmt::runtime(fmt), std::forward<Args>(args)...);
        }
    }
    
    static void FormatEntryTo(fmt::memory_buffer& buffer, const oneplog::LogEntry& entry) {
        std::string msg = entry.snapshot.FormatAll();
        buffer.append(std::string_view(msg));
    }
#endif
};

// Exact copy of FastLogger template
template<oneplog::Mode M = oneplog::Mode::Sync,
         typename FormatType = MessageOnlyFormat,
         typename SinkType = NullSinkType,
         oneplog::Level L = oneplog::Level::Info,
         bool EnableWFC = false,
         bool EnableShadowTail = false>
class TemplateFastLogger {
public:
    static constexpr oneplog::Mode kMode = M;
    static constexpr oneplog::Level kMinLevel = L;
    using Format = FormatType;
    using Sink = SinkType;
    using Req = typename FormatType::Requirements;

    TemplateFastLogger() : m_sink{} { 
        if constexpr (M == oneplog::Mode::Async) {
            InitAsync(8192, oneplog::QueueFullPolicy::Block);
        }
    }
    
    explicit TemplateFastLogger(SinkType sink) : m_sink(std::move(sink)) {
        if constexpr (M == oneplog::Mode::Async) {
            InitAsync(8192, oneplog::QueueFullPolicy::Block);
        }
    }

    ~TemplateFastLogger() { 
        if constexpr (M == oneplog::Mode::Async) {
            StopWorker();
        }
        m_sink.Close();
    }

    TemplateFastLogger(const TemplateFastLogger&) = delete;
    TemplateFastLogger& operator=(const TemplateFastLogger&) = delete;
    TemplateFastLogger(TemplateFastLogger&& o) noexcept : m_sink(std::move(o.m_sink)) {
        if constexpr (M == oneplog::Mode::Async) {
            o.StopWorker();
            m_ringBuffer = std::move(o.m_ringBuffer);
            StartWorker();
        }
    }

    template<typename... Args>
    void Info(const char* fmt, Args&&... args) noexcept {
        if constexpr (static_cast<uint8_t>(oneplog::Level::Info) >= static_cast<uint8_t>(L)) {
            LogImpl<oneplog::Level::Info>(fmt, std::forward<Args>(args)...);
        }
    }

    void Flush() noexcept {
        if constexpr (M == oneplog::Mode::Async) {
            if (m_ringBuffer) {
                while (!m_ringBuffer->IsEmpty()) {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
        }
        m_sink.Flush();
    }

private:
    void InitAsync(size_t bufferSize, oneplog::QueueFullPolicy policy) {
        if constexpr (M == oneplog::Mode::Async) {
            m_ringBuffer = std::make_unique<oneplog::internal::HeapRingBuffer<oneplog::LogEntry, EnableWFC, EnableShadowTail>>(
                bufferSize, policy);
            StartWorker();
        }
    }

    template<oneplog::Level LogLevel, typename... Args>
    void LogImpl(const char* fmt, Args&&... args) noexcept {
        if constexpr (M == oneplog::Mode::Sync) {
            LogImplSync<LogLevel>(fmt, std::forward<Args>(args)...);
        } else {
            LogImplAsync<LogLevel>(fmt, std::forward<Args>(args)...);
        }
    }

    template<oneplog::Level LogLevel, typename... Args>
    void LogImplSync(const char* fmt, Args&&... args) noexcept {
        [[maybe_unused]] uint64_t timestamp = 0;
        [[maybe_unused]] uint32_t threadId = 0;
        [[maybe_unused]] uint32_t processId = 0;

#ifdef ONEPLOG_USE_FMT
        fmt::memory_buffer buffer;
        FormatType::FormatTo(buffer, LogLevel, timestamp, threadId, processId,
                             fmt, std::forward<Args>(args)...);
        m_sink.Write(std::string_view(buffer.data(), buffer.size()));
#endif
    }

    template<oneplog::Level LogLevel, typename... Args>
    void LogImplAsync(const char* fmt, Args&&... args) noexcept {
        if (!m_ringBuffer) return;

        oneplog::LogEntry entry;
        entry.timestamp = 12345;
        entry.level = LogLevel;
        entry.threadId = 1;
        entry.processId = 1;

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
        if constexpr (M == oneplog::Mode::Async) {
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
#ifdef ONEPLOG_USE_FMT
                        fmt::memory_buffer buffer;
                        FormatType::FormatEntryTo(buffer, entry);
                        m_sink.Write(std::string_view(buffer.data(), buffer.size()));
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
                
                if (m_ringBuffer) {
                    oneplog::LogEntry entry;
                    while (m_ringBuffer->TryPop(entry)) {
#ifdef ONEPLOG_USE_FMT
                        fmt::memory_buffer buffer;
                        FormatType::FormatEntryTo(buffer, entry);
                        m_sink.Write(std::string_view(buffer.data(), buffer.size()));
#endif
                    }
                }
                
                m_sink.Flush();
            });
        }
    }

    void StopWorker() {
        if constexpr (M == oneplog::Mode::Async) {
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
#ifdef ONEPLOG_USE_FMT
                    fmt::memory_buffer buffer;
                    FormatType::FormatEntryTo(buffer, entry);
                    m_sink.Write(std::string_view(buffer.data(), buffer.size()));
#endif
                }
            }
        }
    }

    std::unique_ptr<oneplog::internal::HeapRingBuffer<oneplog::LogEntry, EnableWFC, EnableShadowTail>> m_ringBuffer;
    std::thread m_worker;
    std::atomic<bool> m_running{false};
    SinkType m_sink{};
};

int main() {
    std::cout << "Testing TemplateFastLogger (exact copy of FastLogger template)...\n";
    
    for (int run = 0; run < 20; ++run) {
        std::cout << "Run " << run << " starting...\n";
        
        {
            TemplateFastLogger<oneplog::Mode::Async, MessageOnlyFormat, NullSinkType, oneplog::Level::Info> logger;
            
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
