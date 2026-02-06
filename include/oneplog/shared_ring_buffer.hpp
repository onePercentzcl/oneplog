/**
 * @file shared_ring_buffer.hpp
 * @brief Shared Memory Ring Buffer Implementation
 * @brief 共享内存环形缓冲区实现
 *
 * SharedRingBuffer inherits from RingBuffer and is designed to be placed in
 * shared memory for multi-process logging. Unlike HeapRingBuffer which allocates
 * memory on the heap, SharedRingBuffer is constructed in-place on shared memory
 * provided by the caller.
 *
 * SharedRingBuffer 继承自 RingBuffer，设计用于放置在共享内存中以支持多进程日志。
 * 与在堆上分配内存的 HeapRingBuffer 不同，SharedRingBuffer 在调用者提供的
 * 共享内存上就地构造。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include "common.hpp"
#include "ring_buffer.hpp"
#include <cstdint>
#include <cstring>
#include <string>
#include <memory>

// Platform-specific shared memory headers
// 平台特定的共享内存头文件
#if defined(__linux__) || defined(__APPLE__)
    #include <sys/mman.h>
    #include <sys/stat.h>
    #include <fcntl.h>
    #include <unistd.h>
    #include <cerrno>
#elif defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <windows.h>
#endif

namespace oneplog::internal {

/**
 * @brief Shared Memory Ring Buffer for multi-process logging.
 * @brief 用于多进程日志的共享内存环形缓冲区。
 *
 * This class inherits from RingBuffer and is designed to be placed in shared
 * memory. Key differences from HeapRingBuffer:
 * - Memory is provided externally (shared memory region)
 * - Uses placement new for construction
 * - Provides Create() for main process and Connect() for child processes
 *
 * 该类继承自 RingBuffer，设计用于放置在共享内存中。
 * 与 HeapRingBuffer 的主要区别：
 * - 内存由外部提供（共享内存区域）
 * - 使用 placement new 进行构造
 * - 提供 Create() 用于主进程，Connect() 用于子进程
 *
 * @tparam slotSize Size of each slot in bytes (must be multiple of kCacheLineSize)
 *                  每个槽位的大小（字节，必须是 kCacheLineSize 的整数倍）
 * @tparam slotCount Number of slots (must be power of 2)
 *                   槽位数量（必须是 2 的幂）
 */
template <size_t slotSize = 512, size_t slotCount = 1024>
class SharedRingBuffer : public RingBuffer<slotSize, slotCount> {
public:
    using Base = RingBuffer<slotSize, slotCount>;
    
    // Inherit constants from base class
    // 从基类继承常量
    using Base::kMaxDataSize;
    using Base::kIndexMask;

    /**
     * @brief Magic number for shared memory validation.
     * @brief 用于共享内存验证的魔数。
     */
    static constexpr uint32_t kMagicNumber = 0x53524247;  // "SRBG" in ASCII

    /**
     * @brief Version number for compatibility checking.
     * @brief 用于兼容性检查的版本号。
     */
    static constexpr uint32_t kVersion = 1;


    /**
     * @brief Header structure for shared memory metadata.
     * @brief 共享内存元数据头结构。
     * 
     * This header is placed at the beginning of the shared memory region,
     * before the SharedRingBuffer itself.
     * 
     * 此头部放置在共享内存区域的开头，在 SharedRingBuffer 之前。
     */
    struct alignas(kCacheLineSize) ShmMetadata {
        uint32_t m_magic{kMagicNumber};     ///< Magic number for validation / 验证用魔数
        uint32_t m_version{kVersion};        ///< Version for compatibility / 兼容性版本
        size_t m_slotSize{slotSize};         ///< Slot size / 槽位大小
        size_t m_slotCount{slotCount};       ///< Slot count / 槽位数量
        size_t m_totalSize{0};               ///< Total shared memory size / 共享内存总大小
        uint8_t m_reserved[kCacheLineSize - sizeof(uint32_t) * 2 - sizeof(size_t) * 3]{};
    };

    /**
     * @brief Handle type for shared memory (platform-specific).
     * @brief 共享内存句柄类型（平台特定）。
     */
#if defined(_WIN32)
    using ShmHandle = HANDLE;
    static constexpr ShmHandle kInvalidHandle = nullptr;
#else
    using ShmHandle = int;
    static constexpr ShmHandle kInvalidHandle = -1;
#endif

