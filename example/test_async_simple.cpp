/**
 * @file test_async_simple.cpp
 * @brief Minimal test to isolate async memory issue
 */

#include <iostream>
#include <thread>
#include <atomic>
#include <oneplog/internal/heap_memory.hpp>
#include <oneplog/internal/log_entry.hpp>

int main() {
    std::cout << "Test 1: HeapRingBuffer with LogEntry (no BinarySnapshot data)\n";
    
    for (int run = 0; run < 5; ++run) {
        std::cout << "Run " << run << "...\n";
        
        {
            oneplog::internal::HeapRingBuffer<oneplog::LogEntry, false, false> buffer(1024);
            std::atomic<bool> running{true};
            
            // Consumer thread
            std::thread consumer([&]() {
                oneplog::LogEntry entry;
                while (running.load() || !buffer.IsEmpty()) {
                    if (buffer.TryPop(entry)) {
                        // Just consume, don't do anything
                    } else {
                        std::this_thread::yield();
                    }
                }
            });
            
            // Producer - push entries with NO snapshot data
            for (int i = 0; i < 10000; ++i) {
                oneplog::LogEntry entry;
                entry.timestamp = i;
                entry.level = oneplog::Level::Info;
                entry.threadId = 1;
                entry.processId = 1;
                // NO snapshot.Capture() - leave snapshot empty
                
                while (!buffer.TryPush(std::move(entry))) {
                    std::this_thread::yield();
                }
            }
            
            // Wait for consumer to drain
            while (!buffer.IsEmpty()) {
                std::this_thread::yield();
            }
            
            running.store(false);
            consumer.join();
        }
        
        std::cout << "Run " << run << " OK\n";
    }
    
    std::cout << "\nTest 2: HeapRingBuffer with LogEntry (WITH BinarySnapshot data)\n";
    
    for (int run = 0; run < 5; ++run) {
        std::cout << "Run " << run << "...\n";
        
        {
            oneplog::internal::HeapRingBuffer<oneplog::LogEntry, false, false> buffer(1024);
            std::atomic<bool> running{true};
            
            // Consumer thread
            std::thread consumer([&]() {
                oneplog::LogEntry entry;
                while (running.load() || !buffer.IsEmpty()) {
                    if (buffer.TryPop(entry)) {
                        // Just consume, don't do anything
                    } else {
                        std::this_thread::yield();
                    }
                }
            });
            
            // Producer - push entries WITH snapshot data
            for (int i = 0; i < 10000; ++i) {
                oneplog::LogEntry entry;
                entry.timestamp = i;
                entry.level = oneplog::Level::Info;
                entry.threadId = 1;
                entry.processId = 1;
                
                // Capture format string and TWO arguments
                entry.snapshot.CaptureStringView(std::string_view("Message {} value {}"));
                entry.snapshot.Capture(i, i * 2);
                
                while (!buffer.TryPush(std::move(entry))) {
                    std::this_thread::yield();
                }
            }
            
            // Wait for consumer to drain
            while (!buffer.IsEmpty()) {
                std::this_thread::yield();
            }
            
            running.store(false);
            consumer.join();
        }
        
        std::cout << "Run " << run << " OK\n";
    }
    
    std::cout << "\nAll tests passed!\n";
    return 0;
}
