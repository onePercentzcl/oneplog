/**
 * @file binary_snapshot.hpp
 * @brief Binary snapshot for capturing log arguments
 * @brief 用于捕获日志参数的二进制快照
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

namespace oneplog {

/**
 * @brief Type tag enumeration for BinarySnapshot
 * @brief BinarySnapshot 的类型标签枚举
 *
 * Used to identify the type of each captured argument without RTTI.
 * 用于在不使用 RTTI 的情况下识别每个捕获参数的类型。
 */
enum class TypeTag : uint8_t {
    Int32 = 0x01,       ///< int32_t (4B)
    Int64 = 0x02,       ///< int64_t (8B)
    UInt32 = 0x03,      ///< uint32_t (4B)
    UInt64 = 0x04,      ///< uint64_t (8B)
    Float = 0x05,       ///< float (4B)
    Double = 0x06,      ///< double (8B)
    Bool = 0x07,        ///< bool (1B)
    StringView = 0x10,  ///< Static string, pointer+length (12B) / 静态字符串，指针+长度
    StringCopy = 0x11,  ///< Dynamic string, inline copy (2B len + data) / 动态字符串，内联拷贝
    Pointer = 0x20      ///< Pointer type, needs conversion / 指针类型，需要转换
};

/**
 * @brief Binary snapshot for capturing log arguments
 * @brief 用于捕获日志参数的二进制快照
 *
 * BinarySnapshot captures log arguments in a binary format for efficient
 * storage and transmission. It supports:
 * - Zero-copy for static strings (string_view/literals)
 * - Inline copy for dynamic strings (std::string)
 * - Direct storage for primitive types
 *
 * BinarySnapshot 以二进制格式捕获日志参数，以实现高效存储和传输。支持：
 * - 静态字符串的零拷贝（string_view/字面量）
 * - 动态字符串的内联拷贝（std::string）
 * - 基本类型的直接存储
 *
 * Buffer layout / 缓冲区布局:
 * +------------------+
 * | argCount (2B)    |  Number of arguments / 参数数量
 * +------------------+
 * | type[0] (1B)     |  First argument type tag / 第一个参数类型标签
 * +------------------+
 * | data[0] (var)    |  First argument data / 第一个参数数据
 * +------------------+
 * | type[1] (1B)     |  Second argument type tag / 第二个参数类型标签
 * +------------------+
 * | data[1] (var)    |  Second argument data / 第二个参数数据
 * +------------------+
 * | ...              |
 * +------------------+
 */
class BinarySnapshot {
public:
    /// Default buffer size / 默认缓冲区大小
    static constexpr size_t kDefaultBufferSize = 256;
    
    /// Header size (argCount) / 头部大小（参数数量）
    static constexpr size_t kHeaderSize = sizeof(uint16_t);

    /**
     * @brief Default constructor
     * @brief 默认构造函数
     */
    BinarySnapshot() noexcept : m_offset(kHeaderSize), m_argCount(0) {
        // Initialize argCount to 0 / 初始化参数数量为 0
        std::memset(m_buffer.data(), 0, kHeaderSize);
    }

    /**
     * @brief Copy constructor
     * @brief 拷贝构造函数
     */
    BinarySnapshot(const BinarySnapshot& other) noexcept = default;

    /**
     * @brief Move constructor
     * @brief 移动构造函数
     */
    BinarySnapshot(BinarySnapshot&& other) noexcept = default;

    /**
     * @brief Copy assignment
     * @brief 拷贝赋值
     */
    BinarySnapshot& operator=(const BinarySnapshot& other) noexcept = default;

    /**
     * @brief Move assignment
     * @brief 移动赋值
     */
    BinarySnapshot& operator=(BinarySnapshot&& other) noexcept = default;

    /**
     * @brief Get the number of captured arguments
     * @brief 获取捕获的参数数量
     */
    uint16_t ArgCount() const noexcept { return m_argCount; }

    /**
     * @brief Get the current buffer offset (used size)
     * @brief 获取当前缓冲区偏移量（已使用大小）
     */
    size_t Offset() const noexcept { return m_offset; }

    /**
     * @brief Get the buffer capacity
     * @brief 获取缓冲区容量
     */
    static constexpr size_t Capacity() noexcept { return kDefaultBufferSize; }

