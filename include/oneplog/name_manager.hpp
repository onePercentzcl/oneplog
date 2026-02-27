/**
 * @file name_manager.hpp
 * @brief Process and module name management for onePlog
 * @brief onePlog 的进程名和模块名管理
 *
 * This file provides name management functionality for identifying log sources:
 * - Process name: Global identifier for the application/process
 * - Module name: Thread-local identifier for logical components
 *
 * 此文件提供用于标识日志来源的名称管理功能：
 * - 进程名：应用程序/进程的全局标识符
 * - 模块名：逻辑组件的线程局部标识符
 *
 * @section design Design Principles / 设计原则
 * - Process name and module name belong to process/thread, NOT to Logger
 * - 进程名和模块名属于进程/线程，而不是 Logger 对象
 *
 * @section storage Storage Mechanisms / 存储机制
 * - Sync: Process name is global constant, module name is thread_local.
 *         Sink can directly access both names.
 *         进程名为全局常量，模块名为线程局部变量，Sink 可直接访问。
 *
 * - Async: Process name is global constant, module name is thread_local.
 *          BUT WriterThread cannot see thread_local, so need TID-to-name table on heap.
 *          进程名为全局常量，模块名为线程局部变量。
 *          但 WriterThread 看不到 thread_local，所以需要堆上的 TID-模块名对照表。
 *
 * - MProc: Process name is global constant, module name is thread_local.
 *          Consumer process needs shared memory PID/TID-to-name table.
 *          进程名为全局常量，模块名为线程局部变量。
 *          消费者进程需要共享内存中的 PID/TID-名称对照表。
 *
 * @section platform Platform-specific Optimizations / 平台特定优化
 * - Linux: Uses DirectMappingTable with O(1) lookup using TID as array index
 * - Non-Linux (macOS/Windows): Uses ArrayMappingTable with O(n) linear search
 *
 * @see ThreadModuleTable for the TID-to-name mapping implementation
 * @see NameManager for the unified interface
 *
 * @copyright Copyright (c) 2024 onePlog
 *
 * _Requirements: 7.1, 7.2, 7.3, 7.4_
 */

#pragma once

#include "oneplog/common.hpp"
#include "oneplog/internal/platform_lookup_table.hpp"

#include <atomic>
#include <cstring>
#include <memory>
#include <string>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

