# 同步模式流程图文档 / Sync Mode Flow Diagrams

本文档详细描述 oneplog 日志库同步模式（Sync Mode）的控制流程和数据流程。

This document describes the control flow and data flow of oneplog's synchronous logging mode in detail.

## 概述 / Overview

同步模式是 oneplog 最简单的运行模式。在此模式下，日志消息在调用线程中直接格式化并写入 Sink，无需后台线程或队列缓冲。

Sync mode is the simplest operating mode of oneplog. In this mode, log messages are formatted and written to Sinks directly in the calling thread, without background threads or queue buffering.

**特点 / Features:**
- 零延迟：日志立即写入 / Zero latency: logs are written immediately
- 简单可靠：无并发复杂性 / Simple and reliable: no concurrency complexity
- 适用于低吞吐量场景 / Suitable for low-throughput scenarios

## 编译期元数据需求机制 / Compile-time Metadata Requirements

oneplog 使用 `if constexpr` 在编译期决定是否需要获取各种元数据。如果 Format 不需要某个元数据，相应的获取代码**不会被编译到最终二进制文件中**。

oneplog uses `if constexpr` to determine at compile-time whether to acquire various metadata. If a Format doesn't need certain metadata, the corresponding acquisition code **will not be compiled into the final binary**.

### 元数据需求配置 / Metadata Requirements Configuration

```mermaid
flowchart TB
    subgraph CompileTime["编译期配置 / Compile-time Configuration"]
        Format["Format 类型定义 Requirements"]
        
        subgraph Requirements["StaticFormatRequirements"]
            R1["kNeedsTimestamp: bool"]
            R2["kNeedsLevel: bool"]
            R3["kNeedsThreadId: bool"]
            R4["kNeedsProcessId: bool"]
            R5["kNeedsSourceLocation: bool"]
        end
        
        SinkBinding["SinkBinding<Sink, Format>"]
        SinkBindingList["SinkBindingList<Bindings...>"]
        
        subgraph Union["需求并集 (编译期计算)"]
            U1["kNeedsTimestamp = (B1::kNeedsTimestamp || B2::kNeedsTimestamp || ...)"]
            U2["kNeedsLevel = (B1::kNeedsLevel || B2::kNeedsLevel || ...)"]
            U3["kNeedsThreadId = (B1::kNeedsThreadId || B2::kNeedsThreadId || ...)"]
            U4["kNeedsProcessId = (B1::kNeedsProcessId || B2::kNeedsProcessId || ...)"]
            U5["kNeedsSourceLocation = (B1::kNeedsSourceLocation || B2::kNeedsSourceLocation || ...)"]
        end
    end
    
    Format --> Requirements
    Requirements --> SinkBinding
    SinkBinding --> SinkBindingList
    SinkBindingList --> Union
    
    style CompileTime fill:#e8f5e9
```

### 编译期代码生成 / Compile-time Code Generation

```cpp
// 编译期决定：如果 kNeedsTimestamp 为 false，整个 if 块不会生成代码
// Compile-time decision: if kNeedsTimestamp is false, the entire if block generates no code
if constexpr (kNeedsTimestamp) {
    timestamp = internal::GetNanosecondTimestamp();  // 仅当需要时才编译
}
if constexpr (kNeedsThreadId) {
    threadId = internal::GetCurrentThreadId();       // 仅当需要时才编译
}
if constexpr (kNeedsProcessId) {
    processId = internal::GetCurrentProcessId();     // 仅当需要时才编译
}
// kNeedsSourceLocation 用于 __FILE__, __LINE__, __func__ 等
```

## 控制流程图 / Control Flow Diagram

### 日志调用完整调用链 / Complete Call Chain from Log Call to Sink Output

```mermaid
sequenceDiagram
    participant User as 用户代码<br/>User Code
    participant API as Logger API<br/>(Trace/Debug/Info/Warn/Error/Critical)
    participant LoggerImpl as LoggerImpl<Config>
    participant LogImplSync as LogImplSync<Level>
    participant SinkBindings as SinkBindingList
    participant SinkBinding as SinkBinding<Sink, Format>
    participant Format as Format
    participant Sink as Sink

    User->>API: Info("message {}", arg)
    
    Note over API,LoggerImpl: 编译期级别检查 (if constexpr)<br/>Compile-time level check
    
    alt 级别低于 kMinLevel (编译期排除)
        Note over LoggerImpl: 整个调用被优化掉，不生成代码<br/>Entire call optimized away, no code generated
    else 级别 >= kMinLevel (编译期包含)
        LoggerImpl->>LogImplSync: LogImplSync<Level>(fmt, args...)
        
        Note over LogImplSync: 编译期条件元数据获取 (if constexpr)<br/>Compile-time conditional metadata acquisition
        
        rect rgb(232, 245, 233)
            Note over LogImplSync: 以下代码仅在对应 kNeeds* 为 true 时编译
            alt if constexpr (kNeedsTimestamp)
                LogImplSync->>LogImplSync: timestamp = GetNanosecondTimestamp()
            end
            
            alt if constexpr (kNeedsThreadId)
                LogImplSync->>LogImplSync: threadId = GetCurrentThreadId()
            end
            
            alt if constexpr (kNeedsProcessId)
                LogImplSync->>LogImplSync: processId = GetCurrentProcessId()
            end
        end
        
        LogImplSync->>SinkBindings: WriteAllSync<UseFmt>(level, timestamp, threadId, processId, fmt, args...)
        
        loop 遍历所有 SinkBinding / Iterate all SinkBindings
            SinkBindings->>SinkBinding: WriteSync(...)
            SinkBinding->>Format: FormatTo(buffer, level, timestamp, threadId, processId, fmt, args...)
            Format-->>SinkBinding: 格式化后的消息 / Formatted message
            SinkBinding->>Sink: Write(message)
            Sink-->>SinkBinding: 写入完成 / Write complete
        end
        
        SinkBindings-->>LogImplSync: 所有 Sink 写入完成 / All Sinks written
        LogImplSync-->>User: 返回 / Return
    end
```

