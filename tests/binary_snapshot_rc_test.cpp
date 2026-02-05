/**
 * @file binary_snapshot_rc_test.cpp
 * @brief RapidCheck property-based tests for BinarySnapshot class
 * @文件 binary_snapshot_rc_test.cpp
 * @简述 BinarySnapshot 类的 RapidCheck 属性测试
 *
 * Feature: template-logger-refactor
 * Properties: 6, 7
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include "oneplog/binary_snapshot.hpp"
#include <gtest/gtest.h>
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#include <cmath>
#include <sstream>
#include <algorithm>

using namespace oneplog;

// ==============================================================================
// RapidCheck Generators / RapidCheck 生成器
// ==============================================================================

namespace rc {

// 为 BinarySnapshot 生成任意值
template <>
struct Arbitrary<BinarySnapshot> {
    static Gen<BinarySnapshot> arbitrary() {
        return gen::build<BinarySnapshot>(
            gen::set(&BinarySnapshot::CaptureInt32, gen::arbitrary<int32_t>()),
            gen::set(&BinarySnapshot::CaptureInt64, gen::arbitrary<int64_t>()),
            gen::set(&BinarySnapshot::CaptureUInt32, gen::arbitrary<uint32_t>()),
            gen::set(&BinarySnapshot::CaptureUInt64, gen::arbitrary<uint64_t>()),
            gen::set(&BinarySnapshot::CaptureFloat, gen::arbitrary<float>()),
            gen::set(&BinarySnapshot::CaptureDouble, gen::arbitrary<double>()),
            gen::set(&BinarySnapshot::CaptureBool, gen::arbitrary<bool>())
        );
    }
};

} // namespace rc

// ==============================================================================
// Property 6: BinarySnapshot 类型捕获完整性
// Property 6: BinarySnapshot Type Capture Completeness
// ==============================================================================

/**
 * Feature: template-logger-refactor, Property 6: BinarySnapshot 类型捕获完整性
 * 
 * 对于任意支持的类型值（int32_t, int64_t, uint32_t, uint64_t, float, double, bool, string），
 * BinarySnapshot 的 Capture 操作应该成功捕获该值，且 Format 操作应该能够正确还原该值。
 * 
 * For any supported type value (int32_t, int64_t, uint32_t, uint64_t, float, double, bool, string),
 * BinarySnapshot's Capture operation should successfully capture the value, and Format operation
 * should be able to correctly restore the value.
 */

RC_GTEST_PROP(BinarySnapshotRCTest, Property6_Int32CaptureAndFormat, ()) {
    const auto value = *rc::gen::arbitrary<int32_t>();
    
    BinarySnapshot snapshot;
    RC_ASSERT(snapshot.CaptureInt32(value));
    RC_ASSERT(snapshot.GetArgCount() == 1);
    
    std::string result = snapshot.Format("%d");
    int32_t restored = std::stoi(result);
    RC_ASSERT(restored == value);
}

RC_GTEST_PROP(BinarySnapshotRCTest, Property6_Int64CaptureAndFormat, ()) {
    const auto value = *rc::gen::arbitrary<int64_t>();
    
    BinarySnapshot snapshot;
    RC_ASSERT(snapshot.CaptureInt64(value));
    RC_ASSERT(snapshot.GetArgCount() == 1);
    
    std::string result = snapshot.Format("%d");
    int64_t restored = std::stoll(result);
    RC_ASSERT(restored == value);
}

RC_GTEST_PROP(BinarySnapshotRCTest, Property6_UInt32CaptureAndFormat, ()) {
    const auto value = *rc::gen::arbitrary<uint32_t>();
    
    BinarySnapshot snapshot;
    RC_ASSERT(snapshot.CaptureUInt32(value));
    RC_ASSERT(snapshot.GetArgCount() == 1);
    
    std::string result = snapshot.Format("%u");
    uint32_t restored = std::stoul(result);
    RC_ASSERT(restored == value);
}

RC_GTEST_PROP(BinarySnapshotRCTest, Property6_UInt64CaptureAndFormat, ()) {
    const auto value = *rc::gen::arbitrary<uint64_t>();
    
    BinarySnapshot snapshot;
    RC_ASSERT(snapshot.CaptureUInt64(value));
    RC_ASSERT(snapshot.GetArgCount() == 1);
    
    std::string result = snapshot.Format("%u");
    uint64_t restored = std::stoull(result);
    RC_ASSERT(restored == value);
}

