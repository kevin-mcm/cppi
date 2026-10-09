/// @file VmTest.cpp
/// @brief Unit tests of the virtual machine: host calls, runtime conversions,
/// the cost model and the operation budget.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "codegen/ProgramData.hpp"
#include "support/RecordingHost.hpp"
#include "support/Require.hpp"
#include "vm/Vm.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace cppi;
using Calls = std::vector<std::string>;

namespace {

struct Fixture {
    cppi_test::RecordingHost recorder;
    Interpreter interpreter{recorder.make_registry()};

    Program compile(std::string_view source) const {
        auto result = interpreter.compile(source);
        SCOPED_TRACE(::testing::Message() << "compiling: " << source);
        CPPI_REQUIRE(result.ok());
        return *result.program;
    }
};

}  // namespace

TEST(Vm, HostFunctionsRunInProgramOrder) {
    Fixture f;
    auto program = f.compile("harvest();\nmove(East);\nmove(next_direction());\nharvest();");
    auto result = f.interpreter.run(program);
    EXPECT_TRUE(result.ok());
    EXPECT_TRUE(result.diagnostics.empty());
    EXPECT_TRUE((f.recorder.calls == Calls{"harvest()", "move(East)", "next_direction()", "move(South)", "harvest()"}));
}

TEST(Vm, ImplicitConversionsHappenAtRuntime) {
    Fixture f;
    auto program = f.compile("take_bool(count());\ntake_int(harvest());\ntake_bool(0);\ntake_int(West);");
    EXPECT_TRUE(f.interpreter.run(program).ok());
    EXPECT_TRUE((f.recorder.calls ==
                 Calls{"count()", "take_bool(true)", "harvest()", "take_int(1)", "take_bool(false)", "take_int(3)"}));
}

TEST(Vm, OperationsAreCountedWithTheCostModel) {
    Fixture f;
    // harvest: CALL (1 + 5) + POP (1) = 7. move(East): PUSH (1) + CALL (1 + 5) = 7.
    auto program = f.compile("harvest();\nmove(East);");
    EXPECT_EQ(f.interpreter.run(program).operations, 14);

    RunOptions free_instructions;
    free_instructions.cost.instruction = 0;
    EXPECT_EQ(f.interpreter.run(program, free_instructions).operations, 10);
}

TEST(Vm, TheBudgetStopsExecutionBeforeAnActionThatDoesNotFit) {
    Fixture f;
    auto program = f.compile("harvest();\nmove(East);\nharvest();");

    RunOptions options;
    options.budget = 13;  // harvest (7) fits, move (7) would need 14
    auto result = f.interpreter.run(program, options);

    EXPECT_EQ(result.status, RunStatus::BudgetExhausted);
    EXPECT_EQ(result.operations, 8);  // harvest (7) + PUSH_CONST (1)
    EXPECT_EQ(f.recorder.calls, Calls{"harvest()"});
    ASSERT_EQ(result.diagnostics.size(), 1);
    EXPECT_EQ(result.diagnostics[0].code, DiagCode::BudgetExhausted);
    EXPECT_EQ(result.diagnostics[0].range.begin.line, 2);
    EXPECT_EQ(result.diagnostics[0].arg_text("budget"), "13");
}

TEST(Vm, AnExactBudgetIsEnough) {
    Fixture f;
    auto program = f.compile("harvest();");
    RunOptions options;
    options.budget = 7;
    EXPECT_TRUE(f.interpreter.run(program, options).ok());
}

TEST(Vm, HostFailuresStopTheProgramWithADiagnostic) {
    Fixture f;
    f.recorder.fail_next_move = true;
    auto program = f.compile("harvest();\nmove(North);\nharvest();");
    auto result = f.interpreter.run(program);

    EXPECT_EQ(result.status, RunStatus::HostError);
    EXPECT_EQ(f.recorder.calls, Calls{"harvest()"});
    ASSERT_EQ(result.diagnostics.size(), 1);
    const auto& d = result.diagnostics[0];
    EXPECT_EQ(d.code, DiagCode::HostError);
    EXPECT_EQ(d.arg_text("function"), "move");
    EXPECT_EQ(d.arg_text("error"), "7");
    EXPECT_EQ(d.arg_text("detail"), "blocked");
    EXPECT_EQ(d.range.begin.line, 2);
}

TEST(Vm, HostExceptionsAndContractViolationsAreContained) {
    Fixture f;

    auto thrown = f.interpreter.run(f.compile("boom();"));
    EXPECT_EQ(thrown.status, RunStatus::HostError);
    EXPECT_NE(thrown.diagnostics[0].arg_text("detail").find("kaboom"), std::string::npos);

    auto lied = f.interpreter.run(f.compile("liar();"));
    EXPECT_EQ(lied.status, RunStatus::HostError);
    EXPECT_NE(lied.diagnostics[0].arg_text("detail").find("expected int"), std::string::npos);
}

