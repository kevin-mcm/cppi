/// @file DiagnosticsTest.cpp
/// @brief Unit tests of diagnostics: stable codes, ids, keys and named
/// arguments.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <gtest/gtest.h>

#include <cppi/Diagnostic.hpp>

#include <set>
#include <string_view>

TEST(Diagnostics, DiagnosticCodesHaveStableIdsAndKeys) {
    EXPECT_EQ(cppi::diag_id(cppi::DiagCode::UnknownFunction), "E0200");
    EXPECT_EQ(cppi::diag_key(cppi::DiagCode::UnknownFunction), "unknown-function");
    EXPECT_EQ(cppi::diag_key(cppi::DiagCode::BudgetExhausted), "budget-exhausted");
}

TEST(Diagnostics, EveryCodeHasItsOwnKey) {
    std::set<std::string_view> keys;
    for (unsigned n = 0; n < 1000; ++n) {
        const auto code = static_cast<cppi::DiagCode>(n);
        const std::string_view key = cppi::diag_key(code);
        if (key == "unknown") {
            continue;
        }
        EXPECT_TRUE(keys.insert(key).second) << "repeated key " << key;
        EXPECT_EQ(key.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-"), std::string_view::npos) << key;
        EXPECT_EQ(cppi::diag_id(code).size(), 5U) << key;
    }
    EXPECT_GT(keys.size(), 50U);
}

TEST(Diagnostics, DiagnosticArgumentsAreLookedUpByName) {
    cppi::Diagnostic d{cppi::DiagCode::ArgumentCountMismatch,
                       cppi::Severity::Error,
                       {{3, 1}, {3, 7}},
                       {{"function", std::string("move")}, {"expected", std::int64_t{1}}}};
    EXPECT_EQ(d.arg_text("function"), "move");
    EXPECT_EQ(d.arg_text("expected"), "1");
    EXPECT_EQ(d.arg("missing"), nullptr);
    EXPECT_TRUE(d.arg_text("missing").empty());
    EXPECT_EQ(cppi::to_debug_string(d), "3:1: error[E0202 argument-count-mismatch] function=move expected=1");
}
