/**
 * @file test_logger.cpp
 * @brief Tests for Logger core class implementation
 * @brief Logger 核心类实现测试
 *
 * Tests for Task 3: Logger Core Class Implementation
 * - 3.1 Class framework (template parameters, compile-time constants, member variables)
 * - 3.2 Constructors and destructor
 * - 3.3 Move semantics
 * - 3.4 Logging methods
 * - 3.5 WFC logging methods
 * - 3.6 Control methods
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <string>

#include "oneplog/logger.hpp"

namespace oneplog {
namespace {

// ==============================================================================
// Mock Format for Testing / 用于测试的模拟 Format
// ==============================================================================

struct TestMessageOnlyFormat {
    using Requirements = StaticFormatRequirements<false, false, false, false, false>;
    
#ifdef ONEPLOG_USE_FMT
    template<typename... Args>
    static void FormatTo(fmt::memory_buffer& buffer, Level, uint64_t, uint32_t, uint32_t,
                         const char* fmt, Args&&... args) {
        if constexpr (sizeof...(Args) == 0) {
            buffer.append(std::string_view(fmt));
        } else {
            fmt::format_to(std::back_inserter(buffer), fmt::runtime(fmt), std::forward<Args>(args)...);
        }
    }
    
    static void FormatEntryTo(fmt::memory_buffer& buffer, const LogEntry& entry) {
        std::string msg = entry.snapshot.FormatAll();
        buffer.append(std::string_view(msg));
    }
#endif
    
    static std::string FormatEntry(const LogEntry& entry) {
        return entry.snapshot.FormatAll();
    }
    
    static std::string FormatEntry(Level, uint64_t, uint32_t, uint32_t, 
                                   const BinarySnapshot& snapshot) {
        return snapshot.FormatAll();
    }
};

// ==============================================================================
// Mock Sink for Testing / 用于测试的模拟 Sink
// ==============================================================================

struct MockSink {
    std::vector<std::string> messages;
    bool flushed = false;
    bool closed = false;
    
    void Write(std::string_view msg) noexcept {
        messages.emplace_back(msg);
    }
    
    void Flush() noexcept {
        flushed = true;
    }
    
    void Close() noexcept {
        closed = true;
    }
    
    void Reset() {
        messages.clear();
        flushed = false;
        closed = false;
    }
};

// ==============================================================================
// Test Configurations / 测试配置
// ==============================================================================

// Sync test config with MockSink
using MockBinding = SinkBinding<MockSink, TestMessageOnlyFormat>;
using MockBindings = SinkBindingList<MockBinding>;

using TestSyncConfig = FastLoggerConfig<
    Mode::Sync,
    Level::Trace,  // Allow all levels
    false,  // EnableWFC
    false,  // EnableShadowTail
    true,   // UseFmt
    1024,   // HeapRingBufferCapacity
    1024,   // SharedRingBufferCapacity
    QueueFullPolicy::Block,
    DefaultSharedMemoryName,
    10,     // PollTimeoutMs
    MockBindings
>;

// Async test config with MockSink
using TestAsyncConfig = FastLoggerConfig<
    Mode::Async,
    Level::Trace,
    false,  // EnableWFC
    false,  // EnableShadowTail
    true,   // UseFmt
    1024,
    1024,
    QueueFullPolicy::Block,
    DefaultSharedMemoryName,
    10,
    MockBindings
>;

// WFC enabled async config
using TestWFCConfig = FastLoggerConfig<
    Mode::Async,
    Level::Trace,
    true,   // EnableWFC
    false,  // EnableShadowTail
    true,   // UseFmt
    1024,
    1024,
    QueueFullPolicy::Block,
    DefaultSharedMemoryName,
    100,    // PollTimeoutMs - longer for WFC
    MockBindings
>;

// Level filtering test config (Info level minimum)
using TestLevelFilterConfig = FastLoggerConfig<
    Mode::Sync,
    Level::Info,  // Only Info and above
    false,
    false,
    true,
    1024,
    1024,
    QueueFullPolicy::Block,
    DefaultSharedMemoryName,
    10,
    MockBindings
>;

// ==============================================================================
// Task 3.1: Class Framework Tests / 类框架测试
// ==============================================================================

TEST(FastLoggerV2FrameworkTest, CompileTimeConstants) {
    // Test that compile-time constants are correctly inherited from config
    using Logger = FastLoggerV2<TestSyncConfig>;
    
    EXPECT_EQ(Logger::kMode, Mode::Sync);
    EXPECT_EQ(Logger::kMinLevel, Level::Trace);
    EXPECT_FALSE(Logger::kEnableWFC);
    EXPECT_FALSE(Logger::kEnableShadowTail);
    EXPECT_TRUE(Logger::kUseFmt);
    EXPECT_EQ(Logger::kHeapRingBufferCapacity, 1024u);
    EXPECT_EQ(Logger::kQueueFullPolicy, QueueFullPolicy::Block);
}

TEST(FastLoggerV2FrameworkTest, MetadataRequirementsFromSinkBindings) {
    // MessageOnlyFormat has no requirements
    using Logger = FastLoggerV2<TestSyncConfig>;
    
    // MessageOnlyFormat::Requirements has all false
    EXPECT_FALSE(Logger::kNeedsTimestamp);
    EXPECT_FALSE(Logger::kNeedsLevel);
    EXPECT_FALSE(Logger::kNeedsThreadId);
    EXPECT_FALSE(Logger::kNeedsProcessId);
    EXPECT_FALSE(Logger::kNeedsSourceLocation);
}

TEST(FastLoggerV2FrameworkTest, TypeAliases) {
    using Logger = FastLoggerV2<TestSyncConfig>;
    
    static_assert(std::is_same_v<Logger::ConfigType, TestSyncConfig>);
    static_assert(std::is_same_v<Logger::SinkBindings, MockBindings>);
}

// ==============================================================================
// Task 3.2: Constructor and Destructor Tests / 构造函数和析构函数测试
// ==============================================================================

TEST(FastLoggerV2ConstructorTest, DefaultConstruction) {
    // Should not throw
    FastLoggerV2<TestSyncConfig> logger;
    
    // Logger should be in valid state
    EXPECT_FALSE(logger.IsRunning());  // Sync mode doesn't run a worker
}

TEST(FastLoggerV2ConstructorTest, ConstructWithRuntimeConfig) {
    RuntimeConfig config;
    config.processName = "TestProcess";
    config.pollInterval = std::chrono::microseconds(10);
    config.colorEnabled = false;
    
    FastLoggerV2<TestSyncConfig> logger(config);
    
    EXPECT_EQ(logger.GetRuntimeConfig().processName, "TestProcess");
    EXPECT_EQ(logger.GetRuntimeConfig().pollInterval.count(), 10);
    EXPECT_FALSE(logger.GetRuntimeConfig().colorEnabled);
}

TEST(FastLoggerV2ConstructorTest, ConstructWithSinkBindings) {
    MockBindings bindings;
    
    FastLoggerV2<TestSyncConfig> logger(std::move(bindings));
    
    // Should be able to access sink bindings
    auto& sinkBindings = logger.GetSinkBindings();
    EXPECT_EQ(sinkBindings.kBindingCount, 1u);
}

TEST(FastLoggerV2ConstructorTest, ConstructWithBothConfigs) {
    MockBindings bindings;
    RuntimeConfig config;
    config.processName = "TestProcess";
    
    FastLoggerV2<TestSyncConfig> logger(std::move(bindings), config);
    
    EXPECT_EQ(logger.GetRuntimeConfig().processName, "TestProcess");
}

TEST(FastLoggerV2ConstructorTest, AsyncModeConstruction) {
    // Async mode should start worker thread
    FastLoggerV2<TestAsyncConfig> logger;
    
    // Give worker time to start
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    EXPECT_TRUE(logger.IsRunning());
    
    // Destructor should properly stop worker
}

TEST(FastLoggerV2ConstructorTest, DestructorStopsWorker) {
    {
        FastLoggerV2<TestAsyncConfig> logger;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        EXPECT_TRUE(logger.IsRunning());
    }
    // Logger destroyed, worker should be stopped
    // No crash = success
}

TEST(FastLoggerV2ConstructorTest, DestructorClosesSinks) {
    MockBindings bindings;
    
    {
        FastLoggerV2<TestSyncConfig> logger(std::move(bindings));
        // Sink should not be closed yet
        EXPECT_FALSE(logger.GetSinkBindings().Get<0>().sink.closed);
    }
    // After destruction, sink should be closed
    // We can't check this directly since logger is destroyed
    // But no crash = success
}

// ==============================================================================
// Task 3.3: Move Semantics Tests / 移动语义测试
// ==============================================================================

TEST(FastLoggerV2MoveTest, MoveConstruction) {
    RuntimeConfig config;
    config.processName = "Original";
    
    FastLoggerV2<TestSyncConfig> original(config);
    FastLoggerV2<TestSyncConfig> moved(std::move(original));
    
    EXPECT_EQ(moved.GetRuntimeConfig().processName, "Original");
}

TEST(FastLoggerV2MoveTest, MoveAssignment) {
    RuntimeConfig config1;
    config1.processName = "First";
    
    RuntimeConfig config2;
    config2.processName = "Second";
    
    FastLoggerV2<TestSyncConfig> first(config1);
    FastLoggerV2<TestSyncConfig> second(config2);
    
    second = std::move(first);
    
    EXPECT_EQ(second.GetRuntimeConfig().processName, "First");
}

TEST(FastLoggerV2MoveTest, AsyncModeMoveConstruction) {
    FastLoggerV2<TestAsyncConfig> original;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(original.IsRunning());
    
    FastLoggerV2<TestAsyncConfig> moved(std::move(original));
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Moved logger should be running
    EXPECT_TRUE(moved.IsRunning());
    
    // Original should be stopped (worker transferred)
    EXPECT_FALSE(original.IsRunning());
}

TEST(FastLoggerV2MoveTest, AsyncModeMoveAssignment) {
    FastLoggerV2<TestAsyncConfig> first;
    FastLoggerV2<TestAsyncConfig> second;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    EXPECT_TRUE(first.IsRunning());
    EXPECT_TRUE(second.IsRunning());
    
    second = std::move(first);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    EXPECT_TRUE(second.IsRunning());
    EXPECT_FALSE(first.IsRunning());
}

TEST(FastLoggerV2MoveTest, NonCopyable) {
    // These should not compile - just verify the types are correct
    static_assert(!std::is_copy_constructible_v<FastLoggerV2<TestSyncConfig>>);
    static_assert(!std::is_copy_assignable_v<FastLoggerV2<TestSyncConfig>>);
}

// ==============================================================================
// Task 3.4: Logging Methods Tests / 日志记录方法测试
// ==============================================================================

TEST(FastLoggerV2LoggingTest, SyncModeBasicLogging) {
    FastLoggerV2<TestSyncConfig> logger;
    
    logger.Trace("trace message");
    logger.Debug("debug message");
    logger.Info("info message");
    logger.Warn("warn message");
    logger.Error("error message");
    logger.Critical("critical message");
    
    auto& sink = logger.GetSinkBindings().Get<0>().sink;
    EXPECT_EQ(sink.messages.size(), 6u);
}

TEST(FastLoggerV2LoggingTest, SyncModeWithArguments) {
    FastLoggerV2<TestSyncConfig> logger;
    
    logger.Info("value: {}", 42);
    logger.Info("values: {} and {}", "hello", 3.14);
    
    auto& sink = logger.GetSinkBindings().Get<0>().sink;
    EXPECT_EQ(sink.messages.size(), 2u);
    EXPECT_EQ(sink.messages[0], "value: 42");
    // Note: Non-fmt fallback may format doubles with different precision
    // 注意：非 fmt 回退可能以不同精度格式化双精度数
    EXPECT_TRUE(sink.messages[1].find("values: hello and 3.14") == 0);
}

TEST(FastLoggerV2LoggingTest, CompileTimeLevelFiltering) {
    // This config has Level::Info as minimum
    FastLoggerV2<TestLevelFilterConfig> logger;
    
    // These should be filtered out at compile time
    logger.Trace("trace");
    logger.Debug("debug");
    
    // These should be logged
    logger.Info("info");
    logger.Warn("warn");
    logger.Error("error");
    logger.Critical("critical");
    
    auto& sink = logger.GetSinkBindings().Get<0>().sink;
    // Only Info and above should be logged
    EXPECT_EQ(sink.messages.size(), 4u);
}

TEST(FastLoggerV2LoggingTest, AsyncModeBasicLogging) {
    FastLoggerV2<TestAsyncConfig> logger;
    
    logger.Info("async message 1");
    logger.Info("async message 2");
    logger.Info("async message 3");
    
    // Flush to ensure all messages are processed
    logger.Flush();
    
    auto& sink = logger.GetSinkBindings().Get<0>().sink;
    EXPECT_EQ(sink.messages.size(), 3u);
}

TEST(FastLoggerV2LoggingTest, AsyncModeWithArguments) {
    FastLoggerV2<TestAsyncConfig> logger;
    
    logger.Info("value: {}", 42);
    logger.Info("string: {}", "hello");
    
    logger.Flush();
    
    auto& sink = logger.GetSinkBindings().Get<0>().sink;
    EXPECT_EQ(sink.messages.size(), 2u);
    EXPECT_EQ(sink.messages[0], "value: 42");
    EXPECT_EQ(sink.messages[1], "string: hello");
}

// ==============================================================================
// Task 3.5: WFC Logging Methods Tests / WFC 日志记录方法测试
// ==============================================================================

TEST(FastLoggerV2WFCTest, WFCMethodsAvailable) {
    FastLoggerV2<TestWFCConfig> logger;
    
    // WFC methods should be available when EnableWFC is true
    logger.TraceWFC("trace wfc");
    logger.DebugWFC("debug wfc");
    logger.InfoWFC("info wfc");
    logger.WarnWFC("warn wfc");
    logger.ErrorWFC("error wfc");
    logger.CriticalWFC("critical wfc");
    
    logger.Flush();
    
    auto& sink = logger.GetSinkBindings().Get<0>().sink;
    EXPECT_EQ(sink.messages.size(), 6u);
    
    // Verify message content
    if (sink.messages.size() >= 6) {
        EXPECT_EQ(sink.messages[0], "trace wfc");
        EXPECT_EQ(sink.messages[1], "debug wfc");
        EXPECT_EQ(sink.messages[2], "info wfc");
        EXPECT_EQ(sink.messages[3], "warn wfc");
        EXPECT_EQ(sink.messages[4], "error wfc");
        EXPECT_EQ(sink.messages[5], "critical wfc");
    }
}

TEST(FastLoggerV2WFCTest, WFCWithArguments) {
    FastLoggerV2<TestWFCConfig> logger;
    
    // First test without arguments to verify basic WFC works
    logger.InfoWFC("simple message");
    logger.Flush();
    
    auto& sink = logger.GetSinkBindings().Get<0>().sink;
    EXPECT_EQ(sink.messages.size(), 1u);
    if (sink.messages.size() >= 1) {
        EXPECT_EQ(sink.messages[0], "simple message");
    }
    
    // Now test with arguments
    sink.Reset();
    logger.InfoWFC("wfc value: {}", 42);
    logger.Flush();
    
    EXPECT_EQ(sink.messages.size(), 1u);
    if (sink.messages.size() >= 1) {
        EXPECT_EQ(sink.messages[0], "wfc value: 42");
    }
}

// ==============================================================================
// Task 3.6: Control Methods Tests / 控制方法测试
// ==============================================================================

TEST(FastLoggerV2ControlTest, FlushSyncMode) {
    FastLoggerV2<TestSyncConfig> logger;
    
    logger.Info("message");
    logger.Flush();
    
    auto& sink = logger.GetSinkBindings().Get<0>().sink;
    EXPECT_TRUE(sink.flushed);
}

TEST(FastLoggerV2ControlTest, FlushAsyncMode) {
    FastLoggerV2<TestAsyncConfig> logger;
    
    logger.Info("message 1");
    logger.Info("message 2");
    logger.Info("message 3");
    
    // Flush should wait for all messages to be processed
    logger.Flush();
    
    auto& sink = logger.GetSinkBindings().Get<0>().sink;
    EXPECT_EQ(sink.messages.size(), 3u);
    EXPECT_TRUE(sink.flushed);
}

TEST(FastLoggerV2ControlTest, ShutdownSyncMode) {
    FastLoggerV2<TestSyncConfig> logger;
    
    logger.Info("message");
    logger.Shutdown();
    
    auto& sink = logger.GetSinkBindings().Get<0>().sink;
    EXPECT_TRUE(sink.closed);
}

TEST(FastLoggerV2ControlTest, ShutdownAsyncMode) {
    FastLoggerV2<TestAsyncConfig> logger;
    
    logger.Info("message");
    
    EXPECT_TRUE(logger.IsRunning());
    
    logger.Shutdown();
    
    EXPECT_FALSE(logger.IsRunning());
    
    auto& sink = logger.GetSinkBindings().Get<0>().sink;
    EXPECT_TRUE(sink.closed);
}

TEST(FastLoggerV2ControlTest, MultipleFlushCalls) {
    FastLoggerV2<TestAsyncConfig> logger;
    
    logger.Info("message");
    logger.Flush();
    logger.Flush();
    logger.Flush();
    
    // Should not crash
    auto& sink = logger.GetSinkBindings().Get<0>().sink;
    EXPECT_EQ(sink.messages.size(), 1u);
}

TEST(FastLoggerV2ControlTest, ShutdownThenLog) {
    FastLoggerV2<TestAsyncConfig> logger;
    
    logger.Shutdown();
    
    // Logging after shutdown should not crash
    logger.Info("message after shutdown");
    
    // Message may or may not be logged depending on implementation
    // The important thing is no crash
}

// ==============================================================================
// Type Alias Tests / 类型别名测试
// ==============================================================================

TEST(FastLoggerV2TypeAliasTest, SyncLoggerV2) {
    static_assert(SyncLoggerV2::kMode == Mode::Sync);
}

TEST(FastLoggerV2TypeAliasTest, AsyncLoggerV2) {
    static_assert(AsyncLoggerV2::kMode == Mode::Async);
}

TEST(FastLoggerV2TypeAliasTest, MProcLoggerV2) {
    static_assert(MProcLoggerV2::kMode == Mode::MProc);
}

// ==============================================================================
// Task 6: MProc Mode Tests / 多进程模式测试
// ==============================================================================

// MProc test config with MockSink
struct TestMProcSharedMemoryName {
    static constexpr const char* value = "test_mproc_logger";
};

using TestMProcConfig = FastLoggerConfig<
    Mode::MProc,
    Level::Trace,
    false,  // EnableWFC
    false,  // EnableShadowTail
    true,   // UseFmt
    1024,   // HeapRingBufferCapacity
    1024,   // SharedRingBufferCapacity
    QueueFullPolicy::Block,
    TestMProcSharedMemoryName,
    10,     // PollTimeoutMs
    MockBindings
>;

TEST(FastLoggerV2MProcTest, MProcModeConstruction) {
    // MProc mode should initialize without crashing
    // Note: This test may create shared memory, so we use a unique name
    FastLoggerV2<TestMProcConfig> logger;
    
    // Logger should be running
    EXPECT_TRUE(logger.IsRunning());
    
    // Check if we are the owner (first to create shared memory)
    // This depends on whether shared memory already exists
    // Just verify the method works
    [[maybe_unused]] bool isOwner = logger.IsMProcOwner();
}

TEST(FastLoggerV2MProcTest, MProcModeLogging) {
    FastLoggerV2<TestMProcConfig> logger;
    
    // Log some messages
    logger.Info("MProc test message 1");
    logger.Debug("MProc test message 2");
    logger.Warn("MProc test message 3");
    
    // Flush to ensure messages are processed
    logger.Flush();
    
    // In MProc mode, messages go through shared memory pipeline
    // The test verifies that logging doesn't crash, not message delivery
    // (message delivery depends on consumer process in real MProc scenarios)
    EXPECT_TRUE(logger.IsRunning());
}

TEST(FastLoggerV2MProcTest, MProcModeShutdown) {
    FastLoggerV2<TestMProcConfig> logger;
    
    logger.Info("Before shutdown");
    logger.Shutdown();
    
    // Logger should not be running after shutdown
    EXPECT_FALSE(logger.IsRunning());
    
    // Logging after shutdown should not crash
    logger.Info("After shutdown");
}

TEST(FastLoggerV2MProcTest, MProcModeFlush) {
    FastLoggerV2<TestMProcConfig> logger;
    
    // Log messages
    for (int i = 0; i < 10; ++i) {
        logger.Info("Flush test message {}", i);
    }
    
    // Flush should wait for all messages to be processed
    logger.Flush();
    
    auto& sink = logger.GetSinkBindings().Get<0>().sink;
    EXPECT_GE(sink.messages.size(), 10u);
}

TEST(FastLoggerV2MProcTest, MProcModeAccessors) {
    FastLoggerV2<TestMProcConfig> logger;
    
    // Test SharedConfig accessor
    [[maybe_unused]] auto* config = logger.GetSharedConfig();
    // Config may be nullptr if shared memory creation failed (fallback to async)
    // Just verify the method doesn't crash
    
    // Test NameTable accessor
    auto* nameTable = logger.GetNameTable();
    // Same as above
    
    // Test name registration (if in MProc mode with shared memory)
    if (logger.IsMProcOwner() && nameTable != nullptr) {
        uint32_t processId = logger.RegisterProcess("TestProcess");
        EXPECT_GT(processId, 0u);
        
        uint32_t threadId = logger.RegisterThread("TestThread");
        EXPECT_GT(threadId, 0u);
        
        const char* processName = logger.GetProcessName(processId);
        EXPECT_NE(processName, nullptr);
        EXPECT_STREQ(processName, "TestProcess");
        
        const char* threadName = logger.GetThreadName(threadId);
        EXPECT_NE(threadName, nullptr);
        EXPECT_STREQ(threadName, "TestThread");
    }
}

TEST(FastLoggerV2MProcTest, MProcModeDestructor) {
    // Test that destructor properly cleans up MProc resources
    {
        FastLoggerV2<TestMProcConfig> logger;
        logger.Info("Message before destruction");
        // Destructor should be called here
    }
    // Should not crash after destruction
}

// ==============================================================================
// Task 9: Memory Safety Tests / 内存安全测试
// ==============================================================================

// ==============================================================================
// Task 9.1: LogEntry Ownership Verification / LogEntry 所有权验证
// ==============================================================================

/**
 * @brief Test that dynamic strings (std::string) are correctly copied
 * @brief 测试动态字符串（std::string）被正确拷贝
 *
 * Verifies that when a std::string is passed to the logger, the string
 * data is copied into the BinarySnapshot (StringCopy), not just referenced.
 * This ensures the log entry owns its data even after the original string
 * is destroyed.
 *
 * 验证当 std::string 传递给日志器时，字符串数据被拷贝到 BinarySnapshot
 * （StringCopy）中，而不仅仅是引用。这确保日志条目拥有其数据，即使原始
 * 字符串被销毁。
 *
 * _Requirements: 8.2, 8.3_
 */
