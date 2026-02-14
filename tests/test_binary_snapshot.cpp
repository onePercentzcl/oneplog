/**
 * @file test_binary_snapshot.cpp
 * @brief Unit tests for BinarySnapshot
 * @brief BinarySnapshot 的单元测试
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <gtest/gtest.h>
#ifdef ONEPLOG_HAS_RAPIDCHECK
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#endif

#include <oneplog/internal/binary_snapshot.hpp>

namespace oneplog {
namespace test {

// ==============================================================================
// Unit Tests / 单元测试
// ==============================================================================

/**
 * @brief Test default constructor
 * @brief 测试默认构造函数
 */
TEST(BinarySnapshotTest, DefaultConstructor) {
    BinarySnapshot snapshot;
    EXPECT_EQ(snapshot.ArgCount(), 0);
    EXPECT_EQ(snapshot.Offset(), BinarySnapshot::kHeaderSize);
    EXPECT_TRUE(snapshot.IsEmpty());
}

/**
 * @brief Test int32_t capture
 * @brief 测试 int32_t 捕获
 */
TEST(BinarySnapshotTest, CaptureInt32) {
    BinarySnapshot snapshot;
    EXPECT_TRUE(snapshot.CaptureInt32(42));
    EXPECT_EQ(snapshot.ArgCount(), 1);
    EXPECT_FALSE(snapshot.IsEmpty());
    
    // Format test / 格式化测试
    std::string result = snapshot.Format("Value: {}");
    EXPECT_EQ(result, "Value: 42");
}

/**
 * @brief Test int64_t capture
 * @brief 测试 int64_t 捕获
 */
TEST(BinarySnapshotTest, CaptureInt64) {
    BinarySnapshot snapshot;
    EXPECT_TRUE(snapshot.CaptureInt64(1234567890123LL));
    EXPECT_EQ(snapshot.ArgCount(), 1);
    
    std::string result = snapshot.Format("Value: {}");
    EXPECT_EQ(result, "Value: 1234567890123");
}

/**
 * @brief Test uint32_t capture
 * @brief 测试 uint32_t 捕获
 */
TEST(BinarySnapshotTest, CaptureUInt32) {
    BinarySnapshot snapshot;
    EXPECT_TRUE(snapshot.CaptureUInt32(4000000000U));
    EXPECT_EQ(snapshot.ArgCount(), 1);
    
    std::string result = snapshot.Format("Value: {}");
    EXPECT_EQ(result, "Value: 4000000000");
}

/**
 * @brief Test uint64_t capture
 * @brief 测试 uint64_t 捕获
 */
TEST(BinarySnapshotTest, CaptureUInt64) {
    BinarySnapshot snapshot;
    EXPECT_TRUE(snapshot.CaptureUInt64(18446744073709551615ULL));
    EXPECT_EQ(snapshot.ArgCount(), 1);
    
    std::string result = snapshot.Format("Value: {}");
    EXPECT_EQ(result, "Value: 18446744073709551615");
}

/**
 * @brief Test float capture
 * @brief 测试 float 捕获
 */
TEST(BinarySnapshotTest, CaptureFloat) {
    BinarySnapshot snapshot;
    EXPECT_TRUE(snapshot.CaptureFloat(3.14f));
    EXPECT_EQ(snapshot.ArgCount(), 1);
    
    std::string result = snapshot.Format("Value: {}");
    EXPECT_TRUE(result.find("3.14") != std::string::npos);
}

/**
 * @brief Test double capture
 * @brief 测试 double 捕获
 */
TEST(BinarySnapshotTest, CaptureDouble) {
    BinarySnapshot snapshot;
    EXPECT_TRUE(snapshot.CaptureDouble(3.14159265358979));
    EXPECT_EQ(snapshot.ArgCount(), 1);
    
    std::string result = snapshot.Format("Value: {}");
    EXPECT_TRUE(result.find("3.14159") != std::string::npos);
}

/**
 * @brief Test bool capture
 * @brief 测试 bool 捕获
 */
TEST(BinarySnapshotTest, CaptureBool) {
    BinarySnapshot snapshot;
    EXPECT_TRUE(snapshot.CaptureBool(true));
    EXPECT_TRUE(snapshot.CaptureBool(false));
    EXPECT_EQ(snapshot.ArgCount(), 2);
    
    std::string result = snapshot.Format("{} and {}");
    EXPECT_EQ(result, "true and false");
}

