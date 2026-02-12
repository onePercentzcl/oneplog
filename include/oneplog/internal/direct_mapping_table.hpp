/**
 * @file direct_mapping_table.hpp
 * @brief Direct mapping table using TID as array index (Linux only)
 * @brief 使用 TID 作为数组索引的直接映射表（仅 Linux）
 *
 * This file provides an O(1) lookup table for thread module names on Linux.
 * Linux TIDs are typically small integers starting from 1, making them
 * suitable for direct array indexing.
 *
 * 此文件为 Linux 上的线程模块名提供 O(1) 查找表。
 * Linux TID 通常是从 1 开始的小整数，适合直接数组索引。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include "oneplog/internal/fixed_name.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace oneplog {
namespace internal {

/**
 * @brief Direct mapping table using TID as array index (Linux only)
 * @brief 使用 TID 作为数组索引的直接映射表（仅 Linux）
 *
 * Linux TIDs are typically small integers starting from 1, making them
 * suitable for direct array indexing with O(1) lookup complexity.
 *
 * Linux TID 通常是从 1 开始的小整数，适合直接数组索引，实现 O(1) 查找复杂度。
 *
 * @tparam MaxTid Maximum supported TID value (default: Linux default max PID 32768)
 *                支持的最大 TID 值（默认：Linux 默认 pid_max 32768）
 * @tparam MaxNameLen Maximum name length (default: 15, Linux task name limit)
 *                    最大名称长度（默认：15，Linux 任务名限制）
 *
 * _Requirements: 1.1, 1.2, 1.4, 1.5, 6.1, 6.3_
 */
template<size_t MaxTid = 32768, size_t MaxNameLen = 15>
class DirectMappingTable {
public:
    /// Maximum supported TID value (configurable, default from /proc/sys/kernel/pid_max)
    /// 支持的最大 TID 值（可配置，默认来自 Linux pid_max）
    static constexpr size_t kMaxTid = MaxTid;

    /// Maximum name length (configurable, default: Linux TASK_COMM_LEN - 1 = 15)
    /// 最大名称长度（可配置，默认：Linux TASK_COMM_LEN - 1 = 15）
    static constexpr size_t kMaxNameLength = MaxNameLen;

    /// Default name when TID not found / TID 未找到时的默认名称
    static constexpr const char* kDefaultName = "main";

    /**
     * @brief Default constructor
     * @brief 默认构造函数
     */
    DirectMappingTable() noexcept = default;

    /// Non-copyable / 不可复制
    DirectMappingTable(const DirectMappingTable&) = delete;
    DirectMappingTable& operator=(const DirectMappingTable&) = delete;

    /// Non-movable / 不可移动
    DirectMappingTable(DirectMappingTable&&) = delete;
    DirectMappingTable& operator=(DirectMappingTable&&) = delete;

    /**
     * @brief Register or update module name for a TID
     * @brief 注册或更新 TID 的模块名
     *
     * Thread-safe: Uses atomic operations to ensure consistency.
     * 线程安全：使用原子操作确保一致性。
     *
     * @param tid Thread ID (must be < kMaxTid) / 线程 ID（必须 < kMaxTid）
     * @param name Module name (truncated to kMaxNameLength)
     *             模块名（截断到 kMaxNameLength）
     * @return true if successful, false if TID out of range
     *         成功返回 true，TID 超出范围返回 false
     *
     * _Requirements: 1.5_
     */
    bool Register(uint32_t tid, std::string_view name) noexcept {
        if (tid >= kMaxTid) {
            return false;
        }

        Entry& entry = m_entries[tid];

        // Copy name with truncation / 复制名称并截断
        const size_t len = (name.size() < kMaxNameLength) ? name.size() : kMaxNameLength;
        for (size_t i = 0; i < len; ++i) {
            entry.name[i] = name[i];
        }
        entry.name[len] = '\0';
        // Clear remaining bytes / 清除剩余字节
        for (size_t i = len + 1; i <= kMaxNameLength; ++i) {
            entry.name[i] = '\0';
        }

        // Check if this is a new entry / 检查是否为新条目
        bool wasValid = entry.valid.exchange(true, std::memory_order_acq_rel);
        if (!wasValid) {
            m_count.fetch_add(1, std::memory_order_relaxed);
        }

        return true;
    }

