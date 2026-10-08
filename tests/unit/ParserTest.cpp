#include "parse/Parser.hpp"

#include <gtest/gtest.h>

#include <string>
#include <variant>

using namespace cppi;

namespace {

const ast::CallExpr& as_call(const ast::Stmt& stmt) {
    const auto& expr_stmt = std::get<ast::ExprStmt>(stmt.node);
    return std::get<ast::CallExpr>(expr_stmt.expr.node);
}

bool uses_feature(const ast::Unsupported& u, Feature f) {
    for (const auto& use : u.uses) {
        if (use.feature == f) {
            return true;
        }
    }
    return false;
}

}  // namespace

TEST(Parser, TopLevelCallsBecomeExpressionStatements) {
    parse::Parser parser;
    auto result = parser.parse("harvest();\nmove(East); // go\n/* c */ plant(3, true);\n");
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.unit.statements.size(), 3);

    const auto& first = as_call(result.unit.statements[0]);
    EXPECT_EQ(first.callee, "harvest");
    EXPECT_TRUE(first.args.empty());

    const auto& second = as_call(result.unit.statements[1]);
    EXPECT_EQ(second.callee, "move");
    ASSERT_EQ(second.args.size(), 1);
    EXPECT_EQ(std::get<ast::Identifier>(second.args[0].node).name, "East");
    EXPECT_TRUE((second.callee_range.begin == SourceLocation{2, 1}));

    const auto& third = as_call(result.unit.statements[2]);
    ASSERT_EQ(third.args.size(), 2);
    EXPECT_EQ(std::get<ast::IntLiteral>(third.args[0].node).value, 3);
    EXPECT_TRUE(std::get<ast::BoolLiteral>(third.args[1].node).value);
}

TEST(Parser, IntegerLiteralsInEveryBase) {
    parse::Parser parser;
    auto result = parser.parse("f(42); f(0x2A); f(052); f(0b101010); f(1'000); f(7u); f(99999999999999999999);");
    ASSERT_TRUE(result.ok());
    auto literal = [&](std::size_t i) -> const ast::IntLiteral& {
        return std::get<ast::IntLiteral>(as_call(result.unit.statements[i]).args[0].node);
    };
    EXPECT_EQ(literal(0).value, 42);
    EXPECT_EQ(literal(1).value, 42);
    EXPECT_EQ(literal(2).value, 42);
    EXPECT_EQ(literal(3).value, 42);
    EXPECT_EQ(literal(3).requires_feature->feature, Feature::BinaryLiterals);
    EXPECT_EQ(literal(4).value, 1000);
    EXPECT_EQ(literal(4).requires_feature->feature, Feature::DigitSeparators);
    EXPECT_EQ(literal(5).value, 7);
    EXPECT_FALSE(literal(5).requires_feature.has_value());
    EXPECT_TRUE(literal(6).out_of_range);
}

TEST(Parser, ParenthesesAroundArgumentsAreTransparent) {
    parse::Parser parser;
    auto result = parser.parse("move(((East)));");
    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(std::holds_alternative<ast::Identifier>(as_call(result.unit.statements[0]).args[0].node));
}

TEST(Parser, EmptyStatementsAndCommentsAreIgnored) {
    parse::Parser parser;
    auto result = parser.parse(";;\n// only a comment\n");
    ASSERT_TRUE(result.ok());
    EXPECT_TRUE(result.unit.statements.empty());
}

TEST(Parser, AMissingSemicolonIsReportedPrecisely) {
    parse::Parser parser;
    auto result = parser.parse("harvest();\nmove(East)\nharvest();\n");
    ASSERT_EQ(result.diagnostics.size(), 1);
    const auto& d = result.diagnostics[0];
    EXPECT_EQ(d.code, DiagCode::MissingToken);
    EXPECT_EQ(d.arg_text("token"), ";");
    EXPECT_EQ(d.range.begin.line, 2);
}

