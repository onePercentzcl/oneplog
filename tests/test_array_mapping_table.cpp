/**
 * @file test_array_mapping_table.cpp
 * @brief Unit tests for ArrayMappingTable
 * @brief ArrayMappingTable 的单元测试
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <gtest/gtest.h>

#include <oneplog/internal/array_mapping_table.hpp>

#include <thread>
#include <vector>
#include <atomic>

namespace oneplog {
namespace internal {
namespace test {

// ==============================================================================
// ArrayMappingTable Unit Tests / ArrayMappingTable 单元测试
// ==============================================================================

class ArrayMappingTableTest : public ::testing::Test {
protected:
    void SetUp() override {
        table = std::make_unique<ArrayMappingTable<64, 15>>();
    }

    void TearDown() override {
        table.reset();
    }

    std::unique_ptr<ArrayMappingTable<64, 15>> table;
};

/**
 * @brief Test basic registration and lookup
 * @brief 测试基本的注册和查找
 *
 * _Requirements: 2.1, 2.2, 2.3_
 */
TEST_F(ArrayMappingTableTest, BasicRegisterAndLookup) {
    EXPECT_TRUE(table->Register(1, "worker1"));
    EXPECT_TRUE(table->Register(2, "worker2"));
    
    EXPECT_EQ(table->GetName(1), "worker1");
    EXPECT_EQ(table->GetName(2), "worker2");
    EXPECT_EQ(table->Count(), 2);
}

/**
 * @brief Test linear search lookup with large TIDs
 * @brief 测试大 TID 的线性搜索查找
 *
 * _Requirements: 2.1, 2.2_
 */
TEST_F(ArrayMappingTableTest, LargeTidLookup) {
    // Register with large TID values (typical on macOS/Windows)
    // 使用大 TID 值注册（macOS/Windows 上的典型值）
    EXPECT_TRUE(table->Register(100000, "tid_100000"));
    EXPECT_TRUE(table->Register(500000, "tid_500000"));
    EXPECT_TRUE(table->Register(999999, "tid_999999"));
    
    // Linear search should find them / 线性搜索应该找到它们
    EXPECT_EQ(table->GetName(100000), "tid_100000");
    EXPECT_EQ(table->GetName(500000), "tid_500000");
    EXPECT_EQ(table->GetName(999999), "tid_999999");
}

/**
 * @brief Test update existing entry in place (Requirement 2.4)
 * @brief 测试原地更新现有条目（需求 2.4）
 *
 * _Requirements: 2.4_
 */
TEST_F(ArrayMappingTableTest, UpdateExistingEntryInPlace) {
    EXPECT_TRUE(table->Register(1, "worker1"));
    EXPECT_EQ(table->GetName(1), "worker1");
    EXPECT_EQ(table->Count(), 1);
    
    // Update the same TID / 更新相同的 TID
    EXPECT_TRUE(table->Register(1, "updated"));
    EXPECT_EQ(table->GetName(1), "updated");
    EXPECT_EQ(table->Count(), 1);  // Count should not increase / 计数不应增加
    
    // Update again / 再次更新
    EXPECT_TRUE(table->Register(1, "final"));
    EXPECT_EQ(table->GetName(1), "final");
    EXPECT_EQ(table->Count(), 1);  // Count still 1 / 计数仍为 1
}

/**
 * @brief Test lookup non-existent entry returns default
 * @brief 测试查找不存在的条目返回默认值
 */
TEST_F(ArrayMappingTableTest, LookupNonExistentReturnsDefault) {
    EXPECT_TRUE(table->Register(1, "worker1"));
    
    // Non-existent TID should return default / 不存在的 TID 应返回默认值
    EXPECT_EQ(table->GetName(999), "main");
    EXPECT_EQ(table->GetName(0), "main");
}

/**
 * @brief Test table capacity limit (Requirement 2.5)
 * @brief 测试表容量限制（需求 2.5）
 *
 * _Requirements: 2.5_
 */
