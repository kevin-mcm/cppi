/// @file DiagnosticsTest.cpp
/// @brief Unit tests of diagnostics: stable codes, ids, keys and named
/// arguments.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "library/LibraryStandards.hpp"
#include "support/ProgramRunner.hpp"

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

namespace {

/// The first E0304 of `source`, or a default diagnostic when there is none.
cppi::Diagnostic library_error(std::string_view source, cppi::Standard standard) {
    auto r = cppi_test::run(source, cppi_test::standard(standard));
    for (const auto& d : r.compiled.diagnostics) {
        if (d.code == cppi::DiagCode::LibraryRequiresStandard) {
            return d;
        }
    }
    return {};
}

}  // namespace

TEST(Diagnostics, ALibraryNameNewerThanTheStandardGivesE0304) {
    const auto d = library_error("std::optional<int> o;\n", cppi::Standard::Cpp14);
    EXPECT_EQ(d.code, cppi::DiagCode::LibraryRequiresStandard);
    EXPECT_EQ(d.arg_text("name"), "std::optional");
    EXPECT_EQ(d.arg_text("standard"), "C++17");
    EXPECT_EQ(d.range.begin.line, 1);
    EXPECT_EQ(cppi::diag_id(cppi::DiagCode::LibraryRequiresStandard), "E0304");
    EXPECT_EQ(cppi::diag_key(cppi::DiagCode::LibraryRequiresStandard), "library-requires-standard");
    // Functions, values and `using namespace std`.
    EXPECT_EQ(library_error("auto p = std::make_unique<int>(3);\n", cppi::Standard::Cpp11).arg_text("name"),
              "std::make_unique");
    EXPECT_EQ(library_error("auto n = std::nullopt;\n", cppi::Standard::Cpp14).arg_text("name"), "std::nullopt");
    EXPECT_EQ(library_error("using namespace std;\nvariant<int, char> v;\n", cppi::Standard::Cpp14).arg_text("name"),
              "std::variant");
    // At or after the standard that introduced it, and for older names, nothing is reported.
    EXPECT_EQ(library_error("auto p = std::make_unique<int>(3);\n", cppi::Standard::Cpp14).code, cppi::DiagCode{});
    EXPECT_EQ(library_error("std::optional<int> o;\n", cppi::Standard::Cpp20).code, cppi::DiagCode{});
    EXPECT_EQ(library_error("std::vector<int> v;\nstd::map<int, int> m;\n", cppi::Standard::Cpp98).code,
              cppi::DiagCode{});
}

TEST(Diagnostics, ThePreludesOwnUseOfNewerNamesIsNeverReported) {
    // vector's constructor takes a std::initializer_list (C++11) and sort and
    // the adaptors use more; at C++98 none of that is the player's code.
    auto r = cppi_test::run(
        "std::vector<int> v(3);\nstd::sort(v.begin(), v.end());\nstd::priority_queue<int> q;\nq.push(1);\n",
        cppi_test::standard(cppi::Standard::Cpp98));
    EXPECT_TRUE(r.compiled.ok());
}

TEST(Diagnostics, AMemberOfALibraryClassCanHaveItsOwnStandard) {
    // A test-only entry: `optional::value_or` pretends to be from C++23.
    const cppi::library::ScopedEntry entry("optional::value_or", cppi::Standard::Cpp23);
    const std::string source = "std::optional<int> o = 3;\nint v = o.value_or(1);\n";
    const auto d = library_error(source, cppi::Standard::Cpp20);
    EXPECT_EQ(d.arg_text("name"), "std::optional::value_or");
    EXPECT_EQ(d.arg_text("standard"), "C++23");
    EXPECT_EQ(d.range.begin.line, 2);
    EXPECT_EQ(library_error(source, cppi::Standard::Cpp23).code, cppi::DiagCode{});
}
