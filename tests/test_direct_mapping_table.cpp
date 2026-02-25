/**
 * @file test_direct_mapping_table.cpp
 * @brief Unit tests for DirectMappingTable
 * @brief DirectMappingTable 的单元测试
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <gtest/gtest.h>
#ifdef ONEPLOG_HAS_RAPIDCHECK
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#endif

#include <oneplog/internal/direct_mapping_table.hpp>

#include <thread>
#include <vector>
#include <atomic>

namespace oneplog {
namespace internal {
namespace test {

// ==============================================================================
// DirectMappingTable Unit Tests / DirectMappingTable 单元测试
// ==============================================================================

class DirectMappingTableTest : public ::testing::Test {
protected:
    void SetUp() override {
        table = std::make_unique<DirectMappingTable<1024, 15>>();
    }

    void TearDown() override {
        table.reset();
    }

    std::unique_ptr<DirectMappingTable<1024, 15>> table;
};

/**
 * @brief Test basic registration and lookup
 * @brief 测试基本的注册和查找
 */
TEST_F(DirectMappingTableTest, BasicRegisterAndLookup) {
    EXPECT_TRUE(table->Register(1, "worker1"));
    EXPECT_TRUE(table->Register(2, "worker2"));
    
    EXPECT_EQ(table->GetName(1), "worker1");
    EXPECT_EQ(table->GetName(2), "worker2");
    EXPECT_EQ(table->Count(), 2);
}

/**
 * @brief Test O(1) lookup using TID as index
 * @brief 测试使用 TID 作为索引的 O(1) 查找
 */
TEST_F(DirectMappingTableTest, DirectIndexLookup) {
    // Register at specific TID indices / 在特定 TID 索引处注册
    EXPECT_TRUE(table->Register(100, "tid_100"));
    EXPECT_TRUE(table->Register(500, "tid_500"));
    EXPECT_TRUE(table->Register(999, "tid_999"));
    
    // Direct lookup should work / 直接查找应该工作
    EXPECT_EQ(table->GetName(100), "tid_100");
    EXPECT_EQ(table->GetName(500), "tid_500");
    EXPECT_EQ(table->GetName(999), "tid_999");
}

/**
 * @brief Test update existing entry
 * @brief 测试更新现有条目
 */
TEST_F(DirectMappingTableTest, UpdateExistingEntry) {
    EXPECT_TRUE(table->Register(1, "worker1"));
    EXPECT_EQ(table->GetName(1), "worker1");
    EXPECT_EQ(table->Count(), 1);
    
    // Update the same TID / 更新相同的 TID
    EXPECT_TRUE(table->Register(1, "updated"));
    EXPECT_EQ(table->GetName(1), "updated");
    EXPECT_EQ(table->Count(), 1);  // Count should not increase / 计数不应增加
}

/**
 * @brief Test lookup non-existent entry returns default
 * @brief 测试查找不存在的条目返回默认值
 */
TEST_F(DirectMappingTableTest, LookupNonExistentReturnsDefault) {
    EXPECT_TRUE(table->Register(1, "worker1"));
    
    // Non-existent TID should return default / 不存在的 TID 应返回默认值
    EXPECT_EQ(table->GetName(999), "main");
}

/**
 * @brief Test TID out of range returns default (Requirement 1.3)
 * @brief 测试超出范围的 TID 返回默认值（需求 1.3）
 */
TEST_F(DirectMappingTableTest, OutOfRangeTidReturnsDefault) {
    // TID >= kMaxTid should return default / TID >= kMaxTid 应返回默认值
    EXPECT_EQ(table->GetName(1024), "main");
    EXPECT_EQ(table->GetName(2000), "main");
    EXPECT_EQ(table->GetName(UINT32_MAX), "main");
}

/**
 * @brief Test registration with out of range TID fails
 * @brief 测试超出范围的 TID 注册失败
 */
TEST_F(DirectMappingTableTest, OutOfRangeTidRegistrationFails) {
    EXPECT_FALSE(table->Register(1024, "invalid"));
    EXPECT_FALSE(table->Register(2000, "invalid"));
    EXPECT_EQ(table->Count(), 0);
}

/**
 * @brief Test name truncation at kMaxNameLength
 * @brief 测试名称在 kMaxNameLength 处截断
 */
TEST_F(DirectMappingTableTest, NameTruncation) {
    std::string longName(50, 'x');  // 50 character name / 50 字符名称
    EXPECT_TRUE(table->Register(1, longName));
    
    std::string_view result = table->GetName(1);
    EXPECT_EQ(result.length(), 15);  // Should be truncated to 15 / 应截断到 15
    EXPECT_EQ(result, std::string(15, 'x'));
}

/**
 * @brief Test clear functionality
 * @brief 测试清空功能
 */
