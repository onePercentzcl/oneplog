/**
 * @file binary_snapshot_property_test.cpp
 * @brief Property-based tests for BinarySnapshot class
 * @文件 binary_snapshot_property_test.cpp
 * @简述 BinarySnapshot 类的属性测试
 *
 * Feature: template-logger-refactor
 * Properties: 6, 7
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include "oneplog/binary_snapshot.hpp"
#include <gtest/gtest.h>
#include <random>
#include <vector>

using namespace oneplog;

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
class BinarySnapshotProperty6Test : public ::testing::TestWithParam<int> {
protected:
    std::mt19937 rng{42}; // Fixed seed for reproducibility
};

TEST_P(BinarySnapshotProperty6Test, Int32CaptureAndFormat) {
    std::uniform_int_distribution<int32_t> dist(INT32_MIN, INT32_MAX);
    
    for (int i = 0; i < 100; ++i) {
        int32_t value = dist(rng);
        BinarySnapshot snapshot;
        
        // Capture should succeed
        ASSERT_TRUE(snapshot.CaptureInt32(value));
        ASSERT_EQ(snapshot.GetArgCount(), 1);
        
        // Format should restore the value
        std::string result = snapshot.Format("%d");
        int32_t restored = std::stoi(result);
        EXPECT_EQ(restored, value) << "Failed to restore int32_t value: " << value;
    }
}

TEST_P(BinarySnapshotProperty6Test, Int64CaptureAndFormat) {
    std::uniform_int_distribution<int64_t> dist(INT64_MIN / 2, INT64_MAX / 2);
    
    for (int i = 0; i < 100; ++i) {
        int64_t value = dist(rng);
        BinarySnapshot snapshot;
        
        ASSERT_TRUE(snapshot.CaptureInt64(value));
        ASSERT_EQ(snapshot.GetArgCount(), 1);
        
        std::string result = snapshot.Format("%d");
        int64_t restored = std::stoll(result);
        EXPECT_EQ(restored, value) << "Failed to restore int64_t value: " << value;
    }
}

TEST_P(BinarySnapshotProperty6Test, UInt32CaptureAndFormat) {
    std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
    
    for (int i = 0; i < 100; ++i) {
        uint32_t value = dist(rng);
        BinarySnapshot snapshot;
        
        ASSERT_TRUE(snapshot.CaptureUInt32(value));
        ASSERT_EQ(snapshot.GetArgCount(), 1);
        
        std::string result = snapshot.Format("%u");
        uint32_t restored = std::stoul(result);
        EXPECT_EQ(restored, value) << "Failed to restore uint32_t value: " << value;
    }
}

TEST_P(BinarySnapshotProperty6Test, UInt64CaptureAndFormat) {
    std::uniform_int_distribution<uint64_t> dist(0, UINT64_MAX / 2);
    
    for (int i = 0; i < 100; ++i) {
        uint64_t value = dist(rng);
        BinarySnapshot snapshot;
        
        ASSERT_TRUE(snapshot.CaptureUInt64(value));
        ASSERT_EQ(snapshot.GetArgCount(), 1);
        
        std::string result = snapshot.Format("%u");
        uint64_t restored = std::stoull(result);
        EXPECT_EQ(restored, value) << "Failed to restore uint64_t value: " << value;
    }
}

TEST_P(BinarySnapshotProperty6Test, FloatCaptureAndFormat) {
    std::uniform_real_distribution<float> dist(-1000.0f, 1000.0f);
    
    for (int i = 0; i < 100; ++i) {
        float value = dist(rng);
        BinarySnapshot snapshot;
        
        ASSERT_TRUE(snapshot.CaptureFloat(value));
        ASSERT_EQ(snapshot.GetArgCount(), 1);
        
        std::string result = snapshot.Format("%f");
        float restored = std::stof(result);
        EXPECT_NEAR(restored, value, 0.001f) << "Failed to restore float value: " << value;
    }
}

TEST_P(BinarySnapshotProperty6Test, DoubleCaptureAndFormat) {
    std::uniform_real_distribution<double> dist(-1000.0, 1000.0);
    
    for (int i = 0; i < 100; ++i) {
        double value = dist(rng);
        BinarySnapshot snapshot;
        
        ASSERT_TRUE(snapshot.CaptureDouble(value));
        ASSERT_EQ(snapshot.GetArgCount(), 1);
        
        std::string result = snapshot.Format("%f");
        double restored = std::stod(result);
        EXPECT_NEAR(restored, value, 0.001) << "Failed to restore double value: " << value;
    }
}

TEST_P(BinarySnapshotProperty6Test, BoolCaptureAndFormat) {
    for (int i = 0; i < 100; ++i) {
        bool value = (i % 2 == 0);
        BinarySnapshot snapshot;
        
        ASSERT_TRUE(snapshot.CaptureBool(value));
        ASSERT_EQ(snapshot.GetArgCount(), 1);
        
        std::string result = snapshot.Format("%s");
        std::string expected = value ? "true" : "false";
        EXPECT_EQ(result, expected) << "Failed to restore bool value: " << value;
    }
}

TEST_P(BinarySnapshotProperty6Test, StringCaptureAndFormat) {
    std::uniform_int_distribution<int> lengthDist(0, 50);
    std::uniform_int_distribution<int> charDist('a', 'z');
    
    for (int i = 0; i < 100; ++i) {
        // Generate random string
        int length = lengthDist(rng);
        std::string value;
        value.reserve(length);
        for (int j = 0; j < length; ++j) {
            value += static_cast<char>(charDist(rng));
        }
        
        BinarySnapshot snapshot;
        ASSERT_TRUE(snapshot.CaptureString(value));
        ASSERT_EQ(snapshot.GetArgCount(), 1);
        
        std::string result = snapshot.Format("%s");
        EXPECT_EQ(result, value) << "Failed to restore string value: " << value;
    }
}

INSTANTIATE_TEST_SUITE_P(Property6Tests, BinarySnapshotProperty6Test, ::testing::Range(0, 1));

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
class BinarySnapshotProperty7Test : public ::testing::TestWithParam<int> {
protected:
    std::mt19937 rng{42}; // Fixed seed for reproducibility
};

TEST_P(BinarySnapshotProperty7Test, RoundTripInt32) {
    std::uniform_int_distribution<int32_t> dist(INT32_MIN, INT32_MAX);
    
    for (int i = 0; i < 100; ++i) {
        BinarySnapshot original;
        int32_t value = dist(rng);
        ASSERT_TRUE(original.CaptureInt32(value));
        
        // Serialize
        size_t size = original.SerializedSize();
        std::vector<uint8_t> buffer(size);
        ASSERT_TRUE(original.SerializeTo(buffer.data()));
        
        // Deserialize
        BinarySnapshot deserialized = BinarySnapshot::Deserialize(buffer.data(), size);
        
        // Verify equality
        EXPECT_EQ(deserialized, original) << "Round-trip failed for int32_t: " << value;
        EXPECT_EQ(deserialized.GetArgCount(), original.GetArgCount());
        EXPECT_EQ(deserialized.GetOffset(), original.GetOffset());
    }
}

TEST_P(BinarySnapshotProperty7Test, RoundTripMixedTypes) {
    std::uniform_int_distribution<int32_t> intDist(INT32_MIN, INT32_MAX);
    std::uniform_real_distribution<double> doubleDist(-1000.0, 1000.0);
    std::uniform_int_distribution<int> lengthDist(0, 30);
    std::uniform_int_distribution<int> charDist('a', 'z');
    
    for (int i = 0; i < 100; ++i) {
        BinarySnapshot original;
        
        // Capture random mixed types
        int32_t intVal = intDist(rng);
        double doubleVal = doubleDist(rng);
        bool boolVal = (i % 2 == 0);
        
        // Generate random string
        int length = lengthDist(rng);
        std::string strVal;
        strVal.reserve(length);
        for (int j = 0; j < length; ++j) {
            strVal += static_cast<char>(charDist(rng));
        }
        
        ASSERT_TRUE(original.Capture(intVal, doubleVal, boolVal, strVal));
        
        // Serialize
        size_t size = original.SerializedSize();
        std::vector<uint8_t> buffer(size);
        ASSERT_TRUE(original.SerializeTo(buffer.data()));
        
        // Deserialize
        BinarySnapshot deserialized = BinarySnapshot::Deserialize(buffer.data(), size);
        
        // Verify equality
        EXPECT_EQ(deserialized, original) << "Round-trip failed for mixed types";
        EXPECT_EQ(deserialized.GetArgCount(), original.GetArgCount());
    }
}

TEST_P(BinarySnapshotProperty7Test, RoundTripMultipleArgs) {
    std::uniform_int_distribution<int32_t> dist(INT32_MIN, INT32_MAX);
    std::uniform_int_distribution<int> argCountDist(1, 10);
    
    for (int i = 0; i < 100; ++i) {
        BinarySnapshot original;
        
        // Capture random number of arguments
        int argCount = argCountDist(rng);
        for (int j = 0; j < argCount; ++j) {
            int32_t value = dist(rng);
            ASSERT_TRUE(original.CaptureInt32(value));
        }
        
        // Serialize
        size_t size = original.SerializedSize();
        std::vector<uint8_t> buffer(size);
        ASSERT_TRUE(original.SerializeTo(buffer.data()));
        
        // Deserialize
        BinarySnapshot deserialized = BinarySnapshot::Deserialize(buffer.data(), size);
        
        // Verify equality
        EXPECT_EQ(deserialized, original) << "Round-trip failed for " << argCount << " arguments";
        EXPECT_EQ(deserialized.GetArgCount(), original.GetArgCount());
    }
}

TEST_P(BinarySnapshotProperty7Test, RoundTripStringView) {
    std::uniform_int_distribution<int> lengthDist(0, 30);
    std::uniform_int_distribution<int> charDist('a', 'z');
    
    for (int i = 0; i < 100; ++i) {
        // Generate random string
        int length = lengthDist(rng);
        std::string str;
        str.reserve(length);
        for (int j = 0; j < length; ++j) {
            str += static_cast<char>(charDist(rng));
        }
        
        BinarySnapshot original;
        std::string_view sv = str;
        ASSERT_TRUE(original.CaptureStringView(sv));
        
        // Serialize
        size_t size = original.SerializedSize();
        std::vector<uint8_t> buffer(size);
        ASSERT_TRUE(original.SerializeTo(buffer.data()));
        
        // Deserialize
        BinarySnapshot deserialized = BinarySnapshot::Deserialize(buffer.data(), size);
        
        // Verify equality
        EXPECT_EQ(deserialized, original) << "Round-trip failed for string_view: " << str;
    }
}

TEST_P(BinarySnapshotProperty7Test, RoundTripEmpty) {
    for (int i = 0; i < 100; ++i) {
        BinarySnapshot original;
        
        // Serialize empty snapshot
        size_t size = original.SerializedSize();
        std::vector<uint8_t> buffer(size);
        ASSERT_TRUE(original.SerializeTo(buffer.data()));
        
        // Deserialize
        BinarySnapshot deserialized = BinarySnapshot::Deserialize(buffer.data(), size);
        
        // Verify equality
        EXPECT_EQ(deserialized, original) << "Round-trip failed for empty snapshot";
        EXPECT_TRUE(deserialized.IsEmpty());
    }
}

TEST_P(BinarySnapshotProperty7Test, RoundTripAfterReset) {
    std::uniform_int_distribution<int32_t> dist(INT32_MIN, INT32_MAX);
    
    for (int i = 0; i < 100; ++i) {
        BinarySnapshot original;
        
        // Capture some data
        ASSERT_TRUE(original.CaptureInt32(dist(rng)));
        
        // Reset
        original.Reset();
        
        // Capture new data
        int32_t value = dist(rng);
        ASSERT_TRUE(original.CaptureInt32(value));
        
        // Serialize
        size_t size = original.SerializedSize();
        std::vector<uint8_t> buffer(size);
        ASSERT_TRUE(original.SerializeTo(buffer.data()));
        
        // Deserialize
        BinarySnapshot deserialized = BinarySnapshot::Deserialize(buffer.data(), size);
        
        // Verify equality
        EXPECT_EQ(deserialized, original) << "Round-trip failed after reset";
        EXPECT_EQ(deserialized.GetArgCount(), 1);
    }
}

INSTANTIATE_TEST_SUITE_P(Property7Tests, BinarySnapshotProperty7Test, ::testing::Range(0, 1));

// ==============================================================================
// Additional Property Tests / 额外属性测试
// ==============================================================================

/**
 * Property: Idempotence of serialization
 * 序列化的幂等性
 * 
 * Serializing the same snapshot multiple times should produce identical results.
 * 多次序列化同一个快照应该产生相同的结果。
 */
