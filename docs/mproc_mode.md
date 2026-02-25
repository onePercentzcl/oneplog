# 多进程模式流程图文档 / Multi-Process Mode Flow Diagrams

本文档详细描述 oneplog 日志库多进程模式（MProc Mode）的控制流程和数据流程。

This document describes the control flow and data flow of oneplog's multi-process logging mode in detail.

## 概述 / Overview

多进程模式是 oneplog 的跨进程日志聚合模式。在此模式下，多个生产者进程可以将日志写入共享内存中的 SharedRingBuffer，由消费者进程统一处理和输出。

MProc mode is oneplog's cross-process log aggregation mode. In this mode, multiple producer processes can write logs to SharedRingBuffer in shared memory, which is then processed and output by a consumer process.

**特点 / Features:**
- 跨进程日志聚合 / Cross-process log aggregation
- 共享内存通信 / Shared memory communication
- 进程/模块名称注册 / Process/module name registration
- 配置同步 / Configuration synchronization

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
// 多进程模式：timestamp 总是需要（用于日志排序）
// MProc mode: timestamp is always needed (for log ordering)
entry.timestamp = internal::GetNanosecondTimestamp();
entry.level = LogLevel;

// 编译期决定：如果 kNeedsThreadId 为 false，整个 if 块不会生成代码
// Compile-time decision: if kNeedsThreadId is false, the entire if block generates no code
if constexpr (kNeedsThreadId) {
    entry.threadId = internal::GetCurrentThreadId();   // 仅当需要时才编译
}
if constexpr (kNeedsProcessId) {
    entry.processId = internal::GetCurrentProcessId(); // 仅当需要时才编译
}
// 注意：MProc 模式下 PipelineThread 会在转发时添加 processId
// Note: In MProc mode, PipelineThread adds processId during forwarding
```

## 架构概览 / Architecture Overview

```mermaid
flowchart TB
    subgraph Producer1["生产者进程 1"]
        P1_Logger["LoggerImpl<MProcConfig>"]
        P1_Heap["HeapRingBuffer"]
        P1_Pipeline["PipelineThread"]
    end
    
    subgraph Producer2["生产者进程 2"]
        P2_Logger["LoggerImpl<MProcConfig>"]
        P2_Heap["HeapRingBuffer"]
        P2_Pipeline["PipelineThread"]
    end
    
    subgraph SharedMem["共享内存 / Shared Memory"]
        Metadata["SharedMemoryMetadata"]
        Config["SharedLoggerConfig"]
        NameTable["ProcessThreadNameTable"]
        SharedRing["SharedRingBuffer"]
    end
    
    subgraph Consumer["消费者进程"]
        C_Logger["LoggerImpl<MProcConfig>"]
        C_Worker["WriterThread"]
        C_Sinks["SinkBindings"]
    end
    
    P1_Logger --> P1_Heap
    P1_Heap --> P1_Pipeline
    P1_Pipeline --> SharedRing
    
    P2_Logger --> P2_Heap
    P2_Heap --> P2_Pipeline
    P2_Pipeline --> SharedRing
    
    SharedRing --> C_Worker
    C_Worker --> C_Sinks
    
    style SharedMem fill:#fff3e0
```

## 控制流程图 / Control Flow Diagram

### 普通日志 (OUT) vs WFC 日志流程对比 / Normal Log (OUT) vs WFC Log Flow Comparison

```mermaid
flowchart TB
    subgraph NormalLog["普通日志 (OUT) - 非阻塞"]
        N1["Info('message', args...)"]
        N2["LogImplAsync<Level>()"]
        N3["创建 LogEntry + 编译期条件元数据"]
        N4["HeapRingBuffer.TryPush()"]
        N5["立即返回<br/>(不等待 PipelineThread 或消费者)"]
        
        N1 --> N2 --> N3 --> N4 --> N5
    end
    
    subgraph WFCLog["WFC 日志 - 等待完成"]
        W1["InfoWFC('message', args...)"]
        W2["LogImplWFC<Level>()"]
        W3["创建 LogEntry + 编译期条件元数据"]
        W4["HeapRingBuffer.TryPushWFC()"]
        W5["等待 HeapRingBuffer 处理完成"]
        W6["(注意: MProc 模式下 WFC 仅等待到 HeapRingBuffer)"]
        
        W1 --> W2 --> W3 --> W4 --> W5 --> W6
    end
    
    style N5 fill:#c8e6c9
    style W5 fill:#fff9c4
