# onePlog 项目规范

## 项目基本信息

- **项目名称**: onePlog
- **C++标准**: C++17
- **构建工具**: XMake 和 CMake
- **文档工具**: Doxygen
- **代码格式化**: clang-format
- **命名空间**: oneplog
- **远程仓库**: https://github.com/onePercentzcl

## 交流语言

使用中文进行交流和沟通。

## 目录结构

```
oneplog/
├── src/                          # 源代码目录
├── include/                      # 头文件目录
├── example/                      # 示例代码
├── tests/                        # 测试代码
├── docs/                         # 文档目录
├── CMakeLists.txt                # CMake 构建文件
├── xmake.lua                     # XMake 构建文件
├── .clang-format                 # clang-format 配置
└── Doxyfile                      # Doxygen 配置
```

## 命名规范

| **元素** | **规范** | **示例** |
|---------|----------|----------|
| 文件名 | snake_case | `memory_pool.cpp` |
| 类名 | PascalCase | `MemoryPool` |
| 函数名 | PascalCase | `AllocateBlock()` |
| 局部变量 | camelCase | `blockSize` |
| 成员变量 | m_ + camelCase | `m_blockSize` |
| 全局常量 | k + PascalCase | `kDefaultPoolSize` |
| 宏定义 | ALL_CAPS | `ONEPLOG_SYNC_ONLY` |
| 命名空间 | oneplog | `oneplog::Info()` |
| 枚举值 | PascalCase | `Level::Info` |

## 代码格式化配置

```yaml
BasedOnStyle: Google
IndentWidth: 4
ColumnLimit: 100
AllowShortFunctionsOnASingleLine: Inline
```

## Doxygen 注释规范

使用中英双语注释。

## 测试环境

| 环境 | 系统 | 架构 | IP | 用户 |
|------|------|------|-----|------|
| 本地 macOS | macOS | ARM (M4 Pro) | 192.168.10.100 | - |
| 远程 Linux | Armbian | ARM | 192.168.10.13 | root |
| 远程 Linux | Debian | x86 | 192.168.10.51 | onepet |

## 任务完成后的操作

每完成一个任务后，需要执行以下操作：

1. **更新文档**: 更新两份 README 文档
   - `README.md` (中文版)
   - `README_ENGLISH.md` (英文版)

2. **Git 提交并推送**:
   ```bash
   git add .
   git commit -m "描述本次更改"
   git push origin main
   ```
3. 修复对后续开发有影响的警告