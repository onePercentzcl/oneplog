# onePlog

高性能 C++17 多进程日志系统

## 特性

- **三种运行模式**：同步、异步、多进程
- **零拷贝**：静态字符串仅存储指针
- **无锁队列**：高并发场景下的最佳性能
- **灵活格式化**：支持模式字符串和 JSON 格式
- **多种输出目标**：控制台、文件（支持轮转）、网络

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
    // 初始化默认日志系统（异步模式）
    oneplog::Init();
    
    // 记录日志
    ONEPLOG_INFO("Hello, {}!", "onePlog");
    ONEPLOG_ERROR("Error code: {}", 42);
    
    // 关闭
    oneplog::Shutdown();
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
    oneplog::Init(config);
    
    // 或者创建自定义 Logger
    auto logger = std::make_shared<oneplog::Logger>("my_logger");
    logger->Init(config);
    logger->Info("Custom logger message");
    
    return 0;
}
```

## 日志级别

| 级别 | 全称 | 4字符 | 1字符 |
|------|------|-------|-------|
| Trace | trace | TRAC | T |
| Debug | debug | DBUG | D |
| Info | info | INFO | I |
| Warn | warn | WARN | W |
| Error | error | ERRO | E |
| Critical | critical | CRIT | C |

## 运行模式

- **同步模式**：日志直接在调用线程中输出
- **异步模式**：日志通过无锁队列传递给后台线程
- **多进程模式**：多个进程共享同一个日志输出

## 构建选项

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `ONEPLOG_BUILD_SHARED` | 构建动态库 | OFF |
| `ONEPLOG_HEADER_ONLY` | 仅头文件模式 | OFF |
| `ONEPLOG_BUILD_TESTS` | 构建测试 | ON |
| `ONEPLOG_BUILD_EXAMPLES` | 构建示例 | ON |
| `ONEPLOG_USE_FMT` | 使用 fmt 库 | OFF |

## 开发进度

- [x] 项目基础设施
- [x] 核心类型定义（Level、Mode、SlotState、ErrorCode）
- [x] BinarySnapshot 实现
- [x] LogEntry 实现（SourceLocation、LogEntryDebug、LogEntryRelease）
- [x] HeapRingBuffer 实现（无锁环形队列、WFC 支持、通知机制、队列满策略）
- [x] SharedRingBuffer 实现（共享内存环形队列，继承自 RingBufferBase）
- [x] SharedMemory 管理器（元数据、配置、进程/线程名称表）
- [x] Format 实现（PatternFormat、JsonFormat）
- [x] Sink 实现（ConsoleSink、FileSink、NetworkSink）
- [x] PipelineThread 实现（多进程模式管道线程）
- [x] WriterThread 实现（日志输出线程）
- [ ] Logger 实现

## 许可证

MIT License
