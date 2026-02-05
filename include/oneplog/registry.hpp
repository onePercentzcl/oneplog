/**
 * @file registry.hpp
 * @brief Process/Thread Registry for Shared Memory
 * @brief 共享内存的进程/线程注册表
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include <new>      // For placement new / 用于 placement new
#include <cstddef>  // For size_t / 用于 size_t
#include <cstring>  // For strncpy / 用于 strncpy
#include "common.hpp"

namespace oneplog::internal {
// =============================================================
// 1. Linux Implementation (Direct Mapping - God Mode)
// 1. Linux 平台实现 (直接映射模式 - 上帝模式)
// =============================================================
#ifdef __linux__

// ---------------------------------------------------------
// Constants Definition / 常量定义
// ---------------------------------------------------------

// Allow overriding default max PID via macro SET_MAX_PID
// 允许通过外部宏 SET_MAX_PID 覆盖默认 PID 上限
#ifdef SET_MAX_PID
constexpr size_t kLinuxMaxPID = SET_MAX_PID;
#else
/**
 * @brief Default max PID for Linux.
 * @brief Linux 的默认最大 PID。
 * @details The kernel hard limit is usually 4,194,304.
 * We rely on virtual memory demand paging, so unused slots consume no physical RAM.
 * @details 内核硬限制通常为 4,194,304。
 * 我们依赖虚拟内存的按需分页特性，未使用的槽位不占用物理 RAM。
 */
constexpr size_t kLinuxMaxPID = 4194304;
#endif

// ---------------------------------------------------------
// Struct Definition / 结构体定义
// ---------------------------------------------------------

/**
 * @brief Compact registry table using PID as index directly.
 * @brief 直接使用 PID 作为索引的紧凑注册表。
 */
struct alignas(kCacheLineSize) Registry {
    /**
     * @brief 2D array storing names. Index is PID.
     * @brief 存储名称的二维数组。下标即 PID。
     * @details
     * - Layout: [PID 0 Name][PID 1 Name]... Compact arrangement.
     * - Note: This structure is NOT cache-line aligned per slot to save space (Compact Layout).
     * - Trade-off: Potential false sharing during updates is acceptable as updates mostly occur during init.
     * @details
     * - 布局：[PID 0 名称][PID 1 名称]... 紧凑排列。
     * - 注意：为了节省空间（紧凑布局），此结构体没有对每个槽位进行缓存行对齐。
     * - 权衡：更新时的潜在伪共享是可接受的，因为更新主要发生在初始化阶段。
     */
    char m_name[kLinuxMaxPID][16];
};

// =============================================================
// 2. Non-Linux Implementation (macOS/Windows - Compatibility Mode)
// 2. 非 Linux 平台实现 (macOS/Windows - 兼容模式)
// =============================================================
#else

// ---------------------------------------------------------
// Constants Definition / 常量定义
// ---------------------------------------------------------
#ifdef SET_MAX_PID
constexpr size_t kMaxPID = SET_MAX_PID;
#else
constexpr size_t kMaxPID = 32; // Smaller default for dev env / 开发环境默认值较小
#endif

#ifdef SET_MAX_TID
constexpr size_t kMaxTID = SET_MAX_TID;
#else
constexpr size_t kMaxTID = 1024;
#endif

// ---------------------------------------------------------
// Struct Definition (SoA) / 结构体定义 (数组结构)
// ---------------------------------------------------------

/**
 * @brief Registry for systems with sparse/large IDs (e.g., macOS).
 * @brief 适用于 ID 稀疏或数值巨大的系统（如 macOS）的注册表。
 */
struct alignas(kCacheLineSize) Registry {
    pid_t m_pid[kMaxPID];            ///< Array storing PIDs / 存储 PID 的数组
    pid_t m_tid[kMaxTID];            ///< Array storing TIDs / 存储 TID 的数组
    char m_processName[kMaxPID][16]; ///< Names corresponding to m_pid / 对应的进程名
    char m_threadName[kMaxTID][16];  ///< Names corresponding to m_tid / 对应的线程名
};

#endif

// =============================================================
// 3. Helper Functions / 辅助函数
// =============================================================

/**
 * @brief Initialize the registry on a pre-allocated memory block.
 * @brief 在预分配的内存块上初始化注册表。
 * * @param ptr Pointer to the start of the memory block (e.g., mmap result).
 * @param ptr 指向内存块起始位置的指针（例如 mmap 的结果）。
 * @return Registry* Pointer to the initialized Registry object.
 * @return Registry* 指向初始化后的 Registry 对象的指针。
 */
inline Registry* InitRegistry(void* ptr) {
    if (!ptr) return nullptr;
    // Placement new to construct the object in shared memory
    // 使用 Placement new 在共享内存中构造对象
    return new(ptr) Registry();
}
} // namespace oneplog::internal