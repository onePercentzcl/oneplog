/**
 * @file binary_snapshot_test.cpp
 * @brief Unit tests for BinarySnapshot class
 * @文件 binary_snapshot_test.cpp
 * @简述 BinarySnapshot 类的单元测试
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include "oneplog/binary_snapshot.hpp"
#include <gtest/gtest.h>
#include <string>

using namespace oneplog;

// ==============================================================================
// Basic Type Capture Tests / 基本类型捕获测试
// ==============================================================================

TEST(BinarySnapshotTest, CaptureInt32) {
    BinarySnapshot snapshot;
    EXPECT_TRUE(snapshot.CaptureInt32(42));
    EXPECT_EQ(snapshot.GetArgCount(), 1);
    EXPECT_FALSE(snapshot.IsEmpty());

    std::string result = snapshot.Format("%d");
    EXPECT_EQ(result, "42");
}

TEST(BinarySnapshotTest, CaptureInt64) {
    BinarySnapshot snapshot;
    EXPECT_TRUE(snapshot.CaptureInt64(9223372036854775807LL));
    EXPECT_EQ(snapshot.GetArgCount(), 1);

    std::string result = snapshot.Format("%d");
    EXPECT_TRUE(result.find("9223372036854775807") != std::string::npos);
}

TEST(BinarySnapshotTest, CaptureUInt32) {
    BinarySnapshot snapshot;
    EXPECT_TRUE(snapshot.CaptureUInt32(4294967295U));
    EXPECT_EQ(snapshot.GetArgCount(), 1);

    std::string result = snapshot.Format("%u");
    EXPECT_EQ(result, "4294967295");
}

TEST(BinarySnapshotTest, CaptureUInt64) {
    BinarySnapshot snapshot;
    EXPECT_TRUE(snapshot.CaptureUInt64(18446744073709551615ULL));
    EXPECT_EQ(snapshot.GetArgCount(), 1);
}

TEST(BinarySnapshotTest, CaptureFloat) {
    BinarySnapshot snapshot;
    EXPECT_TRUE(snapshot.CaptureFloat(3.14f));
    EXPECT_EQ(snapshot.GetArgCount(), 1);

    std::string result = snapshot.Format("%f");
    EXPECT_TRUE(result.find("3.14") != std::string::npos);
}

TEST(BinarySnapshotTest, CaptureDouble) {
    BinarySnapshot snapshot;
    EXPECT_TRUE(snapshot.CaptureDouble(2.718281828));
    EXPECT_EQ(snapshot.GetArgCount(), 1);

    std::string result = snapshot.Format("%f");
    EXPECT_TRUE(result.find("2.718") != std::string::npos);
}

TEST(BinarySnapshotTest, CaptureBool) {
    BinarySnapshot snapshot;
    EXPECT_TRUE(snapshot.CaptureBool(true));
    EXPECT_EQ(snapshot.GetArgCount(), 1);

    std::string result = snapshot.Format("%s");
    EXPECT_EQ(result, "true");

    snapshot.Reset();
    EXPECT_TRUE(snapshot.CaptureBool(false));
    result = snapshot.Format("%s");
    EXPECT_EQ(result, "false");
}

// ==============================================================================
// String Capture Tests / 字符串捕获测试
// ==============================================================================

TEST(BinarySnapshotTest, CaptureStringView) {
    BinarySnapshot snapshot;
    std::string_view sv = "Hello, World!";
    EXPECT_TRUE(snapshot.CaptureStringView(sv));
    EXPECT_EQ(snapshot.GetArgCount(), 1);

    std::string result = snapshot.Format("%s");
    EXPECT_EQ(result, "Hello, World!");
}

TEST(BinarySnapshotTest, CaptureStdString) {
    BinarySnapshot snapshot;
    std::string str = "Dynamic String";
    EXPECT_TRUE(snapshot.CaptureString(str));
    EXPECT_EQ(snapshot.GetArgCount(), 1);

    std::string result = snapshot.Format("%s");
    EXPECT_EQ(result, "Dynamic String");
}

TEST(BinarySnapshotTest, CaptureCString) {
    BinarySnapshot snapshot;
    const char* cstr = "C-Style String";
    EXPECT_TRUE(snapshot.CaptureString(cstr));
    EXPECT_EQ(snapshot.GetArgCount(), 1);

    std::string result = snapshot.Format("%s");
    EXPECT_EQ(result, "C-Style String");
}

TEST(BinarySnapshotTest, CaptureEmptyString) {
    BinarySnapshot snapshot;
    EXPECT_TRUE(snapshot.CaptureString(""));
    EXPECT_EQ(snapshot.GetArgCount(), 1);

    std::string result = snapshot.Format("%s");
    EXPECT_EQ(result, "");
}

// ==============================================================================
// Variadic Template Tests / 变参模板测试
// ==============================================================================

TEST(BinarySnapshotTest, CaptureMultipleArgs) {
    BinarySnapshot snapshot;
    EXPECT_TRUE(snapshot.Capture(42, 3.14, "test", true));
    EXPECT_EQ(snapshot.GetArgCount(), 4);

    std::string result = snapshot.Format("%d %f %s %s");
    EXPECT_TRUE(result.find("42") != std::string::npos);
    EXPECT_TRUE(result.find("3.14") != std::string::npos);
    EXPECT_TRUE(result.find("test") != std::string::npos);
    EXPECT_TRUE(result.find("true") != std::string::npos);
}

TEST(BinarySnapshotTest, CaptureMixedTypes) {
    BinarySnapshot snapshot;
    int32_t i32 = 100;
    int64_t i64 = 200L;
    float f = 1.5f;
    double d = 2.5;
    bool b = true;
    std::string str = "mixed";

    EXPECT_TRUE(snapshot.Capture(i32, i64, f, d, b, str));
    EXPECT_EQ(snapshot.GetArgCount(), 6);
}

// ==============================================================================
// Serialization Tests / 序列化测试
// ==============================================================================

TEST(BinarySnapshotTest, SerializeDeserialize) {
    BinarySnapshot original;
    EXPECT_TRUE(original.Capture(42, 3.14, "test"));

    size_t size = original.SerializedSize();
    EXPECT_GT(size, 0);

    std::vector<uint8_t> buffer(size);
    EXPECT_TRUE(original.SerializeTo(buffer.data()));

    BinarySnapshot deserialized = BinarySnapshot::Deserialize(buffer.data(), size);
    EXPECT_EQ(deserialized.GetArgCount(), original.GetArgCount());
    EXPECT_EQ(deserialized, original);
}

TEST(BinarySnapshotTest, SerializeEmpty) {
    BinarySnapshot snapshot;
    size_t size = snapshot.SerializedSize();
    EXPECT_GT(size, 0);

    std::vector<uint8_t> buffer(size);
    EXPECT_TRUE(snapshot.SerializeTo(buffer.data()));

    BinarySnapshot deserialized = BinarySnapshot::Deserialize(buffer.data(), size);
    EXPECT_EQ(deserialized.GetArgCount(), 0);
    EXPECT_TRUE(deserialized.IsEmpty());
}

TEST(BinarySnapshotTest, DeserializeInvalidData) {
    uint8_t invalidData[] = {0xFF, 0xFF};
    BinarySnapshot snapshot = BinarySnapshot::Deserialize(invalidData, sizeof(invalidData));
    EXPECT_TRUE(snapshot.IsEmpty());
}

// ==============================================================================
// Format Tests / 格式化测试
// ==============================================================================

TEST(BinarySnapshotTest, FormatWithPlaceholders) {
    BinarySnapshot snapshot;
    EXPECT_TRUE(snapshot.Capture(42, "world"));

    std::string result = snapshot.Format("Hello %d %s!");
    EXPECT_EQ(result, "Hello 42 world!");
}

TEST(BinarySnapshotTest, FormatWithoutPlaceholders) {
    BinarySnapshot snapshot;
    EXPECT_TRUE(snapshot.Capture(42));

    std::string result = snapshot.Format("No placeholders here");
    EXPECT_EQ(result, "No placeholders here");
}

TEST(BinarySnapshotTest, FormatEscapedPercent) {
    BinarySnapshot snapshot;
    EXPECT_TRUE(snapshot.Capture(100));

    std::string result = snapshot.Format("Progress: %d%%");
    EXPECT_EQ(result, "Progress: 100%");
}

TEST(BinarySnapshotTest, FormatEmpty) {
    BinarySnapshot snapshot;
    std::string result = snapshot.Format("Empty snapshot");
    EXPECT_EQ(result, "Empty snapshot");
}

// ==============================================================================
// Pointer Conversion Tests / 指针转换测试
// ==============================================================================

TEST(BinarySnapshotTest, ConvertPointersToData) {
    BinarySnapshot snapshot;
    std::string_view sv = "static string";
    EXPECT_TRUE(snapshot.CaptureStringView(sv));

    // Before conversion, StringView stores pointer
    size_t offsetBefore = snapshot.GetOffset();

    // Convert pointers to data
    snapshot.ConvertPointersToData();

    // After conversion, data should be inline
    size_t offsetAfter = snapshot.GetOffset();
    EXPECT_GT(offsetAfter, offsetBefore);

    // Format should still work
    std::string result = snapshot.Format("%s");
    EXPECT_EQ(result, "static string");
}

TEST(BinarySnapshotTest, ConvertMixedData) {
    BinarySnapshot snapshot;
    std::string_view sv = "view";
    std::string str = "copy";
    EXPECT_TRUE(snapshot.Capture(42, sv, str, 3.14));

    snapshot.ConvertPointersToData();

    std::string result = snapshot.Format("%d %s %s %f");
    EXPECT_TRUE(result.find("42") != std::string::npos);
    EXPECT_TRUE(result.find("view") != std::string::npos);
    EXPECT_TRUE(result.find("copy") != std::string::npos);
    EXPECT_TRUE(result.find("3.14") != std::string::npos);
}

// ==============================================================================
// Edge Cases / 边界情况测试
// ==============================================================================

TEST(BinarySnapshotTest, MaxArguments) {
    BinarySnapshot snapshot;
    for (uint16_t i = 0; i < BinarySnapshot::kMaxArgCount; ++i) {
        EXPECT_TRUE(snapshot.CaptureInt32(static_cast<int32_t>(i)));
    }
    EXPECT_EQ(snapshot.GetArgCount(), BinarySnapshot::kMaxArgCount);

    // Should fail to add more
    EXPECT_FALSE(snapshot.CaptureInt32(999));
    EXPECT_EQ(snapshot.GetArgCount(), BinarySnapshot::kMaxArgCount);
}

TEST(BinarySnapshotTest, BufferOverflow) {
    BinarySnapshot snapshot;
    // Try to fill buffer with large strings
    std::string largeString(100, 'X');
    
    bool success = true;
    int count = 0;
    while (success && count < 10) {
        success = snapshot.CaptureString(largeString);
        if (success) {
            ++count;
        }
    }

    // Should eventually fail due to buffer limit
    EXPECT_LT(count, 10);
}

TEST(BinarySnapshotTest, Reset) {
    BinarySnapshot snapshot;
    EXPECT_TRUE(snapshot.Capture(42, "test", 3.14));
    EXPECT_EQ(snapshot.GetArgCount(), 3);
    EXPECT_FALSE(snapshot.IsEmpty());

    snapshot.Reset();
    EXPECT_EQ(snapshot.GetArgCount(), 0);
    EXPECT_TRUE(snapshot.IsEmpty());
    EXPECT_EQ(snapshot.GetOffset(), 0);
}

TEST(BinarySnapshotTest, CopyConstructor) {
    BinarySnapshot original;
    EXPECT_TRUE(original.Capture(42, "test"));

    BinarySnapshot copy(original);
    EXPECT_EQ(copy.GetArgCount(), original.GetArgCount());
    EXPECT_EQ(copy, original);

    std::string result = copy.Format("%d %s");
    EXPECT_TRUE(result.find("42") != std::string::npos);
    EXPECT_TRUE(result.find("test") != std::string::npos);
}

TEST(BinarySnapshotTest, MoveConstructor) {
    BinarySnapshot original;
    EXPECT_TRUE(original.Capture(42, "test"));
    uint16_t argCount = original.GetArgCount();

    BinarySnapshot moved(std::move(original));
    EXPECT_EQ(moved.GetArgCount(), argCount);
}

TEST(BinarySnapshotTest, EqualityOperator) {
    BinarySnapshot snap1;
    BinarySnapshot snap2;

    EXPECT_TRUE(snap1.Capture(42, "test"));
    EXPECT_TRUE(snap2.Capture(42, "test"));

    EXPECT_EQ(snap1, snap2);

    BinarySnapshot snap3;
    EXPECT_TRUE(snap3.Capture(43, "test"));
    EXPECT_NE(snap1, snap3);
}

// ==============================================================================
// Type Conversion Tests / 类型转换测试
// ==============================================================================

TEST(BinarySnapshotTest, IntegralTypeConversion) {
    BinarySnapshot snapshot;
    
    // Test various integral types
    int8_t i8 = 127;
    int16_t i16 = 32767;
    uint8_t u8 = 255;
    uint16_t u16 = 65535;
    
    EXPECT_TRUE(snapshot.Capture(i8, i16, u8, u16));
    EXPECT_EQ(snapshot.GetArgCount(), 4);
}

TEST(BinarySnapshotTest, FloatingPointConversion) {
    BinarySnapshot snapshot;
    
    long double ld = 1.234567890123456789L;
    EXPECT_TRUE(snapshot.Capture(ld));
    EXPECT_EQ(snapshot.GetArgCount(), 1);
}
