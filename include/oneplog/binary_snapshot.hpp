/**
 * @file binary_snapshot.hpp
 * @brief Binary snapshot for zero-copy log parameter capture
 * @文件 binary_snapshot.hpp
 * @简述 用于零拷贝日志参数捕获的二进制快照
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#pragma once

#include "common.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

namespace oneplog {

// ==============================================================================
// Type Tag / 类型标签
// ==============================================================================

/**
 * @brief Type tag enumeration for identifying captured value types
 * @brief 类型标签枚举，用于标识捕获值的类型
 */
enum class TypeTag : uint8_t {
    Int32 = 0x01,      ///< int32_t type / int32_t 类型
    Int64 = 0x02,      ///< int64_t type / int64_t 类型
    UInt32 = 0x03,     ///< uint32_t type / uint32_t 类型
    UInt64 = 0x04,     ///< uint64_t type / uint64_t 类型
    Float = 0x05,      ///< float type / float 类型
    Double = 0x06,     ///< double type / double 类型
    Bool = 0x07,       ///< bool type / bool 类型
    StringView = 0x10, ///< string_view (zero-copy) / string_view（零拷贝）
    StringCopy = 0x11, ///< string (inline copy) / string（内联拷贝）
    Pointer = 0x20     ///< pointer type (needs conversion) / 指针类型（需要转换）
};

/**
 * @brief Convert type tag to string representation
 * @brief 将类型标签转换为字符串表示
 * @param tag The type tag / 类型标签
 * @return std::string_view The string representation / 字符串表示
 */
constexpr std::string_view TypeTagToString(const TypeTag tag) noexcept {
    switch (tag) {
        case TypeTag::Int32:
            return "Int32";
        case TypeTag::Int64:
            return "Int64";
        case TypeTag::UInt32:
            return "UInt32";
        case TypeTag::UInt64:
            return "UInt64";
        case TypeTag::Float:
            return "Float";
        case TypeTag::Double:
            return "Double";
        case TypeTag::Bool:
            return "Bool";
        case TypeTag::StringView:
            return "StringView";
        case TypeTag::StringCopy:
            return "StringCopy";
        case TypeTag::Pointer:
            return "Pointer";
        default:
            return "Unknown";
    }
}

// ==============================================================================
// BinarySnapshot Class / BinarySnapshot 类
// ==============================================================================

/**
 * @brief Binary snapshot for capturing log parameters with zero-copy optimization
 * @brief 用于捕获日志参数的二进制快照，具有零拷贝优化
 * 
 * BinarySnapshot uses a mixed storage strategy:
 * - Static strings (string_view, literals): Store pointer + length (zero-copy)
 * - Dynamic strings (std::string, temporary const char*): Inline copy to buffer
 * - Basic types: Direct inline storage
 * 
 * BinarySnapshot 使用混合存储策略：
 * - 静态字符串（string_view、字面量）：仅存储指针+长度（零拷贝）
 * - 动态字符串（std::string、临时 const char*）：内联拷贝到缓冲区
 * - 基本类型：直接内联存储
 */
class BinarySnapshot {
public:
    /// Default buffer size for inline storage / 内联存储的默认缓冲区大小
    static constexpr size_t kDefaultBufferSize = 256;

    /// Maximum number of arguments / 最大参数数量
    static constexpr uint16_t kMaxArgCount = 32;

    /**
     * @brief Default constructor
     * @brief 默认构造函数
     */
    BinarySnapshot() = default;

    /**
     * @brief Copy constructor
     * @brief 拷贝构造函数
     */
    BinarySnapshot(const BinarySnapshot&) = default;

    /**
     * @brief Move constructor
     * @brief 移动构造函数
     */
    BinarySnapshot(BinarySnapshot&&) noexcept = default;

    /**
     * @brief Copy assignment operator
     * @brief 拷贝赋值运算符
     */
    BinarySnapshot& operator=(const BinarySnapshot&) = default;

    /**
     * @brief Move assignment operator
     * @brief 移动赋值运算符
     */
    BinarySnapshot& operator=(BinarySnapshot&&) noexcept = default;

