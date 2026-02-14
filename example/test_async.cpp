/**
 * @file test_async.cpp
 * @brief Test FastLogger async mode - skip FormatAll
 */

#include <iostream>
#include <thread>
#include <atomic>
#include <oneplog/internal/log_entry.hpp>
#include <oneplog/fast_logger.hpp>

// Custom format that doesn't call FormatAll
struct NoFormatType {
    using Requirements = oneplog::StaticFormatRequirements<false, false, false, false>;

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
    
    // Format from LogEntry - just return empty string (skip FormatAll)
    static void FormatEntryTo(fmt::memory_buffer& buffer, const oneplog::LogEntry& entry) {
        // Don't call FormatAll - just append a fixed string
        buffer.append(std::string_view("test"));
        (void)entry;
    }
#endif
};

int main() {
    std::cout << "Starting FastLogger async test - skip FormatAll...\n";
    
    for (int run = 0; run < 20; ++run) {
        std::cout << "Run " << run << " starting...\n";
        
        {
            // Create FastLogger with custom format that skips FormatAll
            oneplog::FastLogger<oneplog::Mode::Async, NoFormatType, 
                               oneplog::NullSinkType, oneplog::Level::Info> logger;
            
            std::cout << "  Logger created\n";
            
            // Push messages with two int arguments
            for (int i = 0; i < 100000; ++i) {
                logger.Info("Message {} value {}", i, i * 2);
            }
            
            std::cout << "  Messages sent, flushing...\n";
            logger.Flush();
            std::cout << "  Flushed\n";
        }
        
        std::cout << "Run " << run << " complete.\n";
    }
    
    std::cout << "All runs complete!\n";
    return 0;
}