TEST(Vm, ExecutionCanAdvanceOneInstructionAtATime) {
    Fixture f;
    auto program = f.compile("move(East);\nharvest();");
    auto execution = f.interpreter.start(program);

    EXPECT_EQ(execution.current_location().begin.line, 1);
    EXPECT_EQ(execution.step(), RunStatus::Running);  // PUSH_CONST East
    EXPECT_TRUE(f.recorder.calls.empty());
    EXPECT_EQ(execution.step(), RunStatus::Running);  // CALL move
    EXPECT_EQ(f.recorder.calls, Calls{"move(East)"});
    EXPECT_EQ(execution.current_location().begin.line, 2);
    EXPECT_EQ(execution.operations(), 7);

    auto rest = execution.run();
    EXPECT_TRUE(rest.ok());
    EXPECT_TRUE(execution.finished());
    EXPECT_EQ(execution.step(), RunStatus::Completed);  // stepping a finished run is harmless
    EXPECT_TRUE((f.recorder.calls == Calls{"move(East)", "harvest()"}));
}

TEST(Vm, ObserversSeeEveryHostCallAndStep) {
    struct Spy : ExecutionObserver {
        std::vector<std::string> calls;
        int steps = 0;
        std::uint64_t last_operations = 0;
        void on_host_call(const HostCall& call) override { calls.emplace_back(call.function_name()); }
        void on_step(SourceRange, std::uint64_t ops) override {
            ++steps;
            last_operations = ops;
        }
    };

    Fixture f;
    Spy spy;
    RunOptions options;
    options.observer = &spy;
    auto result = f.interpreter.run(f.compile("harvest();\nmove(West);"), options);
    EXPECT_TRUE(result.ok());
    EXPECT_TRUE((spy.calls == std::vector<std::string>{"harvest", "move"}));
    EXPECT_EQ(spy.steps, 4);  // CALL, POP, PUSH, CALL
    EXPECT_EQ(spy.last_operations, result.operations);
}

TEST(Vm, AnEmptyProgramCompletesImmediately) {
    Fixture f;
    auto result = f.interpreter.run(f.compile("// nothing to do\n"));
    EXPECT_TRUE(result.ok());
    EXPECT_EQ(result.operations, 0);
}

TEST(Vm, OperationsArePerLineAndAddUp) {
    Fixture f;
    auto program = f.compile("harvest();\nfor (int i = 0; i < 3; i++) {\n    move(East);\n}\n");
    auto result = f.interpreter.run(program);
    ASSERT_TRUE(result.ok());
    ASSERT_EQ(result.line_operations.size(), 3U);  // the closing brace costs nothing
    EXPECT_EQ(result.line_operations[0], (LineOperations{1, 7}));
    EXPECT_EQ(result.line_operations[1].line, 2U);  // the loop's counter and condition
    EXPECT_EQ(result.line_operations[2], (LineOperations{3, 3 * 7}));
    std::uint64_t total = 0;
    for (const LineOperations& l : result.line_operations) {
        total += l.operations;
    }
    EXPECT_EQ(total, result.operations);

    // An execution has its profile so far.
    auto execution = f.interpreter.start(program);
    while (execution.step() == RunStatus::Running) {
    }
    EXPECT_EQ(execution.line_operations(), result.line_operations);
}

TEST(Vm, TheLibraryAndHostFunctionsAreChargedToTheCallingLine) {
    Fixture f;
    auto program = f.compile("std::vector<int> v;\nv.push_back(1);\nv.push_back(2);\nharvest();\n");
    RunOptions paid_library;
    paid_library.cost.library_instruction = 1;
    auto result = f.interpreter.run(program, paid_library);
    ASSERT_TRUE(result.ok());
    std::uint64_t total = 0;
    for (const LineOperations& l : result.line_operations) {
        EXPECT_NE(l.line, 0U);
        EXPECT_LE(l.line, 4U);
        total += l.operations;
    }
    EXPECT_EQ(total, result.operations);
    ASSERT_EQ(result.line_operations.size(), 4U);
    EXPECT_GT(result.line_operations[1].operations, 10U);  // push_back's own code
    EXPECT_EQ(result.line_operations[3], (LineOperations{4, 7}));

    // Free library: only the calls are left.
    auto free_library = f.interpreter.run(program);
    EXPECT_LT(free_library.line_operations[1].operations, 10U);
}