    /**
     * @brief Destructor
     * @brief 析构函数
     */
    ~BinarySnapshot() = default;

    // ==========================================================================
    // Basic Type Capture Methods / 基本类型捕获方法
    // ==========================================================================

    /**
     * @brief Capture int32_t value
     * @brief 捕获 int32_t 值
     * @param value The value to capture / 待捕获的值
     * @return bool True if successful / 如果成功则返回 true
     */
    bool CaptureInt32(int32_t value);

    /**
     * @brief Capture int64_t value
     * @brief 捕获 int64_t 值
     * @param value The value to capture / 待捕获的值
     * @return bool True if successful / 如果成功则返回 true
     */
    bool CaptureInt64(int64_t value);

    /**
     * @brief Capture uint32_t value
     * @brief 捕获 uint32_t 值
     * @param value The value to capture / 待捕获的值
     * @return bool True if successful / 如果成功则返回 true
     */
    bool CaptureUInt32(uint32_t value);

    /**
     * @brief Capture uint64_t value
     * @brief 捕获 uint64_t 值
     * @param value The value to capture / 待捕获的值
     * @return bool True if successful / 如果成功则返回 true
     */
    bool CaptureUInt64(uint64_t value);

    /**
     * @brief Capture float value
     * @brief 捕获 float 值
     * @param value The value to capture / 待捕获的值
     * @return bool True if successful / 如果成功则返回 true
     */
    bool CaptureFloat(float value);

    /**
     * @brief Capture double value
     * @brief 捕获 double 值
     * @param value The value to capture / 待捕获的值
     * @return bool True if successful / 如果成功则返回 true
     */
    bool CaptureDouble(double value);

    /**
     * @brief Capture bool value
     * @brief 捕获 bool 值
     * @param value The value to capture / 待捕获的值
     * @return bool True if successful / 如果成功则返回 true
     */
    bool CaptureBool(bool value);

    // ==========================================================================
    // String Capture Methods / 字符串捕获方法
    // ==========================================================================

    /**
     * @brief Capture string_view (zero-copy, stores pointer + length)
     * @brief 捕获 string_view（零拷贝，仅存储指针+长度）
     * @param sv The string_view to capture / 待捕获的 string_view
     * @return bool True if successful / 如果成功则返回 true
     */
    bool CaptureStringView(std::string_view sv);

    /**
     * @brief Capture std::string (inline copy to buffer)
     * @brief 捕获 std::string（内联拷贝到缓冲区）
     * @param str The string to capture / 待捕获的字符串
     * @return bool True if successful / 如果成功则返回 true
     */
    bool CaptureString(const std::string& str);

    /**
     * @brief Capture const char* (inline copy to buffer)
     * @brief 捕获 const char*（内联拷贝到缓冲区）
     * @param str The C-string to capture / 待捕获的 C 字符串
     * @return bool True if successful / 如果成功则返回 true
     */
    bool CaptureString(const char* str);

    // ==========================================================================
    // Variadic Template Capture / 变参模板捕获
    // ==========================================================================

    /**
     * @brief Capture multiple arguments using variadic templates
     * @brief 使用变参模板捕获多个参数
     * @tparam Args Argument types / 参数类型
     * @param args Arguments to capture / 待捕获的参数
     * @return bool True if all captures successful / 如果所有捕获都成功则返回 true
     */
    template <typename... Args>
    bool Capture(Args&&... args);

    // ==========================================================================
    // Serialization / 序列化
    // ==========================================================================

    /**
     * @brief Get the size required for serialization
     * @brief 获取序列化所需的大小
     * @return size_t The serialized size in bytes / 序列化大小（字节）
     */
    size_t SerializedSize() const;

    /**
     * @brief Serialize to buffer
     * @brief 序列化到缓冲区
     * @param buffer The destination buffer / 目标缓冲区
     * @return bool True if successful / 如果成功则返回 true
     */
    bool SerializeTo(uint8_t* buffer) const;

