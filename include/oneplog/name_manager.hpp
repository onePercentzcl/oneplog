/**
 * @file name_manager.hpp
 * @brief Process and module name management for onePlog
 * @brief onePlog 的进程名和模块名管理
 *
 * Design principles / 设计原则：
 * - Process name and module name belong to process/thread, NOT to Logger
 * - 进程名和模块名属于进程/线程，而不是 Logger 对象
 *
 * Storage mechanisms for different modes / 不同模式的存储机制：
 * - Sync: Process name is global constant, module name is thread_local
 *         Sink can directly access both names
 *         进程名为全局常量，模块名为线程局部变量，Sink 可直接访问
 *
 * - Async: Process name is global constant, module name is thread_local
 *          BUT WriterThread cannot see thread_local, so need TID-to-name table on heap
 *          进程名为全局常量，模块名为线程局部变量
 *          但 WriterThread 看不到 thread_local，所以需要堆上的 TID-模块名对照表
 *
 * - MProc: Process name is global constant, module name is thread_local
 *          Consumer process needs shared memory PID/TID-to-name table
 *          进程名为全局常量，模块名为线程局部变量
 *          消费者进程需要共享内存中的 PID/TID-名称对照表
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include "oneplog/common.hpp"

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
// Global Process Name / 全局进程名
// ==============================================================================

/**
 * @brief Global process name (compile-time or early runtime constant)
 * @brief 全局进程名（编译期或早期运行时常量）
 *
 * This should be set once at program startup and never changed.
 * 应该在程序启动时设置一次，之后不再改变。
 */
namespace detail {

inline std::string& GetGlobalProcessName() {
    static std::string processName = "main";
    return processName;
}

}  // namespace detail

/**
 * @brief Set global process name (call once at startup)
 * @brief 设置全局进程名（启动时调用一次）
 */
inline void SetProcessName(const std::string& name) {
    detail::GetGlobalProcessName() = name;
}

/**
 * @brief Get global process name
 * @brief 获取全局进程名
 */
inline const std::string& GetProcessName() {
    return detail::GetGlobalProcessName();
}

// ==============================================================================
// Thread-local Module Name / 线程局部模块名
// ==============================================================================

namespace detail {

inline thread_local std::string tls_moduleName = "main";

inline uint32_t GetCurrentThreadIdInternal() {
#ifdef _WIN32
    return static_cast<uint32_t>(::GetCurrentThreadId());
#elif defined(__APPLE__)
    uint64_t tid;
    pthread_threadid_np(nullptr, &tid);
    return static_cast<uint32_t>(tid);
#else
    return static_cast<uint32_t>(pthread_self());
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

}  // namespace detail

// Forward declaration for RegisterModuleName
inline void RegisterModuleName();

/**
 * @brief Set module name for current thread
 * @brief 设置当前线程的模块名
 *
 * In Async/MProc mode, this automatically registers to the global table.
 * 在 Async/MProc 模式下，会自动注册到全局表。
 */
inline void SetModuleName(const std::string& name) {
    detail::tls_moduleName = name;
    
    // Auto-register to global table if enabled (Async/MProc mode)
    // 如果启用了自动注册（Async/MProc 模式），则自动注册到全局表
    if (detail::GetAutoRegisterFlag().load(std::memory_order_acquire)) {
        RegisterModuleName();
    }
}

/**
 * @brief Get module name for current thread
 * @brief 获取当前线程的模块名
 */
inline const std::string& GetModuleName() {
    return detail::tls_moduleName;
}

/**
 * @brief Enable/disable automatic registration to global table
 * @brief 启用/禁用自动注册到全局表
 *
 * Called by Logger::Init() based on mode.
 * 由 Logger::Init() 根据模式调用。
 */
inline void SetAutoRegisterModuleName(bool enable) {
    detail::GetAutoRegisterFlag().store(enable, std::memory_order_release);
}

// ==============================================================================
// ThreadModuleTable - TID to Module Name Mapping for Async Mode
// TID-模块名映射表，用于异步模式
// ==============================================================================

/**
 * @brief Thread-safe TID to module name mapping table for Async mode
 * @brief 用于 Async 模式的线程安全 TID 到模块名映射表
 *
 * In Async mode, WriterThread cannot access thread_local variables of other threads.
 * This table allows WriterThread to lookup module name by TID.
 *
 * 在异步模式下，WriterThread 无法访问其他线程的 thread_local 变量。
 * 此表允许 WriterThread 通过 TID 查找模块名。
 */
class ThreadModuleTable {
public:
    static constexpr size_t kMaxEntries = 256;
    static constexpr size_t kMaxNameLength = 31;

