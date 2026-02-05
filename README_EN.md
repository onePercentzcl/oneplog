# onePlog

High-performance C++20 multi-process aggregated logging system

## Features

- **Zero-copy**: Uses BinarySnapshot to capture parameters, static strings only store pointers
- **Zero-allocation**: Uses pre-allocated memory pools, avoiding runtime heap allocation
- **Low-latency**: Uses lock-free ring buffers with acquire-index-then-write approach
- **Compile-time optimization**: Disabled features produce no runtime overhead
- **Multi-mode support**: Sync, Async, and Multi-process operating modes
- **Cross-platform**: Supports Linux, macOS, HarmonyOS, OpenHarmony

## Operating Modes

| Mode | Description | Use Case |
|------|-------------|----------|
| Sync | Synchronous mode, formatting and output in calling thread | Debugging, simple applications |
| Async | Asynchronous mode, transfers via lock-free queue to background thread | High-performance applications |
| MProc | Multi-process mode, aggregates logs from multiple processes to consumer | Distributed systems |

## Quick Start

### Basic Usage

```cpp
#include <oneplog/oneplog.hpp>

int main() {
    // Initialize
    oneplog::Init();
    
    // Log messages
    oneplog::Info("Hello, {}!", "onePlog");
    oneplog::Debug("Debug message: value = {}", 42);
    oneplog::Error("Error occurred: {}", "file not found");
    
    // Shutdown
    oneplog::Shutdown();
    return 0;
}
```

### Using Global Configuration

```cpp
#include <oneplog/oneplog.hpp>

int main() {
    // Set global configuration
    oneplog::config::mode = oneplog::Mode::Async;
    oneplog::config::level = oneplog::Level::Debugging;
    oneplog::config::heapSlotCount = 2048;
    oneplog::config::processName = "myapp";
    
    // Initialize with global configuration
    oneplog::Init();
    
    oneplog::Info("Application started");
    
    oneplog::Shutdown();
    return 0;
}
```

### Using LoggerConfig

```cpp
#include <oneplog/oneplog.hpp>

int main() {
    oneplog::LoggerConfig config;
    config.mode = oneplog::Mode::Async;
    config.level = oneplog::Level::Informational;
    config.heapRingBufferSlotCount = 4096;
    config.queueFullPolicy = oneplog::QueueFullPolicy::DropNewest;
    config.processName = "myapp";
    
    oneplog::Init(config);
    
    oneplog::Info("Application started with custom config");
    
    oneplog::Shutdown();
    return 0;
}
```

### Using Macros

```cpp
#include <oneplog/oneplog.hpp>

int main() {
    oneplog::Init();
    
    ONEPLOG_INFO("Info message");
    ONEPLOG_DEBUG("Debug: x = {}", 100);
    ONEPLOG_ERROR("Error: {}", "something went wrong");
    
    // Conditional logging
    ONEPLOG_IF(shouldLog, INFO, "Conditional message");
    
    oneplog::Shutdown();
    return 0;
}
```

## Log Levels

| Level | Value | Description |
|-------|-------|-------------|
| Alert | 0 | Action must be taken immediately |
| Critical | 1 | Critical conditions |
| Error | 2 | Runtime errors |
| Warning | 3 | Warning messages |
| Notice | 4 | Normal but significant conditions |
| Informational | 5 | Informational messages |
| Debugging | 6 | Debug messages |
| Trace | 7 | Most detailed trace information |
| Off | 8 | Disable logging |

## Building

### CMake

```bash
mkdir build && cd build
cmake ..
make
```

### XMake

```bash
xmake
```

## Project Structure

```
oneplog/
├── src/                          # Source code directory
├── include/                      # Header files directory
├── example/                      # Example code
├── tests/                        # Test code
├── docs/                         # Documentation directory
├── CMakeLists.txt                # CMake build file
├── xmake.lua                     # XMake build file
└── .clang-format                 # clang-format configuration
```

## Development Status

Currently in template refactoring phase. See design documents in `.kiro/specs/template-logger-refactor/` directory.

## License

MIT License
