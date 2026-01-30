# onePlog

High Performance C++17 Multi-Process Logging System

## Features

- **Three Operating Modes**: Sync, Async, Multi-process
- **Zero-Copy**: Static strings store only pointers
- **Lock-Free Queue**: Best performance in high-concurrency scenarios
- **Flexible Formatting**: Console, File, JSON format support
- **Multiple Sinks**: Console, File (with rotation), Network
- **Color Output**: ANSI color support in Release mode
- **fmt Library Support**: Optional fmt library for formatting

## Quick Start

### Using oneplog in Your Project

#### Method 1: Git Submodule (Recommended)

```bash
# Add submodule
git submodule add https://github.com/onePercentzcl/oneplog.git third_party/oneplog
```

**CMake:**
```cmake
add_subdirectory(third_party/oneplog)
target_link_libraries(your_target PRIVATE oneplog)
```

**XMake:**
```lua
includes("third_party/oneplog")
target("your_target")
    add_deps("oneplog")
```

#### Method 2: CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(oneplog
    GIT_REPOSITORY https://github.com/onePercentzcl/oneplog.git
    GIT_TAG v0.1.1)
FetchContent_MakeAvailable(oneplog)
target_link_libraries(your_target PRIVATE oneplog)
```

#### Method 3: XMake Remote Package (Recommended)

```lua
-- xmake.lua
add_rules("mode.debug", "mode.release")
set_languages("c++17")

-- Add onePercent repository
add_repositories("onePercent-repo https://github.com/onePercentzcl/xmake-repo")

-- Add oneplog dependency
add_requires("oneplog")

target("your_target")
    set_kind("binary")
    add_files("src/*.cpp")
    add_packages("oneplog")
```

### Building oneplog Itself

Using XMake:
```bash
xmake
```

Using CMake:
```bash
mkdir build && cd build
cmake ..
make
```

### Basic Usage

```cpp
#include <oneplog/oneplog.hpp>

int main() {
    // One-line initialization (async mode + console output)
    oneplog::Init();
    
    // Use global functions for logging (recommended)
    oneplog::Info("Hello, {}!", "onePlog");
    oneplog::Error("Error code: {}", 42);
    oneplog::Debug("Debug message");  // Optimized away in Release mode
    
    // Or use log:: static class (legacy API)
    log::Info("Static class style");
    
    // Using macros
    ONEPLOG_INFO("Macro logging: {}", "test");
    
    // Flush and shutdown
    oneplog::Flush();
    oneplog::Shutdown();
    return 0;
}
```

### Custom Logger Instance

When you need non-default configuration (e.g., sync mode, custom level, WFC enabled), use a custom Logger instance:

```cpp
#include <oneplog/oneplog.hpp>

int main() {
    // Create sync mode Logger (template params: Mode, Level, EnableWFC)
    oneplog::Logger<oneplog::Mode::Sync, oneplog::Level::Debug, false> logger;
    logger.SetSink(std::make_shared<oneplog::ConsoleSink>());
    logger.SetFormat(std::make_shared<oneplog::ConsoleFormat>());
    logger.Init();
    
    logger.Info("Custom logger message");
    logger.Debug("Debug info");
    
    logger.Shutdown();
    return 0;
}
```

### Customize Default Logger Type

Customize the default Logger type used by global API via macros:

```cpp
// Define before including header
#define ONEPLOG_DEFAULT_MODE oneplog::Mode::Sync
#define ONEPLOG_DEFAULT_LEVEL oneplog::Level::Debug
#define ONEPLOG_DEFAULT_ENABLE_WFC true
#include <oneplog/oneplog.hpp>

int main() {
    oneplog::Init();  // Uses customized default type
    oneplog::Info("Now using Sync mode with Debug level");
    oneplog::Shutdown();
    return 0;
}
```

## Log Levels

| Level | Full | Short4 | Short1 | Color |
|-------|------|--------|--------|-------|
| Trace | trace | TRAC | T | None |
| Debug | debug | DBUG | D | Blue |
| Info | info | INFO | I | Green |
| Warn | warn | WARN | W | Yellow |
| Error | error | ERRO | E | Red |
| Critical | critical | CRIT | C | Magenta |

## Process/Module Name Management

onePlog supports setting process and module names for logs, making it easy to identify log sources in multi-process and multi-threaded environments.

### Design Principles

- **Process name and module name belong to process/thread, NOT to Logger object**
- Process name is a global constant, should be set once at program startup
- Module name is thread-local, each thread can set its own module name

### Setting Process Name

```cpp
#include <oneplog/oneplog.hpp>

