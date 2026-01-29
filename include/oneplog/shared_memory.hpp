/**
 * @file shared_memory.hpp
 * @brief SharedMemory manager for multi-process logging
 * @brief 多进程日志的共享内存管理器
 *
 * SharedMemory contains:
 * - SharedRingBuffer: Ring buffer for log entries
 * - ProcessThreadNameTable: Process/Thread name-ID mapping
 * - SharedLoggerConfig: Logger configuration
 *
 * SharedMemory 包含：
 * - SharedRingBuffer：日志条目的环形队列
 * - ProcessThreadNameTable：进程/线程名称-ID对照表
 * - SharedLoggerConfig：Logger 配置
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include "oneplog/ring_buffer.hpp"
#include "oneplog/shared_ring_buffer.hpp"
#include "oneplog/log_entry.hpp"
#include "oneplog/common.hpp"

#include <atomic>
#include <cstring>
#include <memory>
#include <string>

#ifdef __linux__
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/eventfd.h>
#elif defined(__APPLE__)
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dispatch/dispatch.h>
#endif

namespace oneplog {

// ==============================================================================
// SharedMemoryMetadata - Shared Memory Metadata / 共享内存元数据
// ==============================================================================

/**
 * @brief Metadata for shared memory validation and layout
 * @brief 共享内存验证和布局的元数据
 */
struct alignas(kCacheLineSize) SharedMemoryMetadata {
    uint32_t magic{0};              ///< Magic number for validation / 用于验证的魔数
    uint32_t version{0};            ///< Version for compatibility / 用于兼容性的版本号
    size_t totalSize{0};            ///< Total shared memory size / 共享内存总大小
    size_t configOffset{0};         ///< Offset to SharedLoggerConfig / SharedLoggerConfig 偏移
    size_t nameTableOffset{0};      ///< Offset to ProcessThreadNameTable / ProcessThreadNameTable 偏移
    size_t ringBufferOffset{0};     ///< Offset to SharedRingBuffer / SharedRingBuffer 偏移
    size_t ringBufferCapacity{0};   ///< Ring buffer capacity / 环形队列容量
    QueueFullPolicy policy{QueueFullPolicy::DropNewest};  ///< Queue full policy / 队列满策略
    
    static constexpr uint32_t kMagic = 0x4F4E4550;  ///< "ONEP"
    static constexpr uint32_t kVersion = 1;
    
    /**
     * @brief Initialize metadata
     * @brief 初始化元数据
     */
    void Init(size_t total, size_t configOff, size_t nameTableOff, 
              size_t ringBufferOff, size_t rbCapacity, QueueFullPolicy pol) noexcept {
        magic = kMagic;
        version = kVersion;
        totalSize = total;
        configOffset = configOff;
        nameTableOffset = nameTableOff;
        ringBufferOffset = ringBufferOff;
        ringBufferCapacity = rbCapacity;
        policy = pol;
    }
    
    /**
     * @brief Check if metadata is valid
     * @brief 检查元数据是否有效
     */
    bool IsValid() const noexcept {
        return magic == kMagic && version == kVersion;
    }
};

// ==============================================================================
// SharedLoggerConfig - Shared Logger Configuration / 共享 Logger 配置
// ==============================================================================

/**
 * @brief Logger configuration in shared memory
 * @brief 共享内存中的 Logger 配置
 */
struct alignas(kCacheLineSize) SharedLoggerConfig {
    alignas(kCacheLineSize) std::atomic<Level> logLevel{Level::Info};
    alignas(kCacheLineSize) std::atomic<uint32_t> configVersion{0};
    
    /**
     * @brief Initialize configuration
     * @brief 初始化配置
     */
    void Init() noexcept {
        logLevel.store(Level::Info, std::memory_order_relaxed);
        configVersion.store(0, std::memory_order_relaxed);
    }
    
