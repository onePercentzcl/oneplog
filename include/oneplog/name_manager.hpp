/**
 * @file name_manager.hpp
 * @brief Process and module name management for onePlog
 * @brief onePlog 的进程名和模块名管理
 *
 * Provides name storage and lookup mechanisms for three operating modes:
 * 为三种运行模式提供名称存储和查找机制：
 * - Sync: thread_local storage for both process and module names
 * - Async: global process name + heap-based TID-to-module-name table
 * - MProc: shared memory ProcessThreadNameTable
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include "oneplog/common.hpp"
#include "oneplog/shared_memory.hpp"

#include <atomic>
#include <cstring>
#include <memory>
#include <string>
#include <thread>
#include <tuple>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

namespace oneplog {

// ==============================================================================
// ThreadModuleTable - Thread-safe TID to Module Name Mapping / 线程安全的 TID-模块名映射表
// ==============================================================================

/**
 * @brief Thread-safe TID to module name mapping table for Async mode
 * @brief 用于 Async 模式的线程安全 TID 到模块名映射表
 *
 * Uses lock-free atomic operations for thread safety.
 * 使用无锁原子操作确保线程安全。
 */
class ThreadModuleTable {
public:
    static constexpr size_t kMaxEntries = 256;      ///< Max entries / 最大条目数
    static constexpr size_t kMaxNameLength = 31;    ///< Max name length / 最大名称长度

    /**
     * @brief Construct a new ThreadModuleTable
     * @brief 构造新的 ThreadModuleTable
     */
    ThreadModuleTable() {
        Clear();
    }

    ~ThreadModuleTable() = default;

    // Non-copyable, non-movable
    ThreadModuleTable(const ThreadModuleTable&) = delete;
    ThreadModuleTable& operator=(const ThreadModuleTable&) = delete;
    ThreadModuleTable(ThreadModuleTable&&) = delete;
    ThreadModuleTable& operator=(ThreadModuleTable&&) = delete;

    /**
     * @brief Register or update module name for a thread
     * @brief 注册或更新线程的模块名
     *
     * @param threadId Thread ID / 线程 ID
     * @param name Module name (will be truncated to 31 chars) / 模块名（超过31字符会被截断）
     * @return true if successful, false if table is full / 成功返回 true，表满返回 false
     */
    bool Register(uint32_t threadId, const std::string& name) {
        // First, check if this thread is already registered
        // 首先检查该线程是否已注册
        size_t count = m_count.load(std::memory_order_acquire);
        for (size_t i = 0; i < count && i < kMaxEntries; ++i) {
            if (m_entries[i].valid.load(std::memory_order_acquire) &&
                m_entries[i].threadId.load(std::memory_order_acquire) == threadId) {
                // Update existing entry / 更新现有条目
                CopyName(m_entries[i].name, name);
                return true;
            }
        }

        // Find an empty slot or add new entry
        // 查找空槽位或添加新条目
        size_t newIndex = m_count.fetch_add(1, std::memory_order_acq_rel);
        if (newIndex >= kMaxEntries) {
            m_count.fetch_sub(1, std::memory_order_relaxed);
            return false;  // Table full / 表已满
        }

        // Initialize the new entry / 初始化新条目
        m_entries[newIndex].threadId.store(threadId, std::memory_order_relaxed);
        CopyName(m_entries[newIndex].name, name);
        m_entries[newIndex].valid.store(true, std::memory_order_release);

        return true;
    }

    /**
     * @brief Get module name by thread ID
     * @brief 通过线程 ID 获取模块名
     *
     * @param threadId Thread ID / 线程 ID
     * @return Module name, or empty string if not found / 模块名，未找到返回空字符串
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

    /**
     * @brief Clear all entries
     * @brief 清空所有条目
     */
    void Clear() {
        for (size_t i = 0; i < kMaxEntries; ++i) {
            m_entries[i].valid.store(false, std::memory_order_relaxed);
            m_entries[i].threadId.store(0, std::memory_order_relaxed);
            std::memset(m_entries[i].name, 0, kMaxNameLength + 1);
        }
        m_count.store(0, std::memory_order_release);
    }

    /**
     * @brief Get current entry count
     * @brief 获取当前条目数
     */
    size_t Count() const {
        return m_count.load(std::memory_order_acquire);
    }

private:
    /**
     * @brief Copy name with truncation
     * @brief 复制名称并截断
     */
    static void CopyName(char* dest, const std::string& src) {
        size_t len = std::min(src.length(), kMaxNameLength);
        std::memcpy(dest, src.c_str(), len);
        dest[len] = '\0';
    }