    /**
     * @brief Result structure for Create/Connect operations.
     * @brief Create/Connect 操作的结果结构。
     */
    struct CreateResult {
        SharedRingBuffer* buffer{nullptr};   ///< Pointer to buffer / 缓冲区指针
        void* mappedMemory{nullptr};         ///< Mapped memory address / 映射内存地址
        ShmHandle handle{kInvalidHandle};    ///< Shared memory handle / 共享内存句柄
        size_t size{0};                      ///< Total size / 总大小
        bool isOwner{false};                 ///< Whether this is the creator / 是否是创建者
    };

    /**
     * @brief Custom deleter for shared memory cleanup.
     * @brief 用于共享内存清理的自定义删除器。
     */
    struct ShmDeleter {
        void* mappedMemory{nullptr};
        ShmHandle handle{kInvalidHandle};
        size_t size{0};
        bool isOwner{false};
        std::string name;

        void operator()(SharedRingBuffer* ptr) const {
            if (!ptr) return;
            
            // Call destructor (no-op for SharedRingBuffer, but good practice)
            // 调用析构函数（对 SharedRingBuffer 无操作，但是好习惯）
            ptr->~SharedRingBuffer();
            
            // Unmap memory
            // 取消内存映射
#if defined(_WIN32)
            if (mappedMemory) {
                UnmapViewOfFile(mappedMemory);
            }
            if (handle != kInvalidHandle) {
                CloseHandle(handle);
            }
#else
            if (mappedMemory && mappedMemory != MAP_FAILED) {
                munmap(mappedMemory, size);
            }
            if (handle != kInvalidHandle) {
                close(handle);
            }
            // Only unlink if owner
            // 只有所有者才取消链接
            if (isOwner && !name.empty()) {
                shm_unlink(name.c_str());
            }
#endif
        }
    };

    /**
     * @brief Unique pointer type for SharedRingBuffer with shared memory cleanup.
     * @brief 带共享内存清理的 SharedRingBuffer 唯一指针类型。
     */
    using Ptr = std::unique_ptr<SharedRingBuffer, ShmDeleter>;


    /**
     * @brief Get the required shared memory size for this buffer type.
     * @brief 获取此缓冲区类型所需的共享内存大小。
     * @return Total size including metadata and buffer / 包含元数据和缓冲区的总大小
     */
    static constexpr size_t GetRequiredMemorySize() noexcept {
        return sizeof(ShmMetadata) + sizeof(SharedRingBuffer);
    }

    /**
     * @brief Create a SharedRingBuffer in new shared memory (main process).
     * @brief 在新共享内存中创建 SharedRingBuffer（主进程）。
     * 
     * @param name Shared memory name (e.g., "/oneplog_buffer")
     *             共享内存名称（例如 "/oneplog_buffer"）
     * @param policy Queue full policy / 队列满策略
     * @return Unique pointer to the created buffer, or nullptr on failure
     *         指向创建的缓冲区的唯一指针，失败时返回 nullptr
     * 
     * @note On POSIX systems, name should start with '/' and contain no other '/'.
     * @note 在 POSIX 系统上，名称应以 '/' 开头且不包含其他 '/'。
     */
    static Ptr Create(const std::string& name, 
                      QueueFullPolicy policy = QueueFullPolicy::DropNewest) {
        if (name.empty()) return nullptr;
        
        size_t totalSize = GetRequiredMemorySize();
        
#if defined(_WIN32)
        // Windows: CreateFileMapping
        HANDLE handle = CreateFileMappingA(
            INVALID_HANDLE_VALUE,
            nullptr,
            PAGE_READWRITE,
            static_cast<DWORD>(totalSize >> 32),
            static_cast<DWORD>(totalSize & 0xFFFFFFFF),
            name.c_str()
        );
        
        if (!handle) return nullptr;
        
        void* mappedMemory = MapViewOfFile(
            handle,
            FILE_MAP_ALL_ACCESS,
            0, 0,
            totalSize
        );
        
        if (!mappedMemory) {
            CloseHandle(handle);
            return nullptr;
        }
#else
        // POSIX: shm_open + mmap
        int fd = shm_open(name.c_str(), O_CREAT | O_RDWR | O_EXCL, 0666);
        if (fd == -1) {
            // If already exists, try to unlink and recreate
            // 如果已存在，尝试取消链接并重新创建
            if (errno == EEXIST) {
                shm_unlink(name.c_str());
                fd = shm_open(name.c_str(), O_CREAT | O_RDWR | O_EXCL, 0666);
            }
            if (fd == -1) return nullptr;
        }
        
        // Set size
        // 设置大小
        if (ftruncate(fd, static_cast<off_t>(totalSize)) == -1) {
            close(fd);
            shm_unlink(name.c_str());
            return nullptr;
        }
        
        // Map memory
        // 映射内存
        void* mappedMemory = mmap(nullptr, totalSize, 
                                   PROT_READ | PROT_WRITE, 
                                   MAP_SHARED, fd, 0);
        if (mappedMemory == MAP_FAILED) {
            close(fd);
            shm_unlink(name.c_str());
            return nullptr;
        }
        
        ShmHandle handle = fd;
#endif
        
        // Initialize metadata
        // 初始化元数据
        auto* metadata = static_cast<ShmMetadata*>(mappedMemory);
        new(metadata) ShmMetadata();
        metadata->m_totalSize = totalSize;
        
        // Construct SharedRingBuffer after metadata
        // 在元数据之后构造 SharedRingBuffer
        void* bufferPtr = static_cast<char*>(mappedMemory) + sizeof(ShmMetadata);
        auto* buffer = new(bufferPtr) SharedRingBuffer();
        buffer->Init(policy);
        
        // Create deleter
        // 创建删除器
        ShmDeleter deleter;
        deleter.mappedMemory = mappedMemory;
        deleter.handle = handle;
        deleter.size = totalSize;
        deleter.isOwner = true;
        deleter.name = name;
        
        return Ptr(buffer, deleter);
    }


