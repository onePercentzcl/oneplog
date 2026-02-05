# 实现任务

本文档定义了 onePlog 模板化重构的实现任务列表。

## 任务列表

### 阶段 1: 核心基础设施

- [x] 1. 实现基础类型和枚举
  - [x] 1.1 实现 Level 枚举（9 个级别：Alert 到 Off）
  - [x] 1.2 实现 Mode 枚举（Sync/Async/MProc）
  - [x] 1.3 实现 QueueFullPolicy 枚举（Block/DropNewest/DropOldest）
  - [x] 1.4 实现 SlotState 和 WFCState 枚举
  - [x] 1.5 实现 ErrorCode 枚举
  - [x] 1.6 实现 LevelNameStyle 枚举和 LevelToString 函数

- [x] 2. 实现 BinarySnapshot 类
  - [x] 2.1 实现基本类型捕获方法（int32_t, int64_t, float, double, bool）
  - [x] 2.2 实现字符串捕获方法（StringView 零拷贝，StringCopy 内联拷贝）
  - [x] 2.3 实现变参模板 Capture 方法
  - [x] 2.4 实现序列化和反序列化方法
  - [x] 2.5 实现 Format 方法
  - [x] 2.6 实现 ConvertPointersToData 方法
  - [x] 2.7 编写 BinarySnapshot 单元测试
  - [x] 2.8 编写 BinarySnapshot 属性测试（Property 6, 7）

- [x] 3. 实现 LogEntry 结构体
  - [x] 3.1 实现 SourceLocation 结构体
  - [x] 3.2 实现 LogEntryDebug 结构体
  - [x] 3.3 实现 LogEntryRelease 结构体
  - [x] 3.4 实现编译时选择（NDEBUG 宏）
  - [x] 3.5 编写 LogEntry 单元测试

### 阶段 2: 环形缓冲区

- [ ] 4. 实现 RingBuffer 核心逻辑
  - [ ] 4.1 实现 Slot 模板结构体（缓存行对齐）
  - [ ] 4.2 实现 RingBufferHeader 结构体（冷热分离）
  - [ ] 4.3 实现 RingBuffer 模板类
  - [ ] 4.4 实现 AcquireSlot 和 CommitSlot 方法
  - [ ] 4.5 实现 TryPush 和 TryPop 方法
  - [ ] 4.6 实现队列满策略处理
  - [ ] 4.7 编写 RingBuffer 单元测试
  - [ ] 4.8 编写 RingBuffer 属性测试（Property 8, 9）

- [ ] 5. 实现 HeapRingBuffer
  - [ ] 5.1 基于 RingBuffer 实现 HeapRingBuffer
  - [ ] 5.2 实现堆内存分配和管理
  - [ ] 5.3 编写 HeapRingBuffer 单元测试

- [ ] 6. 实现 WFC 支持
  - [ ] 6.1 实现 TryPushWFC 方法
  - [ ] 6.2 实现 MarkWFCComplete 方法
  - [ ] 6.3 实现 WaitForCompletion 方法
  - [ ] 6.4 编写 WFC 单元测试
  - [ ] 6.5 编写 WFC 属性测试（Property 19）

### 阶段 3: 格式化和输出

- [ ] 7. 实现 Format 基类和派生类
  - [ ] 7.1 实现 Format 抽象基类
  - [ ] 7.2 实现 PatternFormat 类（模式占位符解析）
  - [ ] 7.3 实现 JsonFormat 类
  - [ ] 7.4 编写 Format 单元测试
  - [ ] 7.5 编写 Format 属性测试（Property 11, 12, 13）

- [ ] 8. 实现 Sink 模板类
  - [ ] 8.1 实现 IsValidFormat 类型特征
  - [ ] 8.2 实现 SinkBase 模板基类
  - [ ] 8.3 实现 ConsoleSink 模板类
  - [ ] 8.4 实现 FileSink 模板类（含文件轮转）
  - [ ] 8.5 实现 NetworkSink 模板类
  - [ ] 8.6 实现 ISink 接口和 SinkWrapper
  - [ ] 8.7 实现便捷类型别名和工厂函数
  - [ ] 8.8 编写 Sink 单元测试
  - [ ] 8.9 编写 Sink 属性测试（Property 14, 23, 24）