    /**
     * @brief Get current log level
     * @brief 获取当前日志级别
     */
    Level GetLevel() const noexcept {
        return logLevel.load(std::memory_order_acquire);
    }
    
    /**
     * @brief Set log level
     * @brief 设置日志级别
     */
    void SetLevel(Level level) noexcept {
        logLevel.store(level, std::memory_order_release);
        configVersion.fetch_add(1, std::memory_order_release);
    }
    
    /**
     * @brief Get configuration version
     * @brief 获取配置版本号
     */
    uint32_t GetVersion() const noexcept {
        return configVersion.load(std::memory_order_acquire);
    }
};

// ==============================================================================
// NameIdMapping - Name-ID Mapping Entry / 名称-ID 映射条目
// ==============================================================================

/**
 * @brief Name-ID mapping entry
 * @brief 名称-ID 映射条目
 */
struct NameIdMapping {
    static constexpr size_t kMaxNameLength = 31;
    
    uint32_t id{0};                      ///< ID value / ID 值
    char name[kMaxNameLength + 1]{0};    ///< Name string / 名称字符串
    
    /**
     * @brief Set mapping entry
     * @brief 设置映射条目
     */
    void Set(uint32_t newId, const char* newName) noexcept {
        id = newId;
        std::strncpy(name, newName, kMaxNameLength);
        name[kMaxNameLength] = '\0';
    }
    
    /**
     * @brief Check if entry is empty
     * @brief 检查条目是否为空
     */
    bool IsEmpty() const noexcept {
        return name[0] == '\0';
    }
};

// ==============================================================================
// ProcessThreadNameTable - Process/Thread Name-ID Table / 进程/线程名称-ID 对照表
// ==============================================================================

/**
 * @brief Process and thread name-ID mapping table
 * @brief 进程和线程名称-ID 对照表
 */
struct alignas(kCacheLineSize) ProcessThreadNameTable {
    static constexpr size_t kMaxProcesses = 64;   ///< Max process count / 最大进程数
    static constexpr size_t kMaxThreads = 256;    ///< Max thread count / 最大线程数
    
    alignas(kCacheLineSize) std::atomic<uint32_t> processCount{0};
    alignas(kCacheLineSize) std::atomic<uint32_t> threadCount{0};
    NameIdMapping processes[kMaxProcesses];
    NameIdMapping threads[kMaxThreads];
    
    /**
     * @brief Initialize name table
     * @brief 初始化名称表
     */
    void Init() noexcept {
        processCount.store(0, std::memory_order_relaxed);
        threadCount.store(0, std::memory_order_relaxed);
        for (size_t i = 0; i < kMaxProcesses; ++i) {
            processes[i] = NameIdMapping{};
        }
        for (size_t i = 0; i < kMaxThreads; ++i) {
            threads[i] = NameIdMapping{};
        }
    }
    
    /**
     * @brief Register a process name and get its ID
     * @brief 注册进程名称并获取其 ID
     */
    uint32_t RegisterProcess(const char* name) noexcept {
        uint32_t count = processCount.load(std::memory_order_acquire);
        for (uint32_t i = 0; i < count && i < kMaxProcesses; ++i) {
            if (std::strcmp(processes[i].name, name) == 0) {
                return processes[i].id;
            }
        }
        
        uint32_t newIndex = processCount.fetch_add(1, std::memory_order_acq_rel);
        if (newIndex >= kMaxProcesses) {
            processCount.fetch_sub(1, std::memory_order_relaxed);
            return 0;
        }
        
        uint32_t newId = newIndex + 1;
        processes[newIndex].Set(newId, name);
        return newId;
    }
    
