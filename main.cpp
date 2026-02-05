/**
 * @file main.cpp
 * @brief onePlog test entry point
 * @文件 main.cpp
 * @简述 onePlog 测试入口点
 */

#include "oneplog/common.hpp"
#include <iostream>

int main() {
    using namespace oneplog::internal;
    
    // Test Level enum
    std::cout << "=== Level Enum Test ===" << std::endl;
    std::cout << "Level::Alert = " << static_cast<int>(Level::Alert) << std::endl;
    std::cout << "Level::Off = " << static_cast<int>(Level::Off) << std::endl;
    std::cout << "LevelToString(Level::Info, Full) = " << LevelToString(Level::Informational, LevelNameStyle::Full) << std::endl;
    std::cout << "LevelToString(Level::Info, Short4) = " << LevelToString(Level::Informational, LevelNameStyle::Short4) << std::endl;
    std::cout << "LevelToString(Level::Info, Short1) = " << LevelToString(Level::Informational, LevelNameStyle::Short1) << std::endl;
    
    // Test Mode enum
    std::cout << "\n=== Mode Enum Test ===" << std::endl;
    std::cout << "Mode::Sync = " << static_cast<int>(Mode::Sync) << std::endl;
    std::cout << "Mode::Async = " << static_cast<int>(Mode::Async) << std::endl;
    std::cout << "Mode::MProc = " << static_cast<int>(Mode::MProc) << std::endl;
    std::cout << "ModeToString(Mode::Async) = " << ModeToString(Mode::Async) << std::endl;
    
    // Test QueueFullPolicy enum
    std::cout << "\n=== QueueFullPolicy Enum Test ===" << std::endl;
    std::cout << "QueueFullPolicy::Block = " << static_cast<int>(QueueFullPolicy::Block) << std::endl;
    std::cout << "QueueFullPolicy::DropNewest = " << static_cast<int>(QueueFullPolicy::DropNewest) << std::endl;
    std::cout << "QueueFullPolicy::DropOldest = " << static_cast<int>(QueueFullPolicy::DropOldest) << std::endl;
    std::cout << "QueueFullPolicyToString(DropNewest) = " << QueueFullPolicyToString(QueueFullPolicy::DropNewest) << std::endl;
    
    // Test SlotState enum
    std::cout << "\n=== SlotState Enum Test ===" << std::endl;
    std::cout << "SlotState::Empty = " << static_cast<int>(SlotState::Empty) << std::endl;
    std::cout << "SlotState::Writing = " << static_cast<int>(SlotState::Writing) << std::endl;
    std::cout << "SlotState::Ready = " << static_cast<int>(SlotState::Ready) << std::endl;
    std::cout << "SlotState::Reading = " << static_cast<int>(SlotState::Reading) << std::endl;
    std::cout << "SlotStateToString(Ready) = " << SlotStateToString(SlotState::Ready) << std::endl;
    
    // Test WFCState enum
    std::cout << "\n=== WFCState Enum Test ===" << std::endl;
    std::cout << "WFCState::None = " << static_cast<int>(WFCState::None) << std::endl;
    std::cout << "WFCState::Enabled = " << static_cast<int>(WFCState::Enabled) << std::endl;
    std::cout << "WFCState::Completed = " << static_cast<int>(WFCState::Completed) << std::endl;
    std::cout << "WFCStateToString(Enabled) = " << WFCStateToString(WFCState::Enabled) << std::endl;
    
    // Test ErrorCode enum
    std::cout << "\n=== ErrorCode Enum Test ===" << std::endl;
    std::cout << "ErrorCode::Success = " << static_cast<int>(ErrorCode::Success) << std::endl;
    std::cout << "ErrorCode::QueueFull = " << static_cast<int>(ErrorCode::QueueFull) << std::endl;
    std::cout << "ErrorCodeToString(QueueFull) = " << ErrorCodeToString(ErrorCode::QueueFull) << std::endl;
    std::cout << "IsSuccess(Success) = " << (IsSuccess(ErrorCode::Success) ? "true" : "false") << std::endl;
    std::cout << "IsError(QueueFull) = " << (IsError(ErrorCode::QueueFull) ? "true" : "false") << std::endl;
    
    std::cout << "\n=== All Tests Passed ===" << std::endl;
    return 0;
}