    /**
     * @brief Connect to an existing SharedRingBuffer in shared memory (child process).
     * @brief 连接到共享内存中已存在的 SharedRingBuffer（子进程）。
     * 
     * @param name Shared memory name (must match the name used in Create)
     *             共享内存名称（必须与 Create 中使用的名称匹配）
     * @return Unique pointer to the connected buffer, or nullptr on failure
     *         指向连接的缓冲区的唯一指针，失败时返回 nullptr
     */
    static Ptr Connect(const std::string& name) {
        if (name.empty()) return nullptr;
        
#if defined(_WIN32)
        // Windows: OpenFileMapping
        HANDLE handle = OpenFileMappingA(
            FILE_MAP_ALL_ACCESS,
            FALSE,
            name.c_str()
        );
        
        if (!handle) return nullptr;
        
        // First map just the metadata to get the size
        // 首先只映射元数据以获取大小
        void* tempMap = MapViewOfFile(
            handle,
            FILE_MAP_READ,
            0, 0,
            sizeof(ShmMetadata)
        );
        
        if (!tempMap) {
            CloseHandle(handle);
            return nullptr;
        }
        
        auto* tempMetadata = static_cast<ShmMetadata*>(tempMap);
        
        // Validate metadata
        // 验证元数据
        if (tempMetadata->m_magic != kMagicNumber ||
            tempMetadata->m_version != kVersion ||
            tempMetadata->m_slotSize != slotSize ||
            tempMetadata->m_slotCount != slotCount) {
            UnmapViewOfFile(tempMap);
            CloseHandle(handle);
            return nullptr;
        }
        
        size_t totalSize = tempMetadata->m_totalSize;
        UnmapViewOfFile(tempMap);
        
        // Map the full region
        // 映射完整区域
        void* mappedMemory = MapViewOfFile(
            handle,
            FILE_MAP_ALL_ACCESS,
            0, 0,
            totalSize
        );
        
        if (!mappedMemory) {
            CloseHandle(handle);
            return nullptr;
        }
#else
        // POSIX: shm_open + mmap
        int fd = shm_open(name.c_str(), O_RDWR, 0666);
        if (fd == -1) return nullptr;
        
        // First map just the metadata to get the size
        // 首先只映射元数据以获取大小
        void* tempMap = mmap(nullptr, sizeof(ShmMetadata),
                             PROT_READ, MAP_SHARED, fd, 0);
        if (tempMap == MAP_FAILED) {
            close(fd);
            return nullptr;
        }
        
        auto* tempMetadata = static_cast<ShmMetadata*>(tempMap);
        
        // Validate metadata
        // 验证元数据
        if (tempMetadata->m_magic != kMagicNumber ||
            tempMetadata->m_version != kVersion ||
            tempMetadata->m_slotSize != slotSize ||
            tempMetadata->m_slotCount != slotCount) {
            munmap(tempMap, sizeof(ShmMetadata));
            close(fd);
            return nullptr;
        }
        
        size_t totalSize = tempMetadata->m_totalSize;
        munmap(tempMap, sizeof(ShmMetadata));
        
        // Map the full region
        // 映射完整区域
        void* mappedMemory = mmap(nullptr, totalSize,
                                   PROT_READ | PROT_WRITE,
                                   MAP_SHARED, fd, 0);
        if (mappedMemory == MAP_FAILED) {
            close(fd);
            return nullptr;
        }
        
        ShmHandle handle = fd;
#endif
        
        // Get buffer pointer (after metadata)
        // 获取缓冲区指针（在元数据之后）
        void* bufferPtr = static_cast<char*>(mappedMemory) + sizeof(ShmMetadata);
        auto* buffer = static_cast<SharedRingBuffer*>(bufferPtr);
        
        // Create deleter (not owner, won't unlink)
        // 创建删除器（非所有者，不会取消链接）
        ShmDeleter deleter;
        deleter.mappedMemory = mappedMemory;
        deleter.handle = handle;
        deleter.size = totalSize;
        deleter.isOwner = false;
        deleter.name = name;
        
        return Ptr(buffer, deleter);
    }