    /**
     * @brief Deserialize from buffer
     * @brief 从缓冲区反序列化
     * @param data The source buffer / 源缓冲区
     * @param size The buffer size / 缓冲区大小
     * @return BinarySnapshot The deserialized snapshot / 反序列化的快照
     */
    static BinarySnapshot Deserialize(const uint8_t* data, size_t size);

    // ==========================================================================
    // Formatting / 格式化
    // ==========================================================================

    /**
     * @brief Format the captured arguments using a format string
     * @brief 使用格式字符串格式化捕获的参数
     * @param fmt The format string (printf-style) / 格式字符串（printf 风格）
     * @return std::string The formatted string / 格式化后的字符串
     */
    std::string Format(const char* fmt) const;

    // ==========================================================================
    // Pointer Conversion / 指针转换
    // ==========================================================================

    /**
     * @brief Convert pointer data to actual data (for cross-process transfer)
     * @brief 将指针数据转换为实际数据（用于跨进程传输）
     * 
     * This method is called by PipelineThread to convert StringView pointers
     * to inline StringCopy data before transferring to SharedRingBuffer.
     * 
     * 此方法由 PipelineThread 调用，在传输到 SharedRingBuffer 之前
     * 将 StringView 指针转换为内联 StringCopy 数据。
     */
    void ConvertPointersToData();

    // ==========================================================================
    // Query Methods / 查询方法
    // ==========================================================================

    /**
     * @brief Get the number of captured arguments
     * @brief 获取捕获的参数数量
     * @return uint16_t The argument count / 参数数量
     */
    uint16_t GetArgCount() const { return m_argCount; }

    /**
     * @brief Get the current buffer offset
     * @brief 获取当前缓冲区偏移量
     * @return size_t The offset in bytes / 偏移量（字节）
     */
    size_t GetOffset() const { return m_offset; }

    /**
     * @brief Check if the snapshot is empty
     * @brief 检查快照是否为空
     * @return bool True if empty / 如果为空则返回 true
     */
    bool IsEmpty() const { return m_argCount == 0; }

    /**
     * @brief Reset the snapshot to empty state
     * @brief 重置快照到空状态
     */
    void Reset();

    /**
     * @brief Equality operator for testing
     * @brief 相等运算符（用于测试）
     */
    bool operator==(const BinarySnapshot& other) const;

    /**
     * @brief Inequality operator for testing
     * @brief 不等运算符（用于测试）
     */
    bool operator!=(const BinarySnapshot& other) const;

private:
    // ==========================================================================
    // Helper Methods / 辅助方法
    // ==========================================================================

    /**
     * @brief Write type tag to buffer
     * @brief 将类型标签写入缓冲区
     */
    bool WriteTag(TypeTag tag);

    /**
     * @brief Write raw data to buffer
     * @brief 将原始数据写入缓冲区
     */
    template <typename T>
    bool WriteData(const T& value);

    /**
     * @brief Write bytes to buffer
     * @brief 将字节写入缓冲区
     */
    bool WriteBytes(const void* data, size_t size);

    /**
     * @brief Check if there's enough space in buffer
     * @brief 检查缓冲区是否有足够空间
     */
    bool HasSpace(size_t size) const;

    /**
     * @brief Capture single argument (helper for variadic template)
     * @brief 捕获单个参数（变参模板的辅助函数）
     */
    template <typename T>
    bool CaptureOne(T&& arg);

    // ==========================================================================
    // Member Variables / 成员变量
    // ==========================================================================

    /// Internal buffer for inline storage / 内联存储的内部缓冲区
    std::array<uint8_t, kDefaultBufferSize> m_buffer{};

    /// Current write offset in buffer / 缓冲区中的当前写入偏移量
    size_t m_offset{0};

    /// Number of captured arguments / 捕获的参数数量
    uint16_t m_argCount{0};
};

// ==============================================================================
// Template Implementation / 模板实现
// ==============================================================================

template <typename... Args>
bool BinarySnapshot::Capture(Args&&... args) {
    // Capture each argument using fold expression
    // 使用折叠表达式捕获每个参数
    return (CaptureOne(std::forward<Args>(args)) && ...);
}

