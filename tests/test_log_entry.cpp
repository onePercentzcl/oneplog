/**
 * @file test_log_entry.cpp
 * @brief Unit tests for LogEntry structures
 * @brief LogEntry 结构单元测试
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <gtest/gtest.h>

#include "oneplog/log_entry.hpp"

namespace oneplog {
namespace {

// ==============================================================================
// SourceLocation Tests / SourceLocation 测试
// ==============================================================================

TEST(SourceLocationTest, DefaultConstruction) {
    SourceLocation loc;
    EXPECT_EQ(loc.file, nullptr);
    EXPECT_EQ(loc.line, 0u);
    EXPECT_EQ(loc.function, nullptr);
    EXPECT_FALSE(loc.IsValid());
}

TEST(SourceLocationTest, ParameterizedConstruction) {
    SourceLocation loc("test.cpp", 42, "TestFunc");
    EXPECT_STREQ(loc.file, "test.cpp");
    EXPECT_EQ(loc.line, 42u);
    EXPECT_STREQ(loc.function, "TestFunc");
    EXPECT_TRUE(loc.IsValid());
}

TEST(SourceLocationTest, MacroCapture) {
    auto loc = ONEPLOG_CURRENT_LOCATION;
    EXPECT_NE(loc.file, nullptr);
    EXPECT_GT(loc.line, 0u);
    EXPECT_NE(loc.function, nullptr);
    EXPECT_TRUE(loc.IsValid());
}

// ==============================================================================
// LogEntryDebug Tests / LogEntryDebug 测试
// ==============================================================================

TEST(LogEntryDebugTest, DefaultConstruction) {
    LogEntryDebug entry;
    EXPECT_EQ(entry.timestamp, 0u);
    EXPECT_EQ(entry.file, nullptr);
    EXPECT_EQ(entry.function, nullptr);
    EXPECT_EQ(entry.threadId, 0u);
    EXPECT_EQ(entry.processId, 0u);
    EXPECT_EQ(entry.line, 0u);
    EXPECT_EQ(entry.level, Level::Info);
    EXPECT_TRUE(entry.snapshot.IsEmpty());
}

TEST(LogEntryDebugTest, FullConstruction) {
    BinarySnapshot snap;
    snap.CaptureInt32(42);
    
    LogEntryDebug entry(
        1234567890ULL,  // timestamp
        "test.cpp",     // file
        "TestFunc",     // function
        100,            // threadId
        200,            // processId
        42,             // line
        Level::Error,   // level
        std::move(snap) // snapshot
    );
    
    EXPECT_EQ(entry.timestamp, 1234567890ULL);
    EXPECT_STREQ(entry.file, "test.cpp");
    EXPECT_STREQ(entry.function, "TestFunc");
    EXPECT_EQ(entry.threadId, 100u);
    EXPECT_EQ(entry.processId, 200u);
    EXPECT_EQ(entry.line, 42u);
    EXPECT_EQ(entry.level, Level::Error);
    EXPECT_FALSE(entry.snapshot.IsEmpty());
}

TEST(LogEntryDebugTest, SourceLocationConstruction) {
    SourceLocation loc("test.cpp", 42, "TestFunc");
    BinarySnapshot snap;
    
    LogEntryDebug entry(
        1234567890ULL,  // timestamp
        loc,            // location
        100,            // threadId
        200,            // processId
        Level::Warn,    // level
        std::move(snap) // snapshot
    );
    
    EXPECT_EQ(entry.timestamp, 1234567890ULL);
    EXPECT_STREQ(entry.file, "test.cpp");
    EXPECT_STREQ(entry.function, "TestFunc");
    EXPECT_EQ(entry.line, 42u);
    EXPECT_EQ(entry.level, Level::Warn);
}

TEST(LogEntryDebugTest, GetLocation) {
    LogEntryDebug entry(
        0, "test.cpp", "TestFunc", 0, 0, 42, Level::Info, BinarySnapshot()
    );
    
    auto loc = entry.GetLocation();
    EXPECT_STREQ(loc.file, "test.cpp");
    EXPECT_EQ(loc.line, 42u);
    EXPECT_STREQ(loc.function, "TestFunc");
}

// ==============================================================================
// LogEntryRelease Tests / LogEntryRelease 测试
// ==============================================================================

TEST(LogEntryReleaseTest, DefaultConstruction) {
    LogEntryRelease entry;
    EXPECT_EQ(entry.timestamp, 0u);
    EXPECT_EQ(entry.threadId, 0u);
    EXPECT_EQ(entry.processId, 0u);
    EXPECT_EQ(entry.level, Level::Info);
    EXPECT_TRUE(entry.snapshot.IsEmpty());
}

TEST(LogEntryReleaseTest, FullConstruction) {
    BinarySnapshot snap;
    snap.CaptureInt32(42);
    
    LogEntryRelease entry(
        1234567890ULL,  // timestamp
        100,            // threadId
        200,            // processId
        Level::Critical,// level
        std::move(snap) // snapshot
    );
    
    EXPECT_EQ(entry.timestamp, 1234567890ULL);
    EXPECT_EQ(entry.threadId, 100u);
    EXPECT_EQ(entry.processId, 200u);
    EXPECT_EQ(entry.level, Level::Critical);
    EXPECT_FALSE(entry.snapshot.IsEmpty());
}

// ==============================================================================
// Memory Layout Tests / 内存布局测试
// ==============================================================================

TEST(LogEntryMemoryLayoutTest, DebugEntrySize) {
    // Expected size: 8 + 8 + 8 + 4 + 4 + 4 + 1 + 3 + 256 = 296 bytes
    // But actual size may vary due to alignment
    // 预期大小: 8 + 8 + 8 + 4 + 4 + 4 + 1 + 3 + 256 = 296 字节
    // 但实际大小可能因对齐而有所不同
    
    // Verify the struct is reasonably sized (not excessively padded)
    // 验证结构体大小合理（没有过多填充）
    EXPECT_LE(sizeof(LogEntryDebug), 320u);  // Allow some padding
    EXPECT_GE(sizeof(LogEntryDebug), 296u);  // At least the sum of fields
}

TEST(LogEntryReleaseTest, ReleaseEntrySize) {
    // Expected size: 8 + 4 + 4 + 1 + 7 + 256 = 280 bytes
    // 预期大小: 8 + 4 + 4 + 1 + 7 + 256 = 280 字节
    
    EXPECT_LE(sizeof(LogEntryRelease), 300u);  // Allow some padding
    EXPECT_GE(sizeof(LogEntryRelease), 280u);  // At least the sum of fields
}

TEST(LogEntryMemoryLayoutTest, FieldOffsets) {
    // Verify field ordering is as expected
    // 验证字段顺序符合预期
    
    LogEntryDebug entry;
    
    // timestamp should be at offset 0
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&entry.timestamp) - 
              reinterpret_cast<uintptr_t>(&entry), 0u);
    
    // file should come after timestamp (offset 8)
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&entry.file) - 
              reinterpret_cast<uintptr_t>(&entry), 8u);
    
    // function should come after file (offset 16)
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&entry.function) - 
              reinterpret_cast<uintptr_t>(&entry), 16u);
}

}  // namespace
}  // namespace oneplog
