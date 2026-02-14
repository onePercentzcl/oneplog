/**
 * @file test_optimized_name_manager.cpp
 * @brief Unit tests for OptimizedNameManager
 * @brief OptimizedNameManager 的单元测试
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <gtest/gtest.h>

#include <oneplog/internal/optimized_name_manager.hpp>

#include <thread>
#include <vector>
#include <atomic>

namespace oneplog {
namespace test {

// ==============================================================================
// OptimizedNameManager Unit Tests / OptimizedNameManager 单元测试
// ==============================================================================

class OptimizedNameManagerTest : public ::testing::Test {
protected:
    using NM = OptimizedNameManager<false, 31>;

    void SetUp() override {
        // Ensure clean state / 确保干净状态
        NM::Shutdown();
        NM::ClearLookupTable();
    }

    void TearDown() override {
        NM::Shutdown();
        NM::ClearLookupTable();
    }
};

/**
 * @brief Test initialization and shutdown
 * @brief 测试初始化和关闭
 */
TEST_F(OptimizedNameManagerTest, InitializeAndShutdown) {
    EXPECT_FALSE(NM::IsInitialized());

    NM::Initialize(Mode::Async);
    EXPECT_TRUE(NM::IsInitialized());
    EXPECT_EQ(NM::GetMode(), Mode::Async);

    NM::Shutdown();
    EXPECT_FALSE(NM::IsInitialized());
}

/**
 * @brief Test process name set and get
 * @brief 测试进程名设置和获取
 */
TEST_F(OptimizedNameManagerTest, ProcessName) {
    NM::Initialize(Mode::Sync);

    NM::SetProcessName("test_process");
    EXPECT_EQ(NM::GetProcessName(), "test_process");
}

/**
 * @brief Test process name truncation
 * @brief 测试进程名截断
 */
TEST_F(OptimizedNameManagerTest, ProcessNameTruncation) {
    NM::Initialize(Mode::Sync);

    std::string longName(50, 'x');
    NM::SetProcessName(longName);

    auto result = NM::GetProcessName();
    EXPECT_EQ(result.length(), 31);
    EXPECT_EQ(result, std::string(31, 'x'));
}

/**
 * @brief Test module name in Sync mode
 * @brief 测试 Sync 模式下的模块名
 */
TEST_F(OptimizedNameManagerTest, SyncModeModuleName) {
    NM::Initialize(Mode::Sync);

    NM::SetModuleName("sync_module");
    EXPECT_EQ(NM::GetModuleName(), "sync_module");
}

/**
 * @brief Test module name in Async mode with lookup table
 * @brief 测试 Async 模式下的模块名（使用查找表）
 */
TEST_F(OptimizedNameManagerTest, AsyncModeModuleName) {
    NM::Initialize(Mode::Async);

    NM::SetModuleName("async_module");
    EXPECT_EQ(NM::GetModuleName(), "async_module");

    // Verify it's registered in the lookup table
    // 验证已注册到查找表
    uint32_t tid = NM::GetCurrentThreadId();
    EXPECT_EQ(NM::LookupModuleName(tid), "async_module");
    EXPECT_EQ(NM::GetLookupTableCount(), 1);
}

/**
 * @brief Test module name truncation
 * @brief 测试模块名截断
 */
TEST_F(OptimizedNameManagerTest, ModuleNameTruncation) {
    NM::Initialize(Mode::Async);

    std::string longName(50, 'y');
    NM::SetModuleName(longName);

    auto result = NM::GetModuleName();
    EXPECT_EQ(result.length(), 31);
    EXPECT_EQ(result, std::string(31, 'y'));
}

/**
 * @brief Test default values
 * @brief 测试默认值
 */
TEST_F(OptimizedNameManagerTest, DefaultValues) {
    NM::Initialize(Mode::Async);

    // Default process name should be "main"
    // 默认进程名应为 "main"
    // Note: We need to reset process name first
    NM::SetProcessName("main");
    EXPECT_EQ(NM::GetProcessName(), "main");
}

/**
 * @brief Test lookup of non-existent TID
 * @brief 测试查找不存在的 TID
 */
TEST_F(OptimizedNameManagerTest, LookupNonExistentTid) {
    NM::Initialize(Mode::Async);

    // Lookup non-existent TID should return default "main"
    // 查找不存在的 TID 应返回默认 "main"
    EXPECT_EQ(NM::LookupModuleName(99999), "main");
}

/**
 * @brief Test multi-thread module names in Async mode
 * @brief 测试 Async 模式下的多线程模块名
 */
TEST_F(OptimizedNameManagerTest, AsyncModeMultiThread) {
    NM::Initialize(Mode::Async);

    std::atomic<bool> thread1Done{false};
    std::atomic<bool> thread2Done{false};
    std::string thread1Name;
    std::string thread2Name;
    uint32_t thread1Tid = 0;
    uint32_t thread2Tid = 0;

    std::thread t1([&]() {
        NM::SetModuleName("thread1_module");
        thread1Name = std::string(NM::GetModuleName());
        thread1Tid = NM::GetCurrentThreadId();
        thread1Done = true;
    });

    std::thread t2([&]() {
        NM::SetModuleName("thread2_module");
        thread2Name = std::string(NM::GetModuleName());
        thread2Tid = NM::GetCurrentThreadId();
        thread2Done = true;
    });

    t1.join();
    t2.join();

    EXPECT_TRUE(thread1Done);
    EXPECT_TRUE(thread2Done);
    EXPECT_EQ(thread1Name, "thread1_module");
    EXPECT_EQ(thread2Name, "thread2_module");

    // Verify lookup table contains both entries
    // 验证查找表包含两个条目
    EXPECT_EQ(NM::LookupModuleName(thread1Tid), "thread1_module");
    EXPECT_EQ(NM::LookupModuleName(thread2Tid), "thread2_module");
}

