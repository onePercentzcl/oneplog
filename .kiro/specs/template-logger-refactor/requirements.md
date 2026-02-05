# 需求文档

## 简介

本文档定义了 onePlog 高性能 C++17 多进程聚合日志系统的模板化重构需求。onePlog 旨在提供零拷贝、低延迟的日志记录能力，支持同步、异步和多进程三种运行模式。通过 C++17 模板参数在编译时确定运行模式、最小日志级别和 WFC（Wait For Completion）功能开关，实现零开销抽象。

核心设计目标：
- **零拷贝**：使用 BinarySnapshot 捕获参数，静态字符串仅存储指针
- **零分配**：使用预分配内存池，避免运行时堆分配
- **低延迟**：使用无锁环形队列，采用抢占索引再写入的方式
- **编译时优化**：禁用的功能不产生任何运行时开销

## 术语表

- **Logger**: 日志管理器，负责日志的记录、路由和生命周期管理
- **BinarySnapshot**: 二进制快照，用于捕获日志参数的零拷贝数据结构
- **LogEntry**: 日志条目，包含时间戳、级别、线程ID、进程ID和参数快照
- **HeapRingBuffer**: 堆上无锁环形队列，用于异步模式下的日志传输
- **SharedRingBuffer**: 共享内存环形队列，用于多进程模式下的跨进程日志传输
- **PipelineThread**: 管道线程，负责指针数据转换和跨进程传输
- **WriterThread**: 输出线程（消费者线程），负责日志格式化和写入目标
- **Sink**: 输出目标，如控制台、文件、网络等
- **Format**: 格式化器，负责将 LogEntry 转换为可读字符串
- **WFC**: Wait For Completion，等待日志写入完成后返回的机制
- **Registry**: 注册表，用于存储进程/线程名称与 ID 的映射关系
- **LogConfig**: 日志配置，包含通用配置和内存配置

## 需求

### 需求 1: 模板化 Logger 类

**用户故事:** 作为开发者，我希望通过模板参数在编译时指定 Logger 的运行模式、最小日志级别和 WFC 功能开关，以便实现零开销抽象。

#### 验收标准

1. Logger_Template 应接受三个模板参数：Mode（Sync/Async/MProc）、Level（最小日志级别）和 EnableWFC（布尔值）
2. 当调用的日志方法级别低于模板最小级别时，Logger_Template 应生成空代码，被编译器优化掉
3. 当调用的日志方法级别等于或高于模板最小级别时，Logger_Template 应根据指定模式处理日志条目
4. 当 EnableWFC 为 false 时，Logger_Template 应将 WFC 方法降级为普通日志调用，不产生任何 WFC 状态检查开销
5. Logger_Template 应提供常用配置的类型别名：SyncLogger、AsyncLogger、MProcLogger、DebugLogger、ReleaseLogger

### 需求 2: 日志级别管理

**用户故事:** 作为开发者，我希望系统支持多种日志级别，并能在编译时和运行时进行级别过滤，以便控制日志输出量。

#### 验收标准

1. Logger 应支持九个日志级别：Alert、Critical、Error、Warning、Notice、Informational、Debugging、Trace 和 Off
2. 当设置编译时最小级别时，Logger 应从编译后的二进制文件中排除所有低于该级别的日志调用
3. 当设置运行时级别时，Logger 应在运行时过滤低于该级别的日志调用
4. Logger 应提供三种级别格式化样式：Full（Alert/Critical/Error）、Short4（ALER/CRIT/ERRO）和 Short1（A/C/E）
5. Logger 应在 Debug 构建中使用 Debugging 作为默认最小级别，在 Release 构建中使用 Informational

### 需求 3: 同步模式 (Sync Mode)

**用户故事:** 作为开发者，我希望在同步模式下日志直接在调用线程中完成格式化和输出，以便获得最简单的日志行为。

#### 验收标准

