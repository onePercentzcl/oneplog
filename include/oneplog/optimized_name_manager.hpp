/**
 * @file optimized_name_manager.hpp
 * @brief Optimized name manager with platform-specific lookup
 * @brief 带平台特定查找的优化名称管理器
 *
 * This file provides an optimized name manager that uses:
 * - Stack storage for process name (global) and module name (thread_local)
 * - Platform-specific heap lookup table for async mode
 * - Zero heap allocation in sync mode
 *
 * 此文件提供优化的名称管理器，使用：
 * - 进程名（全局）和模块名（thread_local）的栈存储
 * - 异步模式下的平台特定堆查找表
 * - 同步模式下零堆分配
 *
 * @copyright Copyright (c) 2024 onePlog
 *
 * _Requirements: 3.1, 3.2, 3.3, 3.4, 4.1, 4.2, 4.3, 4.4, 4.5, 7.1, 7.2, 7.3, 7.4_
 */

#pragma once

#include "oneplog/common.hpp"
#include "oneplog/internal/fixed_name.hpp"
#include "oneplog/internal/platform_lookup_table.hpp"

#include <atomic>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

namespace oneplog {

// ==============================================================================
// Internal Helper Functions / 内部辅助函数
// ==============================================================================

namespace optimized_detail {

/**
 * @brief Get current thread ID (platform-specific)
 * @brief 获取当前线程 ID（平台特定）
 */
inline uint32_t GetCurrentThreadId() noexcept {
#ifdef _WIN32
    return static_cast<uint32_t>(::GetCurrentThreadId());
#elif defined(__APPLE__)
    uint64_t tid;
    pthread_threadid_np(nullptr, &tid);
    return static_cast<uint32_t>(tid);
#else
    // Linux: use gettid() for actual thread ID
    // Linux：使用 gettid() 获取实际线程 ID
    return static_cast<uint32_t>(gettid());
#endif
}

}  // namespace optimized_detail

// ==============================================================================
// OptimizedNameManager - Optimized Name Manager with Platform-Specific Lookup
// 优化名称管理器，带平台特定查找
// ==============================================================================

/**
 * @brief Optimized name manager with platform-specific lookup
 * @brief 带平台特定查找的优化名称管理器
 *
 * Provides high-performance name management with:
 * - Stack storage for sync mode (zero heap allocation)
 * - Platform-specific heap lookup table for async mode
 * - Backward compatible API with existing NameManager
 *
 * 提供高性能名称管理：
 * - 同步模式的栈存储（零堆分配）
 * - 异步模式的平台特定堆查找表
 * - 与现有 NameManager 向后兼容的 API
 *
 * @tparam EnableWFC Enable WFC support / 启用 WFC 支持
 * @tparam MaxNameLen Maximum name length (user configurable, default: 31)
 *                    最大名称长度（用户可配置，默认：31）
 *
 * _Requirements: 3.1, 3.2, 3.3, 3.4, 4.1, 4.2, 4.3, 4.4, 4.5, 7.1, 7.2, 7.3, 7.4_
 */
template<bool EnableWFC = false, size_t MaxNameLen = 31>
class OptimizedNameManager {
public:
    /// Maximum name length (user configurable) / 最大名称长度（用户可配置）
    static constexpr size_t kMaxNameLength = MaxNameLen;

    /// Default process name / 默认进程名
    static constexpr const char* kDefaultProcessName = "main";

    /// Default module name / 默认模块名
    static constexpr const char* kDefaultModuleName = "main";

    // =========================================================================
    // Process Name (Global, Stack Storage) / 进程名（全局，栈存储）
    // =========================================================================

    /**
     * @brief Set global process name
     * @brief 设置全局进程名
     *
     * The name is stored in a global stack-allocated buffer.
     * Names longer than kMaxNameLength are truncated.
     *
     * 名称存储在全局栈分配的缓冲区中。
     * 超过 kMaxNameLength 的名称会被截断。
     *
     * @param name Process name / 进程名
     *
     * _Requirements: 4.1, 7.2_
     */
    static void SetProcessName(std::string_view name) noexcept {
        GetProcessNameStorage().Assign(name);
    }