TEST_F(ArrayMappingTableTest, TableCapacityLimit) {
    // Fill the table / 填满表
    for (uint32_t i = 0; i < 64; ++i) {
        EXPECT_TRUE(table->Register(i + 1000, "entry_" + std::to_string(i)));
    }
    EXPECT_EQ(table->Count(), 64);
    EXPECT_TRUE(table->IsFull());
    
    // Next registration should fail / 下一次注册应失败
    EXPECT_FALSE(table->Register(9999, "overflow"));
    EXPECT_EQ(table->Count(), 64);
    
    // But updating existing entry should still work / 但更新现有条目应仍然有效
    EXPECT_TRUE(table->Register(1000, "updated_entry"));
    EXPECT_EQ(table->GetName(1000), "updated_entry");
    EXPECT_EQ(table->Count(), 64);
}

/**
 * @brief Test name truncation at kMaxNameLength
 * @brief 测试名称在 kMaxNameLength 处截断
 */
TEST_F(ArrayMappingTableTest, NameTruncation) {
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
TEST_F(ArrayMappingTableTest, Clear) {
    EXPECT_TRUE(table->Register(1, "worker1"));
    EXPECT_TRUE(table->Register(2, "worker2"));
    EXPECT_EQ(table->Count(), 2);
    
    table->Clear();
    
    EXPECT_EQ(table->Count(), 0);
    EXPECT_EQ(table->GetName(1), "main");
    EXPECT_EQ(table->GetName(2), "main");
    EXPECT_FALSE(table->IsFull());
}

/**
 * @brief Test empty name registration
 * @brief 测试空名称注册
 */
TEST_F(ArrayMappingTableTest, EmptyName) {
    EXPECT_TRUE(table->Register(1, ""));
    EXPECT_EQ(table->GetName(1), "");
    EXPECT_EQ(table->Count(), 1);
}

/**
 * @brief Test IsRegistered method
 * @brief 测试 IsRegistered 方法
 */
TEST_F(ArrayMappingTableTest, IsRegistered) {
    EXPECT_FALSE(table->IsRegistered(1));
    
    EXPECT_TRUE(table->Register(1, "worker1"));
    EXPECT_TRUE(table->IsRegistered(1));
    EXPECT_FALSE(table->IsRegistered(2));
}

/**
 * @brief Test TID 0 registration
 * @brief 测试 TID 0 注册
 */
TEST_F(ArrayMappingTableTest, TidZero) {
    EXPECT_TRUE(table->Register(0, "tid_zero"));
    EXPECT_EQ(table->GetName(0), "tid_zero");
    EXPECT_TRUE(table->IsRegistered(0));
}

/**
 * @brief Test maximum TID value (UINT32_MAX)
 * @brief 测试最大 TID 值 (UINT32_MAX)
 */
TEST_F(ArrayMappingTableTest, MaxTidValue) {
    EXPECT_TRUE(table->Register(UINT32_MAX, "max_tid"));
    EXPECT_EQ(table->GetName(UINT32_MAX), "max_tid");
    EXPECT_TRUE(table->IsRegistered(UINT32_MAX));
}

/**
 * @brief Test IsFull method
 * @brief 测试 IsFull 方法
 */
TEST_F(ArrayMappingTableTest, IsFull) {
    EXPECT_FALSE(table->IsFull());
    
    // Fill the table / 填满表
    for (uint32_t i = 0; i < 64; ++i) {
        EXPECT_TRUE(table->Register(i, "entry"));
    }
    
    EXPECT_TRUE(table->IsFull());
    
    // Clear and check again / 清空后再检查
    table->Clear();
    EXPECT_FALSE(table->IsFull());
}

// ==============================================================================
// Concurrent Access Tests / 并发访问测试
// ==============================================================================

/**
 * @brief Test concurrent registration from multiple threads
 * @brief 测试多线程并发注册
 *
 * _Requirements: 6.1_
 */
TEST_F(ArrayMappingTableTest, ConcurrentRegistration) {
    constexpr size_t numThreads = 8;
    constexpr size_t opsPerThread = 8;  // 8 * 8 = 64 = kMaxEntries
    
    std::vector<std::thread> threads;
    std::atomic<size_t> successCount{0};
    
    for (size_t t = 0; t < numThreads; ++t) {
        threads.emplace_back([this, t, &successCount]() {
            for (size_t i = 0; i < opsPerThread; ++i) {
                uint32_t tid = static_cast<uint32_t>(t * 1000 + i);  // Unique TIDs
                std::string name = "thread_" + std::to_string(t) + "_" + std::to_string(i);
                if (table->Register(tid, name)) {
                    successCount.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // All registrations should succeed (64 total, table capacity is 64)
    // 所有注册应成功（共 64 个，表容量为 64）
    EXPECT_EQ(successCount.load(), numThreads * opsPerThread);
    EXPECT_EQ(table->Count(), numThreads * opsPerThread);
}

/**
 * @brief Test concurrent read and write
 * @brief 测试并发读写
 *
 * _Requirements: 6.1, 6.3_
 */
TEST_F(ArrayMappingTableTest, ConcurrentReadWrite) {
    // Pre-register some entries / 预注册一些条目
    for (uint32_t i = 0; i < 32; ++i) {
        table->Register(i + 1000, "initial_" + std::to_string(i));
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
                for (uint32_t i = 0; i < 32; ++i) {
                    auto name = table->GetName(i + 1000);
                    (void)name;  // Just read / 只读取
                    readCount.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    
    // Writer threads (update existing entries) / 写入线程（更新现有条目）
    for (size_t w = 0; w < numWriters; ++w) {
        threads.emplace_back([this, w, &stop, &writeCount]() {
            size_t iteration = 0;
            while (!stop.load(std::memory_order_acquire)) {
                for (uint32_t i = 0; i < 32; ++i) {
                    std::string name = "writer_" + std::to_string(w) + "_" + std::to_string(iteration);
                    table->Register(i + 1000, name);
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
// Extended ArrayMappingTable Tests / 扩展 ArrayMappingTable 测试
// ==============================================================================

/**
 * @brief Test ExtendedArrayMappingTable with 31-character names
 * @brief 测试 31 字符名称的 ExtendedArrayMappingTable
 */
TEST(ExtendedArrayMappingTableTest, LongerNames) {
    ExtendedArrayMappingTable table;
    
    std::string name31(31, 'y');
    EXPECT_TRUE(table.Register(1, name31));
    EXPECT_EQ(table.GetName(1), name31);
    
    std::string name50(50, 'z');
    EXPECT_TRUE(table.Register(2, name50));
    EXPECT_EQ(table.GetName(2).length(), 31);
}

/**
 * @brief Test DefaultArrayMappingTable type alias
 * @brief 测试 DefaultArrayMappingTable 类型别名
 */
TEST(DefaultArrayMappingTableTest, TypeAlias) {
    DefaultArrayMappingTable table;
    
    EXPECT_EQ(table.kMaxEntries, 256);
    EXPECT_EQ(table.kMaxNameLength, 15);
    
    EXPECT_TRUE(table.Register(12345, "test_entry"));
    EXPECT_EQ(table.GetName(12345), "test_entry");
}

}  // namespace test
}  // namespace internal
}  // namespace oneplog


// ==============================================================================
// Platform Lookup Table Tests / 平台查找表测试
// ==============================================================================

#include <oneplog/internal/platform_lookup_table.hpp>

namespace platform_test {

/**
 * @brief Test PlatformLookupTable type alias
 * @brief 测试 PlatformLookupTable 类型别名
 *
 * _Requirements: 5.1, 5.2, 5.3, 5.4_
 */
TEST(PlatformLookupTableTest, TypeAlias) {
    // PlatformLookupTable should be usable / PlatformLookupTable 应该可用
    oneplog::internal::PlatformLookupTable table;
    
    EXPECT_TRUE(table.Register(1, "test_entry"));
    EXPECT_EQ(table.GetName(1), "test_entry");
    EXPECT_EQ(table.Count(), 1);
    
    // Default name for non-existent TID / 不存在 TID 的默认名称
    EXPECT_EQ(table.GetName(9999), "main");
}

/**
 * @brief Test ExtendedPlatformLookupTable type alias
 * @brief 测试 ExtendedPlatformLookupTable 类型别名
 */
TEST(PlatformLookupTableTest, ExtendedTypeAlias) {
    oneplog::internal::ExtendedPlatformLookupTable table;
    
    // Should support 31-character names / 应支持 31 字符名称
    std::string name31(31, 'x');
    EXPECT_TRUE(table.Register(1, name31));
    EXPECT_EQ(table.GetName(1), name31);
}

/**
 * @brief Test PlatformLookupTableSelector template
 * @brief 测试 PlatformLookupTableSelector 模板
 */
TEST(PlatformLookupTableTest, SelectorTemplate) {
    // Test with custom name length / 测试自定义名称长度
    using CustomTable = typename oneplog::internal::PlatformLookupTableSelector<20>::Type;
    CustomTable table;
    
    std::string name20(20, 'y');
    EXPECT_TRUE(table.Register(1, name20));
    EXPECT_EQ(table.GetName(1), name20);
    
    // Longer name should be truncated / 更长的名称应被截断
    std::string name30(30, 'z');
    EXPECT_TRUE(table.Register(2, name30));
    EXPECT_EQ(table.GetName(2).length(), 20);
}

/**
 * @brief Test compile-time platform constants
 * @brief 测试编译期平台常量
 *
 * _Requirements: 5.3, 9.2_
 */
TEST(PlatformLookupTableTest, PlatformConstants) {
    // kIsLinuxPlatform should be a compile-time constant
    // kIsLinuxPlatform 应该是编译期常量
    constexpr bool isLinux = oneplog::internal::kIsLinuxPlatform;
    
#ifdef __linux__
    EXPECT_TRUE(isLinux);
    EXPECT_STREQ(oneplog::internal::GetPlatformName(), "Linux");
    EXPECT_STREQ(oneplog::internal::GetLookupTableTypeName(), "DirectMappingTable");
    EXPECT_STREQ(oneplog::internal::GetLookupComplexity(), "O(1)");
#else
    EXPECT_FALSE(isLinux);
    EXPECT_STREQ(oneplog::internal::GetPlatformName(), "Non-Linux");
    EXPECT_STREQ(oneplog::internal::GetLookupTableTypeName(), "ArrayMappingTable");
    EXPECT_STREQ(oneplog::internal::GetLookupComplexity(), "O(n)");
#endif
}

/**
 * @brief Test platform lookup table basic operations
 * @brief 测试平台查找表基本操作
 */
TEST(PlatformLookupTableTest, BasicOperations) {
    oneplog::internal::PlatformLookupTable table;
    
    // Register multiple entries / 注册多个条目
    EXPECT_TRUE(table.Register(100, "entry_100"));
    EXPECT_TRUE(table.Register(200, "entry_200"));
    EXPECT_TRUE(table.Register(300, "entry_300"));
    
    // Lookup / 查找
    EXPECT_EQ(table.GetName(100), "entry_100");
    EXPECT_EQ(table.GetName(200), "entry_200");
    EXPECT_EQ(table.GetName(300), "entry_300");
    EXPECT_EQ(table.Count(), 3);
    
    // Update existing / 更新现有条目
    EXPECT_TRUE(table.Register(100, "updated_100"));
    EXPECT_EQ(table.GetName(100), "updated_100");
    EXPECT_EQ(table.Count(), 3);  // Count unchanged / 计数不变
    
    // Clear / 清空
    table.Clear();
    EXPECT_EQ(table.Count(), 0);
    EXPECT_EQ(table.GetName(100), "main");
}

}  // namespace platform_test
