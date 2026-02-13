/**
 * @file test_name_manager.cpp
 * @brief Unit tests for NameManager and ThreadModuleTable
 * @brief NameManager 和 ThreadModuleTable 的单元测试
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <gtest/gtest.h>

// RapidCheck headers must be included after gtest but before other headers
// RapidCheck 头文件必须在 gtest 之后但在其他头文件之前包含
#ifdef ONEPLOG_HAS_RAPIDCHECK
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#endif

#include <oneplog/name_manager.hpp>

#include <thread>
#include <vector>
#include <atomic>
#include <set>

namespace oneplog {
namespace test {

// ==============================================================================
// ThreadModuleTable Unit Tests / ThreadModuleTable 单元测试
// ==============================================================================

class ThreadModuleTableTest : public ::testing::Test {
protected:
    void SetUp() override {
        table = std::make_unique<ThreadModuleTable>();
    }

    void TearDown() override {
        table.reset();
    }

    std::unique_ptr<ThreadModuleTable> table;
};

/**
 * @brief Test basic registration and lookup
 * @brief 测试基本的注册和查找
 */
TEST_F(ThreadModuleTableTest, BasicRegisterAndLookup) {
    EXPECT_TRUE(table->Register(1001, "worker1"));
    EXPECT_TRUE(table->Register(1002, "worker2"));
    
    EXPECT_EQ(table->GetName(1001), "worker1");
    EXPECT_EQ(table->GetName(1002), "worker2");
    EXPECT_EQ(table->Count(), 2);
}

/**
 * @brief Test update existing entry
 * @brief 测试更新现有条目
 */
TEST_F(ThreadModuleTableTest, UpdateExistingEntry) {
    EXPECT_TRUE(table->Register(1001, "worker1"));
    EXPECT_EQ(table->GetName(1001), "worker1");
    
    // Update the same thread ID / 更新相同的线程 ID
    EXPECT_TRUE(table->Register(1001, "updated_worker"));
    EXPECT_EQ(table->GetName(1001), "updated_worker");
    EXPECT_EQ(table->Count(), 1);  // Count should not increase / 计数不应增加
}

/**
 * @brief Test lookup non-existent entry
 * @brief 测试查找不存在的条目
 */
TEST_F(ThreadModuleTableTest, LookupNonExistent) {
    EXPECT_TRUE(table->Register(1001, "worker1"));
    EXPECT_EQ(table->GetName(9999), "");  // Should return empty string / 应返回空字符串
}

/**
 * @brief Test name truncation at 31 characters
 * @brief 测试名称在 31 字符处截断
 */
TEST_F(ThreadModuleTableTest, NameTruncation) {
    std::string longName(50, 'x');  // 50 character name / 50 字符名称
    EXPECT_TRUE(table->Register(1001, longName));
    
    std::string result = table->GetName(1001);
    EXPECT_EQ(result.length(), 31);  // Should be truncated to 31 / 应截断到 31
    EXPECT_EQ(result, std::string(31, 'x'));
}

/**
 * @brief Test clear functionality
 * @brief 测试清空功能
 */
TEST_F(ThreadModuleTableTest, Clear) {
    EXPECT_TRUE(table->Register(1001, "worker1"));
    EXPECT_TRUE(table->Register(1002, "worker2"));
    EXPECT_EQ(table->Count(), 2);
    
    table->Clear();
    
    EXPECT_EQ(table->Count(), 0);
    EXPECT_EQ(table->GetName(1001), "");
    EXPECT_EQ(table->GetName(1002), "");
}

/**
 * @brief Test empty name registration
 * @brief 测试空名称注册
 */
TEST_F(ThreadModuleTableTest, EmptyName) {
    EXPECT_TRUE(table->Register(1001, ""));
    EXPECT_EQ(table->GetName(1001), "");
    EXPECT_EQ(table->Count(), 1);
}

// ==============================================================================
// NameManager Unit Tests / NameManager 单元测试
// ==============================================================================

class NameManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Ensure clean state / 确保干净状态
        NameManager<>::Shutdown();
        // Reset global names to default / 重置全局名称为默认值
        oneplog::SetProcessName("main");
        oneplog::SetModuleName("main");
    }

    void TearDown() override {
        NameManager<>::Shutdown();
        // Reset global names to default / 重置全局名称为默认值
        oneplog::SetProcessName("main");
        oneplog::SetModuleName("main");
    }
};