int main() {
    // Method 1: Set process name via config
    oneplog::LoggerConfig config;
    config.processName = "my_app";
    oneplog::Init(config);
    
    // Method 2: Use global function (recommended)
    oneplog::SetProcessName("my_app");
    
    // Method 3: Use NameManager
    oneplog::NameManager<>::SetProcessName("my_app");
    
    // Get process name
    std::string name = oneplog::GetProcessName();
    
    log::Info("Process name: {}", name);
    
    oneplog::Shutdown();
    return 0;
}
```

### Setting Module Name

Each thread can set its own module name:

```cpp
#include <oneplog/oneplog.hpp>
#include <thread>

int main() {
    oneplog::LoggerConfig config;
    config.processName = "my_app";
    oneplog::Init(config);
    
    // Main thread sets module name
    oneplog::SetModuleName("main");
    log::Info("Main thread message");
    
    // Worker thread sets different module name
    std::thread worker([]() {
        oneplog::SetModuleName("worker");
        log::Info("Worker thread message");
    });
    worker.join();
    
    oneplog::Shutdown();
    return 0;
}
```

### Module Name Inheritance

When creating threads with `ThreadWithModuleName`, child threads automatically inherit the parent thread's module name:

```cpp
#include <oneplog/oneplog.hpp>

int main() {
    oneplog::Init();
    
    // Set parent thread module name
    oneplog::SetModuleName("supervisor");
    
    // Child thread automatically inherits "supervisor" module name
    auto childThread = oneplog::ThreadWithModuleName<>::Create([]() {
        // GetModuleName() returns "supervisor"
        log::Info("Child module: {}", oneplog::GetModuleName());
    });
    childThread.join();
    
    // You can also specify a specific module name for child thread
    auto namedThread = oneplog::ThreadWithModuleName<>::CreateWithName("custom_module", []() {
        log::Info("Custom module: {}", oneplog::GetModuleName());
    });
    namedThread.join();
    
    oneplog::Shutdown();
    return 0;
}
```

### Name Storage in Different Modes

| Mode | Process Name Storage | Module Name Storage | Notes |
|------|---------------------|---------------------|-------|
| Sync | Global variable | thread_local | Sink can access directly |
| Async | Global variable | thread_local + Heap TID-module table | WriterThread looks up by TID |
| MProc | Global variable + Shared memory | thread_local + Shared memory | Consumer process looks up via shared memory |

## Operating Modes

- **Sync Mode**: Logs are output directly in the calling thread, suitable for debugging
- **Async Mode**: Logs are passed to a background thread via lock-free queue, high performance
- **Multi-Process Mode (MProc)**: Multiple processes share the same log output

## Formatters

### Console Formatter (ConsoleFormat)

Debug mode output:
```
[15:20:23:123] [INFO] [process:PID] [module:TID] message
```

Release mode output (with colors):
```
[15:20:23] [INFO] [process] [module] message
```

### File Formatter (FileFormat)

Outputs full information including filename, line number, and function name.

### JSON Formatter (JsonFormat)

Outputs JSON format, suitable for database storage and log analysis.

## Multi-Process Usage

### Fork Child Process

When forking child processes, each child must create its own logger:

```cpp
#include <oneplog/oneplog.hpp>
#include <unistd.h>
#include <sys/wait.h>

void RunChildProcess(int childId) {
    // Child process uses sync mode initialization
    oneplog::LoggerConfig config;
    config.mode = oneplog::Mode::Sync;
    oneplog::Init(config);

    // Set process name for identification
    auto format = std::make_shared<oneplog::ConsoleFormat>();
    format->SetProcessName("child" + std::to_string(childId));
    oneplog::SetFormat(format);

    // Log messages
    log::Info("[Child {}] Message", childId);
    log::Flush();
}