TEST(Parser, AMissingTokenIsReportedRightAfterTheLastTokenBeforeIt) {
    struct Case {
        const char* source;
        const char* token;
        std::uint32_t line;
        std::uint32_t column;
        bool line_end;
    };
    const Case cases[] = {
        // ';' after a declaration: tree-sitter only sees an ERROR in `-5.0`.
        {"int a =-5.0\nstd::cout << a;\n", ";", 1, 12, true},
        {"int a = 5\nint b = 3;\n", ";", 1, 10, true},
        // ';' after a call, at the top level and in a function.
        {"move(East)\nmove(West);\n", ";", 1, 11, true},
        {"int x = 3;\nstd::cout << x\nx = 2;\n", ";", 2, 15, true},
        // ';' before '}': not after the '}', where tree-sitter puts it.
        {"void f() {\n    move(East)\n}\n", ";", 2, 15, true},
        {"void f() { int a = 3 }\n", ";", 1, 21, false},
        // ')' in an if, a while and a for.
        {"int a = 2;\nif (a < 3 {\n    move(East);\n}\n", ")", 2, 10, false},
        {"while (true {\n}\n", ")", 1, 12, false},
        {"for (int i = 0; i < 3; i++ {\n    move(East);\n}\n", ")", 1, 27, false},
        {"int x = (3 + 4;\n", ")", 1, 15, false},
        // '}' at the end: the block the script reading adds around the program does not hide it.
        {"void f() {\n    move(East);\n", "}", 2, 16, true},
        {"int main() {\n    for (int i = 0; i < 3; i++) {\n        move(East);\n    \n    return 0;\n}\n", "}", 6, 2,
         true},
    };
    parse::Parser parser;
    for (const Case& c : cases) {
        SCOPED_TRACE(c.source);
        auto result = parser.parse(c.source);
        ASSERT_EQ(result.diagnostics.size(), 1U);
        const auto& d = result.diagnostics[0];
        EXPECT_EQ(d.code, DiagCode::MissingToken);
        EXPECT_EQ(d.arg_text("token"), c.token);
        EXPECT_EQ(d.range.begin.line, c.line);
        EXPECT_EQ(d.range.begin.column, c.column);
        EXPECT_EQ(d.range.end, d.range.begin);
        EXPECT_EQ(d.arg_text("line"), c.line_end ? std::to_string(c.line) : "");
    }
}

TEST(Parser, EveryForgottenSemicolonIsReported) {
    parse::Parser parser;
    auto result = parser.parse("int main() {\n  int a = 1\n  int b = 2\n  return a + b\n}\n");
    ASSERT_EQ(result.diagnostics.size(), 3U);
    for (std::uint32_t i = 0; i < 3; ++i) {
        EXPECT_EQ(result.diagnostics[i].code, DiagCode::MissingToken);
        EXPECT_EQ(result.diagnostics[i].range.begin.line, i + 2);
    }
    EXPECT_EQ(result.diagnostics[2].range.begin.column, 15U);
}

TEST(Parser, OtherSyntaxErrorsStayGeneric) {
    parse::Parser parser;
    auto result = parser.parse("int x = 1;\nx = = 3;\n");
    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.diagnostics[0].code, DiagCode::SyntaxError);
}

TEST(Parser, GarbageIsASyntaxError) {
    parse::Parser parser;
    auto result = parser.parse("harvest(;;\n");
    ASSERT_FALSE(result.ok());
    EXPECT_TRUE(result.diagnostics.size() <= parse::Parser::kMaxSyntaxErrors);
}

TEST(Parser, StatementsCarryTheFeaturesTheyUseThemselves) {
    parse::Parser parser;
    auto uses = [](const ast::Stmt& stmt, Feature f) {
        for (const auto& use : stmt.uses) {
            if (use.feature == f) {
                return true;
            }
        }
        return false;
    };

    {
        SCOPED_TRACE("loops");
        auto result = parser.parse("while (true) { harvest(); }");
        ASSERT_TRUE(result.ok());
        const auto& stmt = result.unit.statements[0];
        EXPECT_TRUE(std::holds_alternative<ast::WhileStmt>(stmt.node));
        EXPECT_EQ(stmt.uses.front().feature, Feature::Loops);
        EXPECT_FALSE(uses(stmt, Feature::FunctionCalls));  // the body is checked on its own
    }
    {
        SCOPED_TRACE("if vs if constexpr");
        auto plain = parser.parse("if (true) harvest();");
        EXPECT_EQ(plain.unit.statements[0].uses.front().feature, Feature::Conditionals);
        auto cx = parser.parse("if constexpr (true) harvest();");
        EXPECT_EQ(cx.unit.statements[0].uses.front().feature, Feature::IfConstexpr);
        EXPECT_TRUE(std::get<ast::IfStmt>(cx.unit.statements[0].node).is_constexpr);
    }
    {
        SCOPED_TRACE("modern features inside a declaration");
        auto result = parser.parse("auto f = [](){ return nullptr; };");
        const auto& stmt = result.unit.statements[0];
        EXPECT_TRUE(uses(stmt, Feature::Variables));
        EXPECT_TRUE(uses(stmt, Feature::Auto));
        EXPECT_TRUE(uses(stmt, Feature::Lambdas));
        const auto& decl = std::get<ast::DeclStmt>(stmt.node);
        const auto& lambda = std::get<ast::LambdaExpr>(decl.vars.front().init->node);
        ASSERT_NE(lambda.function, nullptr);
        EXPECT_NE(lambda.function->body, nullptr);
        EXPECT_FALSE(lambda.has_return_type);
    }
    {
        SCOPED_TRACE("classes and inheritance");
        auto result = parser.parse("class Zanahoria : public Cultivo { };");
        const auto& stmt = result.unit.statements[0];
        const auto& record = std::get<ast::RecordDef>(stmt.node);
        EXPECT_EQ(record.name, "Zanahoria");
        ASSERT_EQ(record.bases.size(), 1);
        EXPECT_EQ(record.bases[0].name, "Cultivo");
        EXPECT_TRUE(uses(stmt, Feature::Classes));
        EXPECT_TRUE(uses(stmt, Feature::Inheritance));
    }
    {
        SCOPED_TRACE("enum vs enum class");
        auto plain = parser.parse("enum Crop { Grass };");
        EXPECT_EQ(plain.unit.statements[0].uses.front().feature, Feature::Enums);
        auto scoped = parser.parse("enum class Crop { Grass };");
        EXPECT_EQ(scoped.unit.statements[0].uses.front().feature, Feature::EnumClass);
        EXPECT_TRUE(std::get<ast::EnumDef>(scoped.unit.statements[0].node).scoped);
    }
    {
        SCOPED_TRACE("floating point arguments");
        auto result = parser.parse("f(1.5);");
        const auto& arg = as_call(result.unit.statements[0]).args[0];
        EXPECT_EQ(std::get<ast::FloatLiteral>(arg.node).value, 1.5);
        EXPECT_EQ(result.unit.statements[0].uses.front().feature, Feature::FunctionCalls);
    }
    {
        SCOPED_TRACE("constructs the converter does not know stay unsupported");
        auto result = parser.parse("#ifdef X\nharvest();\n#endif\n");
        const auto& u = std::get<ast::Unsupported>(result.unit.statements[0].node);
        EXPECT_EQ(u.uses.front().feature, Feature::Preprocessor);
        EXPECT_TRUE(uses_feature(u, Feature::FunctionCalls));
    }
}

