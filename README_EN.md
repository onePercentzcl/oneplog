# onePlog

High Performance C++17 Multi-Process Logging System

## Features

- **Three Operating Modes**: Sync, Async, Multi-process
- **Zero-Copy**: Static strings store only pointers
- **Lock-Free Queue**: Best performance in high-concurrency scenarios
- **Flexible Formatting**: Pattern strings and JSON format support
- **Multiple Sinks**: Console, File (with rotation), Network

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
    // Initialize logging system
    oneplog::Logger::GetInstance().Init({
        .mode = oneplog::Mode::Async
    });
    
    // Log messages
    ONEPLOG_INFO("Hello, {}!", "onePlog");
    ONEPLOG_ERROR("Error code: {}", 42);
    
    // Shutdown
    oneplog::Shutdown();
    return 0;
}
```

## Log Levels

| Level | Full | Short4 | Short1 |
|-------|------|--------|--------|
| Trace | trace | TRAC | T |
| Debug | debug | DBUG | D |
| Info | info | INFO | I |
| Warn | warn | WARN | W |
| Error | error | ERRO | E |
| Critical | critical | CRIT | C |

## Operating Modes

- **Sync Mode**: Logs are output directly in the calling thread
- **Async Mode**: Logs are passed to a background thread via lock-free queue
- **Multi-Process Mode**: Multiple processes share the same log output

## Build Options

| Option | Description | Default |
|--------|-------------|---------|
| `ONEPLOG_BUILD_SHARED` | Build shared library | OFF |
| `ONEPLOG_HEADER_ONLY` | Header-only mode | OFF |
| `ONEPLOG_BUILD_TESTS` | Build tests | ON |
| `ONEPLOG_BUILD_EXAMPLES` | Build examples | ON |
| `ONEPLOG_USE_FMT` | Use fmt library | OFF |

## Development Progress

- [x] Project infrastructure
- [x] Core type definitions (Level, Mode, SlotState, ErrorCode)
- [x] BinarySnapshot implementation
- [x] LogEntry implementation (SourceLocation, LogEntryDebug, LogEntryRelease)
- [x] HeapRingBuffer implementation (lock-free ring buffer, WFC support, notification mechanism, queue full policies)
- [x] SharedRingBuffer implementation (shared memory ring buffer, inherits from RingBufferBase)
- [x] SharedMemory manager (metadata, config, process/thread name table)
- [x] Format implementation (PatternFormat, JsonFormat)
- [x] Sink implementation (ConsoleSink, FileSink, NetworkSink)
- [x] PipelineThread implementation (multi-process mode pipeline thread)
- [x] WriterThread implementation (log output thread)
- [ ] Logger implementation

## License

MIT License
