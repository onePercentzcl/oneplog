/**
 * @file array_mapping_table.hpp
 * @brief Array mapping table with linear search (non-Linux platforms)
 * @brief 使用线性搜索的数组映射表（非 Linux 平台）
 *
 * This file provides a lookup table for thread module names on non-Linux
 * platforms (macOS, Windows). Since TIDs on these platforms can be large
 * values, direct indexing is impractical. This implementation uses linear
 * search with a fixed-size array.
 *
 * 此文件为非 Linux 平台（macOS、Windows）上的线程模块名提供查找表。
 * 由于这些平台上的 TID 可能是很大的值，直接索引不实际。
 * 此实现使用固定大小数组的线性搜索。
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
 * @brief Array mapping table with linear search (non-Linux platforms)
 * @brief 使用线性搜索的数组映射表（非 Linux 平台）
 *
 * macOS and Windows TIDs can be large values, making direct indexing
 * impractical. This implementation uses linear search with a fixed-size array.
 *
 * macOS 和 Windows 的 TID 可能是很大的值，直接索引不实际。
 * 此实现使用固定大小数组的线性搜索。
 *
 * @tparam MaxEntries Maximum number of entries (default: 256)
 *                    最大条目数（默认：256）
 * @tparam MaxNameLen Maximum name length (default: 15)
 *                    最大名称长度（默认：15）
 *
 * _Requirements: 2.1, 2.2, 2.3, 2.4, 2.5, 6.1, 6.3_
 */
template<size_t MaxEntries = 256, size_t MaxNameLen = 15>
class ArrayMappingTable {
public:
    /// Maximum number of entries (configurable) / 最大条目数（可配置）
    static constexpr size_t kMaxEntries = MaxEntries;

    /// Maximum name length (configurable) / 最大名称长度（可配置）
    static constexpr size_t kMaxNameLength = MaxNameLen;

    /// Default name when TID not found / TID 未找到时的默认名称
    static constexpr const char* kDefaultName = "main";

    /**
     * @brief Default constructor
     * @brief 默认构造函数
     */
    ArrayMappingTable() noexcept = default;

    /// Non-copyable / 不可复制
    ArrayMappingTable(const ArrayMappingTable&) = delete;
    ArrayMappingTable& operator=(const ArrayMappingTable&) = delete;

    /// Non-movable / 不可移动
    ArrayMappingTable(ArrayMappingTable&&) = delete;
    ArrayMappingTable& operator=(ArrayMappingTable&&) = delete;

    /**
     * @brief Register or update module name for a TID
     * @brief 注册或更新 TID 的模块名
     *
     * Thread-safe: Uses atomic operations to ensure consistency.
     * If the TID already exists, updates the name in place.
     * If the TID is new and table is not full, appends a new entry.
     *
     * 线程安全：使用原子操作确保一致性。
     * 如果 TID 已存在，原地更新名称。
     * 如果 TID 是新的且表未满，追加新条目。
     *
     * @param tid Thread ID / 线程 ID
     * @param name Module name (truncated to kMaxNameLength)
     *             模块名（截断到 kMaxNameLength）
     * @return true if successful, false if table full and TID not found
     *         成功返回 true，表满且 TID 未找到返回 false
     *
     * _Requirements: 2.3, 2.4_
     */
    bool Register(uint32_t tid, std::string_view name) noexcept {
        // First, try to find existing entry for this TID (update in place)
        // 首先，尝试找到此 TID 的现有条目（原地更新）
        size_t count = m_count.load(std::memory_order_acquire);
        for (size_t i = 0; i < count; ++i) {
            if (m_entries[i].valid.load(std::memory_order_acquire) &&
                m_entries[i].tid.load(std::memory_order_acquire) == tid) {
                // Found existing entry, update name in place
                // 找到现有条目，原地更新名称
                CopyName(m_entries[i], name);
                return true;
            }
        }

        // TID not found, try to add new entry
        // TID 未找到，尝试添加新条目
        // Use compare-exchange to atomically claim a slot
        // 使用 compare-exchange 原子地获取一个槽位
        size_t currentCount = m_count.load(std::memory_order_acquire);
        while (currentCount < kMaxEntries) {
            if (m_count.compare_exchange_weak(currentCount, currentCount + 1,
                                               std::memory_order_acq_rel,
                                               std::memory_order_acquire)) {
                // Successfully claimed slot at index currentCount
                // 成功获取索引 currentCount 处的槽位
                Entry& entry = m_entries[currentCount];
                entry.tid.store(tid, std::memory_order_relaxed);
                CopyName(entry, name);
                entry.valid.store(true, std::memory_order_release);
                return true;
            }
            // CAS failed, currentCount updated, retry
            // CAS 失败，currentCount 已更新，重试
        }

        // Table is full / 表已满
        return false;
    }

