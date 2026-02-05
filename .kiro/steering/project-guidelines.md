---
inclusion: always
---

# onePlog 项目开发规范

## 核心原则

**使用中文进行所有交流和沟通**。代码注释必须使用中英双语，确保国际化和本地化的平衡。

## 项目架构

### 基本信息
- **项目**: onePlog - 高性能 C++17 多进程聚合日志系统
- **标准**: C++17（严格遵守，不使用 C++20 特性）
- **构建系统**: CMake（主要）和 XMake（辅助）
- **命名空间**: `oneplog`（公共API）/ `oneplog::internal`（内部实现）
- **库模式**: 支持三种构建模式
  - Header-Only（默认，通过 `ONEPLOG_HEADER_ONLY=ON`）
  - 静态库（`.a`/`.lib`）
  - 动态库（`.so`/`.dylib`/`.dll`）

### 设计理念
1. **零拷贝**: 使用 BinarySnapshot 捕获参数，静态字符串仅存储指针
2. **零分配**: 使用预分配内存池，避免运行时堆分配
3. **低延迟**: 使用无锁环形队列，采用抢占索引再写入的方式
4. **编译时优化**: 通过模板参数在编译时确定运行模式、日志级别和 WFC 功能，禁用的功能不产生任何运行时开销

### 目录结构
```
oneplog/
├── include/oneplog/          # 公共头文件（API）
│   ├── common.hpp            # 通用定义（Level, Mode, ErrorCode）
│   ├── binary_snapshot.hpp   # 二进制快照
│   ├── ring_buffer.hpp       # 环形缓冲区
│   └── ...
├── src/                      # 实现文件（非 Header-Only 模式）
│   ├── binary_snapshot.cpp
│   └── ...
├── tests/                    # 测试代码
│   ├── *_test.cpp           # 单元测试
│   └── *_property_test.cpp  # 属性测试
├── example/                  # 示例代码
├── docs/                     # 文档
├── .kiro/                    # Kiro 配置
│   ├── specs/               # 功能规范
│   └── steering/            # 开发指南
├── CMakeLists.txt           # CMake 构建配置
├── xmake.lua                # XMake 构建配置
├── .clang-format            # 代码格式化配置
└── Doxyfile                 # API 文档生成配置
```

## 代码规范

### 命名约定

| 元素 | 规范 | 示例 | 说明 |
|------|------|------|------|
| 文件名 | snake_case | `binary_snapshot.cpp` | 小写+下划线 |
| 类名 | PascalCase | `BinarySnapshot` | 首字母大写 |
| 函数/方法 | PascalCase | `CaptureInt32()` | 首字母大写 |
| 局部变量 | camelCase | `argCount` | 首字母小写 |
| 成员变量 | m_camelCase | `m_buffer` | m_ 前缀 |
| 全局常量 | kPascalCase | `kDefaultBufferSize` | k 前缀 |
| 枚举类型 | PascalCase | `Level` | 首字母大写 |
| 枚举值 | PascalCase | `Level::Info` | 首字母大写 |
| 宏定义 | ALL_CAPS | `ONEPLOG_HEADER_ONLY` | 全大写+下划线 |
| 命名空间 | lowercase | `oneplog` | 全小写 |
| 模板参数 | PascalCase | `template<typename T>` | 首字母大写 |

### 代码组织原则

1. **头文件保护**: 使用 `#pragma once`（不使用传统的 include guards）
2. **包含顺序**: 
   ```cpp
   // 1. 对应的头文件（如果是 .cpp 文件）
   #include "oneplog/binary_snapshot.hpp"
   
   // 2. C++ 标准库
   #include <string>
   #include <vector>
   
   // 3. 第三方库
   #include <gtest/gtest.h>
   
   // 4. 项目内其他头文件
   #include "oneplog/common.hpp"
   ```

3. **命名空间**: 
   - 公共 API 使用 `namespace oneplog`
   - 内部实现使用 `namespace oneplog::internal`
   - 不要在头文件中使用 `using namespace`

4. **前向声明**: 优先使用前向声明减少编译依赖

### 注释规范

**必须使用中英双语 Doxygen 注释**：

```cpp
/**
 * @brief Capture int32_t value
 * @brief 捕获 int32_t 值
 * @param value The value to capture / 待捕获的值
 * @return bool True if successful / 如果成功则返回 true
 */
bool CaptureInt32(int32_t value);
```

**注释要求**：
- 所有公共 API 必须有完整的中英双语注释
- 复杂算法必须有详细的实现说明
- 性能关键代码必须注明设计考虑
- 使用 `@note` 标注重要注意事项
- 使用 `@warning` 标注潜在问题

