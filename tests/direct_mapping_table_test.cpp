/**
 * @file direct_mapping_table_test.cpp
 * @brief Unit tests for DirectMappingTable
 * @brief DirectMappingTable 的单元测试
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <gtest/gtest.h>
#include <oneplog/internal/direct_mapping_table.hpp>
#include <thread>
#include <vector>
#include <atomic>

namespace oneplog {
namespace internal {
namespace test {

class DirectMappingTableTest : public ::testing::Test {
protected:
    void SetUp() override {
        table = std::make_unique<DirectMappingTable<1024, 15>>();
    }
    void TearDown() override { table.reset(); }
    std::unique_ptr<DirectMappingTable<1024, 15>> table;
};

TEST_F(DirectMappingTableTest, BasicRegisterAndLookup) {
    EXPECT_TRUE(table->Register(1, "worker1"));
    EXPECT_TRUE(table->Register(2, "worker2"));
    EXPECT_EQ(table->GetName(1), "worker1");
    EXPECT_EQ(table->GetName(2), "worker2");
    EXPECT_EQ(table->Count(), 2);
}

TEST_F(DirectMappingTableTest, DirectIndexLookup) {
    EXPECT_TRUE(table->Register(100, "tid_100"));
    EXPECT_TRUE(table->Register(500, "tid_500"));
    EXPECT_TRUE(table->Register(999, "tid_999"));
    EXPECT_EQ(table->GetName(100), "tid_100");
    EXPECT_EQ(table->GetName(500), "tid_500");
    EXPECT_EQ(table->GetName(999), "tid_999");
}

TEST_F(DirectMappingTableTest, UpdateExistingEntry) {
    EXPECT_TRUE(table->Register(1, "worker1"));
    EXPECT_EQ(table->Count(), 1);
    EXPECT_TRUE(table->Register(1, "updated"));
    EXPECT_EQ(table->GetName(1), "updated");
    EXPECT_EQ(table->Count(), 1);
}

TEST_F(DirectMappingTableTest, LookupNonExistentReturnsDefault) {
    EXPECT_TRUE(table->Register(1, "worker1"));
    EXPECT_EQ(table->GetName(999), "main");
}

TEST_F(DirectMappingTableTest, OutOfRangeTidReturnsDefault) {
    EXPECT_EQ(table->GetName(1024), "main");
    EXPECT_EQ(table->GetName(2000), "main");
    EXPECT_EQ(table->GetName(UINT32_MAX), "main");
}

TEST_F(DirectMappingTableTest, OutOfRangeTidRegistrationFails) {
    EXPECT_FALSE(table->Register(1024, "invalid"));
    EXPECT_FALSE(table->Register(2000, "invalid"));
    EXPECT_EQ(table->Count(), 0);
}

TEST_F(DirectMappingTableTest, NameTruncation) {
    std::string longName(50, 'x');
    EXPECT_TRUE(table->Register(1, longName));
    std::string_view result = table->GetName(1);
    EXPECT_EQ(result.length(), 15);
    EXPECT_EQ(result, std::string(15, 'x'));
}

TEST_F(DirectMappingTableTest, Clear) {
    EXPECT_TRUE(table->Register(1, "worker1"));
    EXPECT_TRUE(table->Register(2, "worker2"));
    EXPECT_EQ(table->Count(), 2);
    table->Clear();
    EXPECT_EQ(table->Count(), 0);
    EXPECT_EQ(table->GetName(1), "main");
    EXPECT_EQ(table->GetName(2), "main");
}

TEST_F(DirectMappingTableTest, EmptyName) {
    EXPECT_TRUE(table->Register(1, ""));
    EXPECT_EQ(table->GetName(1), "");
    EXPECT_EQ(table->Count(), 1);
}

TEST_F(DirectMappingTableTest, IsRegistered) {
    EXPECT_FALSE(table->IsRegistered(1));
    EXPECT_TRUE(table->Register(1, "worker1"));
    EXPECT_TRUE(table->IsRegistered(1));
    EXPECT_FALSE(table->IsRegistered(2));
    EXPECT_FALSE(table->IsRegistered(1024));
}

TEST_F(DirectMappingTableTest, TidZero) {
    EXPECT_TRUE(table->Register(0, "tid_zero"));
    EXPECT_EQ(table->GetName(0), "tid_zero");
    EXPECT_TRUE(table->IsRegistered(0));
}

TEST_F(DirectMappingTableTest, BoundaryTid) {
    EXPECT_TRUE(table->Register(1023, "boundary"));
    EXPECT_EQ(table->GetName(1023), "boundary");
    EXPECT_TRUE(table->IsRegistered(1023));
    EXPECT_FALSE(table->Register(1024, "invalid"));
}

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
    EXPECT_EQ(successCount.load(), std::min(numThreads * opsPerThread, size_t(1024)));
}

TEST_F(DirectMappingTableTest, ConcurrentReadWrite) {
    for (uint32_t i = 0; i < 100; ++i) {
        table->Register(i, "initial_" + std::to_string(i));
    }

    constexpr size_t numReaders = 4;
    constexpr size_t numWriters = 2;
    std::atomic<bool> stop{false};
    std::atomic<size_t> readCount{0};
    std::atomic<size_t> writeCount{0};
    std::vector<std::thread> threads;

    for (size_t r = 0; r < numReaders; ++r) {
        threads.emplace_back([this, &stop, &readCount]() {
            while (!stop.load(std::memory_order_acquire)) {
                for (uint32_t i = 0; i < 100; ++i) {
                    auto name = table->GetName(i);
                    (void)name;
                    readCount.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

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

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    stop.store(true, std::memory_order_release);

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_GT(readCount.load(), 0);
    EXPECT_GT(writeCount.load(), 0);
}

TEST(ExtendedDirectMappingTableTest, LongerNames) {
    ExtendedDirectMappingTable table;
    std::string name31(31, 'y');
    EXPECT_TRUE(table.Register(1, name31));
    EXPECT_EQ(table.GetName(1), name31);

    std::string name50(50, 'z');
    EXPECT_TRUE(table.Register(2, name50));
    EXPECT_EQ(table.GetName(2).length(), 31);
}

}  // namespace test
}  // namespace internal
}  // namespace oneplog