1. 当 Mode 为 Sync 时，Logger 应在调用线程中直接格式化和写入日志
2. 当 Mode 为 Sync 时，Logger 不应创建任何后台线程或环形缓冲区
3. 当 Mode 为 Sync 时，Logger 应保证同一线程内的日志输出顺序与调用顺序一致
4. 当 Mode 为 Sync 且调用 WFC 方法时，Logger 应与普通日志方法行为相同（原生 WFC 支持）

### 需求 4: 异步模式 (Async Mode)

**用户故事:** 作为开发者，我希望在异步模式下日志通过无锁队列传输到后台消费者线程处理，以便降低日志调用的延迟。

#### 验收标准

1. 当 Mode 为 Async 时，Logger 应创建 HeapRingBuffer 和 WriterThread（消费者线程）
2. 当在 Async 模式下调用日志方法时，工作线程应将参数捕获到 BinarySnapshot 并推送到 HeapRingBuffer
3. 当工作线程推送日志后，工作线程应通知 WriterThread 有新数据可用
4. WriterThread 收到通知后应进入轮询状态，持续读取并处理日志，直到队列为空
5. 当队列为空时，WriterThread 应采用指数回避策略：先短暂轮询，逐步增加等待时间，最终进入等待通知状态
6. 当 HeapRingBuffer 满时，Logger 应根据配置的 QueueFullPolicy（Block/DropNewest/DropOldest）处理

### 需求 5: 多进程模式 (MProc Mode)

**用户故事:** 作为开发者，我希望在多进程模式下多个进程的日志能够聚合到一个消费者进程，以便实现集中式日志管理。

#### 验收标准

1. 当 Mode 为 MProc 时，Logger 应创建 HeapRingBuffer、SharedMemory、PipelineThread 和 WriterThread
2. 当工作线程推送日志到 HeapRingBuffer 后，工作线程应通知 PipelineThread 有新数据可用
3. PipelineThread 应从 HeapRingBuffer 读取，将指针数据转换为实际数据，添加进程 ID，并写入 SharedRingBuffer
4. 当 PipelineThread 写入 SharedRingBuffer 后，PipelineThread 应通知 WriterThread（消费者线程）有新数据可用
5. SharedMemory 应包含 SharedRingBuffer、Registry（进程/线程名称表）和 LogConfig（配置表）
6. 当子进程调用 InitProducer 时，Logger 应连接到现有的 SharedMemory
7. 当在 MProc 模式下启用 WFC 时，PipelineThread 应阻塞直到 WriterThread 完成日志输出
8. PipelineThread 和 WriterThread 应采用与异步模式相同的轮询+指数回避策略

### 需求 6: BinarySnapshot 参数捕获

**用户故事:** 作为开发者，我希望日志参数能够被高效捕获，静态字符串零拷贝，动态字符串内联拷贝，以便实现最小化的日志开销。

#### 验收标准

1. BinarySnapshot 应支持捕获基本类型：int32_t、int64_t、uint32_t、uint64_t、float、double、bool
2. 当捕获 string_view 或字符串字面量时，BinarySnapshot 应仅存储指针和长度（零拷贝）
3. 当捕获 std::string 或临时 const char* 时，BinarySnapshot 应将数据内联拷贝到缓冲区
4. BinarySnapshot 应使用 TypeTag 枚举标识每个捕获值的类型，无需 RTTI
5. BinarySnapshot 应支持序列化以进行跨进程传输
6. 对于所有有效的 BinarySnapshot 对象，序列化后反序列化应产生等价的对象（往返属性）
7. BinarySnapshot 应提供 ConvertPointersToData 方法供 PipelineThread 转换指针数据

### 需求 7: HeapRingBuffer 无锁队列

**用户故事:** 作为开发者，我希望异步模式使用高性能无锁环形队列，以便支持多生产者单消费者场景。

#### 验收标准

