/**
 * @file fixed_name.hpp
 * @brief Fixed-length name storage with compile-time support
 * @brief 支持编译期的固定长度名称存储
 *
 * This file provides a compile-time optimized fixed-length name storage
 * structure for process and module names. It avoids heap allocation by
 * using stack-based storage with a configurable maximum capacity.
 *
 * 此文件提供编译期优化的固定长度名称存储结构，用于进程名和模块名。
 * 通过使用可配置最大容量的栈存储来避免堆分配。
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include <cstddef>
#include <cstring>
#include <string_view>

namespace oneplog {
namespace internal {

/**
 * @brief Fixed-length name storage (compile-time size)
 * @brief 固定长度名称存储（编译期大小）
 *
 * Provides a stack-allocated, fixed-capacity string storage that supports
 * constexpr construction and operations. Designed for high-performance
 * name storage without heap allocation.
 *
 * 提供栈分配的固定容量字符串存储，支持 constexpr 构造和操作。
 * 专为无堆分配的高性能名称存储设计。
 *
 * @tparam Capacity Maximum name length (user configurable, default: 15)
 *                  最大名称长度（用户可配置，默认：15）
 *
 * _Requirements: 9.1, 9.6_
 */
template<size_t Capacity = 15>
struct alignas(32) FixedName {
    /// Maximum capacity of the name storage / 名称存储的最大容量
    static constexpr size_t kCapacity = Capacity;

    /// Internal storage buffer (capacity + 1 for null terminator)
    /// 内部存储缓冲区（容量 + 1 用于空终止符）
    char data[kCapacity + 1]{0};

    /**
     * @brief Default constructor (creates empty name)
     * @brief 默认构造函数（创建空名称）
     */
    constexpr FixedName() noexcept = default;

    /**
     * @brief Construct from string_view (compile-time if possible)
     * @brief 从 string_view 构造（尽可能编译期）
     *
     * Names longer than Capacity are automatically truncated.
     * 超过 Capacity 的名称会自动截断。
     *
     * @param sv Source string view / 源字符串视图
     */
    constexpr explicit FixedName(std::string_view sv) noexcept {
        const size_t len = (sv.size() < kCapacity) ? sv.size() : kCapacity;
        for (size_t i = 0; i < len; ++i) {
            data[i] = sv[i];
        }
        data[len] = '\0';
    }

    /**
     * @brief Construct from C-string (compile-time if possible)
     * @brief 从 C 字符串构造（尽可能编译期）
     *
     * @param str Source C-string (null-terminated) / 源 C 字符串（空终止）
     */
    constexpr explicit FixedName(const char* str) noexcept
        : FixedName(std::string_view(str)) {}

    /**
     * @brief Get string_view of the stored name
     * @brief 获取存储名称的 string_view
     *
     * @return String view of the name / 名称的字符串视图
     */
    constexpr std::string_view View() const noexcept {
        return std::string_view(data, Length());
    }

    /**
     * @brief Get the length of the stored name
     * @brief 获取存储名称的长度
     *
     * @return Length of the name (excluding null terminator)
     *         名称长度（不包括空终止符）
     */
    constexpr size_t Length() const noexcept {
        size_t len = 0;
        while (len < kCapacity && data[len] != '\0') {
            ++len;
        }
        return len;
    }

    /**
     * @brief Check if the name is empty
     * @brief 检查名称是否为空
     *
     * @return true if empty, false otherwise / 如果为空返回 true，否则返回 false
     */
    constexpr bool Empty() const noexcept {
        return data[0] == '\0';
    }

    /**
     * @brief Get pointer to the underlying C-string
     * @brief 获取底层 C 字符串的指针
     *
     * @return Pointer to null-terminated string / 指向空终止字符串的指针
     */
    constexpr const char* CStr() const noexcept {
        return data;
    }

    /**
     * @brief Assign from string_view
     * @brief 从 string_view 赋值
     *
     * @param sv Source string view / 源字符串视图
     */
    constexpr void Assign(std::string_view sv) noexcept {
        const size_t len = (sv.size() < kCapacity) ? sv.size() : kCapacity;
        for (size_t i = 0; i < len; ++i) {
            data[i] = sv[i];
        }
        data[len] = '\0';
        // Clear remaining bytes / 清除剩余字节
        for (size_t i = len + 1; i <= kCapacity; ++i) {
            data[i] = '\0';
        }
    }

    /**
     * @brief Clear the name (set to empty)
     * @brief 清空名称（设为空）
     */
    constexpr void Clear() noexcept {
        for (size_t i = 0; i <= kCapacity; ++i) {
            data[i] = '\0';
        }
    }

    /**
     * @brief Equality comparison
     * @brief 相等比较
     */
    constexpr bool operator==(const FixedName& other) const noexcept {
        for (size_t i = 0; i <= kCapacity; ++i) {
            if (data[i] != other.data[i]) {
                return false;
            }
            if (data[i] == '\0') {
                break;
            }
        }
        return true;
    }

    /**
     * @brief Inequality comparison
     * @brief 不等比较
     */
    constexpr bool operator!=(const FixedName& other) const noexcept {
        return !(*this == other);
    }

    /**
     * @brief Equality comparison with string_view
     * @brief 与 string_view 的相等比较
     */
    constexpr bool operator==(std::string_view sv) const noexcept {
        return View() == sv;
    }

    /**
     * @brief Inequality comparison with string_view
     * @brief 与 string_view 的不等比较
     */
    constexpr bool operator!=(std::string_view sv) const noexcept {
        return View() != sv;
    }
};

// ==============================================================================
// Compile-time Default Names / 编译期默认名称
// ==============================================================================

/**
 * @brief Default process name (compile-time constant)
 * @brief 默认进程名（编译期常量）
 *
 * _Requirements: 9.6_
 */
inline constexpr FixedName<15> kDefaultProcessName{"main"};

/**
 * @brief Default module name (compile-time constant)
 * @brief 默认模块名（编译期常量）
 *
 * _Requirements: 9.6_
 */
inline constexpr FixedName<15> kDefaultModuleName{"main"};

// ==============================================================================
// Type Aliases / 类型别名
// ==============================================================================

/**
 * @brief Default FixedName type with 15-character capacity
 * @brief 15 字符容量的默认 FixedName 类型
 *
 * This matches Linux TASK_COMM_LEN - 1 (15 characters).
 * 这与 Linux TASK_COMM_LEN - 1（15 字符）匹配。
 */
using DefaultFixedName = FixedName<15>;

/**
 * @brief Extended FixedName type with 31-character capacity
 * @brief 31 字符容量的扩展 FixedName 类型
 *
 * For compatibility with existing ThreadModuleTable.
 * 用于与现有 ThreadModuleTable 兼容。
 */
using ExtendedFixedName = FixedName<31>;

}  // namespace internal
}  // namespace oneplog