namespace oneplog {

// ==============================================================================
// Forward Declarations / 前向声明
// ==============================================================================

namespace internal {

// Forward declaration of SharedMemory
template<bool EnableWFC, bool EnableShadowTail>
class SharedMemory;

// Note: IMProcSharedMemory interface and GetGlobalMProcSharedMemory/SetGlobalMProcSharedMemory
// are defined in common.hpp to avoid circular dependencies
// 注意：IMProcSharedMemory 接口和 GetGlobalMProcSharedMemory/SetGlobalMProcSharedMemory
// 定义在 common.hpp 中以避免循环依赖

}  // namespace internal

// ==============================================================================
// Compile-time Configuration / 编译期配置
// ==============================================================================

/**
 * @brief Configuration options for name manager
 * @brief 名称管理器的配置选项
 *
 * Users can customize these values by defining them before including this header.
 * 用户可以在包含此头文件之前定义这些值来自定义。
 */
namespace config {

/// Maximum name length (default: 31 for backward compatibility)
/// 最大名称长度（默认：31，用于向后兼容）
#ifndef ONEPLOG_MAX_NAME_LENGTH
inline constexpr size_t kMaxNameLength = 31;
#else
inline constexpr size_t kMaxNameLength = ONEPLOG_MAX_NAME_LENGTH;
#endif

/// Use optimized lookup table (default: true)
/// 使用优化的查找表（默认：true）
#ifndef ONEPLOG_USE_OPTIMIZED_LOOKUP
inline constexpr bool kUseOptimizedLookup = true;
#else
inline constexpr bool kUseOptimizedLookup = ONEPLOG_USE_OPTIMIZED_LOOKUP;
#endif

}  // namespace config

// ==============================================================================
// Global Process Name / 全局进程名
// ==============================================================================

/**
 * @brief Global process name (compile-time or early runtime constant)
 * @brief 全局进程名（编译期或早期运行时常量）
 *
 * This should be set once at program startup and never changed.
 * 应该在程序启动时设置一次，之后不再改变。
 */
namespace internal {

inline std::string& GetGlobalProcessName() {
    static std::string processName = "main";
    return processName;
}

}  // namespace internal

/**
 * @brief Set global process name (call once at startup)
 * @brief 设置全局进程名（启动时调用一次）
 * @note Name is truncated to kMaxNameLength characters / 名称会被截断到 kMaxNameLength 字符
 * @note In MProc mode, this also registers to shared memory / 在 MProc 模式下，也会注册到共享内存
 */
inline void SetProcessName(const std::string& name) {
    std::string truncatedName = name;
    if (name.length() > config::kMaxNameLength) {
        truncatedName = name.substr(0, config::kMaxNameLength);
    }
    internal::GetGlobalProcessName() = truncatedName;
    
    // Register to MProc shared memory if available
    // 如果可用，则注册到 MProc 共享内存
    auto* shm = internal::GetGlobalMProcSharedMemory();
    if (shm) {
        shm->RegisterProcess(truncatedName);
    }
}

/**
 * @brief Get global process name
 * @brief 获取全局进程名
 */
inline const std::string& GetProcessName() {
    return internal::GetGlobalProcessName();
}

// ==============================================================================
// Thread-local Module Name / 线程局部模块名
// ==============================================================================

namespace internal {

/**
 * @brief Get thread-local module name
 * @brief 获取线程局部模块名
 *
 * Uses a function to avoid duplicate symbol issues with inline thread_local.
 * 使用函数避免 inline thread_local 的重复符号问题。
 */
inline std::string& GetTlsModuleName() {
    static thread_local std::string tls_moduleName = "main";
    return tls_moduleName;
}

inline uint32_t GetCurrentThreadIdInternal() {
#ifdef _WIN32
    return static_cast<uint32_t>(::GetCurrentThreadId());
#elif defined(__APPLE__)
    uint64_t tid;
    pthread_threadid_np(nullptr, &tid);
    return static_cast<uint32_t>(tid);
#else
    // Linux: use gettid() for actual thread ID (optimized for DirectMappingTable)
    // Linux：使用 gettid() 获取实际线程 ID（为 DirectMappingTable 优化）
    return static_cast<uint32_t>(gettid());
#endif
}

/**
 * @brief Global mode flag for automatic registration
 * @brief 用于自动注册的全局模式标志
 *
 * When true, SetModuleName() will automatically register to global table.
 * 当为 true 时，SetModuleName() 会自动注册到全局表。
 */
inline std::atomic<bool>& GetAutoRegisterFlag() {
    static std::atomic<bool> autoRegister{false};
    return autoRegister;
}

}  // namespace internal

// Forward declaration for RegisterModuleName
inline void RegisterModuleName();

/**
 * @brief Set module name for current thread
 * @brief 设置当前线程的模块名
 *
 * In Async/MProc mode, this automatically registers to the global table.
 * In MProc mode, this also registers to shared memory.
 * 在 Async/MProc 模式下，会自动注册到全局表。
 * 在 MProc 模式下，也会注册到共享内存。
 * @note Name is truncated to kMaxNameLength characters / 名称会被截断到 kMaxNameLength 字符
 */
inline void SetModuleName(const std::string& name) {
    std::string truncatedName = name;
    if (name.length() > config::kMaxNameLength) {
        truncatedName = name.substr(0, config::kMaxNameLength);
    }
    internal::GetTlsModuleName() = truncatedName;
    
    // Auto-register to global table if enabled (Async/MProc mode)
    // 如果启用了自动注册（Async/MProc 模式），则自动注册到全局表
    if (internal::GetAutoRegisterFlag().load(std::memory_order_acquire)) {
        RegisterModuleName();
    }
    
    // Register to MProc shared memory if available
    // 如果可用，则注册到 MProc 共享内存
    auto* shm = internal::GetGlobalMProcSharedMemory();
    if (shm) {
        shm->RegisterThread(truncatedName);
    }
}

/**
 * @brief Get module name for current thread
 * @brief 获取当前线程的模块名
 */
inline const std::string& GetModuleName() {
    return internal::GetTlsModuleName();
}

/**
 * @brief Enable/disable automatic registration to global table
 * @brief 启用/禁用自动注册到全局表
 *
 * Called by Logger::Init() based on mode.
 * 由 Logger::Init() 根据模式调用。
 */
inline void SetAutoRegisterModuleName(bool enable) {
    internal::GetAutoRegisterFlag().store(enable, std::memory_order_release);
}

// ==============================================================================
// ThreadModuleTable - TID to Module Name Mapping for Async Mode
// TID-模块名映射表，用于异步模式
// ==============================================================================

/**
 * @brief Thread-safe TID to module name mapping table for Async mode
 * @brief 用于 Async 模式的线程安全 TID 到模块名映射表
 *
 * This class now uses platform-specific optimized lookup tables:
 * - Linux: DirectMappingTable with O(1) lookup using TID as array index
 * - Non-Linux: ArrayMappingTable with O(n) linear search
 *
 * 此类现在使用平台特定的优化查找表：
 * - Linux：使用 TID 作为数组索引的 O(1) 查找 DirectMappingTable
 * - 非 Linux：O(n) 线性搜索的 ArrayMappingTable
 *
 * In Async mode, WriterThread cannot access thread_local variables of other threads.
 * This table allows WriterThread to lookup module name by TID.
 *
 * 在异步模式下，WriterThread 无法访问其他线程的 thread_local 变量。
 * 此表允许 WriterThread 通过 TID 查找模块名。
 *
 * _Requirements: 7.1, 7.2, 7.3, 7.4_
 */
class ThreadModuleTable {
public:
    /// Maximum number of entries (for ArrayMappingTable on non-Linux)
    /// 最大条目数（用于非 Linux 上的 ArrayMappingTable）
    static constexpr size_t kMaxEntries = 256;
    