1. HeapRingBuffer 应实现 MPSC（多生产者单消费者）无锁队列
2. HeapRingBuffer 应使用缓存行对齐的原子变量以防止伪共享
3. HeapRingBuffer 应支持基于槽位的操作：AcquireSlot、CommitSlot
4. HeapRingBuffer 应支持 WFC 操作：TryPushWFC、MarkWFCComplete、WaitForCompletion
5. 对于所有推送操作，HeapRingBuffer 应为来自同一生产者的元素保持 FIFO 顺序
6. HeapRingBuffer 应使用 4 种槽位状态：Empty、Writing、Ready、Reading
7. HeapRingBuffer 应使用 3 种 WFC 状态：None、Enabled、Completed

### 需求 8: SharedRingBuffer 共享内存队列

**用户故事:** 作为开发者，我希望多进程模式使用共享内存队列，以便实现跨进程的日志传输。

#### 验收标准

1. SharedRingBuffer 应将数据存储在多个进程可访问的共享内存中
2. SharedRingBuffer 应与 HeapRingBuffer 共享核心的 RingBuffer 逻辑
3. SharedRingBuffer 应为主进程提供 Create 方法，为子进程提供 Connect 方法
4. SharedRingBuffer 应根据 slotSize 和 slotCount 计算所需的共享内存大小
5. SharedMemory 应在单个内存区域中管理 Header、LogConfig、Registry 和 RingBuffer

### 需求 9: LogEntry 日志条目

**用户故事:** 作为开发者，我希望日志条目包含必要的元数据，并在 Debug 和 Release 模式下有不同的内存布局，以便平衡调试信息和性能。

#### 验收标准

1. LogEntry 应包含：timestamp（纳秒）、threadId、processId、level 和 BinarySnapshot
2. 在 Debug 模式下，LogEntry 应额外包含：文件名、行号和函数名
3. LogEntry 应优化内存布局以避免不必要的填充
4. LogEntry 应使用编译时选择在 LogEntryDebug 和 LogEntryRelease 之间切换

### 需求 10: Format 格式化器

**用户故事:** 作为开发者，我希望能够自定义日志输出格式，支持模式字符串和 JSON 格式，以便满足不同的日志分析需求。

#### 验收标准

1. PatternFormat 应支持模式占位符：%t（时间戳）、%l（级别）、%f（文件）、%n（行号）、%F（函数）、%T（线程 ID）、%P（进程 ID）、%N（进程名）、%M（模块名）、%m（消息）
2. JsonFormat 应将日志条目输出为 JSON 对象，支持可配置的美化打印
3. Format 应支持可配置的级别名称样式（Full/Short4/Short1）
4. Format 应支持可配置的时间戳格式
5. 对于所有有效的 LogEntry 对象，Format 应产生非空的格式化输出

### 需求 11: Sink 输出目标

**用户故事:** 作为开发者，我希望能够配置多种输出目标，包括控制台、文件和网络，以便灵活地管理日志输出。

#### 验收标准

1. ConsoleSink 应支持输出到 stdout 或 stderr，可选颜色
2. FileSink 应支持基于大小和文件数量的文件轮转
3. NetworkSink 应支持 TCP 和 UDP 协议，具有重连功能
4. Sink 应支持在独立线程中运行以实现并行输出
5. Logger 应支持同时配置多个 Sink
6. Sink 应通过 Logger 的 SetSink() 和 AddSink() 方法进行配置和管理
7. 每个 Sink 必须绑定独立的 Format，通过 Sink 的模板参数或构造函数配置
8. 当 Sink 未绑定 Format 时，编译器应产生编译错误

### 需求 24: Sink-Format 编译时绑定检查

**用户故事:** 作为开发者，我希望在编译时确保所有 Sink 都正确绑定了 Format，以便在运行前发现配置错误。

#### 验收标准

1. Sink 应使用模板参数指定绑定的 Format 类型，实现编译时类型检查
2. 当 Logger 添加 Sink 时，编译器应验证 Sink 已绑定有效的 Format 类型
3. 当 Sink 的 Format 类型不兼容时，编译器应产生明确的错误信息
4. 系统应提供 SinkWithFormat<SinkType, FormatType> 模板类用于编译时绑定
5. 系统应支持使用 static_assert 在编译时验证 Format 配置的完整性
6. 当使用 AddSink() 添加未绑定 Format 的 Sink 时，编译应失败

