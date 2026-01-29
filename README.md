# onePlog

高性能 C++17 多进程日志系统

## 特性

- **三种运行模式**：同步、异步、多进程
- **零拷贝**：静态字符串仅存储指针
- **无锁队列**：高并发场景下的最佳性能
- **灵活格式化**：支持控制台、文件、JSON 格式
- **多种输出目标**：控制台、文件（支持轮转）、网络
- **彩色输出**：Release 模式支持 ANSI 颜色
- **fmt 库支持**：可选使用 fmt 库进行格式化

## 快速开始

### 构建

使用 XMake：
```bash
xmake
```

使用 CMake：
```bash
mkdir build && cd build
cmake ..
make
```

### 基本用法

```cpp
#include <oneplog/oneplog.hpp>

int main() {
    // 创建日志器
    auto logger = std::make_shared<oneplog::Logger>("my_logger", oneplog::Mode::Sync);
    auto consoleSink = std::make_shared<oneplog::ConsoleSink>();
    logger->SetSink(consoleSink);
    logger->Init();
    oneplog::SetDefaultLogger(logger);
    
    // 记录日志
    logger->Info("Hello, {}!", "onePlog");
    logger->Error("Error code: {}", 42);
    
    // 使用宏
    ONEPLOG_INFO("Macro logging: {}", "test");
    
    // 关闭
    logger->Flush();
    return 0;
}
```

### 自定义配置

```cpp
#include <oneplog/oneplog.hpp>

int main() {
    // 使用自定义配置初始化
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

## 日志级别

| 级别 | 全称 | 4字符 | 1字符 | 颜色 |
|------|------|-------|-------|------|
| Trace | trace | TRAC | T | 无 |
| Debug | debug | DBUG | D | 蓝色 |
| Info | info | INFO | I | 绿色 |
| Warn | warn | WARN | W | 黄色 |
| Error | error | ERRO | E | 红色 |
| Critical | critical | CRIT | C | 紫色 |

## 运行模式

- **同步模式 (Sync)**：日志直接在调用线程中输出，适合调试
- **异步模式 (Async)**：日志通过无锁队列传递给后台线程，高性能
- **多进程模式 (MProc)**：多个进程共享同一个日志输出

## 格式化器

### 控制台格式化器 (ConsoleFormat)

Debug 模式输出：
```
[15:20:23:123] [INFO] [进程名:PID] [模块名:TID] 消息
```

Release 模式输出（带颜色）：
```
[15:20:23] [INFO] [进程名] [模块名] 消息
```

### 文件格式化器 (FileFormat)

输出完整信息，包括文件名、行号、函数名。

### JSON 格式化器 (JsonFormat)

输出 JSON 格式，适合数据库存储和日志分析。

## 多进程使用

### Fork 子进程

在 fork 子进程时，子进程需要创建自己的日志器：

```cpp
#include <oneplog/oneplog.hpp>
#include <unistd.h>
#include <sys/wait.h>

void RunChildProcess(int childId) {
    // 子进程必须创建新的日志器
    auto logger = std::make_shared<oneplog::Logger>("child_logger", oneplog::Mode::Sync);
    auto consoleSink = std::make_shared<oneplog::ConsoleSink>();
    logger->SetSink(consoleSink);
    logger->Init();

    // 设置进程名称以便识别
    auto format = std::make_shared<oneplog::ConsoleFormat>();
    format->SetProcessName("child" + std::to_string(childId));
    logger->SetFormat(format);

    oneplog::SetDefaultLogger(logger);

    // 记录日志
    logger->Info("[Child {}] Message", childId);
    logger->Flush();
}

int main() {
    // 父进程日志器
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
        // 子进程
        RunChildProcess(0);
        _exit(0);  // 使用 _exit 避免刷新父进程缓冲区
    } else if (pid > 0) {
        // 父进程
        logger->Info("[Parent] Message");
        
        int status;
        waitpid(pid, &status, 0);
    }

    logger->Flush();
    return 0;
}
```

### Exec 子进程（非 fork）

使用 `posix_spawn` 或 `exec` 启动独立子进程时，每个进程独立初始化日志器：

```cpp
#include <oneplog/oneplog.hpp>
#include <spawn.h>
#include <sys/wait.h>

extern char** environ;

// 子进程入口（通过命令行参数判断）
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