    /**
     * @brief Register a thread name and get its ID
     * @brief 注册线程名称并获取其 ID
     */
    uint32_t RegisterThread(const char* name) noexcept {
        uint32_t count = threadCount.load(std::memory_order_acquire);
        for (uint32_t i = 0; i < count && i < kMaxThreads; ++i) {
            if (std::strcmp(threads[i].name, name) == 0) {
                return threads[i].id;
            }
        }
        
        uint32_t newIndex = threadCount.fetch_add(1, std::memory_order_acq_rel);
        if (newIndex >= kMaxThreads) {
            threadCount.fetch_sub(1, std::memory_order_relaxed);
            return 0;
        }
        
        uint32_t newId = newIndex + 1;
        threads[newIndex].Set(newId, name);
        return newId;
    }
    
    const char* GetProcessName(uint32_t id) const noexcept {
        if (id == 0 || id > kMaxProcesses) { return nullptr; }
        uint32_t index = id - 1;
        if (index >= processCount.load(std::memory_order_acquire)) { return nullptr; }
        return processes[index].name;
    }
    
    const char* GetThreadName(uint32_t id) const noexcept {
        if (id == 0 || id > kMaxThreads) { return nullptr; }
        uint32_t index = id - 1;
        if (index >= threadCount.load(std::memory_order_acquire)) { return nullptr; }
        return threads[index].name;
    }
    
    uint32_t GetProcessCount() const noexcept {
        return processCount.load(std::memory_order_acquire);
    }
    
    uint32_t GetThreadCount() const noexcept {
        return threadCount.load(std::memory_order_acquire);
    }
};


// ==============================================================================
// SharedMemory - Shared Memory Manager / 共享内存管理器
// ==============================================================================

/**
 * @brief Shared memory manager for multi-process logging
 * @brief 多进程日志的共享内存管理器
 *
 * @tparam EnableWFC Enable WFC support for SharedRingBuffer / 启用 SharedRingBuffer 的 WFC 支持
 *
 * Memory Layout / 内存布局:
 * +---------------------------+
 * | SharedMemoryMetadata      |  (cache-line aligned / 缓存行对齐)
 * +---------------------------+
 * | SharedLoggerConfig        |  (cache-line aligned / 缓存行对齐)
 * +---------------------------+
 * | ProcessThreadNameTable    |  (cache-line aligned / 缓存行对齐)
 * +---------------------------+
 * | SharedRingBuffer memory   |  (cache-line aligned / 缓存行对齐)
 * +---------------------------+
 */