    /// Maximum name length / 最大名称长度
    static constexpr size_t kMaxNameLength = config::kMaxNameLength;

    /// Platform information / 平台信息
    static constexpr bool kIsLinuxPlatform = internal::kIsLinuxPlatform;

    ThreadModuleTable() = default;
    ~ThreadModuleTable() = default;

    ThreadModuleTable(const ThreadModuleTable&) = delete;
    ThreadModuleTable& operator=(const ThreadModuleTable&) = delete;
    ThreadModuleTable(ThreadModuleTable&&) = delete;
    ThreadModuleTable& operator=(ThreadModuleTable&&) = delete;

    /**
     * @brief Register or update module name for a thread
     * @brief 注册或更新线程的模块名
     *
     * Uses platform-specific optimized lookup table.
     * 使用平台特定的优化查找表。
     *
     * @param threadId Thread ID / 线程 ID
     * @param name Module name / 模块名
     * @return true if successful, false if table full (non-Linux) or TID out of range (Linux)
     *         成功返回 true，表满（非 Linux）或 TID 超出范围（Linux）返回 false
     */
    bool Register(uint32_t threadId, const std::string& name) {
        return m_table.Register(threadId, std::string_view(name));
    }

    /**
     * @brief Get module name by thread ID
     * @brief 通过线程 ID 获取模块名
     *
     * Uses platform-specific optimized lookup:
     * - Linux: O(1) direct mapping
     * - Non-Linux: O(n) linear search
     *
     * 使用平台特定的优化查找：
     * - Linux：O(1) 直接映射
     * - 非 Linux：O(n) 线性搜索
     *
     * @param threadId Thread ID / 线程 ID
     * @return Module name or empty string if not found / 模块名，未找到返回空字符串
     */
    std::string GetName(uint32_t threadId) const {
        std::string_view name = m_table.GetName(threadId);
        // Return empty string if default name is returned (for backward compatibility)
        // 如果返回默认名称则返回空字符串（用于向后兼容）
        if (name == "main") {
            // Check if this TID is actually registered
            // 检查此 TID 是否实际已注册
            if (!m_table.IsRegistered(threadId)) {
                return "";
            }
        }
        return std::string(name);
    }

    /**
     * @brief Clear all entries
     * @brief 清空所有条目
     */
    void Clear() {
        m_table.Clear();
    }

    /**
     * @brief Get number of registered entries
     * @brief 获取已注册条目数
     */
    size_t Count() const {
        return m_table.Count();
    }

