# onePlog API 文档

## 目录

- [快速开始](#快速开始)
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
    // 创建同步日志器
    auto logger = std::make_shared<oneplog::Logger>("app", oneplog::Mode::Sync);
    logger->SetSink(std::make_shared<oneplog::ConsoleSink>());
    logger->Init();
    
    // 设置为默认日志器
    oneplog::SetDefaultLogger(logger);
    
    // 记录日志
    logger->Info("Hello, {}!", "onePlog");
    
    // 使用宏
    ONEPLOG_INFO("Value: {}", 42);
    
    return 0;
}
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

### 使用示例

```cpp
// 同步模式
auto syncLogger = std::make_shared<oneplog::Logger>("sync", oneplog::Mode::Sync);
syncLogger->SetSink(std::make_shared<oneplog::ConsoleSink>());
syncLogger->Init();
syncLogger->Info("Sync message");

// 异步模式
auto asyncLogger = std::make_shared<oneplog::Logger>("async", oneplog::Mode::Async);
asyncLogger->SetSink(std::make_shared<oneplog::FileSink>("app.log"));
oneplog::LoggerConfig cfg;
cfg.heapRingBufferSize = 1024 * 1024;  // 1MB 缓冲区
asyncLogger->Init(cfg);
asyncLogger->Info("Async message");
asyncLogger->Flush();
asyncLogger->Shutdown();
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
logger->SetLevel(oneplog::Level::Warn);  // 只记录 Warn 及以上级别
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

### 默认日志器

```cpp
void SetDefaultLogger(std::shared_ptr<Logger> logger);
std::shared_ptr<Logger> GetDefaultLogger();
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

使用示例：

```cpp
oneplog::SetDefaultLogger(logger);
oneplog::Info("Using global function");
```

---

## 配置结构

### LoggerConfig

```cpp
struct LoggerConfig {
    Mode mode = Mode::Sync;              // 运行模式
    size_t heapRingBufferSize = 65536;   // 异步模式缓冲区大小
    QueueFullPolicy queueFullPolicy = QueueFullPolicy::DropNewest;  // 队列满策略
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

### 配置示例

```cpp
oneplog::LoggerConfig config;
config.mode = oneplog::Mode::Async;
config.heapRingBufferSize = 1024 * 1024;  // 1MB
config.queueFullPolicy = oneplog::QueueFullPolicy::Block;

auto logger = std::make_shared<oneplog::Logger>("app");
logger->SetSink(std::make_shared<oneplog::FileSink>("app.log"));
logger->Init(config);
```

---

## 运行模式

| 模式 | 说明 | 适用场景 |
|------|------|----------|
| `Sync` | 同步模式，日志直接写入 | 调试、低吞吐量场景 |
| `Async` | 异步模式，后台线程写入 | 高性能、生产环境 |
| `MProc` | 多进程模式，共享内存通信 | 多进程应用 |

---

## 完整示例

```cpp
#include <oneplog/oneplog.hpp>

int main() {
    // 创建异步日志器
    auto logger = std::make_shared<oneplog::Logger>("myapp", oneplog::Mode::Async);
    
    // 配置文件输出
    auto sink = std::make_shared<oneplog::FileSink>("app.log", 10*1024*1024, 5);
    logger->SetSink(sink);
    
    // 配置格式
    auto format = std::make_shared<oneplog::ConsoleFormat>();
    format->SetProcessName("myapp");
    logger->SetFormat(format);
    
    // 设置日志级别
    logger->SetLevel(oneplog::Level::Debug);
    
    // 初始化
    oneplog::LoggerConfig config;
    config.heapRingBufferSize = 1024 * 1024;
    logger->Init(config);
    
    // 设置为默认日志器
    oneplog::SetDefaultLogger(logger);
    
    // 记录日志
    logger->Info("Application started");
    logger->Debug("Debug info: {}", 123);
    
    // 使用宏
    ONEPLOG_WARN("Warning message");
    ONEPLOG_ERROR_IF(false, "This won't be logged");
    
    // 关键日志确保写入
    logger->CriticalWFC("Critical event occurred");
    
    // 关闭
    logger->Flush();
    logger->Shutdown();
    
    return 0;
}
```
