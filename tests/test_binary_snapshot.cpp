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
// Multi-Parameter Capture Verification Tests / 多参数捕获验证测试
// ==============================================================================

/**
 * @brief Test multi-parameter capture with Capture() variadic template
 * @brief 测试使用 Capture() 变参模板的多参数捕获
 *
 * Verifies that Capture() correctly handles multiple parameters and
 * maintains parameter order.
 * 验证 Capture() 正确处理多个参数并保持参数顺序。
 *
 * _Requirements: 6.4_
 */
TEST(BinarySnapshotTest, MultiParameterCaptureOrder) {
    BinarySnapshot snapshot;
    
    // Capture multiple parameters of different types
    // 捕获多个不同类型的参数
    EXPECT_TRUE(snapshot.Capture(100, 200, 300));
    EXPECT_EQ(snapshot.ArgCount(), 3);
    
    // Verify order is preserved / 验证顺序保持
    std::string result = snapshot.Format("{} {} {}");
    EXPECT_EQ(result, "100 200 300");
}

/**
 * @brief Test multi-parameter capture with mixed types
 * @brief 测试混合类型的多参数捕获
 *
 * _Requirements: 6.4_
 */
TEST(BinarySnapshotTest, MultiParameterCaptureMixedTypes) {
    BinarySnapshot snapshot;
    
    // Capture mixed types / 捕获混合类型
    EXPECT_TRUE(snapshot.Capture(42, 3.14, true, "hello"));
    EXPECT_EQ(snapshot.ArgCount(), 4);
    
    // Verify all parameters are captured in order
    // 验证所有参数按顺序捕获
    std::string result = snapshot.Format("{} {} {} {}");
    EXPECT_TRUE(result.find("42") != std::string::npos);
    EXPECT_TRUE(result.find("3.14") != std::string::npos);
    EXPECT_TRUE(result.find("true") != std::string::npos);
    EXPECT_TRUE(result.find("hello") != std::string::npos);
    
    // Verify order by checking positions / 通过检查位置验证顺序
    size_t pos42 = result.find("42");
    size_t pos314 = result.find("3.14");
    size_t posTrue = result.find("true");
    size_t posHello = result.find("hello");
    EXPECT_LT(pos42, pos314);
    EXPECT_LT(pos314, posTrue);
    EXPECT_LT(posTrue, posHello);
}

/**
 * @brief Test FormatAll with multiple parameters
 * @brief 测试 FormatAll 多参数格式化
 *
 * Verifies that FormatAll correctly replaces all {} placeholders.
 * 验证 FormatAll 正确替换所有 {} 占位符。
 *
 * _Requirements: 6.5_
 */
TEST(BinarySnapshotTest, FormatAllMultipleParameters) {
    BinarySnapshot snapshot;
    
    // First capture format string, then parameters
    // 首先捕获格式字符串，然后是参数
    snapshot.CaptureStringView("Value: {} and {} and {}");
    snapshot.CaptureInt32(1);
    snapshot.CaptureInt32(2);
    snapshot.CaptureInt32(3);
    
    EXPECT_EQ(snapshot.ArgCount(), 4);
    
    // FormatAll should use first arg as format string
    // FormatAll 应该使用第一个参数作为格式字符串
    std::string result = snapshot.FormatAll();
    EXPECT_EQ(result, "Value: 1 and 2 and 3");
}

/**
 * @brief Test FormatAll with mixed type parameters
 * @brief 测试 FormatAll 混合类型参数
 *
 * _Requirements: 6.5_
 */
TEST(BinarySnapshotTest, FormatAllMixedTypes) {
    BinarySnapshot snapshot;
    
    snapshot.CaptureStringView("int={} bool={} str={}");
    snapshot.CaptureInt32(42);
    snapshot.CaptureBool(true);
    snapshot.CaptureString("test");
    
    std::string result = snapshot.FormatAll();
    EXPECT_EQ(result, "int=42 bool=true str=test");
}

/**
 * @brief Test FormatAll with more placeholders than arguments
 * @brief 测试 FormatAll 占位符多于参数的情况
 *
 * _Requirements: 6.5_
 */
TEST(BinarySnapshotTest, FormatAllMorePlaceholdersThanArgs) {
    BinarySnapshot snapshot;
    
    snapshot.CaptureStringView("{} {} {} {}");  // 4 placeholders
    snapshot.CaptureInt32(1);
    snapshot.CaptureInt32(2);  // Only 2 arguments
    
    // Extra placeholders are skipped (not replaced)
    // 额外的占位符被跳过（不替换）
    std::string result = snapshot.FormatAll();
    // The implementation skips {} when no more args available
    // 当没有更多参数时，实现会跳过 {}
    EXPECT_EQ(result, "1 2  ");
}

/**
 * @brief Test FormatAll with fewer placeholders than arguments
 * @brief 测试 FormatAll 占位符少于参数的情况
 *
 * _Requirements: 6.5_
 */