TEST(Parser, DeclarationsAndControlFlowBecomeStructuredNodes) {
    parse::Parser parser;
    auto result = parser.parse(
        "int a[3] = {1, 2, 3}, *p;\n"
        "for (int i = 0; i < 3; ++i) { if (a[i] > 1) break; }\n"
        "int add(int x, const int& y) { return x + y; }\n"
        "switch (a[0]) { case 1: harvest(); default: break; }\n");
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.unit.statements.size(), 4);

    const auto& decl = std::get<ast::DeclStmt>(result.unit.statements[0].node);
    EXPECT_EQ(decl.type.name, "int");
    ASSERT_EQ(decl.vars.size(), 2);
    EXPECT_EQ(decl.vars[0].declarator.name, "a");
    ASSERT_EQ(decl.vars[0].declarator.parts.size(), 1);
    EXPECT_EQ(decl.vars[0].declarator.parts[0].kind, ast::DeclaratorKind::Array);
    EXPECT_EQ(decl.vars[0].style, ast::InitStyle::Copy);
    EXPECT_TRUE(std::holds_alternative<ast::InitList>(decl.vars[0].init->node));
    EXPECT_EQ(decl.vars[1].declarator.parts[0].kind, ast::DeclaratorKind::Pointer);

    const auto& loop = std::get<ast::ForStmt>(result.unit.statements[1].node);
    EXPECT_NE(loop.init, nullptr);
    EXPECT_NE(loop.condition, nullptr);
    EXPECT_TRUE(std::holds_alternative<ast::IncDecExpr>(loop.update->node));

    const auto& fn = std::get<ast::FunctionDef>(result.unit.statements[2].node).decl;
    EXPECT_EQ(fn.declarator.name, "add");
    ASSERT_EQ(fn.params.size(), 2);
    EXPECT_TRUE(fn.params[1].type.is_const);
    EXPECT_EQ(fn.params[1].declarator.parts[0].kind, ast::DeclaratorKind::Reference);
    EXPECT_NE(fn.body, nullptr);

    const auto& sw = std::get<ast::SwitchStmt>(result.unit.statements[3].node);
    ASSERT_EQ(sw.cases.size(), 2);
    EXPECT_NE(sw.cases[0].value, nullptr);
    EXPECT_EQ(sw.cases[1].value, nullptr);
}

TEST(Parser, ASignFoldedIntoALiteralIsAUnaryOperator) {
    parse::Parser parser;
    auto result = parser.parse("f(-1);");
    ASSERT_TRUE(result.ok());
    const auto& arg = as_call(result.unit.statements[0]).args[0];
    const auto& minus = std::get<ast::UnaryExpr>(arg.node);
    EXPECT_EQ(minus.op, ast::UnaryOp::Minus);
    EXPECT_EQ(std::get<ast::IntLiteral>(minus.operand->node).value, 1);
}

TEST(Parser, PathologicallyDeepNestingIsRejectedNotCrashedOn) {
    parse::Parser parser;
    std::string source = "f(";
    for (int i = 0; i < 5000; ++i) {
        source += "(";
    }
    source += "1";
    for (int i = 0; i < 5000; ++i) {
        source += ")";
    }
    source += ");";
    auto result = parser.parse(source);
    ASSERT_FALSE(result.ok());
    EXPECT_EQ(result.diagnostics[0].code, DiagCode::NestingTooDeep);
}
