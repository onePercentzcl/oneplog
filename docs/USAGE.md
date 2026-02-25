# onePlog 使用说明文档

本文档详细介绍 onePlog 日志库的所有配置选项和使用方法。

## 目录

- [快速开始](#快速开始)
- [使用 xmake 包管理器安装](#使用-xmake-包管理器安装)
- [使用 CMake FetchContent 安装](#使用-cmake-fetchcontent-安装)
- [运行模式](#运行模式)
- [编译期配置 (LoggerConfig)](#编译期配置-loggerconfig)
- [运行时配置 (RuntimeConfig)](#运行时配置-runtimeconfig)
- [Sink 配置](#sink-配置)
- [Format 配置](#format-配置)
- [预设配置](#预设配置)
- [高级用法](#高级用法)
- [编译选项](#编译选项)

---

## 快速开始

### 最简单的使用方式

```cpp
#include <oneplog/logger.hpp>

int main() {
    // 使用默认配置创建同步日志器
    oneplog::SyncLogger logger;
    
    logger.Info("Hello, {}!", "World");
    logger.Debug("Debug message with value: {}", 42);
    logger.Error("Error occurred: {}", "something went wrong");
    
    return 0;
}
```

### 异步日志器

```cpp
#include <oneplog/logger.hpp>

int main() {
    // 使用默认配置创建异步日志器
    oneplog::AsyncLogger logger;
    
    logger.Info("Async logging message");
    
    // 确保所有日志都被写入
    logger.Flush();
    
    return 0;
}
```

### 使用 xmake 包管理器安装

1. 添加远程仓库：

```bash
xmake repo -a oneplog-repo https://github.com/onePercentzcl/xmake-repo.git
```

2. 在 `xmake.lua` 中添加依赖：

```lua
add_requires("oneplog")

target("myapp")
    set_kind("binary")
    add_files("src/*.cpp")
    add_packages("oneplog")
```

3. 可选配置：

```lua
-- 指定版本
add_requires("oneplog v0.2.1")

-- 使用共享库模式（默认为 header-only）
add_requires("oneplog", {configs = {header_only = false}})

-- 使用共享库
add_requires("oneplog", {configs = {header_only = false, shared = true}})
```

### 使用 CMake FetchContent 安装

1. 在 `CMakeLists.txt` 中添加：

```cmake
include(FetchContent)

FetchContent_Declare(
    oneplog
    GIT_REPOSITORY https://github.com/onePercentzcl/oneplog.git
    GIT_TAG        v0.2.1
)

FetchContent_MakeAvailable(oneplog)

target_link_libraries(myapp PRIVATE oneplog::oneplog)
```

2. 或者使用 URL 方式：

```cmake
include(FetchContent)

FetchContent_Declare(
    oneplog
    URL https://github.com/onePercentzcl/oneplog/archive/refs/tags/v0.2.1.tar.gz
)

FetchContent_MakeAvailable(oneplog)

target_link_libraries(myapp PRIVATE oneplog::oneplog)
```

3. 完整示例：

```cmake
cmake_minimum_required(VERSION 3.14)
project(myapp)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
    oneplog
    GIT_REPOSITORY https://github.com/onePercentzcl/oneplog.git
    GIT_TAG        v0.2.1
)

FetchContent_MakeAvailable(oneplog)

add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE oneplog::oneplog)
```

---

## 运行模式

onePlog 支持三种运行模式：

### 1. 同步模式 (Mode::Sync)

- **特点**: 日志消息在调用线程中直接格式化并写入 Sink
- **优点**: 实现简单，无额外线程开销
- **缺点**: 日志写入会阻塞调用线程
- **适用场景**: 日志量较少、对延迟不敏感的应用

```cpp
using MySyncLogger = oneplog::LoggerImpl<oneplog::LoggerConfig<
    oneplog::Mode::Sync,  // 同步模式
    oneplog::Level::Debug
>>;
```

### 2. 异步模式 (Mode::Async)

- **特点**: 日志消息先入队到环形缓冲区，由后台线程异步处理
- **优点**: 不阻塞调用线程，高吞吐量
- **缺点**: 需要额外的后台线程
- **适用场景**: 高性能应用、对延迟敏感的场景

```cpp
using MyAsyncLogger = oneplog::LoggerImpl<oneplog::LoggerConfig<
    oneplog::Mode::Async,  // 异步模式
    oneplog::Level::Debug
>>;
```

### 3. 多进程模式 (Mode::MProc)

- **特点**: 使用共享内存在多个进程间共享日志队列
- **优点**: 支持多进程日志聚合
- **缺点**: 配置较复杂
- **适用场景**: 多进程应用、微服务架构

```cpp
using MyMProcLogger = oneplog::LoggerImpl<oneplog::LoggerConfig<
    oneplog::Mode::MProc,  // 多进程模式
    oneplog::Level::Debug
>>;
```

---

## 编译期配置 (LoggerConfig)

`LoggerConfig` 是核心配置模板，所有配置在编译期确定，实现零运行时开销。

### 完整模板参数

```cpp
template<
    Mode M = Mode::Sync,                              // 运行模式
    Level L = kDefaultLevel,                          // 最小日志级别
    bool EnableWFC = false,                           // 启用 WFC 功能
    bool EnableShadowTail = true,                     // 启用影子尾指针优化
    bool UseFmt = true,                               // 使用 fmt 库格式化
    size_t HeapRingBufferCapacity = 8192,             // 堆环形队列容量
    size_t SharedRingBufferCapacity = 4096,           // 共享环形队列容量
    QueueFullPolicy Policy = QueueFullPolicy::Block,  // 队列满策略
    typename SharedMemoryNameT = DefaultSharedMemoryName,  // 共享内存名称
    int64_t PollTimeoutMs = 10,                       // 轮询超时（毫秒）
    typename SinkBindingsT = SinkBindingList<>        // Sink 绑定列表
>
struct LoggerConfig;
```

### 参数详解

#### 1. Mode M - 运行模式

| 值 | 说明 |
|---|---|
| `Mode::Sync` | 同步模式，直接在调用线程写入 |
| `Mode::Async` | 异步模式，后台线程处理 |
| `Mode::MProc` | 多进程模式，使用共享内存 |

#### 2. Level L - 最小日志级别

编译期过滤，低于此级别的日志调用会被完全优化掉。

| 值 | 说明 |
|---|---|
| `Level::Trace` | 最详细的跟踪信息 |
| `Level::Debug` | 调试信息 |
| `Level::Info` | 一般信息 |
| `Level::Warn` | 警告信息 |
| `Level::Error` | 错误信息 |
| `Level::Critical` | 严重错误 |
| `Level::Off` | 关闭所有日志 |

**默认值**: Debug 模式下为 `Level::Debug`，Release 模式下为 `Level::Info`

```cpp
// 只记录 Warn 及以上级别的日志
using WarnOnlyLogger = oneplog::LoggerImpl<oneplog::LoggerConfig<
    oneplog::Mode::Async,
    oneplog::Level::Warn  // Debug 和 Info 调用会被编译器优化掉
>>;
```

#### 3. EnableWFC - 等待完成功能

启用后可使用 `*WFC` 系列方法，确保日志被处理后才返回。

```cpp
// 启用 WFC
using WFCLogger = oneplog::LoggerImpl<oneplog::LoggerConfig<
    oneplog::Mode::Async,
    oneplog::Level::Debug,
    true  // EnableWFC = true
>>;

WFCLogger logger;
logger.InfoWFC("This message will be processed before returning");
```

**WFC 方法列表**:
- `TraceWFC(fmt, args...)`
- `DebugWFC(fmt, args...)`
- `InfoWFC(fmt, args...)`
- `WarnWFC(fmt, args...)`
- `ErrorWFC(fmt, args...)`
- `CriticalWFC(fmt, args...)`

#### 4. EnableShadowTail - 影子尾指针优化

- **作用**: 减少消费者线程对共享尾指针的原子操作
- **默认值**: `true`
- **建议**: 异步模式下保持启用以获得最佳性能

#### 5. UseFmt - 使用 fmt 库

- **作用**: 控制是否使用 fmt 库进行格式化
- **默认值**: `true`
- **说明**: fmt 库已内置于 oneplog，无需额外配置

#### 6. HeapRingBufferCapacity - 堆环形队列容量

- **作用**: 异步/多进程模式下本地缓冲区的槽位数量
- **默认值**: `8192`
- **说明**: 必须是 2 的幂次方
- **影响**: 
  - 值越大，能缓冲的日志越多，但内存占用越高
  - 值越小，队列满的概率越高

```cpp
// 使用更大的缓冲区
using LargeBufferLogger = oneplog::LoggerImpl<oneplog::LoggerConfig<
    oneplog::Mode::Async,
    oneplog::Level::Debug,
    false,  // EnableWFC
    true,   // EnableShadowTail
    true,   // UseFmt
    16384   // HeapRingBufferCapacity = 16K 槽位
>>;
```

#### 7. SharedRingBufferCapacity - 共享环形队列容量

- **作用**: 多进程模式下共享内存中的队列槽位数量
- **默认值**: `8192`
- **说明**: 仅在 `Mode::MProc` 下使用

#### 8. QueueFullPolicy - 队列满策略

当环形队列满时的处理策略：

| 值 | 说明 |
|---|---|
| `QueueFullPolicy::Block` | 阻塞等待直到有空间（默认） |
| `QueueFullPolicy::DropNewest` | 丢弃新消息 |
| `QueueFullPolicy::DropOldest` | 丢弃最旧的消息 |

```cpp
// 使用阻塞策略（不丢失日志）
using BlockingLogger = oneplog::LoggerImpl<oneplog::LoggerConfig<
    oneplog::Mode::Async,
    oneplog::Level::Debug,
    false,  // EnableWFC
    true,   // EnableShadowTail
    true,   // UseFmt
    8192,   // HeapRingBufferCapacity
    4096,   // SharedRingBufferCapacity
    oneplog::QueueFullPolicy::Block  // 队列满时阻塞
>>;
```

#### 9. SharedMemoryNameT - 共享内存名称

- **作用**: 多进程模式下共享内存的标识名称
- **默认值**: `DefaultSharedMemoryName`（值为 "oneplog_shared"）

```cpp
// 自定义共享内存名称
struct MySharedMemoryName {
    static constexpr const char* value = "my_app_log_shared";
};

using MyMProcLogger = oneplog::LoggerImpl<oneplog::LoggerConfig<
    oneplog::Mode::MProc,
    oneplog::Level::Debug,
    false, true, true, 8192, 4096,
    oneplog::QueueFullPolicy::DropNewest,
    MySharedMemoryName  // 自定义共享内存名称
>>;
```

#### 10. PollTimeoutMs - 轮询超时

- **作用**: 消费者线程等待数据的最大超时时间（毫秒）
- **默认值**: `10`
- **说明**: 超时后会检查是否需要停止，然后继续等待

---

## 运行时配置 (RuntimeConfig)

`RuntimeConfig` 包含可在运行时设置的配置选项。

```cpp
struct RuntimeConfig {
    std::string processName;                    // 进程名称
    std::chrono::microseconds pollInterval{1};  // 轮询间隔
    bool colorEnabled{true};                    // 启用彩色输出
    bool dynamicNameResolution{true};           // 保留字段，暂未使用
};
```

### 使用示例

```cpp
oneplog::RuntimeConfig config;
config.processName = "MyApp";
config.pollInterval = std::chrono::microseconds(10);
config.colorEnabled = true;

oneplog::AsyncLogger logger(config);
```

### 参数说明

| 参数 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `processName` | `std::string` | 空 | 进程名称，用于日志标识 |
| `pollInterval` | `std::chrono::microseconds` | 1μs | 消费者线程在阻塞等待前的自旋等待时间 |
| `colorEnabled` | `bool` | `true` | 是否启用彩色输出 |
| `dynamicNameResolution` | `bool` | `true` | 保留字段，暂未使用 |

---

## Sink 配置

Sink 是日志输出目标。onePlog 提供以下内置 Sink：

### 1. ConsoleSinkType - 控制台输出

```cpp
struct ConsoleSinkType {
    void Write(std::string_view msg) noexcept;  // 写入 stdout
    void Flush() noexcept;                       // 刷新缓冲区
    void Close() noexcept;                       // 关闭（无操作）
};
```

### 2. StderrSinkType - 标准错误输出

```cpp
struct StderrSinkType {
    void Write(std::string_view msg) noexcept;  // 写入 stderr
    void Flush() noexcept;
    void Close() noexcept;
};
```

### 3. NullSinkType - 空输出

丢弃所有日志，用于基准测试或禁用日志。

```cpp
struct NullSinkType {
    void Write(std::string_view msg) noexcept;  // 丢弃消息
    void Flush() noexcept;
    void Close() noexcept;
};
```

### 4. FileSinkType - 文件输出

支持文件轮转的文件 Sink。

```cpp
// 简单文件输出
oneplog::FileSinkType fileSink("app.log");

// 带轮转配置
oneplog::StaticFileSinkConfig config;
config.filename = "app.log";
config.maxSize = 10 * 1024 * 1024;  // 10MB
config.maxFiles = 5;                 // 保留 5 个轮转文件
config.rotateOnOpen = false;         // 打开时不轮转

oneplog::FileSinkType fileSink(config);
```

### FileSinkConfig 参数

| 参数 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `filename` | `std::string` | 空 | 日志文件路径 |
| `maxSize` | `size_t` | 0 | 单个文件最大大小（字节），0 表示无限制 |
| `maxFiles` | `size_t` | 0 | 保留的轮转文件数量，0 表示不轮转 |
| `rotateOnOpen` | `bool` | `false` | 打开文件时是否立即轮转 |

---

## Format 配置

Format 定义日志消息的输出格式。

### 1. MessageOnlyFormat - 仅消息

只输出日志消息，不包含任何元数据。

```
Hello, World!
```

### 2. SimpleFormat - 简单格式（默认）

输出时间和级别。

```
[14:30:45] [INFO] Hello, World!
```

### 3. FullFormat - 完整格式

输出完整时间戳、级别、进程 ID 和线程 ID。

```
[2024-01-15 14:30:45.123] [INFO] [12345:67890] Hello, World!
```

### 格式化需求 (StaticFormatRequirements)

每种 Format 声明其需要的元数据，Logger 只获取必要的元数据以优化性能。

```cpp
template<
    bool NeedsTimestamp = true,      // 是否需要时间戳
    bool NeedsLevel = true,          // 是否需要日志级别
    bool NeedsThreadId = false,      // 是否需要线程 ID
    bool NeedsProcessId = false,     // 是否需要进程 ID
    bool NeedsSourceLocation = false // 是否需要源位置
>
struct StaticFormatRequirements;
```

### 预定义需求

| 别名 | 时间戳 | 级别 | 线程ID | 进程ID | 源位置 |
|---|---|---|---|---|---|
| `NoRequirements` | ✗ | ✗ | ✗ | ✗ | ✗ |
| `TimestampOnlyRequirements` | ✓ | ✗ | ✗ | ✗ | ✗ |
| `BasicRequirements` | ✓ | ✓ | ✗ | ✗ | ✗ |
| `ThreadRequirements` | ✓ | ✓ | ✓ | ✗ | ✗ |
| `FullRequirements` | ✓ | ✓ | ✓ | ✓ | ✗ |
| `DebugRequirements` | ✓ | ✓ | ✓ | ✓ | ✓ |

---

## SinkBinding - Sink 与 Format 绑定

`SinkBinding` 将 Sink 和 Format 配对：

```cpp
template<typename SinkT, typename FormatT>
struct SinkBinding {
    using Sink = SinkT;
    using Format = FormatT;
    using Requirements = typename FormatT::Requirements;
    
    Sink sink;
};
```

### 预定义绑定

```cpp
// 控制台 + SimpleFormat
using DefaultConsoleSinkBinding = SinkBinding<ConsoleSinkType, SimpleFormat>;

// 控制台 + MessageOnlyFormat
using ConsoleMessageOnlyBinding = SinkBinding<ConsoleSinkType, MessageOnlyFormat>;

// 控制台 + FullFormat
using ConsoleFullFormatBinding = SinkBinding<ConsoleSinkType, FullFormat>;

// NullSink + MessageOnlyFormat（用于基准测试）
using NullSinkBinding = SinkBinding<NullSinkType, MessageOnlyFormat>;
```

---

## SinkBindingList - 多 Sink 支持

`SinkBindingList` 管理多个 SinkBinding，支持同时输出到多个目标：

```cpp
// 同时输出到控制台和文件
using ConsoleSinkBinding = oneplog::SinkBinding<oneplog::ConsoleSinkType, oneplog::SimpleFormat>;
using FileSinkBinding = oneplog::SinkBinding<oneplog::FileSinkType, oneplog::FullFormat>;

using MultiSinkBindings = oneplog::SinkBindingList<ConsoleSinkBinding, FileSinkBinding>;

using MultiSinkLogger = oneplog::LoggerImpl<oneplog::LoggerConfig<
    oneplog::Mode::Async,
    oneplog::Level::Debug,
    false, true, true, 8192, 4096,
    oneplog::QueueFullPolicy::DropNewest,
    oneplog::DefaultSharedMemoryName,
    10,
    MultiSinkBindings
>>;

// 创建带预配置 Sink 的日志器
oneplog::FileSinkType fileSink("app.log");
MultiSinkBindings bindings(
    ConsoleSinkBinding{},
    FileSinkBinding{std::move(fileSink)}
);
MultiSinkLogger logger(std::move(bindings));
```

---

## 预设配置

onePlog 提供了几种常用的预设配置：

### 1. DefaultSyncConfig

同步模式 + 控制台输出 + SimpleFormat

```cpp
oneplog::SyncLogger logger;  // 使用 DefaultSyncConfig
```

### 2. DefaultAsyncConfig

异步模式 + 控制台输出 + SimpleFormat

```cpp
oneplog::AsyncLogger logger;  // 使用 DefaultAsyncConfig
```

### 3. DefaultMProcConfig

多进程模式 + 控制台输出 + SimpleFormat

```cpp
oneplog::MProcLogger logger;  // 使用 DefaultMProcConfig
```

### 4. HighPerformanceConfig

异步模式 + NullSink + MessageOnlyFormat（用于基准测试）

```cpp
using BenchmarkLogger = oneplog::LoggerImpl<oneplog::HighPerformanceConfig>;
```

---

## 配置辅助模板

onePlog 提供了简化配置的辅助模板：

### LoggerConfigWithSinks

```cpp
template<typename SinkBindingsT,
         Mode M = Mode::Sync,
         Level L = kDefaultLevel,
         bool EnableWFC = false,
         bool EnableShadowTail = true,
         bool UseFmt = true>
using LoggerConfigWithSinks = LoggerConfig<...>;
```

### AsyncLoggerConfig

```cpp
template<typename SinkBindingsT,
         Level L = kDefaultLevel,
         bool EnableWFC = false,
         bool EnableShadowTail = true,
         size_t BufferCapacity = 8192>
using AsyncLoggerConfig = LoggerConfig<Mode::Async, ...>;
```

### SyncLoggerConfig

```cpp
template<typename SinkBindingsT, Level L = kDefaultLevel>
using SyncLoggerConfig = LoggerConfig<Mode::Sync, ...>;
```

### MProcLoggerConfig

```cpp
template<typename SinkBindingsT,
         typename SharedMemoryNameT,
         Level L = kDefaultLevel,
         bool EnableWFC = false,
         bool EnableShadowTail = true>
using MProcLoggerConfig = LoggerConfig<Mode::MProc, ...>;
```

---

## 高级用法

### 自定义 Format

创建自定义格式化器：

```cpp
struct MyCustomFormat {
    // 声明需要的元数据
    using Requirements = oneplog::StaticFormatRequirements<true, true, true, false, false>;

    // fmt 库格式化（同步模式）
    template<typename... Args>
    static void FormatTo(fmt::memory_buffer& buffer, oneplog::Level level, 
                         uint64_t timestamp, uint32_t threadId, uint32_t,
                         const char* fmt, Args&&... args) {
        // 自定义格式化逻辑
        fmt::format_to(std::back_inserter(buffer), 
            "[T{}] {} - ", threadId, oneplog::LevelToString(level));
        fmt::format_to(std::back_inserter(buffer), fmt::runtime(fmt), 
                       std::forward<Args>(args)...);
    }
    
    // fmt 库格式化（异步模式）
    static void FormatEntryTo(fmt::memory_buffer& buffer, const oneplog::LogEntry& entry) {
        fmt::format_to(std::back_inserter(buffer), 
            "[T{}] {} - ", entry.threadId, oneplog::LevelToString(entry.level));
        buffer.append(std::string_view(entry.snapshot.FormatAll()));
    }

    // 非 fmt 回退实现
    static std::string FormatEntry(const oneplog::LogEntry& entry) {
        return "[T" + std::to_string(entry.threadId) + "] " + 
               std::string(oneplog::LevelToString(entry.level)) + " - " + 
               entry.snapshot.FormatAll();
    }
};
```

### 自定义 Sink

创建自定义输出目标：

```cpp
class NetworkSink {
public:
    NetworkSink(const std::string& host, int port) 
        : m_host(host), m_port(port) {
        // 初始化网络连接
    }
    
    void Write(std::string_view msg) noexcept {
        // 发送到网络
    }
    
    void Flush() noexcept {
        // 刷新网络缓冲区
    }
    
    void Close() noexcept {
        // 关闭连接
    }
    
private:
    std::string m_host;
    int m_port;
};

// 使用自定义 Sink
using NetworkSinkBinding = oneplog::SinkBinding<NetworkSink, oneplog::SimpleFormat>;
using NetworkSinkBindings = oneplog::SinkBindingList<NetworkSinkBinding>;

using NetworkLogger = oneplog::LoggerImpl<oneplog::LoggerConfig<
    oneplog::Mode::Async,
    oneplog::Level::Debug,
    false, true, true, 8192, 4096,
    oneplog::QueueFullPolicy::DropNewest,
    oneplog::DefaultSharedMemoryName,
    10,
    NetworkSinkBindings
>>;
```

### 多进程日志

```cpp
// 生产者进程
struct MySharedMem {
    static constexpr const char* value = "my_app_logs";
};

using ProducerConfig = oneplog::LoggerConfig<
    oneplog::Mode::MProc,
    oneplog::Level::Debug,
    false, true, true, 8192, 4096,
    oneplog::QueueFullPolicy::DropNewest,
    MySharedMem
>;

oneplog::LoggerImpl<ProducerConfig> producer;
producer.Info("Message from producer");

// 消费者进程（使用相同配置）
oneplog::LoggerImpl<ProducerConfig> consumer;
// 消费者会自动从共享内存读取日志
```

### 文件轮转日志

```cpp
// 配置文件轮转
oneplog::StaticFileSinkConfig fileConfig;
fileConfig.filename = "/var/log/myapp/app.log";
fileConfig.maxSize = 100 * 1024 * 1024;  // 100MB
fileConfig.maxFiles = 10;                 // 保留 10 个文件
fileConfig.rotateOnOpen = false;

oneplog::FileSinkType fileSink(fileConfig);

using FileSinkBinding = oneplog::SinkBinding<oneplog::FileSinkType, oneplog::FullFormat>;
using FileSinkBindings = oneplog::SinkBindingList<FileSinkBinding>;

using FileLogger = oneplog::LoggerImpl<oneplog::LoggerConfig<
    oneplog::Mode::Async,
    oneplog::Level::Debug,
    false, true, true, 8192, 4096,
    oneplog::QueueFullPolicy::DropNewest,
    oneplog::DefaultSharedMemoryName,
    10,
    FileSinkBindings
>>;

FileLogger logger(FileSinkBindings{FileSinkBinding{std::move(fileSink)}});
```

---

## 编译选项

### CMake 选项

| 选项 | 默认值 | 说明 |
|---|---|---|
| `ONEPLOG_BUILD_TESTS` | `OFF` | 构建测试 |
| `ONEPLOG_BUILD_EXAMPLES` | `OFF` | 构建示例 |

### 编译宏

| 宏 | 说明 |
|---|---|
| `FMT_HEADER_ONLY` | 使用 fmt 头文件模式 |
| `NDEBUG` | Release 模式（默认日志级别为 Info） |

### 编译命令示例

```bash
# 使用 fmt 库（内置）
clang++ -std=c++17 -O3 -I include \
    -o myapp main.cpp -lpthread

# CMake 构建
cmake -B build
cmake --build build
```

---

## API 参考

### LoggerImpl 方法

#### 日志方法

```cpp
template<typename... Args> void Trace(const char* fmt, Args&&... args);
template<typename... Args> void Debug(const char* fmt, Args&&... args);
template<typename... Args> void Info(const char* fmt, Args&&... args);
template<typename... Args> void Warn(const char* fmt, Args&&... args);
template<typename... Args> void Error(const char* fmt, Args&&... args);
template<typename... Args> void Critical(const char* fmt, Args&&... args);
```

#### WFC 日志方法（需要 EnableWFC=true）

```cpp
template<typename... Args> void TraceWFC(const char* fmt, Args&&... args);
template<typename... Args> void DebugWFC(const char* fmt, Args&&... args);
template<typename... Args> void InfoWFC(const char* fmt, Args&&... args);
template<typename... Args> void WarnWFC(const char* fmt, Args&&... args);
template<typename... Args> void ErrorWFC(const char* fmt, Args&&... args);
template<typename... Args> void CriticalWFC(const char* fmt, Args&&... args);
```

#### 控制方法

```cpp
void Flush();     // 刷新所有待处理的日志
void Shutdown();  // 关闭日志器
```

#### 访问器

```cpp
SinkBindings& GetSinkBindings();
const SinkBindings& GetSinkBindings() const;
RuntimeConfig& GetRuntimeConfig();
const RuntimeConfig& GetRuntimeConfig() const;
bool IsRunning() const;
```

#### 多进程模式方法

```cpp
bool IsMProcOwner() const;                           // 是否是生产者
SharedLoggerConfig* GetSharedConfig();               // 获取共享配置
ProcessThreadNameTable* GetNameTable();              // 获取名称表
uint32_t RegisterProcess(const std::string& name);   // 注册进程名
uint32_t RegisterModule(const std::string& name);    // 注册模块名
const char* GetProcessName(uint32_t id) const;       // 获取进程名
const char* GetModuleName(uint32_t id) const;        // 获取模块名

// 已弃用（向后兼容）
[[deprecated]] uint32_t RegisterThread(const std::string& name);  // 使用 RegisterModule
[[deprecated]] const char* GetThreadName(uint32_t id) const;      // 使用 GetModuleName
```

---

## 类型别名速查

| 别名 | 说明 |
|---|---|
| `SyncLogger` | 同步日志器（DefaultSyncConfig） |
| `AsyncLogger` | 异步日志器（DefaultAsyncConfig） |
| `MProcLogger` | 多进程日志器（DefaultMProcConfig） |
| `SyncLoggerV2` | 同步日志器（带 V2 后缀） |
| `AsyncLoggerV2` | 异步日志器（带 V2 后缀） |
| `MProcLoggerV2` | 多进程日志器（带 V2 后缀） |
| `Logger<M, L, WFC, ST>` | 通用日志器模板 |

---

## 最佳实践

1. **选择合适的模式**
   - 低日志量：使用 `Mode::Sync`
   - 高性能要求：使用 `Mode::Async`
   - 多进程应用：使用 `Mode::MProc`

2. **设置合适的日志级别**
   - 开发环境：`Level::Debug` 或 `Level::Trace`
   - 生产环境：`Level::Info` 或 `Level::Warn`

3. **队列满策略选择**
   - 不能丢失日志：`QueueFullPolicy::Block`（默认）
   - 优先新日志：`QueueFullPolicy::DropOldest`
   - 优先旧日志：`QueueFullPolicy::DropNewest`

4. **使用 fmt 库**
   - fmt 库已内置于 oneplog，无需额外配置

5. **文件日志轮转**
   - 设置合理的 `maxSize` 和 `maxFiles` 防止磁盘占满

6. **程序退出前**
   - 调用 `Flush()` 确保所有日志被写入
   - 或让日志器正常析构（会自动 Flush）
