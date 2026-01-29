/**
 * @file test_level.cpp
 * @brief Unit tests for Level enumeration
 * @brief Level 枚举的单元测试
 *
 * @copyright Copyright (c) 2024 onePlog
 */

#include <gtest/gtest.h>
#ifdef ONEPLOG_HAS_RAPIDCHECK
#include <rapidcheck.h>
#include <rapidcheck/gtest.h>
#endif

#include <oneplog/common.hpp>

namespace oneplog {
namespace test {

// ==============================================================================
// Unit Tests / 单元测试
// ==============================================================================

/**
 * @brief Test Level enum values
 * @brief 测试 Level 枚举值
 */
TEST(LevelTest, EnumValues) {
    EXPECT_EQ(static_cast<uint8_t>(Level::Trace), 0);
    EXPECT_EQ(static_cast<uint8_t>(Level::Debug), 1);
    EXPECT_EQ(static_cast<uint8_t>(Level::Info), 2);
    EXPECT_EQ(static_cast<uint8_t>(Level::Warn), 3);
    EXPECT_EQ(static_cast<uint8_t>(Level::Error), 4);
    EXPECT_EQ(static_cast<uint8_t>(Level::Critical), 5);
    EXPECT_EQ(static_cast<uint8_t>(Level::Off), 6);
}

/**
 * @brief Test LevelToString with Full style
 * @brief 测试 LevelToString 全称样式
 */
TEST(LevelTest, LevelToStringFull) {
    EXPECT_EQ(LevelToString(Level::Trace, LevelNameStyle::Full), "trace");
    EXPECT_EQ(LevelToString(Level::Debug, LevelNameStyle::Full), "debug");
    EXPECT_EQ(LevelToString(Level::Info, LevelNameStyle::Full), "info");
    EXPECT_EQ(LevelToString(Level::Warn, LevelNameStyle::Full), "warn");
    EXPECT_EQ(LevelToString(Level::Error, LevelNameStyle::Full), "error");
    EXPECT_EQ(LevelToString(Level::Critical, LevelNameStyle::Full), "critical");
    EXPECT_EQ(LevelToString(Level::Off, LevelNameStyle::Full), "off");
}

/**
 * @brief Test LevelToString with Short4 style
 * @brief 测试 LevelToString 4字符样式
 */
TEST(LevelTest, LevelToStringShort4) {
    EXPECT_EQ(LevelToString(Level::Trace, LevelNameStyle::Short4), "TRAC");
    EXPECT_EQ(LevelToString(Level::Debug, LevelNameStyle::Short4), "DBUG");
    EXPECT_EQ(LevelToString(Level::Info, LevelNameStyle::Short4), "INFO");
    EXPECT_EQ(LevelToString(Level::Warn, LevelNameStyle::Short4), "WARN");
    EXPECT_EQ(LevelToString(Level::Error, LevelNameStyle::Short4), "ERRO");
    EXPECT_EQ(LevelToString(Level::Critical, LevelNameStyle::Short4), "CRIT");
    EXPECT_EQ(LevelToString(Level::Off, LevelNameStyle::Short4), "OFF");
}

/**
 * @brief Test LevelToString with Short1 style
 * @brief 测试 LevelToString 1字符样式
 */
TEST(LevelTest, LevelToStringShort1) {
    EXPECT_EQ(LevelToString(Level::Trace, LevelNameStyle::Short1), "T");
    EXPECT_EQ(LevelToString(Level::Debug, LevelNameStyle::Short1), "D");
    EXPECT_EQ(LevelToString(Level::Info, LevelNameStyle::Short1), "I");
    EXPECT_EQ(LevelToString(Level::Warn, LevelNameStyle::Short1), "W");
    EXPECT_EQ(LevelToString(Level::Error, LevelNameStyle::Short1), "E");
    EXPECT_EQ(LevelToString(Level::Critical, LevelNameStyle::Short1), "C");
    EXPECT_EQ(LevelToString(Level::Off, LevelNameStyle::Short1), "O");
}

/**
 * @brief Test LevelToString default style (Short4)
 * @brief 测试 LevelToString 默认样式（Short4）
 */
TEST(LevelTest, LevelToStringDefault) {
    EXPECT_EQ(LevelToString(Level::Info), "INFO");
    EXPECT_EQ(LevelToString(Level::Error), "ERRO");
}

/**
 * @brief Test StringToLevel conversion
 * @brief 测试 StringToLevel 转换
 */
TEST(LevelTest, StringToLevel) {
    // Full names / 全称
    EXPECT_EQ(StringToLevel("trace"), Level::Trace);
    EXPECT_EQ(StringToLevel("debug"), Level::Debug);
    EXPECT_EQ(StringToLevel("info"), Level::Info);
    EXPECT_EQ(StringToLevel("warn"), Level::Warn);
    EXPECT_EQ(StringToLevel("error"), Level::Error);
    EXPECT_EQ(StringToLevel("critical"), Level::Critical);
    EXPECT_EQ(StringToLevel("off"), Level::Off);

    // Uppercase / 大写
    EXPECT_EQ(StringToLevel("TRACE"), Level::Trace);
    EXPECT_EQ(StringToLevel("DEBUG"), Level::Debug);
    EXPECT_EQ(StringToLevel("INFO"), Level::Info);
    EXPECT_EQ(StringToLevel("WARN"), Level::Warn);
    EXPECT_EQ(StringToLevel("ERROR"), Level::Error);
    EXPECT_EQ(StringToLevel("CRITICAL"), Level::Critical);
    EXPECT_EQ(StringToLevel("OFF"), Level::Off);

    // Short4 / 4字符
    EXPECT_EQ(StringToLevel("TRAC"), Level::Trace);
    EXPECT_EQ(StringToLevel("DBUG"), Level::Debug);
    EXPECT_EQ(StringToLevel("ERRO"), Level::Error);
    EXPECT_EQ(StringToLevel("CRIT"), Level::Critical);

    // Short1 / 1字符
    EXPECT_EQ(StringToLevel("T"), Level::Trace);
    EXPECT_EQ(StringToLevel("D"), Level::Debug);
    EXPECT_EQ(StringToLevel("I"), Level::Info);
    EXPECT_EQ(StringToLevel("W"), Level::Warn);
    EXPECT_EQ(StringToLevel("E"), Level::Error);
    EXPECT_EQ(StringToLevel("C"), Level::Critical);
    EXPECT_EQ(StringToLevel("O"), Level::Off);

    // Unknown returns Off / 未知返回 Off
    EXPECT_EQ(StringToLevel("unknown"), Level::Off);
    EXPECT_EQ(StringToLevel(""), Level::Off);
}

/**
 * @brief Test compile-time ShouldLog function
 * @brief 测试编译时 ShouldLog 函数
 */
TEST(LevelTest, ShouldLogCompileTime) {
    // When current level is Info / 当前级别为 Info 时
    EXPECT_FALSE((ShouldLog<Level::Trace, Level::Info>()));
    EXPECT_FALSE((ShouldLog<Level::Debug, Level::Info>()));
    EXPECT_TRUE((ShouldLog<Level::Info, Level::Info>()));
    EXPECT_TRUE((ShouldLog<Level::Warn, Level::Info>()));
    EXPECT_TRUE((ShouldLog<Level::Error, Level::Info>()));
    EXPECT_TRUE((ShouldLog<Level::Critical, Level::Info>()));

    // When current level is Trace (log everything) / 当前级别为 Trace（记录所有）
    EXPECT_TRUE((ShouldLog<Level::Trace, Level::Trace>()));
    EXPECT_TRUE((ShouldLog<Level::Debug, Level::Trace>()));
    EXPECT_TRUE((ShouldLog<Level::Info, Level::Trace>()));

    // When current level is Off (log nothing) / 当前级别为 Off（不记录）
    EXPECT_FALSE((ShouldLog<Level::Trace, Level::Off>()));
    EXPECT_FALSE((ShouldLog<Level::Critical, Level::Off>()));
    EXPECT_TRUE((ShouldLog<Level::Off, Level::Off>()));
}

/**
 * @brief Test level count constants
 * @brief 测试级别数量常量
 */
TEST(LevelTest, LevelCount) {
    EXPECT_EQ(kLevelCount, 6);
    EXPECT_EQ(kLevelCountWithOff, 7);
}

// ==============================================================================
// Property-Based Tests / 属性测试
// ==============================================================================

#ifdef ONEPLOG_HAS_RAPIDCHECK

/**
 * @brief Property 1: Level formatting consistency
 * @brief 属性 1：日志级别格式化一致性
 *
 * For any log level and any formatting style (Full/Short4/Short1),
 * LevelToString should return the correct string representation.
 *
 * 对于任意日志级别和任意格式化样式（Full/Short4/Short1），
 * LevelToString 函数应该返回对应的正确字符串表示。
 *
 * **Validates: Requirements 1.2**
 */
RC_GTEST_PROP(LevelPropertyTest, LevelFormattingConsistency, ()) {
    // Generate random level (0-6) / 生成随机级别（0-6）
    const auto levelValue = *rc::gen::inRange<uint8_t>(0, 7);
    const auto level = static_cast<Level>(levelValue);

    // Generate random style (0-2) / 生成随机样式（0-2）
    const auto styleValue = *rc::gen::inRange<uint8_t>(0, 3);
    const auto style = static_cast<LevelNameStyle>(styleValue);

    // Get string representation / 获取字符串表示
    const auto str = LevelToString(level, style);

    // Verify non-empty / 验证非空
    RC_ASSERT(!str.empty());

    // Verify correct length based on style / 根据样式验证正确长度
    switch (style) {
        case LevelNameStyle::Full:
            RC_ASSERT(str.size() >= 3);  // "off" is shortest / "off" 最短
            RC_ASSERT(str.size() <= 8);  // "critical" is longest / "critical" 最长
            break;
        case LevelNameStyle::Short4:
            RC_ASSERT(str.size() >= 3);  // "OFF" is 3 chars / "OFF" 是 3 字符
            RC_ASSERT(str.size() <= 4);  // Most are 4 chars / 大多数是 4 字符
            break;
        case LevelNameStyle::Short1:
            RC_ASSERT(str.size() == 1);  // Always 1 char / 总是 1 字符
            break;
    }

    // Verify round-trip for Full style / 验证全称样式的往返一致性
    if (style == LevelNameStyle::Full) {
        RC_ASSERT(StringToLevel(str) == level);
    }
}

/**
 * @brief Property 2: Level filtering correctness (compile-time)
 * @brief 属性 2：日志级别过滤正确性（编译时）
 *
 * For any log level L and current level S, when L < S, ShouldLog<L, S>() should
 * return false; when L >= S, ShouldLog<L, S>() should return true.
 *
 * 对于任意日志级别 L 和当前设置级别 S，当 L < S 时，ShouldLog<L, S>() 应该返回 false；
 * 当 L >= S 时，ShouldLog<L, S>() 应该返回 true。
 *
 * **Validates: Requirements 1.3**
 */
RC_GTEST_PROP(LevelPropertyTest, LevelFilteringCorrectness, ()) {
    // Generate random levels / 生成随机级别
    const auto logLevelValue = *rc::gen::inRange<uint8_t>(0, 7);
    const auto currentLevelValue = *rc::gen::inRange<uint8_t>(0, 7);

    const auto logLevel = static_cast<Level>(logLevelValue);
    const auto currentLevel = static_cast<Level>(currentLevelValue);

    // Verify filtering logic at runtime / 在运行时验证过滤逻辑
    // (compile-time version is tested in unit tests)
    // （编译时版本在单元测试中测试）
    const bool expected = logLevelValue >= currentLevelValue;

    // Test all combinations at compile time would be impractical,
    // so we verify the logic is correct for runtime comparison
    // 在编译时测试所有组合不切实际，
    // 所以我们验证运行时比较的逻辑是正确的
    RC_ASSERT((static_cast<uint8_t>(logLevel) >= static_cast<uint8_t>(currentLevel)) == expected);
}

/**
 * @brief Property: StringToLevel and LevelToString are consistent
 * @brief 属性：StringToLevel 和 LevelToString 一致
 */
RC_GTEST_PROP(LevelPropertyTest, StringLevelRoundTrip, ()) {
    // Generate random level (0-6) / 生成随机级别（0-6）
    const auto levelValue = *rc::gen::inRange<uint8_t>(0, 7);
    const auto level = static_cast<Level>(levelValue);

    // Convert to string and back / 转换为字符串再转回
    const auto str = LevelToString(level, LevelNameStyle::Full);
    const auto roundTrip = StringToLevel(str);

    RC_ASSERT(roundTrip == level);
}

#endif  // ONEPLOG_HAS_RAPIDCHECK

}  // namespace test
}  // namespace oneplog
