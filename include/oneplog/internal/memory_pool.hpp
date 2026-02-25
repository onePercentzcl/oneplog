/**
 * @file memory_pool.hpp
 * @brief Lock-free memory pool for onePlog
 * @brief onePlog 无锁内存池
 *
 * This file provides a high-performance lock-free memory pool for
 * pre-allocated fixed-size objects. Key features:
 *
 * 本文件提供用于预分配固定大小对象的高性能无锁内存池。主要特性：
 *
 * - Lock-free allocation and deallocation using CAS operations
 *   使用 CAS 操作的无锁分配和释放
 * - Pre-allocated blocks to avoid runtime heap allocation
 *   预分配块以避免运行时堆分配
 * - Cache-line aligned blocks to prevent false sharing
 *   缓存行对齐块以防止伪共享
 * - Dynamic expansion support when pool is exhausted
 *   池耗尽时的动态扩展支持
 *
 * Thread Safety / 线程安全性:
 * - Multiple threads can allocate/deallocate concurrently
 *   多线程可以并发分配/释放
 * - Uses atomic compare-exchange for free list management
 *   使用原子比较交换进行空闲链表管理
 * - ABA problem mitigated by not reusing blocks immediately
 *   通过不立即重用块来缓解 ABA 问题
 *
 * @see HeapRingBuffer for the primary use case
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "oneplog/common.hpp"
#include "oneplog/internal/heap_memory.hpp"

namespace oneplog {

/**
 * @brief Lock-free memory pool template class
 * @brief 无锁内存池模板类
 *
 * Pre-allocates fixed-size memory blocks for efficient allocation and deallocation.
 * Uses a lock-free free list for thread-safe operations.
 * 预分配固定大小内存块，实现高效的分配和释放。
 * 使用无锁空闲链表实现线程安全操作。
 *
 * @tparam T Type of objects to allocate / 要分配的对象类型
 */
template <typename T>
class MemoryPool {
public:
    /**
     * @brief Construct a memory pool with initial capacity
     * @brief 构造具有初始容量的内存池
     *
     * @param initialSize Initial number of blocks to allocate / 初始分配的块数
     */
    explicit MemoryPool(size_t initialSize) : m_totalCount(0) {
        m_freeList.value.store(nullptr, std::memory_order_relaxed);
        if (initialSize > 0) {
            AllocateBlocks(initialSize);
        }
    }

    /**
     * @brief Destructor - releases all allocated memory
     * @brief 析构函数 - 释放所有分配的内存
     */
    ~MemoryPool() {
        // All blocks are owned by m_blocks vector, which will be automatically cleaned up
        // 所有块由 m_blocks 向量拥有，将自动清理
        m_blocks.clear();
    }

    // Non-copyable and non-movable / 不可复制和移动
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
    MemoryPool(MemoryPool&&) = delete;
    MemoryPool& operator=(MemoryPool&&) = delete;

    /**
     * @brief Allocate an object from the pool (lock-free)
     * @brief 从池中分配一个对象（无锁）
     *
     * @return Pointer to allocated object, or nullptr if pool is exhausted
     * @return 指向分配对象的指针，如果池耗尽则返回 nullptr
     */
    T* Allocate() {
        Block* block = PopFromFreeList();
        if (block == nullptr) {
            return nullptr;
        }
        return &block->data;
    }

    /**
     * @brief Deallocate an object back to the pool (lock-free)
     * @brief 将对象释放回池中（无锁）
     *
     * @param ptr Pointer to object to deallocate / 要释放的对象指针
     */
    void Deallocate(T* ptr) {
        if (ptr == nullptr) {
            return;
        }
        // Convert T* back to Block*
        // 将 T* 转换回 Block*
        Block* block = reinterpret_cast<Block*>(
            reinterpret_cast<uint8_t*>(ptr) - offsetof(Block, data));
        PushToFreeList(block);
    }