    /**
     * @brief Entry structure for TID-name mapping
     * @brief TID-名称映射的条目结构
     */
    struct Entry {
        std::atomic<uint32_t> threadId{0};          ///< Thread ID / 线程 ID
        char name[kMaxNameLength + 1]{0};           ///< Module name / 模块名
        std::atomic<bool> valid{false};             ///< Entry validity / 条目有效性
    };

    alignas(64) Entry m_entries[kMaxEntries];       ///< Entry array / 条目数组
    std::atomic<size_t> m_count{0};                 ///< Current count / 当前计数
};

// ==============================================================================
// NameManagerState - Global State for Name Management / 名称管理的全局状态
// ==============================================================================

namespace detail {

/**
 * @brief Global state for name management
 * @brief 名称管理的全局状态
 */
struct NameManagerState {
    Mode mode{Mode::Async};                                 ///< Operating mode / 运行模式
    SharedMemory* sharedMemory{nullptr};                    ///< Shared memory pointer / 共享内存指针
    std::unique_ptr<ThreadModuleTable> threadModuleTable;   ///< Heap table for Async mode / Async 模式的堆表
    std::string globalProcessName{"main"};                  ///< Global process name / 全局进程名
    uint32_t registeredProcessId{0};                        ///< Registered process ID / 注册的进程 ID
    std::atomic<bool> initialized{false};                   ///< Initialization flag / 初始化标志
};

/**
 * @brief Get the global NameManagerState instance
 * @brief 获取全局 NameManagerState 实例
 */
inline NameManagerState& GetNameManagerState() {
    static NameManagerState state;
    return state;
}

// Thread-local storage for Sync mode
// Sync 模式的线程局部存储
inline thread_local std::string tls_processName = "main";
inline thread_local std::string tls_moduleName = "main";
inline thread_local uint32_t tls_registeredThreadId = 0;
inline thread_local bool tls_moduleNameInitialized = false;

/**
 * @brief Get current thread ID (platform-specific)
 * @brief 获取当前线程 ID（平台相关）
 */
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

}  // namespace detail

// ==============================================================================
// NameManager - Name Manager Static Class / 名称管理器静态类
// ==============================================================================

/**
 * @brief Name manager for process and module names
 * @brief 进程名和模块名管理器
 *
 * Provides unified interface for name storage and lookup across all modes.
 * 为所有模式提供统一的名称存储和查找接口。
 */
class NameManager {
public:
    static constexpr size_t kMaxNameLength = 31;    ///< Max name length / 最大名称长度

    /**
     * @brief Initialize the name manager
     * @brief 初始化名称管理器
     *
     * @param mode Operating mode / 运行模式
     * @param sharedMemory Shared memory pointer (for MProc mode) / 共享内存指针（多进程模式）
     */
    static void Initialize(Mode mode, SharedMemory* sharedMemory = nullptr) {
        auto& state = detail::GetNameManagerState();
        
        if (state.initialized.load(std::memory_order_acquire)) {
            return;  // Already initialized / 已经初始化
        }

        state.mode = mode;
        state.sharedMemory = sharedMemory;
        state.globalProcessName = "main";
        state.registeredProcessId = 0;

        // Create heap table for Async mode / 为 Async 模式创建堆表
        if (mode == Mode::Async) {
            state.threadModuleTable = std::make_unique<ThreadModuleTable>();
        } else {
            state.threadModuleTable.reset();
        }

        state.initialized.store(true, std::memory_order_release);
    }

    /**
     * @brief Shutdown and cleanup
     * @brief 关闭并清理资源
     */
    static void Shutdown() {
        auto& state = detail::GetNameManagerState();
        
        if (!state.initialized.load(std::memory_order_acquire)) {
            return;
        }

        state.threadModuleTable.reset();
        state.sharedMemory = nullptr;
        state.globalProcessName = "main";
        state.registeredProcessId = 0;
        state.initialized.store(false, std::memory_order_release);
    }

    /**
     * @brief Set process name
     * @brief 设置进程名
     *
     * @param name Process name (will be truncated to 31 chars) / 进程名（超过31字符会被截断）
     */
    static void SetProcessName(const std::string& name) {
        auto& state = detail::GetNameManagerState();
        std::string truncatedName = TruncateName(name);

        switch (state.mode) {
            case Mode::Sync:
                // Sync mode: store in thread_local / 同步模式：存储在 thread_local
                detail::tls_processName = truncatedName;
                break;

            case Mode::Async:
                // Async mode: store in global variable / 异步模式：存储在全局变量
                state.globalProcessName = truncatedName;
                break;

            case Mode::MProc:
                // MProc mode: register in shared memory / 多进程模式：在共享内存中注册
                state.globalProcessName = truncatedName;
                if (state.sharedMemory) {
                    state.registeredProcessId = state.sharedMemory->RegisterProcess(truncatedName);
                }
                break;
        }
    }