    /**
     * @brief Get module name by TID
     * @brief 通过 TID 获取模块名
     *
     * O(n) lookup complexity using linear search.
     * 使用线性搜索，O(n) 查找复杂度。
     *
     * @param tid Thread ID / 线程 ID
     * @return Module name or kDefaultName if not found
     *         模块名，如果未找到则返回 kDefaultName
     *
     * _Requirements: 2.2_
     */
    std::string_view GetName(uint32_t tid) const noexcept {
        size_t count = m_count.load(std::memory_order_acquire);
        for (size_t i = 0; i < count; ++i) {
            if (m_entries[i].valid.load(std::memory_order_acquire) &&
                m_entries[i].tid.load(std::memory_order_acquire) == tid) {
                return std::string_view(m_entries[i].name);
            }
        }
        return kDefaultName;
    }

    /**
     * @brief Clear all entries
     * @brief 清空所有条目
     *
     * Thread-safe: Can be called while other threads are accessing the table.
     * Note: This is not fully atomic with concurrent Register calls.
     * 线程安全：可以在其他线程访问表时调用。
     * 注意：与并发 Register 调用不是完全原子的。
     */
    void Clear() noexcept {
        // First set count to 0 to prevent new lookups from finding entries
        // 首先将 count 设为 0 以防止新的查找找到条目
        size_t oldCount = m_count.exchange(0, std::memory_order_acq_rel);
        
        // Then invalidate all entries
        // 然后使所有条目无效
        for (size_t i = 0; i < oldCount; ++i) {
            m_entries[i].valid.store(false, std::memory_order_relaxed);
        }
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
        size_t count = m_count.load(std::memory_order_acquire);
        for (size_t i = 0; i < count; ++i) {
            if (m_entries[i].valid.load(std::memory_order_acquire) &&
                m_entries[i].tid.load(std::memory_order_acquire) == tid) {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Check if the table is full
     * @brief 检查表是否已满
     *
     * @return true if full, false otherwise / 已满返回 true，否则返回 false
     */
    bool IsFull() const noexcept {
        return m_count.load(std::memory_order_acquire) >= kMaxEntries;
    }

private:
    /**
     * @brief Entry structure for array mapping
     * @brief 数组映射的条目结构
     *
     * Aligned to cache line to avoid false sharing in concurrent access.
     * 对齐到缓存行以避免并发访问时的伪共享。
     */
    struct alignas(64) Entry {
        std::atomic<uint32_t> tid{0};         ///< Thread ID / 线程 ID
        char name[kMaxNameLength + 1]{0};     ///< Module name / 模块名
        std::atomic<bool> valid{false};       ///< Entry validity flag / 条目有效标志
        // Padding to fill cache line / 填充以填满缓存行
        char padding[64 - sizeof(std::atomic<uint32_t>) - sizeof(char[kMaxNameLength + 1]) - sizeof(std::atomic<bool>)];
    };

    /**
     * @brief Copy name to entry with truncation
     * @brief 复制名称到条目并截断
     *
     * @param entry Target entry / 目标条目
     * @param name Source name / 源名称
     */
    static void CopyName(Entry& entry, std::string_view name) noexcept {
        const size_t len = (name.size() < kMaxNameLength) ? name.size() : kMaxNameLength;
        for (size_t i = 0; i < len; ++i) {
            entry.name[i] = name[i];
        }
        entry.name[len] = '\0';
        // Clear remaining bytes / 清除剩余字节
        for (size_t i = len + 1; i <= kMaxNameLength; ++i) {
            entry.name[i] = '\0';
        }
    }

    /// Entry array / 条目数组
    Entry m_entries[kMaxEntries];

    /// Number of registered entries / 已注册条目数
    std::atomic<size_t> m_count{0};
};

// ==============================================================================
// Type Aliases / 类型别名
// ==============================================================================

/**
 * @brief Default ArrayMappingTable with standard defaults
 * @brief 使用标准默认值的 ArrayMappingTable
 *
 * - MaxEntries: 256
 * - MaxNameLen: 15
 */
using DefaultArrayMappingTable = ArrayMappingTable<256, 15>;

/**
 * @brief Extended ArrayMappingTable with 31-character names
 * @brief 31 字符名称的扩展 ArrayMappingTable
 *
 * For compatibility with existing ThreadModuleTable.
 * 用于与现有 ThreadModuleTable 兼容。
 */
using ExtendedArrayMappingTable = ArrayMappingTable<256, 31>;

}  // namespace internal
}  // namespace oneplog
