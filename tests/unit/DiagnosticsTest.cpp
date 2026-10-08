#include <gtest/gtest.h>

#include <cppi/Diagnostic.hpp>

TEST(Diagnostics, DiagnosticCodesHaveStableIdsAndKeys) {
    EXPECT_EQ(cppi::diag_id(cppi::DiagCode::UnknownFunction), "E0200");
    EXPECT_EQ(cppi::diag_key(cppi::DiagCode::UnknownFunction), "unknown-function");
    EXPECT_EQ(cppi::diag_key(cppi::DiagCode::BudgetExhausted), "budget-exhausted");
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