int main() {
    // Parent process uses sync mode initialization
    oneplog::LoggerConfig config;
    config.mode = oneplog::Mode::Sync;
    oneplog::Init(config);

    auto format = std::make_shared<oneplog::ConsoleFormat>();
    format->SetProcessName("parent");
    oneplog::SetFormat(format);

    pid_t pid = fork();

    if (pid == 0) {
        // Child process - reinitialize
        oneplog::Shutdown();
        RunChildProcess(0);
        _exit(0);  // Use _exit to avoid flushing parent's buffers
    } else if (pid > 0) {
        // Parent process
        log::Info("[Parent] Message");
        
        int status;
        waitpid(pid, &status, 0);
    }

    log::Flush();
    return 0;
}
```

### Exec Child Process (Non-fork)

When launching independent child processes via `posix_spawn` or `exec`, each process initializes its logger independently:

```cpp
#include <oneplog/oneplog.hpp>
#include <spawn.h>
#include <sys/wait.h>

extern char** environ;

// Child process entry (determined by command line arguments)
void RunAsChild(int childId) {
    oneplog::LoggerConfig config;
    config.mode = oneplog::Mode::Sync;
    oneplog::Init(config);

    auto format = std::make_shared<oneplog::ConsoleFormat>();
    format->SetProcessName("child" + std::to_string(childId));
    oneplog::SetFormat(format);

    log::Info("[Child {}] Started, PID={}", childId, getpid());
    log::Flush();
}

// Parent process entry
void RunAsParent(const char* programPath) {
    oneplog::LoggerConfig config;
    config.mode = oneplog::Mode::Sync;
    oneplog::Init(config);

    auto format = std::make_shared<oneplog::ConsoleFormat>();
    format->SetProcessName("parent");
    oneplog::SetFormat(format);

    log::Info("[Parent] Starting, PID={}", getpid());

    // Launch child process using posix_spawn
    pid_t pid;
    char* argv[] = {
        const_cast<char*>(programPath),
        const_cast<char*>("child"),
        const_cast<char*>("0"),
        nullptr
    };

    if (posix_spawn(&pid, programPath, nullptr, nullptr, argv, environ) == 0) {
        log::Info("[Parent] Spawned child with PID={}", pid);
        
        int status;
        waitpid(pid, &status, 0);
    }

    log::Flush();
}