### 关键类交互方式 / Key Class Interactions

```mermaid
classDiagram
    class LoggerImpl~Config~ {
        +SinkBindings m_sinkBindings
        +RuntimeConfig m_runtimeConfig
        +Trace(fmt, args...)
        +Debug(fmt, args...)
        +Info(fmt, args...)
        +Warn(fmt, args...)
        +Error(fmt, args...)
        +Critical(fmt, args...)
        -LogImpl~Level~(fmt, args...)
        -LogImplSync~Level~(fmt, args...)
    }
    
    class SinkBindingList~Bindings...~ {
        +tuple~Bindings...~ bindings
        +kNeedsTimestamp : bool
        +kNeedsLevel : bool
        +kNeedsThreadId : bool
        +kNeedsProcessId : bool
        +WriteAllSync~UseFmt~(level, timestamp, threadId, processId, fmt, args...)
        +FlushAll()
        +CloseAll()
    }
    
    class SinkBinding~SinkT, FormatT~ {
        +Sink sink
        +WriteSync~UseFmt~(level, timestamp, threadId, processId, fmt, args...)
        +Flush()
        +Close()
    }
    
    class Format {
        <<interface>>
        +FormatTo(buffer, level, timestamp, threadId, processId, fmt, args...)
        +FormatEntry(entry)
    }
    
    class Sink {
        <<interface>>
        +Write(message)
        +Flush()
        +Close()
    }
    
    LoggerImpl --> SinkBindingList : contains
    SinkBindingList --> SinkBinding : contains multiple
    SinkBinding --> Format : uses
    SinkBinding --> Sink : owns
```

## 数据流程图 / Data Flow Diagram

### 日志数据从格式化到输出的流转 / Log Data Flow from Formatting to Output

```mermaid
flowchart TB
    subgraph Input["输入层 / Input Layer"]
        FmtStr["格式字符串<br/>Format String<br/>(const char*)"]
        Args["参数列表<br/>Arguments<br/>(Args&&...)"]
        Level["日志级别<br/>Log Level"]
    end
    
    subgraph Metadata["元数据获取 / Metadata Acquisition"]
        direction TB
        TS["时间戳<br/>Timestamp<br/>(uint64_t nanoseconds)"]
        TID["线程 ID<br/>Thread ID<br/>(uint32_t)"]
        PID["进程 ID<br/>Process ID<br/>(uint32_t)"]
        
        Note1["条件编译：仅获取 Format 需要的元数据<br/>Conditional: only acquire metadata needed by Format"]
    end
    
    subgraph Formatting["格式化层 / Formatting Layer"]
        FmtBuffer["fmt::memory_buffer"]
        FormatTo["Format::FormatTo()"]
        
        FmtBuffer --> FormatTo
    end
    
    subgraph Output["输出层 / Output Layer"]
        direction TB
        Console["ConsoleSink<br/>控制台输出"]
        File["FileSink<br/>文件输出"]
        Network["NetworkSink<br/>网络输出"]
    end
    
    FmtStr --> FormatTo
    Args --> FormatTo
    Level --> FormatTo
    TS --> FormatTo
    TID --> FormatTo
    PID --> FormatTo
    
    FormatTo --> Console
    FormatTo --> File
    FormatTo --> Network
    
    style Input fill:#e1f5fe
    style Metadata fill:#fff3e0
    style Formatting fill:#f3e5f5
    style Output fill:#e8f5e9
```

### 多 Sink 并行输出流程 / Multi-Sink Parallel Output Flow

```mermaid
flowchart LR
    subgraph LogCall["日志调用"]
        Call["Info('User {} logged in', username)"]
    end
    
    subgraph Processing["处理"]
        Metadata["收集元数据"]
        Format1["Format 1<br/>(SimpleFormat)"]
        Format2["Format 2<br/>(DetailedFormat)"]
    end
    
    subgraph Sinks["Sink 输出"]
        Sink1["ConsoleSink<br/>[INFO] User admin logged in"]
        Sink2["FileSink<br/>2024-01-15 10:30:45.123 [INFO] [tid:1234] User admin logged in"]
    end
    
    Call --> Metadata
    Metadata --> Format1
    Metadata --> Format2
    Format1 --> Sink1
    Format2 --> Sink2
```