### 需求 12: WFC (Wait For Completion) 机制

**用户故事:** 作为开发者，我希望能够等待关键日志写入完成后再继续执行，以便确保重要日志不丢失。

#### 验收标准

1. 当在 Sync 模式下调用 WFC 方法时，Logger 应与普通日志行为相同（原生支持）
2. 当在 Async 模式下调用 WFC 方法时，Logger 应等待 WriterThread 完成输出
3. 当在 MProc 模式下调用 WFC 方法时，Logger 应等待 PipelineThread 和 WriterThread 都完成
4. WFC 机制应使用槽位的 WFCState（None/Enabled/Completed）来检测完成状态
5. 当 EnableWFC 模板参数为 false 时，Logger 应将 WFC 方法降级为普通日志调用，零开销

### 需求 13: 进程名/模块名注册

**用户故事:** 作为开发者，我希望能够注册和查询进程名和模块名，以便在日志中标识日志来源。

#### 验收标准

1. Logger 应提供 RegisterProcessName 方法用于注册进程名称，该方法应在 Init 之后调用
2. Logger 应提供 RegisterModuleName 方法用于注册当前线程的模块名称，该方法应在 Init 之后调用
3. 在 Linux 平台，Registry 应使用 PID 作为直接索引存储进程名（直接映射模式），最大支持 kLinuxMaxPID 个进程
4. 在非 Linux 平台（macOS/Windows），Registry 应使用数组存储 PID/TID 与名称的映射关系
5. Registry 应支持最长 15 个字符的名称（16 字节含终止符），超长名称截断
6. 对于同一线程中的所有 RegisterModuleName/GetModuleName 调用，Registry 应返回相同的名称（往返属性）
7. 对于所有并发的注册和查询调用，Registry 应无数据竞争地完成

### 需求 14: 日志配置管理

**用户故事:** 作为开发者，我希望能够配置日志系统的各项参数，包括运行模式、日志级别和内存配置。

#### 验收标准

1. LogConfig 应包含 GeneralConfig（运行模式、日志级别、WFC 开关）
2. LogConfig 应包含 MemoryConfig（HeapRingBuffer 和 SharedRingBuffer 的 slotSize 和 slotCount）
3. LogConfig 应使用缓存行对齐以防止伪共享
4. 在多进程模式下，LogConfig 应存储在 SharedMemory 中供所有进程访问

### 需求 15: 内存池

**用户故事:** 作为开发者，我希望系统使用预分配内存池，以便避免运行时堆分配带来的延迟。

#### 验收标准

1. MemoryPool 应在初始化时预分配内存块
2. MemoryPool 应提供无锁的 Allocate 和 Deallocate 操作
3. 对于所有已分配的对象，MemoryPool 应在释放后重用内存
4. MemoryPool 应使用缓存行对齐的空闲列表以防止伪共享

### 需求 16: 跨平台支持

**用户故事:** 作为开发者，我希望日志库能够在多个平台上运行，支持 Linux、macOS、HarmonyOS 和 OpenHarmony。

#### 验收标准

1. Logger 应能在 Linux（x86_64、ARM64）和 macOS（ARM64）上编译和运行
2. Logger 应能在 HarmonyOS 和 OpenHarmony 上编译和运行
3. Logger 应使用 std::filesystem::path 进行跨平台路径处理
4. Logger 应在编译时使用 std::hardware_destructive_interference_size（如可用）自动检测缓存行大小，否则回退到 64 字节
5. Logger 应支持 Header-only、静态库和动态库三种构建模式
6. 在 Linux 平台应使用 eventfd 进行线程间通知
7. 在 macOS 平台应使用 GCD (Grand Central Dispatch) 进行线程间通知
8. 在 HarmonyOS/OpenHarmony 平台应使用适当的线程间通知机制（如 eventfd 或条件变量）

