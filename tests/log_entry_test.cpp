/**
 * @file log_entry_test.cpp
 * @brief Unit tests for LogEntry structures
 * @文件 log_entry_test.cpp
 * @简述 LogEntry 结构的单元测试
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include "oneplog/log_entry.hpp"
#include <gtest/gtest.h>
#include <thread>

using namespace oneplog;
using namespace oneplog::internal;

// ==============================================================================
// SourceLocation Tests / SourceLocation 测试
// ==============================================================================

TEST(SourceLocationTest, DefaultConstructor) {
    SourceLocation loc;
    EXPECT_EQ(loc.file, nullptr);
    EXPECT_EQ(loc.line, 0);
    EXPECT_EQ(loc.function, nullptr);
    EXPECT_FALSE(loc.IsValid());
}

TEST(SourceLocationTest, ParameterizedConstructor) {
    SourceLocation loc("test.cpp", 42, "TestFunction");
    EXPECT_STREQ(loc.file, "test.cpp");
    EXPECT_EQ(loc.line, 42);
    EXPECT_STREQ(loc.function, "TestFunction");
    EXPECT_TRUE(loc.IsValid());
}

TEST(SourceLocationTest, EqualityOperator) {
    SourceLocation loc1("test.cpp", 42, "TestFunction");
    SourceLocation loc2("test.cpp", 42, "TestFunction");
    SourceLocation loc3("other.cpp", 42, "TestFunction");
    SourceLocation loc4("test.cpp", 43, "TestFunction");
    SourceLocation loc5("test.cpp", 42, "OtherFunction");

    EXPECT_EQ(loc1, loc2);
    EXPECT_NE(loc1, loc3);
    EXPECT_NE(loc1, loc4);
    EXPECT_NE(loc1, loc5);
}

TEST(SourceLocationTest, IsValid) {
    SourceLocation valid("test.cpp", 42, "TestFunction");
    SourceLocation invalidFile(nullptr, 42, "TestFunction");
    SourceLocation invalidFunction("test.cpp", 42, nullptr);
    SourceLocation invalidBoth(nullptr, 42, nullptr);

    EXPECT_TRUE(valid.IsValid());
    EXPECT_FALSE(invalidFile.IsValid());
    EXPECT_FALSE(invalidFunction.IsValid());
    EXPECT_FALSE(invalidBoth.IsValid());
}

// ==============================================================================
// LogEntryDebug Tests / LogEntryDebug 测试
// ==============================================================================

TEST(LogEntryDebugTest, DefaultConstructor) {
    LogEntryDebug entry;
    EXPECT_EQ(entry.timestamp, 0);
    EXPECT_EQ(entry.file, nullptr);
    EXPECT_EQ(entry.function, nullptr);
    EXPECT_EQ(entry.threadId, 0);
    EXPECT_EQ(entry.processId, 0);
    EXPECT_EQ(entry.line, 0);
    EXPECT_EQ(entry.level, Level::Info);
    EXPECT_FALSE(entry.IsValid());
}

TEST(LogEntryDebugTest, ParameterizedConstructor) {
    SourceLocation loc("test.cpp", 42, "TestFunction");
    uint64_t timestamp = 1234567890;
    uint32_t threadId = 100;
    uint32_t processId = 200;
    Level level = Level::Error;

    LogEntryDebug entry;
    entry.SetTimestamp<>(timestamp);
    entry.SetLocation<>(loc);
    entry.SetThreadId<>(threadId);
    entry.SetProcessId<>(processId);
    entry.level = level;

    EXPECT_EQ(entry.GetTimestamp<>(), timestamp);
    SourceLocation retrieved = entry.GetLocation<>();
    EXPECT_STREQ(retrieved.file, "test.cpp");
    EXPECT_STREQ(retrieved.function, "TestFunction");
    EXPECT_EQ(retrieved.line, 42);
    EXPECT_EQ(entry.GetThreadId<>(), threadId);
    EXPECT_EQ(entry.GetProcessId<>(), processId);
    EXPECT_EQ(entry.level, level);
    EXPECT_TRUE(entry.IsValid());
}

TEST(LogEntryDebugTest, GetLocation) {
    SourceLocation loc("test.cpp", 42, "TestFunction");
    LogEntryDebug entry;
    entry.SetTimestamp<>(1234567890);
    entry.SetLocation<>(loc);
    entry.SetThreadId<>(100);
    entry.SetProcessId<>(200);
    entry.level = Level::Info;

    SourceLocation retrieved = entry.GetLocation<>();
    EXPECT_EQ(retrieved, loc);
    EXPECT_STREQ(retrieved.file, "test.cpp");
    EXPECT_EQ(retrieved.line, 42);
    EXPECT_STREQ(retrieved.function, "TestFunction");
}

TEST(LogEntryDebugTest, IsValid) {
    SourceLocation loc("test.cpp", 42, "TestFunction");
    
    // Valid entry
    LogEntryDebug validEntry;
    validEntry.SetTimestamp<>(1234567890);
    validEntry.SetLocation<>(loc);
    validEntry.SetThreadId<>(100);
    validEntry.SetProcessId<>(200);
    validEntry.level = Level::Info;
    EXPECT_TRUE(validEntry.IsValid());

    // Invalid: zero timestamp
    LogEntryDebug invalidTimestamp;
    invalidTimestamp.SetLocation<>(loc);
    invalidTimestamp.SetThreadId<>(100);
    invalidTimestamp.SetProcessId<>(200);
    invalidTimestamp.level = Level::Info;
    EXPECT_FALSE(invalidTimestamp.IsValid());

    // Invalid: null file
    SourceLocation invalidLoc(nullptr, 42, "TestFunction");
    LogEntryDebug invalidFile;
    invalidFile.SetTimestamp<>(1234567890);
    invalidFile.SetLocation<>(invalidLoc);
    invalidFile.SetThreadId<>(100);
    invalidFile.SetProcessId<>(200);
    invalidFile.level = Level::Info;
    EXPECT_FALSE(invalidFile.IsValid());

    // Invalid: null function
    SourceLocation invalidLoc2("test.cpp", 42, nullptr);
    LogEntryDebug invalidFunction;
    invalidFunction.SetTimestamp<>(1234567890);
    invalidFunction.SetLocation<>(invalidLoc2);
    invalidFunction.SetThreadId<>(100);
    invalidFunction.SetProcessId<>(200);
    invalidFunction.level = Level::Info;
    EXPECT_FALSE(invalidFunction.IsValid());
}

TEST(LogEntryDebugTest, WithBinarySnapshot) {
    SourceLocation loc("test.cpp", 42, "TestFunction");
    LogEntryDebug entry;
    entry.SetTimestamp<>(1234567890);
    entry.SetLocation<>(loc);
    entry.SetThreadId<>(100);
    entry.SetProcessId<>(200);
    entry.level = Level::Info;

    // Capture some data in the snapshot
    EXPECT_TRUE(entry.snapshot.CaptureInt32(42));
    EXPECT_TRUE(entry.snapshot.CaptureString("Hello"));
    EXPECT_EQ(entry.snapshot.GetArgCount(), 2);

    // Verify the entry is still valid
    EXPECT_TRUE(entry.IsValid());
}

TEST(LogEntryDebugTest, MemoryLayout) {
    // Verify the structure size is reasonable
    // Debug entry should be around 320 bytes
    size_t size = sizeof(LogEntryDebug);
    EXPECT_GT(size, 280);  // At least 280 bytes
    EXPECT_LE(size, 320);  // Less than or equal to 320 bytes
}

// ==============================================================================
// LogEntryRelease Tests / LogEntryRelease 测试
// ==============================================================================

TEST(LogEntryReleaseTest, DefaultConstructor) {
    LogEntryRelease entry;
    EXPECT_EQ(entry.timestamp, 0);
    EXPECT_EQ(entry.threadId, 0);
    EXPECT_EQ(entry.processId, 0);
    EXPECT_EQ(entry.level, Level::Info);
    EXPECT_FALSE(entry.IsValid());
}

TEST(LogEntryReleaseTest, ParameterizedConstructor) {
    uint64_t timestamp = 1234567890;
    uint32_t threadId = 100;
    uint32_t processId = 200;
    Level level = Level::Error;

    LogEntryRelease entry;
    entry.SetTimestamp<>(timestamp);
    entry.SetThreadId<>(threadId);
    entry.SetProcessId<>(processId);
    entry.level = level;

    EXPECT_EQ(entry.GetTimestamp<>(), timestamp);
    EXPECT_EQ(entry.GetThreadId<>(), threadId);
    EXPECT_EQ(entry.GetProcessId<>(), processId);
    EXPECT_EQ(entry.level, level);
    EXPECT_TRUE(entry.IsValid());
}

TEST(LogEntryReleaseTest, IsValid) {
    // Valid entry
    LogEntryRelease validEntry;
    validEntry.SetTimestamp<>(1234567890);
    validEntry.SetThreadId<>(100);
    validEntry.SetProcessId<>(200);
    validEntry.level = Level::Info;
    EXPECT_TRUE(validEntry.IsValid());

    // Invalid: zero timestamp
    LogEntryRelease invalidEntry;
    invalidEntry.level = Level::Info;
    EXPECT_FALSE(invalidEntry.IsValid());
}

TEST(LogEntryReleaseTest, WithBinarySnapshot) {
    LogEntryRelease entry;
    entry.SetTimestamp<>(1234567890);
    entry.SetThreadId<>(100);
    entry.SetProcessId<>(200);
    entry.level = Level::Info;

    // Capture some data in the snapshot
    EXPECT_TRUE(entry.snapshot.CaptureInt32(42));
    EXPECT_TRUE(entry.snapshot.CaptureString("Hello"));
    EXPECT_EQ(entry.snapshot.GetArgCount(), 2);

    // Verify the entry is still valid
    EXPECT_TRUE(entry.IsValid());
}

TEST(LogEntryReleaseTest, MemoryLayout) {
    // Verify the structure size is reasonable
    // Release entry should be around 304 bytes (C++17 compatible implementation)
    size_t size = sizeof(LogEntryRelease);
    EXPECT_GT(size, 280);  // At least 280 bytes
    EXPECT_LE(size, 320);  // Less than or equal to 320 bytes
}

TEST(LogEntryReleaseTest, SmallerThanDebug) {
    // Release entry should be smaller than Debug entry
    EXPECT_LT(sizeof(LogEntryRelease), sizeof(LogEntryDebug));
}

// ==============================================================================
// Compile-time Selection Tests / 编译时选择测试
// ==============================================================================

TEST(LogEntryTest, CompileTimeSelection) {
#ifdef NDEBUG
    // In Release builds, LogEntry should be LogEntryRelease
    EXPECT_TRUE((std::is_same_v<LogEntry, LogEntryRelease>));
    EXPECT_FALSE((std::is_same_v<LogEntry, LogEntryDebug>));
    EXPECT_TRUE(IsReleaseBuild());
    EXPECT_FALSE(IsDebugBuild());
#else
    // In Debug builds, LogEntry should be LogEntryDebug
    EXPECT_TRUE((std::is_same_v<LogEntry, LogEntryDebug>));
    EXPECT_FALSE((std::is_same_v<LogEntry, LogEntryRelease>));
    EXPECT_TRUE(IsDebugBuild());
    EXPECT_FALSE(IsReleaseBuild());
#endif
}

TEST(LogEntryTest, GetLogEntrySize) {
    size_t size = GetLogEntrySize();
    EXPECT_EQ(size, sizeof(LogEntry));
    EXPECT_GT(size, 0);
}

// ==============================================================================
// Integration Tests / 集成测试
// ==============================================================================

TEST(LogEntryTest, CreateWithCurrentThreadAndProcess) {
    SourceLocation loc(__FILE__, __LINE__, __FUNCTION__);
    uint64_t timestamp = 1234567890;
    
    // Get current thread and process IDs
    auto threadId = static_cast<uint32_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));
    auto processId = static_cast<uint32_t>(getpid());

    LogEntry entry;
    entry.SetTimestamp<>(timestamp);
#ifndef NDEBUG
    entry.SetLocation<>(loc);
#endif
    entry.SetThreadId<>(threadId);
    entry.SetProcessId<>(processId);
    entry.level = Level::Info;

    EXPECT_EQ(entry.GetTimestamp<>(), timestamp);
    EXPECT_EQ(entry.GetThreadId<>(), threadId);
    EXPECT_EQ(entry.GetProcessId<>(), processId);
    EXPECT_EQ(entry.level, Level::Info);
    EXPECT_TRUE(entry.IsValid());
}

TEST(LogEntryTest, AllLogLevels) {
    SourceLocation loc("test.cpp", 42, "TestFunction");
    uint64_t timestamp = 1234567890;

    Level levels[] = {
        Level::Alert,
        Level::Fatal,
        Level::Error,
        Level::Warn,
        Level::Notice,
        Level::Info,
        Level::Debug,
        Level::Trace
    };

    for (Level level : levels) {
        LogEntry entry;
        entry.SetTimestamp<>(timestamp);
#ifndef NDEBUG
        entry.SetLocation<>(loc);
#endif
        entry.SetThreadId<>(100);
        entry.SetProcessId<>(200);
        entry.level = level;
        
        EXPECT_EQ(entry.level, level);
        EXPECT_TRUE(entry.IsValid());
    }
}

TEST(LogEntryTest, WithComplexSnapshot) {
    SourceLocation loc("test.cpp", 42, "TestFunction");
    uint64_t timestamp = 1234567890;

    LogEntry entry;
    entry.SetTimestamp<>(timestamp);
#ifndef NDEBUG
    entry.SetLocation<>(loc);
#endif
    entry.SetThreadId<>(100);
    entry.SetProcessId<>(200);
    entry.level = Level::Info;

    // Capture various types
    EXPECT_TRUE(entry.snapshot.CaptureInt32(42));
    EXPECT_TRUE(entry.snapshot.CaptureFloat(3.14f));
    EXPECT_TRUE(entry.snapshot.CaptureString("Test message"));
    EXPECT_TRUE(entry.snapshot.CaptureBool(true));
    EXPECT_TRUE(entry.snapshot.CaptureInt64(9876543210LL));

    EXPECT_EQ(entry.snapshot.GetArgCount(), 5);
    EXPECT_TRUE(entry.IsValid());

    // Format the snapshot
    std::string formatted = entry.snapshot.Format("Value: %d, Pi: %f, Message: %s, Flag: %s, Big: %d");
    EXPECT_FALSE(formatted.empty());
}

// ==============================================================================
// Edge Cases / 边界情况测试
// ==============================================================================

TEST(LogEntryTest, ZeroTimestamp) {
    SourceLocation loc("test.cpp", 42, "TestFunction");

    LogEntry entry;
#ifndef NDEBUG
    entry.SetLocation<>(loc);
#endif
    entry.SetThreadId<>(100);
    entry.SetProcessId<>(200);
    entry.level = Level::Info;

    // Entry is invalid because timestamp is zero (required for Standard/Debug configs)
    EXPECT_FALSE(entry.IsValid());
}

TEST(LogEntryTest, MaxTimestamp) {
    SourceLocation loc("test.cpp", 42, "TestFunction");
    uint64_t maxTimestamp = UINT64_MAX;

    LogEntry entry;
    entry.SetTimestamp<>(maxTimestamp);
#ifndef NDEBUG
    entry.SetLocation<>(loc);
#endif
    entry.SetThreadId<>(100);
    entry.SetProcessId<>(200);
    entry.level = Level::Info;

    EXPECT_EQ(entry.GetTimestamp<>(), maxTimestamp);
    EXPECT_TRUE(entry.IsValid());
}

TEST(LogEntryTest, MaxThreadAndProcessIds) {
    SourceLocation loc("test.cpp", 42, "TestFunction");
    uint32_t maxId = UINT32_MAX;

    LogEntry entry;
    entry.SetTimestamp<>(1234567890);
#ifndef NDEBUG
    entry.SetLocation<>(loc);
#endif
    entry.SetThreadId<>(maxId);
    entry.SetProcessId<>(maxId);
    entry.level = Level::Info;

    EXPECT_EQ(entry.GetThreadId<>(), maxId);
    EXPECT_EQ(entry.GetProcessId<>(), maxId);
    EXPECT_TRUE(entry.IsValid());
}

#ifndef NDEBUG
TEST(LogEntryTest, EmptySourceLocation) {
    SourceLocation emptyLoc("", 0, "");
    LogEntry entry;
    entry.SetTimestamp<>(1234567890);
    entry.SetLocation<>(emptyLoc);
    entry.SetThreadId<>(100);
    entry.SetProcessId<>(200);
    entry.level = Level::Info;

    // Entry with empty strings is technically valid (non-null pointers)
    EXPECT_TRUE(entry.IsValid());
    SourceLocation retrieved = entry.GetLocation<>();
    EXPECT_STREQ(retrieved.file, "");
    EXPECT_STREQ(retrieved.function, "");
    EXPECT_EQ(retrieved.line, 0);
}

TEST(LogEntryTest, VeryLongFilePath) {
    std::string longPath(1000, 'a');
    longPath += ".cpp";
    SourceLocation loc(longPath.c_str(), 42, "TestFunction");
    
    LogEntry entry;
    entry.SetTimestamp<>(1234567890);
    entry.SetLocation<>(loc);
    entry.SetThreadId<>(100);
    entry.SetProcessId<>(200);
    entry.level = Level::Info;
    
    EXPECT_TRUE(entry.IsValid());
    SourceLocation retrieved = entry.GetLocation<>();
    EXPECT_STREQ(retrieved.file, longPath.c_str());
}
#endif

// ==============================================================================
// Performance Tests / 性能测试
// ==============================================================================

TEST(LogEntryTest, ConstructionPerformance) {
    SourceLocation loc("test.cpp", 42, "TestFunction");
    constexpr int iterations = 10000;

    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; ++i) {
        LogEntry entry;
        entry.SetTimestamp<>(1234567890 + i);
#ifndef NDEBUG
        entry.SetLocation<>(loc);
#endif
        entry.SetThreadId<>(100);
        entry.SetProcessId<>(200);
        entry.level = Level::Info;
        
        // Prevent optimization
        volatile bool valid = entry.IsValid();
        (void)valid;
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Construction should be very fast (< 1 microsecond per entry on average)
    double avgTime = static_cast<double>(duration.count()) / iterations;
    EXPECT_LT(avgTime, 1.0);  // Less than 1 microsecond per entry
}


// ==============================================================================
// LogEntryTemplate Tests / LogEntryTemplate 模板测试
// ==============================================================================

TEST(LogEntryTemplateTest, MinimalConfiguration) {
    // Minimal: only level and snapshot
    LogEntryMinimal entry;
    
    // Check compile-time constants
    EXPECT_FALSE(entry.kHasTimestamp);
    EXPECT_FALSE(entry.kHasSourceLocation);
    EXPECT_FALSE(entry.kHasThreadId);
    EXPECT_FALSE(entry.kHasProcessId);
    
    // Set level and snapshot
    entry.level = Level::Info;
    EXPECT_TRUE(entry.snapshot.CaptureString("test"));
    
    // Minimal entry is valid without timestamp
    EXPECT_TRUE(entry.IsValid());
    
    // Size should be smallest
    size_t minimalSize = sizeof(LogEntryMinimal);
    EXPECT_LT(minimalSize, sizeof(LogEntryStandard));
    EXPECT_LT(minimalSize, sizeof(LogEntryDebug));
}

TEST(LogEntryTemplateTest, NoTimestampConfiguration) {
    // NoTimestamp: IDs only, no timestamp
    LogEntryNoTimestamp entry;
    
    // Check compile-time constants
    EXPECT_FALSE(entry.kHasTimestamp);
    EXPECT_FALSE(entry.kHasSourceLocation);
    EXPECT_TRUE(entry.kHasThreadId);
    EXPECT_TRUE(entry.kHasProcessId);
    
    // Set IDs
    entry.SetThreadId<>(100);
    entry.SetProcessId<>(200);
    entry.level = Level::Info;
    
    EXPECT_EQ(entry.GetThreadId<>(), 100);
    EXPECT_EQ(entry.GetProcessId<>(), 200);
    EXPECT_TRUE(entry.IsValid());
}

TEST(LogEntryTemplateTest, StandardConfiguration) {
    // Standard: timestamp + IDs
    LogEntryStandard entry;
    
    // Check compile-time constants
    EXPECT_TRUE(entry.kHasTimestamp);
    EXPECT_FALSE(entry.kHasSourceLocation);
    EXPECT_TRUE(entry.kHasThreadId);
    EXPECT_TRUE(entry.kHasProcessId);
    
    // Set all fields
    entry.SetTimestamp<>(1234567890);
    entry.SetThreadId<>(100);
    entry.SetProcessId<>(200);
    entry.level = Level::Info;
    
    EXPECT_EQ(entry.GetTimestamp<>(), 1234567890);
    EXPECT_EQ(entry.GetThreadId<>(), 100);
    EXPECT_EQ(entry.GetProcessId<>(), 200);
    EXPECT_TRUE(entry.IsValid());
}

TEST(LogEntryTemplateTest, DebugConfiguration) {
    // Debug: all fields including source location
    LogEntryDebug entry;
    
    // Check compile-time constants
    EXPECT_TRUE(entry.kHasTimestamp);
    EXPECT_TRUE(entry.kHasSourceLocation);
    EXPECT_TRUE(entry.kHasThreadId);
    EXPECT_TRUE(entry.kHasProcessId);
    
    // Set all fields
    entry.SetTimestamp<>(1234567890);
    SourceLocation loc("test.cpp", 42, "TestFunction");
    entry.SetLocation<>(loc);
    entry.SetThreadId<>(100);
    entry.SetProcessId<>(200);
    entry.level = Level::Info;
    
    EXPECT_EQ(entry.GetTimestamp<>(), 1234567890);
    EXPECT_EQ(entry.GetThreadId<>(), 100);
    EXPECT_EQ(entry.GetProcessId<>(), 200);
    
    SourceLocation retrieved = entry.GetLocation<>();
    EXPECT_STREQ(retrieved.file, "test.cpp");
    EXPECT_EQ(retrieved.line, 42);
    EXPECT_STREQ(retrieved.function, "TestFunction");
    
    EXPECT_TRUE(entry.IsValid());
}

TEST(LogEntryTemplateTest, SizeComparison) {
    // Verify size hierarchy
    size_t minimalSize = sizeof(LogEntryMinimal);
    size_t noTimestampSize = sizeof(LogEntryNoTimestamp);
    size_t standardSize = sizeof(LogEntryStandard);
    size_t debugSize = sizeof(LogEntryDebug);
    
    // Minimal should be smallest
    EXPECT_LE(minimalSize, noTimestampSize);
    EXPECT_LE(minimalSize, standardSize);
    EXPECT_LE(minimalSize, debugSize);
    
    // Standard should be smaller than Debug
    EXPECT_LE(standardSize, debugSize);
    
    // Print sizes for information
    std::cout << "LogEntry sizes:" << std::endl;
    std::cout << "  Minimal: " << minimalSize << " bytes" << std::endl;
    std::cout << "  NoTimestamp: " << noTimestampSize << " bytes" << std::endl;
    std::cout << "  Standard: " << standardSize << " bytes" << std::endl;
    std::cout << "  Debug: " << debugSize << " bytes" << std::endl;
}

TEST(LogEntryTemplateTest, CustomConfiguration) {
    // Custom: timestamp + source location, no IDs
    using CustomEntry = LogEntryTemplate<LogEntryFeatures::Timestamp | LogEntryFeatures::SourceLocation>;
    
    CustomEntry entry;
    EXPECT_TRUE(entry.kHasTimestamp);
    EXPECT_TRUE(entry.kHasSourceLocation);
    EXPECT_FALSE(entry.kHasThreadId);
    EXPECT_FALSE(entry.kHasProcessId);
    
    entry.SetTimestamp<>(1234567890);
    SourceLocation loc("test.cpp", 42, "TestFunction");
    entry.SetLocation<>(loc);
    entry.level = Level::Info;
    
    EXPECT_TRUE(entry.IsValid());
}

TEST(LogEntryTemplateTest, BackwardCompatibility) {
    // Verify backward compatibility aliases work
    LogEntryRelease releaseEntry;
    EXPECT_TRUE(releaseEntry.kHasTimestamp);
    EXPECT_FALSE(releaseEntry.kHasSourceLocation);
    EXPECT_TRUE(releaseEntry.kHasThreadId);
    EXPECT_TRUE(releaseEntry.kHasProcessId);
    
    // LogEntryRelease should be same as LogEntryStandard
    EXPECT_EQ(sizeof(LogEntryRelease), sizeof(LogEntryStandard));
}

TEST(LogEntryTemplateTest, DefaultLogEntryType) {
    // Verify default LogEntry type based on build mode
#ifdef NDEBUG
    // Release build: LogEntry = LogEntryRelease = LogEntryStandard
    EXPECT_TRUE((std::is_same_v<LogEntry, LogEntryRelease>));
    EXPECT_TRUE((std::is_same_v<LogEntry, LogEntryStandard>));
    EXPECT_FALSE(LogEntry::kHasSourceLocation);
#else
    // Debug build: LogEntry = LogEntryDebug
    EXPECT_TRUE((std::is_same_v<LogEntry, LogEntryDebug>));
    EXPECT_TRUE(LogEntry::kHasSourceLocation);
#endif
}

TEST(LogEntryTemplateTest, ZeroOverheadAbstraction) {
    // Verify that disabled features don't consume space
    // This is a compile-time property, but we can check sizes
    
    // Minimal should not have timestamp overhead
    LogEntryMinimal minimal;
    LogEntryStandard standard;
    
    // The difference should be approximately the size of optional fields
    size_t diff = sizeof(standard) - sizeof(minimal);
    
    // Should include at least timestamp (8 bytes) + 2 IDs (8 bytes)
    EXPECT_GE(diff, 16);
}

TEST(LogEntryTemplateTest, FeatureFlagOperators) {
    // Test bitwise operators
    auto combined = LogEntryFeatures::Timestamp | LogEntryFeatures::ThreadId;
    EXPECT_TRUE(HasFeature(combined, LogEntryFeatures::Timestamp));
    EXPECT_TRUE(HasFeature(combined, LogEntryFeatures::ThreadId));
    EXPECT_FALSE(HasFeature(combined, LogEntryFeatures::SourceLocation));
    EXPECT_FALSE(HasFeature(combined, LogEntryFeatures::ProcessId));
}

TEST(LogEntryTemplateTest, GetFeatureDescription) {
    EXPECT_STREQ(GetFeatureDescription(LogEntryFeatures::Minimal), "Minimal (level + snapshot only)");
    EXPECT_STREQ(GetFeatureDescription(LogEntryFeatures::NoTimestamp), "NoTimestamp (IDs only)");
    EXPECT_STREQ(GetFeatureDescription(LogEntryFeatures::Standard), "Standard (timestamp + IDs)");
    EXPECT_STREQ(GetFeatureDescription(LogEntryFeatures::Debug), "Debug (all fields)");
}

TEST(LogEntryTemplateTest, ValidationRules) {
    // Minimal: valid without timestamp
    LogEntryMinimal minimal;
    minimal.level = Level::Info;
    EXPECT_TRUE(minimal.IsValid());
    
    // Standard: requires timestamp
    LogEntryStandard standard;
    standard.level = Level::Info;
    EXPECT_FALSE(standard.IsValid());  // No timestamp
    
    standard.SetTimestamp<>(1234567890);
    EXPECT_TRUE(standard.IsValid());  // Now valid
    
    // Debug: requires timestamp and source location
    LogEntryDebug debug;
    debug.level = Level::Info;
    EXPECT_FALSE(debug.IsValid());  // No timestamp or location
    
    debug.SetTimestamp<>(1234567890);
    EXPECT_FALSE(debug.IsValid());  // Still no location
    
    SourceLocation loc("test.cpp", 42, "TestFunction");
    debug.SetLocation<>(loc);
    EXPECT_TRUE(debug.IsValid());  // Now valid
}