RC_GTEST_PROP(BinarySnapshotRCTest, Property6_FloatCaptureAndFormat, ()) {
    // 使用 arbitrary 生成浮点数，然后手动限制范围
    auto rawValue = *rc::gen::arbitrary<float>();
    // 限制到合理范围并避免特殊值
    if (std::isnan(rawValue) || std::isinf(rawValue)) {
        rawValue = 0.0f;
    }
    const float value = std::fmod(rawValue, 1000.0f);
    
    BinarySnapshot snapshot;
    RC_ASSERT(snapshot.CaptureFloat(value));
    RC_ASSERT(snapshot.GetArgCount() == 1);
    
    std::string result = snapshot.Format("%f");
    float restored = std::stof(result);
    
    // 使用相对误差检查浮点数相等性
    float relativeError = std::abs((restored - value) / (value + 1e-10f));
    RC_ASSERT(relativeError < 0.001f || std::abs(restored - value) < 0.001f);
}

RC_GTEST_PROP(BinarySnapshotRCTest, Property6_DoubleCaptureAndFormat, ()) {
    // 使用 arbitrary 生成双精度浮点数，然后手动限制范围
    auto rawValue = *rc::gen::arbitrary<double>();
    // 限制到合理范围并避免特殊值
    if (std::isnan(rawValue) || std::isinf(rawValue)) {
        rawValue = 0.0;
    }
    const double value = std::fmod(rawValue, 1000.0);
    
    BinarySnapshot snapshot;
    RC_ASSERT(snapshot.CaptureDouble(value));
    RC_ASSERT(snapshot.GetArgCount() == 1);
    
    std::string result = snapshot.Format("%f");
    double restored = std::stod(result);
    
    // 使用相对误差检查双精度浮点数相等性
    double relativeError = std::abs((restored - value) / (value + 1e-10));
    RC_ASSERT(relativeError < 0.001 || std::abs(restored - value) < 0.001);
}

RC_GTEST_PROP(BinarySnapshotRCTest, Property6_BoolCaptureAndFormat, ()) {
    const auto value = *rc::gen::arbitrary<bool>();
    
    BinarySnapshot snapshot;
    RC_ASSERT(snapshot.CaptureBool(value));
    RC_ASSERT(snapshot.GetArgCount() == 1);
    
    std::string result = snapshot.Format("%s");
    std::string expected = value ? "true" : "false";
    RC_ASSERT(result == expected);
}

RC_GTEST_PROP(BinarySnapshotRCTest, Property6_StringCaptureAndFormat, ()) {
    // 生成可打印 ASCII 字符串 - 使用正确的 API
    const auto value = *rc::gen::string<std::string>();
    
    // 限制字符串长度
    std::string limitedValue = value.substr(0, std::min(value.size(), size_t(50)));
    
    BinarySnapshot snapshot;
    RC_ASSERT(snapshot.CaptureString(limitedValue));
    RC_ASSERT(snapshot.GetArgCount() == 1);
    
    std::string result = snapshot.Format("%s");
    RC_ASSERT(result == limitedValue);
}

RC_GTEST_PROP(BinarySnapshotRCTest, Property6_MixedTypesCaptureAndFormat, ()) {
    const auto intVal = *rc::gen::arbitrary<int32_t>();
    auto rawDoubleVal = *rc::gen::arbitrary<double>();
    if (std::isnan(rawDoubleVal) || std::isinf(rawDoubleVal)) {
        rawDoubleVal = 0.0;
    }
    const double doubleVal = std::fmod(rawDoubleVal, 1000.0);
    const auto boolVal = *rc::gen::arbitrary<bool>();
    const auto strVal = *rc::gen::string<std::string>();
    std::string limitedStr = strVal.substr(0, std::min(strVal.size(), size_t(30)));
    
    BinarySnapshot snapshot;
    RC_ASSERT(snapshot.Capture(intVal, doubleVal, boolVal, limitedStr));
    RC_ASSERT(snapshot.GetArgCount() == 4);
    
    std::string result = snapshot.Format("%d %f %s %s");
    
    // 验证结果包含所有值
    RC_ASSERT(result.find(std::to_string(intVal)) != std::string::npos);
    RC_ASSERT(result.find(boolVal ? "true" : "false") != std::string::npos);
    RC_ASSERT(result.find(limitedStr) != std::string::npos);
}

// ==============================================================================
// Property 7: BinarySnapshot 序列化往返一致性
// Property 7: BinarySnapshot Serialization Round-Trip Consistency
// ==============================================================================

/**
 * Feature: template-logger-refactor, Property 7: BinarySnapshot 序列化往返一致性
 * 
 * 对于任意有效的 BinarySnapshot 对象 S，Deserialize(Serialize(S)) 应该产生与 S 等价的对象。
 * 
 * For any valid BinarySnapshot object S, Deserialize(Serialize(S)) should produce an object
 * equivalent to S.
 */