TEST(BinarySnapshotTest, FormatAllFewerPlaceholdersThanArgs) {
    BinarySnapshot snapshot;
    
    snapshot.CaptureStringView("{} {}");  // 2 placeholders
    snapshot.CaptureInt32(1);
    snapshot.CaptureInt32(2);
    snapshot.CaptureInt32(3);  // 3 arguments, extra one ignored
    
    std::string result = snapshot.FormatAll();
    EXPECT_EQ(result, "1 2");
}

/**
 * @brief Test FormatAll with no placeholders
 * @brief 测试 FormatAll 无占位符的情况
 *
 * _Requirements: 6.5_
 */
TEST(BinarySnapshotTest, FormatAllNoPlaceholders) {
    BinarySnapshot snapshot;
    
    snapshot.CaptureStringView("No placeholders here");
    snapshot.CaptureInt32(42);  // This will be ignored
    
    std::string result = snapshot.FormatAll();
    EXPECT_EQ(result, "No placeholders here");
}

/**
 * @brief Test FormatAll with only format string (no args)
 * @brief 测试 FormatAll 只有格式字符串（无参数）
 *
 * _Requirements: 6.5_
 */
TEST(BinarySnapshotTest, FormatAllOnlyFormatString) {
    BinarySnapshot snapshot;
    
    snapshot.CaptureStringView("Just a message");
    
    std::string result = snapshot.FormatAll();
    EXPECT_EQ(result, "Just a message");
}

/**
 * @brief Test multi-parameter capture with 5+ parameters
 * @brief 测试 5 个以上参数的多参数捕获
 *
 * _Requirements: 6.4_
 */
TEST(BinarySnapshotTest, MultiParameterCaptureFiveOrMore) {
    BinarySnapshot snapshot;
    
    EXPECT_TRUE(snapshot.Capture(1, 2, 3, 4, 5, 6));
    EXPECT_EQ(snapshot.ArgCount(), 6);
    
    std::string result = snapshot.Format("{}-{}-{}-{}-{}-{}");
    EXPECT_EQ(result, "1-2-3-4-5-6");
}

/**
 * @brief Test FormatAll with StringCopy format string
 * @brief 测试 FormatAll 使用 StringCopy 格式字符串
 *
 * _Requirements: 6.5_
 */
TEST(BinarySnapshotTest, FormatAllWithStringCopyFormat) {
    BinarySnapshot snapshot;
    
    // Use CaptureString (StringCopy) instead of CaptureStringView
    // 使用 CaptureString (StringCopy) 而不是 CaptureStringView
    std::string fmtStr = "Value: {} and {}";
    snapshot.CaptureString(fmtStr);
    snapshot.CaptureInt32(100);
    snapshot.CaptureInt32(200);
    
    std::string result = snapshot.FormatAll();
    EXPECT_EQ(result, "Value: 100 and 200");
}

/**
 * @brief Test FormatAll type conversion for all supported types
 * @brief 测试 FormatAll 对所有支持类型的类型转换
 *
 * Verifies that all {} placeholders are correctly replaced with
 * properly converted type values.
 * 验证所有 {} 占位符都被正确替换为正确转换的类型值。
 *
 * _Requirements: 6.5_
 */
TEST(BinarySnapshotTest, FormatAllTypeConversion) {
    BinarySnapshot snapshot;
    
    snapshot.CaptureStringView("i32={} i64={} u32={} u64={} f={} d={} b={}");
    snapshot.CaptureInt32(-42);
    snapshot.CaptureInt64(-9876543210LL);
    snapshot.CaptureUInt32(4000000000U);
    snapshot.CaptureUInt64(18446744073709551615ULL);
    snapshot.CaptureFloat(3.14f);
    snapshot.CaptureDouble(2.71828);
    snapshot.CaptureBool(false);
    
    std::string result = snapshot.FormatAll();
    
    // Verify each type is correctly converted
    // 验证每种类型都被正确转换
    EXPECT_TRUE(result.find("i32=-42") != std::string::npos);
    EXPECT_TRUE(result.find("i64=-9876543210") != std::string::npos);
    EXPECT_TRUE(result.find("u32=4000000000") != std::string::npos);
    EXPECT_TRUE(result.find("u64=18446744073709551615") != std::string::npos);
    EXPECT_TRUE(result.find("f=3.14") != std::string::npos);
    EXPECT_TRUE(result.find("d=2.71") != std::string::npos);
    EXPECT_TRUE(result.find("b=false") != std::string::npos);
}

/**
 * @brief Test FormatAll with string arguments
 * @brief 测试 FormatAll 字符串参数
 *
 * _Requirements: 6.5_
 */
TEST(BinarySnapshotTest, FormatAllStringArguments) {
    BinarySnapshot snapshot;
    
    snapshot.CaptureStringView("name={} msg={}");
    snapshot.CaptureString("Alice");  // StringCopy
    std::string_view sv = "Hello World";
    snapshot.CaptureStringView(sv);   // StringView
    
    std::string result = snapshot.FormatAll();
    EXPECT_EQ(result, "name=Alice msg=Hello World");
}

/**
 * @brief Test FormatAll placeholder replacement completeness
 * @brief 测试 FormatAll 占位符替换完整性
 *
 * Verifies that ALL {} placeholders are replaced when enough arguments exist.
 * 验证当有足够参数时，所有 {} 占位符都被替换。
 *
 * _Requirements: 6.5_
 */
