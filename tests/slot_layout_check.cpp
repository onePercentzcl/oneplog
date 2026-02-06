/**
 * @file slot_layout_check.cpp
 * @brief Verify Slot memory layout
 * @brief 验证 Slot 内存布局
 */

#include "oneplog/ring_buffer.hpp"
#include <iostream>
#include <cstddef>

using namespace oneplog::internal;

int main() {
    Slot<512> slot;
    
    std::cout << "=== Slot Memory Layout ===" << std::endl;
    std::cout << "sizeof(Slot<512>): " << sizeof(Slot<512>) << std::endl;
    std::cout << "alignof(Slot<512>): " << alignof(Slot<512>) << std::endl;
    std::cout << "kHeaderSize: " << Slot<512>::kHeaderSize << std::endl;
    std::cout << "DataSize(): " << Slot<512>::DataSize() << std::endl;
    std::cout << std::endl;
    
    std::cout << "Member offsets:" << std::endl;
    std::cout << "  m_state:    offset " << offsetof(Slot<512>, m_state) << ", size " << sizeof(slot.m_state) << std::endl;
    std::cout << "  m_padding1: offset " << offsetof(Slot<512>, m_padding1) << ", size " << sizeof(slot.m_padding1) << std::endl;
    std::cout << "  m_dataSize: offset " << offsetof(Slot<512>, m_dataSize) << ", size " << sizeof(slot.m_dataSize) << std::endl;
    std::cout << "  m_padding2: offset " << offsetof(Slot<512>, m_padding2) << ", size " << sizeof(slot.m_padding2) << std::endl;
    std::cout << "  m_item:     offset " << offsetof(Slot<512>, m_item) << ", size " << sizeof(slot.m_item) << std::endl;
    std::cout << std::endl;
    
    // Verify alignment
    std::cout << "Alignment checks:" << std::endl;
    std::cout << "  m_item offset % 8 == " << (offsetof(Slot<512>, m_item) % 8) << " (should be 0)" << std::endl;
    std::cout << "  Slot aligned to cache line: " << (alignof(Slot<512>) == 64 ? "YES" : "NO") << std::endl;
    
    return 0;
}
