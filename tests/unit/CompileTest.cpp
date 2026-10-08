#include "support/RecordingHost.hpp"
#include "support/Require.hpp"

#include <gtest/gtest.h>

#include <string_view>

using namespace cppi;

namespace {

struct Fixture {
    cppi_test::RecordingHost recorder;
    Interpreter interpreter;

    explicit Fixture(Options options = {}) : interpreter(recorder.make_registry(), options) {}

    CompileResult compile(std::string_view source) const { return interpreter.compile(source); }
};

const Diagnostic& only(const CompileResult& result) {
    EXPECT_EQ(result.diagnostics.size(), 1u);
    CPPI_REQUIRE(!result.diagnostics.empty());
    return result.diagnostics.front();
}

}  // namespace

TEST(Compile, ValidPhase1ProgramsCompile) {
    Fixture f;
    auto result = f.compile("harvest();\nmove(East);\nmove(next_direction());\ntake_int(count());\n");
    EXPECT_TRUE(result.ok());
    EXPECT_TRUE(result.diagnostics.empty());
}

TEST(Compile, UnknownNamesGetASuggestion) {
    Fixture f;

    auto fn = f.compile("harvset();");
    EXPECT_EQ(only(fn).code, DiagCode::UnknownFunction);
    EXPECT_EQ(only(fn).arg_text("name"), "harvset");
    EXPECT_EQ(only(fn).arg_text("suggestion"), "harvest");

    auto id = f.compile("move(Est);");
    EXPECT_EQ(only(id).code, DiagCode::UnknownIdentifier);
    EXPECT_EQ(only(id).arg_text("suggestion"), "East");

    auto far = f.compile("teleport();");
    EXPECT_EQ(only(far).arg("suggestion"), nullptr);
}

TEST(Compile, NamesLeftWithoutTheirNamespaceSuggestTheQualifiedName) {
    Fixture f;

    auto fn = f.compile("namespace a { void greet() {} }\nnamespace b { void greet() {} }\ngreet();");
    EXPECT_EQ(only(fn).code, DiagCode::UnknownFunction);
    EXPECT_EQ(only(fn).arg_text("suggestion"), "a::greet");

    auto nested = f.compile("namespace a { namespace b { int depth = 3; } }\nint x = depth;");
    EXPECT_EQ(only(nested).code, DiagCode::UnknownIdentifier);
    EXPECT_EQ(only(nested).arg_text("suggestion"), "a::b::depth");

    auto library = f.compile("cout << 1;");
    EXPECT_EQ(only(library).arg_text("suggestion"), "std::cout");
}

TEST(Compile, ArgumentCountAndTypesAreChecked) {
    Fixture f;

    auto count = f.compile("move();");
    EXPECT_EQ(only(count).code, DiagCode::ArgumentCountMismatch);
    EXPECT_EQ(only(count).arg_text("expected"), "1");
    EXPECT_EQ(only(count).arg_text("actual"), "0");

    auto type = f.compile("move(3);");
    EXPECT_EQ(only(type).code, DiagCode::ArgumentTypeMismatch);
    EXPECT_EQ(only(type).arg_text("expected"), "Direction");
    EXPECT_EQ(only(type).arg_text("actual"), "int");
    EXPECT_EQ(only(type).arg_text("index"), "1");

    auto void_arg = f.compile("take_int(move(East));");
    EXPECT_EQ(only(void_arg).code, DiagCode::ArgumentTypeMismatch);
    EXPECT_EQ(only(void_arg).arg_text("actual"), "void");
}

TEST(Compile, CImplicitConversionsAreHonored) {
    Fixture f;
    EXPECT_TRUE(f.compile("take_int(true);").ok());       // bool -> int
    EXPECT_TRUE(f.compile("take_int(East);").ok());       // unscoped enum -> int
    EXPECT_TRUE(f.compile("take_bool(5);").ok());         // int -> bool
    EXPECT_TRUE(f.compile("take_int(harvest());").ok());  // runtime bool -> int
    EXPECT_TRUE(f.compile("take_bool(count());").ok());   // runtime int -> bool
    EXPECT_FALSE(f.compile("move(true);").ok());          // nothing converts to an enum
}

