# onePlog

高性能 C++17 多进程日志系统

## 特性

- **三种运行模式**：同步、异步、多进程
- **零拷贝**：静态字符串仅存储指针
- **无锁队列**：高并发场景下的最佳性能
- **编译期配置**：通过模板参数在编译时确定所有配置，零运行时开销
- **灵活格式化**：支持控制台、文件、JSON 格式
- **多种输出目标**：控制台、文件（支持轮转）
- **彩色输出**：Release 模式支持 ANSI 颜色
- **fmt 库支持**：可选使用 fmt 库进行格式化

## 项目结构

```
include/oneplog/
├── oneplog.hpp          # 主头文件（包含此文件即可使用所有功能）
├── common.hpp           # 核心类型定义（Level、Mode、ErrorCode 等）
├── logger.hpp           # Logger 类（FastLoggerV2 模板类）
├── name_manager.hpp     # 进程名/模块名管理
├── sinks/
│   └── sink.hpp         # Sink 类型定义
└── internal/            # 内部实现（不建议直接使用）
    ├── binary_snapshot.hpp      # 参数捕获
    ├── log_entry.hpp            # 日志条目
    ├── heap_memory.hpp          # 堆内存环形队列
    ├── shared_memory.hpp        # 共享内存管理
    ├── logger_config.hpp        # Logger 配置
    ├── static_formats.hpp       # 格式化器
    ├── pipeline_thread.hpp      # 管道线程（多进程模式）
    └── ...
```

## 快速开始

### 在其他项目中使用 oneplog

#### 方式 1：Git Submodule（推荐）

```bash
# 添加 submodule
git submodule add https://github.com/onePercentzcl/oneplog.git third_party/oneplog
```

**CMake：**
```cmake
add_subdirectory(third_party/oneplog)
target_link_libraries(your_target PRIVATE oneplog)
```

**XMake：**
```lua
includes("third_party/oneplog")
target("your_target")
    add_deps("oneplog")
```

#### 方式 2：CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(oneplog
    GIT_REPOSITORY https://github.com/onePercentzcl/oneplog.git
    GIT_TAG v0.2.0)
FetchContent_MakeAvailable(oneplog)
target_link_libraries(your_target PRIVATE oneplog)
```

#### 方式 3：XMake 远程包（推荐）

```lua
-- xmake.lua
add_rules("mode.debug", "mode.release")
set_languages("c++17")

-- 添加 onePercent 仓库
add_repositories("onePercent-repo https://github.com/onePercentzcl/xmake-repo")

-- 添加 oneplog 依赖
add_requires("oneplog")

target("your_target")
    set_kind("binary")
    add_files("src/*.cpp")
    add_packages("oneplog")
```

### 构建 oneplog 本身

使用 CMake：
```bash
mkdir build && cd build
cmake ..
make
```

### 基本用法

```cpp
#include <oneplog/logger.hpp>

int main() {
    // 创建同步模式 Logger（最简单的用法）
    oneplog::SyncLogger logger;
    
    // 记录日志
    logger.Info("Hello, {}!", "onePlog");
    logger.Error("Error code: {}", 42);
    logger.Debug("Debug message");
    
    return 0;
}
```

### 异步模式

```cpp
#include <oneplog/logger.hpp>

int main() {
    // 创建异步模式 Logger
    oneplog::AsyncLogger logger;
    
    // 记录日志（立即返回，后台线程处理）
    logger.Info("Async message");
    
    // 确保所有日志都被处理
    logger.Flush();
    
    return 0;
}
```

### 自定义配置

```cpp
#include <oneplog/logger.hpp>

int main() {
    // 使用自定义配置
    using MyConfig = oneplog::FastLoggerConfig<
        oneplog::Mode::Async,           // 异步模式
        oneplog::Level::Debug,          // Debug 级别
        false,                          // 禁用 WFC
        true,                           // 启用时间戳
        true,                           // 启用源位置
        8192,                           // 缓冲区容量
        4096,                           // 最大消息大小
        oneplog::internal::QueueFullPolicy::DropNewest,
        oneplog::DefaultSharedMemoryName,
        10,                             // 轮询间隔 (ms)
        oneplog::SinkBindingList<
            oneplog::SinkBinding<oneplog::FileSinkType, oneplog::SimpleFormat>
        >
    >;
    
    oneplog::FastLoggerV2<MyConfig> logger;
    logger.Info("Custom config logger");
    
    return 0;
}
```

### 输出到文件

```cpp
#include <oneplog/logger.hpp>