TEST(Vm, ACountingLoopCostsItsBodyPlusThreeOperationsPerIteration) {
    // i = 0 (2), jump to the test (1), 10 x (harvest(); (7) + i++ (1) + the test
    // i < 10 (3)), and the last test (3). See CodeGenerator for the layout.
    Fixture f;
    auto program = f.compile("for (int i = 0; i < 10; i++) {\n    harvest();\n}\n");
    EXPECT_EQ(f.interpreter.run(program).operations, 2 + 1 + 10 * (7 + 1 + 3) + 3);
}

TEST(Vm, ShorterCodeKeepsEveryRuntimeCheck) {
    Fixture f;
    // ++ on an int or long variable is one instruction that still checks overflow...
    auto overflow = f.interpreter.run(f.compile("int big = 2147483647;\nbig++;\ntake_int(big);"));
    ASSERT_EQ(overflow.diagnostics.size(), 1U);
    EXPECT_EQ(overflow.diagnostics[0].code, DiagCode::IntegerOverflow);
    EXPECT_EQ(overflow.diagnostics[0].arg_text("type"), "int");
    EXPECT_EQ(overflow.diagnostics[0].range.begin.line, 2);
    auto wide = f.interpreter.run(f.compile("long big = 9223372036854775807;\nbig++;"));
    ASSERT_EQ(wide.diagnostics.size(), 1U);
    EXPECT_EQ(wide.diagnostics[0].arg_text("type"), "long");
    // ...and reading an uninitialized one.
    auto uninitialized =
        f.interpreter.run(f.compile("int f() {\n    int x;\n    x--;\n    return x;\n}\ntake_int(f());"));
    ASSERT_EQ(uninitialized.diagnostics.size(), 1U);
    EXPECT_EQ(uninitialized.diagnostics[0].code, DiagCode::UninitializedRead);
    EXPECT_EQ(uninitialized.diagnostics[0].arg_text("name"), "x");
    EXPECT_EQ(uninitialized.diagnostics[0].range.begin.line, 3);
    // Constants are folded, but not an operation that overflows: that still happens at runtime.
    auto folded =
        f.interpreter.run(f.compile("const int kMax = 2147483647;\ntake_int(kMax - 1);\ntake_int(kMax + 1);"));
    EXPECT_EQ(f.recorder.calls.back(), "take_int(2147483646)");
    ASSERT_EQ(folded.diagnostics.size(), 1U);
    EXPECT_EQ(folded.diagnostics[0].code, DiagCode::IntegerOverflow);
    EXPECT_EQ(folded.diagnostics[0].range.begin.line, 3);
}