template <typename T>
bool BinarySnapshot::CaptureOne(T&& arg) {
    using DecayedT = std::decay_t<T>;

    // Handle different types
    if constexpr (std::is_same_v<DecayedT, int32_t>) {
        return CaptureInt32(arg);
    } else if constexpr (std::is_same_v<DecayedT, int64_t>) {
        return CaptureInt64(arg);
    } else if constexpr (std::is_same_v<DecayedT, uint32_t>) {
        return CaptureUInt32(arg);
    } else if constexpr (std::is_same_v<DecayedT, uint64_t>) {
        return CaptureUInt64(arg);
    } else if constexpr (std::is_same_v<DecayedT, float>) {
        return CaptureFloat(arg);
    } else if constexpr (std::is_same_v<DecayedT, double>) {
        return CaptureDouble(arg);
    } else if constexpr (std::is_same_v<DecayedT, bool>) {
        return CaptureBool(arg);
    } else if constexpr (std::is_same_v<DecayedT, std::string_view>) {
        return CaptureStringView(arg);
    } else if constexpr (std::is_same_v<DecayedT, std::string>) {
        return CaptureString(arg);
    } else if constexpr (std::is_same_v<DecayedT, const char*> || std::is_same_v<DecayedT, char*>) {
        return CaptureString(arg);
    } else if constexpr (std::is_integral_v<DecayedT>) {
        // Handle other integral types by converting to int64_t or uint64_t
        if constexpr (std::is_signed_v<DecayedT>) {
            return CaptureInt64(static_cast<int64_t>(arg));
        } else {
            return CaptureUInt64(static_cast<uint64_t>(arg));
        }
    } else if constexpr (std::is_floating_point_v<DecayedT>) {
        return CaptureDouble(static_cast<double>(arg));
    } else {
        // Unsupported type
        static_assert(std::is_same_v<DecayedT, void>, "Unsupported type for BinarySnapshot::Capture");
        return false;
    }
}

template <typename T>
bool BinarySnapshot::WriteData(const T& value) {
    if (!HasSpace(sizeof(T))) {
        return false;
    }
    std::memcpy(m_buffer.data() + m_offset, &value, sizeof(T));
    m_offset += sizeof(T);
    return true;
}

// ==============================================================================
// Inline Implementation / 内联实现
// ==============================================================================

// Basic Type Capture Methods / 基本类型捕获方法

inline bool BinarySnapshot::CaptureInt32(int32_t value) {
    if (m_argCount >= kMaxArgCount) {
        return false;
    }
    if (!WriteTag(TypeTag::Int32)) {
        return false;
    }
    if (!WriteData(value)) {
        return false;
    }
    ++m_argCount;
    return true;
}

inline bool BinarySnapshot::CaptureInt64(int64_t value) {
    if (m_argCount >= kMaxArgCount) {
        return false;
    }
    if (!WriteTag(TypeTag::Int64)) {
        return false;
    }
    if (!WriteData(value)) {
        return false;
    }
    ++m_argCount;
    return true;
}

inline bool BinarySnapshot::CaptureUInt32(uint32_t value) {
    if (m_argCount >= kMaxArgCount) {
        return false;
    }
    if (!WriteTag(TypeTag::UInt32)) {
        return false;
    }
    if (!WriteData(value)) {
        return false;
    }
    ++m_argCount;
    return true;
}

inline bool BinarySnapshot::CaptureUInt64(uint64_t value) {
    if (m_argCount >= kMaxArgCount) {
        return false;
    }
    if (!WriteTag(TypeTag::UInt64)) {
        return false;
    }
    if (!WriteData(value)) {
        return false;
    }
    ++m_argCount;
    return true;
}

inline bool BinarySnapshot::CaptureFloat(float value) {
    if (m_argCount >= kMaxArgCount) {
        return false;
    }
    if (!WriteTag(TypeTag::Float)) {
        return false;
    }
    if (!WriteData(value)) {
        return false;
    }
    ++m_argCount;
    return true;
}