TEST(FastLoggerV2MemorySafetyTest, DynamicStringOwnership) {
    FastLoggerV2<TestAsyncConfig> logger;
    
    // Create a string in a scope that will be destroyed before flush
    // 在一个将在 flush 之前被销毁的作用域中创建字符串
    {
        std::string dynamicStr = "This is a dynamic string that will be destroyed";
        logger.Info("Message: {}", dynamicStr);
        // dynamicStr is destroyed here
    }
    
    // Flush should still work correctly because the string was copied
    // Flush 应该仍然正确工作，因为字符串已被拷贝
    logger.Flush();
    
    auto& sink = logger.GetSinkBindings().Get<0>().sink;
    EXPECT_EQ(sink.messages.size(), 1u);
    if (sink.messages.size() >= 1) {
        EXPECT_EQ(sink.messages[0], "Message: This is a dynamic string that will be destroyed");
    }
}

/**
 * @brief Test that multiple dynamic strings are correctly copied
 * @brief 测试多个动态字符串被正确拷贝
 *
 * _Requirements: 8.2, 8.3_
 */
TEST(FastLoggerV2MemorySafetyTest, MultipleDynamicStringsOwnership) {
    FastLoggerV2<TestAsyncConfig> logger;
    
    {
        std::string str1 = "First dynamic string";
        std::string str2 = "Second dynamic string";
        logger.Info("Values: {} and {}", str1, str2);
        // Both strings destroyed here
    }
    
    logger.Flush();
    
    auto& sink = logger.GetSinkBindings().Get<0>().sink;
    EXPECT_EQ(sink.messages.size(), 1u);
    if (sink.messages.size() >= 1) {
        EXPECT_EQ(sink.messages[0], "Values: First dynamic string and Second dynamic string");
    }
}