TEST(BinarySnapshotPropertyTest, SerializationIdempotence) {
    std::mt19937 rng{42};
    std::uniform_int_distribution<int32_t> dist(INT32_MIN, INT32_MAX);
    
    for (int i = 0; i < 100; ++i) {
        BinarySnapshot snapshot;
        ASSERT_TRUE(snapshot.Capture(dist(rng), dist(rng), dist(rng)));
        
        size_t size = snapshot.SerializedSize();
        std::vector<uint8_t> buffer1(size);
        std::vector<uint8_t> buffer2(size);
        
        ASSERT_TRUE(snapshot.SerializeTo(buffer1.data()));
        ASSERT_TRUE(snapshot.SerializeTo(buffer2.data()));
        
        EXPECT_EQ(buffer1, buffer2) << "Serialization is not idempotent";
    }
}

/**
 * Property: Capture order preservation
 * 捕获顺序保持
 * 
 * Arguments should be formatted in the same order they were captured.
 * 参数应该按照捕获的顺序进行格式化。
 */
TEST(BinarySnapshotPropertyTest, CaptureOrderPreservation) {
    std::mt19937 rng{42};
    std::uniform_int_distribution<int32_t> dist(0, 1000);
    
    for (int i = 0; i < 100; ++i) {
        BinarySnapshot snapshot;
        std::vector<int32_t> values;
        
        int count = 5;
        for (int j = 0; j < count; ++j) {
            int32_t value = dist(rng);
            values.push_back(value);
            ASSERT_TRUE(snapshot.CaptureInt32(value));
        }
        
        std::string result = snapshot.Format("%d %d %d %d %d");
        
        // Parse result and verify order
        std::istringstream iss(result);
        for (int j = 0; j < count; ++j) {
            int32_t parsed;
            iss >> parsed;
            EXPECT_EQ(parsed, values[j]) << "Order not preserved at position " << j;
        }
    }
}