inline bool BinarySnapshot::CaptureDouble(double value) {
    if (m_argCount >= kMaxArgCount) {
        return false;
    }
    if (!WriteTag(TypeTag::Double)) {
        return false;
    }
    if (!WriteData(value)) {
        return false;
    }
    ++m_argCount;
    return true;
}

inline bool BinarySnapshot::CaptureBool(bool value) {
    if (m_argCount >= kMaxArgCount) {
        return false;
    }
    if (!WriteTag(TypeTag::Bool)) {
        return false;
    }
    uint8_t boolValue = value ? 1 : 0;
    if (!WriteData(boolValue)) {
        return false;
    }
    ++m_argCount;
    return true;
}

// String Capture Methods / 字符串捕获方法

inline bool BinarySnapshot::CaptureStringView(std::string_view sv) {
    if (m_argCount >= kMaxArgCount) {
        return false;
    }
    if (!WriteTag(TypeTag::StringView)) {
        return false;
    }

    // Store pointer and length (zero-copy)
    // 存储指针和长度（零拷贝）
    const char* ptr = sv.data();
    uint32_t length = static_cast<uint32_t>(sv.size());

    if (!WriteData(reinterpret_cast<uintptr_t>(ptr))) {
        return false;
    }
    if (!WriteData(length)) {
        return false;
    }

    ++m_argCount;
    return true;
}

inline bool BinarySnapshot::CaptureString(const std::string& str) {
    return CaptureString(str.c_str());
}

inline bool BinarySnapshot::CaptureString(const char* str) {
    if (m_argCount >= kMaxArgCount) {
        return false;
    }
    if (!WriteTag(TypeTag::StringCopy)) {
        return false;
    }

    // Inline copy to buffer
    // 内联拷贝到缓冲区
    const size_t length = std::strlen(str);
    const uint32_t length32 = static_cast<uint32_t>(length);

    // Write length first
    if (!WriteData(length32)) {
        return false;
    }

    // Write string data
    if (!WriteBytes(str, length)) {
        return false;
    }

    ++m_argCount;
    return true;
}

// Serialization / 序列化

inline size_t BinarySnapshot::SerializedSize() const {
    // Format: argCount (2B) + offset (size_t) + buffer data
    return sizeof(uint16_t) + sizeof(size_t) + m_offset;
}

inline bool BinarySnapshot::SerializeTo(uint8_t* buffer) const {
    if (buffer == nullptr) {
        return false;
    }

    size_t offset = 0;

    // Write argCount
    std::memcpy(buffer + offset, &m_argCount, sizeof(uint16_t));
    offset += sizeof(uint16_t);

    // Write data offset
    std::memcpy(buffer + offset, &m_offset, sizeof(size_t));
    offset += sizeof(size_t);

    // Write buffer data
    std::memcpy(buffer + offset, m_buffer.data(), m_offset);

    return true;
}

inline BinarySnapshot BinarySnapshot::Deserialize(const uint8_t* data, size_t size) {
    BinarySnapshot snapshot;

    if (data == nullptr || size < sizeof(uint16_t) + sizeof(size_t)) {
        return snapshot;
    }

    size_t offset = 0;

    // Read argCount
    std::memcpy(&snapshot.m_argCount, data + offset, sizeof(uint16_t));
    offset += sizeof(uint16_t);

    // Read data offset
    std::memcpy(&snapshot.m_offset, data + offset, sizeof(size_t));
    offset += sizeof(size_t);

    // Validate offset
    if (snapshot.m_offset > kDefaultBufferSize || offset + snapshot.m_offset > size) {
        snapshot.Reset();
        return snapshot;
    }

    // Read buffer data
    std::memcpy(snapshot.m_buffer.data(), data + offset, snapshot.m_offset);

    return snapshot;
}

// Formatting / 格式化