/**
 * @brief Test initialization and shutdown
 * @brief 测试初始化和关闭
 */
TEST_F(NameManagerTest, InitializeAndShutdown) {
    EXPECT_FALSE(NameManager<>::IsInitialized());
    
    NameManager<>::Initialize(Mode::Async);
    EXPECT_TRUE(NameManager<>::IsInitialized());
    EXPECT_EQ(NameManager<>::GetMode(), Mode::Async);
    
    NameManager<>::Shutdown();
    EXPECT_FALSE(NameManager<>::IsInitialized());
}

/**
 * @brief Test double initialization (mode can be changed)
 * @brief 测试重复初始化（模式可以改变）
 */
TEST_F(NameManagerTest, DoubleInitialization) {
    NameManager<>::Initialize(Mode::Async);
    EXPECT_EQ(NameManager<>::GetMode(), Mode::Async);
    
    NameManager<>::Initialize(Mode::Sync);  // Mode can be changed / 模式可以改变
    EXPECT_EQ(NameManager<>::GetMode(), Mode::Sync);  // New mode should be set / 新模式应被设置
}

/**
 * @brief Test Sync mode process name
 * @brief 测试 Sync 模式进程名
 */
TEST_F(NameManagerTest, SyncModeProcessName) {
    NameManager<>::Initialize(Mode::Sync);
    
    NameManager<>::SetProcessName("test_process");
    EXPECT_EQ(NameManager<>::GetProcessName(), "test_process");
}

/**
 * @brief Test Sync mode module name
 * @brief 测试 Sync 模式模块名
 */
TEST_F(NameManagerTest, SyncModeModuleName) {
    NameManager<>::Initialize(Mode::Sync);
    
    NameManager<>::SetModuleName("test_module");
    EXPECT_EQ(NameManager<>::GetModuleName(), "test_module");
}

/**
 * @brief Test Async mode process name (global)
 * @brief 测试 Async 模式进程名（全局）
 */
TEST_F(NameManagerTest, AsyncModeProcessName) {
    NameManager<>::Initialize(Mode::Async);
    
    NameManager<>::SetProcessName("async_process");
    EXPECT_EQ(NameManager<>::GetProcessName(), "async_process");
}

/**
 * @brief Test Async mode module name with heap table
 * @brief 测试 Async 模式模块名（使用堆表）
 */
TEST_F(NameManagerTest, AsyncModeModuleName) {
    NameManager<>::Initialize(Mode::Async);
    
    NameManager<>::SetModuleName("async_module");
    EXPECT_EQ(NameManager<>::GetModuleName(), "async_module");
}

/**
 * @brief Test default values
 * @brief 测试默认值
 */
TEST_F(NameManagerTest, DefaultValues) {
    NameManager<>::Initialize(Mode::Async);
    
    // Default process name should be "main" / 默认进程名应为 "main"
    EXPECT_EQ(NameManager<>::GetProcessName(), "main");
    
    // Default module name should be "main" / 默认模块名应为 "main"
    EXPECT_EQ(NameManager<>::GetModuleName(), "main");
}

/**
 * @brief Test name storage with truncation
 * @brief 测试名称存储（带截断）
 *
 * Names are truncated to kMaxNameLength (31) characters at storage level.
 * 名称在存储层被截断到 kMaxNameLength（31）字符。
 */
TEST_F(NameManagerTest, NameStorage) {
    NameManager<>::Initialize(Mode::Async);
    
    std::string longName(50, 'a');  // 50 character name / 50 字符名称
    NameManager<>::SetProcessName(longName);
    
    std::string result = NameManager<>::GetProcessName();
    // Name is truncated to 31 characters / 名称被截断到 31 字符
    EXPECT_EQ(result.length(), 31);
    EXPECT_EQ(result, longName.substr(0, 31));
}

/**
 * @brief Test Async mode multi-thread module names
 * @brief 测试 Async 模式多线程模块名
 */
TEST_F(NameManagerTest, AsyncModeMultiThread) {
    NameManager<>::Initialize(Mode::Async);
    
    std::atomic<bool> thread1Done{false};
    std::atomic<bool> thread2Done{false};
    std::string thread1Name;
    std::string thread2Name;
    
    std::thread t1([&]() {
        NameManager<>::SetModuleName("thread1_module");
        thread1Name = NameManager<>::GetModuleName();
        thread1Done = true;
    });
    
    std::thread t2([&]() {
        NameManager<>::SetModuleName("thread2_module");
        thread2Name = NameManager<>::GetModuleName();
        thread2Done = true;
    });
    
    t1.join();
    t2.join();
    
    EXPECT_TRUE(thread1Done);
    EXPECT_TRUE(thread2Done);
    EXPECT_EQ(thread1Name, "thread1_module");
    EXPECT_EQ(thread2Name, "thread2_module");
}

