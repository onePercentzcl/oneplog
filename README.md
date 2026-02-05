# onePlog

高性能 C++17 多进程聚合日志系统

## 特性

- **零拷贝**：使用 BinarySnapshot 捕获参数，静态字符串仅存储指针
- **零分配**：使用预分配内存池，避免运行时堆分配
- **低延迟**：使用无锁环形队列，采用抢占索引再写入的方式
- **编译时优化**：禁用的功能不产生任何运行时开销
- **多模式支持**：同步、异步、多进程三种运行模式
- **跨平台**：支持 Linux、macOS、HarmonyOS、OpenHarmony

## 运行模式

| 模式 | 说明 | 适用场景 |
|------|------|---------|
| Sync | 同步模式，直接在调用线程完成格式化和输出 | 调试、简单应用 |
| Async | 异步模式，通过无锁队列传输到后台线程处理 | 高性能应用 |
| MProc | 多进程模式，多个进程的日志聚合到消费者进程 | 分布式系统 |

## 快速开始

### 基本使用

```cpp
#include <oneplog/oneplog.hpp>

int main() {
    // 初始化
    oneplog::Init();
    
    // 记录日志
    oneplog::Info("Hello, {}!", "onePlog");
    oneplog::Debug("Debug message: value = {}", 42);
    oneplog::Error("Error occurred: {}", "file not found");
    
    // 关闭
    oneplog::Shutdown();
    return 0;
}
```

### 使用全局配置

```cpp
#include <oneplog/oneplog.hpp>

int main() {
    // 设置全局配置
    oneplog::config::mode = oneplog::Mode::Async;
    oneplog::config::level = oneplog::Level::Debugging;
    oneplog::config::heapSlotCount = 2048;
    oneplog::config::processName = "myapp";
    
    // 使用全局配置初始化
    oneplog::Init();
    
    oneplog::Info("Application started");
    
    oneplog::Shutdown();
    return 0;
}
```

### 使用 LoggerConfig

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

### 使用宏

```cpp
#include <oneplog/oneplog.hpp>

int main() {
    oneplog::Init();
    
    ONEPLOG_INFO("Info message");
    ONEPLOG_DEBUG("Debug: x = {}", 100);
    ONEPLOG_ERROR("Error: {}", "something went wrong");
    
    // 条件日志
    ONEPLOG_IF(shouldLog, INFO, "Conditional message");
    
    oneplog::Shutdown();
    return 0;
}
```

## 日志级别

| 级别 | 值 | 说明 |
|------|-----|------|
| Alert | 0 | 必须立即采取行动 |
| Critical | 1 | 严重情况 |
| Error | 2 | 运行时错误 |
| Warning | 3 | 警告信息 |
| Notice | 4 | 正常但重要的情况 |
| Informational | 5 | 普通信息 |
| Debugging | 6 | 调试信息 |
| Trace | 7 | 最详细的跟踪信息 |
| Off | 8 | 关闭日志 |

## 构建

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

## 项目结构

```
oneplog/
├── src/                          # 源代码目录
├── include/                      # 头文件目录
├── example/                      # 示例代码
├── tests/                        # 测试代码
├── docs/                         # 文档目录
├── CMakeLists.txt                # CMake 构建文件
├── xmake.lua                     # XMake 构建文件
└── .clang-format                 # clang-format 配置
```

## 开发状态

当前处于模板化重构阶段，详见 `.kiro/specs/template-logger-refactor/` 目录下的设计文档。

## 许可证

MIT License