```

### 生产者进程、消费者进程和 PipelineThread 的交互 / Interaction Between Producer, Consumer and PipelineThread

```mermaid
sequenceDiagram
    participant P1 as 生产者进程 1<br/>Producer Process 1
    participant P2 as 生产者进程 2<br/>Producer Process 2
    participant HeapBuf1 as HeapRingBuffer 1
    participant HeapBuf2 as HeapRingBuffer 2
    participant Pipeline1 as PipelineThread 1
    participant Pipeline2 as PipelineThread 2
    participant SharedMem as SharedMemory
    participant SharedBuf as SharedRingBuffer
    participant Consumer as 消费者进程<br/>Consumer Process
    participant Sink as Sink

    Note over P1,Sink: 初始化阶段 / Initialization Phase
    
    Consumer->>SharedMem: SharedMemory::Create(name, capacity, policy)
    SharedMem->>SharedBuf: 创建 SharedRingBuffer
    SharedMem->>SharedMem: 初始化 NameTable, Config
    Consumer->>Consumer: StartMProcWorker()
    
    P1->>SharedMem: SharedMemory::Connect(name)
    P1->>HeapBuf1: 创建 HeapRingBuffer
    P1->>Pipeline1: 创建并启动 PipelineThread
    P1->>SharedMem: RegisterProcess("Process1")
    
    P2->>SharedMem: SharedMemory::Connect(name)
    P2->>HeapBuf2: 创建 HeapRingBuffer
    P2->>Pipeline2: 创建并启动 PipelineThread
    P2->>SharedMem: RegisterProcess("Process2")
    
    Note over P1,Sink: 运行阶段 - 普通日志 (OUT) / Runtime Phase - Normal Log (OUT)
    
    par 生产者进程 1
        P1->>P1: Info("message", args...)
        
        Note over P1: 编译期级别检查 + 条件元数据获取
        
        rect rgb(232, 245, 233)
            Note over P1: if constexpr (kNeedsThreadId) → 获取 threadId
            Note over P1: if constexpr (kNeedsProcessId) → 获取 processId
        end
        
        P1->>HeapBuf1: TryPush(entry)
        P1->>P1: 立即返回 (不等待)
        
        Pipeline1->>HeapBuf1: TryPop(entry)
        Pipeline1->>Pipeline1: ConvertPointers(entry)
        Pipeline1->>Pipeline1: AddProcessId(entry)
        Pipeline1->>SharedBuf: TryPush(entry)
        Pipeline1->>SharedMem: NotifyConsumer()
    and 生产者进程 2
        P2->>P2: Debug("other message", args...)
        
        Note over P2: 编译期级别检查 + 条件元数据获取
        
        P2->>HeapBuf2: TryPush(entry)
        P2->>P2: 立即返回 (不等待)
        
        Pipeline2->>HeapBuf2: TryPop(entry)
        Pipeline2->>Pipeline2: ConvertPointers(entry)
        Pipeline2->>Pipeline2: AddProcessId(entry)
        Pipeline2->>SharedBuf: TryPush(entry)
        Pipeline2->>SharedMem: NotifyConsumer()
    and 消费者进程
        Consumer->>SharedBuf: TryPop(entry)
        Consumer->>Consumer: ProcessEntry(entry)
        Consumer->>Sink: WriteAllAsync(entry)
    end
```

### 初始化流程详解 / Detailed Initialization Flow

```mermaid
flowchart TB
    subgraph Init["InitMProc() 初始化流程"]
        Start["开始初始化"]
        
        TryCreate["尝试创建共享内存<br/>SharedMemory::Create()"]
        
        CreateSuccess{"创建成功?"}
        
        subgraph Owner["作为所有者 (Owner)"]
            O1["m_isMProcOwner = true"]
            O2["创建 HeapRingBuffer"]
            O3["创建 PipelineThread"]
            O4["启动 PipelineThread"]
            O5["注册进程名称"]
            O6["启动 WriterThread"]
        end
        
        TryConnect["尝试连接共享内存<br/>SharedMemory::Connect()"]
        
        ConnectSuccess{"连接成功?"}
        
        subgraph Client["作为客户端 (Client)"]
            C1["m_isMProcOwner = false"]
            C2["注册进程名称"]
            C3["启动 WriterThread"]
        end
        
        Fallback["回退到异步模式<br/>InitAsync()"]
        
        End["初始化完成"]
    end
    
    Start --> TryCreate
    TryCreate --> CreateSuccess
    
    CreateSuccess -->|是| O1
    O1 --> O2 --> O3 --> O4 --> O5 --> O6 --> End
    
    CreateSuccess -->|否| TryConnect
    TryConnect --> ConnectSuccess
    
    ConnectSuccess -->|是| C1
    C1 --> C2 --> C3 --> End
    
    ConnectSuccess -->|否| Fallback
    Fallback --> End