### 需求 17: 并发安全

**用户故事:** 作为开发者，我希望日志库在多线程和多进程环境下是安全的，不会产生数据竞争。

#### 验收标准

1. 对于来自多个线程的所有并发 Logger::Log 调用，Logger 应正确记录所有日志条目，无数据竞争
2. 对于所有并发写入 HeapRingBuffer 的操作，HeapRingBuffer 应保持数据完整性
3. 对于来自多个进程的所有并发写入 SharedRingBuffer 的操作，SharedRingBuffer 应保持数据完整性
4. Logger 应为无锁数据结构适当使用原子操作和内存屏障
5. RingBufferHeader 应实现冷热分离，将生产者和消费者拥有的变量物理隔离到不同缓存行

### 需求 18: 错误处理

**用户故事:** 作为开发者，我希望系统能够优雅地处理各种错误情况，不会因为日志系统的问题影响主程序运行。

#### 验收标准

1. 当 HeapRingBuffer 满且策略为 Block 时，Logger 应阻塞直到有可用空间
2. 当 HeapRingBuffer 满且策略为 DropNewest 时，Logger 应丢弃新日志并增加 droppedCount
3. 当 HeapRingBuffer 满且策略为 DropOldest 时，Logger 应丢弃最旧日志并增加 droppedCount
4. 当 Sink 写入失败时，Logger 应记录错误并继续处理其他日志
5. 当 MProc 模式下 SharedMemory 创建失败时，Logger 应回退到 Async 模式
6. 如果初始化失败，Logger 应使用默认值并记录警告
7. Logger 应使用 ErrorCode 枚举标识各类错误（内存错误、队列错误、文件错误、网络错误、共享内存错误、配置错误、线程错误）

### 需求 19: Logger 公共接口

**用户故事:** 作为开发者，我希望 Logger 提供简洁易用的公共接口，以便快速集成到项目中。

#### 验收标准

1. Logger 应提供 Init() 和 Init(const LoggerConfig&) 方法用于初始化
2. Logger 应提供 Shutdown() 方法用于关闭日志系统
3. Logger 应提供各级别的日志方法：Alert()、Critical()、Error()、Warning()、Notice()、Info()、Debug()、Trace()
4. Logger 应提供各级别的 WFC 日志方法：AlertWFC()、CriticalWFC()、ErrorWFC() 等
5. Logger 应提供 SetSink() 和 AddSink() 方法用于配置输出目标
6. Logger 应提供 SetFormat() 方法用于配置格式化器
7. Logger 应提供 Flush() 方法用于刷新缓冲区
8. Logger 应提供 Name() 方法返回 Logger 名称
9. Logger 应提供静态方法 GetMode()、GetMinLevel()、IsWFCEnabled() 返回编译时配置

### 需求 20: 全局便捷函数接口

**用户故事:** 作为开发者，我希望能够使用全局函数快速记录日志，无需显式创建 Logger 实例。

#### 验收标准

1. oneplog 命名空间应提供全局日志函数：oneplog::Alert()、oneplog::Critical()、oneplog::Error()、oneplog::Warning()、oneplog::Notice()、oneplog::Info()、oneplog::Debug()、oneplog::Trace()
2. oneplog 命名空间应提供全局 WFC 日志函数：oneplog::AlertWFC()、oneplog::CriticalWFC() 等
3. oneplog 命名空间应提供 Init() 和 InitProducer() 函数用于初始化
4. oneplog 命名空间应提供 RegisterProcessName() 和 RegisterModuleName() 函数用于注册名称
5. oneplog 命名空间应提供 SetLevel()、SetPattern()、Flush()、Shutdown() 等全局配置函数
6. 全局函数应操作默认 Logger 实例

### 需求 21: 日志宏接口

**用户故事:** 作为开发者，我希望能够使用宏来记录日志，以便自动捕获源代码位置信息。

