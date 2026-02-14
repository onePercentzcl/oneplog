/**
 * @file test_async_real.cpp
 * @brief Test real FastLogger
 */

#include <iostream>
#include <thread>
#include <atomic>
#include <oneplog/fast_logger.hpp>

int main() {
    std::cout << "Testing REAL FastLogger async mode...\n";
    
    for (int run = 0; run < 20; ++run) {
        std::cout << "Run " << run << " starting...\n";
        
        {
            // Use the real FastLogger with NullSinkType
            oneplog::FastLogger<oneplog::Mode::Async, 
                               oneplog::MessageOnlyFormat, 
                               oneplog::NullSinkType, 
                               oneplog::Level::Info>
                logger;
            
            std::cout << "  Logger created\n";
            
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