    /**
     * @brief Get module name by TID
     * @brief 通过 TID 获取模块名
     *
     * O(1) lookup complexity using TID as direct array index.
     * 使用 TID 作为直接数组索引，O(1) 查找复杂度。
     *
     * @param tid Thread ID / 线程 ID
     * @return Module name or kDefaultName if not found or out of range
     *         模块名，如果未找到或超出范围则返回 kDefaultName
     *
     * _Requirements: 1.2, 1.3_
     */
    std::string_view GetName(uint32_t tid) const noexcept {
        if (tid >= kMaxTid) {
            return kDefaultName;
        }

        const Entry& entry = m_entries[tid];
        if (!entry.valid.load(std::memory_order_acquire)) {
            return kDefaultName;
        }

        return std::string_view(entry.name);
    }

    /**
     * @brief Clear all entries
     * @brief 清空所有条目
     *
     * Thread-safe: Can be called while other threads are accessing the table.
     * 线程安全：可以在其他线程访问表时调用。
     */
    void Clear() noexcept {
        for (size_t i = 0; i < kMaxTid; ++i) {
            m_entries[i].valid.store(false, std::memory_order_relaxed);
            // Note: We don't clear the name data for performance
            // The valid flag is sufficient to indicate empty entries
            // 注意：为了性能，我们不清除名称数据
            // valid 标志足以指示空条目
        }
        m_count.store(0, std::memory_order_release);
    }

    /**
     * @brief Get number of registered entries
     * @brief 获取已注册条目数
     *
     * @return Number of valid entries / 有效条目数
     */
    size_t Count() const noexcept {
        return m_count.load(std::memory_order_acquire);
    }

    /**
     * @brief Check if a TID is registered
     * @brief 检查 TID 是否已注册
     *
     * @param tid Thread ID to check / 要检查的线程 ID
     * @return true if registered, false otherwise / 已注册返回 true，否则返回 false
     */
    bool IsRegistered(uint32_t tid) const noexcept {
        if (tid >= kMaxTid) {
            return false;
        }
        return m_entries[tid].valid.load(std::memory_order_acquire);
    }

private:
    /**
     * @brief Entry structure for direct mapping
     * @brief 直接映射的条目结构
     *
     * Aligned to cache line to avoid false sharing in concurrent access.
     * 对齐到缓存行以避免并发访问时的伪共享。
     */
    struct alignas(64) Entry {
        char name[kMaxNameLength + 1]{0};    ///< Module name / 模块名
        std::atomic<bool> valid{false};       ///< Entry validity flag / 条目有效标志
        // Padding to fill cache line / 填充以填满缓存行
        char padding[64 - sizeof(char[kMaxNameLength + 1]) - sizeof(std::atomic<bool>)];
    };

    /// Entry array indexed by TID / 以 TID 为索引的条目数组
    Entry m_entries[kMaxTid];

    /// Number of registered entries / 已注册条目数
    std::atomic<size_t> m_count{0};
};

// ==============================================================================
// Type Aliases / 类型别名
// ==============================================================================

/**
 * @brief Default DirectMappingTable with Linux defaults
 * @brief 使用 Linux 默认值的 DirectMappingTable
 *
 * - MaxTid: 32768 (Linux default pid_max)
 * - MaxNameLen: 15 (Linux TASK_COMM_LEN - 1)
 */
using DefaultDirectMappingTable = DirectMappingTable<32768, 15>;

/**
 * @brief Extended DirectMappingTable with 31-character names
 * @brief 31 字符名称的扩展 DirectMappingTable
 *
 * For compatibility with existing ThreadModuleTable.
 * 用于与现有 ThreadModuleTable 兼容。
 */
using ExtendedDirectMappingTable = DirectMappingTable<32768, 31>;

}  // namespace internal
}  // namespace oneplog