## LoggerImpl、SinkBindings、Format 交互详解 / Detailed Interaction

### 编译期配置传递 / Compile-time Configuration Propagation

```mermaid
flowchart TB
    subgraph Config["LoggerConfig 编译期配置"]
        Mode["Mode::Sync"]
        MinLevel["Level::Info"]
        SinkBindingsType["SinkBindingList<br/>SinkBinding<ConsoleSink, SimpleFormat>,<br/>SinkBinding<FileSink, DetailedFormat>"]
    end
    
    subgraph Requirements["格式化需求聚合 (编译期)"]
        R1["SimpleFormat::Requirements<br/>kNeedsTimestamp = true<br/>kNeedsLevel = true<br/>kNeedsThreadId = false<br/>kNeedsProcessId = false<br/>kNeedsSourceLocation = false"]
        R2["DetailedFormat::Requirements<br/>kNeedsTimestamp = true<br/>kNeedsLevel = true<br/>kNeedsThreadId = true<br/>kNeedsProcessId = false<br/>kNeedsSourceLocation = true"]
        Union["SinkBindingList 需求并集<br/>kNeedsTimestamp = true<br/>kNeedsLevel = true<br/>kNeedsThreadId = true<br/>kNeedsProcessId = false<br/>kNeedsSourceLocation = true"]
    end
    
    subgraph CodeGen["编译期代码生成"]
        Gen1["✓ if constexpr (kNeedsTimestamp) → 生成代码"]
        Gen2["✓ if constexpr (kNeedsThreadId) → 生成代码"]
        Gen3["✗ if constexpr (kNeedsProcessId) → 不生成代码"]
        Gen4["✓ if constexpr (kNeedsSourceLocation) → 生成代码"]
    end
    
    Config --> Requirements
    R1 --> Union
    R2 --> Union
    Union --> CodeGen
    
    style Gen3 fill:#ffcdd2
    style Gen1 fill:#c8e6c9
    style Gen2 fill:#c8e6c9
    style Gen4 fill:#c8e6c9
```

### WriteSync 方法执行流程 / WriteSync Method Execution Flow

```mermaid
sequenceDiagram
    participant SB as SinkBinding
    participant Fmt as Format
    participant Buffer as fmt::memory_buffer
    participant Sink as Sink

    Note over SB: WriteSync<UseFmt=true> 执行
    
    SB->>Buffer: 创建 buffer
    SB->>Fmt: FormatTo(buffer, level, timestamp, threadId, processId, fmt, args...)
    
    Note over Fmt: 格式化过程
    Fmt->>Buffer: 写入时间戳 "[2024-01-15 10:30:45.123]"
    Fmt->>Buffer: 写入级别 "[INFO]"
    Fmt->>Buffer: 写入线程 ID "[tid:1234]"
    Fmt->>Buffer: 使用 fmt::format_to 格式化消息
    
    Fmt-->>SB: 格式化完成
    
    SB->>Sink: Write(string_view(buffer.data(), buffer.size()))
    
    Note over Sink: 写入目标
    alt ConsoleSink
        Sink->>Sink: fputs(message, stdout)
    else FileSink
        Sink->>Sink: file << message << '\n'
    end
    
    Sink-->>SB: 写入完成
```

## 错误处理 / Error Handling

同步模式下的错误处理策略：

```mermaid
flowchart TB
    subgraph ErrorHandling["错误处理策略"]
        Try["try { ... }"]
        
        subgraph Errors["可能的错误"]
            FmtError["格式化错误<br/>Format Error"]
            WriteError["写入错误<br/>Write Error"]
            IOError["I/O 错误<br/>I/O Error"]
        end
        
        Catch["catch (...) { }"]
        Silent["静默忽略<br/>Silently Ignore"]
    end
    
    Try --> Errors
    Errors --> Catch
    Catch --> Silent
    
    Note1["设计原则：日志系统不应影响主程序运行<br/>Design principle: logging should not affect main program"]
```

## 性能特点 / Performance Characteristics

| 特性 / Feature | 同步模式 / Sync Mode |
|----------------|---------------------|
| 延迟 / Latency | 取决于 Sink I/O / Depends on Sink I/O |
| 吞吐量 / Throughput | 受 I/O 限制 / I/O bound |
| 内存使用 / Memory | 最小（无队列）/ Minimal (no queue) |
| 线程安全 / Thread Safety | 由 Sink 保证 / Guaranteed by Sink |
| 日志丢失 / Log Loss | 无 / None |
| 适用场景 / Use Case | 调试、低频日志 / Debug, low-frequency logging |

## 相关需求 / Related Requirements

- 需求 1.1: 创建同步模式的控制流程图
- 需求 1.2: 创建同步模式的数据流程图
- 需求 1.3: 标注关键类和方法的调用关系
- 需求 1.4: 说明 LoggerImpl、SinkBindings、Format 之间的交互方式
- 需求 1.5: 使用 Mermaid 格式创建可渲染的流程图