```

## 数据流程图 / Data Flow Diagram

### HeapRingBuffer → SharedRingBuffer → Sink 完整流程 / Complete Data Flow

```mermaid
flowchart TB
    subgraph Producer["生产者进程"]
        direction TB
        Log["Info('User {} logged in', username)"]
        Entry1["创建 LogEntry"]
        Capture["BinarySnapshot 捕获参数<br/>(可能包含指针)"]
        HeapPush["HeapRingBuffer.TryPush()"]
    end
    
    subgraph Pipeline["PipelineThread"]
        direction TB
        HeapPop["HeapRingBuffer.TryPop()"]
        Convert["ConvertPointers()<br/>指针数据 → 内联数据"]
        AddPID["AddProcessId()<br/>添加进程 ID"]
        SharedPush["SharedRingBuffer.TryPush()"]
        Notify["NotifyConsumer()"]
    end
    
    subgraph SharedMemory["共享内存"]
        direction TB
        SharedBuf["SharedRingBuffer<br/>(跨进程可见)"]
    end
    
    subgraph Consumer["消费者进程"]
        direction TB
        SharedPop["SharedRingBuffer.TryPop()"]
        Process["ProcessEntry()"]
        Format["Format::FormatEntryTo()"]
        Write["Sink::Write()"]
    end
    
    Log --> Entry1 --> Capture --> HeapPush
    HeapPush --> HeapPop
    HeapPop --> Convert --> AddPID --> SharedPush
    SharedPush --> SharedBuf
    SharedBuf --> SharedPop
    Notify -.-> SharedPop
    SharedPop --> Process --> Format --> Write
    
    style Producer fill:#e3f2fd
    style Pipeline fill:#fff8e1
    style SharedMemory fill:#ffecb3
    style Consumer fill:#e8f5e9
```

### 指针转换过程 / Pointer Conversion Process

```mermaid
flowchart LR
    subgraph Before["转换前 (进程内)"]
        B1["BinarySnapshot"]
        B2["format_string: 0x7fff1234<br/>(指向进程内存)"]
        B3["arg_data: 指针引用"]
    end
    
    subgraph Convert["ConvertPointersToData()"]
        C1["遍历所有参数"]
        C2["将指针指向的数据<br/>复制到内联缓冲区"]
        C3["更新引用为内联数据"]
    end
    
    subgraph After["转换后 (可跨进程)"]
        A1["BinarySnapshot"]
        A2["format_string: 内联存储<br/>'User {} logged in'"]
        A3["arg_data: 内联存储"]
    end
    
    Before --> Convert --> After
```

## 共享内存的创建、附加和同步机制 / Shared Memory Creation, Attachment and Synchronization

### 共享内存布局 / Shared Memory Layout

```mermaid
flowchart TB
    subgraph Layout["共享内存布局 (缓存行对齐)"]
        direction TB
        
        subgraph Meta["SharedMemoryMetadata (64B 对齐)"]
            M1["magic: 0x4F4E4550 ('ONEP')"]
            M2["version: 1"]
            M3["totalSize"]
            M4["configOffset, nameTableOffset, ringBufferOffset"]
            M5["ringBufferCapacity, policy"]
        end
        
        subgraph Config["SharedLoggerConfig (64B 对齐)"]
            C1["logLevel: atomic<Level>"]
            C2["configVersion: atomic<uint32_t>"]
        end
        
        subgraph NameTable["ProcessThreadNameTable"]
            N1["processCount: atomic<uint32_t>"]
            N2["threadCount: atomic<uint32_t>"]
            N3["processes[64]: NameIdMapping"]
            N4["threads[256]: NameIdMapping"]
        end
        
        subgraph RingBuffer["SharedRingBuffer"]
            R1["RingBufferHeader"]
            R2["SlotStatus[capacity]"]
            R3["LogEntry[capacity]"]
        end
    end
    
    Meta --> Config --> NameTable --> RingBuffer