TEST(BinarySnapshotTest, FormatAllPlaceholderCompleteness) {
    BinarySnapshot snapshot;
    
    // 10 placeholders with 10 arguments
    // 10 个占位符和 10 个参数
    snapshot.CaptureStringView("{}{}{}{}{}{}{}{}{}{}");
    for (int i = 0; i < 10; ++i) {
        snapshot.CaptureInt32(i);
    }
    
    std::string result = snapshot.FormatAll();
    EXPECT_EQ(result, "0123456789");
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

// ==============================================================================
// Task 9.1: String Ownership Verification Tests / 字符串所有权验证测试
// ==============================================================================

/**
 * @brief Test that std::string is captured with StringCopy (inline copy)
 * @brief 测试 std::string 使用 StringCopy（内联拷贝）捕获
 *
 * Verifies that dynamic strings are copied into the snapshot buffer,
 * not just referenced by pointer.
 *
 * 验证动态字符串被拷贝到快照缓冲区中，而不仅仅是通过指针引用。
 *
 * _Requirements: 8.2, 8.3_
 */
TEST(BinarySnapshotOwnershipTest, StdStringIsCopied) {
    BinarySnapshot snapshot;
    
    {
        std::string dynamicStr = "This string will be destroyed";
        EXPECT_TRUE(snapshot.CaptureString(dynamicStr));
        // dynamicStr destroyed here
    }
    
    // Format should still work because data was copied
    std::string result = snapshot.Format("{}");
    EXPECT_EQ(result, "This string will be destroyed");
}

/**
 * @brief Test that const char* is captured with StringCopy (inline copy)
 * @brief 测试 const char* 使用 StringCopy（内联拷贝）捕获
 *
 * _Requirements: 8.2, 8.3_
 */
TEST(BinarySnapshotOwnershipTest, CharPointerIsCopied) {
    BinarySnapshot snapshot;
    
    {
        char buffer[64] = "Temporary buffer";
        EXPECT_TRUE(snapshot.CaptureString(buffer));
        // Modify buffer to verify copy was made
        std::memset(buffer, 'X', sizeof(buffer) - 1);
    }
    
    // Format should return original content
    std::string result = snapshot.Format("{}");
    EXPECT_EQ(result, "Temporary buffer");
}

/**
 * @brief Test that Capture() variadic template uses correct capture method
 * @brief 测试 Capture() 变参模板使用正确的捕获方法
 *
 * Verifies that std::string passed through Capture() is copied.
 *
 * 验证通过 Capture() 传递的 std::string 被拷贝。
 *
 * _Requirements: 8.2, 8.3_
 */
TEST(BinarySnapshotOwnershipTest, CaptureVariadicCopiesStrings) {
    BinarySnapshot snapshot;
    
    {
        std::string str1 = "First string";
        std::string str2 = "Second string";
        EXPECT_TRUE(snapshot.Capture(str1, 42, str2));
        // Strings destroyed here
    }
    
    // Format should work correctly
    std::string result = snapshot.Format("{} {} {}");
    EXPECT_EQ(result, "First string 42 Second string");
}

/**
 * @brief Test string ownership after ConvertPointersToData
 * @brief 测试 ConvertPointersToData 后的字符串所有权
 *
 * Verifies that StringView pointers are converted to StringCopy.
 *
 * 验证 StringView 指针被转换为 StringCopy。
 *
 * _Requirements: 8.2, 8.3_
 */
TEST(BinarySnapshotOwnershipTest, ConvertPointersPreservesData) {
    BinarySnapshot snapshot;
    
    {
        std::string_view sv = "Static string view";
        snapshot.CaptureStringView(sv);
        // sv still valid here
    }
    
    // Convert pointers to data (simulates cross-process transfer)
    snapshot.ConvertPointersToData();
    
    // Format should still work after conversion
    std::string result = snapshot.Format("{}");
    EXPECT_EQ(result, "Static string view");
}

/**
 * @brief Test FormatAll with destroyed source strings
 * @brief 测试源字符串销毁后的 FormatAll
 *
 * Verifies that FormatAll works correctly even after source strings
 * are destroyed, because StringCopy was used.
 *
 * 验证即使源字符串被销毁后 FormatAll 仍能正确工作，因为使用了 StringCopy。
 *
 * _Requirements: 8.2, 8.3_
 */
TEST(BinarySnapshotOwnershipTest, FormatAllWithDestroyedStrings) {
    BinarySnapshot snapshot;
    
    {
        std::string fmtStr = "Value: {} Name: {}";
        std::string nameStr = "TestName";
        
        // Use CaptureString for format string (StringCopy)
        snapshot.CaptureString(fmtStr);
        snapshot.CaptureInt32(42);
        snapshot.CaptureString(nameStr);
        // Strings destroyed here
    }
    
    // FormatAll should work correctly
    std::string result = snapshot.FormatAll();
    EXPECT_EQ(result, "Value: 42 Name: TestName");
}

}  // namespace test
}  // namespace oneplog