    /**
     * @brief Get lookup complexity description
     * @brief 获取查找复杂度描述
     *
     * @return "O(1)" on Linux, "O(n)" on other platforms
     *         Linux 上返回 "O(1)"，其他平台返回 "O(n)"
     */
    static constexpr const char* GetLookupComplexity() noexcept {
        return internal::GetLookupComplexity();
    }

    /**
     * @brief Get platform name
     * @brief 获取平台名称
     *
     * @return "Linux" on Linux, "Non-Linux" on other platforms
     *         Linux 上返回 "Linux"，其他平台返回 "Non-Linux"
     */
    static constexpr const char* GetPlatformName() noexcept {
        return internal::GetPlatformName();
    }

private:
    /// Platform-specific lookup table / 平台特定查找表
    internal::ExtendedPlatformLookupTable m_table;
};

// ==============================================================================
// Global ThreadModuleTable for Async Mode / 异步模式的全局 TID-模块名表
// ==============================================================================

namespace internal {

inline ThreadModuleTable& GetGlobalThreadModuleTable() {
    static ThreadModuleTable table;
    return table;
}

}  // namespace internal

/**
 * @brief Register current thread's module name to global table (for Async mode)
 * @brief 将当前线程的模块名注册到全局表（用于异步模式）
 *
 * Call this after SetModuleName() in Async mode to make the name visible to WriterThread.
 * 在异步模式下，调用 SetModuleName() 后调用此函数，使模块名对 WriterThread 可见。
 */
inline void RegisterModuleName() {
    uint32_t tid = internal::GetCurrentThreadIdInternal();
    internal::GetGlobalThreadModuleTable().Register(tid, internal::GetTlsModuleName());
}

/**
 * @brief Set module name and register to global table
 * @brief 设置模块名并注册到全局表
 *
 * @deprecated In Async/MProc mode, SetModuleName() now auto-registers.
 *             This function is kept for backward compatibility.
 * @deprecated 在 Async/MProc 模式下，SetModuleName() 现在会自动注册。
 *             此函数保留用于向后兼容。
 */
inline void SetAndRegisterModuleName(const std::string& name) {
    SetModuleName(name);
    // RegisterModuleName() is now called automatically by SetModuleName()
    // when auto-register is enabled
}

/**
 * @brief Lookup module name by thread ID from global table
 * @brief 从全局表中通过线程 ID 查找模块名
 *
 * Used by WriterThread in Async mode.
 * 用于异步模式下的 WriterThread。
 */
inline std::string LookupModuleName(uint32_t threadId) {
    std::string name = internal::GetGlobalThreadModuleTable().GetName(threadId);
    if (!name.empty()) {
        return name;
    }
    return "main";
}

// ==============================================================================
// NameManager - Unified Interface for All Modes / 统一接口
// ==============================================================================

/**
 * @brief Name manager for process and module names
 * @brief 进程名和模块名管理器
 *
 * Provides a unified interface for all three modes.
 * 为三种模式提供统一接口。
 *
 * @tparam EnableWFC Enable WFC support / 启用 WFC 支持
 *
 * _Requirements: 7.1, 7.2, 7.3, 7.4_
 */
template<bool EnableWFC = false>
class NameManager {
public:
    static constexpr size_t kMaxNameLength = config::kMaxNameLength;

    // =========================================================================
    // Process Name (Global) / 进程名（全局）
    // =========================================================================

    static void SetProcessName(const std::string& name) {
        oneplog::SetProcessName(name);
        
        // Note: For MProc mode with shared memory, call RegisterProcessToSharedMemory()
        // 注意：对于使用共享内存的 MProc 模式，请调用 RegisterProcessToSharedMemory()
    }

    static std::string GetProcessName(uint32_t processId = 0) {
        // For local process or when processId is 0: return global process name
        // 对于本地进程或 processId 为 0 时：返回全局进程名
        if (processId == 0) {
            return oneplog::GetProcessName();
        }
        
        // For MProc consumer with non-zero processId: return ID as string
        // (actual shared memory lookup should be done by the caller with SharedMemory instance)
        // 对于非零 processId 的 MProc 消费者：返回 ID 字符串
        // （实际的共享内存查找应由调用者使用 SharedMemory 实例完成）
        return std::to_string(processId);
    }

    // =========================================================================
    // Module Name (Thread-local + Table for Async) / 模块名
    // =========================================================================