int main(int argc, char* argv[]) {
    if (argc >= 3 && strcmp(argv[1], "child") == 0) {
        RunAsChild(atoi(argv[2]));
    } else {
        RunAsParent(argv[0]);
    }
    return 0;
}
```

### Multi-Process Notes

1. **Must recreate logger after fork**: Child process inherits parent's memory, but background threads are not inherited
2. **Use `_exit()` to exit child process**: Avoid flushing parent's I/O buffers
3. **Set process name**: Use `ConsoleFormat::SetProcessName()` to distinguish logs from different processes
4. **Sync mode is safer**: Recommend using sync mode in multi-process scenarios to avoid race conditions

## Build Options

### XMake Options

| Option | Description | Default |
|--------|-------------|---------|
| `shared` | Build shared library | false |
| `headeronly` | Header-only mode | false |
| `tests` | Build tests | false |
| `examples` | Build examples | false |
| `use_fmt` | Use fmt library | true |

### CMake Options

| Option | Description | Default |
|--------|-------------|---------|
| `ONEPLOG_BUILD_SHARED` | Build shared library | OFF |
| `ONEPLOG_HEADER_ONLY` | Header-only mode | OFF |
| `ONEPLOG_BUILD_TESTS` | Build tests | OFF |
| `ONEPLOG_BUILD_EXAMPLES` | Build examples | OFF |
| `ONEPLOG_USE_FMT` | Use fmt library | OFF |

### Compile Macros

#### Mode Selection Macros

| Macro | Description |
|-------|-------------|
| `ONEPLOG_SYNC_ONLY` | Only compile sync mode code |
| `ONEPLOG_ASYNC_ONLY` | Only compile async mode code |
| `ONEPLOG_MPROC_ONLY` | Only compile multi-process mode code |

Note: These three macros are mutually exclusive.

#### Compile-time Log Level

Use `ONEPLOG_ACTIVE_LEVEL` macro to completely remove log code below the specified level at compile time:

| Value | Level | Description |
|-------|-------|-------------|
| 0 | Trace | Default, all levels enabled |
| 1 | Debug | Remove Trace |
| 2 | Info | Remove Trace, Debug |
| 3 | Warn | Remove Trace, Debug, Info |
| 4 | Error | Remove Trace, Debug, Info, Warn |
| 5 | Critical | Only Critical |
| 6 | Off | Disable all logging |

Example:
```cpp
// Define before including header
#define ONEPLOG_ACTIVE_LEVEL 3  // Only Warn and above will be compiled
#include <oneplog/oneplog.hpp>
```

Or in CMake:
```cmake
target_compile_definitions(your_target PRIVATE ONEPLOG_ACTIVE_LEVEL=3)
```

## Example Programs

| Example | Description |
|---------|-------------|
| `sync_example` | Sync mode example |
| `async_example` | Async mode example |
| `mproc_example` | Fork multi-process example |
| `exec_example` | Exec child process example |
| `wfc_example` | WFC (Wait For Completion) example |
| `benchmark` | Performance benchmark |

Run examples:
```bash
xmake run sync_example
xmake run async_example
xmake run mproc_example
xmake run exec_example
xmake run wfc_example
xmake run benchmark
```

## Performance Benchmark

Test results on Apple M4 Pro (14 cores) macOS:

### Component Performance

| Test | Throughput | Avg Latency | P99 Latency |
|------|------------|-------------|-------------|
| BinarySnapshot Capture | 34.8M ops/sec | 15 ns | 42 ns |
| HeapRingBuffer Push/Pop | 26.6M ops/sec | 25 ns | 125 ns |
| Format | 2.77M ops/sec | 336 ns | 459 ns |

### Comparison with spdlog

Fair comparison using the same output format (message only). Results are average ± standard deviation over 100 runs:

| Test | onePlog | spdlog | Comparison |
|------|---------|--------|------------|
| Sync Mode (Null Sink) | 15.1M ± 1.0M ops/sec | 15.1M ± 0.6M ops/sec | +0.2% |
| Async Mode (1 Thread) | 19.4M ± 1.6M ops/sec | 4.8M ± 0.4M ops/sec | +300% |
| Async Mode (4 Threads) | 8.8M ± 0.6M ops/sec | 3.2M ± 0.04M ops/sec | +180% |
| Sync File Output | 10.1M ± 0.1M ops/sec | 8.8M ± 0.2M ops/sec | +15% |
| Async File Output | 19.5M ± 0.2M ops/sec | 5.0M ± 0.1M ops/sec | +290% |
| Async File (4 Threads) | 9.3M ± 0.5M ops/sec | 3.4M ± 0.03M ops/sec | +172% |

**Key Optimizations**:
- Sync mode uses `fmt::memory_buffer` stack buffer for zero heap allocation
- Async mode uses lock-free ring buffer for excellent high-concurrency performance
- Formatters fetch metadata (thread ID, process ID, etc.) on demand

Run performance benchmark:
```bash
xmake f -m release
xmake -r benchmark_compare
./build/macosx/arm64/release/benchmark_compare -i 500000 -r 5
```

### WFC Compile-Time Overhead Test

Test the performance overhead of enabling WFC compile-time flag (`EnableWFC=true`) without using WFC methods (average ± standard deviation over 100 runs):

| Test | WFC Disabled | WFC Enabled | Overhead |
|------|--------------|-------------|----------|
| Async Mode (1 Thread) | 28.1M ± 4.2M ops/sec | 28.8M ± 5.0M ops/sec | +2.4% (noise) |
| Async Mode (4 Threads) | 13.4M ± 1.3M ops/sec | 13.1M ± 1.2M ops/sec | -2.1% (noise) |
| MProc Mode (1 Thread) | 15.4M ± 3.2M ops/sec | 16.4M ± 3.5M ops/sec | +6.2% (noise) |
| MProc Mode (4 Threads) | 10.1M ± 1.0M ops/sec | 10.2M ± 1.2M ops/sec | no significant difference |

**Note**: In the current implementation, the consumer thread (WriterThread) checks the WFC flag (`IsWFCEnabled()`) for every log entry read, regardless of whether the `EnableWFC` template parameter is `true` or `false`. This is because `RingBufferBase` doesn't know about the template parameter.

**Why the overhead is small**:
- Atomic read (`atomic<uint8_t>::load`) is very fast on modern CPUs (~1-2 nanoseconds)
- CPU branch predictor handles the almost-always-false branch very well
- The `wfc` field is in the same cache line as `state`, already loaded when reading `state`

**Conclusion**: The WFC compile-time flag has negligible overhead when not using WFC methods.

Run WFC overhead test:
```bash
cd example
xmake -P . -m release
./build/macosx/arm64/release/benchmark_wfc_overhead -r 100
```

Command line arguments:
- `-i <iterations>`: Number of iterations per test (default: 500000)
- `-t <threads>`: Number of threads for multi-threaded tests (default: 4)
- `-r <runs>`: Number of runs for calculating average and standard deviation (default: 100)

## Development Progress

- [x] Project infrastructure
- [x] Core type definitions (Level, Mode, SlotState, ErrorCode)
- [x] BinarySnapshot implementation
- [x] LogEntry implementation (SourceLocation, LogEntryDebug, LogEntryRelease)
- [x] HeapRingBuffer implementation (lock-free ring buffer, WFC support, notification mechanism, queue full policies)
- [x] SharedRingBuffer implementation (shared memory ring buffer, inherits from RingBufferBase)
- [x] SharedMemory manager (metadata, config, process/thread name table)
- [x] Format implementation (ConsoleFormat, FileFormat, JsonFormat, PatternFormat)
- [x] Sink implementation (ConsoleSink, FileSink, NetworkSink)
- [x] PipelineThread implementation (multi-process mode pipeline thread)
- [x] WriterThread implementation (log output thread)
- [x] Logger implementation (Sync/Async/MProc modes, WFC support)
- [x] Template Logger implementation (compile-time mode/level selection, zero-overhead abstraction)
- [x] Global API (DefaultLogger, convenience functions, process/module names)
- [x] Macros (ONEPLOG_TRACE/DEBUG/INFO/WARN/ERROR/CRITICAL, WFC, conditional logging, compile-time disable)
- [x] MemoryPool implementation (lock-free memory pool, pre-allocation, allocate/deallocate)
- [x] Example code (sync mode, async mode, multi-process mode, exec child process, WFC)

## Template Logger

onePlog uses a template-based `Logger` class that allows specifying operating mode, minimum log level, and WFC functionality at compile time, achieving zero-overhead abstraction.

### Template Parameters

```cpp
template<Mode M = Mode::Async, Level L = kDefaultLevel, bool EnableWFC = false>
class Logger;
```

- `M`: Operating mode (`Mode::Sync`, `Mode::Async`, `Mode::MProc`)
- `L`: Compile-time minimum log level (log calls below this level are optimized away)
- `EnableWFC`: Whether to enable WFC (Wait For Completion) functionality

### Type Aliases

```cpp
// Predefined type aliases
oneplog::SyncLogger<>           // Sync mode, default level
oneplog::AsyncLogger<>          // Async mode, default level
oneplog::MProcLogger<>          // Multi-process mode, default level

