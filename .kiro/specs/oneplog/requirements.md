# 需求文档

## 简介

onePlog 是一个高性能 C++17 多进程日志系统，支持同步、异步和多进程三种操作模式。系统设计目标是提供零拷贝、低延迟的日志记录能力，同时支持灵活的日志格式化和多种输出目标。

## 术语表

- **Logger**: 日志记录器，负责管理日志级别和日志条目的创建
- **BinarySnapshot**: 二进制快照，用于零拷贝捕获日志参数
- **LogEntry**: 日志条目，包含时间戳、级别、位置信息和消息内容
- **HeapRingBuffer**: 堆上无锁环形队列，用于异步模式下的日志条目传输
- **SharedRingBuffer**: 共享内存环形队列，用于多进程模式下的日志传输
- **PipelineThread**: 管道线程，负责从 HeapRingBuffer 读取日志，转换指针数据并添加进程 ID
- **WriterThread**: 输出线程，负责将格式化后的日志写入目标
- **Format**: 格式化器，将日志条目转换为字符串
- **Sink**: 输出目标，如控制台、文件、网络等
- **同步模式**: 日志直接在调用线程中格式化和输出
- **异步模式**: 日志通过 HeapRingBuffer 传递给 WriterThread 处理
- **多进程模式**: 多个进程通过 HeapRingBuffer → PipelineThread → SharedRingBuffer → WriterThread 写入日志
- **WFC**: Wait For Completion，等待日志完成写入后返回

## 需求

### 需求 1：日志级别管理

**用户故事：** 作为开发者，我希望能够设置和过滤日志级别，以便控制日志输出的详细程度。

#### 验收标准

1. THE Logger SHALL 支持 Trace、Debug、Info、Warn、Error、Critical、Off 七个日志级别（参照 spdlog）
2. THE Logger SHALL 支持日志级别的短名称格式化（1字符：T/D/I/W/E/C，4字符：TRAC/DBUG/INFO/WARN/ERRO/CRIT/OFF）
3. WHEN 日志级别低于当前设置级别时，THE Logger SHALL 跳过该日志的处理
4. THE Logger SHALL 支持运行时动态修改日志级别
5. THE 对外接口 SHALL 参照 spdlog 的设计模式

### 需求 2：同步日志模式

**用户故事：** 作为开发者，我希望在简单场景下使用同步日志模式，以便获得最简单的日志记录方式。

#### 验收标准

1. WHEN 使用同步模式时，THE Logger SHALL 在调用线程中直接完成格式化和输出
2. THE 同步模式 SHALL 保证日志的顺序与调用顺序一致
3. WHEN 编译时定义 ONEPLOG_SYNC_ONLY 宏时，THE System SHALL 仅编译同步模式代码

### 需求 3：异步日志模式

**用户故事：** 作为开发者，我希望使用异步日志模式，以便减少日志记录对主线程的性能影响。

#### 验收标准

1. WHEN 使用异步模式时，THE Source线程 SHALL 将日志条目二进制化后写入 HeapRingBuffer（堆上预分配的无锁环形队列）
2. THE WriterThread SHALL 从 HeapRingBuffer 读取日志条目并进行格式化和输出
3. THE HeapRingBuffer SHALL 采用抢占索引再写入的方式实现无锁操作
4. WHEN 队列满时，THE Logger SHALL 根据配置选择丢弃或阻塞等待
5. THE 异步模式 SHALL 支持 WFC（Wait For Completion）日志，通过监控 HeapRingBuffer 状态位实现
6. THE System SHALL 使用 EventFD 通知 WriterThread 有新数据

### 需求 4：多进程日志模式

**用户故事：** 作为开发者，我希望多个进程能够共享同一个日志输出，以便统一管理分布式系统的日志。

#### 验收标准

