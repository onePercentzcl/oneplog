/**
 * @file test_shared_memory.cpp
 * @brief Unit tests for SharedMemory
 * @brief SharedMemory 的单元测试
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <gtest/gtest.h>
#ifdef ONEPLOG_HAS_RAPIDCHECK
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#endif

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

// ==============================================================================
// Property-Based Tests / 属性测试
// ==============================================================================

#ifdef ONEPLOG_HAS_RAPIDCHECK

// Note: To run with 1000+ iterations, set RC_PARAMS="max_success=1000" environment variable
// 注意：要运行 1000+ 次迭代，设置 RC_PARAMS="max_success=1000" 环境变量

/**
 * @brief Property 8: SharedMemory create and attach consistency
 * @brief 属性 8：SharedMemory 创建附加一致性
 *
 * For any SharedMemory created with name N, Connect(N) should successfully
 * attach to the same shared memory region and see the same data.
 *
 * 对于任意使用名称 N 创建的 SharedMemory，Connect(N) 应成功附加到
 * 相同的共享内存区域并看到相同的数据。
 *
 * **Feature: oneplog-refactor-and-docs, Property 8: SharedMemory 创建附加一致性**
 * **Validates: Requirements 5.10**
 */
RC_GTEST_PROP(SharedMemoryPropertyTest, CreateAttachConsistency, ()) {
    // Clean up any leftover shared memory / 清理任何残留的共享内存
    shm_unlink("/test_prop_shm");
    
    // Generate random capacity / 生成随机容量
    auto capacity = *rc::gen::inRange<size_t>(4, 64);
    
    // Create shared memory / 创建共享内存
    auto owner = SharedMemory::Create("/test_prop_shm", capacity);
    RC_ASSERT(owner != nullptr);
    RC_ASSERT(owner->IsOwner());
    
    // Connect to shared memory / 连接到共享内存
    auto client = SharedMemory::Connect("/test_prop_shm");
    RC_ASSERT(client != nullptr);
    RC_ASSERT(!client->IsOwner());
    
    // Both should see the same ring buffer capacity / 两者应看到相同的环形缓冲区容量
    RC_ASSERT(owner->GetRingBuffer()->Capacity() == client->GetRingBuffer()->Capacity());
    
    // Clean up / 清理
    owner.reset();
    client.reset();
    shm_unlink("/test_prop_shm");
}

/**
 * @brief Property: Config changes are visible across connections
 * @brief 属性：配置更改在连接间可见
 *
 * **Feature: oneplog-refactor-and-docs, Property 8: SharedMemory 创建附加一致性**
 * **Validates: Requirements 5.10**
 */
RC_GTEST_PROP(SharedMemoryPropertyTest, ConfigVisibility, ()) {
    shm_unlink("/test_prop_config");
    
    auto owner = SharedMemory::Create("/test_prop_config", 16);
    RC_ASSERT(owner != nullptr);
    
    auto client = SharedMemory::Connect("/test_prop_config");
    RC_ASSERT(client != nullptr);
    
    // Generate random level / 生成随机级别
    auto levelInt = *rc::gen::inRange(0, 6);
    Level level = static_cast<Level>(levelInt);
    
    // Owner sets level / 所有者设置级别
    owner->GetConfig()->SetLevel(level);
    
    // Client should see the change / 客户端应看到更改
    RC_ASSERT(client->GetConfig()->GetLevel() == level);
    
    owner.reset();
    client.reset();
    shm_unlink("/test_prop_config");
}

/**
 * @brief Property: Name registration is visible across connections
 * @brief 属性：名称注册在连接间可见
 *
 * **Feature: oneplog-refactor-and-docs, Property 8: SharedMemory 创建附加一致性**
 * **Validates: Requirements 5.10**
 */
RC_GTEST_PROP(SharedMemoryPropertyTest, NameRegistrationVisibility, ()) {
    shm_unlink("/test_prop_names");
    
    auto owner = SharedMemory::Create("/test_prop_names", 16);
    RC_ASSERT(owner != nullptr);
    
    auto client = SharedMemory::Connect("/test_prop_names");
    RC_ASSERT(client != nullptr);
    
    // Generate random process name (limited length) / 生成随机进程名（限制长度）
    auto nameLen = *rc::gen::inRange<size_t>(1, 20);
    std::string name(nameLen, 'p');
    
    // Owner registers process / 所有者注册进程
    uint32_t id = owner->RegisterProcess(name);
    RC_ASSERT(id > 0);
    
    // Client should see the registration / 客户端应看到注册
    const char* retrievedName = client->GetProcessName(id);
    RC_ASSERT(retrievedName != nullptr);
    RC_ASSERT(std::string(retrievedName) == name);
    
    owner.reset();
    client.reset();
    shm_unlink("/test_prop_names");
}

#endif  // ONEPLOG_HAS_RAPIDCHECK

}  // namespace test
}  // namespace oneplog