oneplog::DebugLogger            // Async mode, Debug level
oneplog::ReleaseLogger          // Async mode, Info level
oneplog::DebugLoggerWFC         // Async mode, Debug level, WFC enabled
oneplog::ReleaseLoggerWFC       // Async mode, Info level, WFC enabled
```

### Compile-time Level Filtering

The template parameter `L` specifies the compile-time minimum log level. Log calls below this level are completely optimized away by the compiler:

```cpp
// Minimum level is Warn
oneplog::Logger<oneplog::Mode::Async, oneplog::Level::Warn> logger;

logger.Trace("...");    // Compiles to no-op
logger.Debug("...");    // Compiles to no-op
logger.Info("...");     // Compiles to no-op
logger.Warn("...");     // Logged normally
logger.Error("...");    // Logged normally
logger.Critical("..."); // Logged normally
```

### WFC Functionality

WFC (Wait For Completion) functionality can be enabled or disabled at compile time:

```cpp
// WFC enabled
oneplog::Logger<oneplog::Mode::Async, oneplog::Level::Debug, true> logger;
logger.InfoWFC("This will wait for completion");

// WFC disabled (default)
oneplog::Logger<oneplog::Mode::Async, oneplog::Level::Debug, false> logger2;
logger2.InfoWFC("This degrades to normal Info()");  // Degrades to normal log
```

## License

MIT License