1. THE 多进程模式 SHALL 采用 Source → HeapRingBuffer → Pipeline → SharedRingBuffer → Writer 的数据流
2. THE Source线程 SHALL 将日志条目二进制化后写入 HeapRingBuffer
3. THE PipelineThread SHALL 从 HeapRingBuffer 读取数据，将指针数据转换为实际数据，并添加进程 ID
4. THE PipelineThread SHALL 将转换后的数据写入 SharedRingBuffer（共享内存队列）
5. WHEN 多个进程写入日志时，THE SharedRingBuffer SHALL 保证无数据竞争
6. THE 主进程 SHALL 负责创建和管理共享内存，子进程通过 InitProducer 连接
7. THE System SHALL 对 fork 子进程使用 EventFD 通知，对非 fork 子进程使用 UDS 通知
8. THE 多进程模式 SHALL 支持 WFC 日志，Pipeline 转发后阻塞直到 Writer 完成

### 需求 5：二进制快照

**用户故事：** 作为开发者，我希望日志系统能够零拷贝地捕获日志参数，以便获得最佳性能。

#### 验收标准

1. THE BinarySnapshot SHALL 使用 C++17 变参模板将日志参数按二进制布局存入定长缓冲区（默认 256 字节）
2. THE BinarySnapshot SHALL 支持捕获基本类型（int32_t, int64_t, uint32_t, uint64_t, float, double, bool）
3. THE BinarySnapshot SHALL 对静态字符串（string_view/字面量）采用零拷贝策略，仅存储指针和长度
4. THE BinarySnapshot SHALL 对动态字符串（std::string/char*）采用内联拷贝策略，将内容复制到缓冲区
5. THE BinarySnapshot SHALL 使用类型标签（TypeTag）实现类型擦除，无需 RTTI 或虚函数
6. THE BinarySnapshot SHALL 支持序列化和反序列化操作（用于跨进程传输）
7. FOR ALL 有效的 BinarySnapshot 对象，序列化后反序列化 SHALL 产生等价的对象（往返一致性）

### 需求 6：日志条目

**用户故事：** 作为开发者，我希望日志条目包含完整的上下文信息，以便于问题定位和分析。

#### 验收标准

1. THE LogEntry SHALL 包含纳秒级时间戳
2. THE LogEntry SHALL 包含日志级别
3. THE LogEntry SHALL 包含源文件名、行号和函数名
4. THE LogEntry SHALL 包含线程 ID
5. THE LogEntry SHALL 包含进程 ID（多进程模式）
6. THE LogEntry SHALL 包含格式化字符串和参数快照

### 需求 7：无锁队列

**用户故事：** 作为开发者，我希望日志队列是无锁的，以便在高并发场景下获得最佳性能。

#### 验收标准

1. THE HeapRingBuffer SHALL 实现无锁环形队列，采用抢占索引再写入的方式
2. THE HeapRingBuffer SHALL 支持多生产者单消费者（MPSC）模式
3. THE 控制用原子变量 SHALL 独占缓存行以防止伪共享
4. THE System SHALL 在编译时自动识别系统 cacheline 大小
5. WHEN 队列为空时，THE Consumer SHALL 先轮询等待（默认 10ms/1us），超时后进入等待通知状态
6. WHEN 队列满时，THE Producer SHALL 能够检测并处理
7. THE RingBuffer SHALL 支持批量入队和出队操作
8. FOR ALL 入队的元素，出队顺序 SHALL 保持 FIFO 顺序

### 需求 8：日志格式化

**用户故事：** 作为开发者，我希望能够自定义日志格式，以便满足不同的日志分析需求。

#### 验收标准

1. THE Format SHALL 支持模式字符串定义输出格式
2. THE Format SHALL 支持时间戳格式化（可配置精度）
3. THE Format SHALL 支持日志级别格式化（全称/简称）
4. THE Format SHALL 支持源位置格式化（文件名/行号/函数名）
5. THE Format SHALL 支持自定义分隔符和前缀后缀
6. THE Format SHALL 支持 JSON 格式输出
7. FOR ALL 有效的 LogEntry，格式化操作 SHALL 产生非空字符串

### 需求 9：日志输出目标

**用户故事：** 作为开发者，我希望日志能够输出到多种目标，以便灵活配置日志存储方式。

#### 验收标准

1. THE Sink SHALL 支持控制台输出（stdout/stderr）
2. THE Sink SHALL 支持文件输出
3. THE Sink SHALL 支持文件轮转（按大小/按时间）
4. THE Sink SHALL 支持网络输出（TCP/UDP）
5. THE Sink SHALL 支持自定义输出目标扩展
6. WHEN 输出失败时，THE Sink SHALL 记录错误并尝试恢复