### 阶段 4: 线程管理

- [ ] 9. 实现跨平台通知机制
  - [ ] 9.1 实现 Notifier 抽象基类
  - [ ] 9.2 实现 Linux EventFDNotifier
  - [ ] 9.3 实现 macOS GCDNotifier
  - [ ] 9.4 编写通知机制单元测试

- [ ] 10. 实现 WriterThread
  - [ ] 10.1 实现 WriterThread 类
  - [ ] 10.2 实现指数回避策略
  - [ ] 10.3 实现 Flush 方法
  - [ ] 10.4 编写 WriterThread 单元测试

- [ ] 11. 实现 PipelineThread
  - [ ] 11.1 实现 PipelineThread 类
  - [ ] 11.2 实现指针数据转换
  - [ ] 11.3 实现进程 ID 添加
  - [ ] 11.4 编写 PipelineThread 单元测试
  - [ ] 11.5 编写 Pipeline 属性测试（Property 22）

### 阶段 5: 名称管理和配置

- [ ] 12. 实现 Registry
  - [ ] 12.1 实现 Linux 平台直接映射模式
  - [ ] 12.2 实现非 Linux 平台数组映射模式
  - [ ] 12.3 实现名称注册和查询方法
  - [ ] 12.4 编写 Registry 单元测试
  - [ ] 12.5 编写 Registry 属性测试（Property 15, 16）

- [ ] 13. 实现 LogConfig
  - [ ] 13.1 实现 GeneralConfig 结构体
  - [ ] 13.2 实现 MemoryConfig 结构体
  - [ ] 13.3 实现 LogConfig 结构体
  - [ ] 13.4 编写 LogConfig 单元测试

- [ ] 14. 实现 LoggerConfig 和全局配置
  - [ ] 14.1 实现 LoggerConfig 结构体（完整版）
  - [ ] 14.2 实现 Validate() 方法
  - [ ] 14.3 实现 CheckConflicts() 方法
  - [ ] 14.4 实现 oneplog::config 命名空间全局变量
  - [ ] 14.5 实现 BuildConfig() 函数
  - [ ] 14.6 编写 LoggerConfig 单元测试
  - [ ] 14.7 编写 LoggerConfig 属性测试（Property 25, 26, 27）

### 阶段 6: 共享内存（多进程模式）

- [ ] 15. 实现 SharedMemory
  - [ ] 15.1 实现 ShmHeader 结构体
  - [ ] 15.2 实现 SharedMemory 类
  - [ ] 15.3 实现 Create 和 Connect 方法
  - [ ] 15.4 编写 SharedMemory 单元测试

- [ ] 16. 实现 SharedRingBuffer
  - [ ] 16.1 基于 RingBuffer 实现 SharedRingBuffer
  - [ ] 16.2 实现共享内存布局计算
  - [ ] 16.3 编写 SharedRingBuffer 单元测试
  - [ ] 16.4 编写 SharedRingBuffer 属性测试（Property 10, 18）

### 阶段 7: Logger 模板类

- [ ] 17. 实现 Logger 模板类核心
  - [ ] 17.1 实现 Logger 模板类声明
  - [ ] 17.2 实现构造函数和析构函数
  - [ ] 17.3 实现 Init() 和 Init(LoggerConfig) 方法
  - [ ] 17.4 实现 Shutdown() 方法
  - [ ] 17.5 实现 SetSink() 和 AddSink() 方法（编译时验证）

- [ ] 18. 实现日志方法
  - [ ] 18.1 实现 LogImpl 模板方法
  - [ ] 18.2 实现各级别日志方法（Alert 到 Trace）
  - [ ] 18.3 实现编译时级别过滤（if constexpr）
  - [ ] 18.4 实现 ProcessEntrySyncDirect 方法
  - [ ] 18.5 实现 ProcessEntryAsync 方法
  - [ ] 18.6 编写日志方法单元测试
  - [ ] 18.7 编写日志方法属性测试（Property 1, 2, 3, 4）