```

### 创建和连接流程 / Create and Connect Flow

```mermaid
sequenceDiagram
    participant Owner as 所有者进程<br/>Owner Process
    participant OS as 操作系统
    participant Client as 客户端进程<br/>Client Process

    Note over Owner,Client: 创建共享内存 / Create Shared Memory
    
    Owner->>OS: shm_open(name, O_CREAT | O_RDWR | O_EXCL)
    OS-->>Owner: fd
    Owner->>OS: ftruncate(fd, totalSize)
    Owner->>OS: mmap(fd, PROT_READ | PROT_WRITE, MAP_SHARED)
    OS-->>Owner: memory pointer
    
    Owner->>Owner: 初始化 SharedMemoryMetadata
    Owner->>Owner: 初始化 SharedLoggerConfig
    Owner->>Owner: 初始化 ProcessThreadNameTable
    Owner->>Owner: 创建 SharedRingBuffer
    
    Note over Owner,Client: 连接共享内存 / Connect to Shared Memory
    
    Client->>OS: shm_open(name, O_RDWR)
    OS-->>Client: fd
    Client->>OS: fstat(fd) 获取大小
    Client->>OS: mmap(fd, PROT_READ | PROT_WRITE, MAP_SHARED)
    OS-->>Client: memory pointer
    
    Client->>Client: 验证 metadata.IsValid()
    Client->>Client: 获取各组件指针
    Client->>Client: 连接 SharedRingBuffer
```

### 同步机制 / Synchronization Mechanism

```mermaid
flowchart TB
    subgraph Atomic["原子操作同步"]
        A1["所有共享变量使用 std::atomic"]
        A2["使用适当的内存序"]
        A3["acquire-release 语义确保可见性"]
    end
    
    subgraph CacheLine["缓存行对齐"]
        C1["热点变量独占缓存行"]
        C2["避免伪共享"]
        C3["alignas(64) 对齐"]
    end
    
    subgraph Notification["通知机制"]
        N1["Linux: eventfd"]
        N2["macOS: dispatch_semaphore"]
        N3["跨进程唤醒"]
    end
    
    Atomic --> CacheLine --> Notification
```

## ProcessThreadNameTable 的名称注册和查找机制 / Name Registration and Lookup

### 名称注册流程 / Name Registration Flow

```mermaid
sequenceDiagram
    participant Process as 进程
    participant Logger as LoggerImpl
    participant SharedMem as SharedMemory
    participant NameTable as ProcessThreadNameTable

    Process->>Logger: RegisterProcess("MyApp")
    Logger->>SharedMem: RegisterProcess("MyApp")
    SharedMem->>NameTable: RegisterProcess("MyApp")
    
    NameTable->>NameTable: 检查是否已存在
    
    alt 名称已存在
        NameTable-->>SharedMem: 返回现有 ID
    else 名称不存在
        NameTable->>NameTable: newIndex = processCount.fetch_add(1)
        
        alt newIndex < kMaxProcesses (64)
            NameTable->>NameTable: processes[newIndex].Set(newIndex+1, "MyApp")
            NameTable-->>SharedMem: 返回新 ID
        else 超出限制
            NameTable->>NameTable: processCount.fetch_sub(1)
            NameTable-->>SharedMem: 返回 0 (失败)
        end
    end
    
    SharedMem-->>Logger: 返回 ID
    Logger-->>Process: 返回 ID
```

### NameIdMapping 结构 / NameIdMapping Structure

```mermaid
flowchart TB
    subgraph NameIdMapping["NameIdMapping 结构"]
        ID["id: uint32_t<br/>(1-based, 0 表示无效)"]
        Name["name: char[32]<br/>(最大 31 字符 + null)"]
    end
    
    subgraph ProcessTable["进程名称表"]
        P0["[0] id=1, name='MainApp'"]
        P1["[1] id=2, name='Worker1'"]
        P2["[2] id=3, name='Worker2'"]
        P3["[3-63] 未使用"]
    end
    
    subgraph ThreadTable["线程/模块名称表"]
        T0["[0] id=1, name='Logger'"]
        T1["[1] id=2, name='Network'"]
        T2["[2-255] 未使用"]
    end
```

### 名称查找流程 / Name Lookup Flow

```mermaid
flowchart TB
    Start["GetProcessName(id)"]
    
    Check1{"id == 0 或<br/>id > kMaxProcesses?"}
    Check2{"index >= processCount?"}
    
    Return1["返回 nullptr"]
    Return2["返回 processes[index].name"]
    
    Start --> Check1
    Check1 -->|是| Return1
    Check1 -->|否| Check2
    Check2 -->|是| Return1
    Check2 -->|否| Return2
    
    Note1["index = id - 1<br/>(ID 是 1-based)"]
