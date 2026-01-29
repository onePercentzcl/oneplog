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

    bool Register(uint32_t threadId, const std::string& name) {
        size_t count = m_count.load(std::memory_order_acquire);
        for (size_t i = 0; i < count && i < kMaxEntries; ++i) {
            if (m_entries[i].valid.load(std::memory_order_acquire) &&
                m_entries[i].threadId.load(std::memory_order_acquire) == threadId) {
                CopyName(m_entries[i].name, name);
                return true;
            }
        }

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
// NameManagerState - Global State for Name Management / 名称管理的全局状态
// ==============================================================================

namespace detail {

// Thread-local storage / 线程局部存储
inline thread_local std::string tls_processName = "main";
inline thread_local std::string tls_moduleName = "main";
inline thread_local uint32_t tls_registeredThreadId = 0;
inline thread_local bool tls_moduleNameInitialized = false;

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
 * @brief Global state for name management (template version)
 * @brief 名称管理的全局状态（模板版本）
 */
template<bool EnableWFC>
struct NameManagerState {
    Mode mode{Mode::Async};
    SharedMemory<EnableWFC>* sharedMemory{nullptr};
    std::unique_ptr<ThreadModuleTable> threadModuleTable;
    std::string globalProcessName{"main"};
    uint32_t registeredProcessId{0};
    std::atomic<bool> initialized{false};
};

template<bool EnableWFC>
inline NameManagerState<EnableWFC>& GetNameManagerState() {
    static NameManagerState<EnableWFC> state;
    return state;
}

}  // namespace detail

// ==============================================================================
// NameManager - Name Manager Template Class / 名称管理器模板类
// ==============================================================================

/**
 * @brief Name manager for process and module names
 * @brief 进程名和模块名管理器
 *
 * @tparam EnableWFC Enable WFC support / 启用 WFC 支持
 */
template<bool EnableWFC = false>
class NameManager {
public:
    static constexpr size_t kMaxNameLength = 31;

    /**
     * @brief Initialize the name manager
     * @brief 初始化名称管理器
     */
    static void Initialize(Mode mode, SharedMemory<EnableWFC>* sharedMemory = nullptr) {
        auto& state = detail::GetNameManagerState<EnableWFC>();
        
        if (state.initialized.load(std::memory_order_acquire)) {
            return;
        }

        state.mode = mode;
        state.sharedMemory = sharedMemory;
        state.globalProcessName = "main";
        state.registeredProcessId = 0;

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
        auto& state = detail::GetNameManagerState<EnableWFC>();
        
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
     */
    static void SetProcessName(const std::string& name) {
        auto& state = detail::GetNameManagerState<EnableWFC>();
        std::string truncatedName = TruncateName(name);

        switch (state.mode) {
            case Mode::Sync:
                detail::tls_processName = truncatedName;
                break;

            case Mode::Async:
                state.globalProcessName = truncatedName;
                break;

            case Mode::MProc:
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
     */
    static std::string GetProcessName(uint32_t processId = 0) {
        auto& state = detail::GetNameManagerState<EnableWFC>();

        switch (state.mode) {
            case Mode::Sync:
                return detail::tls_processName;

            case Mode::Async:
                return state.globalProcessName;

            case Mode::MProc:
                if (processId == 0) {
                    return state.globalProcessName;
                }
                if (state.sharedMemory) {
                    const char* name = state.sharedMemory->GetProcessName(processId);
                    if (name && name[0] != '\0') {
                        return std::string(name);
                    }
                }
                return std::to_string(processId);
        }

        return "main";
    }

    /**
     * @brief Set module name for current thread
     * @brief 设置当前线程的模块名
     */
    static void SetModuleName(const std::string& name) {
        auto& state = detail::GetNameManagerState<EnableWFC>();
        std::string truncatedName = TruncateName(name);

        switch (state.mode) {
            case Mode::Sync:
                detail::tls_moduleName = truncatedName;
                detail::tls_moduleNameInitialized = true;
                break;

            case Mode::Async:
                if (state.threadModuleTable) {
                    uint32_t tid = detail::GetCurrentThreadIdInternal();
                    state.threadModuleTable->Register(tid, truncatedName);
                }
                break;

            case Mode::MProc:
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
     */
    static std::string GetModuleName(uint32_t threadId = 0) {
        auto& state = detail::GetNameManagerState<EnableWFC>();

        switch (state.mode) {
            case Mode::Sync:
                return detail::tls_moduleName;

            case Mode::Async:
                if (state.threadModuleTable) {
                    uint32_t tid = (threadId == 0) ? detail::GetCurrentThreadIdInternal() : threadId;
                    std::string name = state.threadModuleTable->GetName(tid);
                    if (!name.empty()) {
                        return name;
                    }
                }
                return "main";

            case Mode::MProc:
                if (threadId == 0) {
                    return detail::tls_moduleName;
                }
                if (state.sharedMemory) {
                    const char* name = state.sharedMemory->GetThreadName(threadId);
                    if (name && name[0] != '\0') {
                        return std::string(name);
                    }
                }
                return std::to_string(threadId);
        }

        return "main";
    }

    static Mode GetMode() {
        return detail::GetNameManagerState<EnableWFC>().mode;
    }

    static uint32_t GetRegisteredProcessId() {
        return detail::GetNameManagerState<EnableWFC>().registeredProcessId;
    }

    static uint32_t GetRegisteredThreadId() {
        return detail::tls_registeredThreadId;
    }

    static bool IsInitialized() {
        return detail::GetNameManagerState<EnableWFC>().initialized.load(std::memory_order_acquire);
    }

private:
    NameManager() = delete;

    static std::string TruncateName(const std::string& name) {
        if (name.length() <= kMaxNameLength) {
            return name;
        }
        return name.substr(0, kMaxNameLength);
    }
};

// ==============================================================================
// ThreadWithModuleName - Thread Creation Helper / 线程创建辅助类
// ==============================================================================

/**
 * @brief Thread creation helper that inherits module name from parent thread
 * @brief 继承父线程模块名的线程创建辅助类
 *
 * @tparam EnableWFC Enable WFC support / 启用 WFC 支持
 */
template<bool EnableWFC = false>
class ThreadWithModuleName {
public:
    template<typename Func, typename... Args>
    static std::thread Create(Func&& func, Args&&... args) {
        std::string parentModuleName = NameManager<EnableWFC>::GetModuleName();
        auto argsTuple = std::make_tuple(std::forward<Args>(args)...);
        
        return std::thread([parentModuleName, 
                           f = std::forward<Func>(func),
                           argsTuple = std::move(argsTuple)]() mutable {
            if (!detail::tls_moduleNameInitialized) {
                NameManager<EnableWFC>::SetModuleName(parentModuleName);
            }
            std::apply(f, std::move(argsTuple));
        });
    }

    template<typename Func, typename... Args>
    static std::thread CreateWithName(const std::string& moduleName, Func&& func, Args&&... args) {
        auto argsTuple = std::make_tuple(std::forward<Args>(args)...);
        
        return std::thread([moduleName,
                           f = std::forward<Func>(func),
                           argsTuple = std::move(argsTuple)]() mutable {
            NameManager<EnableWFC>::SetModuleName(moduleName);
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
 * @brief Initialize name manager
 * @brief 初始化名称管理器
 */
template<bool EnableWFC = false>
inline void InitNameManager(Mode mode, SharedMemory<EnableWFC>* sharedMemory = nullptr) {
    NameManager<EnableWFC>::Initialize(mode, sharedMemory);
}

/**
 * @brief Shutdown name manager
 * @brief 关闭名称管理器
 */
template<bool EnableWFC = false>
inline void ShutdownNameManager() {
    NameManager<EnableWFC>::Shutdown();
}

}  // namespace oneplog