    /**
     * @brief Get remaining buffer space
     * @brief 获取剩余缓冲区空间
     */
    size_t Remaining() const noexcept { return kDefaultBufferSize - m_offset; }

    /**
     * @brief Check if buffer is empty (no arguments captured)
     * @brief 检查缓冲区是否为空（没有捕获参数）
     */
    bool IsEmpty() const noexcept { return m_argCount == 0; }

    /**
     * @brief Get raw buffer data
     * @brief 获取原始缓冲区数据
     */
    const uint8_t* Data() const noexcept { return m_buffer.data(); }

    /**
     * @brief Get mutable raw buffer data
     * @brief 获取可变原始缓冲区数据
     */
    uint8_t* Data() noexcept { return m_buffer.data(); }

    /**
     * @brief Reset the snapshot to empty state
     * @brief 重置快照为空状态
     */
    void Reset() noexcept {
        m_offset = kHeaderSize;
        m_argCount = 0;
        std::memset(m_buffer.data(), 0, kHeaderSize);
    }

    // =========================================================================
    // Basic type capture methods / 基本类型捕获方法
    // =========================================================================

    /**
     * @brief Capture int32_t value
     * @brief 捕获 int32_t 值
     */
    bool CaptureInt32(int32_t value) noexcept {
        return CaptureValue(TypeTag::Int32, value);
    }

    /**
     * @brief Capture int64_t value
     * @brief 捕获 int64_t 值
     */
    bool CaptureInt64(int64_t value) noexcept {
        return CaptureValue(TypeTag::Int64, value);
    }

    /**
     * @brief Capture uint32_t value
     * @brief 捕获 uint32_t 值
     */
    bool CaptureUInt32(uint32_t value) noexcept {
        return CaptureValue(TypeTag::UInt32, value);
    }

    /**
     * @brief Capture uint64_t value
     * @brief 捕获 uint64_t 值
     */
    bool CaptureUInt64(uint64_t value) noexcept {
        return CaptureValue(TypeTag::UInt64, value);
    }

    /**
     * @brief Capture float value
     * @brief 捕获 float 值
     */
    bool CaptureFloat(float value) noexcept {
        return CaptureValue(TypeTag::Float, value);
    }

    /**
     * @brief Capture double value
     * @brief 捕获 double 值
     */
    bool CaptureDouble(double value) noexcept {
        return CaptureValue(TypeTag::Double, value);
    }

    /**
     * @brief Capture bool value
     * @brief 捕获 bool 值
     */
    bool CaptureBool(bool value) noexcept {
        return CaptureValue(TypeTag::Bool, static_cast<uint8_t>(value ? 1 : 0));
    }

    // =========================================================================
    // String capture methods / 字符串捕获方法
    // =========================================================================

    /**
     * @brief Capture string_view (zero-copy, pointer + length)
     * @brief 捕获 string_view（零拷贝，指针 + 长度）
     *
     * @warning The string data must remain valid until the snapshot is consumed
     * @warning 字符串数据必须在快照被消费之前保持有效
     */
    bool CaptureStringView(std::string_view sv) noexcept {
        // Need: 1B tag + 8B pointer + 4B length = 13B
        constexpr size_t kNeeded = 1 + sizeof(const char*) + sizeof(uint32_t);
        if (Remaining() < kNeeded) {
            return false;
        }

        // Write type tag / 写入类型标签
        m_buffer[m_offset++] = static_cast<uint8_t>(TypeTag::StringView);

        // Write pointer / 写入指针
        const char* ptr = sv.data();
        std::memcpy(&m_buffer[m_offset], &ptr, sizeof(ptr));
        m_offset += sizeof(ptr);

        // Write length / 写入长度
        uint32_t len = static_cast<uint32_t>(sv.size());
        std::memcpy(&m_buffer[m_offset], &len, sizeof(len));
        m_offset += sizeof(len);

        ++m_argCount;
        UpdateArgCount();
        return true;
    }

    /**
     * @brief Capture string (inline copy)
     * @brief 捕获字符串（内联拷贝）
     *
     * The string data is copied into the buffer.
     * 字符串数据被拷贝到缓冲区中。
     */
    bool CaptureString(const std::string& str) noexcept {
        return CaptureStringData(str.data(), str.size());
    }