    /**
     * @brief Create a SharedRingBuffer on externally provided memory.
     * @brief 在外部提供的内存上创建 SharedRingBuffer。
     * 
     * This is useful when the shared memory is managed by another component
     * (e.g., SharedMemory class).
     * 
     * 当共享内存由其他组件管理时（例如 SharedMemory 类），这很有用。
     * 
     * @param memory Pointer to memory region (must be at least GetRequiredMemorySize())
     *               指向内存区域的指针（必须至少为 GetRequiredMemorySize()）
     * @param policy Queue full policy / 队列满策略
     * @return Pointer to the created buffer, or nullptr on failure
     *         指向创建的缓冲区的指针，失败时返回 nullptr
     * 
     * @note Caller is responsible for memory management.
     * @note 调用者负责内存管理。
     */
    static SharedRingBuffer* CreateInPlace(void* memory, 
                                           QueueFullPolicy policy = QueueFullPolicy::DropNewest) {
        if (!memory) return nullptr;
        
        auto* buffer = new(memory) SharedRingBuffer();
        buffer->Init(policy);
        return buffer;
    }

    /**
     * @brief Connect to a SharedRingBuffer on externally provided memory.
     * @brief 连接到外部提供的内存上的 SharedRingBuffer。
     * 
     * @param memory Pointer to memory region containing an initialized SharedRingBuffer
     *               指向包含已初始化 SharedRingBuffer 的内存区域的指针
     * @return Pointer to the buffer, or nullptr on failure
     *         指向缓冲区的指针，失败时返回 nullptr
     * 
     * @note Caller is responsible for memory management.
     * @note 调用者负责内存管理。
     */
    static SharedRingBuffer* ConnectInPlace(void* memory) {
        if (!memory) return nullptr;
        return static_cast<SharedRingBuffer*>(memory);
    }

    /**
     * @brief Get the total memory size for this buffer type (without metadata).
     * @brief 获取此缓冲区类型的总内存大小（不含元数据）。
     */
    static constexpr size_t GetTotalMemorySize() noexcept {
        return sizeof(SharedRingBuffer);
    }

    /**
     * @brief Default constructor.
     * @brief 默认构造函数。
     * @note Use Create() or Connect() factory methods for shared memory allocation.
     * @note 使用 Create() 或 Connect() 工厂方法进行共享内存分配。
     */
    SharedRingBuffer() = default;

    /**
     * @brief Destructor.
     * @brief 析构函数。
     */
    ~SharedRingBuffer() = default;

    // Non-copyable / 不可复制
    SharedRingBuffer(const SharedRingBuffer&) = delete;
    SharedRingBuffer& operator=(const SharedRingBuffer&) = delete;

    // Non-movable (due to shared memory placement)
    // 不可移动（由于共享内存放置）
    SharedRingBuffer(SharedRingBuffer&&) = delete;
    SharedRingBuffer& operator=(SharedRingBuffer&&) = delete;
};

// ============================================================================
// Type Aliases / 类型别名
// ============================================================================

/**
 * @brief Default SharedRingBuffer with 512-byte slots and 1024 slots.
 * @brief 默认的 SharedRingBuffer，512 字节槽位，1024 个槽位。
 */
using DefaultSharedRingBuffer = SharedRingBuffer<512, 1024>;

/**
 * @brief Small SharedRingBuffer with 256-byte slots and 256 slots.
 * @brief 小型 SharedRingBuffer，256 字节槽位，256 个槽位。
 */
using SmallSharedRingBuffer = SharedRingBuffer<256, 256>;

/**
 * @brief Large SharedRingBuffer with 1024-byte slots and 4096 slots.
 * @brief 大型 SharedRingBuffer，1024 字节槽位，4096 个槽位。
 */
using LargeSharedRingBuffer = SharedRingBuffer<1024, 4096>;

} // namespace oneplog::internal
