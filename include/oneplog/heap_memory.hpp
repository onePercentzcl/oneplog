/**
 * @file heap_memory.hpp
 * @brief Heap Memory Layout Definition
 * @brief 堆内存布局定义
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include "common.hpp"
#include "log_config.hpp"
#include "registry.hpp"
#include "ring_buffer.hpp"

namespace oneplog::internal {
/**
 * @brief Header describing the heap memory layout.
 * @brief 描述堆内存布局的头部。
 * * @details Located at the very beginning of the heap memory block.
 * @details 位于堆内存块的最开始位置。
 */
struct alignas(kCacheLineSize) HpmHeader {
    uint32_t m_magicNumber{0x4F4E4550}; ///< Magic Number ("ONEP") / 魔数，用于校验
    uint32_t m_version{1};              ///< Version control / 版本控制，防止结构体不兼容
    uint64_t m_totalSize{};             ///< Total size of HPM / 共享内存总大小

    /**
     * @brief Helper to align offset to the next cache line boundary.
     * @brief 将偏移量对齐到下一个缓存行边界的辅助函数。
     */
    static constexpr uint64_t Align(const uint64_t value) {
        return (value + kCacheLineSize - 1) & ~(kCacheLineSize - 1);
    }

    // Offsets to various sections (Manual calculation required based on layout)
    // 各个区域的偏移量（需要根据布局手动计算）
    // Note: sizeof(ShmHeader) is invalid here (incomplete type), but since it's aligned
    // and members are small (<64B), its size is effectively kCacheLineSize.
    // 注意：此处不能使用 sizeof(ShmHeader)，但因其已对齐且成员较小，其实际大小即为 kCacheLineSize。
    uint64_t m_offsetConfig{kCacheLineSize};                                 ///< Offset to LogConfig / 配置数据偏移量
    uint64_t m_offsetRegistry{Align(m_offsetConfig + sizeof(LogConfig))};    ///< Offset to Registry / 注册表偏移量
    uint64_t m_offsetRingBuffer{Align(m_offsetRegistry + sizeof(Registry))}; ///< Offset to RingBuffer / 环形缓冲区偏移量
};

/**
 * @brief Layout descriptor for the entire heap memory block.
 * @brief 整个堆内存块的布局描述符。
 * * @details This class is not meant to be instantiated on stack.
 * It describes how the memory mapped file is structured.
 * Use placement new on the mmap pointer to initialize.
 * * @details 该类不应在栈上实例化。它描述了内存映射文件的结构。
 * 请在 mmap 指针上使用 placement new 进行初始化。
 */
class HeapMemory {
public:
    HeapMemory() = default;

    HeapMemory(const HeapMemory&) = delete;

    HeapMemory& operator=(const HeapMemory&) = delete;

    /**
     * @brief Get pointer to the Header section.
     * @brief 获取 Header 区域指针。
     */
    HpmHeader* GetHeader() { return &m_hpmHeader; }

    /**
     * @brief Get pointer to the Config section.
     * @brief 获取配置区域指针。
     */
    LogConfig* GetConfig() { return &m_logConfig; }

    /**
     * @brief Get pointer to the Registry section.
     * @brief 获取注册表区域指针。
     */
    Registry* GetRegistry() { return &m_registry; }

    /**
     * @brief Get pointer to the RingBuffer section.
     * @brief 获取环形缓冲区区域指针。
     * @note Assumes RingBuffer stores `int` for now, adjust template type as needed.
     * @note 暂定 RingBuffer 存储 `int`，请按需调整模板类型。
     */
    RingBuffer<256, 1024>* GetRingBuffer() { return &m_ringBuffer; }

private:
    // Physical layout in memory:
    // 内存中的物理布局：

    HpmHeader m_hpmHeader{}; ///< 0x00: Header
    LogConfig m_logConfig{}; ///< Config section
    Registry m_registry{};   ///< Registry section (Large on Linux)

    // Using explicit types for layout definition.
    // In practice, RingBuffer size might be dynamic in advanced implementations.
    // 使用显式类型进行布局定义。在高级实现中，RingBuffer 大小可能是动态的。
    RingBuffer<256, 1024> m_ringBuffer{};
};
} // namespace oneplog::internal