/**
 * @brief Test string_view capture (zero-copy)
 * @brief 测试 string_view 捕获（零拷贝）
 */
TEST(BinarySnapshotTest, CaptureStringView) {
    BinarySnapshot snapshot;
    std::string_view sv = "Hello, World!";
    EXPECT_TRUE(snapshot.CaptureStringView(sv));
    EXPECT_EQ(snapshot.ArgCount(), 1);
    
    std::string result = snapshot.Format("Message: {}");
    EXPECT_EQ(result, "Message: Hello, World!");
}

/**
 * @brief Test string capture (inline copy)
 * @brief 测试字符串捕获（内联拷贝）
 */
TEST(BinarySnapshotTest, CaptureString) {
    BinarySnapshot snapshot;
    std::string str = "Dynamic String";
    EXPECT_TRUE(snapshot.CaptureString(str));
    EXPECT_EQ(snapshot.ArgCount(), 1);
    
    std::string result = snapshot.Format("Message: {}");
    EXPECT_EQ(result, "Message: Dynamic String");
}

/**
 * @brief Test C-string capture
 * @brief 测试 C 字符串捕获
 */
TEST(BinarySnapshotTest, CaptureCString) {
    BinarySnapshot snapshot;
    const char* cstr = "C String";
    EXPECT_TRUE(snapshot.CaptureString(cstr));
    EXPECT_EQ(snapshot.ArgCount(), 1);
    
    std::string result = snapshot.Format("Message: {}");
    EXPECT_EQ(result, "Message: C String");
}

/**
 * @brief Test null C-string capture
 * @brief 测试空 C 字符串捕获
 */
TEST(BinarySnapshotTest, CaptureNullCString) {
    BinarySnapshot snapshot;
    const char* cstr = nullptr;
    EXPECT_TRUE(snapshot.CaptureString(cstr));
    EXPECT_EQ(snapshot.ArgCount(), 1);
    
    std::string result = snapshot.Format("Message: [{}]");
    EXPECT_EQ(result, "Message: []");
}

/**
 * @brief Test variadic capture
 * @brief 测试变参捕获
 */
TEST(BinarySnapshotTest, VariadicCapture) {
    BinarySnapshot snapshot;
    EXPECT_TRUE(snapshot.Capture(42, 3.14, true, "test"));
    EXPECT_EQ(snapshot.ArgCount(), 4);
    
    std::string result = snapshot.Format("{} {} {} {}");
    EXPECT_TRUE(result.find("42") != std::string::npos);
    EXPECT_TRUE(result.find("3.14") != std::string::npos);
    EXPECT_TRUE(result.find("true") != std::string::npos);
    EXPECT_TRUE(result.find("test") != std::string::npos);
}

/**
 * @brief Test multiple captures
 * @brief 测试多次捕获
 */
TEST(BinarySnapshotTest, MultipleCaptures) {
    BinarySnapshot snapshot;
    EXPECT_TRUE(snapshot.CaptureInt32(1));
    EXPECT_TRUE(snapshot.CaptureInt32(2));
    EXPECT_TRUE(snapshot.CaptureInt32(3));
    EXPECT_EQ(snapshot.ArgCount(), 3);
    
    std::string result = snapshot.Format("{}, {}, {}");
    EXPECT_EQ(result, "1, 2, 3");
}

/**
 * @brief Test reset
 * @brief 测试重置
 */
TEST(BinarySnapshotTest, Reset) {
    BinarySnapshot snapshot;
    snapshot.CaptureInt32(42);
    EXPECT_EQ(snapshot.ArgCount(), 1);
    
    snapshot.Reset();
    EXPECT_EQ(snapshot.ArgCount(), 0);
    EXPECT_TRUE(snapshot.IsEmpty());
}

/**
 * @brief Test serialization round-trip
 * @brief 测试序列化往返
 */