inline std::string BinarySnapshot::Format(const char* fmt) const {
    if (fmt == nullptr || m_argCount == 0) {
        return fmt ? std::string(fmt) : std::string();
    }

    // Simple implementation: replace %d, %s, %f, etc. with captured values
    // 简单实现：将 %d、%s、%f 等替换为捕获的值
    std::string result;
    result.reserve(256);

    size_t readOffset = 0;
    size_t argIndex = 0;

    for (const char* p = fmt; *p != '\0'; ++p) {
        if (*p == '%' && *(p + 1) != '\0' && *(p + 1) != '%') {
            // Format specifier found
            ++p; // Skip '%'

            if (argIndex >= m_argCount || readOffset >= m_offset) {
                result += '%';
                result += *p;
                continue;
            }

            // Read type tag
            TypeTag tag = static_cast<TypeTag>(m_buffer[readOffset]);
            ++readOffset;

            char buffer[64];
            switch (tag) {
                case TypeTag::Int32: {
                    int32_t value;
                    std::memcpy(&value, m_buffer.data() + readOffset, sizeof(int32_t));
                    readOffset += sizeof(int32_t);
                    std::snprintf(buffer, sizeof(buffer), "%d", value);
                    result += buffer;
                    break;
                }
                case TypeTag::Int64: {
                    int64_t value;
                    std::memcpy(&value, m_buffer.data() + readOffset, sizeof(int64_t));
                    readOffset += sizeof(int64_t);
                    std::snprintf(buffer, sizeof(buffer), "%lld", static_cast<long long>(value));
                    result += buffer;
                    break;
                }
                case TypeTag::UInt32: {
                    uint32_t value;
                    std::memcpy(&value, m_buffer.data() + readOffset, sizeof(uint32_t));
                    readOffset += sizeof(uint32_t);
                    std::snprintf(buffer, sizeof(buffer), "%u", value);
                    result += buffer;
                    break;
                }
                case TypeTag::UInt64: {
                    uint64_t value;
                    std::memcpy(&value, m_buffer.data() + readOffset, sizeof(uint64_t));
                    readOffset += sizeof(uint64_t);
                    std::snprintf(buffer, sizeof(buffer), "%llu", static_cast<unsigned long long>(value));
                    result += buffer;
                    break;
                }
                case TypeTag::Float: {
                    float value;
                    std::memcpy(&value, m_buffer.data() + readOffset, sizeof(float));
                    readOffset += sizeof(float);
                    std::snprintf(buffer, sizeof(buffer), "%f", value);
                    result += buffer;
                    break;
                }
                case TypeTag::Double: {
                    double value;
                    std::memcpy(&value, m_buffer.data() + readOffset, sizeof(double));
                    readOffset += sizeof(double);
                    std::snprintf(buffer, sizeof(buffer), "%f", value);
                    result += buffer;
                    break;
                }
                case TypeTag::Bool: {
                    uint8_t value;
                    std::memcpy(&value, m_buffer.data() + readOffset, sizeof(uint8_t));
                    readOffset += sizeof(uint8_t);
                    result += value ? "true" : "false";
                    break;
                }
                case TypeTag::StringView: {
                    uintptr_t ptr;
                    uint32_t length;
                    std::memcpy(&ptr, m_buffer.data() + readOffset, sizeof(uintptr_t));
                    readOffset += sizeof(uintptr_t);
                    std::memcpy(&length, m_buffer.data() + readOffset, sizeof(uint32_t));
                    readOffset += sizeof(uint32_t);
                    
                    if (ptr != 0 && length > 0) {
                        result.append(reinterpret_cast<const char*>(ptr), length);
                    }
                    break;
                }
                case TypeTag::StringCopy: {
                    uint32_t length;
                    std::memcpy(&length, m_buffer.data() + readOffset, sizeof(uint32_t));
                    readOffset += sizeof(uint32_t);
                    
                    if (length > 0 && readOffset + length <= m_offset) {
                        result.append(reinterpret_cast<const char*>(m_buffer.data() + readOffset), length);
                        readOffset += length;
                    }
                    break;
                }
                default:
                    result += "%?";
                    break;
            }

            ++argIndex;
        } else if (*p == '%' && *(p + 1) == '%') {
            // Escaped '%'
            result += '%';
            ++p;
        } else {
            result += *p;
        }
    }

    return result;
}

// Pointer Conversion / 指针转换