template<bool EnableWFC = true>
class SharedMemory {
public:
    /**
     * @brief Create a new shared memory region
     * @brief 创建新的共享内存区域
     */
    static std::unique_ptr<SharedMemory> Create(
        const std::string& name,
        size_t ringBufferCapacity,
        QueueFullPolicy policy = QueueFullPolicy::DropNewest) {
        
        if (name.empty() || ringBufferCapacity == 0) {
            return nullptr;
        }
        
        size_t totalSize = CalculateRequiredSize(ringBufferCapacity);
        
        int fd = shm_open(name.c_str(), O_CREAT | O_RDWR | O_EXCL, 0666);
        if (fd < 0) {
            shm_unlink(name.c_str());
            fd = shm_open(name.c_str(), O_CREAT | O_RDWR | O_EXCL, 0666);
            if (fd < 0) {
                return nullptr;
            }
        }
        
        if (ftruncate(fd, static_cast<off_t>(totalSize)) < 0) {
            close(fd);
            shm_unlink(name.c_str());
            return nullptr;
        }
        
        void* memory = mmap(nullptr, totalSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (memory == MAP_FAILED) {
            close(fd);
            shm_unlink(name.c_str());
            return nullptr;
        }
        
        std::memset(memory, 0, totalSize);
        
        auto shm = std::unique_ptr<SharedMemory>(new SharedMemory(name, true));
        shm->m_fd = fd;
        shm->m_memory = memory;
        shm->m_size = totalSize;
        
        size_t metadataOffset = 0;
        size_t configOffset = AlignUp(sizeof(SharedMemoryMetadata), kCacheLineSize);
        size_t nameTableOffset = configOffset + AlignUp(sizeof(SharedLoggerConfig), kCacheLineSize);
        size_t ringBufferOffset = nameTableOffset + AlignUp(sizeof(ProcessThreadNameTable), kCacheLineSize);
        
        shm->m_metadata = reinterpret_cast<SharedMemoryMetadata*>(
            static_cast<uint8_t*>(memory) + metadataOffset);
        shm->m_metadata->Init(totalSize, configOffset, nameTableOffset, 
                              ringBufferOffset, ringBufferCapacity, policy);
        
        shm->m_config = reinterpret_cast<SharedLoggerConfig*>(
            static_cast<uint8_t*>(memory) + configOffset);
        shm->m_config->Init();
        
        shm->m_nameTable = reinterpret_cast<ProcessThreadNameTable*>(
            static_cast<uint8_t*>(memory) + nameTableOffset);
        shm->m_nameTable->Init();
        
        void* ringBufferMemory = static_cast<uint8_t*>(memory) + ringBufferOffset;
        size_t ringBufferSize = SharedRingBuffer<LogEntry, EnableWFC>::CalculateRequiredSize(ringBufferCapacity);
        shm->m_ringBuffer = SharedRingBuffer<LogEntry, EnableWFC>::Create(
            ringBufferMemory, ringBufferSize, ringBufferCapacity, policy);
        
        if (!shm->m_ringBuffer) {
            return nullptr;
        }
        
        return shm;
    }
    
    /**
     * @brief Connect to an existing shared memory region
     * @brief 连接到已存在的共享内存区域
     */
    static std::unique_ptr<SharedMemory> Connect(const std::string& name) {
        if (name.empty()) {
            return nullptr;
        }
        
        int fd = shm_open(name.c_str(), O_RDWR, 0666);
        if (fd < 0) {
            return nullptr;
        }
        
        struct stat st;
        if (fstat(fd, &st) < 0) {
            close(fd);
            return nullptr;
        }
        size_t totalSize = static_cast<size_t>(st.st_size);
        
        void* memory = mmap(nullptr, totalSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (memory == MAP_FAILED) {
            close(fd);
            return nullptr;
        }
        
        auto* metadata = reinterpret_cast<SharedMemoryMetadata*>(memory);
        if (!metadata->IsValid()) {
            munmap(memory, totalSize);
            close(fd);
            return nullptr;
        }
        
        auto shm = std::unique_ptr<SharedMemory>(new SharedMemory(name, false));
        shm->m_fd = fd;
        shm->m_memory = memory;
        shm->m_size = totalSize;
        shm->m_metadata = metadata;
        
        shm->m_config = reinterpret_cast<SharedLoggerConfig*>(
            static_cast<uint8_t*>(memory) + metadata->configOffset);
        shm->m_nameTable = reinterpret_cast<ProcessThreadNameTable*>(
            static_cast<uint8_t*>(memory) + metadata->nameTableOffset);
        
        void* ringBufferMemory = static_cast<uint8_t*>(memory) + metadata->ringBufferOffset;
        size_t ringBufferSize = SharedRingBuffer<LogEntry, EnableWFC>::CalculateRequiredSize(
            metadata->ringBufferCapacity);
        shm->m_ringBuffer = SharedRingBuffer<LogEntry, EnableWFC>::Connect(ringBufferMemory, ringBufferSize);
        
        if (!shm->m_ringBuffer) {
            munmap(memory, totalSize);
            close(fd);
            return nullptr;
        }
        
        return shm;
    }
    
    /**
     * @brief Calculate required shared memory size
     * @brief 计算所需的共享内存大小
     */
    static size_t CalculateRequiredSize(size_t ringBufferCapacity) {
        size_t metadataSize = AlignUp(sizeof(SharedMemoryMetadata), kCacheLineSize);
        size_t configSize = AlignUp(sizeof(SharedLoggerConfig), kCacheLineSize);
        size_t nameTableSize = AlignUp(sizeof(ProcessThreadNameTable), kCacheLineSize);
        size_t ringBufferSize = SharedRingBuffer<LogEntry, EnableWFC>::CalculateRequiredSize(ringBufferCapacity);
        
        return metadataSize + configSize + nameTableSize + ringBufferSize;
    }
    
    ~SharedMemory() {
        if (m_ringBuffer) {
            delete m_ringBuffer;
            m_ringBuffer = nullptr;
        }
        
        if (m_memory && m_size > 0) {
            munmap(m_memory, m_size);
            m_memory = nullptr;
        }
        
        if (m_fd >= 0) {
            close(m_fd);
            m_fd = -1;
        }
        
        if (m_isOwner && !m_name.empty()) {
            shm_unlink(m_name.c_str());
        }
    }
    
    // Non-copyable, non-movable
    SharedMemory(const SharedMemory&) = delete;
    SharedMemory& operator=(const SharedMemory&) = delete;
    SharedMemory(SharedMemory&&) = delete;
    SharedMemory& operator=(SharedMemory&&) = delete;
    
    // =========================================================================
    // Component Access / 组件访问
    // =========================================================================
    
    SharedLoggerConfig* GetConfig() noexcept { return m_config; }
    const SharedLoggerConfig* GetConfig() const noexcept { return m_config; }
    
    ProcessThreadNameTable* GetNameTable() noexcept { return m_nameTable; }
    const ProcessThreadNameTable* GetNameTable() const noexcept { return m_nameTable; }
    
    SharedRingBuffer<LogEntry, EnableWFC>* GetRingBuffer() noexcept { return m_ringBuffer; }
    const SharedRingBuffer<LogEntry, EnableWFC>* GetRingBuffer() const noexcept { return m_ringBuffer; }
    
    // =========================================================================
    // Name Registration / 名称注册
    // =========================================================================
    
    uint32_t RegisterProcess(const std::string& name) {
        return m_nameTable ? m_nameTable->RegisterProcess(name.c_str()) : 0;
    }
    
    uint32_t RegisterThread(const std::string& name) {
        return m_nameTable ? m_nameTable->RegisterThread(name.c_str()) : 0;
    }
    
    const char* GetProcessName(uint32_t id) const {
        return m_nameTable ? m_nameTable->GetProcessName(id) : nullptr;
    }
    
    const char* GetThreadName(uint32_t id) const {
        return m_nameTable ? m_nameTable->GetThreadName(id) : nullptr;
    }
    
    // =========================================================================
    // Status / 状态
    // =========================================================================
    
    bool IsOwner() const noexcept { return m_isOwner; }
    const std::string& Name() const noexcept { return m_name; }
    size_t Size() const noexcept { return m_size; }
    
    // =========================================================================
    // Notification / 通知机制
    // =========================================================================
    
    void NotifyConsumer() noexcept {
        if (m_ringBuffer) {
            m_ringBuffer->NotifyConsumer();
        }
    }
    
    bool WaitForData(std::chrono::microseconds pollInterval,
                     std::chrono::milliseconds pollTimeout) noexcept {
        if (m_ringBuffer) {
            return m_ringBuffer->WaitForData(pollInterval, pollTimeout);
        }
        return false;
    }

private:
    SharedMemory(const std::string& name, bool isOwner)
        : m_name(name)
        , m_isOwner(isOwner)
    {}
    
    static size_t AlignUp(size_t value, size_t alignment) noexcept {
        return (value + alignment - 1) & ~(alignment - 1);
    }
    
    std::string m_name;
    bool m_isOwner{false};
    void* m_memory{nullptr};
    size_t m_size{0};
    int m_fd{-1};
    
    SharedMemoryMetadata* m_metadata{nullptr};
    SharedLoggerConfig* m_config{nullptr};
    ProcessThreadNameTable* m_nameTable{nullptr};
    SharedRingBuffer<LogEntry, EnableWFC>* m_ringBuffer{nullptr};
};

}  // namespace oneplog