TEST(BinarySnapshotTest, SerializationRoundTrip) {
    BinarySnapshot original;
    original.CaptureInt32(42);
    original.CaptureString("test");
    original.CaptureBool(true);
    
    // Serialize / 序列化
    std::vector<uint8_t> buffer(original.SerializedSize());
    original.SerializeTo(buffer.data());
    
    // Deserialize / 反序列化
    BinarySnapshot restored = BinarySnapshot::Deserialize(buffer.data(), buffer.size());
    
    EXPECT_EQ(original.ArgCount(), restored.ArgCount());
    EXPECT_EQ(original, restored);
}

/**
 * @brief Test ConvertPointersToData
 * @brief 测试指针转换
 */
TEST(BinarySnapshotTest, ConvertPointersToData) {
    BinarySnapshot snapshot;
    std::string_view sv = "Static String";
    snapshot.CaptureStringView(sv);
    snapshot.CaptureInt32(42);
    
    // Before conversion, StringView stores pointer
    // 转换前，StringView 存储指针
    EXPECT_EQ(snapshot.ArgCount(), 2);
    
    // Convert pointers to data / 转换指针为数据
    snapshot.ConvertPointersToData();
    
    // After conversion, should still format correctly
    // 转换后，应该仍然能正确格式化
    EXPECT_EQ(snapshot.ArgCount(), 2);
    std::string result = snapshot.Format("{} {}");
    EXPECT_EQ(result, "Static String 42");
}

/**
 * @brief Test buffer overflow protection
 * @brief 测试缓冲区溢出保护
 */
TEST(BinarySnapshotTest, BufferOverflowProtection) {
    BinarySnapshot snapshot;
    
    // Fill buffer with strings until it's full
    // 用字符串填充缓冲区直到满
    std::string longStr(100, 'x');
    int count = 0;
    while (snapshot.CaptureString(longStr)) {
        ++count;
        if (count > 10) break;  // Safety limit / 安全限制
    }
    
    // Should have captured at least one / 应该至少捕获了一个
    EXPECT_GT(snapshot.ArgCount(), 0);
    // Should have stopped before overflow / 应该在溢出前停止
    EXPECT_LE(snapshot.Offset(), BinarySnapshot::Capacity());
}

/**
 * @brief Test copy constructor
 * @brief 测试拷贝构造函数
 */
TEST(BinarySnapshotTest, CopyConstructor) {
    BinarySnapshot original;
    original.CaptureInt32(42);
    original.CaptureString("test");
    
    BinarySnapshot copy(original);
    EXPECT_EQ(original, copy);
    EXPECT_EQ(copy.ArgCount(), 2);
}

/**
 * @brief Test move constructor
 * @brief 测试移动构造函数
 */
TEST(BinarySnapshotTest, MoveConstructor) {
    BinarySnapshot original;
    original.CaptureInt32(42);
    original.CaptureString("test");
    uint16_t argCount = original.ArgCount();
    
    BinarySnapshot moved(std::move(original));
    EXPECT_EQ(moved.ArgCount(), argCount);
}

/**
 * @brief Test equality operator
 * @brief 测试相等运算符
 */
TEST(BinarySnapshotTest, EqualityOperator) {
    BinarySnapshot a, b;
    a.CaptureInt32(42);
    b.CaptureInt32(42);
    EXPECT_EQ(a, b);
    
    BinarySnapshot c;
    c.CaptureInt32(43);
    EXPECT_NE(a, c);
}

// ==============================================================================
// Property-Based Tests / 属性测试
// ==============================================================================

#ifdef ONEPLOG_HAS_RAPIDCHECK

/**
 * @brief Property 4: BinarySnapshot type capture completeness
 * @brief 属性 4：BinarySnapshot 类型捕获完整性
 *
 * For any supported type value, BinarySnapshot's Capture operation should
 * successfully capture the value, and Format operation should correctly
 * restore the value.
 *
 * 对于任意支持的类型值，BinarySnapshot 的 Capture 操作应该成功捕获该值，
 * 且 Format 操作应该能够正确还原该值。
 *
 * **Validates: Requirements 5.1, 5.2, 5.3**
 */