TEST_F(DirectMappingTableTest, Clear) {
    EXPECT_TRUE(table->Register(1, "worker1"));
    EXPECT_TRUE(table->Register(2, "worker2"));
    EXPECT_EQ(table->Count(), 2);
    
    table->Clear();
    
    EXPECT_EQ(table->Count(), 0);
    EXPECT_EQ(table->GetName(1), "main");
    EXPECT_EQ(table->GetName(2), "main");
}

/**
 * @brief Test empty name registration
 * @brief 测试空名称注册
 */
TEST_F(DirectMappingTableTest, EmptyName) {
    EXPECT_TRUE(table->Register(1, ""));
    EXPECT_EQ(table->GetName(1), "");
    EXPECT_EQ(table->Count(), 1);
}

/**
 * @brief Test IsRegistered method
 * @brief 测试 IsRegistered 方法
 */
TEST_F(DirectMappingTableTest, IsRegistered) {
    EXPECT_FALSE(table->IsRegistered(1));
    
    EXPECT_TRUE(table->Register(1, "worker1"));
    EXPECT_TRUE(table->IsRegistered(1));
    EXPECT_FALSE(table->IsRegistered(2));
    
    // Out of range TID / 超出范围的 TID
    EXPECT_FALSE(table->IsRegistered(1024));
}

/**
 * @brief Test TID 0 registration
 * @brief 测试 TID 0 注册
 */
TEST_F(DirectMappingTableTest, TidZero) {
    EXPECT_TRUE(table->Register(0, "tid_zero"));
    EXPECT_EQ(table->GetName(0), "tid_zero");
    EXPECT_TRUE(table->IsRegistered(0));
}

/**
 * @brief Test boundary TID (kMaxTid - 1)
 * @brief 测试边界 TID (kMaxTid - 1)
 */
TEST_F(DirectMappingTableTest, BoundaryTid) {
    EXPECT_TRUE(table->Register(1023, "boundary"));
    EXPECT_EQ(table->GetName(1023), "boundary");
    EXPECT_TRUE(table->IsRegistered(1023));
    
    // kMaxTid should fail / kMaxTid 应该失败
    EXPECT_FALSE(table->Register(1024, "invalid"));
}

// ==============================================================================
// Concurrent Access Tests / 并发访问测试
// ==============================================================================

/**
 * @brief Test concurrent registration from multiple threads
 * @brief 测试多线程并发注册
 */
