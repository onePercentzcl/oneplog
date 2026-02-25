# onePlog API 文档

## 目录

- [快速开始](#快速开始)
- [LoggerImpl 类](#loggerimpl-类)
- [预定义配置](#预定义配置)
- [LoggerConfig 配置](#loggerconfig-配置)
- [Sink 类型](#sink-类型)
- [Format 类型](#format-类型)
- [日志级别](#日志级别)
- [运行模式](#运行模式)

---

## 快速开始

```cpp
#include <oneplog/logger.hpp>

int main() {
    // 使用默认配置（同步模式 + 控制台输出）
    oneplog::SyncLogger logger;
    
    // 记录日志
    logger.Info("Hello, {}!", "onePlog");
    logger.Error("Error code: {}", 42);
    
    // 刷新并关闭
    logger.Flush();
    logger.Shutdown();
    
    return 0;
}
```

---

## LoggerImpl 类

LoggerImpl 是 oneplog 的核心日志器类，通过编译期模板配置实现零虚函数调用开销。

### 模板参数

```cpp
template<typename Config = LoggerConfig<>>
class LoggerImpl;
```

- `Config`: 编译期配置类型，通常使用 `LoggerConfig<...>` 或预定义配置

### 构造函数

```cpp
// 默认构造
LoggerImpl();

// 使用运行时配置
explicit LoggerImpl(const RuntimeConfig& config);

// 使用 SinkBindings
explicit LoggerImpl(SinkBindings bindings);

// 使用 SinkBindings 和运行时配置
LoggerImpl(SinkBindings bindings, const RuntimeConfig& config);
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

仅当配置中 `EnableWFC = true` 时可用：

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

### 控制方法

```cpp
void Flush();      // 刷新缓冲区，等待所有日志写入完成
void Shutdown();   // 关闭日志器，停止工作线程
bool IsRunning();  // 检查日志器是否正在运行（异步模式）
```

### 访问器

```cpp
SinkBindings& GetSinkBindings();
const SinkBindings& GetSinkBindings() const;

RuntimeConfig& GetRuntimeConfig();
const RuntimeConfig& GetRuntimeConfig() const;
```

### MProc 模式专用方法

```cpp
bool IsMProcOwner() const;                    // 是否为 MProc 所有者
SharedLoggerConfig* GetSharedConfig();        // 获取共享配置
ProcessThreadNameTable* GetNameTable();       // 获取名称表
uint32_t RegisterProcess(const std::string& name);  // 注册进程名
uint32_t RegisterModule(const std::string& name);   // 注册模块名
const char* GetProcessName(uint32_t id) const;      // 获取进程名
const char* GetModuleName(uint32_t id) const;       // 获取模块名

// 已弃用（向后兼容）
[[deprecated]] uint32_t RegisterThread(const std::string& name);  // 使用 RegisterModule
[[deprecated]] const char* GetThreadName(uint32_t id) const;      // 使用 GetModuleName
```

---

## 预定义配置

### 类型别名

```cpp
// 同步日志器（直接输出，无后台线程）
using SyncLogger = LoggerImpl<DefaultSyncConfig>;

// 异步日志器（后台线程处理，高性能）
using AsyncLogger = LoggerImpl<DefaultAsyncConfig>;

// 多进程日志器（共享内存通信）
using MProcLogger = LoggerImpl<DefaultMProcConfig>;

// 向后兼容别名
using SyncLoggerV2 = SyncLogger;
using AsyncLoggerV2 = AsyncLogger;
using MProcLoggerV2 = MProcLogger;

// 旧版 API 兼容（已弃用）
template<typename Config>
using FastLoggerV2 = LoggerImpl<Config>;
```

### 使用示例

```cpp
// 同步日志器
oneplog::SyncLogger syncLogger;
syncLogger.Info("Sync message");

// 异步日志器
oneplog::AsyncLogger asyncLogger;
asyncLogger.Info("Async message");
asyncLogger.Flush();

// 多进程日志器
oneplog::MProcLogger mprocLogger;
mprocLogger.Info("MProc message");
```

---

## LoggerConfig 配置

### 模板参数

```cpp
template<
    Mode M = Mode::Sync,                              // 运行模式
    Level L = kDefaultLevel,                          // 最小日志级别
    bool EnableWFC = false,                           // 启用 WFC
    bool EnableShadowTail = true,                     // 启用影子尾指针优化
    bool UseFmt = true,                               // 使用 fmt 库
    size_t HeapRingBufferCapacity = 8192,             // 堆环形队列容量
    size_t SharedRingBufferCapacity = 8192,           // 共享环形队列容量
    QueueFullPolicy Policy = QueueFullPolicy::Block,  // 队列满策略
    typename SharedMemoryNameT = DefaultSharedMemoryName,  // 共享内存名称
    int64_t PollTimeoutMs = 10,                       // 轮询超时（毫秒）
    typename SinkBindingsT = DefaultSinkBindings      // Sink 绑定列表
>
struct LoggerConfig;
```

### 自定义配置示例

```cpp
// 自定义异步配置
using MyAsyncConfig = oneplog::LoggerConfig<
    oneplog::Mode::Async,
    oneplog::Level::Debug,
    false,  // EnableWFC
    true,   // EnableShadowTail
    true,   // UseFmt
    8192,   // HeapRingBufferCapacity
    4096,   // SharedRingBufferCapacity
    oneplog::QueueFullPolicy::DropNewest,
    oneplog::DefaultSharedMemoryName,
    10,     // PollTimeoutMs
    oneplog::SinkBindingList<
        oneplog::SinkBinding<oneplog::ConsoleSinkType, oneplog::SimpleFormat>
    >
>;

oneplog::LoggerImpl<MyAsyncConfig> logger;
```

### QueueFullPolicy

```cpp
enum class QueueFullPolicy {
    Block,       // 阻塞等待（默认）
    DropNewest,  // 丢弃最新消息
    DropOldest   // 丢弃最旧消息
};
```

---

## Sink 类型

### 内置 Sink 类型

| Sink 类型 | 说明 |
|----------|------|
| `ConsoleSinkType` | 输出到 stdout |
| `StderrSinkType` | 输出到 stderr |
| `FileSinkType` | 输出到文件（支持轮转） |
| `NullSinkType` | 丢弃所有输出（用于基准测试） |

### SinkBinding

将 Sink 类型与 Format 类型绑定：

```cpp
template<typename SinkT, typename FormatT>
struct SinkBinding;
```

### SinkBindingList

组合多个 SinkBinding：

```cpp
template<typename... Bindings>
struct SinkBindingList;
```

### 多 Sink 示例

```cpp
using MultiSinkConfig = oneplog::LoggerConfig<
    oneplog::Mode::Sync,
    oneplog::Level::Info,
    false, true, true,
    8192, 8192,
    oneplog::QueueFullPolicy::DropNewest,
    oneplog::DefaultSharedMemoryName,
    10,
    oneplog::SinkBindingList<
        oneplog::SinkBinding<oneplog::ConsoleSinkType, oneplog::SimpleFormat>,
        oneplog::SinkBinding<oneplog::FileSinkType, oneplog::FullFormat>
    >
>;

oneplog::LoggerImpl<MultiSinkConfig> logger;
logger.Info("Output to both console and file");
```

---

## Format 类型

### 内置 Format 类型

| 格式化器 | 输出格式 | 需要的元数据 |
|---------|---------|-------------|
| `MessageOnlyFormat` | `message` | 无 |
| `SimpleFormat` | `[HH:MM:SS] [LEVEL] message` | 时间戳、级别 |
| `FullFormat` | `[YYYY-MM-DD HH:MM:SS.mmm] [LEVEL] [PID:TID] message` | 全部 |

### 元数据需求

Format 类型通过静态常量声明其元数据需求：

```cpp
struct SimpleFormat {
    static constexpr bool kNeedsTimestamp = true;
    static constexpr bool kNeedsLevel = true;
    static constexpr bool kNeedsThreadId = false;
    static constexpr bool kNeedsProcessId = false;
    static constexpr bool kNeedsSourceLocation = false;
};
```

LoggerImpl 会在编译期计算所有 Format 的元数据需求，仅获取必要的元数据。

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

### 级别显示

| 级别 | 全称 | 4字符 | 1字符 | 颜色 |
|------|------|-------|-------|------|
| Trace | trace | TRAC | T | 无 |
| Debug | debug | DBUG | D | 蓝色 |
| Info | info | INFO | I | 绿色 |
| Warn | warn | WARN | W | 黄色 |
| Error | error | ERRO | E | 红色 |
| Critical | critical | CRIT | C | 紫色 |

### 编译期级别过滤

```cpp
// 只记录 Warn 及以上级别
using WarnConfig = oneplog::LoggerConfig<
    oneplog::Mode::Sync,
    oneplog::Level::Warn,  // 最小级别
    // ... 其他参数
>;

oneplog::LoggerImpl<WarnConfig> logger;
logger.Debug("This won't be compiled");  // 编译期优化掉
logger.Warn("This will be logged");      // 正常记录
```

---

## 运行模式

| 模式 | 说明 | 适用场景 |
|------|------|----------|
| `Mode::Sync` | 同步模式，日志直接写入 | 调试、低吞吐量场景 |
| `Mode::Async` | 异步模式，后台线程写入 | 高性能、生产环境 |
| `Mode::MProc` | 多进程模式，共享内存通信 | 多进程应用 |

### 模式选择建议

- **Sync**: 适合调试和简单应用，日志立即写入，便于调试
- **Async**: 适合生产环境，高吞吐量，不阻塞主线程
- **MProc**: 适合多进程应用，通过共享内存实现跨进程日志收集

---

## RuntimeConfig

运行时配置结构：

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
config.processName = "my_app";
config.colorEnabled = true;

oneplog::SyncLogger logger(config);
logger.Info("Hello from my_app");
```

---

## FileSinkConfig

文件 Sink 配置：

```cpp
struct FileSinkConfig {
    std::string filename;       // 文件路径
    size_t maxSize{0};          // 最大文件大小（0 = 无限制）
    size_t maxFiles{0};         // 轮转文件数量（0 = 不轮转）
    bool rotateOnOpen{false};   // 打开时轮转
};
```

---

## 完整示例

### 基本用法

```cpp
#include <oneplog/logger.hpp>

int main() {
    oneplog::SyncLogger logger;
    
    logger.Trace("Trace message");
    logger.Debug("Debug value: {}", 123);
    logger.Info("Application started");
    logger.Warn("Warning message");
    logger.Error("Error occurred");
    logger.Critical("Critical error");
    
    logger.Flush();
    return 0;
}
```

### 异步高性能用法

```cpp
#include <oneplog/logger.hpp>

int main() {
    oneplog::AsyncLogger logger;
    
    // 高吞吐量日志记录
    for (int i = 0; i < 1000000; ++i) {
        logger.Info("Message {}", i);
    }
    
    // 确保所有日志写入完成
    logger.Flush();
    logger.Shutdown();
    
    return 0;
}
```

### 文件输出

```cpp
#include <oneplog/logger.hpp>

int main() {
    using FileConfig = oneplog::LoggerConfig<
        oneplog::Mode::Async,
        oneplog::Level::Info,
        false, true, true,
        8192, 4096,
        oneplog::QueueFullPolicy::DropNewest,
        oneplog::DefaultSharedMemoryName,
        10,
        oneplog::SinkBindingList<
            oneplog::SinkBinding<oneplog::FileSinkType, oneplog::SimpleFormat>
        >
    >;
    
    oneplog::LoggerImpl<FileConfig> logger;
    logger.Info("Logged to file");
    logger.Flush();
    
    return 0;
}
```