    /**
     * @brief Get the number of available blocks in the pool
     * @brief 获取池中可用块的数量
     *
     * @return Number of available blocks / 可用块数量
     */
    size_t AvailableCount() const {
        size_t count = 0;
        Block* current = m_freeList.value.load(std::memory_order_acquire);
        while (current != nullptr) {
            ++count;
            current = current->next.load(std::memory_order_relaxed);
        }
        return count;
    }

    /**
     * @brief Get the total number of blocks in the pool
     * @brief 获取池中块的总数
     *
     * @return Total number of blocks / 块总数
     */
    size_t TotalCount() const {
        return m_totalCount.load(std::memory_order_acquire);
    }

    /**
     * @brief Expand the pool by allocating more blocks
     * @brief 通过分配更多块来扩展池
     *
     * @param count Number of blocks to add / 要添加的块数
     */
    void Expand(size_t count) {
        if (count > 0) {
            AllocateBlocks(count);
        }
    }

private:
    /**
     * @brief Memory block structure with next pointer for free list
     * @brief 带有空闲链表下一个指针的内存块结构
     */
    struct alignas(kCacheLineSize) Block {
        T data;                       ///< Actual data storage / 实际数据存储
        std::atomic<Block*> next;     ///< Next block in free list / 空闲链表中的下一个块

        Block() : next(nullptr) {}
    };

    /**
     * @brief Allocate a batch of blocks and add them to the free list
     * @brief 分配一批块并将它们添加到空闲链表
     *
     * @param count Number of blocks to allocate / 要分配的块数
     */
    void AllocateBlocks(size_t count) {
        // Allocate a new array of blocks
        // 分配新的块数组
        auto blockArray = std::make_unique<Block[]>(count);

        // Add all blocks to the free list
        // 将所有块添加到空闲链表
        for (size_t i = 0; i < count; ++i) {
            PushToFreeList(&blockArray[i]);
        }

        // Update total count
        // 更新总数
        m_totalCount.fetch_add(count, std::memory_order_release);

        // Store the block array to keep it alive
        // 存储块数组以保持其生命周期
        m_blocks.push_back(std::move(blockArray));
    }

    /**
     * @brief Push a block to the free list (lock-free)
     * @brief 将块推入空闲链表（无锁）
     *
     * @param block Block to push / 要推入的块
     */
    void PushToFreeList(Block* block) {
        Block* oldHead = m_freeList.value.load(std::memory_order_relaxed);
        do {
            block->next.store(oldHead, std::memory_order_relaxed);
        } while (!m_freeList.value.compare_exchange_weak(
            oldHead, block, std::memory_order_release, std::memory_order_relaxed));
    }

    /**
     * @brief Pop a block from the free list (lock-free)
     * @brief 从空闲链表弹出一个块（无锁）
     *
     * @return Popped block, or nullptr if list is empty / 弹出的块，如果链表为空则返回 nullptr
     */
    Block* PopFromFreeList() {
        Block* oldHead = m_freeList.value.load(std::memory_order_acquire);
        while (oldHead != nullptr) {
            Block* newHead = oldHead->next.load(std::memory_order_relaxed);
            if (m_freeList.value.compare_exchange_weak(
                    oldHead, newHead, std::memory_order_release, std::memory_order_acquire)) {
                return oldHead;
            }
            // oldHead is updated by compare_exchange_weak on failure
            // 失败时 oldHead 会被 compare_exchange_weak 更新
        }
        return nullptr;
    }

    /// Free list head (cache-line aligned to prevent false sharing)
    /// 空闲链表头（缓存行对齐以防止伪共享）
    internal::CacheLineAligned<std::atomic<Block*>> m_freeList;

    /// Storage for allocated block arrays
    /// 分配的块数组存储
    std::vector<std::unique_ptr<Block[]>> m_blocks;

    /// Total number of blocks in the pool
    /// 池中块的总数
    std::atomic<size_t> m_totalCount;
};

}  // namespace oneplog
