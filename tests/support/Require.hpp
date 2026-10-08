#pragma once

// GoogleTest's ASSERT_* macros only work in functions that return void.
// Helpers that return a value use CPPI_REQUIRE instead: it records the
// failure, then throws to leave the test (GoogleTest reports the exception
// too, which is fine: the test has already failed).

#include <gtest/gtest.h>

#include <stdexcept>

namespace cppi_test {

struct RequirementFailed : std::runtime_error {
    RequirementFailed() : std::runtime_error("a required condition failed (see the failure above)") {}
};

}  // namespace cppi_test

#define CPPI_REQUIRE(...)                               \
    do {                                                \
        if (!(__VA_ARGS__)) {                           \
            ADD_FAILURE() << "Required: " #__VA_ARGS__; \
            throw ::cppi_test::RequirementFailed();     \
        }                                               \
    } while (false)