// 父进程入口
void RunAsParent(const char* programPath) {
    auto logger = std::make_shared<oneplog::Logger>("parent_logger", oneplog::Mode::Sync);
    logger->SetSink(std::make_shared<oneplog::ConsoleSink>());
    logger->Init();

    auto format = std::make_shared<oneplog::ConsoleFormat>();
    format->SetProcessName("parent");
    logger->SetFormat(format);

    logger->Info("[Parent] Starting, PID={}", getpid());

    // 使用 posix_spawn 启动子进程
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

### 多进程注意事项

1. **Fork 后必须重新创建日志器**：fork 后子进程继承父进程的内存，但后台线程不会被继承
2. **使用 `_exit()` 退出子进程**：避免刷新父进程的 I/O 缓冲区
3. **设置进程名称**：使用 `ConsoleFormat::SetProcessName()` 区分不同进程的日志
4. **同步模式更安全**：多进程场景建议使用同步模式，避免竞争条件

## 构建选项

### XMake 选项

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `shared` | 构建动态库 | false |
| `headeronly` | 仅头文件模式 | false |
| `tests` | 构建测试 | true |
| `examples` | 构建示例 | true |
| `use_fmt` | 使用 fmt 库 | true |

### CMake 选项

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `ONEPLOG_BUILD_SHARED` | 构建动态库 | OFF |
| `ONEPLOG_HEADER_ONLY` | 仅头文件模式 | OFF |
| `ONEPLOG_BUILD_TESTS` | 构建测试 | ON |
| `ONEPLOG_BUILD_EXAMPLES` | 构建示例 | ON |
| `ONEPLOG_USE_FMT` | 使用 fmt 库 | ON |

## 示例程序

| 示例 | 说明 |
|------|------|
| `sync_example` | 同步模式示例 |
| `async_example` | 异步模式示例 |
| `mproc_example` | Fork 多进程示例 |
| `exec_example` | Exec 子进程示例 |
| `wfc_example` | WFC（等待完成）示例 |
| `benchmark` | 性能基准测试 |

运行示例：
```bash
xmake run sync_example
xmake run async_example
xmake run mproc_example
xmake run exec_example
xmake run wfc_example
xmake run benchmark
```

## 性能测试

在 Apple M4 Pro (14 核) macOS 上的测试结果：

### 组件性能

| 测试项 | 吞吐量 | 平均延迟 | P99 延迟 |
|--------|--------|----------|----------|
| BinarySnapshot 捕获 | 3475 万 ops/sec | 15 ns | 42 ns |
| HeapRingBuffer 入队/出队 | 2662 万 ops/sec | 25 ns | 125 ns |
| 格式化 | 277 万 ops/sec | 336 ns | 459 ns |

### 与 spdlog 性能对比

使用相同输出格式（仅消息）进行公平对比：

| 测试项 | onePlog | spdlog | 对比 |
|--------|---------|--------|------|
| 同步模式（Null Sink） | 1469 万 ops/sec | 1588 万 ops/sec | -7.5% |
| 异步模式（单线程） | 966 万 ops/sec | 458 万 ops/sec | +111% |
| 异步模式（4线程） | 753 万 ops/sec | 305 万 ops/sec | +147% |
| 同步文件输出 | 990 万 ops/sec | 925 万 ops/sec | +7% |
| 异步文件输出 | 1409 万 ops/sec | 490 万 ops/sec | +188% |
| 异步文件（4线程） | 822 万 ops/sec | 329 万 ops/sec | +150% |

**关键优化**：
- 同步模式使用 `fmt::memory_buffer` 栈缓冲区，实现零堆分配
- 异步模式使用无锁环形队列，高并发场景性能优异
- 格式化器按需获取元数据（线程ID、进程ID等）

运行性能测试：
```bash
xmake f -m release
xmake -r benchmark_compare
./build/macosx/arm64/release/benchmark_compare -i 500000
```

## 开发进度

- [x] 项目基础设施
- [x] 核心类型定义（Level、Mode、SlotState、ErrorCode）
- [x] BinarySnapshot 实现
- [x] LogEntry 实现（SourceLocation、LogEntryDebug、LogEntryRelease）
- [x] HeapRingBuffer 实现（无锁环形队列、WFC 支持、通知机制、队列满策略）
- [x] SharedRingBuffer 实现（共享内存环形队列，继承自 RingBufferBase）
- [x] SharedMemory 管理器（元数据、配置、进程/线程名称表）
- [x] Format 实现（ConsoleFormat、FileFormat、JsonFormat、PatternFormat）
- [x] Sink 实现（ConsoleSink、FileSink、NetworkSink）
- [x] PipelineThread 实现（多进程模式管道线程）
- [x] WriterThread 实现（日志输出线程）
- [x] Logger 实现（Sync/Async/MProc 模式、WFC 支持）
- [x] 全局 API（DefaultLogger、便捷函数、进程/模块名称）
- [x] 宏定义（ONEPLOG_TRACE/DEBUG/INFO/WARN/ERROR/CRITICAL、WFC、条件日志、编译时禁用）
- [x] MemoryPool 实现（无锁内存池、预分配、分配/释放）
- [x] 示例代码（同步模式、异步模式、多进程模式、Exec 子进程、WFC）

## 许可证

MIT License