    ThreadModuleTable() { Clear(); }
    ~ThreadModuleTable() = default;

    ThreadModuleTable(const ThreadModuleTable&) = delete;
    ThreadModuleTable& operator=(const ThreadModuleTable&) = delete;
    ThreadModuleTable(ThreadModuleTable&&) = delete;
    ThreadModuleTable& operator=(ThreadModuleTable&&) = delete;

    /**
     * @brief Register or update module name for a thread
     * @brief 注册或更新线程的模块名
     */
    bool Register(uint32_t threadId, const std::string& name) {
        // First, try to find existing entry
        size_t count = m_count.load(std::memory_order_acquire);
        for (size_t i = 0; i < count && i < kMaxEntries; ++i) {
            if (m_entries[i].valid.load(std::memory_order_acquire) &&
                m_entries[i].threadId.load(std::memory_order_acquire) == threadId) {
                CopyName(m_entries[i].name, name);
                return true;
            }
        }

        // Add new entry
        size_t newIndex = m_count.fetch_add(1, std::memory_order_acq_rel);
        if (newIndex >= kMaxEntries) {
            m_count.fetch_sub(1, std::memory_order_relaxed);
            return false;
        }

        m_entries[newIndex].threadId.store(threadId, std::memory_order_relaxed);
        CopyName(m_entries[newIndex].name, name);
        m_entries[newIndex].valid.store(true, std::memory_order_release);
        return true;
    }

    /**
     * @brief Get module name by thread ID
     * @brief 通过线程 ID 获取模块名
     */
    std::string GetName(uint32_t threadId) const {
        size_t count = m_count.load(std::memory_order_acquire);
        for (size_t i = 0; i < count && i < kMaxEntries; ++i) {
            if (m_entries[i].valid.load(std::memory_order_acquire) &&
                m_entries[i].threadId.load(std::memory_order_acquire) == threadId) {
                return std::string(m_entries[i].name);
            }
        }
        return "";
    }

    void Clear() {
        for (size_t i = 0; i < kMaxEntries; ++i) {
            m_entries[i].valid.store(false, std::memory_order_relaxed);
            m_entries[i].threadId.store(0, std::memory_order_relaxed);
            std::memset(m_entries[i].name, 0, kMaxNameLength + 1);
        }
        m_count.store(0, std::memory_order_release);
    }

    size_t Count() const { return m_count.load(std::memory_order_acquire); }

private:
    static void CopyName(char* dest, const std::string& src) {
        size_t len = std::min(src.length(), kMaxNameLength);
        std::memcpy(dest, src.c_str(), len);
        dest[len] = '\0';
    }

    struct Entry {
        std::atomic<uint32_t> threadId{0};
        char name[kMaxNameLength + 1]{0};
        std::atomic<bool> valid{false};
    };

    alignas(64) Entry m_entries[kMaxEntries];
    std::atomic<size_t> m_count{0};
};

// ==============================================================================
// Global ThreadModuleTable for Async Mode / 异步模式的全局 TID-模块名表
// ==============================================================================

namespace detail {

inline ThreadModuleTable& GetGlobalThreadModuleTable() {
    static ThreadModuleTable table;
    return table;
}

}  // namespace detail

/**
 * @brief Register current thread's module name to global table (for Async mode)
 * @brief 将当前线程的模块名注册到全局表（用于异步模式）
 *
 * Call this after SetModuleName() in Async mode to make the name visible to WriterThread.
 * 在异步模式下，调用 SetModuleName() 后调用此函数，使模块名对 WriterThread 可见。
 */
inline void RegisterModuleName() {
    uint32_t tid = detail::GetCurrentThreadIdInternal();
    detail::GetGlobalThreadModuleTable().Register(tid, detail::tls_moduleName);
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
    std::string name = detail::GetGlobalThreadModuleTable().GetName(threadId);
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
 */
template<bool EnableWFC = false>
class NameManager {
public:
    static constexpr size_t kMaxNameLength = 31;

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