### 类型和模板

1. **使用 C++17 特性**:
   - `std::string_view` 代替 `const std::string&`（零拷贝）
   - `std::optional` 表示可选值
   - `if constexpr` 实现编译时分支
   - `std::filesystem::path` 处理路径
   - 结构化绑定 `auto [a, b] = ...`

2. **模板编程**:
   - 使用 `typename` 而非 `class` 声明模板参数
   - 使用 SFINAE 或 `std::enable_if` 进行类型约束
   - 模板实现放在头文件中
   - 复杂模板使用 `using` 创建类型别名

3. **类型安全**:
   - 使用 `enum class` 而非 `enum`
   - 避免隐式类型转换
   - 使用 `static_cast` 进行显式转换
   - 使用 `constexpr` 标记编译时常量

### 内存管理

1. **智能指针优先**:
   - 使用 `std::unique_ptr` 表示独占所有权
   - 使用 `std::shared_ptr` 表示共享所有权
   - 避免裸指针（除非性能关键且有充分理由）

2. **RAII 原则**: 所有资源（内存、文件、锁）必须通过 RAII 管理

3. **对齐要求**: 
   - 使用 `alignas(kCacheLineSize)` 防止伪共享
   - 缓存行大小优先使用 `std::hardware_destructive_interference_size`
   - 回退值为 64 字节

### 并发安全

1. **原子操作**: 使用 `std::atomic` 实现无锁数据结构
2. **内存序**: 明确指定内存序（`memory_order_acquire`, `memory_order_release` 等）
3. **数据竞争**: 使用 ThreadSanitizer 检测数据竞争
4. **锁策略**: 
   - 优先无锁设计
   - 必要时使用 `std::mutex`
   - 避免死锁（锁顺序一致）

## 测试规范

### 测试框架
- **单元测试**: Google Test (gtest)
- **属性测试**: 使用 Google Test 的参数化测试模拟属性测试（每个属性至少 100 次迭代）

### 测试要求

1. **测试覆盖率**:
   - 核心组件（BinarySnapshot, RingBuffer）: 95%+ 行覆盖率，90%+ 分支覆盖率
   - 其他组件: 90%+ 行覆盖率，85%+ 分支覆盖率

2. **测试类型**:
   - **单元测试**: 测试特定功能、边界条件、错误处理
   - **属性测试**: 验证通用属性（如序列化往返一致性）
   - **集成测试**: 测试组件间交互
   - **并发测试**: 测试多线程/多进程场景

3. **测试命名**:
   ```cpp
   // 单元测试
   TEST(ComponentTest, SpecificBehavior)
   
   // 属性测试（必须包含 Property 标识和编号）
   TEST_P(ComponentPropertyTest, PropertyDescription)
   // 注释中必须标注: Feature: template-logger-refactor, Property X: 描述
   ```

4. **测试原则**:
   - 每个测试只验证一个行为
   - 测试必须可重复、独立
   - 使用 ASSERT_* 验证前置条件，EXPECT_* 验证结果
   - 失败时提供清晰的错误信息

### 属性测试规范

属性测试必须：
1. 在注释中明确标注 `Feature: template-logger-refactor, Property X: 属性描述`
2. 每个属性至少运行 100 次迭代
3. 使用随机数生成器（固定种子以保证可重复性）
4. 验证设计文档中定义的正确性属性

## 构建和部署

### CMake 配置

```cmake
# 构建选项
option(ONEPLOG_BUILD_TESTS "构建测试" OFF)
option(ONEPLOG_BUILD_EXAMPLES "构建示例" OFF)
option(ONEPLOG_HEADER_ONLY "Header-Only 模式" ON)

# 编译器要求
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
```

### 编译器支持
- GCC 7.0+
- Clang 5.0+
- MSVC 2017+
- Apple Clang 10.0+

### 平台支持
- Linux (x86_64, ARM64)
- macOS (ARM64, x86_64)
- HarmonyOS / OpenHarmony (ARM64)
- Windows (x86_64) - 未来支持

## 跨平台开发

### 路径处理
```cpp
// 正确：使用 std::filesystem::path
std::filesystem::path logPath = "/var/log/app.log";

// 错误：硬编码路径分隔符
std::string logPath = "/var/log/app.log";  // 在 Windows 上失败
```

### 平台特定代码
```cpp
#if defined(__linux__)
    // Linux 特定代码
#elif defined(__APPLE__)
    // macOS 特定代码
#elif defined(_WIN32)
    // Windows 特定代码
#endif
```

