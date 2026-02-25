/**
 * @file test_logger_config.cpp
 * @brief Tests for Logger compile-time configuration infrastructure
 * @brief Logger 编译期配置基础设施测试
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <gtest/gtest.h>
#include "oneplog/internal/logger_config.hpp"

namespace oneplog {
namespace {

// ==============================================================================
// Test StaticFormatRequirements / 测试 StaticFormatRequirements
// ==============================================================================

TEST(StaticFormatRequirementsTest, DefaultValues) {
    using Req = StaticFormatRequirements<>;
    
    // Default values should be: timestamp=true, level=true, others=false
    EXPECT_TRUE(Req::kNeedsTimestamp);
    EXPECT_TRUE(Req::kNeedsLevel);
    EXPECT_FALSE(Req::kNeedsThreadId);
    EXPECT_FALSE(Req::kNeedsProcessId);
    EXPECT_FALSE(Req::kNeedsSourceLocation);
}

TEST(StaticFormatRequirementsTest, CustomValues) {
    using Req = StaticFormatRequirements<false, false, true, true, true>;
    
    EXPECT_FALSE(Req::kNeedsTimestamp);
    EXPECT_FALSE(Req::kNeedsLevel);
    EXPECT_TRUE(Req::kNeedsThreadId);
    EXPECT_TRUE(Req::kNeedsProcessId);
    EXPECT_TRUE(Req::kNeedsSourceLocation);
}

TEST(StaticFormatRequirementsTest, Presets) {
    // NoRequirements
    EXPECT_FALSE(NoRequirements::kNeedsTimestamp);
    EXPECT_FALSE(NoRequirements::kNeedsLevel);
    EXPECT_FALSE(NoRequirements::kNeedsThreadId);
    EXPECT_FALSE(NoRequirements::kNeedsProcessId);
    EXPECT_FALSE(NoRequirements::kNeedsSourceLocation);
    
    // BasicRequirements
    EXPECT_TRUE(BasicRequirements::kNeedsTimestamp);
    EXPECT_TRUE(BasicRequirements::kNeedsLevel);
    EXPECT_FALSE(BasicRequirements::kNeedsThreadId);
    EXPECT_FALSE(BasicRequirements::kNeedsProcessId);
    
    // FullRequirements
    EXPECT_TRUE(FullRequirements::kNeedsTimestamp);
    EXPECT_TRUE(FullRequirements::kNeedsLevel);
    EXPECT_TRUE(FullRequirements::kNeedsThreadId);
    EXPECT_TRUE(FullRequirements::kNeedsProcessId);
    EXPECT_FALSE(FullRequirements::kNeedsSourceLocation);
    
    // DebugRequirements
    EXPECT_TRUE(DebugRequirements::kNeedsSourceLocation);
}

// ==============================================================================
// Mock Format and Sink for testing / 用于测试的模拟 Format 和 Sink
// ==============================================================================

struct MockFormat1 {
    using Requirements = StaticFormatRequirements<true, true, false, false, false>;
    
    template<typename... Args>
    static void FormatTo(fmt::memory_buffer& buffer, Level, uint64_t, uint32_t, uint32_t,
                         const char* fmt, Args&&...) {
        buffer.append(std::string_view(fmt));
    }
    
    static void FormatEntryTo(fmt::memory_buffer& buffer, const LogEntry& entry) {
        std::string msg = entry.snapshot.FormatAll();
        buffer.append(std::string_view(msg));
    }
    
    static std::string FormatEntry(const LogEntry& entry) {
        return entry.snapshot.FormatAll();
    }
    
    static std::string FormatEntry(Level, uint64_t, uint32_t, uint32_t, 
                                   const BinarySnapshot& snapshot) {
        return snapshot.FormatAll();
    }
};

struct MockFormat2 {
    using Requirements = StaticFormatRequirements<false, true, true, true, false>;
    
    template<typename... Args>
    static void FormatTo(fmt::memory_buffer& buffer, Level, uint64_t, uint32_t, uint32_t,
                         const char* fmt, Args&&...) {
        buffer.append(std::string_view(fmt));
    }
    
    static void FormatEntryTo(fmt::memory_buffer& buffer, const LogEntry& entry) {
        std::string msg = entry.snapshot.FormatAll();
        buffer.append(std::string_view(msg));
    }
    
    static std::string FormatEntry(const LogEntry& entry) {
        return entry.snapshot.FormatAll();
    }
    
    static std::string FormatEntry(Level, uint64_t, uint32_t, uint32_t, 
                                   const BinarySnapshot& snapshot) {
        return snapshot.FormatAll();
    }
};

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
};

// ==============================================================================
// Test SinkBinding / 测试 SinkBinding
// ==============================================================================

TEST(SinkBindingTest, TypeAliases) {
    using Binding = SinkBinding<MockSink, MockFormat1>;
    
    static_assert(std::is_same_v<Binding::Sink, MockSink>);
    static_assert(std::is_same_v<Binding::Format, MockFormat1>);
    static_assert(std::is_same_v<Binding::Requirements, MockFormat1::Requirements>);
}

TEST(SinkBindingTest, FlushAndClose) {
    SinkBinding<MockSink, MockFormat1> binding;
    
    EXPECT_FALSE(binding.sink.flushed);
    EXPECT_FALSE(binding.sink.closed);
    
    binding.Flush();
    EXPECT_TRUE(binding.sink.flushed);
    
    binding.Close();
    EXPECT_TRUE(binding.sink.closed);
}

// ==============================================================================
// Test SinkBindingList / 测试 SinkBindingList
// ==============================================================================

TEST(SinkBindingListTest, EmptyList) {
    using List = SinkBindingList<>;
    
    EXPECT_EQ(List::kBindingCount, 0u);
    EXPECT_FALSE(List::kNeedsTimestamp);
    EXPECT_FALSE(List::kNeedsLevel);
    EXPECT_FALSE(List::kNeedsThreadId);
    EXPECT_FALSE(List::kNeedsProcessId);
    EXPECT_FALSE(List::kNeedsSourceLocation);
}

TEST(SinkBindingListTest, SingleBinding) {
    using Binding = SinkBinding<MockSink, MockFormat1>;
    using List = SinkBindingList<Binding>;
    
    EXPECT_EQ(List::kBindingCount, 1u);
    EXPECT_TRUE(List::kNeedsTimestamp);
    EXPECT_TRUE(List::kNeedsLevel);
    EXPECT_FALSE(List::kNeedsThreadId);
    EXPECT_FALSE(List::kNeedsProcessId);
}

TEST(SinkBindingListTest, RequirementUnion) {
    using Binding1 = SinkBinding<MockSink, MockFormat1>;
    using Binding2 = SinkBinding<MockSink, MockFormat2>;
    using List = SinkBindingList<Binding1, Binding2>;
    
    EXPECT_EQ(List::kBindingCount, 2u);
    
    // Union: MockFormat1 needs timestamp, MockFormat2 doesn't
    // Result should be true (at least one needs it)
    EXPECT_TRUE(List::kNeedsTimestamp);
    
    // Both need level
    EXPECT_TRUE(List::kNeedsLevel);
    
    // Only MockFormat2 needs threadId and processId
    EXPECT_TRUE(List::kNeedsThreadId);
    EXPECT_TRUE(List::kNeedsProcessId);
    
    // Neither needs source location
    EXPECT_FALSE(List::kNeedsSourceLocation);
}

TEST(SinkBindingListTest, FlushAllAndCloseAll) {
    using Binding = SinkBinding<MockSink, MockFormat1>;
    SinkBindingList<Binding, Binding> list;
    
    list.FlushAll();
    EXPECT_TRUE(list.Get<0>().sink.flushed);
    EXPECT_TRUE(list.Get<1>().sink.flushed);
    
    list.CloseAll();
    EXPECT_TRUE(list.Get<0>().sink.closed);
    EXPECT_TRUE(list.Get<1>().sink.closed);
}

// ==============================================================================
// Test FastLoggerConfig / 测试 FastLoggerConfig
// ==============================================================================

TEST(FastLoggerConfigTest, DefaultValues) {
    using Config = FastLoggerConfig<>;
    
    EXPECT_EQ(Config::kMode, Mode::Sync);
    EXPECT_EQ(Config::kLevel, kDefaultLevel);
    EXPECT_FALSE(Config::kEnableWFC);
    EXPECT_TRUE(Config::kEnableShadowTail);
    EXPECT_TRUE(Config::kUseFmt);
    EXPECT_EQ(Config::kHeapRingBufferCapacity, 8192u);
    EXPECT_EQ(Config::kSharedRingBufferCapacity, 8192u);
    EXPECT_EQ(Config::kQueueFullPolicy, QueueFullPolicy::Block);
    EXPECT_EQ(Config::kPollTimeout.count(), 10);
}

TEST(FastLoggerConfigTest, CustomValues) {
    using Config = FastLoggerConfig<
        Mode::Async,
        Level::Debug,
        true,   // EnableWFC
        false,  // EnableShadowTail
        false,  // UseFmt
        4096,   // HeapRingBufferCapacity
        2048,   // SharedRingBufferCapacity
        QueueFullPolicy::Block,
        DefaultSharedMemoryName,
        20      // PollTimeoutMs
    >;
    
    EXPECT_EQ(Config::kMode, Mode::Async);
    EXPECT_EQ(Config::kLevel, Level::Debug);
    EXPECT_TRUE(Config::kEnableWFC);
    EXPECT_FALSE(Config::kEnableShadowTail);
    EXPECT_FALSE(Config::kUseFmt);
    EXPECT_EQ(Config::kHeapRingBufferCapacity, 4096u);
    EXPECT_EQ(Config::kSharedRingBufferCapacity, 2048u);
    EXPECT_EQ(Config::kQueueFullPolicy, QueueFullPolicy::Block);
    EXPECT_EQ(Config::kPollTimeout.count(), 20);
}

// Custom shared memory name for testing (must be at namespace scope)
struct CustomSharedMemoryName {
    static constexpr const char* value = "custom_shared_memory";
};

TEST(FastLoggerConfigTest, SharedMemoryName) {
    using Config = FastLoggerConfig<
        Mode::MProc,
        Level::Info,
        false, true, true,
        8192, 4096,
        QueueFullPolicy::DropNewest,
        CustomSharedMemoryName
    >;
    
    EXPECT_STREQ(Config::SharedMemoryName::value, "custom_shared_memory");
}

TEST(FastLoggerConfigTest, WithSinkBindings) {
    using Binding = SinkBinding<MockSink, MockFormat1>;
    using Bindings = SinkBindingList<Binding>;
    using Config = FastLoggerConfig<
        Mode::Async,
        Level::Info,
        false, true, true,
        8192, 4096,
        QueueFullPolicy::DropNewest,
        DefaultSharedMemoryName,
        10,
        Bindings
    >;
    
    static_assert(std::is_same_v<Config::SinkBindings, Bindings>);
    EXPECT_EQ(Config::SinkBindings::kBindingCount, 1u);
}

// ==============================================================================
// Test Configuration Helpers / 测试配置辅助模板
// ==============================================================================

TEST(ConfigHelpersTest, AsyncFastLoggerConfig) {
    using Binding = SinkBinding<MockSink, MockFormat1>;
    using Bindings = SinkBindingList<Binding>;
    using Config = AsyncFastLoggerConfig<Bindings, Level::Debug>;
    
    EXPECT_EQ(Config::kMode, Mode::Async);
    EXPECT_EQ(Config::kLevel, Level::Debug);
}

TEST(ConfigHelpersTest, SyncFastLoggerConfig) {
    using Binding = SinkBinding<MockSink, MockFormat1>;
    using Bindings = SinkBindingList<Binding>;
    using Config = SyncFastLoggerConfig<Bindings, Level::Warn>;
    
    EXPECT_EQ(Config::kMode, Mode::Sync);
    EXPECT_EQ(Config::kLevel, Level::Warn);
    EXPECT_FALSE(Config::kEnableWFC);
    EXPECT_FALSE(Config::kEnableShadowTail);
}

}  // namespace
}  // namespace oneplog