    static void SetModuleName(const std::string& name) {
        // Global SetModuleName() now auto-registers when in Async/MProc mode
        // 全局 SetModuleName() 现在在 Async/MProc 模式下会自动注册
        oneplog::SetModuleName(name);
        
        // Note: For MProc mode with shared memory, call RegisterThreadToSharedMemory()
        // 注意：对于使用共享内存的 MProc 模式，请调用 RegisterThreadToSharedMemory()
    }

    static std::string GetModuleName(uint32_t threadId = 0) {
        // For Sync mode or current thread: return thread_local
        if (s_mode == Mode::Sync || threadId == 0) {
            return oneplog::GetModuleName();
        }
        
        // For Async mode: lookup from global table
        if (s_mode == Mode::Async) {
            return LookupModuleName(threadId);
        }
        
        // For MProc consumer with non-zero threadId: return ID as string
        // (actual shared memory lookup should be done by the caller with SharedMemory instance)
        // 对于非零 threadId 的 MProc 消费者：返回 ID 字符串
        // （实际的共享内存查找应由调用者使用 SharedMemory 实例完成）
        if (s_mode == Mode::MProc && threadId != 0) {
            return std::to_string(threadId);
        }
        
        return "main";
    }

    // =========================================================================
    // Initialization / 初始化
    // =========================================================================

    static void Initialize(Mode mode) {
        s_mode = mode;
        s_initialized = true;
        
        // Enable auto-registration for Async/MProc modes
        // 为 Async/MProc 模式启用自动注册
        SetAutoRegisterModuleName(mode == Mode::Async || mode == Mode::MProc);
    }

    static void Shutdown() {
        s_initialized = false;
        SetAutoRegisterModuleName(false);
    }

    static bool IsInitialized() { return s_initialized; }
    static Mode GetMode() { return s_mode; }

    // =========================================================================
    // Platform Information / 平台信息
    // =========================================================================

    /**
     * @brief Check if running on Linux platform
     * @brief 检查是否在 Linux 平台上运行
     */
    static constexpr bool IsLinuxPlatform() noexcept {
        return internal::kIsLinuxPlatform;
    }

    /**
     * @brief Get lookup complexity description
     * @brief 获取查找复杂度描述
     */
    static constexpr const char* GetLookupComplexity() noexcept {
        return internal::GetLookupComplexity();
    }

private:
    NameManager() = delete;

    inline static Mode s_mode{Mode::Async};
    inline static bool s_initialized{false};
};

// ==============================================================================
// ThreadWithModuleName - Thread Creation Helper / 线程创建辅助类
// ==============================================================================

/**
 * @brief Thread creation helper that sets module name
 * @brief 设置模块名的线程创建辅助类
 *
 * @tparam EnableWFC Enable WFC support / 启用 WFC 支持
 */
template<bool EnableWFC = false>
class ThreadWithModuleName {
public:
    /**
     * @brief Create thread that inherits parent's module name
     * @brief 创建继承父线程模块名的线程
     *
     * Note: This version only supports functions without arguments.
     * For functions with arguments, use CreateWithName or wrap in a lambda.
     * 注意：此版本仅支持无参数函数。
     * 对于有参数的函数，请使用 CreateWithName 或包装在 lambda 中。
     */
    template<typename Func>
    static std::thread Create(Func&& func) {
        std::string parentModuleName = GetModuleName();
        
        return std::thread([parentModuleName, f = std::forward<Func>(func)]() mutable {
            NameManager<EnableWFC>::SetModuleName(parentModuleName);
            f();
        });
    }

    /**
     * @brief Create thread with explicit module name
     * @brief 创建具有指定模块名的线程
     *
     * Note: This version only supports functions without arguments.
     * For functions with arguments, wrap in a lambda.
     * 注意：此版本仅支持无参数函数。
     * 对于有参数的函数，请包装在 lambda 中。
     */
    template<typename Func>
    static std::thread CreateWithName(const std::string& moduleName, Func&& func) {
        return std::thread([moduleName, f = std::forward<Func>(func)]() mutable {
            NameManager<EnableWFC>::SetModuleName(moduleName);
            f();
        });
    }

private:
    ThreadWithModuleName() = delete;
};

}  // namespace oneplog