    /**
     * @brief Get process name by ID
     * @brief 通过 ID 获取进程名
     *
     * @param processId Process ID (used in MProc mode, 0 for current) / 进程 ID（多进程模式使用，0 表示当前）
     * @return Process name / 进程名
     */
    static std::string GetProcessName(uint32_t processId = 0) {
        auto& state = detail::GetNameManagerState();

        switch (state.mode) {
            case Mode::Sync:
                // Sync mode: return from thread_local / 同步模式：从 thread_local 返回
                return detail::tls_processName;

            case Mode::Async:
                // Async mode: return global process name / 异步模式：返回全局进程名
                return state.globalProcessName;

            case Mode::MProc:
                // MProc mode: lookup in shared memory / 多进程模式：在共享内存中查找
                if (processId == 0) {
                    return state.globalProcessName;
                }
                if (state.sharedMemory) {
                    const char* name = state.sharedMemory->GetProcessName(processId);
                    if (name && name[0] != '\0') {
                        return std::string(name);
                    }
                }
                // Fallback to numeric ID / 回退到数字 ID
                return std::to_string(processId);
        }

        return "main";
    }

    /**
     * @brief Set module name for current thread
     * @brief 设置当前线程的模块名
     *
     * @param name Module name (will be truncated to 31 chars) / 模块名（超过31字符会被截断）
     */
    static void SetModuleName(const std::string& name) {
        auto& state = detail::GetNameManagerState();
        std::string truncatedName = TruncateName(name);

        switch (state.mode) {
            case Mode::Sync:
                // Sync mode: store in thread_local / 同步模式：存储在 thread_local
                detail::tls_moduleName = truncatedName;
                detail::tls_moduleNameInitialized = true;
                break;

            case Mode::Async:
                // Async mode: register in heap table / 异步模式：在堆表中注册
                if (state.threadModuleTable) {
                    uint32_t tid = detail::GetCurrentThreadIdInternal();
                    state.threadModuleTable->Register(tid, truncatedName);
                }
                break;

            case Mode::MProc:
                // MProc mode: register in shared memory / 多进程模式：在共享内存中注册
                if (state.sharedMemory) {
                    uint32_t threadId = state.sharedMemory->RegisterThread(truncatedName);
                    detail::tls_registeredThreadId = threadId;
                }
                detail::tls_moduleName = truncatedName;
                break;
        }
    }

    /**
     * @brief Get module name by thread ID
     * @brief 通过线程 ID 获取模块名
     *
     * @param threadId Thread ID (0 for current thread) / 线程 ID（0 表示当前线程）
     * @return Module name / 模块名
     */
    static std::string GetModuleName(uint32_t threadId = 0) {
        auto& state = detail::GetNameManagerState();

        switch (state.mode) {
            case Mode::Sync:
                // Sync mode: return from thread_local / 同步模式：从 thread_local 返回
                return detail::tls_moduleName;

            case Mode::Async:
                // Async mode: lookup in heap table / 异步模式：在堆表中查找
                if (state.threadModuleTable) {
                    uint32_t tid = (threadId == 0) ? detail::GetCurrentThreadIdInternal() : threadId;
                    std::string name = state.threadModuleTable->GetName(tid);
                    if (!name.empty()) {
                        return name;
                    }
                }
                return "main";  // Default / 默认值

            case Mode::MProc:
                // MProc mode: lookup in shared memory / 多进程模式：在共享内存中查找
                if (threadId == 0) {
                    // Return current thread's module name / 返回当前线程的模块名
                    return detail::tls_moduleName;
                }
                if (state.sharedMemory) {
                    const char* name = state.sharedMemory->GetThreadName(threadId);
                    if (name && name[0] != '\0') {
                        return std::string(name);
                    }
                }
                // Fallback to numeric ID / 回退到数字 ID
                return std::to_string(threadId);
        }

        return "main";
    }

    /**
     * @brief Get current operating mode
     * @brief 获取当前运行模式
     */
    static Mode GetMode() {
        return detail::GetNameManagerState().mode;
    }

    /**
     * @brief Get registered process ID (for MProc mode)
     * @brief 获取注册的进程 ID（多进程模式）
     */
    static uint32_t GetRegisteredProcessId() {
        return detail::GetNameManagerState().registeredProcessId;
    }

