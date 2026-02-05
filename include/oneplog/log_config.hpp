/**
* @file log_config.hpp
 * @brief Configuration structures for the logging system
 * @brief 日志系统的配置结构体定义
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include "common.hpp"

namespace oneplog::internal {

/**
 * @brief General configuration settings.
 * @brief 常规配置设置。
 * @details Aligned to cache line to prevent false sharing when updated or read frequently.
 * @details 对齐到缓存行，以防止在频繁更新或读取时发生伪共享。
 */
struct alignas(kCacheLineSize) GeneralConfig {
    Mode m_logMode;      ///< Current operating mode / 当前运行模式
    Level m_logLevel;    ///< Current logging threshold / 当前日志阈值
    uint8_t m_enableWFC; ///< Wait-For-Confirmation flag (0 or 1) / 等待确认标志
};

/**
 * @brief Memory subsystem configuration.
 * @brief 内存子系统配置。
 */
struct alignas(kCacheLineSize) MemoryConfig {
    /**
     * @brief Configuration for the process-local heap ring buffer.
     * @brief 进程本地堆环形缓冲区的配置。
     */
    struct HeapRingBufferConfig {
        size_t m_slotSize;  ///< Size of each slot in bytes / 每个槽位的字节大小
        size_t m_slotCount; ///< Total number of slots / 槽位总数
    } m_heapRingBufferConfig;

    /**
     * @brief Configuration for the inter-process shared ring buffer.
     * @brief 进程间共享环形缓冲区的配置。
     */
    struct SharedRingBufferConfig {
        size_t m_slotSize;  ///< Size of each slot in bytes / 每个槽位的字节大小
        size_t m_slotCount; ///< Total number of slots / 槽位总数
    } m_sharedRingBufferConfig;
};

/**
 * @brief Aggregated configuration structure used in shared memory.
 * @brief 共享内存中使用的聚合配置结构体。
 */
struct alignas(kCacheLineSize) LogConfig {
    GeneralConfig m_generalConfig; ///< General settings / 常规设置
    MemoryConfig m_memoryConfig;   ///< Memory settings / 内存设置
};

} // namespace oneplog::internal