- [ ] 19. 实现 WFC 日志方法
  - [ ] 19.1 实现 LogWFCImpl 模板方法
  - [ ] 19.2 实现各级别 WFC 方法
  - [ ] 19.3 实现 WFC 禁用时的降级行为
  - [ ] 19.4 编写 WFC 方法单元测试
  - [ ] 19.5 编写 WFC 方法属性测试（Property 5）

- [ ] 20. 实现 OUT 日志方法
  - [ ] 20.1 实现各级别 OUT 方法
  - [ ] 20.2 编写 OUT 方法单元测试
  - [ ] 20.3 编写 OUT 方法属性测试（Property 21）

- [ ] 21. 实现类型别名
  - [ ] 21.1 实现 SyncLogger、AsyncLogger、MProcLogger 别名
  - [ ] 21.2 实现 DebugLogger、ReleaseLogger 别名
  - [ ] 21.3 编写类型别名单元测试

### 阶段 8: 全局接口

- [ ] 22. 实现全局便捷函数
  - [ ] 22.1 实现 DefaultLogger() 函数
  - [ ] 22.2 实现 Init() 和 Init(LoggerConfig) 全局函数
  - [ ] 22.3 实现 InitProducer() 函数
  - [ ] 22.4 实现 Shutdown() 和 Flush() 全局函数
  - [ ] 22.5 实现各级别全局日志函数
  - [ ] 22.6 实现各级别全局 WFC 函数
  - [ ] 22.7 实现各级别全局 OUT 函数
  - [ ] 22.8 实现 RegisterProcessName() 和 RegisterModuleName() 函数
  - [ ] 22.9 编写全局函数单元测试

- [ ] 23. 实现日志宏
  - [ ] 23.1 实现 ONEPLOG_CURRENT_LOCATION 宏
  - [ ] 23.2 实现各级别日志宏
  - [ ] 23.3 实现各级别 WFC 宏
  - [ ] 23.4 实现各级别 OUT 宏
  - [ ] 23.5 实现 ONEPLOG_IF 条件宏
  - [ ] 23.6 实现编译时禁用宏
  - [ ] 23.7 编写日志宏单元测试

### 阶段 9: 并发测试和集成

- [ ] 24. 并发安全测试
  - [ ] 24.1 编写多线程并发写入测试
  - [ ] 24.2 编写多进程并发写入测试
  - [ ] 24.3 编写并发属性测试（Property 17, 18）

- [ ] 25. 集成测试
  - [ ] 25.1 编写同步模式集成测试
  - [ ] 25.2 编写异步模式集成测试
  - [ ] 25.3 编写多进程模式集成测试
  - [ ] 25.4 编写模式切换测试

### 阶段 10: 文档和示例

- [ ] 26. 更新文档
  - [ ] 26.1 更新 README.md（中文版）
  - [ ] 26.2 更新 README_ENGLISH.md（英文版）
  - [ ] 26.3 添加 API 文档（Doxygen）

- [ ] 27. 编写示例代码
  - [ ] 27.1 编写基本使用示例
  - [ ] 27.2 编写全局配置示例
  - [ ] 27.3 编写 LoggerConfig 配置示例
  - [ ] 27.4 编写多进程模式示例
  - [ ] 27.5 编写自定义 Sink 示例

## 依赖关系

```
阶段 1 (基础) → 阶段 2 (缓冲区) → 阶段 4 (线程)
                    ↓
阶段 3 (格式化) → 阶段 7 (Logger) → 阶段 8 (全局接口)
                    ↓
阶段 5 (名称/配置) → 阶段 6 (共享内存)
                    ↓
              阶段 9 (测试) → 阶段 10 (文档)
```

## 优先级说明

- **P0 (必须)**: 阶段 1-3, 7-8（核心功能）
- **P1 (重要)**: 阶段 4-5, 9（线程和测试）
- **P2 (可选)**: 阶段 6, 10（多进程和文档）
