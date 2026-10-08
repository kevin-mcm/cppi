/// @file NumberFormatTest.cpp
/// @brief Floating-point literals and std::cout output of doubles, which must
/// not depend on iostreams or on the locale (see support/NumberFormat.hpp).
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "parse/Parser.hpp"
#include "support/NumberFormat.hpp"
#include "support/ProgramRunner.hpp"

#include <gtest/gtest.h>

#include <clocale>
#include <cmath>
#include <cstring>
#include <limits>
#include <locale>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>

using namespace cppi;
using cppi::detail::NumberFormat;
using cppi_test::printed;
using cppi_test::standard;

namespace {

/// The value of the literal `spelling` as cppi's parser reads it.
std::optional<double> literal(const std::string& spelling) {
    parse::Parser parser;
    auto result = parser.parse("f(" + spelling + ");");
    if (!result.ok() || result.unit.statements.size() != 1) {
        return std::nullopt;
    }
    const auto& call = std::get<ast::CallExpr>(std::get<ast::ExprStmt>(result.unit.statements[0].node).expr.node);
    if (const auto* f = std::get_if<ast::FloatLiteral>(&call.args.at(0).node)) {
        return f->value;
    }
    return std::nullopt;
}

bool same_bits(double a, double b) {
    return std::memcmp(&a, &b, sizeof a) == 0;
}

/// What std::cout prints for `value` by default (precision 6, "C" locale).
std::string cout_text(double value) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << value;
    return out.str();
}

void expect_literals_and_output() {
    struct Literal {
        const char* spelling;
        double value;
    };
    const Literal literals[] = {
        {"5.0", 5.0},
        {"1e10", 1e10},
        {"2.5e-3", 2.5e-3},
        {".5", .5},
        {"5.", 5.},
        {"1'000.5", 1000.5},
        {"3.0f", 3.0},
        {"2.5L", 2.5},
        {"0.1", 0.1},
        {"1E-7", 1E-7},
        {"6.02214076e23", 6.02214076e23},
        {"123456789012345678901234567890.0", 123456789012345678901234567890.0},
        {"0x1.8p3", 0x1.8p3},
        {"0x1Fp-2f", 0x1Fp-2},
    };
    for (const Literal& l : literals) {
        SCOPED_TRACE(l.spelling);
        const auto value = literal(l.spelling);
        ASSERT_TRUE(value.has_value());
        EXPECT_TRUE(same_bits(*value, l.value)) << *value;
    }

    const double values[] = {0.1 + 0.2,
                             1e21,
                             123456789.0,
                             -0.0,
                             0.0,
                             5.0,
                             2.5e-3,
                             1e-5,
                             100000.0,
                             1000000.0,
                             -1234.5678,
                             1.0 / 3.0,
                             std::numeric_limits<double>::infinity(),
                             -std::numeric_limits<double>::infinity(),
                             std::numeric_limits<double>::quiet_NaN(),
                             std::numeric_limits<double>::max(),
                             std::numeric_limits<double>::denorm_min()};
    for (const double v : values) {
        EXPECT_EQ(NumberFormat::shortest(v), cout_text(v));
    }
    EXPECT_EQ(NumberFormat::shortest(0.1 + 0.2), "0.3");
    EXPECT_EQ(NumberFormat::shortest(1e21), "1e+21");
    EXPECT_EQ(NumberFormat::shortest(123456789.0), "1.23457e+08");
    EXPECT_EQ(NumberFormat::shortest(-0.0), "-0");
    EXPECT_EQ(NumberFormat::shortest(std::numeric_limits<double>::infinity()), "inf");
    EXPECT_EQ(NumberFormat::shortest(std::numeric_limits<double>::quiet_NaN()), "nan");

    // End to end: the literals as the player writes them, printed with std::cout.
    EXPECT_EQ(printed(R"(
        std::cout << 5.0 << " " << -0.0 << " " << 1e10 << " " << 2.5e-3 << " " << .5 << " " << 5. << " "
                  << 1'000.5 << " " << 3.0f << std::endl;
        std::cout << 0.1 + 0.2 << " " << 1e21 << " " << 123456789.0 << std::endl;
    )",
                      standard(Standard::Cpp14)),
              "5 -0 1e+10 0.0025 0.5 5 1000.5 3\n0.3 1e+21 1.23457e+08\n");
}

/// Restores the C and C++ global locales when the test ends.
class LocaleGuard {
public:
    LocaleGuard() : cpp_(std::locale()), c_(std::setlocale(LC_ALL, nullptr)) {}
    LocaleGuard(const LocaleGuard&) = delete;
    LocaleGuard& operator=(const LocaleGuard&) = delete;
    LocaleGuard(LocaleGuard&&) = delete;
    LocaleGuard& operator=(LocaleGuard&&) = delete;
    ~LocaleGuard() {
        std::locale::global(cpp_);
        std::setlocale(LC_ALL, c_.c_str());
    }

private:
    std::locale cpp_;
    std::string c_;
};

}  // namespace

TEST(NumberFormat, LiteralsAndOutputMatchStdCout) {
    expect_literals_and_output();
}

TEST(NumberFormat, MalformedOrOutOfRangeTextIsRejected) {
    EXPECT_FALSE(NumberFormat::parse("").has_value());
    EXPECT_FALSE(NumberFormat::parse("1.5x").has_value());
    EXPECT_FALSE(NumberFormat::parse("-1.5").has_value());
    EXPECT_FALSE(NumberFormat::parse("inf").has_value());
    EXPECT_FALSE(NumberFormat::parse("1e999").has_value());
    EXPECT_FALSE(literal("1e999").has_value());  // ill-formed in C++
}

TEST(NumberFormat, AGlobalLocaleWithADecimalCommaChangesNothing) {
    const LocaleGuard guard;
    bool installed = false;
    for (const char* name : {"de_DE.UTF-8", "de_DE.utf8", "de_DE", "es_ES.UTF-8", "fr_FR.UTF-8", "ru_RU.UTF-8"}) {
        try {
            std::locale::global(std::locale(name));
        } catch (const std::runtime_error&) {
            continue;
        }
        if (std::setlocale(LC_ALL, name) == nullptr) {
            continue;
        }
        installed = true;
        break;
    }
    if (!installed) {
        GTEST_SKIP() << "no locale with a decimal comma is installed";
    }
    {
        std::ostringstream check;
        check << 1.5;
        ASSERT_EQ(check.str(), "1,5");  // the locale really is in effect
    }
    expect_literals_and_output();
}