    /**
     * @brief Capture C-string (inline copy)
     * @brief 捕获 C 字符串（内联拷贝）
     */
    bool CaptureString(const char* str) noexcept {
        if (str == nullptr) {
            return CaptureStringData("", 0);
        }
        return CaptureStringData(str, std::strlen(str));
    }

    // =========================================================================
    // Variadic template capture / 变参模板捕获
    // =========================================================================

    /**
     * @brief Capture multiple arguments
     * @brief 捕获多个参数
     */
    template<typename... Args>
    bool Capture(Args&&... args) {
        return (CaptureOne(std::forward<Args>(args)) && ...);
    }

    // =========================================================================
    // Serialization methods / 序列化方法
    // =========================================================================

    /**
     * @brief Get serialized size
     * @brief 获取序列化大小
     */
    size_t SerializedSize() const noexcept { return m_offset; }

    /**
     * @brief Serialize to buffer
     * @brief 序列化到缓冲区
     */
    void SerializeTo(uint8_t* buffer) const noexcept {
        std::memcpy(buffer, m_buffer.data(), m_offset);
    }

    /**
     * @brief Deserialize from buffer
     * @brief 从缓冲区反序列化
     */
    static BinarySnapshot Deserialize(const uint8_t* data, size_t size) noexcept {
        BinarySnapshot snapshot;
        size_t copySize = (size < kDefaultBufferSize) ? size : kDefaultBufferSize;
        std::memcpy(snapshot.m_buffer.data(), data, copySize);
        
        // Read argCount / 读取参数数量
        std::memcpy(&snapshot.m_argCount, data, sizeof(uint16_t));
        snapshot.m_offset = copySize;
        
        return snapshot;
    }

    // =========================================================================
    // Format method / 格式化方法
    // =========================================================================

    /**
     * @brief Format the captured arguments using format string
     * @brief 使用格式字符串格式化捕获的参数
     */
    std::string Format(const char* fmt) const {
        if (fmt == nullptr || m_argCount == 0) {
            return fmt ? std::string(fmt) : std::string();
        }

        std::string result;
        result.reserve(256);
        const char* p = fmt;
        size_t offset = kHeaderSize;  // Skip argCount header
        uint16_t argIndex = 0;

        while (*p != '\0') {
            if (*p == '{' && *(p + 1) == '}') {
                // Found placeholder {} / 找到占位符 {}
                if (argIndex < m_argCount && offset < m_offset) {
                    FormatArg(result, offset);
                    ++argIndex;
                }
                p += 2;  // Skip {}
            } else {
                result += *p++;
            }
        }

        return result;
    }

    /**
     * @brief Format all captured arguments using the first argument as format string
     * @brief 使用第一个参数作为格式字符串格式化所有捕获的参数
     *
     * The first captured argument should be the format string (StringView).
     * Subsequent arguments are formatted according to {} placeholders.
     * 第一个捕获的参数应该是格式字符串（StringView）。
     * 后续参数根据 {} 占位符进行格式化。
     *
     * @return Formatted string with all arguments / 包含所有参数的格式化字符串
     */
    std::string FormatAll() const {
        if (m_argCount == 0) {
            return std::string();
        }

        // First argument should be the format string
        // 第一个参数应该是格式字符串
        size_t offset = kHeaderSize;
        
        // Bounds check
        if (offset >= m_offset) {
            return std::string();
        }
        
        TypeTag firstTag = static_cast<TypeTag>(m_buffer[offset]);

        if (firstTag == TypeTag::StringView || firstTag == TypeTag::StringCopy) {
            // Extract format string / 提取格式字符串
            std::string fmtStr;
            ++offset;  // Skip tag

            if (firstTag == TypeTag::StringView) {
                // Bounds check for pointer and length
                if (offset + sizeof(const char*) + sizeof(uint32_t) > m_offset) {
                    return std::string();
                }
                
                const char* ptr;
                std::memcpy(&ptr, &m_buffer[offset], sizeof(ptr));
                offset += sizeof(ptr);
                uint32_t len;
                std::memcpy(&len, &m_buffer[offset], sizeof(len));
                offset += sizeof(len);
                
                // Validate pointer before dereferencing
                if (ptr != nullptr && len > 0 && len < 10000) {  // Sanity check on length
                    fmtStr.assign(ptr, len);
                }
            } else {  // StringCopy
                // Bounds check for length
                if (offset + sizeof(uint16_t) > m_offset) {
                    return std::string();
                }
                
                uint16_t len;
                std::memcpy(&len, &m_buffer[offset], sizeof(len));
                offset += sizeof(len);
                
                // Bounds check for string data
                if (offset + len > m_offset) {
                    return std::string();
                }
                
                if (len > 0) {
                    fmtStr.assign(reinterpret_cast<const char*>(&m_buffer[offset]), len);
                    offset += len;
                }
            }

            // If only format string (no placeholders or no more args), return it
            // 如果只有格式字符串（没有占位符或没有更多参数），直接返回
            if (m_argCount == 1) {
                return fmtStr;
            }

            // Format with remaining arguments / 使用剩余参数格式化
            std::string result;
            result.reserve(256);
            const char* p = fmtStr.c_str();
            uint16_t argIndex = 1;  // Skip format string (arg 0)

            while (*p != '\0') {
                if (*p == '{' && *(p + 1) == '}') {
                    // Found placeholder {} / 找到占位符 {}
                    if (argIndex < m_argCount && offset < m_offset) {
                        FormatArg(result, offset);
                        ++argIndex;
                    }
                    p += 2;  // Skip {}
                } else {
                    result += *p++;
                }
            }

            return result;
        }

        // Fallback: format all arguments separated by space
        // 回退：用空格分隔格式化所有参数
        std::string result;
        result.reserve(256);

        for (uint16_t i = 0; i < m_argCount && offset < m_offset; ++i) {
            if (i > 0) {
                result += ' ';
            }
            FormatArg(result, offset);
        }

        return result;
    }

