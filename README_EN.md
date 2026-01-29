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

### Build

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
    // Create logger
    auto logger = std::make_shared<oneplog::Logger>("my_logger", oneplog::Mode::Sync);
    auto consoleSink = std::make_shared<oneplog::ConsoleSink>();
    logger->SetSink(consoleSink);
    logger->Init();
    oneplog::SetDefaultLogger(logger);
    
    // Log messages
    logger->Info("Hello, {}!", "onePlog");
    logger->Error("Error code: {}", 42);
    
    // Using macros
    ONEPLOG_INFO("Macro logging: {}", "test");
    
    // Shutdown
    logger->Flush();
    return 0;
}
```

### Custom Configuration

```cpp
#include <oneplog/oneplog.hpp>

int main() {
    // Initialize with custom config
    oneplog::LoggerConfig config;
    config.mode = oneplog::Mode::Async;
    config.heapRingBufferSize = 8192;
    
    auto logger = std::make_shared<oneplog::Logger>("my_logger");
    logger->SetSink(std::make_shared<oneplog::ConsoleSink>());
    logger->Init(config);
    
    logger->Info("Custom logger message");
    
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
    // Child process must create a new logger
    auto logger = std::make_shared<oneplog::Logger>("child_logger", oneplog::Mode::Sync);
    auto consoleSink = std::make_shared<oneplog::ConsoleSink>();
    logger->SetSink(consoleSink);
    logger->Init();

    // Set process name for identification
    auto format = std::make_shared<oneplog::ConsoleFormat>();
    format->SetProcessName("child" + std::to_string(childId));
    logger->SetFormat(format);

    oneplog::SetDefaultLogger(logger);

    // Log messages
    logger->Info("[Child {}] Message", childId);
    logger->Flush();
}

int main() {
    // Parent logger
    auto logger = std::make_shared<oneplog::Logger>("parent_logger", oneplog::Mode::Sync);
    auto consoleSink = std::make_shared<oneplog::ConsoleSink>();
    logger->SetSink(consoleSink);
    logger->Init();

    auto format = std::make_shared<oneplog::ConsoleFormat>();
    format->SetProcessName("parent");
    logger->SetFormat(format);
    oneplog::SetDefaultLogger(logger);

    pid_t pid = fork();

    if (pid == 0) {
        // Child process
        RunChildProcess(0);
        _exit(0);  // Use _exit to avoid flushing parent's buffers
    } else if (pid > 0) {
        // Parent process
        logger->Info("[Parent] Message");
        
        int status;
        waitpid(pid, &status, 0);
    }

    logger->Flush();
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
    auto logger = std::make_shared<oneplog::Logger>("child_logger", oneplog::Mode::Sync);
    logger->SetSink(std::make_shared<oneplog::ConsoleSink>());
    logger->Init();

    auto format = std::make_shared<oneplog::ConsoleFormat>();
    format->SetProcessName("child" + std::to_string(childId));
    logger->SetFormat(format);

    logger->Info("[Child {}] Started, PID={}", childId, getpid());
    logger->Flush();
}

// Parent process entry
void RunAsParent(const char* programPath) {
    auto logger = std::make_shared<oneplog::Logger>("parent_logger", oneplog::Mode::Sync);
    logger->SetSink(std::make_shared<oneplog::ConsoleSink>());
    logger->Init();

    auto format = std::make_shared<oneplog::ConsoleFormat>();
    format->SetProcessName("parent");
    logger->SetFormat(format);

    logger->Info("[Parent] Starting, PID={}", getpid());

    // Launch child process using posix_spawn
    pid_t pid;
    char* argv[] = {
        const_cast<char*>(programPath),
        const_cast<char*>("child"),
        const_cast<char*>("0"),
        nullptr
    };

    if (posix_spawn(&pid, programPath, nullptr, nullptr, argv, environ) == 0) {
        logger->Info("[Parent] Spawned child with PID={}", pid);
        
        int status;
        waitpid(pid, &status, 0);
    }

    logger->Flush();
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
| `tests` | Build tests | true |
| `examples` | Build examples | true |
| `use_fmt` | Use fmt library | true |

### CMake Options

| Option | Description | Default |
|--------|-------------|---------|
| `ONEPLOG_BUILD_SHARED` | Build shared library | OFF |
| `ONEPLOG_HEADER_ONLY` | Header-only mode | OFF |
| `ONEPLOG_BUILD_TESTS` | Build tests | ON |
| `ONEPLOG_BUILD_EXAMPLES` | Build examples | ON |
| `ONEPLOG_USE_FMT` | Use fmt library | ON |

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

Fair comparison using the same output format (message only):

| Test | onePlog | spdlog | Comparison |
|------|---------|--------|------------|
| Sync Mode (Null Sink) | 14.7M ops/sec | 15.9M ops/sec | -7.5% |
| Async Mode (1 Thread) | 9.7M ops/sec | 4.6M ops/sec | +111% |
| Async Mode (4 Threads) | 7.5M ops/sec | 3.1M ops/sec | +147% |
| Sync File Output | 9.9M ops/sec | 9.3M ops/sec | +7% |
| Async File Output | 14.1M ops/sec | 4.9M ops/sec | +188% |
| Async File (4 Threads) | 8.2M ops/sec | 3.3M ops/sec | +150% |

**Key Optimizations**:
- Sync mode uses `fmt::memory_buffer` stack buffer for zero heap allocation
- Async mode uses lock-free ring buffer for excellent high-concurrency performance
- Formatters fetch metadata (thread ID, process ID, etc.) on demand

Run performance benchmark:
```bash
xmake f -m release
xmake -r benchmark_compare
./build/macosx/arm64/release/benchmark_compare -i 500000
```

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
- [x] Global API (DefaultLogger, convenience functions, process/module names)
- [x] Macros (ONEPLOG_TRACE/DEBUG/INFO/WARN/ERROR/CRITICAL, WFC, conditional logging, compile-time disable)
- [x] MemoryPool implementation (lock-free memory pool, pre-allocation, allocate/deallocate)
- [x] Example code (sync mode, async mode, multi-process mode, exec child process, WFC)

## License

MIT License
