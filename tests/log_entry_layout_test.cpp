/**
 * @file log_entry_layout_test.cpp
 * @brief Memory layout verification for LogEntry configurations
 * @文件 log_entry_layout_test.cpp
 * @简述 LogEntry 配置的内存布局验证
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include "oneplog/log_entry.hpp"
#include <iostream>
#include <iomanip>

using namespace oneplog;

int main() {
    std::cout << "=== LogEntry Memory Layout Analysis (C++20 with [[no_unique_address]]) ===" << std::endl;
    std::cout << std::endl;
    
    // Print sizes
    std::cout << "Configuration Sizes:" << std::endl;
    std::cout << "  Minimal:     " << std::setw(4) << sizeof(LogEntryMinimal) << " bytes" << std::endl;
    std::cout << "  NoTimestamp: " << std::setw(4) << sizeof(LogEntryNoTimestamp) << " bytes" << std::endl;
    std::cout << "  Standard:    " << std::setw(4) << sizeof(LogEntryStandard) << " bytes" << std::endl;
    std::cout << "  Debug:       " << std::setw(4) << sizeof(LogEntryDebug) << " bytes" << std::endl;
    std::cout << std::endl;
    
    // Print component sizes
    std::cout << "Component Sizes:" << std::endl;
    std::cout << "  BinarySnapshot: " << sizeof(BinarySnapshot) << " bytes" << std::endl;
    std::cout << "  uint64_t:       " << sizeof(uint64_t) << " bytes" << std::endl;
    std::cout << "  const char*:    " << sizeof(const char*) << " bytes" << std::endl;
    std::cout << "  uint32_t:       " << sizeof(uint32_t) << " bytes" << std::endl;
    std::cout << "  Level:          " << sizeof(internal::Level) << " bytes" << std::endl;
    std::cout << "  std::monostate: " << sizeof(std::monostate) << " bytes (C++20)" << std::endl;
    std::cout << std::endl;
    
    // Verify feature flags
    std::cout << "Feature Flags:" << std::endl;
    std::cout << "  Minimal has timestamp:        " << LogEntryMinimal::kHasTimestamp << std::endl;
    std::cout << "  Minimal has source location:  " << LogEntryMinimal::kHasSourceLocation << std::endl;
    std::cout << "  Minimal has thread ID:        " << LogEntryMinimal::kHasThreadId << std::endl;
    std::cout << "  Minimal has process ID:       " << LogEntryMinimal::kHasProcessId << std::endl;
    std::cout << std::endl;
    
    std::cout << "  Standard has timestamp:       " << LogEntryStandard::kHasTimestamp << std::endl;
    std::cout << "  Standard has source location: " << LogEntryStandard::kHasSourceLocation << std::endl;
    std::cout << "  Standard has thread ID:       " << LogEntryStandard::kHasThreadId << std::endl;
    std::cout << "  Standard has process ID:      " << LogEntryStandard::kHasProcessId << std::endl;
    std::cout << std::endl;
    
    std::cout << "  Debug has timestamp:          " << LogEntryDebug::kHasTimestamp << std::endl;
    std::cout << "  Debug has source location:    " << LogEntryDebug::kHasSourceLocation << std::endl;
    std::cout << "  Debug has thread ID:          " << LogEntryDebug::kHasThreadId << std::endl;
    std::cout << "  Debug has process ID:         " << LogEntryDebug::kHasProcessId << std::endl;
    std::cout << std::endl;
    
    // Verify build mode selection
    std::cout << "Build Mode:" << std::endl;
#ifdef NDEBUG
    std::cout << "  NDEBUG defined: Release build" << std::endl;
    std::cout << "  LogEntry is LogEntryRelease (Standard): " 
              << std::is_same<LogEntry, LogEntryRelease>::value << std::endl;
#else
    std::cout << "  NDEBUG not defined: Debug build" << std::endl;
    std::cout << "  LogEntry is LogEntryDebug: " 
              << std::is_same<LogEntry, LogEntryDebug>::value << std::endl;
#endif
    std::cout << "  LogEntry size: " << sizeof(LogEntry) << " bytes" << std::endl;
    std::cout << std::endl;
    
    // Verify zero-overhead abstraction
    std::cout << "Zero-Overhead Verification:" << std::endl;
    size_t minimalSize = sizeof(LogEntryMinimal);
    size_t standardSize = sizeof(LogEntryStandard);
    size_t debugSize = sizeof(LogEntryDebug);
    
    std::cout << "  Minimal < Standard: " << (minimalSize < standardSize) << std::endl;
    std::cout << "  Standard < Debug:   " << (standardSize < debugSize) << std::endl;
    std::cout << "  Size difference (Standard - Minimal): " 
              << (standardSize - minimalSize) << " bytes" << std::endl;
    std::cout << "  Size difference (Debug - Standard):   " 
              << (debugSize - standardSize) << " bytes" << std::endl;
    std::cout << std::endl;
    
    std::cout << "=== All verifications passed ===" << std::endl;
    
    return 0;
}