    // =========================================================================
    // Pointer conversion / 指针转换
    // =========================================================================

    /**
     * @brief Convert pointer data to inline data (for cross-process transfer)
     * @brief 将指针数据转换为内联数据（用于跨进程传输）
     *
     * Called by PipelineThread to convert StringView pointers to StringCopy.
     * 由 PipelineThread 调用，将 StringView 指针转换为 StringCopy。
     */
    void ConvertPointersToData() {
        if (m_argCount == 0) {
            return;
        }

        // Create a new buffer for converted data
        // 创建一个新缓冲区用于转换后的数据
        std::array<uint8_t, kDefaultBufferSize> newBuffer;
        size_t newOffset = kHeaderSize;
        size_t offset = kHeaderSize;
        uint16_t newArgCount = 0;

        while (offset < m_offset && newArgCount < m_argCount) {
            TypeTag tag = static_cast<TypeTag>(m_buffer[offset++]);

            if (tag == TypeTag::StringView) {
                // Convert StringView to StringCopy
                // 将 StringView 转换为 StringCopy
                const char* ptr;
                std::memcpy(&ptr, &m_buffer[offset], sizeof(ptr));
                offset += sizeof(ptr);
                uint32_t len;
                std::memcpy(&len, &m_buffer[offset], sizeof(len));
                offset += sizeof(len);

                // Check if we have enough space / 检查是否有足够空间
                size_t needed = 1 + sizeof(uint16_t) + len;
                if (newOffset + needed <= kDefaultBufferSize) {
                    newBuffer[newOffset++] = static_cast<uint8_t>(TypeTag::StringCopy);
                    uint16_t strLen = static_cast<uint16_t>(len > 65535 ? 65535 : len);
                    std::memcpy(&newBuffer[newOffset], &strLen, sizeof(strLen));
                    newOffset += sizeof(strLen);
                    if (ptr != nullptr && strLen > 0) {
                        std::memcpy(&newBuffer[newOffset], ptr, strLen);
                        newOffset += strLen;
                    }
                    ++newArgCount;
                }
            } else {
                // Copy other types as-is / 其他类型原样拷贝
                size_t dataSize = GetTypeDataSize(tag, offset);
                size_t needed = 1 + dataSize;
                if (newOffset + needed <= kDefaultBufferSize) {
                    newBuffer[newOffset++] = static_cast<uint8_t>(tag);
                    std::memcpy(&newBuffer[newOffset], &m_buffer[offset], dataSize);
                    newOffset += dataSize;
                    ++newArgCount;
                }
                offset += dataSize;
            }
        }

        // Update buffer with converted data / 用转换后的数据更新缓冲区
        m_buffer = newBuffer;
        m_offset = newOffset;
        m_argCount = newArgCount;
        std::memcpy(m_buffer.data(), &m_argCount, sizeof(m_argCount));
    }