```

## SharedLoggerConfig 的配置同步方式 / Configuration Synchronization

### 配置结构 / Configuration Structure

```mermaid
flowchart TB
    subgraph SharedLoggerConfig["SharedLoggerConfig"]
        Level["logLevel: atomic<Level><br/>(缓存行对齐)"]
        Version["configVersion: atomic<uint32_t><br/>(缓存行对齐)"]
    end
    
    subgraph Operations["操作"]
        GetLevel["GetLevel()<br/>logLevel.load(acquire)"]
        SetLevel["SetLevel(level)<br/>logLevel.store(release)<br/>configVersion++"]
        GetVersion["GetVersion()<br/>configVersion.load(acquire)"]
    end
    
    SharedLoggerConfig --> Operations
```

### 配置同步流程 / Configuration Sync Flow

```mermaid
sequenceDiagram
    participant Owner as 所有者进程
    participant Config as SharedLoggerConfig
    participant Client1 as 客户端进程 1
    participant Client2 as 客户端进程 2

    Note over Owner,Client2: 初始配置
    Owner->>Config: Init() - logLevel=Info, version=0
    
    Note over Owner,Client2: 配置变更
    Owner->>Config: SetLevel(Level::Debug)
    Config->>Config: logLevel.store(Debug, release)
    Config->>Config: configVersion.fetch_add(1)
    
    Note over Owner,Client2: 客户端读取
    Client1->>Config: GetLevel()
    Config-->>Client1: Level::Debug
    
    Client2->>Config: GetVersion()
    Config-->>Client2: 1
    
    Note over Client2: 检测到版本变化，重新读取配置
    Client2->>Config: GetLevel()
    Config-->>Client2: Level::Debug
```

### 配置版本检测 / Configuration Version Detection

```mermaid
flowchart TB
    subgraph Client["客户端进程"]
        Cache["本地缓存<br/>cachedVersion = 0<br/>cachedLevel = Info"]
        
        Check["检查版本"]
        Compare{"currentVersion ><br/>cachedVersion?"}
        
        UseCache["使用缓存的配置"]
        Refresh["刷新配置<br/>cachedLevel = GetLevel()<br/>cachedVersion = currentVersion"]
    end
    
    subgraph Shared["共享内存"]
        Config["SharedLoggerConfig<br/>version=1, level=Debug"]
    end
    
    Check --> Compare
    Compare -->|否| UseCache
    Compare -->|是| Refresh
    Refresh --> Config
```

## PipelineThread 工作机制 / PipelineThread Mechanism

### PipelineThread 职责 / PipelineThread Responsibilities

```mermaid
flowchart TB
    subgraph Responsibilities["PipelineThread 职责"]
        R1["从 HeapRingBuffer 读取日志"]
        R2["转换指针数据为内联数据"]
        R3["添加进程 ID"]
        R4["写入 SharedRingBuffer"]
        R5["通知消费者"]
    end
    
    R1 --> R2 --> R3 --> R4 --> R5
```

### PipelineThread 主循环 / PipelineThread Main Loop

```mermaid
flowchart TB
    Start["ThreadFunc() 开始"]
    
    CheckRunning{"m_running?"}
    
    TryPop["HeapRingBuffer.TryPop(entry)"]
    HasData{"有数据?"}
    
    Process["ProcessEntry(entry)"]
    Convert["ConvertPointers(entry)"]
    AddPID["AddProcessId(entry)"]
    Push["SharedRingBuffer.TryPush(entry)"]
    Notify["NotifyConsumer()"]
    
    Wait["WaitForData(pollInterval, pollTimeout)"]
    
    Drain["排空剩余日志"]
    End["结束"]
    
    Start --> CheckRunning
    CheckRunning -->|是| TryPop
    CheckRunning -->|否| Drain
    
    TryPop --> HasData
    HasData -->|是| Process
    Process --> Convert --> AddPID --> Push --> Notify
    Notify --> TryPop
    
    HasData -->|否| Wait
    Wait --> CheckRunning
    
    Drain --> End