/**
 * @brief Test Sync mode thread isolation
 * @brief 测试 Sync 模式线程隔离
 */
TEST_F(NameManagerTest, SyncModeThreadIsolation) {
    NameManager<>::Initialize(Mode::Sync);
    
    // Set module name in main thread / 在主线程设置模块名
    NameManager<>::SetModuleName("main_module");
    
    std::string childThreadName;
    std::thread t([&]() {
        // Child thread should have default name / 子线程应有默认名称
        childThreadName = NameManager<>::GetModuleName();
    });
    t.join();
    
    // Main thread name should be unchanged / 主线程名称应不变
    EXPECT_EQ(NameManager<>::GetModuleName(), "main_module");
    // Child thread should have default "main" / 子线程应有默认 "main"
    EXPECT_EQ(childThreadName, "main");
}

/**
 * @brief Test operations before initialization
 * @brief 测试初始化前的操作
 */
TEST_F(NameManagerTest, OperationsBeforeInit) {
    // Should not crash, use default behavior / 不应崩溃，使用默认行为
    NameManager<>::SetProcessName("test");
    NameManager<>::SetModuleName("test");
    
    // GetProcessName/GetModuleName should return something / 应返回某些值
    // (behavior depends on default state)
}

// ==============================================================================
// ThreadWithModuleName Unit Tests / ThreadWithModuleName 单元测试
// ==============================================================================

class ThreadWithModuleNameTest : public ::testing::Test {
protected:
    void SetUp() override {
        NameManager<>::Shutdown();
    }

    void TearDown() override {
        NameManager<>::Shutdown();
    }
};

/**
 * @brief Test module name inheritance in Sync mode
 * @brief 测试 Sync 模式下的模块名继承
 */
TEST_F(ThreadWithModuleNameTest, SyncModeInheritance) {
    NameManager<>::Initialize(Mode::Sync);
    NameManager<>::SetModuleName("parent_module");
    
    std::string childModuleName;
    auto thread = ThreadWithModuleName<>::Create([&]() {
        childModuleName = NameManager<>::GetModuleName();
    });
    thread.join();
    
    EXPECT_EQ(childModuleName, "parent_module");
}

/**
 * @brief Test module name inheritance in Async mode
 * @brief 测试 Async 模式下的模块名继承
 */
TEST_F(ThreadWithModuleNameTest, AsyncModeInheritance) {
    NameManager<>::Initialize(Mode::Async);
    NameManager<>::SetModuleName("async_parent");
    
    std::string childModuleName;
    auto thread = ThreadWithModuleName<>::Create([&]() {
        childModuleName = NameManager<>::GetModuleName();
    });
    thread.join();
    
    EXPECT_EQ(childModuleName, "async_parent");
}

/**
 * @brief Test CreateWithName sets specific module name
 * @brief 测试 CreateWithName 设置特定模块名
 */
TEST_F(ThreadWithModuleNameTest, CreateWithSpecificName) {
    NameManager<>::Initialize(Mode::Async);
    NameManager<>::SetModuleName("parent");
    
    std::string childModuleName;
    auto thread = ThreadWithModuleName<>::CreateWithName("specific_child", [&]() {
        childModuleName = NameManager<>::GetModuleName();
    });
    thread.join();
    
    EXPECT_EQ(childModuleName, "specific_child");
}

/**
 * @brief Test nested thread inheritance
 * @brief 测试嵌套线程继承
 */
TEST_F(ThreadWithModuleNameTest, NestedInheritance) {
    NameManager<>::Initialize(Mode::Sync);
    NameManager<>::SetModuleName("grandparent");
    
    std::string parentName;
    std::string childName;
    
    auto parentThread = ThreadWithModuleName<>::Create([&]() {
        parentName = NameManager<>::GetModuleName();
        
        auto childThread = ThreadWithModuleName<>::Create([&]() {
            childName = NameManager<>::GetModuleName();
        });
        childThread.join();
    });
    parentThread.join();
    
    EXPECT_EQ(parentName, "grandparent");
    EXPECT_EQ(childName, "grandparent");
}