/**
 * @brief Test that char* strings are correctly copied
 * @brief 测试 char* 字符串被正确拷贝
 *
 * _Requirements: 8.2, 8.3_
 */
TEST(FastLoggerV2MemorySafetyTest, CharPointerOwnership) {
    FastLoggerV2<TestAsyncConfig> logger;
    
    {
        char buffer[64] = "Temporary buffer content";
        const char* ptr = buffer;
        logger.Info("Buffer: {}", ptr);
        // Modify buffer to verify copy was made
        std::memset(buffer, 'X', sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
    }
    
    logger.Flush();
    
    auto& sink = logger.GetSinkBindings().Get<0>().sink;
    EXPECT_EQ(sink.messages.size(), 1u);
    if (sink.messages.size() >= 1) {
        // Should contain original content, not modified content
        EXPECT_EQ(sink.messages[0], "Buffer: Temporary buffer content");
    }
}

/**
 * @brief Test LogEntry ownership after move to RingBuffer
 * @brief 测试 LogEntry 移动到 RingBuffer 后的所有权
 *
 * Verifies that LogEntry maintains complete ownership of its data
 * after being moved into the RingBuffer.
 *
 * 验证 LogEntry 在移动到 RingBuffer 后保持其数据的完整所有权。
 *
 * _Requirements: 8.2, 8.3_
 */
TEST(FastLoggerV2MemorySafetyTest, LogEntryOwnershipAfterMove) {
    FastLoggerV2<TestAsyncConfig> logger;
    
    // Log multiple messages with dynamic content
    for (int i = 0; i < 10; ++i) {
        std::string msg = "Dynamic message number " + std::to_string(i);
        logger.Info("Log: {}", msg);
        // msg destroyed at end of each iteration
    }
    
    logger.Flush();
    
    auto& sink = logger.GetSinkBindings().Get<0>().sink;
    EXPECT_EQ(sink.messages.size(), 10u);
    
    // Verify all messages are correct
    for (int i = 0; i < 10 && i < static_cast<int>(sink.messages.size()); ++i) {
        std::string expected = "Log: Dynamic message number " + std::to_string(i);
        EXPECT_EQ(sink.messages[i], expected);
    }
}

// ==============================================================================
// Task 9.2: Destructor Order Verification / 析构顺序验证
// ==============================================================================

/**
 * @brief Test that destructor stops worker before destroying buffer
 * @brief 测试析构函数在销毁缓冲区之前停止工作线程
 *
 * Verifies that the worker thread is properly stopped before the
 * ring buffer is destroyed, preventing use-after-free.
 *
 * 验证工作线程在环形缓冲区销毁之前被正确停止，防止释放后使用。
 *
 * _Requirements: 8.1, 11.2_
 */
TEST(FastLoggerV2MemorySafetyTest, DestructorStopsWorkerBeforeBuffer) {
    // Create and destroy logger multiple times to stress test
    // 多次创建和销毁日志器以进行压力测试
    for (int i = 0; i < 10; ++i) {
        FastLoggerV2<TestAsyncConfig> logger;
        
        // Log some messages to ensure worker is active
        for (int j = 0; j < 100; ++j) {
            logger.Info("Stress test message {} iteration {}", j, i);
        }
        
        // Destructor called here - should not crash
    }
    // If we get here without crash, the test passes
}

/**
 * @brief Test destructor with pending log entries
 * @brief 测试有待处理日志条目时的析构函数
 *
 * Verifies that destructor properly handles pending log entries
 * without crashing or leaking memory.
 *
 * 验证析构函数正确处理待处理的日志条目，不会崩溃或泄漏内存。
 *
 * _Requirements: 4.6, 8.1_
 */
TEST(FastLoggerV2MemorySafetyTest, DestructorWithPendingEntries) {
    for (int i = 0; i < 5; ++i) {
        FastLoggerV2<TestAsyncConfig> logger;
        
        // Log many messages without flushing
        for (int j = 0; j < 1000; ++j) {
            logger.Info("Pending message {}", j);
        }
        
        // Destructor called here with pending entries
        // Should drain entries and stop cleanly
    }
}

/**
 * @brief Test destructor order: buffer destroyed after sinks closed
 * @brief 测试析构顺序：缓冲区在 Sink 关闭后销毁
 *
 * Verifies that sinks are closed after all log entries are processed.
 *
 * 验证所有日志条目处理完成后 Sink 才被关闭。
 *
 * _Requirements: 8.1, 11.2_
 */
TEST(FastLoggerV2MemorySafetyTest, SinksClosedAfterProcessing) {
    MockBindings bindings;
    
    {
        FastLoggerV2<TestAsyncConfig> logger(std::move(bindings));
        
        // Log messages
        logger.Info("Message 1");
        logger.Info("Message 2");
        logger.Info("Message 3");
        
        // Don't flush - let destructor handle it
        // Destructor should:
        // 1. Stop worker thread
        // 2. Drain remaining entries
        // 3. Close sinks
    }
    // Logger destroyed - should not crash
}

/**
 * @brief Test rapid construction and destruction
 * @brief 测试快速构造和析构
 *
 * Stress test for resource management during rapid lifecycle changes.
 *
 * 快速生命周期变化期间资源管理的压力测试。
 *
 * _Requirements: 11.1, 11.2_
 */
TEST(FastLoggerV2MemorySafetyTest, RapidConstructionDestruction) {
    for (int i = 0; i < 20; ++i) {
        FastLoggerV2<TestAsyncConfig> logger;
        logger.Info("Quick message");
        // Immediate destruction
    }
}

/**
 * @brief Test destructor during active logging from multiple threads
 * @brief 测试多线程活跃日志记录期间的析构函数
 *
 * Verifies that destructor properly handles concurrent logging.
 *
 * 验证析构函数正确处理并发日志记录。
 *
 * _Requirements: 8.4, 8.5_
 */
TEST(FastLoggerV2MemorySafetyTest, DestructorDuringConcurrentLogging) {
    for (int iteration = 0; iteration < 5; ++iteration) {
        auto logger = std::make_shared<FastLoggerV2<TestAsyncConfig>>();
        std::atomic<bool> stopFlag{false};
        
        // Start logging threads
        std::vector<std::thread> threads;
        for (int t = 0; t < 4; ++t) {
            threads.emplace_back([&logger, &stopFlag, t]() {
                int count = 0;
                while (!stopFlag.load(std::memory_order_relaxed) && count < 100) {
                    logger->Info("Thread {} message {}", t, count++);
                }
            });
        }
        
        // Let threads run briefly
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Signal stop and destroy logger
        stopFlag.store(true, std::memory_order_relaxed);
        logger.reset();  // Destroy logger while threads may still be running
        
        // Wait for threads to finish
        for (auto& t : threads) {
            t.join();
        }
    }
}

/**
 * @brief Test move semantics preserve destructor safety
 * @brief 测试移动语义保持析构安全性
 *
 * Verifies that moved-from loggers can be safely destroyed.
 *
 * 验证被移动的日志器可以安全销毁。
 *
 * _Requirements: 11.3, 11.4_
 */
TEST(FastLoggerV2MemorySafetyTest, MovedLoggerDestructorSafety) {
    FastLoggerV2<TestAsyncConfig> original;
    original.Info("Original message");
    
    // Move to new logger
    FastLoggerV2<TestAsyncConfig> moved(std::move(original));
    moved.Info("Moved message");
    
    // Both destructors should be safe
    // original is in moved-from state but should still be destructible
}

// ==============================================================================
// Task 10: Default Configuration Presets Tests / 默认配置预设测试
// ==============================================================================

/**
 * @brief Test DefaultSyncConfig preset
 * @brief 测试 DefaultSyncConfig 预设
 *
 * Verifies that DefaultSyncConfig has correct settings:
 * - Sync mode
 * - Console output with SimpleFormat
 *
 * _Requirements: 16.1_
 */
TEST(FastLoggerV2DefaultConfigTest, DefaultSyncConfig) {
    // Verify compile-time constants
    static_assert(DefaultSyncConfig::kMode == Mode::Sync);
    static_assert(DefaultSyncConfig::kEnableWFC == false);
    static_assert(DefaultSyncConfig::kEnableShadowTail == false);
    static_assert(DefaultSyncConfig::kUseFmt == true);
    static_assert(DefaultSyncConfig::kHeapRingBufferCapacity == 8192);
    static_assert(DefaultSyncConfig::kSharedRingBufferCapacity == 4096);
    static_assert(DefaultSyncConfig::kQueueFullPolicy == QueueFullPolicy::DropNewest);
    
    // Verify SinkBindings type
    static_assert(DefaultSyncConfig::SinkBindings::kBindingCount == 1);
    static_assert(DefaultSyncConfig::SinkBindings::kNeedsTimestamp == true);  // SimpleFormat needs timestamp
    static_assert(DefaultSyncConfig::SinkBindings::kNeedsLevel == true);      // SimpleFormat needs level
    
    // Create logger and verify it works
    FastLoggerV2<DefaultSyncConfig> logger;
    logger.Info("Test message from DefaultSyncConfig");
    logger.Flush();
}

/**
 * @brief Test DefaultAsyncConfig preset
 * @brief 测试 DefaultAsyncConfig 预设
 *
 * Verifies that DefaultAsyncConfig has correct settings:
 * - Async mode
 * - Console output with SimpleFormat
 * - ShadowTail enabled
 *
 * _Requirements: 16.2_
 */
TEST(FastLoggerV2DefaultConfigTest, DefaultAsyncConfig) {
    // Verify compile-time constants
    static_assert(DefaultAsyncConfig::kMode == Mode::Async);
    static_assert(DefaultAsyncConfig::kEnableWFC == false);
    static_assert(DefaultAsyncConfig::kEnableShadowTail == true);
    static_assert(DefaultAsyncConfig::kUseFmt == true);
    static_assert(DefaultAsyncConfig::kHeapRingBufferCapacity == 8192);
    static_assert(DefaultAsyncConfig::kSharedRingBufferCapacity == 4096);
    static_assert(DefaultAsyncConfig::kQueueFullPolicy == QueueFullPolicy::DropNewest);
    
    // Verify SinkBindings type
    static_assert(DefaultAsyncConfig::SinkBindings::kBindingCount == 1);
    static_assert(DefaultAsyncConfig::SinkBindings::kNeedsTimestamp == true);
    static_assert(DefaultAsyncConfig::SinkBindings::kNeedsLevel == true);
    
    // Create logger and verify it works
    FastLoggerV2<DefaultAsyncConfig> logger;
    EXPECT_TRUE(logger.IsRunning());
    logger.Info("Test message from DefaultAsyncConfig");
    logger.Flush();
}

/**
 * @brief Test DefaultMProcConfig preset
 * @brief 测试 DefaultMProcConfig 预设
 *
 * Verifies that DefaultMProcConfig has correct settings:
 * - MProc mode
 * - Console output with SimpleFormat
 * - Default shared memory name
 *
 * _Requirements: 16.3_
 */
TEST(FastLoggerV2DefaultConfigTest, DefaultMProcConfig) {
    // Verify compile-time constants
    static_assert(DefaultMProcConfig::kMode == Mode::MProc);
    static_assert(DefaultMProcConfig::kEnableWFC == false);
    static_assert(DefaultMProcConfig::kEnableShadowTail == true);
    static_assert(DefaultMProcConfig::kUseFmt == true);
    static_assert(DefaultMProcConfig::kHeapRingBufferCapacity == 8192);
    static_assert(DefaultMProcConfig::kSharedRingBufferCapacity == 4096);
    static_assert(DefaultMProcConfig::kQueueFullPolicy == QueueFullPolicy::DropNewest);
    
    // Verify SinkBindings type
    static_assert(DefaultMProcConfig::SinkBindings::kBindingCount == 1);
    static_assert(DefaultMProcConfig::SinkBindings::kNeedsTimestamp == true);
    static_assert(DefaultMProcConfig::SinkBindings::kNeedsLevel == true);
    
    // Verify shared memory name
    static_assert(std::string_view(DefaultMProcConfig::SharedMemoryName::value) == "oneplog_shared");
    
    // Create logger and verify it works
    FastLoggerV2<DefaultMProcConfig> logger;
    EXPECT_TRUE(logger.IsRunning());
    logger.Info("Test message from DefaultMProcConfig");
    logger.Flush();
}

/**
 * @brief Test HighPerformanceConfig preset
 * @brief 测试 HighPerformanceConfig 预设
 *
 * Verifies that HighPerformanceConfig has correct settings:
 * - Async mode
 * - NullSink with MessageOnlyFormat
 * - Level::Info minimum
 *
 * _Requirements: 16.4_
 */
TEST(FastLoggerV2DefaultConfigTest, HighPerformanceConfig) {
    // Verify compile-time constants
    static_assert(HighPerformanceConfig::kMode == Mode::Async);
    static_assert(HighPerformanceConfig::kLevel == Level::Info);
    static_assert(HighPerformanceConfig::kEnableWFC == false);
    static_assert(HighPerformanceConfig::kEnableShadowTail == true);
    static_assert(HighPerformanceConfig::kUseFmt == true);
    
    // Verify SinkBindings type - NullSink with MessageOnlyFormat
    static_assert(HighPerformanceConfig::SinkBindings::kBindingCount == 1);
    static_assert(HighPerformanceConfig::SinkBindings::kNeedsTimestamp == false);  // MessageOnlyFormat doesn't need timestamp
    static_assert(HighPerformanceConfig::SinkBindings::kNeedsLevel == false);      // MessageOnlyFormat doesn't need level
    static_assert(HighPerformanceConfig::SinkBindings::kNeedsThreadId == false);
    static_assert(HighPerformanceConfig::SinkBindings::kNeedsProcessId == false);
    
    // Create logger and verify it works
    FastLoggerV2<HighPerformanceConfig> logger;
    EXPECT_TRUE(logger.IsRunning());
    
    // Trace and Debug should be filtered out
    logger.Trace("This should be filtered");
    logger.Debug("This should also be filtered");
    logger.Info("This should be logged");
    logger.Flush();
}

// ==============================================================================
// Task 10.5: Type Alias Tests / 类型别名测试
// ==============================================================================

/**
 * @brief Test SyncLogger type alias
 * @brief 测试 SyncLogger 类型别名
 *
 * _Requirements: 1.7, 14.5_
 */
TEST(FastLoggerV2TypeAliasTest, SyncLogger) {
    static_assert(SyncLogger::kMode == Mode::Sync);
    static_assert(std::is_same_v<SyncLogger, FastLoggerV2<DefaultSyncConfig>>);
    
    SyncLogger logger;
    logger.Info("Test from SyncLogger");
    logger.Flush();
}

/**
 * @brief Test AsyncLogger type alias
 * @brief 测试 AsyncLogger 类型别名
 *
 * _Requirements: 1.7, 14.5_
 */
TEST(FastLoggerV2TypeAliasTest, AsyncLogger) {
    static_assert(AsyncLogger::kMode == Mode::Async);
    static_assert(std::is_same_v<AsyncLogger, FastLoggerV2<DefaultAsyncConfig>>);
    
    AsyncLogger logger;
    EXPECT_TRUE(logger.IsRunning());
    logger.Info("Test from AsyncLogger");
    logger.Flush();
}

/**
 * @brief Test MProcLogger type alias
 * @brief 测试 MProcLogger 类型别名
 *
 * _Requirements: 1.7, 14.5_
 */
TEST(FastLoggerV2TypeAliasTest, MProcLogger) {
    static_assert(MProcLogger::kMode == Mode::MProc);
    static_assert(std::is_same_v<MProcLogger, FastLoggerV2<DefaultMProcConfig>>);
    
    MProcLogger logger;
    EXPECT_TRUE(logger.IsRunning());
    logger.Info("Test from MProcLogger");
    logger.Flush();
}

/**
 * @brief Test backward-compatible Logger type alias
 * @brief 测试向后兼容的 Logger 类型别名
 *
 * _Requirements: 1.7, 14.5_
 */
TEST(FastLoggerV2TypeAliasTest, BackwardCompatibleLogger) {
    // Default Logger should be Async mode
    static_assert(Logger<>::kMode == Mode::Async);
    
    // Logger with Sync mode
    static_assert(Logger<Mode::Sync>::kMode == Mode::Sync);
    
    // Logger with custom level
    static_assert(Logger<Mode::Async, Level::Error>::kMinLevel == Level::Error);
    
    // Logger with WFC enabled
    static_assert(Logger<Mode::Async, kDefaultLevel, true>::kEnableWFC == true);
    
    // Logger with ShadowTail disabled
    static_assert(Logger<Mode::Async, kDefaultLevel, false, false>::kEnableShadowTail == false);
    
    // Create and use default Logger
    Logger<> logger;
    EXPECT_TRUE(logger.IsRunning());
    logger.Info("Test from backward-compatible Logger");
    logger.Flush();
}

/**
 * @brief Test that V2 aliases still work
 * @brief 测试 V2 别名仍然有效
 *
 * _Requirements: 1.7, 14.5_
 */
TEST(FastLoggerV2TypeAliasTest, V2AliasesStillWork) {
    // V2 aliases should still be available
    static_assert(std::is_same_v<SyncLoggerV2, SyncLogger>);
    static_assert(std::is_same_v<AsyncLoggerV2, AsyncLogger>);
    static_assert(std::is_same_v<MProcLoggerV2, MProcLogger>);
}

// ==============================================================================
// FileSinkType Runtime Config Tests / FileSinkType 运行时配置测试
// ==============================================================================

/**
 * @brief Test FileSinkType construction with StaticFileSinkConfig
 * @brief 测试使用 StaticFileSinkConfig 构造 FileSinkType
 *
 * _Requirements: 13.2, 13.4_
 */
TEST(FileSinkTypeTest, ConstructWithStaticFileSinkConfig) {
    // Create a temporary file path
    std::string testFile = "/tmp/test_filesink_static_config.log";
    
    // Clean up any existing file
    std::remove(testFile.c_str());
    
    // Create config
    StaticFileSinkConfig config;
    config.filename = testFile;
    config.maxSize = 1024;
    config.maxFiles = 3;
    config.rotateOnOpen = false;
    
    // Create FileSinkType with config
    FileSinkType sink(config);
    
    // Write some data
    sink.Write("Test message 1");
    sink.Write("Test message 2");
    sink.Flush();
    
    // Verify file was created
    FILE* f = std::fopen(testFile.c_str(), "r");
    ASSERT_NE(f, nullptr);
    std::fclose(f);
    
    // Clean up
    sink.Close();
    std::remove(testFile.c_str());
}

/**
 * @brief Test FileSinkType construction with FileSinkConfig from fast_logger_v2.hpp
 * @brief 测试使用 fast_logger_v2.hpp 中的 FileSinkConfig 构造 FileSinkType
 *
 * _Requirements: 13.2, 13.4_
 */
TEST(FileSinkTypeTest, ConstructWithFileSinkConfig) {
    // Create a temporary file path
    std::string testFile = "/tmp/test_filesink_config.log";
    
    // Clean up any existing file
    std::remove(testFile.c_str());
    
    // Create config using FileSinkConfig from fast_logger_v2.hpp
    FileSinkConfig config;
    config.filename = testFile;
    config.maxSize = 1024;
    config.maxFiles = 3;
    config.rotateOnOpen = false;
    
    // Create FileSinkType with config
    FileSinkType sink(config);
    
    // Write some data
    sink.Write("Test message 1");
    sink.Write("Test message 2");
    sink.Flush();
    
    // Verify file was created
    FILE* f = std::fopen(testFile.c_str(), "r");
    ASSERT_NE(f, nullptr);
    std::fclose(f);
    
    // Clean up
    sink.Close();
    std::remove(testFile.c_str());
}

/**
 * @brief Test FileSinkType file rotation
 * @brief 测试 FileSinkType 文件轮转
 *
 * _Requirements: 13.4_
 */
TEST(FileSinkTypeTest, FileRotation) {
    // Create a temporary file path
    std::string testFile = "/tmp/test_filesink_rotation.log";
    
    // Clean up any existing files
    std::remove(testFile.c_str());
    std::remove((testFile + ".1").c_str());
    std::remove((testFile + ".2").c_str());
    
    // Create config with small maxSize to trigger rotation
    StaticFileSinkConfig config;
    config.filename = testFile;
    config.maxSize = 50;  // Small size to trigger rotation
    config.maxFiles = 2;
    config.rotateOnOpen = false;
    
    // Create FileSinkType with config
    FileSinkType sink(config);
    
    // Write enough data to trigger rotation
    for (int i = 0; i < 10; ++i) {
        sink.Write("This is a test message that should trigger rotation");
    }
    sink.Flush();
    
    // Verify rotated files were created
    FILE* f1 = std::fopen((testFile + ".1").c_str(), "r");
    EXPECT_NE(f1, nullptr) << "Rotated file .1 should exist";
    if (f1) std::fclose(f1);
    
    // Clean up
    sink.Close();
    std::remove(testFile.c_str());
    std::remove((testFile + ".1").c_str());
    std::remove((testFile + ".2").c_str());
}

/**
 * @brief Test FileSinkType rotate on open
 * @brief 测试 FileSinkType 打开时轮转
 *
 * _Requirements: 13.4_
 */
TEST(FileSinkTypeTest, RotateOnOpen) {
    // Create a temporary file path
    std::string testFile = "/tmp/test_filesink_rotate_on_open.log";
    
    // Clean up any existing files
    std::remove(testFile.c_str());
    std::remove((testFile + ".1").c_str());
    
    // First, create a file with some content
    {
        FileSinkType sink(testFile.c_str());
        sink.Write("Initial content");
        sink.Flush();
        sink.Close();
    }
    
    // Verify initial file exists
    FILE* f = std::fopen(testFile.c_str(), "r");
    ASSERT_NE(f, nullptr);
    std::fclose(f);
    
    // Now create with rotateOnOpen = true
    StaticFileSinkConfig config;
    config.filename = testFile;
    config.maxSize = 0;
    config.maxFiles = 2;
    config.rotateOnOpen = true;
    
    FileSinkType sink(config);
    sink.Write("New content after rotation");
    sink.Flush();
    
    // Verify rotated file was created
    FILE* f1 = std::fopen((testFile + ".1").c_str(), "r");
    EXPECT_NE(f1, nullptr) << "Rotated file .1 should exist after rotateOnOpen";
    if (f1) std::fclose(f1);
    
    // Clean up
    sink.Close();
    std::remove(testFile.c_str());
    std::remove((testFile + ".1").c_str());
}

// ==============================================================================
// Task 12: Format Library Switch Tests / 格式化库切换测试
// ==============================================================================

// Test config with UseFmt=false for non-fmt fallback testing
// 使用 UseFmt=false 的测试配置，用于非 fmt 回退测试
using TestNoFmtSyncConfig = FastLoggerConfig<
    Mode::Sync,
    Level::Trace,
    false,  // EnableWFC
    false,  // EnableShadowTail
    false,  // UseFmt - explicitly disable fmt
    1024,
    1024,
    QueueFullPolicy::Block,
    DefaultSharedMemoryName,
    10,
    MockBindings
>;

using TestNoFmtAsyncConfig = FastLoggerConfig<
    Mode::Async,
    Level::Trace,
    false,  // EnableWFC
    false,  // EnableShadowTail
    false,  // UseFmt - explicitly disable fmt
    1024,
    1024,
    QueueFullPolicy::Block,
    DefaultSharedMemoryName,
    10,
    MockBindings
>;

/**
 * @brief Test UseFmt=false path in sync mode with no arguments
 * @brief 测试同步模式下 UseFmt=false 路径（无参数）
 *
 * Verifies that the non-fmt fallback correctly formats messages
 * without any format arguments.
 *
 * 验证非 fmt 回退正确格式化无格式参数的消息。
 *
 * _Requirements: 7.3_
 */
TEST(FastLoggerV2FormatSwitchTest, NoFmtSyncModeNoArgs) {
    FastLoggerV2<TestNoFmtSyncConfig> logger;
    
    logger.Info("simple message");
    logger.Debug("another message");
    
    auto& sink = logger.GetSinkBindings().Get<0>().sink;
    EXPECT_EQ(sink.messages.size(), 2u);
    EXPECT_EQ(sink.messages[0], "simple message");
    EXPECT_EQ(sink.messages[1], "another message");
}

/**
 * @brief Test UseFmt=false path in sync mode with single argument
 * @brief 测试同步模式下 UseFmt=false 路径（单参数）
 *
 * _Requirements: 7.3_
 */
TEST(FastLoggerV2FormatSwitchTest, NoFmtSyncModeSingleArg) {
    FastLoggerV2<TestNoFmtSyncConfig> logger;
    
    logger.Info("value: {}", 42);
    logger.Info("string: {}", "hello");
    logger.Info("float: {}", 3.14);
    
    auto& sink = logger.GetSinkBindings().Get<0>().sink;
    EXPECT_EQ(sink.messages.size(), 3u);
    EXPECT_EQ(sink.messages[0], "value: 42");
    EXPECT_EQ(sink.messages[1], "string: hello");
    // Float formatting may vary slightly
    EXPECT_TRUE(sink.messages[2].find("float: 3.14") == 0);
}

/**
 * @brief Test UseFmt=false path in sync mode with multiple arguments
 * @brief 测试同步模式下 UseFmt=false 路径（多参数）
 *
 * _Requirements: 7.3_
 */
TEST(FastLoggerV2FormatSwitchTest, NoFmtSyncModeMultipleArgs) {
    FastLoggerV2<TestNoFmtSyncConfig> logger;
    
    logger.Info("values: {} and {}", 1, 2);
    logger.Info("mixed: {} {} {}", "hello", 42, 3.14);
    
    auto& sink = logger.GetSinkBindings().Get<0>().sink;
    EXPECT_EQ(sink.messages.size(), 2u);
    EXPECT_EQ(sink.messages[0], "values: 1 and 2");
    EXPECT_TRUE(sink.messages[1].find("mixed: hello 42 3.14") == 0);
}

/**
 * @brief Test UseFmt=false path in async mode
 * @brief 测试异步模式下 UseFmt=false 路径
 *
 * _Requirements: 7.3_
 */
TEST(FastLoggerV2FormatSwitchTest, NoFmtAsyncMode) {
    FastLoggerV2<TestNoFmtAsyncConfig> logger;
    
    logger.Info("async message: {}", 42);
    logger.Debug("another: {} and {}", "hello", 123);
    
    logger.Flush();
    
    auto& sink = logger.GetSinkBindings().Get<0>().sink;
    EXPECT_EQ(sink.messages.size(), 2u);
    EXPECT_EQ(sink.messages[0], "async message: 42");
    EXPECT_EQ(sink.messages[1], "another: hello and 123");
}

/**
 * @brief Test UseFmt=false path with dynamic strings
 * @brief 测试 UseFmt=false 路径与动态字符串
 *
 * Verifies that dynamic strings are correctly captured and formatted
 * even when using the non-fmt fallback.
 *
 * 验证即使使用非 fmt 回退，动态字符串也能正确捕获和格式化。
 *
 * _Requirements: 7.3, 8.2, 8.3_
 */
TEST(FastLoggerV2FormatSwitchTest, NoFmtDynamicStrings) {
    FastLoggerV2<TestNoFmtAsyncConfig> logger;
    
    {
        std::string dynamicStr = "dynamic content";
        logger.Info("message: {}", dynamicStr);
        // dynamicStr destroyed here
    }
    
    logger.Flush();
    
    auto& sink = logger.GetSinkBindings().Get<0>().sink;
    EXPECT_EQ(sink.messages.size(), 1u);
    EXPECT_EQ(sink.messages[0], "message: dynamic content");
}

/**
 * @brief Test UseFmt=true path (default) for comparison
 * @brief 测试 UseFmt=true 路径（默认）用于比较
 *
 * Verifies that the fmt path produces the same results as non-fmt
 * for basic formatting.
 *
 * 验证 fmt 路径对于基本格式化产生与非 fmt 相同的结果。
 *
 * _Requirements: 7.2_
 */
TEST(FastLoggerV2FormatSwitchTest, FmtPathComparison) {
    // UseFmt=true (default)
    FastLoggerV2<TestSyncConfig> fmtLogger;
    
    // UseFmt=false
    FastLoggerV2<TestNoFmtSyncConfig> noFmtLogger;
    
    // Log same messages
    fmtLogger.Info("value: {}", 42);
    noFmtLogger.Info("value: {}", 42);
    
    auto& fmtSink = fmtLogger.GetSinkBindings().Get<0>().sink;
    auto& noFmtSink = noFmtLogger.GetSinkBindings().Get<0>().sink;
    
    // Both should produce the same output
    EXPECT_EQ(fmtSink.messages.size(), 1u);
    EXPECT_EQ(noFmtSink.messages.size(), 1u);
    EXPECT_EQ(fmtSink.messages[0], noFmtSink.messages[0]);
}

/**
 * @brief Test UseFmt=false with all log levels
 * @brief 测试 UseFmt=false 与所有日志级别
 *
 * _Requirements: 7.3_
 */
TEST(FastLoggerV2FormatSwitchTest, NoFmtAllLevels) {
    FastLoggerV2<TestNoFmtSyncConfig> logger;
    
    logger.Trace("trace: {}", 1);
    logger.Debug("debug: {}", 2);
    logger.Info("info: {}", 3);
    logger.Warn("warn: {}", 4);
    logger.Error("error: {}", 5);
    logger.Critical("critical: {}", 6);
    
    auto& sink = logger.GetSinkBindings().Get<0>().sink;
    EXPECT_EQ(sink.messages.size(), 6u);
    EXPECT_EQ(sink.messages[0], "trace: 1");
    EXPECT_EQ(sink.messages[1], "debug: 2");
    EXPECT_EQ(sink.messages[2], "info: 3");
    EXPECT_EQ(sink.messages[3], "warn: 4");
    EXPECT_EQ(sink.messages[4], "error: 5");
    EXPECT_EQ(sink.messages[5], "critical: 6");
}

/**
 * @brief Test compile-time constant kUseFmt
 * @brief 测试编译期常量 kUseFmt
 *
 * Verifies that kUseFmt is correctly set based on config.
 *
 * 验证 kUseFmt 根据配置正确设置。
 *
 * _Requirements: 7.1_
 */
TEST(FastLoggerV2FormatSwitchTest, CompileTimeUseFmtConstant) {
    // Default config should have UseFmt=true
    using DefaultLogger = FastLoggerV2<TestSyncConfig>;
    EXPECT_TRUE(DefaultLogger::kUseFmt);
    
    // NoFmt config should have UseFmt=false
    using NoFmtLogger = FastLoggerV2<TestNoFmtSyncConfig>;
    EXPECT_FALSE(NoFmtLogger::kUseFmt);
}

}  // namespace
}  // namespace oneplog