    // =========================================================================
    // Comparison operators / 比较运算符
    // =========================================================================

    bool operator==(const BinarySnapshot& other) const noexcept {
        if (m_argCount != other.m_argCount || m_offset != other.m_offset) {
            return false;
        }
        return std::memcmp(m_buffer.data(), other.m_buffer.data(), m_offset) == 0;
    }

    bool operator!=(const BinarySnapshot& other) const noexcept {
        return !(*this == other);
    }

private:
    /**
     * @brief Capture a primitive value
     * @brief 捕获一个基本类型值
     */
    template<typename T>
    bool CaptureValue(TypeTag tag, T value) noexcept {
        constexpr size_t kNeeded = 1 + sizeof(T);
        if (Remaining() < kNeeded) {
            return false;
        }

        // Write type tag / 写入类型标签
        m_buffer[m_offset++] = static_cast<uint8_t>(tag);

        // Write value / 写入值
        std::memcpy(&m_buffer[m_offset], &value, sizeof(T));
        m_offset += sizeof(T);

        ++m_argCount;
        UpdateArgCount();
        return true;
    }

    /**
     * @brief Capture string data (inline copy)
     * @brief 捕获字符串数据（内联拷贝）
     */
    bool CaptureStringData(const char* data, size_t len) noexcept {
        // Need: 1B tag + 2B length + data
        size_t needed = 1 + sizeof(uint16_t) + len;
        if (Remaining() < needed) {
            return false;
        }

        // Write type tag / 写入类型标签
        m_buffer[m_offset++] = static_cast<uint8_t>(TypeTag::StringCopy);

        // Write length (max 65535) / 写入长度（最大 65535）
        uint16_t strLen = static_cast<uint16_t>(
            len > 65535 ? 65535 : len);
        std::memcpy(&m_buffer[m_offset], &strLen, sizeof(strLen));
        m_offset += sizeof(strLen);

        // Write data / 写入数据
        if (strLen > 0) {
            std::memcpy(&m_buffer[m_offset], data, strLen);
            m_offset += strLen;
        }

        ++m_argCount;
        UpdateArgCount();
        return true;
    }

    /**
     * @brief Update argCount in buffer header
     * @brief 更新缓冲区头部的参数数量
     */
    void UpdateArgCount() noexcept {
        std::memcpy(m_buffer.data(), &m_argCount, sizeof(m_argCount));
    }

    /**
     * @brief Format a single argument and append to result
     * @brief 格式化单个参数并追加到结果
     */
    void FormatArg(std::string& result, size_t& offset) const {
        TypeTag tag = static_cast<TypeTag>(m_buffer[offset++]);

        switch (tag) {
            case TypeTag::Int32: {
                int32_t val;
                std::memcpy(&val, &m_buffer[offset], sizeof(val));
                offset += sizeof(val);
                result += std::to_string(val);
                break;
            }
            case TypeTag::Int64: {
                int64_t val;
                std::memcpy(&val, &m_buffer[offset], sizeof(val));
                offset += sizeof(val);
                result += std::to_string(val);
                break;
            }
            case TypeTag::UInt32: {
                uint32_t val;
                std::memcpy(&val, &m_buffer[offset], sizeof(val));
                offset += sizeof(val);
                result += std::to_string(val);
                break;
            }
            case TypeTag::UInt64: {
                uint64_t val;
                std::memcpy(&val, &m_buffer[offset], sizeof(val));
                offset += sizeof(val);
                result += std::to_string(val);
                break;
            }
            case TypeTag::Float: {
                float val;
                std::memcpy(&val, &m_buffer[offset], sizeof(val));
                offset += sizeof(val);
                result += std::to_string(val);
                break;
            }
            case TypeTag::Double: {
                double val;
                std::memcpy(&val, &m_buffer[offset], sizeof(val));
                offset += sizeof(val);
                result += std::to_string(val);
                break;
            }
            case TypeTag::Bool: {
                uint8_t val = m_buffer[offset++];
                result += (val ? "true" : "false");
                break;
            }
            case TypeTag::StringView: {
                const char* ptr;
                std::memcpy(&ptr, &m_buffer[offset], sizeof(ptr));
                offset += sizeof(ptr);
                uint32_t len;
                std::memcpy(&len, &m_buffer[offset], sizeof(len));
                offset += sizeof(len);
                if (ptr != nullptr && len > 0) {
                    result.append(ptr, len);
                }
                break;
            }
            case TypeTag::StringCopy: {
                uint16_t len;
                std::memcpy(&len, &m_buffer[offset], sizeof(len));
                offset += sizeof(len);
                if (len > 0) {
                    result.append(reinterpret_cast<const char*>(&m_buffer[offset]), len);
                    offset += len;
                }
                break;
            }
            case TypeTag::Pointer: {
                void* ptr;
                std::memcpy(&ptr, &m_buffer[offset], sizeof(ptr));
                offset += sizeof(ptr);
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%p", ptr);
                result += buf;
                break;
            }
        }
    }