#### 验收标准

1. 系统应提供各级别的日志宏：ONEPLOG_ALERT、ONEPLOG_CRITICAL、ONEPLOG_ERROR、ONEPLOG_WARNING、ONEPLOG_NOTICE、ONEPLOG_INFO、ONEPLOG_DEBUG、ONEPLOG_TRACE
2. 系统应提供各级别的 WFC 日志宏：ONEPLOG_ALERT_WFC、ONEPLOG_CRITICAL_WFC 等
3. 日志宏应自动捕获 __FILE__、__LINE__、__FUNCTION__ 信息
4. 系统应提供条件日志宏 ONEPLOG_IF(condition, level, ...)
5. 系统应支持通过编译宏禁用特定级别的日志：ONEPLOG_DISABLE_TRACE、ONEPLOG_DISABLE_DEBUG 等
6. 当日志级别被禁用时，对应的宏应展开为空操作 ((void)0)

### 需求 22: 多进程模式生产者接口

**用户故事:** 作为开发者，我希望子进程能够方便地连接到主进程的日志系统，以便实现多进程日志聚合。

#### 验收标准

1. oneplog 命名空间应提供 InitProducer(const std::string& shmName) 函数用于子进程初始化
2. InitProducer 应连接到指定名称的共享内存
3. InitProducer 应自动在 Registry 中注册当前进程
4. 子进程应能够使用与主进程相同的日志接口（全局函数和宏）
5. 子进程的日志应通过 SharedRingBuffer 传输到主进程的 WriterThread

### 需求 23: ALO (Always Log Output) 接口

**用户故事:** 作为开发者，我希望能够记录始终输出的日志，不受编译时级别限制，以便在关键场景下确保日志输出。

#### 验收标准

1. Logger 应提供 ALO 日志方法：AlertALO()、CriticalALO()、ErrorALO() 等
2. oneplog 命名空间应提供全局 ALO 函数：oneplog::AlertALO()、oneplog::CriticalALO() 等
3. 系统应提供 ALO 日志宏：ONEPLOG_ALERT_ALO、ONEPLOG_CRITICAL_ALO 等
4. ALO 日志应绕过编译时级别检查，始终输出
5. ALO 日志应绕过运行时级别检查，始终输出（不受任何级别限制）

### 需求 25: 默认 Logger 参数配置

**用户故事:** 作为开发者，我希望能够通过全局配置变量或 LoggerConfig 结构体配置默认 Logger 的参数，以便灵活地控制日志系统的行为。

#### 验收标准

1. LoggerConfig 结构体应包含完整的配置字段：mode、level、enableWFC、heapRingBufferSlotCount、heapRingBufferSlotSize、sharedRingBufferSlotCount、sharedRingBufferSlotSize、queueFullPolicy、sharedMemoryName、pollInterval、pollTimeout、processName、moduleName
2. LoggerConfig 应提供 Validate() 方法验证配置有效性（槽位数量为 2 的幂、槽位大小为 CacheLine 整数倍、MProc 模式必须指定共享内存名称）
3. LoggerConfig 应提供 CheckConflicts() 方法检查配置冲突
4. oneplog::config 命名空间应提供全局配置变量：mode、level、heapSlotCount、heapSlotSize、sharedSlotCount、sharedSlotSize、queueFullPolicy、sharedMemoryName、pollInterval、pollTimeout、processName、moduleName
5. oneplog::config 命名空间应提供 BuildConfig() 函数从全局变量构建 LoggerConfig
6. oneplog::Init() 应使用全局配置变量初始化默认 Logger
7. oneplog::Init(const LoggerConfig&) 应使用自定义配置初始化默认 Logger
8. oneplog::InitProducer(shmName) 应初始化多进程模式的生产者（子进程调用）
9. oneplog::InitProducer(shmName, processName) 应支持带进程名的生产者初始化
10. 当配置验证失败时，Init() 应使用默认配置并记录警告
