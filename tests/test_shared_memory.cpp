/**
 * @file test_shared_memory.cpp
 * @brief Unit tests for SharedMemory
 * @brief SharedMemory 的单元测试
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <gtest/gtest.h>

#include <atomic>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#include <oneplog/internal/shared_memory.hpp>

namespace oneplog {
namespace test {

// Type alias for SharedMemory with default template parameter
// 使用默认模板参数的 SharedMemory 类型别名
using SharedMemory = oneplog::SharedMemory<true>;

// ==============================================================================
// Test Fixture / 测试夹具
// ==============================================================================

class SharedMemoryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clean up any leftover shared memory from previous tests
        shm_unlink("/test_shm");
        shm_unlink("/test_shm_connect");
    }
    
    void TearDown() override {
        shm_unlink("/test_shm");
        shm_unlink("/test_shm_connect");
    }
};

// ==============================================================================
// SharedMemoryMetadata Tests / 元数据测试
// ==============================================================================

TEST_F(SharedMemoryTest, MetadataInit) {
    SharedMemoryMetadata metadata;
    metadata.Init(4096, 64, 128, 256, 16, QueueFullPolicy::DropNewest);
    
    EXPECT_TRUE(metadata.IsValid());
    EXPECT_EQ(metadata.magic, SharedMemoryMetadata::kMagic);
    EXPECT_EQ(metadata.version, SharedMemoryMetadata::kVersion);
    EXPECT_EQ(metadata.totalSize, 4096);
    EXPECT_EQ(metadata.configOffset, 64);
    EXPECT_EQ(metadata.nameTableOffset, 128);
    EXPECT_EQ(metadata.ringBufferOffset, 256);
    EXPECT_EQ(metadata.ringBufferCapacity, 16);
    EXPECT_EQ(metadata.policy, QueueFullPolicy::DropNewest);
}

TEST_F(SharedMemoryTest, MetadataInvalid) {
    SharedMemoryMetadata metadata;
    EXPECT_FALSE(metadata.IsValid());
    
    metadata.magic = 0x12345678;
    EXPECT_FALSE(metadata.IsValid());
}

// ==============================================================================
// SharedLoggerConfig Tests / 配置测试
// ==============================================================================

TEST_F(SharedMemoryTest, ConfigInit) {
    SharedLoggerConfig config;
    config.Init();
    
    EXPECT_EQ(config.GetLevel(), Level::Info);
    EXPECT_EQ(config.GetVersion(), 0);
}

TEST_F(SharedMemoryTest, ConfigSetLevel) {
    SharedLoggerConfig config;
    config.Init();
    
    config.SetLevel(Level::Debug);
    EXPECT_EQ(config.GetLevel(), Level::Debug);
    EXPECT_EQ(config.GetVersion(), 1);
    
    config.SetLevel(Level::Error);
    EXPECT_EQ(config.GetLevel(), Level::Error);
    EXPECT_EQ(config.GetVersion(), 2);
}

// ==============================================================================
// ProcessThreadNameTable Tests / 名称表测试
// ==============================================================================

TEST_F(SharedMemoryTest, NameTableInit) {
    ProcessThreadNameTable table;
    table.Init();
    
    EXPECT_EQ(table.GetProcessCount(), 0);
    EXPECT_EQ(table.GetThreadCount(), 0);
}

TEST_F(SharedMemoryTest, RegisterProcess) {
    ProcessThreadNameTable table;
    table.Init();
    
    uint32_t id1 = table.RegisterProcess("process1");
    EXPECT_EQ(id1, 1);
    EXPECT_EQ(table.GetProcessCount(), 1);
    
    uint32_t id2 = table.RegisterProcess("process2");
    EXPECT_EQ(id2, 2);
    EXPECT_EQ(table.GetProcessCount(), 2);
    
    // Duplicate registration should return same ID
    uint32_t id1_dup = table.RegisterProcess("process1");
    EXPECT_EQ(id1_dup, 1);
    EXPECT_EQ(table.GetProcessCount(), 2);
}

TEST_F(SharedMemoryTest, RegisterThread) {
    ProcessThreadNameTable table;
    table.Init();
    
    uint32_t id1 = table.RegisterThread("thread1");
    EXPECT_EQ(id1, 1);
    EXPECT_EQ(table.GetThreadCount(), 1);
    
    uint32_t id2 = table.RegisterThread("thread2");
    EXPECT_EQ(id2, 2);
    EXPECT_EQ(table.GetThreadCount(), 2);
    
    // Duplicate registration should return same ID
    uint32_t id1_dup = table.RegisterThread("thread1");
    EXPECT_EQ(id1_dup, 1);
    EXPECT_EQ(table.GetThreadCount(), 2);
}

TEST_F(SharedMemoryTest, GetProcessName) {
    ProcessThreadNameTable table;
    table.Init();
    
    table.RegisterProcess("my_process");
    
    const char* name = table.GetProcessName(1);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "my_process");
    
    // Invalid ID
    EXPECT_EQ(table.GetProcessName(0), nullptr);
    EXPECT_EQ(table.GetProcessName(100), nullptr);
}

TEST_F(SharedMemoryTest, GetThreadName) {
    ProcessThreadNameTable table;
    table.Init();
    
    table.RegisterThread("my_thread");
    
    const char* name = table.GetThreadName(1);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "my_thread");
    
    // Invalid ID
    EXPECT_EQ(table.GetThreadName(0), nullptr);
    EXPECT_EQ(table.GetThreadName(100), nullptr);
}

TEST_F(SharedMemoryTest, NameTruncation) {
    ProcessThreadNameTable table;
    table.Init();
    
    // Name longer than kMaxNameLength should be truncated
    std::string longName(64, 'x');
    table.RegisterProcess(longName.c_str());
    
    const char* name = table.GetProcessName(1);
    ASSERT_NE(name, nullptr);
    EXPECT_EQ(std::strlen(name), NameIdMapping::kMaxNameLength);
}

// ==============================================================================
// SharedMemory Create/Connect Tests / 创建/连接测试
// ==============================================================================

TEST_F(SharedMemoryTest, CalculateRequiredSize) {
    size_t size16 = SharedMemory::CalculateRequiredSize(16);
    size_t size32 = SharedMemory::CalculateRequiredSize(32);
    
    EXPECT_GT(size16, 0);
    EXPECT_GT(size32, size16);
}

TEST_F(SharedMemoryTest, CreateBasic) {
    auto shm = SharedMemory::Create("/test_shm", 16);
    ASSERT_NE(shm, nullptr);
    
    EXPECT_TRUE(shm->IsOwner());
    EXPECT_EQ(shm->Name(), "/test_shm");
    EXPECT_GT(shm->Size(), 0);
    
    EXPECT_NE(shm->GetConfig(), nullptr);
    EXPECT_NE(shm->GetNameTable(), nullptr);
    EXPECT_NE(shm->GetRingBuffer(), nullptr);
}

TEST_F(SharedMemoryTest, CreateInvalidParams) {
    // Empty name
    auto shm1 = SharedMemory::Create("", 16);
    EXPECT_EQ(shm1, nullptr);
    
    // Zero capacity
    auto shm2 = SharedMemory::Create("/test_shm", 0);
    EXPECT_EQ(shm2, nullptr);
}

TEST_F(SharedMemoryTest, ConnectBasic) {
    // Create first
    auto owner = SharedMemory::Create("/test_shm_connect", 16);
    ASSERT_NE(owner, nullptr);
    
    // Connect
    auto client = SharedMemory::Connect("/test_shm_connect");
    ASSERT_NE(client, nullptr);
    
    EXPECT_FALSE(client->IsOwner());
    EXPECT_EQ(client->Name(), "/test_shm_connect");
    
    EXPECT_NE(client->GetConfig(), nullptr);
    EXPECT_NE(client->GetNameTable(), nullptr);
    EXPECT_NE(client->GetRingBuffer(), nullptr);
}

TEST_F(SharedMemoryTest, ConnectNonExistent) {
    auto shm = SharedMemory::Connect("/nonexistent_shm_12345");
    EXPECT_EQ(shm, nullptr);
}

// ==============================================================================
// SharedMemory Data Sharing Tests / 数据共享测试
// ==============================================================================

TEST_F(SharedMemoryTest, ConfigSharing) {
    auto owner = SharedMemory::Create("/test_shm", 16);
    ASSERT_NE(owner, nullptr);
    
    auto client = SharedMemory::Connect("/test_shm");
    ASSERT_NE(client, nullptr);
    
    // Owner sets level
    owner->GetConfig()->SetLevel(Level::Debug);
    
    // Client should see the change
    EXPECT_EQ(client->GetConfig()->GetLevel(), Level::Debug);
}

TEST_F(SharedMemoryTest, NameTableSharing) {
    auto owner = SharedMemory::Create("/test_shm", 16);
    ASSERT_NE(owner, nullptr);
    
    auto client = SharedMemory::Connect("/test_shm");
    ASSERT_NE(client, nullptr);
    
    // Owner registers process
    uint32_t id = owner->RegisterProcess("shared_process");
    EXPECT_EQ(id, 1);
    
    // Client should see the registration
    const char* name = client->GetProcessName(1);
    ASSERT_NE(name, nullptr);
    EXPECT_STREQ(name, "shared_process");
}

TEST_F(SharedMemoryTest, RingBufferSharing) {
    auto owner = SharedMemory::Create("/test_shm", 16);
    ASSERT_NE(owner, nullptr);
    
    auto client = SharedMemory::Connect("/test_shm");
    ASSERT_NE(client, nullptr);
    
    // Owner pushes data
    LogEntry entry;
    entry.timestamp = 12345;
    entry.level = Level::Info;
    EXPECT_TRUE(owner->GetRingBuffer()->TryPush(std::move(entry)));
    
    // Client should be able to pop
    LogEntry popped;
    EXPECT_TRUE(client->GetRingBuffer()->TryPop(popped));
    EXPECT_EQ(popped.timestamp, 12345);
    EXPECT_EQ(popped.level, Level::Info);
}

// ==============================================================================
// Multi-threaded Tests / 多线程测试
// ==============================================================================

TEST_F(SharedMemoryTest, ConcurrentProcessRegistration) {
    auto shm = SharedMemory::Create("/test_shm", 16);
    ASSERT_NE(shm, nullptr);
    
    constexpr int kThreadCount = 4;
    constexpr int kRegistrationsPerThread = 10;
    
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};
    
    for (int t = 0; t < kThreadCount; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < kRegistrationsPerThread; ++i) {
                std::string name = "proc_" + std::to_string(t) + "_" + std::to_string(i);
                uint32_t id = shm->RegisterProcess(name);
                if (id > 0) {
                    successCount.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    // All registrations should succeed (total < kMaxProcesses)
    EXPECT_EQ(successCount.load(), kThreadCount * kRegistrationsPerThread);
}

TEST_F(SharedMemoryTest, ConcurrentRingBufferAccess) {
    auto owner = SharedMemory::Create("/test_shm", 64);
    ASSERT_NE(owner, nullptr);
    
    auto client = SharedMemory::Connect("/test_shm");
    ASSERT_NE(client, nullptr);
    
    constexpr int kItemCount = 100;
    std::atomic<int> producedCount{0};
    std::atomic<int> consumedCount{0};
    std::atomic<bool> done{false};
    
    // Producer thread (owner)
    std::thread producer([&]() {
        for (int i = 0; i < kItemCount; ++i) {
            LogEntry entry;
            entry.timestamp = static_cast<uint64_t>(i);
            entry.level = Level::Info;
            
            while (!owner->GetRingBuffer()->TryPush(std::move(entry))) {
                std::this_thread::yield();
            }
            producedCount.fetch_add(1, std::memory_order_relaxed);
        }
        done.store(true, std::memory_order_release);
    });
    
    // Consumer thread (client)
    std::thread consumer([&]() {
        while (!done.load(std::memory_order_acquire) || 
               !client->GetRingBuffer()->IsEmpty()) {
            LogEntry entry;
            if (client->GetRingBuffer()->TryPop(entry)) {
                consumedCount.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::this_thread::yield();
            }
        }
    });
    
    producer.join();
    consumer.join();
    
    EXPECT_EQ(producedCount.load(), kItemCount);
    EXPECT_EQ(consumedCount.load(), kItemCount);
}

}  // namespace test
}  // namespace oneplog