inline void BinarySnapshot::ConvertPointersToData() {
    // Create a temporary buffer for the converted data
    std::array<uint8_t, kDefaultBufferSize> tempBuffer{};
    size_t tempOffset = 0;
    size_t readOffset = 0;

    for (uint16_t i = 0; i < m_argCount && readOffset < m_offset; ++i) {
        // Read type tag
        TypeTag tag = static_cast<TypeTag>(m_buffer[readOffset]);

        if (tag == TypeTag::StringView) {
            // Convert StringView to StringCopy
            ++readOffset; // Skip tag

            uintptr_t ptr;
            uint32_t length;
            std::memcpy(&ptr, m_buffer.data() + readOffset, sizeof(uintptr_t));
            readOffset += sizeof(uintptr_t);
            std::memcpy(&length, m_buffer.data() + readOffset, sizeof(uint32_t));
            readOffset += sizeof(uint32_t);

            // Write StringCopy tag
            tempBuffer[tempOffset++] = static_cast<uint8_t>(TypeTag::StringCopy);

            // Write length
            std::memcpy(tempBuffer.data() + tempOffset, &length, sizeof(uint32_t));
            tempOffset += sizeof(uint32_t);

            // Copy string data from pointer
            if (ptr != 0 && length > 0 && tempOffset + length <= kDefaultBufferSize) {
                std::memcpy(tempBuffer.data() + tempOffset, reinterpret_cast<const char*>(ptr), length);
                tempOffset += length;
            }
        } else {
            // Copy other types as-is
            size_t typeSize = 0;

            switch (tag) {
                case TypeTag::Int32:
                case TypeTag::UInt32:
                case TypeTag::Float:
                    typeSize = 1 + sizeof(uint32_t);
                    break;
                case TypeTag::Int64:
                case TypeTag::UInt64:
                case TypeTag::Double:
                    typeSize = 1 + sizeof(uint64_t);
                    break;
                case TypeTag::Bool:
                    typeSize = 1 + sizeof(uint8_t);
                    break;
                case TypeTag::StringCopy: {
                    ++readOffset; // Skip tag
                    uint32_t length;
                    std::memcpy(&length, m_buffer.data() + readOffset, sizeof(uint32_t));
                    typeSize = sizeof(uint32_t) + length;
                    --readOffset; // Restore for copy
                    typeSize += 1; // Include tag
                    break;
                }
                default:
                    typeSize = 1;
                    break;
            }

            if (tempOffset + typeSize <= kDefaultBufferSize && readOffset + typeSize <= m_offset) {
                std::memcpy(tempBuffer.data() + tempOffset, m_buffer.data() + readOffset, typeSize);
                tempOffset += typeSize;
                readOffset += typeSize;
            } else {
                break;
            }
        }
    }

    // Replace buffer with converted data
    m_buffer = tempBuffer;
    m_offset = tempOffset;
}

// Query Methods / 查询方法

inline void BinarySnapshot::Reset() {
    m_offset = 0;
    m_argCount = 0;
    m_buffer.fill(0);
}

inline bool BinarySnapshot::operator==(const BinarySnapshot& other) const {
    if (m_argCount != other.m_argCount || m_offset != other.m_offset) {
        return false;
    }
    return std::memcmp(m_buffer.data(), other.m_buffer.data(), m_offset) == 0;
}

inline bool BinarySnapshot::operator!=(const BinarySnapshot& other) const {
    return !(*this == other);
}

// Helper Methods / 辅助方法

inline bool BinarySnapshot::WriteTag(TypeTag tag) {
    if (!HasSpace(sizeof(uint8_t))) {
        return false;
    }
    m_buffer[m_offset++] = static_cast<uint8_t>(tag);
    return true;
}

inline bool BinarySnapshot::WriteBytes(const void* data, size_t size) {
    if (!HasSpace(size)) {
        return false;
    }
    std::memcpy(m_buffer.data() + m_offset, data, size);
    m_offset += size;
    return true;
}

inline bool BinarySnapshot::HasSpace(size_t size) const {
    return m_offset + size <= kDefaultBufferSize;
}

} // namespace oneplog
