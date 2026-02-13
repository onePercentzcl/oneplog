/**
 * @file platform_lookup_table.hpp
 * @brief Platform-specific lookup table selector
 * @brief 平台特定查找表选择器
 *
 * This file provides compile-time platform detection to automatically select
 * the optimal lookup table implementation:
 * - Linux: DirectMappingTable with O(1) lookup using TID as array index
 * - Non-Linux (macOS/Windows): ArrayMappingTable with O(n) linear search
 *
 * 此文件提供编译期平台检测，自动选择最优的查找表实现：
 * - Linux：使用 TID 作为数组索引的 O(1) 查找 DirectMappingTable
 * - 非 Linux（macOS/Windows）：O(n) 线性搜索的 ArrayMappingTable
 *
 * @copyright Copyright (c) 2024 onePlog
 *
 * _Requirements: 5.1, 5.2, 5.3, 5.4, 9.2_
 */

#pragma once

#include "oneplog/internal/direct_mapping_table.hpp"
#include "oneplog/internal/array_mapping_table.hpp"

namespace oneplog {
namespace internal {

// ==============================================================================
// Compile-time Platform Detection / 编译期平台检测
// ==============================================================================

/**
 * @brief Compile-time platform check
 * @brief 编译期平台检查
 *
 * Uses preprocessor macro __linux__ for platform detection.
 * 使用预处理器宏 __linux__ 进行平台检测。
 *
 * _Requirements: 5.3, 9.2_
 */
#ifdef __linux__
inline constexpr bool kIsLinuxPlatform = true;
#else
inline constexpr bool kIsLinuxPlatform = false;
#endif

// ==============================================================================
// Platform Lookup Table Selector / 平台查找表选择器
// ==============================================================================

/**
 * @brief Platform-specific lookup table selector
 * @brief 平台特定查找表选择器
 *
 * Uses compile-time platform detection to select the optimal implementation.
 * - Linux: DirectMappingTable<32768, MaxNameLen> for O(1) lookup
 * - Non-Linux: ArrayMappingTable<256, MaxNameLen> for O(n) lookup
 *
 * 使用编译期平台检测选择最优实现。
 * - Linux：DirectMappingTable<32768, MaxNameLen> 实现 O(1) 查找
 * - 非 Linux：ArrayMappingTable<256, MaxNameLen> 实现 O(n) 查找
 *
 * @tparam MaxNameLen Maximum name length (user configurable, default: 15)
 *                    最大名称长度（用户可配置，默认：15）
 *
 * _Requirements: 5.1, 5.2, 5.3, 5.4_
 */
template<size_t MaxNameLen = 15>
struct PlatformLookupTableSelector {
#ifdef __linux__
    /// Linux: Use DirectMappingTable with O(1) lookup
    /// Linux：使用 O(1) 查找的 DirectMappingTable
    using Type = DirectMappingTable<32768, MaxNameLen>;
#else
    /// Non-Linux: Use ArrayMappingTable with O(n) lookup
    /// 非 Linux：使用 O(n) 查找的 ArrayMappingTable
    using Type = ArrayMappingTable<256, MaxNameLen>;
#endif
};

// ==============================================================================
// Type Aliases / 类型别名
// ==============================================================================

/**
 * @brief Default platform lookup table
 * @brief 默认平台查找表
 *
 * Uses the platform-specific implementation with default name length (15).
 * 使用平台特定实现和默认名称长度（15）。
 *
 * _Requirements: 5.4_
 */
using PlatformLookupTable = typename PlatformLookupTableSelector<15>::Type;

/**
 * @brief Extended platform lookup table with 31-character names
 * @brief 31 字符名称的扩展平台查找表
 *
 * For compatibility with existing ThreadModuleTable.
 * 用于与现有 ThreadModuleTable 兼容。
 */
using ExtendedPlatformLookupTable = typename PlatformLookupTableSelector<31>::Type;

// ==============================================================================
// Platform Information / 平台信息
// ==============================================================================

/**
 * @brief Get platform name as compile-time string
 * @brief 获取平台名称作为编译期字符串
 *
 * @return "Linux" on Linux, "Non-Linux" on other platforms
 *         Linux 上返回 "Linux"，其他平台返回 "Non-Linux"
 */
inline constexpr const char* GetPlatformName() noexcept {
    return kIsLinuxPlatform ? "Linux" : "Non-Linux";
}

/**
 * @brief Get lookup table type name as compile-time string
 * @brief 获取查找表类型名称作为编译期字符串
 *
 * @return "DirectMappingTable" on Linux, "ArrayMappingTable" on other platforms
 *         Linux 上返回 "DirectMappingTable"，其他平台返回 "ArrayMappingTable"
 */
inline constexpr const char* GetLookupTableTypeName() noexcept {
    return kIsLinuxPlatform ? "DirectMappingTable" : "ArrayMappingTable";
}

/**
 * @brief Get lookup complexity description
 * @brief 获取查找复杂度描述
 *
 * @return "O(1)" on Linux, "O(n)" on other platforms
 *         Linux 上返回 "O(1)"，其他平台返回 "O(n)"
 */
inline constexpr const char* GetLookupComplexity() noexcept {
    return kIsLinuxPlatform ? "O(1)" : "O(n)";
}

}  // namespace internal
}  // namespace oneplog