RC_GTEST_PROP(BinarySnapshotRCTest, Property7_RoundTripInt32, ()) {
    const auto value = *rc::gen::arbitrary<int32_t>();
    
    BinarySnapshot original;
    RC_ASSERT(original.CaptureInt32(value));
    
    // 序列化
    size_t size = original.SerializedSize();
    std::vector<uint8_t> buffer(size);
    RC_ASSERT(original.SerializeTo(buffer.data()));
    
    // 反序列化
    BinarySnapshot deserialized = BinarySnapshot::Deserialize(buffer.data(), size);
    
    // 验证相等性
    RC_ASSERT(deserialized == original);
    RC_ASSERT(deserialized.GetArgCount() == original.GetArgCount());
    RC_ASSERT(deserialized.GetOffset() == original.GetOffset());
}

RC_GTEST_PROP(BinarySnapshotRCTest, Property7_RoundTripMultipleInts, ()) {
    // 生成 1-10 个随机整数
    const auto count = *rc::gen::inRange<size_t>(1, 11);
    std::vector<int32_t> values;
    for (size_t i = 0; i < count; ++i) {
        values.push_back(*rc::gen::arbitrary<int32_t>());
    }
    
    BinarySnapshot original;
    for (const auto& value : values) {
        RC_ASSERT(original.CaptureInt32(value));
    }
    
    // 序列化
    size_t size = original.SerializedSize();
    std::vector<uint8_t> buffer(size);
    RC_ASSERT(original.SerializeTo(buffer.data()));
    
    // 反序列化
    BinarySnapshot deserialized = BinarySnapshot::Deserialize(buffer.data(), size);
    
    // 验证相等性
    RC_ASSERT(deserialized == original);
    RC_ASSERT(deserialized.GetArgCount() == original.GetArgCount());
}

RC_GTEST_PROP(BinarySnapshotRCTest, Property7_RoundTripMixedTypes, ()) {
    const auto intVal = *rc::gen::arbitrary<int32_t>();
    auto rawDoubleVal = *rc::gen::arbitrary<double>();
    if (std::isnan(rawDoubleVal) || std::isinf(rawDoubleVal)) {
        rawDoubleVal = 0.0;
    }
    const double doubleVal = std::fmod(rawDoubleVal, 1000.0);
    const auto boolVal = *rc::gen::arbitrary<bool>();
    const auto strVal = *rc::gen::string<std::string>();
    std::string limitedStr = strVal.substr(0, std::min(strVal.size(), size_t(30)));
    
    BinarySnapshot original;
    RC_ASSERT(original.Capture(intVal, doubleVal, boolVal, limitedStr));
    
    // 序列化
    size_t size = original.SerializedSize();
    std::vector<uint8_t> buffer(size);
    RC_ASSERT(original.SerializeTo(buffer.data()));
    
    // 反序列化
    BinarySnapshot deserialized = BinarySnapshot::Deserialize(buffer.data(), size);
    
    // 验证相等性
    RC_ASSERT(deserialized == original);
    RC_ASSERT(deserialized.GetArgCount() == original.GetArgCount());
}

RC_GTEST_PROP(BinarySnapshotRCTest, Property7_RoundTripEmpty, ()) {
    BinarySnapshot original;
    
    // 序列化空快照
    size_t size = original.SerializedSize();
    std::vector<uint8_t> buffer(size);
    RC_ASSERT(original.SerializeTo(buffer.data()));
    
    // 反序列化
    BinarySnapshot deserialized = BinarySnapshot::Deserialize(buffer.data(), size);
    
    // 验证相等性
    RC_ASSERT(deserialized == original);
    RC_ASSERT(deserialized.IsEmpty());
}

RC_GTEST_PROP(BinarySnapshotRCTest, Property7_RoundTripAfterReset, ()) {
    const auto value1 = *rc::gen::arbitrary<int32_t>();
    const auto value2 = *rc::gen::arbitrary<int32_t>();
    
    BinarySnapshot original;
    
    // 捕获一些数据
    RC_ASSERT(original.CaptureInt32(value1));
    
    // 重置
    original.Reset();
    
    // 捕获新数据
    RC_ASSERT(original.CaptureInt32(value2));
    
    // 序列化
    size_t size = original.SerializedSize();
    std::vector<uint8_t> buffer(size);
    RC_ASSERT(original.SerializeTo(buffer.data()));
    
    // 反序列化
    BinarySnapshot deserialized = BinarySnapshot::Deserialize(buffer.data(), size);
    
    // 验证相等性
    RC_ASSERT(deserialized == original);
    RC_ASSERT(deserialized.GetArgCount() == 1);
}