    /**
     * @brief Get global process name
     * @brief 获取全局进程名
     *
     * @return Process name as string_view / 进程名的 string_view
     *
     * _Requirements: 4.2_
     */
    static std::string_view GetProcessName() noexcept {
        const auto& storage = GetProcessNameStorage();
        if (storage.Empty()) {
            return kDefaultProcessName;
        }
        return storage.View();
    }

    /**
     * @brief Get global process name (with optional processId for compatibility)
     * @brief 获取全局进程名（带可选 processId，用于兼容性）
     *
     * @param processId Process ID (ignored, for API compatibility)
     *                  进程 ID（忽略，用于 API 兼容性）
     * @return Process name as std::string / 进程名的 std::string
     *
     * _Requirements: 7.3_
     */
    static std::string GetProcessName(uint32_t processId) {
        if (processId == 0) {
            return std::string(GetProcessName());
        }
        // For MProc consumer with non-zero processId: return ID as string
        // 对于非零 processId 的 MProc 消费者：返回 ID 字符串
        return std::to_string(processId);
    }

    // =========================================================================
    // Module Name (Thread-local + Heap Table for Async) / 模块名
    // =========================================================================

    /**
     * @brief Set module name for current thread
     * @brief 设置当前线程的模块名
     *
     * In Sync mode: Only updates thread_local storage (stack).
     * In Async mode: Updates both thread_local storage and heap lookup table.
     *
     * 同步模式：仅更新 thread_local 存储（栈）。
     * 异步模式：同时更新 thread_local 存储和堆查找表。
     *
     * @param name Module name / 模块名
     *
     * _Requirements: 3.1, 4.3, 7.2_
     */
    static void SetModuleName(std::string_view name) noexcept {
        // Always update thread_local storage / 始终更新 thread_local 存储
        GetModuleNameStorage().Assign(name);

        // In Async/MProc mode, also register to heap lookup table
        // 在 Async/MProc 模式下，同时注册到堆查找表
        if (s_autoRegister.load(std::memory_order_acquire)) {
            RegisterModuleName();
        }
    }

    /**
     * @brief Get module name for current thread (Sync mode)
     * @brief 获取当前线程的模块名（同步模式）
     *
     * Returns the thread_local module name directly.
     * 直接返回 thread_local 模块名。
     *
     * @return Module name as string_view / 模块名的 string_view
     *
     * _Requirements: 3.2, 3.4_
     */
    static std::string_view GetModuleName() noexcept {
        const auto& storage = GetModuleNameStorage();
        if (storage.Empty()) {
            return kDefaultModuleName;
        }
        return storage.View();
    }

    /**
     * @brief Get module name (with optional threadId for compatibility)
     * @brief 获取模块名（带可选 threadId，用于兼容性）
     *
     * @param threadId Thread ID (0 = current thread)
     *                 线程 ID（0 = 当前线程）
     * @return Module name as std::string / 模块名的 std::string
     *
     * _Requirements: 7.3_
     */
    static std::string GetModuleName(uint32_t threadId) {
        // For current thread or Sync mode: return thread_local
        // 对于当前线程或同步模式：返回 thread_local
        if (threadId == 0 || s_mode == Mode::Sync) {
            return std::string(GetModuleName());
        }

        // For Async mode: lookup from heap table
        // 对于异步模式：从堆表查找
        if (s_mode == Mode::Async) {
            return std::string(LookupModuleName(threadId));
        }

        // For MProc consumer with non-zero threadId: return ID as string
        // 对于非零 threadId 的 MProc 消费者：返回 ID 字符串
        if (s_mode == Mode::MProc && threadId != 0) {
            return std::to_string(threadId);
        }

        return std::string(kDefaultModuleName);
    }

    /**
     * @brief Lookup module name by TID (Async mode, for WriterThread)
     * @brief 通过 TID 查找模块名（异步模式，供 WriterThread 使用）
     *
     * Uses platform-specific lookup table:
     * - Linux: O(1) direct mapping
     * - Non-Linux: O(n) linear search
     *
     * 使用平台特定查找表：
     * - Linux：O(1) 直接映射
     * - 非 Linux：O(n) 线性搜索
     *
     * @param tid Thread ID / 线程 ID
     * @return Module name or default if not found / 模块名，未找到则返回默认值
     *
     * _Requirements: 4.4, 4.5_
     */
    static std::string_view LookupModuleName(uint32_t tid) noexcept {
        return GetLookupTable().GetName(tid);
    }