TEST(Vm, ConditionsThatJumpKeepCppSemantics) {
    Fixture f;
    // NaN compares false both ways: `!(n < 1)` is not `n >= 1`.
    EXPECT_TRUE(f.interpreter
                    .run(f.compile(R"(
        double zero = 0.0;
        double nan = zero / zero;
        take_bool(nan < 1.0);
        if (!(nan < 1.0)) { take_int(1); }
        if (!(nan >= 1.0)) { take_int(2); }
        if (nan != nan) { take_int(3); }
        unsigned int u = 3000000000u;
        if (u > 5u) { take_int(4); }
        int a[3] = {1, 2, 3};
        int* p = a;
        while (p < a + 2) { ++p; }
        if (p == a + 2) { take_int(5); }
    )"))
                    .ok());
    EXPECT_EQ(f.recorder.calls,
              (Calls{"take_bool(false)", "take_int(1)", "take_int(2)", "take_int(3)", "take_int(4)", "take_int(5)"}));
    // && and || still evaluate left to right and stop early.
    f.recorder.calls.clear();
    EXPECT_TRUE(f.interpreter
                    .run(f.compile(R"(
        for (int i = 0; i < 3; i++) {
            if (count() > 1 && count() > 3 || count() == 0) { take_int(i); }
        }
    )"))
                    .ok());
    // count() returns 1, 2, 3...: i = 0 calls it twice (1 > 1 fails, then 2 == 0),
    // i = 1 twice (3 > 1, 4 > 3) and i = 2 twice (5 > 1, 6 > 3).
    EXPECT_EQ(f.recorder.calls,
              (Calls{"count()", "count()", "count()", "count()", "take_int(1)", "count()", "count()", "take_int(2)"}));
}

TEST(Vm, TheStatementUnitChargesEachStatementAndEachConditionOnce) {
    Fixture f;
    // A loop of 10 iterations with 2 statements in its body.
    auto program = f.compile("for (int i = 0; i < 10; i++) {\n    take_int(i);\n    take_int(i * 2);\n}\n");
    RunOptions statements;
    statements.cost.unit = CostModel::Unit::Statement;
    // int i = 0 (1), the condition 11 times (11), the body 10 x 2 (20); i++ is
    // part of the loop. take_int adds its host cost (1) to each call, in both units.
    auto by_statement = f.interpreter.run(program, statements);
    EXPECT_EQ(by_statement.operations, 1 + 11 + 20 + 20);
    ASSERT_EQ(by_statement.line_operations.size(), 3U);
    EXPECT_EQ(by_statement.line_operations[0], (LineOperations{1, 12}));
    EXPECT_EQ(by_statement.line_operations[1], (LineOperations{2, 10 + 10}));
    EXPECT_EQ(by_statement.line_operations[2], (LineOperations{3, 10 + 10}));
    // The default unit charges instructions: i = 0 (2), jump to the test (1),
    // 10 x (take_int(i) (2) + take_int(i * 2) (4) + i++ (1) + the test (3)), the last test (3).
    EXPECT_EQ(f.interpreter.run(program).operations, 2 + 1 + 10 * (2 + 4 + 1 + 3) + 3 + 20);

    // `statement` sets the price; host functions add their own cost as usual.
    statements.cost.statement = 2;
    EXPECT_EQ(f.interpreter.run(f.compile("harvest();\nmove(East);"), statements).operations, 2 * 2 + 5 + 5);
}

TEST(Vm, TheStatementUnitChargesConditionsOfEveryKindOfStatement) {
    Fixture f;
    RunOptions statements;
    statements.cost.unit = CostModel::Unit::Statement;
    const auto cost = [&](const char* source) { return f.interpreter.run(f.compile(source), statements).operations; };
    EXPECT_EQ(cost("int a = 1;\nif (a > 0) {\n    a = 2;\n} else {\n    a = 3;\n}"), 3);
    EXPECT_EQ(cost("int n = 0;\nwhile (n < 3) {\n    n++;\n}"), 1 + 4 + 3);
    EXPECT_EQ(cost("int n = 0;\ndo {\n    n++;\n} while (n < 3);"), 1 + 3 + 3);
    EXPECT_EQ(cost("int n = 0;\nswitch (n) {\n    case 0: n = 5; break;\n    default: n = 6;\n}"), 1 + 1 + 2);
    // n = 0, three conditions of the if, two jumps back to the top of for (;;), the break.
    EXPECT_EQ(cost("int n = 0;\nfor (;;) {\n    if (++n == 3) break;\n}"), 1 + 3 + 2 + 1);
    // A function call costs its statement, then the function's own statements
    // (plus take_int's host cost).
    EXPECT_EQ(cost("int twice(int x) {\n    return x * 2;\n}\ntake_int(twice(3));"), 1 + 1 + 1);
    // An empty endless loop still runs out of operations.
    RunOptions limited = statements;
    limited.budget = 100;
    EXPECT_EQ(f.interpreter.run(f.compile("while (true) {\n}"), limited).status, RunStatus::BudgetExhausted);
    EXPECT_EQ(f.interpreter.run(f.compile("for (;;) {}"), limited).status, RunStatus::BudgetExhausted);
}

TEST(Vm, AnOutOfRangeDoubleConstantIsNotFoldedIntoAnInteger) {
    Fixture f;
    for (const char* source : {"int x = 1e19;\ntake_int(x);", "unsigned int u = -1.5;\ntake_long(u);",
                               "const double big = 3e9;\nint x = big;\ntake_int(x);"}) {
        SCOPED_TRACE(source);
        auto result = f.interpreter.run(f.compile(source));
        ASSERT_EQ(result.diagnostics.size(), 1U);
        EXPECT_EQ(result.diagnostics[0].code, DiagCode::IntegerOverflow);
    }
    f.recorder.calls.clear();
    EXPECT_TRUE(f.interpreter.run(f.compile("int y = 2.7;\ntake_int(y);")).ok());
    EXPECT_EQ(f.recorder.calls, Calls{"take_int(2)"});
}

TEST(Vm, AnOperandStackUnderflowIsAnInternalErrorNotACrash) {
    // Code generation never emits these; a bug that did must stop the run, not read past the stack.
    Fixture f;
    const Program program = f.compile("harvest();");
    for (const detail::Instruction bad :
         {detail::Instruction{detail::OpCode::Pop}, detail::Instruction{detail::OpCode::Dup},
          detail::Instruction{detail::OpCode::Swap}, detail::Instruction{detail::OpCode::Over},
          detail::Instruction{detail::OpCode::Ret, 0, 1, 0}}) {
        SCOPED_TRACE(detail::to_string(bad.op));
        auto data = std::make_shared<detail::ProgramData>(program.data());
        data->code[data->functions.front().entry] = bad;
        detail::Vm vm(data, RunOptions{});
        const RunResult result = vm.run();
        EXPECT_EQ(result.status, RunStatus::RuntimeError);
        ASSERT_EQ(result.diagnostics.size(), 1U);
        EXPECT_EQ(result.diagnostics[0].code, DiagCode::InternalError);
        EXPECT_EQ(result.diagnostics[0].args.at(0).name, "what");
    }
}