// ==============================================================================
// 额外的属性测试 / Additional Property Tests
// ==============================================================================

/**
 * Property: 序列化幂等性
 * Property: Serialization Idempotence
 * 
 * 多次序列化同一个快照应该产生相同的结果。
 * Serializing the same snapshot multiple times should produce identical results.
 */
RC_GTEST_PROP(BinarySnapshotRCTest, SerializationIdempotence, ()) {
    const auto count = *rc::gen::inRange<size_t>(1, 6);
    std::vector<int32_t> values;
    for (size_t i = 0; i < count; ++i) {
        values.push_back(*rc::gen::arbitrary<int32_t>());
    }
    
    BinarySnapshot snapshot;
    for (const auto& value : values) {
        RC_ASSERT(snapshot.CaptureInt32(value));
    }
    
    size_t size = snapshot.SerializedSize();
    std::vector<uint8_t> buffer1(size);
    std::vector<uint8_t> buffer2(size);
    
    RC_ASSERT(snapshot.SerializeTo(buffer1.data()));
    RC_ASSERT(snapshot.SerializeTo(buffer2.data()));
    
    RC_ASSERT(buffer1 == buffer2);
}

/**
 * Property: 捕获顺序保持
 * Property: Capture Order Preservation
 * 
 * 参数应该按照捕获的顺序进行格式化。
 * Arguments should be formatted in the same order they were captured.
 */
RC_GTEST_PROP(BinarySnapshotRCTest, CaptureOrderPreservation, ()) {
    const auto count = *rc::gen::inRange<size_t>(2, 6);
    std::vector<int32_t> values;
    for (size_t i = 0; i < count; ++i) {
        values.push_back(*rc::gen::inRange<int32_t>(0, 1001));
    }
    
    BinarySnapshot snapshot;
    std::string formatStr;
    
    for (const auto& value : values) {
        RC_ASSERT(snapshot.CaptureInt32(value));
        if (!formatStr.empty()) {
            formatStr += " ";
        }
        formatStr += "%d";
    }
    
    std::string result = snapshot.Format(formatStr.c_str());
    
    // 解析结果并验证顺序
    std::istringstream iss(result);
    for (size_t i = 0; i < values.size(); ++i) {
        int32_t parsed;
        iss >> parsed;
        RC_ASSERT(parsed == values[i]);
    }
}

/**
 * Property: 指针转换正确性
 * Property: Pointer Conversion Correctness
 * 
 * ConvertPointersToData 后，StringView 应该被转换为 StringCopy，
 * 但格式化结果应该保持不变。
 * 
 * After ConvertPointersToData, StringView should be converted to StringCopy,
 * but the formatted result should remain unchanged.
 */
RC_GTEST_PROP(BinarySnapshotRCTest, PointerConversionCorrectness, ()) {
    const auto str = *rc::gen::string<std::string>();
    std::string limitedStr = str.substr(0, std::min(str.size(), size_t(30)));
    
    // 跳过空字符串
    if (limitedStr.empty()) {
        limitedStr = "test";
    }
    
    BinarySnapshot snapshot;
    std::string_view sv = limitedStr;
    RC_ASSERT(snapshot.CaptureStringView(sv));
    
    // 格式化前的结果
    std::string resultBefore = snapshot.Format("%s");
    
    // 转换指针
    snapshot.ConvertPointersToData();
    
    // 格式化后的结果
    std::string resultAfter = snapshot.Format("%s");
    
    // 结果应该相同
    RC_ASSERT(resultBefore == resultAfter);
    RC_ASSERT(resultAfter == limitedStr);
}

/**
 * Property: 缓冲区边界安全
 * Property: Buffer Boundary Safety
 * 
 * 即使尝试捕获大量数据，也不应该发生缓冲区溢出。
 * Even when attempting to capture large amounts of data, buffer overflow should not occur.
 */
RC_GTEST_PROP(BinarySnapshotRCTest, BufferBoundarySafety, ()) {
    const auto largeString = *rc::gen::string<std::string>();
    // 确保字符串足够大
    std::string testString = largeString + std::string(100, 'x');
    testString = testString.substr(0, std::min(testString.size(), size_t(200)));
    
    BinarySnapshot snapshot;
    
    // 尝试填充缓冲区
    bool success = true;
    int count = 0;
    while (success && count < 10) {
        success = snapshot.CaptureString(testString);
        if (success) {
            ++count;
        }
    }
    
    // 应该在某个点失败（缓冲区满），而不是崩溃
    RC_ASSERT(count < 10);
    
    // 快照应该仍然有效
    RC_ASSERT(snapshot.GetArgCount() == static_cast<uint16_t>(count));
}