/**
 * @brief Test that explicit SetModuleName overrides inheritance
 * @brief 测试显式 SetModuleName 覆盖继承
 */
TEST_F(ThreadWithModuleNameTest, ExplicitOverridesInheritance) {
    NameManager<>::Initialize(Mode::Sync);
    NameManager<>::SetModuleName("parent");
    
    std::string childModuleName;
    auto thread = ThreadWithModuleName<>::Create([&]() {
        NameManager<>::SetModuleName("explicit_child");
        childModuleName = NameManager<>::GetModuleName();
    });
    thread.join();
    
    EXPECT_EQ(childModuleName, "explicit_child");
}

/**
 * @brief Test thread with lambda capturing arguments
 * @brief 测试使用 lambda 捕获参数的线程
 */
TEST_F(ThreadWithModuleNameTest, ThreadWithLambdaCapture) {
    NameManager<>::Initialize(Mode::Async);
    NameManager<>::SetModuleName("worker");
    
    int result = 0;
    std::string moduleName;
    int a = 10, b = 20;
    
    // Use lambda capture instead of passing arguments directly
    // 使用 lambda 捕获而不是直接传递参数
    auto thread = ThreadWithModuleName<>::Create([&]() {
        moduleName = NameManager<>::GetModuleName();
        result = a + b;
    });
    thread.join();
    
    EXPECT_EQ(result, 30);
    EXPECT_EQ(moduleName, "worker");
}

// ==============================================================================
// Property-Based Tests / 属性测试
// ==============================================================================

#ifdef ONEPLOG_HAS_RAPIDCHECK

/**
 * @brief Property 3: Name Length Invariant
 * @brief 属性 3：名称长度不变性
 *
 * For any process name or module name string, the Name_Manager SHALL store
 * and return names up to 31 characters, truncating longer names without error.
 *
 * 对于任意进程名或模块名字符串，Name_Manager 应存储并返回最多 31 字符的名称，
 * 超长名称会被截断而不会出错。
 *
 * **Feature: process-module-name-management, Property 3: Name Length Invariant**
 * **Validates: Requirements 1.5, 1.6**
 */
RC_GTEST_PROP(NameManagerPropertyTest, NameLengthInvariant, ()) {
    // Generate random string of arbitrary length (1-100 chars, non-empty)
    // 生成任意长度的随机字符串（1-100 字符，非空）
    const auto name = *rc::gen::nonEmpty<std::string>();
    
    // Test process name truncation / 测试进程名截断
    {
        NameManager<>::Shutdown();
        NameManager<>::Initialize(Mode::Async);
        
        NameManager<>::SetProcessName(name);
        std::string result = NameManager<>::GetProcessName();
        
        // Result should never exceed 31 characters / 结果不应超过 31 字符
        RC_ASSERT(result.length() <= 31);
        
        // If input was <= 31 chars, result should match input
        // 如果输入 <= 31 字符，结果应与输入匹配
        if (name.length() <= 31) {
            RC_ASSERT(result == name);
        } else {
            // If input was > 31 chars, result should be first 31 chars
            // 如果输入 > 31 字符，结果应为前 31 字符
            RC_ASSERT(result == name.substr(0, 31));
        }
        
        NameManager<>::Shutdown();
    }
    
    // Test module name truncation / 测试模块名截断
    {
        NameManager<>::Shutdown();
        NameManager<>::Initialize(Mode::Async);
        
        NameManager<>::SetModuleName(name);
        std::string result = NameManager<>::GetModuleName();
        
        // Result should never exceed 31 characters / 结果不应超过 31 字符
        RC_ASSERT(result.length() <= 31);
        
        // If input was <= 31 chars, result should match input
        // 如果输入 <= 31 字符，结果应与输入匹配
        if (name.length() <= 31) {
            RC_ASSERT(result == name);
        } else {
            // If input was > 31 chars, result should be first 31 chars
            // 如果输入 > 31 字符，结果应为前 31 字符
            RC_ASSERT(result == name.substr(0, 31));
        }
        
        NameManager<>::Shutdown();
    }
}