```

## WFC 在多进程模式下的行为 / WFC Behavior in MProc Mode

### WFC 限制说明 / WFC Limitations

在多进程模式下，WFC 的等待范围仅限于 HeapRingBuffer，不会等待 SharedRingBuffer 或最终 Sink 写入完成。

In MProc mode, WFC waiting scope is limited to HeapRingBuffer only, it does NOT wait for SharedRingBuffer or final Sink write completion.

```mermaid
flowchart LR
    subgraph WFCScope["WFC 等待范围"]
        direction TB
        HeapBuf["HeapRingBuffer<br/>✓ WFC 等待到这里"]
    end
    
    subgraph NoWFC["WFC 不等待"]
        direction TB
        Pipeline["PipelineThread"]
        SharedBuf["SharedRingBuffer"]
        Consumer["消费者进程"]
        Sink["Sink"]
    end
    
    HeapBuf --> Pipeline --> SharedBuf --> Consumer --> Sink
    
    style WFCScope fill:#c8e6c9
    style NoWFC fill:#ffcdd2
```

### WFC 多进程模式流程 / WFC MProc Mode Flow

```mermaid
sequenceDiagram
    participant User as 用户代码
    participant Logger as LoggerImpl
    participant HeapBuf as HeapRingBuffer
    participant Pipeline as PipelineThread
    participant SharedBuf as SharedRingBuffer
    participant Consumer as 消费者进程

    User->>Logger: InfoWFC("critical message")
    Logger->>HeapBuf: TryPushWFC(entry)
    HeapBuf->>HeapBuf: EnableWFC()
    HeapBuf-->>Logger: slot
    
    Logger->>HeapBuf: WaitForCompletion(slot)
    Note over Logger: 等待 HeapRingBuffer 处理...
    
    Pipeline->>HeapBuf: TryPop(entry)
    Pipeline->>HeapBuf: (隐式完成 WFC)
    
    HeapBuf-->>Logger: WFC 完成
    Logger-->>User: 返回
    
    Note over User: 此时日志可能还未到达 SharedRingBuffer
    
    Pipeline->>Pipeline: ConvertPointers()
    Pipeline->>SharedBuf: TryPush(entry)
    
    Note over Consumer: 稍后消费者处理...
    Consumer->>SharedBuf: TryPop(entry)
    Consumer->>Consumer: WriteToSink()
```

## 错误处理和回退机制 / Error Handling and Fallback

### 共享内存创建失败处理 / Shared Memory Creation Failure Handling

```mermaid
flowchart TB
    Start["InitMProc()"]
    
    Create["SharedMemory::Create()"]
    CreateOK{"创建成功?"}
    
    Connect["SharedMemory::Connect()"]
    ConnectOK{"连接成功?"}
    
    Fallback["回退到异步模式<br/>InitAsync()"]
    Warning["输出警告信息"]
    
    OwnerInit["作为所有者初始化"]
    ClientInit["作为客户端初始化"]
    
    End["初始化完成"]
    
    Start --> Create
    Create --> CreateOK
    
    CreateOK -->|是| OwnerInit --> End
    CreateOK -->|否| Connect
    
    Connect --> ConnectOK
    ConnectOK -->|是| ClientInit --> End
    ConnectOK -->|否| Warning --> Fallback --> End
    
    style Fallback fill:#ffcdd2
    style Warning fill:#fff9c4
```

## 性能特点 / Performance Characteristics

| 特性 / Feature | 多进程模式 / MProc Mode |
|----------------|------------------------|
| 跨进程通信 / IPC | 共享内存（最快）/ Shared memory (fastest) |
| 延迟 / Latency | 中等（两级队列）/ Medium (two-level queue) |
| 吞吐量 / Throughput | 高（批量传输）/ High (batch transfer) |
| 内存使用 / Memory | 共享内存 + 进程内队列 / Shared + per-process |
| 进程数限制 / Process Limit | 64 个进程 / 64 processes |
| 模块数限制 / Module Limit | 256 个模块 / 256 modules |
| 适用场景 / Use Case | 微服务、多进程应用 / Microservices, multi-process apps |

## 相关需求 / Related Requirements

- 需求 3.1: 创建多进程模式的控制流程图
- 需求 3.2: 创建多进程模式的数据流程图
- 需求 3.3: 标注共享内存的创建、附加和同步机制
- 需求 3.4: 说明 ProcessThreadNameTable 的名称注册和查找机制
- 需求 3.5: 说明 SharedLoggerConfig 的配置同步方式
- 需求 3.6: 使用 Mermaid 格式创建可渲染的流程图