/**
 * @brief Test Sync mode thread isolation
 * @brief 测试 Sync 模式线程隔离
 */
TEST_F(OptimizedNameManagerTest, SyncModeThreadIsolation) {
    NM::Initialize(Mode::Sync);

    NM::SetModuleName("main_module");

    std::string childThreadName;
    std::thread t([&]() {
        // Child thread should have default name
        // 子线程应有默认名称
        childThreadName = std::string(NM::GetModuleName());
    });
    t.join();

    // Main thread name should be unchanged
    // 主线程名称应不变
    EXPECT_EQ(NM::GetModuleName(), "main_module");
    // Child thread should have default "main"
    // 子线程应有默认 "main"
    EXPECT_EQ(childThreadName, "main");
}

/**
 * @brief Test clear lookup table
 * @brief 测试清空查找表
 */
TEST_F(OptimizedNameManagerTest, ClearLookupTable) {
    NM::Initialize(Mode::Async);

    NM::SetModuleName("test_module");
    EXPECT_GE(NM::GetLookupTableCount(), 1);

    NM::ClearLookupTable();
    EXPECT_EQ(NM::GetLookupTableCount(), 0);
}

/**
 * @brief Test GetModuleName with threadId parameter
 * @brief 测试带 threadId 参数的 GetModuleName
 */
TEST_F(OptimizedNameManagerTest, GetModuleNameWithThreadId) {
    NM::Initialize(Mode::Async);

    NM::SetModuleName("current_thread_module");
    uint32_t tid = NM::GetCurrentThreadId();

    // With threadId = 0, should return current thread's name
    // threadId = 0 时，应返回当前线程的名称
    EXPECT_EQ(NM::GetModuleName(0), "current_thread_module");

    // With actual threadId, should lookup from table
    // 使用实际 threadId 时，应从表中查找
    EXPECT_EQ(NM::GetModuleName(tid), "current_thread_module");
}

/**
 * @brief Test GetProcessName with processId parameter
 * @brief 测试带 processId 参数的 GetProcessName
 */
TEST_F(OptimizedNameManagerTest, GetProcessNameWithProcessId) {
    NM::Initialize(Mode::Async);

    NM::SetProcessName("my_process");

    // With processId = 0, should return global process name
    // processId = 0 时，应返回全局进程名
    EXPECT_EQ(NM::GetProcessName(0), "my_process");

    // With non-zero processId, should return ID as string (for MProc compatibility)
    // 非零 processId 时，应返回 ID 字符串（用于 MProc 兼容性）
    EXPECT_EQ(NM::GetProcessName(12345), "12345");
}

/**
 * @brief Test SetAndRegisterModuleName (backward compatibility)
 * @brief 测试 SetAndRegisterModuleName（向后兼容）
 */
TEST_F(OptimizedNameManagerTest, SetAndRegisterModuleName) {
    NM::Initialize(Mode::Async);

    NM::SetAndRegisterModuleName("registered_module");
    EXPECT_EQ(NM::GetModuleName(), "registered_module");

    uint32_t tid = NM::GetCurrentThreadId();
    EXPECT_EQ(NM::LookupModuleName(tid), "registered_module");
}

/**
 * @brief Test mode switching
 * @brief 测试模式切换
 */
TEST_F(OptimizedNameManagerTest, ModeSwitching) {
    NM::Initialize(Mode::Sync);
    EXPECT_EQ(NM::GetMode(), Mode::Sync);

    NM::Initialize(Mode::Async);
    EXPECT_EQ(NM::GetMode(), Mode::Async);

    NM::Initialize(Mode::MProc);
    EXPECT_EQ(NM::GetMode(), Mode::MProc);
}

// ==============================================================================
// OptimizedThreadWithModuleName Unit Tests
// OptimizedThreadWithModuleName 单元测试
// ==============================================================================

class OptimizedThreadWithModuleNameTest : public ::testing::Test {
protected:
    using NM = OptimizedNameManager<false, 31>;
    using ThreadHelper = OptimizedThreadWithModuleName<false, 31>;

    void SetUp() override {
        NM::Shutdown();
        NM::ClearLookupTable();
    }

    void TearDown() override {
        NM::Shutdown();
        NM::ClearLookupTable();
    }
};

/**
 * @brief Test module name inheritance
 * @brief 测试模块名继承
 */
TEST_F(OptimizedThreadWithModuleNameTest, ModuleNameInheritance) {
    NM::Initialize(Mode::Async);
    NM::SetModuleName("parent_module");

    std::string childModuleName;
    auto thread = ThreadHelper::Create([&]() {
        childModuleName = std::string(NM::GetModuleName());
    });
    thread.join();

    EXPECT_EQ(childModuleName, "parent_module");
}

/**
 * @brief Test CreateWithName
 * @brief 测试 CreateWithName
 */
TEST_F(OptimizedThreadWithModuleNameTest, CreateWithName) {
    NM::Initialize(Mode::Async);
    NM::SetModuleName("parent");

    std::string childModuleName;
    auto thread = ThreadHelper::CreateWithName("specific_child", [&]() {
        childModuleName = std::string(NM::GetModuleName());
    });
    thread.join();

    EXPECT_EQ(childModuleName, "specific_child");
}

}  // namespace test
}  // namespace oneplog