    /**
     * @brief Get data size for a type tag
     * @brief 获取类型标签的数据大小
     */
    size_t GetTypeDataSize(TypeTag tag, size_t offset) const noexcept {
        switch (tag) {
            case TypeTag::Int32:
            case TypeTag::UInt32:
            case TypeTag::Float:
                return 4;
            case TypeTag::Int64:
            case TypeTag::UInt64:
            case TypeTag::Double:
            case TypeTag::Pointer:
                return 8;
            case TypeTag::Bool:
                return 1;
            case TypeTag::StringView:
                return sizeof(const char*) + sizeof(uint32_t);
            case TypeTag::StringCopy: {
                uint16_t len;
                std::memcpy(&len, &m_buffer[offset], sizeof(len));
                return sizeof(uint16_t) + len;
            }
            default:
                return 0;
        }
    }

    /**
     * @brief Capture one argument (type dispatch)
     * @brief 捕获一个参数（类型分发）
     */
    template<typename T>
    bool CaptureOne(T&& value) {
        using DecayT = std::decay_t<T>;
        
        if constexpr (std::is_same_v<DecayT, int32_t> || 
                      std::is_same_v<DecayT, int>) {
            return CaptureInt32(static_cast<int32_t>(value));
        } else if constexpr (std::is_same_v<DecayT, int64_t> ||
                             std::is_same_v<DecayT, long long>) {
            return CaptureInt64(static_cast<int64_t>(value));
        } else if constexpr (std::is_same_v<DecayT, uint32_t> ||
                             std::is_same_v<DecayT, unsigned int>) {
            return CaptureUInt32(static_cast<uint32_t>(value));
        } else if constexpr (std::is_same_v<DecayT, uint64_t> ||
                             std::is_same_v<DecayT, unsigned long long>) {
            return CaptureUInt64(static_cast<uint64_t>(value));
        } else if constexpr (std::is_same_v<DecayT, float>) {
            return CaptureFloat(value);
        } else if constexpr (std::is_same_v<DecayT, double>) {
            return CaptureDouble(value);
        } else if constexpr (std::is_same_v<DecayT, bool>) {
            return CaptureBool(value);
        } else if constexpr (std::is_same_v<DecayT, std::string_view>) {
            return CaptureStringView(value);
        } else if constexpr (std::is_same_v<DecayT, std::string>) {
            return CaptureString(value);
        } else if constexpr (std::is_same_v<DecayT, const char*> ||
                             std::is_same_v<DecayT, char*>) {
            // For char*, use inline copy by default
            // 对于 char*，默认使用内联拷贝
            return CaptureString(value);
        } else if constexpr (std::is_array_v<std::remove_reference_t<T>> &&
                             std::is_same_v<std::remove_extent_t<std::remove_reference_t<T>>, char>) {
            // For char arrays (string literals), use string_view (zero-copy)
            // 对于字符数组（字符串字面量），使用 string_view（零拷贝）
            return CaptureStringView(std::string_view(value));
        } else {
            // Unsupported type / 不支持的类型
            static_assert(sizeof(T) == 0, "Unsupported type for BinarySnapshot::Capture");
            return false;
        }
    }

private:
    std::array<uint8_t, kDefaultBufferSize> m_buffer;  ///< Data buffer / 数据缓冲区
    size_t m_offset;                                    ///< Current write offset / 当前写入偏移
    uint16_t m_argCount;                                ///< Number of arguments / 参数数量
};

}  // namespace oneplog
