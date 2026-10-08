#pragma once

/// @file Require.hpp
/// @brief CPPI_REQUIRE: an assertion for test helpers that return a value.
///
/// GoogleTest's ASSERT_* macros only work in functions that return void.
/// Helpers that return a value use CPPI_REQUIRE instead: it records the
/// failure, then throws to leave the test (GoogleTest reports the exception
/// too, which is fine: the test has already failed).
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <gtest/gtest.h>

#include <stdexcept>

namespace cppi_test {

/// Thrown by CPPI_REQUIRE to leave the current test.
struct RequirementFailed : std::runtime_error {
    /// Points at the GoogleTest failure reported just before.
    RequirementFailed() : std::runtime_error("a required condition failed (see the failure above)") {}
};

}  // namespace cppi_test

/// Like ASSERT_TRUE, but usable in functions that return a value.
#define CPPI_REQUIRE(...)                               \
    do {                                                \
        if (!(__VA_ARGS__)) {                           \
            ADD_FAILURE() << "Required: " #__VA_ARGS__; \
            throw ::cppi_test::RequirementFailed();     \
        }                                               \
    } while (false)