TEST(Compile, MisusedNames) {
    Fixture f;

    auto not_callable = f.compile("East();");
    EXPECT_EQ(only(not_callable).code, DiagCode::NotCallable);

    auto as_value = f.compile("take_int(count);");
    EXPECT_EQ(only(as_value).code, DiagCode::FunctionNotCalled);
    EXPECT_EQ(only(as_value).severity, Severity::Error);

    // `harvest;` is valid (useless) C++: a warning, and the program still compiles.
    auto statement = f.compile("harvest;");
    EXPECT_TRUE(statement.ok());
    EXPECT_EQ(only(statement).code, DiagCode::FunctionNotCalled);
    EXPECT_EQ(only(statement).severity, Severity::Warning);

    auto no_effect = f.compile("42;");
    EXPECT_TRUE(no_effect.ok());
    EXPECT_EQ(only(no_effect).code, DiagCode::ExpressionHasNoEffect);
}

TEST(Compile, FeatureGatingExplainsWhyCodeIsRejected) {
    {
        SCOPED_TRACE("needs a newer standard beats everything else");
        Options options;
        options.locked.add(Feature::Variables);
        Fixture f(options);
        auto result = f.compile("auto f = [](){};");
        ASSERT_EQ(result.diagnostics.size(), 2);
        for (const auto& d : result.diagnostics) {
            EXPECT_EQ(d.code, DiagCode::FeatureRequiresStandard);
            EXPECT_EQ(d.arg_text("required"), "C++11");
            EXPECT_EQ(d.arg_text("current"), "C++98");
        }
    }
    {
        SCOPED_TRACE("locked by the game");
        Options options;
        options.locked.add(Feature::Loops);
        Fixture f(options);
        const auto result = f.compile("while (true) { harvest(); }");
        const auto& d = only(result);
        EXPECT_EQ(d.code, DiagCode::FeatureLocked);
        EXPECT_EQ(d.arg_text("feature"), "loops");
    }
    {
        SCOPED_TRACE("allowed but not implemented yet");
        Fixture f;
        const auto result = f.compile("#define LIMIT 3\n");
        const auto& d = only(result);
        EXPECT_EQ(d.code, DiagCode::FeatureNotImplemented);
        EXPECT_EQ(d.arg_text("feature"), "preprocessor");
    }
    {
        SCOPED_TRACE("the game can lock even function calls");
        Options options;
        options.locked.add(Feature::FunctionCalls);
        Fixture f(options);
        EXPECT_EQ(only(f.compile("harvest();")).code, DiagCode::FeatureLocked);
    }
    {
        SCOPED_TRACE("C++14 literals depend on the standard");
        Fixture old;
        EXPECT_EQ(only(old.compile("take_int(0b101);")).code, DiagCode::FeatureRequiresStandard);
        EXPECT_EQ(only(old.compile("take_int(1'000);")).code, DiagCode::FeatureRequiresStandard);

        Options cpp14;
        cpp14.standard = Standard::Cpp14;
        Fixture modern(cpp14);
        EXPECT_TRUE(modern.compile("take_int(0b101);").ok());
        EXPECT_TRUE(modern.compile("take_int(1'000);").ok());
    }
}

TEST(Compile, LiteralsThatDontFitAreReported) {
    Fixture f;
    EXPECT_EQ(only(f.compile("take_int(99999999999999999999);")).code, DiagCode::IntegerOutOfRange);
    EXPECT_TRUE(f.compile("take_int(9223372036854775807);").ok());
}

TEST(Compile, EveryStatementIsAnalyzedNotJustTheFirstError) {
    Fixture f;
    auto result = f.compile("harvset();\nmove(Est);\nmove();\n");
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.diagnostics.size(), 3);
}

TEST(Compile, SetOptionsChangesTheRulesForTheNextCompile) {
    Fixture f;
    EXPECT_TRUE(f.compile("harvest();").ok());
    Options locked;
    locked.locked.add(Feature::FunctionCalls);
    f.interpreter.set_options(locked);
    EXPECT_FALSE(f.compile("harvest();").ok());
}