    /**
     * @brief Get registered thread ID for current thread (for MProc mode)
     * @brief 获取当前线程注册的线程 ID（多进程模式）
     */
    static uint32_t GetRegisteredThreadId() {
        return detail::tls_registeredThreadId;
    }

    /**
     * @brief Check if name manager is initialized
     * @brief 检查名称管理器是否已初始化
     */
    static bool IsInitialized() {
        return detail::GetNameManagerState().initialized.load(std::memory_order_acquire);
    }

private:
    NameManager() = delete;

    /**
     * @brief Truncate name to max length
     * @brief 截断名称到最大长度
     */
    static std::string TruncateName(const std::string& name) {
        if (name.length() <= kMaxNameLength) {
            return name;
        }
        return name.substr(0, kMaxNameLength);
    }
};

// ==============================================================================
// ThreadWithModuleName - Thread Creation Helper with Module Name Inheritance
// 带模块名继承的线程创建辅助类
// ==============================================================================

/**
 * @brief Thread creation helper that inherits module name from parent thread
 * @brief 继承父线程模块名的线程创建辅助类
 *
 * Usage / 用法:
 * @code
 * NameManager::SetModuleName("parent");
 * auto thread = ThreadWithModuleName::Create([]() {
 *     // GetModuleName() returns "parent" here
 *     // 这里 GetModuleName() 返回 "parent"
 * });
 * @endcode
 */
class ThreadWithModuleName {
public:
    /**
     * @brief Create a thread that inherits the parent thread's module name
     * @brief 创建一个继承父线程模块名的线程
     *
     * @tparam Func Callable type / 可调用类型
     * @tparam Args Argument types / 参数类型
     * @param func Function to execute / 要执行的函数
     * @param args Arguments to pass to the function / 传递给函数的参数
     * @return std::thread The created thread / 创建的线程
     */
    template<typename Func, typename... Args>
    static std::thread Create(Func&& func, Args&&... args) {
        // Capture the parent thread's module name
        // 捕获父线程的模块名
        std::string parentModuleName = NameManager::GetModuleName();
        
        // Use tuple to capture arguments (C++17 compatible)
        // 使用 tuple 捕获参数（C++17 兼容）
        auto argsTuple = std::make_tuple(std::forward<Args>(args)...);
        
        return std::thread([parentModuleName, 
                           f = std::forward<Func>(func),
                           argsTuple = std::move(argsTuple)]() mutable {
            // Set the inherited module name in the new thread
            // 在新线程中设置继承的模块名
            if (!detail::tls_moduleNameInitialized) {
                NameManager::SetModuleName(parentModuleName);
            }
            // Execute the user function with unpacked arguments
            // 使用解包的参数执行用户函数
            std::apply(f, std::move(argsTuple));
        });
    }

    /**
     * @brief Create a thread with a specific module name
     * @brief 创建一个具有特定模块名的线程
     *
     * @tparam Func Callable type / 可调用类型
     * @tparam Args Argument types / 参数类型
     * @param moduleName Module name for the new thread / 新线程的模块名
     * @param func Function to execute / 要执行的函数
     * @param args Arguments to pass to the function / 传递给函数的参数
     * @return std::thread The created thread / 创建的线程
     */
    template<typename Func, typename... Args>
    static std::thread CreateWithName(const std::string& moduleName, Func&& func, Args&&... args) {
        // Use tuple to capture arguments (C++17 compatible)
        // 使用 tuple 捕获参数（C++17 兼容）
        auto argsTuple = std::make_tuple(std::forward<Args>(args)...);
        
        return std::thread([moduleName,
                           f = std::forward<Func>(func),
                           argsTuple = std::move(argsTuple)]() mutable {
            // Set the specified module name in the new thread
            // 在新线程中设置指定的模块名
            NameManager::SetModuleName(moduleName);
            // Execute the user function with unpacked arguments
            // 使用解包的参数执行用户函数
            std::apply(f, std::move(argsTuple));
        });
    }

private:
    ThreadWithModuleName() = delete;
};

// ==============================================================================
// Global Convenience Functions / 全局便捷函数
// ==============================================================================

/**
 * @brief Initialize name manager (called by Logger::Init)
 * @brief 初始化名称管理器（由 Logger::Init 调用）
 */
inline void InitNameManager(Mode mode, SharedMemory* sharedMemory = nullptr) {
    NameManager::Initialize(mode, sharedMemory);
}

/**
 * @brief Shutdown name manager (called by Logger::Shutdown)
 * @brief 关闭名称管理器（由 Logger::Shutdown 调用）
 */
inline void ShutdownNameManager() {
    NameManager::Shutdown();
}

}  // namespace oneplog