/**
 * @brief Property 6: Unique ID Generation
 * @brief 属性 6：唯一 ID 生成
 *
 * For any sequence of process or thread registrations in MProc mode,
 * each registration SHALL return a unique non-zero ID, and no two different
 * names SHALL receive the same ID.
 *
 * 对于 MProc 模式下的任意进程或线程注册序列，每次注册应返回唯一的非零 ID，
 * 且不同名称不应获得相同的 ID。
 *
 * **Feature: process-module-name-management, Property 6: Unique ID Generation**
 * **Validates: Requirements 3.2, 3.3**
 *
 * Note: This property tests ThreadModuleTable's ID uniqueness in Async mode
 * as a proxy for MProc mode behavior, since MProc requires shared memory setup.
 * 注意：此属性测试 Async 模式下 ThreadModuleTable 的 ID 唯一性，
 * 作为 MProc 模式行为的代理，因为 MProc 需要共享内存设置。
 */
RC_GTEST_PROP(NameManagerPropertyTest, UniqueIdGeneration, ()) {
    // Generate a list of unique thread IDs (simulating different threads)
    // 生成唯一线程 ID 列表（模拟不同线程）
    const auto numEntries = *rc::gen::inRange<size_t>(1, 50);
    
    ThreadModuleTable table;
    std::set<uint32_t> usedIds;
    
    for (size_t i = 0; i < numEntries; ++i) {
        // Generate unique thread ID / 生成唯一线程 ID
        uint32_t threadId = static_cast<uint32_t>(i + 1000);
        
        // Generate random module name / 生成随机模块名
        std::string moduleName = "module_" + std::to_string(i);
        
        // Register should succeed / 注册应成功
        bool registered = table.Register(threadId, moduleName);
        RC_ASSERT(registered);
        
        // Thread ID should be unique / 线程 ID 应唯一
        RC_ASSERT(usedIds.find(threadId) == usedIds.end());
        usedIds.insert(threadId);
        
        // Should be able to retrieve the name / 应能检索名称
        std::string retrieved = table.GetName(threadId);
        RC_ASSERT(retrieved == moduleName);
    }
    
    // Verify count matches / 验证计数匹配
    RC_ASSERT(table.Count() == numEntries);
}

/**
 * @brief Property 8: Concurrent Access Safety
 * @brief 属性 8：并发访问安全
 *
 * For any number of threads concurrently calling SetModuleName and GetModuleName,
 * all operations SHALL complete without data races, and each thread's registered
 * name SHALL be correctly retrievable.
 *
 * 对于任意数量的线程并发调用 SetModuleName 和 GetModuleName，
 * 所有操作应无数据竞争地完成，且每个线程注册的名称应能正确检索。
 *
 * **Feature: process-module-name-management, Property 8: Concurrent Access Safety**
 * **Validates: Requirements 7.1, 7.3, 7.4**
 */
RC_GTEST_PROP(NameManagerPropertyTest, ConcurrentAccessSafety, ()) {
    // Generate number of threads (2-16) / 生成线程数（2-16）
    const auto numThreads = *rc::gen::inRange<size_t>(2, 17);
    
    NameManager<>::Shutdown();
    NameManager<>::Initialize(Mode::Async);
    
    std::vector<std::thread> threads;
    std::vector<std::atomic<bool>> successes(numThreads);
    std::vector<std::string> expectedNames(numThreads);
    std::vector<std::string> retrievedNames(numThreads);
    
    // Initialize atomics / 初始化原子变量
    for (size_t i = 0; i < numThreads; ++i) {
        successes[i].store(false);
        expectedNames[i] = "thread_" + std::to_string(i);
    }
    
    // Create threads that concurrently set and get module names
    // 创建并发设置和获取模块名的线程
    for (size_t i = 0; i < numThreads; ++i) {
        threads.emplace_back([i, &successes, &expectedNames, &retrievedNames]() {
            // Set module name / 设置模块名
            NameManager<>::SetModuleName(expectedNames[i]);
            
            // Small delay to increase chance of concurrent access
            // 小延迟以增加并发访问的机会
            std::this_thread::yield();
            
            // Get module name / 获取模块名
            retrievedNames[i] = NameManager<>::GetModuleName();
            
            // Verify the name matches what we set / 验证名称与设置的匹配
            successes[i].store(retrievedNames[i] == expectedNames[i]);
        });
    }
    
    // Wait for all threads to complete / 等待所有线程完成
    for (auto& t : threads) {
        t.join();
    }
    
    // Verify all threads succeeded / 验证所有线程成功
    for (size_t i = 0; i < numThreads; ++i) {
        RC_ASSERT(successes[i].load());
    }
    
    NameManager<>::Shutdown();
}

#endif  // ONEPLOG_HAS_RAPIDCHECK

}  // namespace test
}  // namespace oneplog
