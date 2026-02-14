/**
 * @file shared_memory.hpp
 * @brief SharedMemory manager for multi-process logging
 * @brief 多进程日志的共享内存管理器
 *
 * This file contains:
 * - SharedRingBuffer: Ring buffer in shared memory
 * - SharedMemory: Shared memory manager
 *
 * 本文件包含：
 * - SharedRingBuffer：共享内存中的环形队列
 * - SharedMemory：共享内存管理器
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include "oneplog/internal/heap_memory.hpp"
#include "oneplog/internal/log_entry.hpp"
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
namespace internal {

// ==============================================================================
// SharedRingBuffer - Ring buffer in shared memory / 共享内存中的环形队列
// ==============================================================================

template<typename T, bool EnableWFC = true, bool EnableShadowTail = true>
class SharedRingBuffer : public RingBufferBase<T, EnableWFC, EnableShadowTail> {
public:
    using Base = RingBufferBase<T, EnableWFC, EnableShadowTail>;

    static size_t CalculateRequiredSize(size_t capacity) noexcept {
        size_t headerSize = AlignUp(sizeof(RingBufferHeader), kCacheLineSize);
        size_t slotStatusSize = AlignUp(sizeof(SlotStatus) * capacity, kCacheLineSize);
        size_t bufferSize = AlignUp(sizeof(T) * capacity, kCacheLineSize);
        return headerSize + slotStatusSize + bufferSize;
    }

    static SharedRingBuffer* Create(void* memory, size_t memorySize, size_t capacity,
                                    QueueFullPolicy policy = QueueFullPolicy::DropNewest) {
        if (!memory || capacity == 0) { return nullptr; }
        
        size_t requiredSize = CalculateRequiredSize(capacity);
        if (memorySize < requiredSize) { return nullptr; }
        
        size_t headerOffset = 0;
        size_t slotStatusOffset = AlignUp(sizeof(RingBufferHeader), kCacheLineSize);
        size_t bufferOffset = slotStatusOffset + AlignUp(sizeof(SlotStatus) * capacity, kCacheLineSize);
        
        auto* header = reinterpret_cast<RingBufferHeader*>(static_cast<uint8_t*>(memory) + headerOffset);
        auto* slotStatus = reinterpret_cast<SlotStatus*>(static_cast<uint8_t*>(memory) + slotStatusOffset);
        auto* buffer = reinterpret_cast<T*>(static_cast<uint8_t*>(memory) + bufferOffset);
        
        auto* rb = new SharedRingBuffer(memory, memorySize, true);
        rb->InitWithExternalMemory(header, buffer, slotStatus, true, capacity, policy);
        rb->InitNotification();
        
        for (size_t i = 0; i < capacity; ++i) { new (&buffer[i]) T(); }
        
        return rb;
    }

    static SharedRingBuffer* Connect(void* memory, size_t memorySize) {
        if (!memory) { return nullptr; }
        
        auto* header = reinterpret_cast<RingBufferHeader*>(memory);
        if (!header->IsValid()) { return nullptr; }
        
        size_t capacity = header->capacity;
        size_t requiredSize = CalculateRequiredSize(capacity);
        if (memorySize < requiredSize) { return nullptr; }
        
        size_t slotStatusOffset = AlignUp(sizeof(RingBufferHeader), kCacheLineSize);
        size_t bufferOffset = slotStatusOffset + AlignUp(sizeof(SlotStatus) * capacity, kCacheLineSize);
        
        auto* slotStatus = reinterpret_cast<SlotStatus*>(static_cast<uint8_t*>(memory) + slotStatusOffset);
        auto* buffer = reinterpret_cast<T*>(static_cast<uint8_t*>(memory) + bufferOffset);
        
        auto* rb = new SharedRingBuffer(memory, memorySize, false);
        rb->InitWithExternalMemory(header, buffer, slotStatus, false, capacity, header->policy);
        rb->InitNotification();
        
        return rb;
    }

    ~SharedRingBuffer() override {
        if (m_isOwner && Base::m_buffer && Base::m_header) {
            size_t capacity = Base::m_header->capacity;
            for (size_t i = 0; i < capacity; ++i) { Base::m_buffer[i].~T(); }
        }
    }

    SharedRingBuffer(const SharedRingBuffer&) = delete;
    SharedRingBuffer& operator=(const SharedRingBuffer&) = delete;
    SharedRingBuffer(SharedRingBuffer&&) = delete;
    SharedRingBuffer& operator=(SharedRingBuffer&&) = delete;

    bool IsOwner() const noexcept { return m_isOwner; }
    void* GetMemory() const noexcept { return m_memory; }
    size_t GetMemorySize() const noexcept { return m_memorySize; }

private:
    SharedRingBuffer(void* memory, size_t memorySize, bool isOwner)
        : Base(), m_memory(memory), m_memorySize(memorySize), m_isOwner(isOwner) {}

    static size_t AlignUp(size_t value, size_t alignment) noexcept {
        return (value + alignment - 1) & ~(alignment - 1);
    }

    void* m_memory{nullptr};
    size_t m_memorySize{0};
    bool m_isOwner{false};
};

// ==============================================================================
// SharedMemoryMetadata - Shared Memory Metadata / 共享内存元数据
// ==============================================================================

struct alignas(kCacheLineSize) SharedMemoryMetadata {
    uint32_t magic{0};
    uint32_t version{0};
    size_t totalSize{0};
    size_t configOffset{0};
    size_t nameTableOffset{0};
    size_t ringBufferOffset{0};
    size_t ringBufferCapacity{0};
    QueueFullPolicy policy{QueueFullPolicy::DropNewest};
    
    static constexpr uint32_t kMagic = 0x4F4E4550;  // "ONEP"
    static constexpr uint32_t kVersion = 1;
    
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
    
    bool IsValid() const noexcept { return magic == kMagic && version == kVersion; }
};

// ==============================================================================
// SharedLoggerConfig - Shared Logger Configuration / 共享 Logger 配置
// ==============================================================================

struct alignas(kCacheLineSize) SharedLoggerConfig {
    alignas(kCacheLineSize) std::atomic<Level> logLevel{Level::Info};
    alignas(kCacheLineSize) std::atomic<uint32_t> configVersion{0};
    
    void Init() noexcept {
        logLevel.store(Level::Info, std::memory_order_relaxed);
        configVersion.store(0, std::memory_order_relaxed);
    }
    
    Level GetLevel() const noexcept { return logLevel.load(std::memory_order_acquire); }
    
    void SetLevel(Level level) noexcept {
        logLevel.store(level, std::memory_order_release);
        configVersion.fetch_add(1, std::memory_order_release);
    }
    
    uint32_t GetVersion() const noexcept { return configVersion.load(std::memory_order_acquire); }
};

// ==============================================================================
// NameIdMapping - Name-ID Mapping Entry / 名称-ID 映射条目
// ==============================================================================

struct NameIdMapping {
    static constexpr size_t kMaxNameLength = 31;
    
    uint32_t id{0};
    char name[kMaxNameLength + 1]{0};
    
    void Set(uint32_t newId, const char* newName) noexcept {
        id = newId;
        std::strncpy(name, newName, kMaxNameLength);
        name[kMaxNameLength] = '\0';
    }
    
    bool IsEmpty() const noexcept { return name[0] == '\0'; }
};

// ==============================================================================
// ProcessThreadNameTable - Process/Thread Name-ID Table / 进程/线程名称-ID 对照表
// ==============================================================================

struct alignas(kCacheLineSize) ProcessThreadNameTable {
    static constexpr size_t kMaxProcesses = 64;
    static constexpr size_t kMaxThreads = 256;
    
    alignas(kCacheLineSize) std::atomic<uint32_t> processCount{0};
    alignas(kCacheLineSize) std::atomic<uint32_t> threadCount{0};
    NameIdMapping processes[kMaxProcesses];
    NameIdMapping threads[kMaxThreads];
    
    void Init() noexcept {
        processCount.store(0, std::memory_order_relaxed);
        threadCount.store(0, std::memory_order_relaxed);
        for (size_t i = 0; i < kMaxProcesses; ++i) { processes[i] = NameIdMapping{}; }
        for (size_t i = 0; i < kMaxThreads; ++i) { threads[i] = NameIdMapping{}; }
    }
    
    uint32_t RegisterProcess(const char* name) noexcept {
        uint32_t count = processCount.load(std::memory_order_acquire);
        for (uint32_t i = 0; i < count && i < kMaxProcesses; ++i) {
            if (std::strcmp(processes[i].name, name) == 0) { return processes[i].id; }
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
    
    uint32_t RegisterThread(const char* name) noexcept {
        uint32_t count = threadCount.load(std::memory_order_acquire);
        for (uint32_t i = 0; i < count && i < kMaxThreads; ++i) {
            if (std::strcmp(threads[i].name, name) == 0) { return threads[i].id; }
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
    
    uint32_t GetProcessCount() const noexcept { return processCount.load(std::memory_order_acquire); }
    uint32_t GetThreadCount() const noexcept { return threadCount.load(std::memory_order_acquire); }
};

// ==============================================================================
// SharedMemory - Shared Memory Manager / 共享内存管理器
// ==============================================================================

template<bool EnableWFC = true, bool EnableShadowTail = true>
class SharedMemory {
public:
    static std::unique_ptr<SharedMemory> Create(
        const std::string& name,
        size_t ringBufferCapacity,
        QueueFullPolicy policy = QueueFullPolicy::DropNewest) {
        
        if (name.empty() || ringBufferCapacity == 0) { return nullptr; }
        
        size_t totalSize = CalculateRequiredSize(ringBufferCapacity);
        
        int fd = shm_open(name.c_str(), O_CREAT | O_RDWR | O_EXCL, 0666);
        if (fd < 0) {
            shm_unlink(name.c_str());
            fd = shm_open(name.c_str(), O_CREAT | O_RDWR | O_EXCL, 0666);
            if (fd < 0) { return nullptr; }
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
        
        shm->m_metadata = reinterpret_cast<SharedMemoryMetadata*>(static_cast<uint8_t*>(memory) + metadataOffset);
        shm->m_metadata->Init(totalSize, configOffset, nameTableOffset, ringBufferOffset, ringBufferCapacity, policy);
        
        shm->m_config = reinterpret_cast<SharedLoggerConfig*>(static_cast<uint8_t*>(memory) + configOffset);
        shm->m_config->Init();
        
        shm->m_nameTable = reinterpret_cast<ProcessThreadNameTable*>(static_cast<uint8_t*>(memory) + nameTableOffset);
        shm->m_nameTable->Init();
        
        void* ringBufferMemory = static_cast<uint8_t*>(memory) + ringBufferOffset;
        size_t ringBufferSize = SharedRingBuffer<LogEntry, EnableWFC, EnableShadowTail>::CalculateRequiredSize(ringBufferCapacity);
        shm->m_ringBuffer = SharedRingBuffer<LogEntry, EnableWFC, EnableShadowTail>::Create(
            ringBufferMemory, ringBufferSize, ringBufferCapacity, policy);
        
        if (!shm->m_ringBuffer) { return nullptr; }
        
        return shm;
    }

    static std::unique_ptr<SharedMemory> Connect(const std::string& name) {
        if (name.empty()) { return nullptr; }
        
        int fd = shm_open(name.c_str(), O_RDWR, 0666);
        if (fd < 0) { return nullptr; }
        
        struct stat st;
        if (fstat(fd, &st) < 0) { close(fd); return nullptr; }
        size_t totalSize = static_cast<size_t>(st.st_size);
        
        void* memory = mmap(nullptr, totalSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (memory == MAP_FAILED) { close(fd); return nullptr; }
        
        auto* metadata = reinterpret_cast<SharedMemoryMetadata*>(memory);
        if (!metadata->IsValid()) { munmap(memory, totalSize); close(fd); return nullptr; }
        
        auto shm = std::unique_ptr<SharedMemory>(new SharedMemory(name, false));
        shm->m_fd = fd;
        shm->m_memory = memory;
        shm->m_size = totalSize;
        shm->m_metadata = metadata;
        
        shm->m_config = reinterpret_cast<SharedLoggerConfig*>(static_cast<uint8_t*>(memory) + metadata->configOffset);
        shm->m_nameTable = reinterpret_cast<ProcessThreadNameTable*>(static_cast<uint8_t*>(memory) + metadata->nameTableOffset);
        
        void* ringBufferMemory = static_cast<uint8_t*>(memory) + metadata->ringBufferOffset;
        size_t ringBufferSize = SharedRingBuffer<LogEntry, EnableWFC, EnableShadowTail>::CalculateRequiredSize(metadata->ringBufferCapacity);
        shm->m_ringBuffer = SharedRingBuffer<LogEntry, EnableWFC, EnableShadowTail>::Connect(ringBufferMemory, ringBufferSize);
        
        if (!shm->m_ringBuffer) { munmap(memory, totalSize); close(fd); return nullptr; }
        
        return shm;
    }
    
    static size_t CalculateRequiredSize(size_t ringBufferCapacity) {
        size_t metadataSize = AlignUp(sizeof(SharedMemoryMetadata), kCacheLineSize);
        size_t configSize = AlignUp(sizeof(SharedLoggerConfig), kCacheLineSize);
        size_t nameTableSize = AlignUp(sizeof(ProcessThreadNameTable), kCacheLineSize);
        size_t ringBufferSize = SharedRingBuffer<LogEntry, EnableWFC, EnableShadowTail>::CalculateRequiredSize(ringBufferCapacity);
        return metadataSize + configSize + nameTableSize + ringBufferSize;
    }
    
    ~SharedMemory() {
        if (m_ringBuffer) { delete m_ringBuffer; m_ringBuffer = nullptr; }
        if (m_memory && m_size > 0) { munmap(m_memory, m_size); m_memory = nullptr; }
        if (m_fd >= 0) { close(m_fd); m_fd = -1; }
        if (m_isOwner && !m_name.empty()) { shm_unlink(m_name.c_str()); }
    }
    
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
    
    SharedRingBuffer<LogEntry, EnableWFC, EnableShadowTail>* GetRingBuffer() noexcept { return m_ringBuffer; }
    const SharedRingBuffer<LogEntry, EnableWFC, EnableShadowTail>* GetRingBuffer() const noexcept { return m_ringBuffer; }
    
    // =========================================================================
    // Name Registration / 名称注册
    // =========================================================================
    
    uint32_t RegisterProcess(const std::string& name) { return m_nameTable ? m_nameTable->RegisterProcess(name.c_str()) : 0; }
    uint32_t RegisterThread(const std::string& name) { return m_nameTable ? m_nameTable->RegisterThread(name.c_str()) : 0; }
    const char* GetProcessName(uint32_t id) const { return m_nameTable ? m_nameTable->GetProcessName(id) : nullptr; }
    const char* GetThreadName(uint32_t id) const { return m_nameTable ? m_nameTable->GetThreadName(id) : nullptr; }
    
    // =========================================================================
    // Status / 状态
    // =========================================================================
    
    bool IsOwner() const noexcept { return m_isOwner; }
    const std::string& Name() const noexcept { return m_name; }
    size_t Size() const noexcept { return m_size; }
    
    // =========================================================================
    // Notification / 通知机制
    // =========================================================================
    
    void NotifyConsumer() noexcept { if (m_ringBuffer) { m_ringBuffer->NotifyConsumer(); } }
    
    bool WaitForData(std::chrono::microseconds pollInterval, std::chrono::milliseconds pollTimeout) noexcept {
        if (m_ringBuffer) { return m_ringBuffer->WaitForData(pollInterval, pollTimeout); }
        return false;
    }

private:
    SharedMemory(const std::string& name, bool isOwner) : m_name(name), m_isOwner(isOwner) {}
    
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
    SharedRingBuffer<LogEntry, EnableWFC, EnableShadowTail>* m_ringBuffer{nullptr};
};

}  // namespace internal

// ==============================================================================
// Public API aliases / 公共 API 别名
// ==============================================================================

template<typename T, bool EnableWFC = true, bool EnableShadowTail = true>
using SharedRingBuffer = internal::SharedRingBuffer<T, EnableWFC, EnableShadowTail>;

template<bool EnableWFC = true, bool EnableShadowTail = true>
using SharedMemory = internal::SharedMemory<EnableWFC, EnableShadowTail>;

using SharedMemoryMetadata = internal::SharedMemoryMetadata;
using SharedLoggerConfig = internal::SharedLoggerConfig;
using NameIdMapping = internal::NameIdMapping;
using ProcessThreadNameTable = internal::ProcessThreadNameTable;

}  // namespace oneplog