TEST_F(DirectMappingTableTest, ConcurrentRegistration) {
    constexpr size_t numThreads = 8;
    constexpr size_t opsPerThread = 100;
    
    std::vector<std::thread> threads;
    std::atomic<size_t> successCount{0};
    
    for (size_t t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, t, &successCount]() {
            for (size_t i = 0; i < opsPerThread; ++i) {
                uint32_t tid = static_cast<uint32_t>(t * opsPerThread + i);
                if (tid < 1024) {
                    std::string name = "thread_" + std::to_string(t) + "_" + std::to_string(i);
                    if (table->Register(tid, name)) {
                        successCount.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // All registrations within range should succeed / 范围内的所有注册应成功
    EXPECT_EQ(successCount.load(), std::min(numThreads * opsPerThread, size_t(1024)));
}

/**
 * @brief Test concurrent read and write
 * @brief 测试并发读写
 */
TEST_F(DirectMappingTableTest, ConcurrentReadWrite) {
    // Pre-register some entries / 预注册一些条目
    for (uint32_t i = 0; i < 100; ++i) {
        table->Register(i, "initial_" + std::to_string(i));
    }
    
    constexpr size_t numReaders = 4;
    constexpr size_t numWriters = 2;
    std::atomic<bool> stop{false};
    std::atomic<size_t> readCount{0};
    std::atomic<size_t> writeCount{0};
    
    std::vector<std::thread> threads;
    
    // Reader threads / 读取线程
    for (size_t r = 0; r < numReaders; ++r) {
        threads.emplace_back([this, &stop, &readCount]() {
            while (!stop.load(std::memory_order_acquire)) {
                for (uint32_t i = 0; i < 100; ++i) {
                    auto name = table->GetName(i);
                    (void)name;  // Just read / 只读取
                    readCount.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    
    // Writer threads / 写入线程
    for (size_t w = 0; w < numWriters; ++w) {
        threads.emplace_back([this, w, &stop, &writeCount]() {
            size_t iteration = 0;
            while (!stop.load(std::memory_order_acquire)) {
                for (uint32_t i = 0; i < 100; ++i) {
                    std::string name = "writer_" + std::to_string(w) + "_" + std::to_string(iteration);
                    table->Register(i, name);
                    writeCount.fetch_add(1, std::memory_order_relaxed);
                }
                ++iteration;
            }
        });
    }
    
    // Let it run for a short time / 运行一小段时间
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.store(true, std::memory_order_release);
    
    for (auto& t : threads) {
        t.join();
    }
    
    // Verify no crashes and operations completed / 验证无崩溃且操作完成
    EXPECT_GT(readCount.load(), 0);
    EXPECT_GT(writeCount.load(), 0);
}

// ==============================================================================
// Extended DirectMappingTable Tests / 扩展 DirectMappingTable 测试
// ==============================================================================

/**
 * @brief Test ExtendedDirectMappingTable with 31-character names
 * @brief 测试 31 字符名称的 ExtendedDirectMappingTable
 */
TEST(ExtendedDirectMappingTableTest, LongerNames) {
    ExtendedDirectMappingTable table;
    
    std::string name31(31, 'y');
    EXPECT_TRUE(table.Register(1, name31));
    EXPECT_EQ(table.GetName(1), name31);
    
    std::string name50(50, 'z');
    EXPECT_TRUE(table.Register(2, name50));
    EXPECT_EQ(table.GetName(2).length(), 31);
}

// ==============================================================================
// Property-Based Tests / 属性测试
// ==============================================================================

#ifdef ONEPLOG_HAS_RAPIDCHECK

// Note: To run with 1000+ iterations, set RC_PARAMS="max_success=1000" environment variable
// 注意：要运行 1000+ 次迭代，设置 RC_PARAMS="max_success=1000" 环境变量

/**
 * @brief Property 7: Mapping table storage and lookup consistency
 * @brief 属性 7：映射表存储查找一致性
 *
 * For any registered (TID, name) pair, GetName(TID) should return the
 * registered name (possibly truncated to kMaxNameLength).
 *
 * 对于任意注册的 (TID, name) 对，GetName(TID) 应返回注册的名称
 * （可能被截断到 kMaxNameLength）。
 *
 * **Feature: oneplog-refactor-and-docs, Property 7: 映射表存储查找一致性**
 * **Validates: Requirements 5.9**
 */
RC_GTEST_PROP(DirectMappingTablePropertyTest, StorageLookupConsistency, ()) {
    DirectMappingTable<1024, 15> table;
    
    // Generate random TID within valid range / 生成有效范围内的随机 TID
    auto tid = *rc::gen::inRange<uint32_t>(0, 1024);
    
    // Generate random name / 生成随机名称
    auto name = *rc::gen::nonEmpty<std::string>();
    
    // Register / 注册
    bool registered = table.Register(tid, name);
    RC_ASSERT(registered);
    
    // Lookup / 查找
    std::string_view result = table.GetName(tid);
    
    // Result should match (truncated if necessary) / 结果应匹配（必要时截断）
    if (name.length() <= 15) {
        RC_ASSERT(result == name);
    } else {
        RC_ASSERT(result == name.substr(0, 15));
    }
}

/**
 * @brief Property: Out of range TID returns default
 * @brief 属性：超出范围的 TID 返回默认值
 *
 * **Feature: oneplog-refactor-and-docs, Property 7: 映射表存储查找一致性**
 * **Validates: Requirements 5.9**
 */
RC_GTEST_PROP(DirectMappingTablePropertyTest, OutOfRangeReturnsDefault, ()) {
    DirectMappingTable<1024, 15> table;
    
    // Generate TID outside valid range / 生成有效范围外的 TID
    auto tid = *rc::gen::inRange<uint32_t>(1024, UINT32_MAX);
    
    // Lookup should return default / 查找应返回默认值
    std::string_view result = table.GetName(tid);
    RC_ASSERT(result == "main");
}

/**
 * @brief Property: Update preserves count
 * @brief 属性：更新保持计数不变
 *
 * **Feature: oneplog-refactor-and-docs, Property 7: 映射表存储查找一致性**
 * **Validates: Requirements 5.9**
 */
RC_GTEST_PROP(DirectMappingTablePropertyTest, UpdatePreservesCount, ()) {
    DirectMappingTable<1024, 15> table;
    
    auto tid = *rc::gen::inRange<uint32_t>(0, 1024);
    auto name1 = *rc::gen::nonEmpty<std::string>();
    auto name2 = *rc::gen::nonEmpty<std::string>();
    
    // First registration / 第一次注册
    table.Register(tid, name1);
    size_t countAfterFirst = table.Count();
    
    // Update same TID / 更新相同 TID
    table.Register(tid, name2);
    size_t countAfterUpdate = table.Count();
    
    // Count should not change / 计数不应改变
    RC_ASSERT(countAfterFirst == countAfterUpdate);
    
    // Should return updated name / 应返回更新后的名称
    std::string_view result = table.GetName(tid);
    if (name2.length() <= 15) {
        RC_ASSERT(result == name2);
    } else {
        RC_ASSERT(result == name2.substr(0, 15));
    }
}

#endif  // ONEPLOG_HAS_RAPIDCHECK

}  // namespace test
}  // namespace internal
}  // namespace oneplog