### 字节序处理
- 网络传输使用大端序（网络字节序）
- 使用 `htonl()`, `ntohl()` 等函数转换

## 性能优化

### 关键原则
1. **测量优先**: 使用 profiler 确认瓶颈后再优化
2. **零拷贝**: 使用 `std::string_view`, 指针传递
3. **内联**: 性能关键函数使用 `inline` 或模板
4. **分支预测**: 使用 `[[likely]]` / `[[unlikely]]` (C++20) 或编译器内建函数
5. **缓存友好**: 
   - 数据结构缓存行对齐
   - 热数据和冷数据分离
   - 顺序访问优于随机访问

### 禁止的操作
- 在热路径中使用异常
- 在热路径中使用虚函数（除非必要）
- 在热路径中进行堆分配
- 在热路径中使用 `std::cout`（使用日志系统）

## 错误处理

### 错误码优先
```cpp
// 推荐：返回错误码
ErrorCode DoSomething() {
    if (error) {
        return ErrorCode::InvalidArgument;
    }
    return ErrorCode::Success;
}

// 避免：在性能关键路径使用异常
void DoSomething() {
    if (error) {
        throw std::runtime_error("error");  // 性能开销大
    }
}
```

### 错误处理策略
1. **可恢复错误**: 返回错误码，记录日志
2. **不可恢复错误**: 使用 `assert` 或 `std::terminate`
3. **用户输入错误**: 验证并返回错误码
4. **系统错误**: 记录详细日志，尝试恢复或优雅降级

## 文档要求

### 代码文档
- 所有公共 API 必须有 Doxygen 注释
- 复杂算法必须有实现说明
- 性能关键代码必须注明设计考虑

### 项目文档
每次完成任务后必须更新：
1. `README.md` (中文版)
2. `README_EN.md` (英文版)
3. 相关的设计文档（`.kiro/specs/` 目录）

### 提交规范
```bash
# 提交格式
git commit -m "feat: 实现 BinarySnapshot 类

- 实现基本类型捕获方法
- 实现字符串捕获（零拷贝和内联拷贝）
- 实现序列化和反序列化
- 添加 46 个单元测试和属性测试

Closes #123"
```

**提交类型**:
- `feat`: 新功能
- `fix`: 错误修复
- `docs`: 文档更新
- `style`: 代码格式化
- `refactor`: 重构
- `test`: 测试相关
- `chore`: 构建/工具相关

## 开发流程

### 功能开发流程
1. **需求分析**: 在 `.kiro/specs/` 创建或更新需求文档
2. **设计**: 编写设计文档，定义接口和正确性属性
3. **实现**: 按照设计文档实现功能
4. **测试**: 编写单元测试和属性测试
5. **文档**: 更新 README 和 API 文档
6. **审查**: 代码审查和性能测试
7. **提交**: 提交代码并推送

### 代码审查要点
- 是否符合命名规范
- 是否有充分的注释
- 是否有足够的测试覆盖
- 是否考虑了并发安全
- 是否考虑了性能影响
- 是否处理了错误情况

## 工具和环境

### 开发工具
- **编辑器**: 支持 C++17 的任何编辑器（推荐 VSCode, CLion）
- **格式化**: clang-format（配置文件：`.clang-format`）
- **静态分析**: clang-tidy, cppcheck
- **动态分析**: Valgrind, AddressSanitizer, ThreadSanitizer
- **性能分析**: perf, Instruments (macOS), VTune

### 测试环境
| 环境 | 系统 | 架构 | 用途 |
|------|------|------|------|
| 本地 macOS | macOS | ARM (M4 Pro) | 主要开发环境 |
| 远程 Linux | Armbian | ARM | ARM 平台测试 |
| 远程 Linux | Debian | x86_64 | x86 平台测试 |

### CI/CD
- 每次提交自动运行测试
- 检查代码格式
- 运行静态分析
- 生成测试覆盖率报告

## 常见问题

### Q: 何时使用 Header-Only 模式？
A: 对于模板密集的代码或需要最大化内联优化的场景。对于大型项目，推荐使用静态库或动态库模式以减少编译时间。

### Q: 如何处理平台差异？
A: 使用条件编译和抽象层。核心逻辑保持平台无关，平台特定代码隔离到单独的文件。

### Q: 性能测试的标准是什么？
A: 日志记录延迟应 < 100ns（同步模式）或 < 50ns（异步模式）。吞吐量应 > 1M logs/sec（单线程）。

### Q: 如何贡献代码？
A: 1) Fork 仓库 2) 创建功能分支 3) 实现功能并测试 4) 提交 Pull Request 5) 等待代码审查