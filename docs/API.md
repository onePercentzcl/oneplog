# onePlog API 文档

## 目录

- [快速开始](#快速开始)
- [FastLogger（推荐）](#fastlogger推荐)
- [简化接口](#简化接口)
- [Logger 类](#logger-类)
- [Sink 类](#sink-类)
- [Format 类](#format-类)
- [日志级别](#日志级别)
- [宏定义](#宏定义)
- [全局函数](#全局函数)
- [配置结构](#配置结构)

---

## 快速开始

```cpp
#include <oneplog/oneplog.hpp>

int main() {
    // 一行初始化（异步模式 + 控制台输出）
    oneplog::Init();
    
    // 使用 log:: 静态类记录日志
    log::Info("Hello, {}!", "onePlog");
    log::Error("Error code: {}", 42);
    
    // 或使用全局函数
    oneplog::Info("Global function style");
    
    // 关闭
    oneplog::Shutdown();
    return 0;
}
```

---

## FastLogger（推荐）

FastLogger 是 oneplog 的高性能模板化日志器，通过编译期确定所有类型和配置来实现零虚函数调用开销。

### 基本用法

```cpp
#include <oneplog/fast_logger.hpp>

int main() {
    // 使用默认配置（同步模式 + 控制台输出）
    oneplog::FastLogger<> logger;
    
    // 记录日志
    logger.Info("Hello, {}!", "FastLogger");
    logger.Debug("Debug value: {}", 42);
    logger.Error("Error occurred: {}", "something went wrong");
    
    // 刷新并关闭
    logger.Flush();
    logger.Shutdown();
    
    return 0;
}
```

### 预设配置

```cpp
// 同步日志器（直接输出，无后台线程）
oneplog::SyncLogger syncLogger;

// 异步日志器（后台线程处理，高性能）
oneplog::AsyncLogger asyncLogger;

// 高性能日志器（用于基准测试）
oneplog::HighPerformanceLogger benchLogger;

// 带完整格式的日志器
oneplog::SyncFullLogger fullLogger;
oneplog::AsyncFullLogger asyncFullLogger;
```

### 自定义配置

```cpp
// 自定义模式和级别
using MyLogger = oneplog::FastLogger<
    oneplog::Mode::Async,           // 异步模式
    oneplog::SimpleFormat,          // 简单格式
    oneplog::ConsoleSinkType,       // 控制台输出
    oneplog::Level::Debug,          // 最小级别
    false,                          // 禁用 WFC
    true                            // 启用 ShadowTail 优化
>;

MyLogger logger;
logger.Info("Custom logger message");
```

### 文件输出

```cpp
// 使用文件 Sink
oneplog::FastLogger<
    oneplog::Mode::Sync,
    oneplog::SimpleFormat,
    oneplog::FileSinkType,
    oneplog::Level::Info
> fileLogger(oneplog::FileSinkType("/var/log/app.log"));

fileLogger.Info("Writing to file");
fileLogger.Flush();
```

### 多 Sink 输出

```cpp
// 使用 MultiSink 同时输出到多个目标
auto multiSink = oneplog::MakeSinks(
    oneplog::ConsoleSinkType{},
    oneplog::FileSinkType("/var/log/app.log")
);

oneplog::FastLogger<
    oneplog::Mode::Sync,
    oneplog::SimpleFormat,
    decltype(multiSink),
    oneplog::Level::Info
> multiLogger(std::move(multiSink));

multiLogger.Info("Output to both console and file");
```

### 日志级别

```cpp
// 编译期级别过滤 - 低于最小级别的日志调用会被完全优化掉
oneplog::FastLogger<
    oneplog::Mode::Sync,
    oneplog::SimpleFormat,
    oneplog::ConsoleSinkType,
    oneplog::Level::Warn  // 只记录 Warn 及以上级别
> warnLogger;

warnLogger.Debug("This won't be compiled");  // 编译期优化掉
warnLogger.Warn("This will be logged");      // 正常记录
warnLogger.Error("This will be logged");     // 正常记录
```

### 格式化器类型

| 格式化器 | 输出格式 | 需要的元数据 |
|---------|---------|-------------|
| `MessageOnlyFormat` | `message` | 无 |
| `SimpleFormat` | `[HH:MM:SS] [LEVEL] message` | 时间戳、级别 |
| `FullFormat` | `[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [PID:TID] message` | 全部 |

### Sink 类型

| Sink 类型 | 说明 |
|----------|------|
| `ConsoleSinkType` | 输出到 stdout |
| `StderrSinkType` | 输出到 stderr |
| `FileSinkType` | 输出到文件 |
| `NullSinkType` | 丢弃所有输出（用于基准测试） |

### 全局 API

```cpp
#include <oneplog/fast_logger.hpp>

int main() {
    // 初始化全局 FastLogger
    oneplog::fast::Init();
    
    // 使用全局函数记录日志
    oneplog::fast::Info("Global FastLogger message");
    oneplog::fast::Debug("Debug: {}", 42);
    oneplog::fast::Error("Error: {}", "something wrong");
    
    // 刷新并关闭
    oneplog::fast::Flush();
    oneplog::fast::Shutdown();
    
    return 0;
}
```

### 向后兼容

FastLogger 提供了与旧版 Logger 兼容的类型别名：

```cpp
// 使用 Logger 别名（向后兼容）
oneplog::Logger<oneplog::Mode::Async, oneplog::Level::Debug> logger;
logger.Info("Compatible with old API");
```

---

## 简化接口

### 一键初始化

```cpp
// 默认初始化：异步模式 + ConsoleSink + ConsoleFormat
oneplog::Init();

// 自定义配置初始化
oneplog::LoggerConfig config;
config.mode = oneplog::Mode::Sync;  // 同步模式
config.level = oneplog::Level::Debug;
oneplog::Init(config);
```

### log:: 静态类

提供类似 `log::Info()` 的简洁调用方式：

```cpp
// 基本日志
log::Trace("Trace message");
log::Debug("Debug message");
log::Info("Info message");
log::Warn("Warning message");
log::Error("Error message");
log::Critical("Critical message");

// WFC（等待完成）日志
log::InfoWFC("Guaranteed to be written");
log::CriticalWFC("Critical error: {}", code);

// 工具方法
log::SetLevel(oneplog::Level::Warn);
log::Flush();
log::Shutdown();
```

### 动态修改 Sink 和 Format

```cpp
oneplog::Init();

// 修改输出目标
oneplog::SetSink(std::make_shared<oneplog::FileSink>("app.log"));

// 修改格式化器
oneplog::SetFormat(std::make_shared<oneplog::FileFormat>());
```

---

## Logger 类

### 构造函数

```cpp
Logger(const std::string& name, Mode mode = Mode::Sync);
```

| 参数 | 类型 | 说明 |
|------|------|------|
| `name` | `std::string` | 日志器名称 |
| `mode` | `Mode` | 运行模式：`Sync`、`Async`、`MProc` |

### 初始化

```cpp
void Init();
void Init(const LoggerConfig& config);
```

### 日志方法

```cpp
template<typename... Args>
void Trace(const char* fmt, Args&&... args);

template<typename... Args>
void Debug(const char* fmt, Args&&... args);

template<typename... Args>
void Info(const char* fmt, Args&&... args);

template<typename... Args>
void Warn(const char* fmt, Args&&... args);

template<typename... Args>
void Error(const char* fmt, Args&&... args);

template<typename... Args>
void Critical(const char* fmt, Args&&... args);
```

### WFC（等待完成）日志方法

确保日志写入完成后才返回：

```cpp
template<typename... Args>
void TraceWFC(const char* fmt, Args&&... args);

template<typename... Args>
void DebugWFC(const char* fmt, Args&&... args);

template<typename... Args>
void InfoWFC(const char* fmt, Args&&... args);

template<typename... Args>
void WarnWFC(const char* fmt, Args&&... args);

template<typename... Args>
void ErrorWFC(const char* fmt, Args&&... args);

template<typename... Args>
void CriticalWFC(const char* fmt, Args&&... args);
```

### 配置方法

```cpp
void SetSink(std::shared_ptr<Sink> sink);
void SetFormat(std::shared_ptr<Format> format);
void SetLevel(Level level);
Level GetLevel() const;
```

### 控制方法

```cpp
void Flush();      // 刷新缓冲区
void Shutdown();   // 关闭日志器
```

---

## Sink 类

### ConsoleSink

输出到控制台（stdout）。

```cpp
auto sink = std::make_shared<oneplog::ConsoleSink>();
```

### FileSink

输出到文件，支持文件轮转。

```cpp
// 基本用法
auto sink = std::make_shared<oneplog::FileSink>("app.log");

// 带轮转配置
auto sink = std::make_shared<oneplog::FileSink>(
    "app.log",           // 文件路径
    10 * 1024 * 1024,    // 最大文件大小 (10MB)
    5                    // 保留文件数量
);
```

### NetworkSink

输出到网络（TCP/UDP）。

```cpp
auto sink = std::make_shared<oneplog::NetworkSink>("127.0.0.1", 9000);
```

### Sink 接口

自定义 Sink 需要实现以下接口：

```cpp
class Sink {
public:
    virtual void Write(const std::string& message) = 0;
    virtual void Write(std::string_view message) = 0;
    virtual void WriteBatch(const std::vector<std::string>& messages) = 0;
    virtual void Flush() = 0;
    virtual void Close() = 0;
    virtual bool HasError() const = 0;
    virtual std::string GetLastError() const = 0;
};
```

---

## Format 类

### ConsoleFormat

控制台格式化器，支持彩色输出。

```cpp
auto format = std::make_shared<oneplog::ConsoleFormat>();
format->SetProcessName("myapp");
format->SetModuleName("main");
logger->SetFormat(format);
```

输出格式：
```
[15:20:23] [INFO] [myapp] [main] 消息内容
```

### FileFormat

文件格式化器，包含完整信息。

```cpp
auto format = std::make_shared<oneplog::FileFormat>();
logger->SetFormat(format);
```

输出格式：
```
[2024-01-15 15:20:23.123] [INFO] [PID:1234] [TID:5678] [file.cpp:42:func] 消息内容
```

### JsonFormat

JSON 格式化器。

```cpp
auto format = std::make_shared<oneplog::JsonFormat>();
logger->SetFormat(format);
```

输出格式：
```json
{"timestamp":"2024-01-15T15:20:23.123","level":"INFO","message":"消息内容"}
```

### PatternFormat

自定义模式格式化器。

```cpp
auto format = std::make_shared<oneplog::PatternFormat>("[%T] [%L] %M");
logger->SetFormat(format);
```

模式说明：
| 模式 | 说明 |
|------|------|
| `%T` | 时间戳 |
| `%L` | 日志级别 |
| `%M` | 消息内容 |
| `%P` | 进程ID |
| `%t` | 线程ID |
| `%n` | 进程名 |
| `%m` | 模块名 |

---

## 日志级别

```cpp
namespace oneplog {
    enum class Level {
        Trace = 0,    // 最详细
        Debug = 1,
        Info = 2,
        Warn = 3,
        Error = 4,
        Critical = 5  // 最严重
    };
}
```

### 级别过滤

```cpp
log::SetLevel(oneplog::Level::Warn);  // 只记录 Warn 及以上级别
```

### 级别转换

```cpp
// 级别转字符串
std::string_view str = oneplog::LevelToString(oneplog::Level::Info);  // "info"
std::string_view str = oneplog::LevelToString(oneplog::Level::Info, 
                                               oneplog::LevelStringFormat::Short4);  // "INFO"

// 字符串转级别
oneplog::Level level = oneplog::StringToLevel("info");
```

---

## 宏定义

### 基本日志宏

```cpp
ONEPLOG_TRACE(fmt, ...)
ONEPLOG_DEBUG(fmt, ...)
ONEPLOG_INFO(fmt, ...)
ONEPLOG_WARN(fmt, ...)
ONEPLOG_ERROR(fmt, ...)
ONEPLOG_CRITICAL(fmt, ...)
```

### WFC 日志宏

```cpp
ONEPLOG_TRACE_WFC(fmt, ...)
ONEPLOG_DEBUG_WFC(fmt, ...)
ONEPLOG_INFO_WFC(fmt, ...)
ONEPLOG_WARN_WFC(fmt, ...)
ONEPLOG_ERROR_WFC(fmt, ...)
ONEPLOG_CRITICAL_WFC(fmt, ...)
```

### 条件日志宏

```cpp
ONEPLOG_INFO_IF(condition, fmt, ...)
ONEPLOG_ERROR_IF(condition, fmt, ...)
// ... 其他级别类似
```

### 使用示例

```cpp
int value = 42;
ONEPLOG_INFO("Value is {}", value);
ONEPLOG_ERROR_IF(value < 0, "Invalid value: {}", value);
ONEPLOG_CRITICAL_WFC("Fatal error, code: {}", 0xDEAD);
```

---

## 全局函数

### 初始化和配置

```cpp
void Init();                                    // 默认初始化
void Init(const LoggerConfig& config);          // 自定义配置初始化
void SetSink(std::shared_ptr<Sink> sink);       // 设置 Sink
void SetFormat(std::shared_ptr<Format> format); // 设置格式化器
void SetLevel(Level level);                     // 设置日志级别
void Flush();                                   // 刷新缓冲区
void Shutdown();                                // 关闭日志器
```

### 便捷日志函数

```cpp
namespace oneplog {
    template<typename... Args>
    void Trace(const char* fmt, Args&&... args);
    
    template<typename... Args>
    void Debug(const char* fmt, Args&&... args);
    
    template<typename... Args>
    void Info(const char* fmt, Args&&... args);
    
    template<typename... Args>
    void Warn(const char* fmt, Args&&... args);
    
    template<typename... Args>
    void Error(const char* fmt, Args&&... args);
    
    template<typename... Args>
    void Critical(const char* fmt, Args&&... args);
}
```

### WFC 日志函数

```cpp
namespace oneplog {
    template<typename... Args>
    void TraceWFC(const char* fmt, Args&&... args);
    
    template<typename... Args>
    void DebugWFC(const char* fmt, Args&&... args);
    
    template<typename... Args>
    void InfoWFC(const char* fmt, Args&&... args);
    
    template<typename... Args>
    void WarnWFC(const char* fmt, Args&&... args);
    
    template<typename... Args>
    void ErrorWFC(const char* fmt, Args&&... args);
    
    template<typename... Args>
    void CriticalWFC(const char* fmt, Args&&... args);
}
```

---

## 配置结构

### LoggerConfig

```cpp
struct LoggerConfig {
    Mode mode = Mode::Async;             // 运行模式
    Level level = Level::Info;           // 日志级别
    size_t heapRingBufferSize = 8192;    // 异步模式缓冲区大小
    QueueFullPolicy queueFullPolicy = QueueFullPolicy::DropNewest;  // 队列满策略
    std::string processName;             // 进程名（可选）
};
```

### QueueFullPolicy

```cpp
enum class QueueFullPolicy {
    DropNewest,  // 丢弃最新消息（默认）
    DropOldest,  // 丢弃最旧消息
    Block        // 阻塞等待
};
```

---

## NameManager 类

NameManager 提供进程名和模块名的管理功能，支持在三种运行模式下使用不同的存储策略。

### 初始化

```cpp
// 通常由 oneplog::Init() 自动调用
static void Initialize(Mode mode, SharedMemory* sharedMemory = nullptr);

// 关闭（通常由 oneplog::Shutdown() 自动调用）
static void Shutdown();

// 检查是否已初始化
static bool IsInitialized();
```

### 进程名管理

```cpp
// 设置进程名（最多 31 字符，超长会被截断）
static void SetProcessName(const std::string& name);

// 获取进程名
// processId: 进程 ID（MProc 模式使用，0 表示当前进程）
static std::string GetProcessName(uint32_t processId = 0);
```

### 模块名管理

```cpp
// 设置当前线程的模块名（最多 31 字符，超长会被截断）
static void SetModuleName(const std::string& name);

// 获取模块名
// threadId: 线程 ID（0 表示当前线程）
static std::string GetModuleName(uint32_t threadId = 0);
```

### 模式和 ID 查询

```cpp
// 获取当前运行模式
static Mode GetMode();

// 获取注册的进程 ID（MProc 模式）
static uint32_t GetRegisteredProcessId();

// 获取当前线程注册的线程 ID（MProc 模式）
static uint32_t GetRegisteredThreadId();
```

### 使用示例

```cpp
#include <oneplog/oneplog.hpp>

int main() {
    // 方式 1：通过配置设置进程名
    oneplog::LoggerConfig config;
    config.processName = "my_app";
    oneplog::Init(config);
    
    // 方式 2：初始化后设置
    oneplog::NameManager::SetProcessName("my_app");
    
    // 设置模块名
    oneplog::NameManager::SetModuleName("main");
    
    // 获取名称
    std::string procName = oneplog::NameManager::GetProcessName();
    std::string modName = oneplog::NameManager::GetModuleName();
    
    log::Info("Process: {}, Module: {}", procName, modName);
    
    oneplog::Shutdown();
    return 0;
}
```

---

## ThreadWithModuleName 类

ThreadWithModuleName 提供带模块名继承的线程创建功能。

### 创建继承模块名的线程

```cpp
// 创建线程，自动继承父线程的模块名
template<typename Func, typename... Args>
static std::thread Create(Func&& func, Args&&... args);
```

### 创建指定模块名的线程

```cpp
// 创建线程，使用指定的模块名
template<typename Func, typename... Args>
static std::thread CreateWithName(const std::string& moduleName, Func&& func, Args&&... args);
```

### 使用示例

```cpp
#include <oneplog/oneplog.hpp>

int main() {
    oneplog::Init();
    
    // 设置父线程模块名
    oneplog::NameManager::SetModuleName("supervisor");
    
    // 子线程自动继承 "supervisor" 模块名
    auto childThread = oneplog::ThreadWithModuleName::Create([]() {
        log::Info("Child module: {}", oneplog::NameManager::GetModuleName());
        // 输出: Child module: supervisor
    });
    childThread.join();
    
    // 创建具有特定模块名的线程
    auto workerThread = oneplog::ThreadWithModuleName::CreateWithName("worker", []() {
        log::Info("Worker module: {}", oneplog::NameManager::GetModuleName());
        // 输出: Worker module: worker
    });
    workerThread.join();
    
    // 支持带参数的函数
    auto paramThread = oneplog::ThreadWithModuleName::Create([](int a, int b) {
        log::Info("Sum: {}", a + b);
    }, 10, 20);
    paramThread.join();
    
    oneplog::Shutdown();
    return 0;
}
```

### 嵌套继承

```cpp
oneplog::NameManager::SetModuleName("grandparent");

auto parentThread = oneplog::ThreadWithModuleName::Create([]() {
    // 继承 "grandparent"
    log::Info("Parent: {}", oneplog::NameManager::GetModuleName());
    
    auto childThread = oneplog::ThreadWithModuleName::Create([]() {
        // 也继承 "grandparent"
        log::Info("Child: {}", oneplog::NameManager::GetModuleName());
    });
    childThread.join();
});
parentThread.join();
```

---

## 三种模式下的名称存储

| 模式 | 进程名存储 | 模块名存储 | 说明 |
|------|-----------|-----------|------|
| Sync | thread_local | thread_local | 每个线程独立存储 |
| Async | 全局变量 | 堆上的 TID-模块名映射表 | 后台线程可通过 TID 查找 |
| MProc | 共享内存 | 共享内存 | 多进程共享 |

---

## 运行模式

| 模式 | 说明 | 适用场景 |
|------|------|----------|
| `Sync` | 同步模式，日志直接写入 | 调试、低吞吐量场景 |
| `Async` | 异步模式，后台线程写入 | 高性能、生产环境 |
| `MProc` | 多进程模式，共享内存通信 | 多进程应用 |

---

## 完整示例

### 简单用法

```cpp
#include <oneplog/oneplog.hpp>

int main() {
    oneplog::Init();
    
    log::Info("Application started");
    log::Debug("Debug info: {}", 123);
    log::Warn("Warning message");
    log::CriticalWFC("Critical event");
    
    oneplog::Shutdown();
    return 0;
}
```

### 高级用法

```cpp
#include <oneplog/oneplog.hpp>

int main() {
    // 自定义配置
    oneplog::LoggerConfig config;
    config.mode = oneplog::Mode::Async;
    config.level = oneplog::Level::Debug;
    config.heapRingBufferSize = 1024 * 1024;  // 1MB
    oneplog::Init(config);
    
    // 修改输出到文件
    oneplog::SetSink(std::make_shared<oneplog::FileSink>("app.log", 10*1024*1024, 5));
    
    // 修改格式
    auto format = std::make_shared<oneplog::ConsoleFormat>();
    format->SetProcessName("myapp");
    oneplog::SetFormat(format);
    
    // 记录日志
    log::Info("Application started");
    log::Debug("Debug info: {}", 123);
    
    // 使用宏
    ONEPLOG_WARN("Warning message");
    ONEPLOG_ERROR_IF(false, "This won't be logged");
    
    // 关键日志确保写入
    log::CriticalWFC("Critical event occurred");
    
    // 关闭
    log::Flush();
    oneplog::Shutdown();
    
    return 0;
}
```