RC_GTEST_PROP(BinarySnapshotPropertyTest, TypeCaptureCompleteness, ()) {
    // Test int32_t / 测试 int32_t
    {
        auto value = *rc::gen::arbitrary<int32_t>();
        BinarySnapshot snapshot;
        RC_ASSERT(snapshot.CaptureInt32(value));
        RC_ASSERT(snapshot.ArgCount() == 1);
        std::string result = snapshot.Format("{}");
        RC_ASSERT(result == std::to_string(value));
    }
    
    // Test int64_t / 测试 int64_t
    {
        auto value = *rc::gen::arbitrary<int64_t>();
        BinarySnapshot snapshot;
        RC_ASSERT(snapshot.CaptureInt64(value));
        RC_ASSERT(snapshot.ArgCount() == 1);
        std::string result = snapshot.Format("{}");
        RC_ASSERT(result == std::to_string(value));
    }
    
    // Test bool / 测试 bool
    {
        auto value = *rc::gen::arbitrary<bool>();
        BinarySnapshot snapshot;
        RC_ASSERT(snapshot.CaptureBool(value));
        RC_ASSERT(snapshot.ArgCount() == 1);
        std::string result = snapshot.Format("{}");
        RC_ASSERT(result == (value ? "true" : "false"));
    }
}

/**
 * @brief Property 5: BinarySnapshot serialization round-trip consistency
 * @brief 属性 5：BinarySnapshot 序列化往返一致性
 *
 * For any valid BinarySnapshot object S, Deserialize(Serialize(S)) should
 * produce an object equivalent to S.
 *
 * 对于任意有效的 BinarySnapshot 对象 S，Deserialize(Serialize(S)) 应该
 * 产生与 S 等价的对象。
 *
 * **Validates: Requirements 5.6**
 */
RC_GTEST_PROP(BinarySnapshotPropertyTest, SerializationRoundTrip, ()) {
    BinarySnapshot original;
    
    // Generate random number of arguments / 生成随机数量的参数
    auto argCount = *rc::gen::inRange(0, 5);
    for (int i = 0; i < argCount; ++i) {
        auto typeChoice = *rc::gen::inRange(0, 4);
        switch (typeChoice) {
            case 0: {
                auto val = *rc::gen::arbitrary<int32_t>();
                original.CaptureInt32(val);
                break;
            }
            case 1: {
                auto val = *rc::gen::arbitrary<bool>();
                original.CaptureBool(val);
                break;
            }
            case 2: {
                auto val = *rc::gen::arbitrary<double>();
                original.CaptureDouble(val);
                break;
            }
            case 3: {
                // Generate short string to avoid buffer overflow
                // 生成短字符串以避免缓冲区溢出
                auto len = *rc::gen::inRange(0, 20);
                std::string str(len, 'x');
                original.CaptureString(str);
                break;
            }
        }
    }
    
    // Serialize / 序列化
    std::vector<uint8_t> buffer(original.SerializedSize());
    original.SerializeTo(buffer.data());
    
    // Deserialize / 反序列化
    BinarySnapshot restored = BinarySnapshot::Deserialize(buffer.data(), buffer.size());
    
    // Verify equality / 验证相等
    RC_ASSERT(original == restored);
}

/**
 * @brief Property: Capture never exceeds buffer capacity
 * @brief 属性：捕获永远不会超过缓冲区容量
 */
RC_GTEST_PROP(BinarySnapshotPropertyTest, NeverExceedsCapacity, ()) {
    BinarySnapshot snapshot;
    
    // Try to capture many values / 尝试捕获多个值
    auto count = *rc::gen::inRange(0, 50);
    for (int i = 0; i < count; ++i) {
        auto typeChoice = *rc::gen::inRange(0, 3);
        switch (typeChoice) {
            case 0:
                snapshot.CaptureInt32(*rc::gen::arbitrary<int32_t>());
                break;
            case 1:
                snapshot.CaptureBool(*rc::gen::arbitrary<bool>());
                break;
            case 2: {
                auto len = *rc::gen::inRange(0, 30);
                std::string str(len, 'a');
                snapshot.CaptureString(str);
                break;
            }
        }
    }
    
    // Buffer should never exceed capacity / 缓冲区永远不应超过容量
    RC_ASSERT(snapshot.Offset() <= BinarySnapshot::Capacity());
}

#endif  // ONEPLOG_HAS_RAPIDCHECK

}  // namespace test
}  // namespace oneplog