int main() {
    // 使用文件输出的配置
    using FileConfig = oneplog::FastLoggerConfig<
        oneplog::Mode::Sync,
        oneplog::Level::Info,
        false, true, true,
        8192, 4096,
        oneplog::internal::QueueFullPolicy::DropNewest,
        oneplog::DefaultSharedMemoryName,
        10,
        oneplog::SinkBindingList<
            oneplog::SinkBinding<oneplog::FileSinkType, oneplog::SimpleFormat>
        >
    >;
    
    // 创建文件 Sink 配置
    oneplog::FileSinkConfig fileConfig;
    fileConfig.filename = "app.log";
    fileConfig.maxFileSize = 10 * 1024 * 1024;  // 10MB
    fileConfig.maxFiles = 5;
    
    // 创建 Logger
    oneplog::FastLoggerV2<FileConfig> logger(
        oneplog::RuntimeLoggerConfig{},
        oneplog::SinkBindingList<
            oneplog::SinkBinding<oneplog::FileSinkType, oneplog::SimpleFormat>
        >(fileConfig)
    );
    
    logger.Info("Logged to file");
    
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

## 预定义类型别名

```cpp
// 同步模式
using SyncLogger = FastLoggerV2<DefaultSyncConfig>;

// 异步模式
using AsyncLogger = FastLoggerV2<DefaultAsyncConfig>;

// 多进程模式
using MProcLogger = FastLoggerV2<DefaultMProcConfig>;

// 向后兼容别名
using Logger = SyncLogger;
using SyncLoggerV2 = SyncLogger;
using AsyncLoggerV2 = AsyncLogger;
using MProcLoggerV2 = MProcLogger;
```

## 构建选项

### CMake 选项

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `ONEPLOG_BUILD_SHARED` | 构建动态库 | OFF |
| `ONEPLOG_HEADER_ONLY` | 仅头文件模式 | ON |
| `ONEPLOG_BUILD_TESTS` | 构建测试 | OFF |
| `ONEPLOG_BUILD_EXAMPLES` | 构建示例 | OFF |
| `ONEPLOG_USE_FMT` | 使用 fmt 库 | OFF |

### 构建模式

```bash
# Header-only 模式（默认）
cmake -B build

# 静态库模式
cmake -B build -DONEPLOG_HEADER_ONLY=OFF

# 动态库模式
cmake -B build -DONEPLOG_HEADER_ONLY=OFF -DONEPLOG_BUILD_SHARED=ON

# 启用 fmt 库
cmake -B build -DONEPLOG_USE_FMT=ON
```

## 性能测试

在 Apple M4 Pro (14 核) macOS 上的测试结果：

### Logger 与 spdlog 性能对比

| 测试项 | onePlog Logger | spdlog | 对比 |
|--------|----------------|--------|------|
| 同步模式（Null Sink） | 1660 万 ops/sec | 1652 万 ops/sec | +0.5% |
| 同步模式（File Sink） | 879 万 ops/sec | 860 万 ops/sec | +2% |
| 异步模式（Null Sink） | 1577 万 ops/sec | 469 万 ops/sec | +236% |
| 异步模式（File Sink） | 1619 万 ops/sec | 480 万 ops/sec | +237% |

**关键优化**：
- 编译期配置：通过模板参数在编译时确定所有配置
- SinkBindingList：多 Sink 绑定，编译期计算元数据需求
- 条件通知：使用 `NotifyConsumerIfWaiting()` 仅在消费者等待时发送通知
- 按需获取元数据：根据 Format 需求决定是否获取 TID/PID 等

运行性能测试：
```bash
# 编译（需要 fmt 库）
clang++ -std=c++17 -O3 -I include -I /opt/homebrew/opt/fmt/include \
    -DONEPLOG_USE_FMT -DFMT_HEADER_ONLY \
    -o benchmark_compare example/benchmark_compare.cpp -lpthread

# 运行
./benchmark_compare
```

## 许可证

MIT License