### 需求 10：日志 API

**用户故事：** 作为开发者，我希望有简洁易用的日志 API，以便快速集成到项目中。

#### 验收标准

1. THE API SHALL 提供宏定义简化日志调用（ONEPLOG_TRACE、ONEPLOG_DEBUG、ONEPLOG_INFO、ONEPLOG_WARN、ONEPLOG_ERROR、ONEPLOG_CRITICAL）
2. THE API SHALL 提供全局便捷函数（oneplog::Trace、oneplog::Debug、oneplog::Info、oneplog::Warn、oneplog::Error、oneplog::Critical）
3. THE API SHALL 提供 WFC 版本（oneplog::InfoWFC 等，等待日志完成后返回）
4. THE API SHALL 支持条件日志（仅在条件为真时记录）
5. WHEN 日志级别被禁用时，THE API SHALL 在编译时消除日志调用开销

### 需求 11：内存管理

**用户故事：** 作为开发者，我希望日志系统有高效的内存管理，以便减少内存分配开销。

#### 验收标准

1. THE System SHALL 使用内存池管理 LogEntry 和 BinarySnapshot
2. THE 内存池 SHALL 支持预分配固定大小的内存块
3. WHEN 内存池耗尽时，THE System SHALL 根据配置选择分配新内存或阻塞等待
4. FOR ALL 从内存池分配的对象，释放后 SHALL 能够被重用

### 需求 12：线程安全

**用户故事：** 作为开发者，我希望日志系统是线程安全的，以便在多线程环境中安全使用。

#### 验收标准

1. THE Logger SHALL 支持多线程并发调用
2. THE HeapRingBuffer 和 SharedRingBuffer SHALL 保证多线程访问的原子性
3. THE System SHALL 尽量使用原子操作代替互斥锁
4. WHEN 多线程同时写入时，THE System SHALL 不产生数据竞争

### 需求 13：配置管理

**用户故事：** 作为开发者，我希望能够通过配置文件管理日志设置，以便无需重新编译即可调整日志行为。

#### 验收标准

1. THE System SHALL 支持通过代码配置日志参数
2. THE System SHALL 支持通过配置文件配置日志参数
3. THE System SHALL 支持运行时重新加载配置
4. WHEN 配置无效时，THE System SHALL 使用默认值并记录警告

### 需求 14：性能监控

**用户故事：** 作为开发者，我希望能够监控日志系统的性能指标，以便及时发现性能问题。

#### 验收标准

1. THE System SHALL 记录日志吞吐量（条/秒）
2. THE System SHALL 记录队列使用率
3. THE System SHALL 记录丢弃的日志数量
4. THE System SHALL 记录平均日志延迟
5. THE System SHALL 支持导出性能指标

### 需求 15：错误处理

**用户故事：** 作为开发者，我希望日志系统有完善的错误处理机制，以便在异常情况下保持稳定。

#### 验收标准

1. IF 队列满，THEN THE System SHALL 根据策略丢弃或阻塞
2. IF 文件写入失败，THEN THE System SHALL 尝试重新打开文件
3. IF 网络连接断开，THEN THE System SHALL 尝试重新连接
4. THE System SHALL 记录所有错误到备用日志

### 需求 16：优雅关闭

**用户故事：** 作为开发者，我希望日志系统能够优雅关闭，以便确保所有日志都被正确写入。

#### 验收标准

1. WHEN 关闭时，THE System SHALL 等待队列中的日志全部处理完成
2. WHEN 关闭时，THE System SHALL 刷新所有 Sink 的缓冲区
3. THE System SHALL 支持设置关闭超时时间
4. IF 关闭超时，THEN THE System SHALL 强制关闭并记录未处理的日志数量

### 需求 17：编译时优化

**用户故事：** 作为开发者，我希望日志系统支持编译时优化，以便在发布版本中获得最佳性能。

#### 验收标准

1. THE System SHALL 支持编译时禁用特定日志级别
2. THE System SHALL 支持编译时选择同步/异步模式
3. WHEN 日志被编译时禁用时，THE System SHALL 完全消除相关代码
4. THE System SHALL 使用 constexpr 和模板元编程优化性能
