# ==============================================================================
# onePlog Tests / onePlog 测试
# ==============================================================================

# ==============================================================================
# Dependencies / 依赖
# ==============================================================================
include(FetchContent)

# Google Test
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

# RapidCheck (Property-based testing / 属性测试)
FetchContent_Declare(
    rapidcheck
    GIT_REPOSITORY https://github.com/emil-e/rapidcheck.git
    GIT_TAG master
)
set(RC_ENABLE_GTEST ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(rapidcheck)

# ==============================================================================
# Test Sources / 测试源文件
# ==============================================================================
set(TEST_SOURCES
    test_level.cpp
    test_binary_snapshot.cpp
    test_log_entry.cpp
    test_heap_ring_buffer.cpp
    test_name_manager.cpp
)

# ==============================================================================
# Test Executable / 测试可执行文件
# ==============================================================================
add_executable(oneplog_tests ${TEST_SOURCES})

target_link_libraries(oneplog_tests PRIVATE
    oneplog
    GTest::gtest
    GTest::gtest_main
    rapidcheck
    rapidcheck_gtest
)

target_include_directories(oneplog_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${rapidcheck_SOURCE_DIR}/extras/gtest/include
)

# Enable RapidCheck property-based testing / 启用 RapidCheck 属性测试
target_compile_definitions(oneplog_tests PRIVATE ONEPLOG_HAS_RAPIDCHECK)

# ==============================================================================
# Register Tests / 注册测试
# ==============================================================================
include(GoogleTest)
gtest_discover_tests(oneplog_tests)