    // =========================================================================
    // Registration (for Async mode) / 注册（用于异步模式）
    // =========================================================================

    /**
     * @brief Register current thread's module name to heap lookup table
     * @brief 将当前线程的模块名注册到堆查找表
     *
     * Called automatically by SetModuleName() in Async/MProc mode.
     * 在 Async/MProc 模式下由 SetModuleName() 自动调用。
     *
     * _Requirements: 4.3_
     */
    static void RegisterModuleName() noexcept {
        uint32_t tid = optimized_detail::GetCurrentThreadId();
        GetLookupTable().Register(tid, GetModuleNameStorage().View());
    }

    /**
     * @brief Set and register module name (for backward compatibility)
     * @brief 设置并注册模块名（用于向后兼容）
     *
     * @deprecated Use SetModuleName() instead, which auto-registers in Async mode.
     * @deprecated 请使用 SetModuleName()，它在异步模式下会自动注册。
     *
     * @param name Module name / 模块名
     *
     * _Requirements: 7.1_
     */
    static void SetAndRegisterModuleName(const std::string& name) noexcept {
        SetModuleName(name);
        // RegisterModuleName() is called automatically by SetModuleName()
        // when auto-register is enabled
    }

    // =========================================================================
    // Initialization / 初始化
    // =========================================================================

    /**
     * @brief Initialize the name manager with specified mode
     * @brief 使用指定模式初始化名称管理器
     *
     * @param mode Operating mode (Sync/Async/MProc)
     *             操作模式（Sync/Async/MProc）
     */
    static void Initialize(Mode mode) noexcept {
        s_mode = mode;
        s_initialized.store(true, std::memory_order_release);

        // Enable auto-registration for Async/MProc modes
        // 为 Async/MProc 模式启用自动注册
        s_autoRegister.store(mode == Mode::Async || mode == Mode::MProc,
                             std::memory_order_release);
    }

    /**
     * @brief Shutdown the name manager
     * @brief 关闭名称管理器
     */
    static void Shutdown() noexcept {
        s_initialized.store(false, std::memory_order_release);
        s_autoRegister.store(false, std::memory_order_release);
    }

    /**
     * @brief Check if the name manager is initialized
     * @brief 检查名称管理器是否已初始化
     *
     * @return true if initialized / 已初始化返回 true
     */
    static bool IsInitialized() noexcept {
        return s_initialized.load(std::memory_order_acquire);
    }

    /**
     * @brief Get current operating mode
     * @brief 获取当前操作模式
     *
     * @return Current mode / 当前模式
     */
    static Mode GetMode() noexcept {
        return s_mode;
    }

    /**
     * @brief Enable/disable automatic registration to heap table
     * @brief 启用/禁用自动注册到堆表
     *
     * @param enable true to enable, false to disable
     *               true 启用，false 禁用
     *
     * _Requirements: 7.1_
     */
    static void SetAutoRegisterModuleName(bool enable) noexcept {
        s_autoRegister.store(enable, std::memory_order_release);
    }

    // =========================================================================
    // Utility Functions / 实用函数
    // =========================================================================

    /**
     * @brief Get current thread ID
     * @brief 获取当前线程 ID
     *
     * @return Current thread ID / 当前线程 ID
     */
    static uint32_t GetCurrentThreadId() noexcept {
        return optimized_detail::GetCurrentThreadId();
    }

    /**
     * @brief Clear the heap lookup table
     * @brief 清空堆查找表
     *
     * Useful for testing or resetting state.
     * 用于测试或重置状态。
     */
    static void ClearLookupTable() noexcept {
        GetLookupTable().Clear();
    }

    /**
     * @brief Get number of entries in the heap lookup table
     * @brief 获取堆查找表中的条目数
     *
     * @return Number of registered entries / 已注册条目数
     */
    static size_t GetLookupTableCount() noexcept {
        return GetLookupTable().Count();
    }

private:
    // =========================================================================
    // Private Storage Accessors / 私有存储访问器
    // =========================================================================

    /**
     * @brief Get global process name storage (stack-allocated)
     * @brief 获取全局进程名存储（栈分配）
     *
     * Uses a static local variable to ensure proper initialization.
     * 使用静态局部变量确保正确初始化。
     *
     * _Requirements: 4.1_
     */
    static internal::FixedName<MaxNameLen>& GetProcessNameStorage() noexcept {
        static internal::FixedName<MaxNameLen> storage{kDefaultProcessName};
        return storage;
    }

    /**
     * @brief Get thread-local module name storage (stack-allocated)
     * @brief 获取线程局部模块名存储（栈分配）
     *
     * Each thread has its own storage on the stack.
     * 每个线程在栈上有自己的存储。
     *
     * _Requirements: 3.1_
     */
    static internal::FixedName<MaxNameLen>& GetModuleNameStorage() noexcept {
        thread_local internal::FixedName<MaxNameLen> storage{kDefaultModuleName};
        return storage;
    }

    /**
     * @brief Get heap lookup table (platform-specific)
     * @brief 获取堆查找表（平台特定）
     *
     * Uses platform-specific implementation:
     * - Linux: DirectMappingTable with O(1) lookup
     * - Non-Linux: ArrayMappingTable with O(n) lookup
     *
     * 使用平台特定实现：
     * - Linux：O(1) 查找的 DirectMappingTable
     * - 非 Linux：O(n) 查找的 ArrayMappingTable
     *
     * _Requirements: 4.5_
     */
    static typename internal::PlatformLookupTableSelector<MaxNameLen>::Type&
    GetLookupTable() noexcept {
        static typename internal::PlatformLookupTableSelector<MaxNameLen>::Type table;
        return table;
    }

    // =========================================================================
    // Private Static Members / 私有静态成员
    // =========================================================================

    /// Current operating mode / 当前操作模式
    inline static Mode s_mode{Mode::Async};

    /// Initialization flag / 初始化标志
    inline static std::atomic<bool> s_initialized{false};

    /// Auto-register flag for Async/MProc modes / Async/MProc 模式的自动注册标志
    inline static std::atomic<bool> s_autoRegister{false};

    // Delete constructor to prevent instantiation
    // 删除构造函数以防止实例化
    OptimizedNameManager() = delete;
};

// ==============================================================================
// Type Aliases / 类型别名
// ==============================================================================

/**
 * @brief Default OptimizedNameManager with standard settings
 * @brief 使用标准设置的默认 OptimizedNameManager
 */
using DefaultOptimizedNameManager = OptimizedNameManager<false, 31>;

/**
 * @brief OptimizedNameManager with WFC support
 * @brief 带 WFC 支持的 OptimizedNameManager
 */
using WFCOptimizedNameManager = OptimizedNameManager<true, 31>;

// ==============================================================================
// OptimizedThreadWithModuleName - Thread Creation Helper
// 优化的线程创建辅助类
// ==============================================================================

/**
 * @brief Thread creation helper that sets module name using OptimizedNameManager
 * @brief 使用 OptimizedNameManager 设置模块名的线程创建辅助类
 *
 * @tparam EnableWFC Enable WFC support / 启用 WFC 支持
 * @tparam MaxNameLen Maximum name length / 最大名称长度
 */
template<bool EnableWFC = false, size_t MaxNameLen = 31>
class OptimizedThreadWithModuleName {
public:
    using NameManagerType = OptimizedNameManager<EnableWFC, MaxNameLen>;

    /**
     * @brief Create thread that inherits parent's module name
     * @brief 创建继承父线程模块名的线程
     *
     * @tparam Func Function type / 函数类型
     * @param func Function to execute / 要执行的函数
     * @return Created thread / 创建的线程
     */
    template<typename Func>
    static std::thread Create(Func&& func) {
        std::string parentModuleName{NameManagerType::GetModuleName()};

        return std::thread([parentModuleName, f = std::forward<Func>(func)]() mutable {
            NameManagerType::SetModuleName(parentModuleName);
            f();
        });
    }

    /**
     * @brief Create thread with explicit module name
     * @brief 创建具有指定模块名的线程
     *
     * @tparam Func Function type / 函数类型
     * @param moduleName Module name for the new thread / 新线程的模块名
     * @param func Function to execute / 要执行的函数
     * @return Created thread / 创建的线程
     */
    template<typename Func>
    static std::thread CreateWithName(const std::string& moduleName, Func&& func) {
        return std::thread([moduleName, f = std::forward<Func>(func)]() mutable {
            NameManagerType::SetModuleName(moduleName);
            f();
        });
    }

private:
    OptimizedThreadWithModuleName() = delete;
};

}  // namespace oneplog
