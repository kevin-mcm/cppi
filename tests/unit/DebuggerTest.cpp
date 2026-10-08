// Phase 4 teaching tools: breakpoints, stepping, variable inspection and the
// program cache.

#include "support/RecordingHost.hpp"

#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <vector>

using namespace cppi;

namespace {

const Variable* find(const std::vector<Variable>& vars, std::string_view name) {
    for (const auto& v : vars) {
        if (v.name == name) {
            return &v;
        }
    }
    return nullptr;
}

}  // namespace

TEST(Debugger, BreakpointsPauseExecutionAndRunContinues) {
    cppi_test::RecordingHost recorder;
    Interpreter interpreter(recorder.make_registry());
    auto compiled = interpreter.compile("take_int(1);\ntake_int(2);\ntake_int(3);\n");
    ASSERT_TRUE(compiled.ok());
    auto execution = interpreter.start(*compiled.program);
    execution.set_breakpoint(2);

    auto first = execution.run();
    EXPECT_EQ(first.status, RunStatus::Paused);
    EXPECT_FALSE(execution.finished());
    EXPECT_EQ(execution.current_location().begin.line, 2);
    EXPECT_EQ(recorder.calls, std::vector<std::string>{"take_int(1)"});

    auto rest = execution.run();
    EXPECT_EQ(rest.status, RunStatus::Completed);
    EXPECT_EQ(recorder.calls.size(), 3);
}

TEST(Debugger, ABreakpointInsideALoopPausesOnEveryIteration) {
    cppi_test::RecordingHost recorder;
    Interpreter interpreter(recorder.make_registry());
    auto compiled = interpreter.compile("for (int i = 0; i < 3; i++) {\n  take_int(i);\n}\n");
    ASSERT_TRUE(compiled.ok());
    auto execution = interpreter.start(*compiled.program);
    execution.set_breakpoint(2);
    int pauses = 0;
    while (execution.run().status == RunStatus::Paused) {
        ++pauses;
    }
    EXPECT_EQ(pauses, 3);
}

TEST(Debugger, TheCallStackShowsFunctionsAndTheirVariables) {
    cppi_test::RecordingHost recorder;
    Interpreter interpreter(recorder.make_registry());
    const char* source =
        "struct Point { int x; int y; };\n"
        "int total = 0;\n"
        "void visit(int depth, Point p) {\n"
        "  int seen;\n"
        "  int grid[3] = {7, 8, 9};\n"
        "  take_int(depth);\n"
        "}\n"
        "Point start = {1, 2};\n"
        "visit(5, start);\n";
    auto compiled = interpreter.compile(source);
    ASSERT_TRUE(compiled.ok());
    auto execution = interpreter.start(*compiled.program);
    execution.set_breakpoint(6);
    ASSERT_EQ(execution.run().status, RunStatus::Paused);

    const auto stack = execution.call_stack();
    ASSERT_EQ(stack.size(), 2);
    EXPECT_EQ(stack[0].function, "visit");
    EXPECT_EQ(stack[0].location.begin.line, 6);
    EXPECT_EQ(stack[1].function, "<script>");
    EXPECT_EQ(stack[1].location.begin.line, 9);

    const auto& locals = stack[0].locals;
    const Variable* depth = find(locals, "depth");
    ASSERT_NE(depth, nullptr);
    EXPECT_EQ(depth->value, "5");
    EXPECT_EQ(depth->type, "int");
    const Variable* seen = find(locals, "seen");
    ASSERT_NE(seen, nullptr);
    EXPECT_FALSE(seen->initialized);  // reading it would be undefined behavior
    const Variable* grid = find(locals, "grid");
    ASSERT_NE(grid, nullptr);
    EXPECT_EQ(grid->value, "{7, 8, 9}");
    ASSERT_EQ(grid->children.size(), 3);
    const Variable* p = find(locals, "p");
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(p->value, "{x=1, y=2}");

    const auto globals = execution.globals();
    ASSERT_NE(find(globals, "total"), nullptr);
    EXPECT_EQ(find(globals, "total")->value, "0");
}

TEST(Debugger, PointersAreShownByWhatTheyPointTo) {
    cppi_test::RecordingHost recorder;
    Interpreter interpreter(recorder.make_registry());
    auto compiled = interpreter.compile("int a[3] = {1, 2, 3};\nint* p = &a[1];\nint* q = 0;\ntake_int(*p);\n");
    ASSERT_TRUE(compiled.ok());
    auto execution = interpreter.start(*compiled.program);
    execution.set_breakpoint(4);
    ASSERT_EQ(execution.run().status, RunStatus::Paused);
    const auto globals = execution.globals();
    const Variable* p = find(globals, "p");
    const Variable* q = find(globals, "q");
    ASSERT_NE(p, nullptr);
    ASSERT_NE(q, nullptr);
    EXPECT_EQ(p->value, "&a[1]");
    EXPECT_EQ(q->value, "nullptr");
}

TEST(Debugger, StepLineAdvancesOneSourceLineAtATime) {
    cppi_test::RecordingHost recorder;
    Interpreter interpreter(recorder.make_registry());
    auto compiled = interpreter.compile("int a = 1;\nint b = a + 1;\ntake_int(b);\n");
    ASSERT_TRUE(compiled.ok());
    auto execution = interpreter.start(*compiled.program);
    EXPECT_EQ(execution.current_location().begin.line, 1);
    execution.step_line();
    EXPECT_EQ(execution.current_location().begin.line, 2);
    execution.step_line();
    EXPECT_EQ(execution.current_location().begin.line, 3);
    EXPECT_TRUE(recorder.calls.empty());
    execution.step_line();
    EXPECT_EQ(recorder.calls, std::vector<std::string>{"take_int(2)"});
}

TEST(Debugger, TheProgramCacheCompilesEachSourceOnce) {
    cppi_test::RecordingHost recorder;
    Interpreter interpreter(recorder.make_registry());
    ProgramCache cache(2);

    auto a = cache.compile(interpreter, "harvest();");
    auto b = cache.compile(interpreter, "harvest();");
    ASSERT_TRUE(a.ok());
    EXPECT_EQ(cache.hits(), 1);
    EXPECT_EQ(cache.misses(), 1);
    EXPECT_TRUE(&a.program->data() == &b.program->data());  // the same compiled program

    // Different rules mean a different entry.
    Options locked;
    locked.locked.add(Feature::FunctionCalls);
    interpreter.set_options(locked);
    auto c = cache.compile(interpreter, "harvest();");
    EXPECT_FALSE(c.ok());
    EXPECT_EQ(cache.misses(), 2);

    // Least recently used entries are evicted.
    (void)cache.compile(interpreter, "move(East);");
    EXPECT_EQ(cache.size(), 2);
}

TEST(DebuggerThreads, TheProgramCacheIsSafeToShareBetweenThreads) {
    cppi_test::RecordingHost recorder;
    const Interpreter interpreter(recorder.make_registry());
    ProgramCache cache;
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < 50; ++i) {
                (void)cache.compile(interpreter, i % 2 == 0 ? "harvest();" : "move(East);");
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    EXPECT_EQ(cache.size(), 2);
    EXPECT_EQ(cache.hits() + cache.misses(), 200);
}

TEST(Debugger, StdVectorShowsItsElementsNotItsInternals) {
    cppi_test::RecordingHost recorder;
    Interpreter interpreter(recorder.make_registry());
    auto compiled = interpreter.compile(
        "std::vector<int> v;\nv.push_back(4);\nv.push_back(7);\nstd::vector<int> empty;\ntake_int(v[0]);\n");
    ASSERT_TRUE(compiled.ok());
    auto execution = interpreter.start(*compiled.program);
    execution.set_breakpoint(5);
    ASSERT_EQ(execution.run().status, RunStatus::Paused);
    const auto globals = execution.globals();
    const Variable* v = find(globals, "v");
    ASSERT_NE(v, nullptr);
    EXPECT_EQ(v->type, "vector<int>");
    EXPECT_EQ(v->value, "{4, 7}");
    ASSERT_EQ(v->children.size(), 2);
    EXPECT_EQ(v->children[0].name, "[0]");
    EXPECT_EQ(v->children[1].value, "7");
    const Variable* e = find(globals, "empty");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->value, "{}");
    EXPECT_TRUE(e->children.empty());
}

TEST(Debugger, StdVectorOfRecordsShowsEachRecord) {
    cppi_test::RecordingHost recorder;
    Interpreter interpreter(recorder.make_registry());
    auto compiled = interpreter.compile(
        "struct P { int x; int y; };\nstd::vector<P> ps;\nP a; a.x = 1; a.y = "
        "2;\nps.push_back(a);\ntake_int(ps.size());\n");
    ASSERT_TRUE(compiled.ok());
    auto execution = interpreter.start(*compiled.program);
    execution.set_breakpoint(5);
    ASSERT_EQ(execution.run().status, RunStatus::Paused);
    const auto globals = execution.globals();  // find() points into it
    const Variable* ps = find(globals, "ps");
    ASSERT_NE(ps, nullptr);
    ASSERT_EQ(ps->children.size(), 1);
    EXPECT_EQ(ps->children[0].value, "{x=1, y=2}");
}

TEST(Debugger, StdStringShowsItsText) {
    cppi_test::RecordingHost recorder;
    Interpreter interpreter(recorder.make_registry());
    auto compiled = interpreter.compile("std::string s = \"hay\";\ns += \"!\";\ntake_int(s.size());\n");
    ASSERT_TRUE(compiled.ok());
    auto execution = interpreter.start(*compiled.program);
    execution.set_breakpoint(3);
    ASSERT_EQ(execution.run().status, RunStatus::Paused);
    const auto globals = execution.globals();  // find() points into it
    const Variable* s = find(globals, "s");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(s->type, "string");
    EXPECT_EQ(s->value, "\"hay!\"");
    EXPECT_TRUE(s->children.empty());
}

TEST(Debugger, StepLineVisitsLoopHeadersAndConditionsInSourceOrder) {
    // Conditions compile to jumps and loops test at the bottom; stepping must
    // still visit the lines in the order the program runs them.
    cppi_test::RecordingHost recorder;
    Interpreter interpreter(recorder.make_registry());
    auto compiled = interpreter.compile(
        "int total = 0;\n"
        "for (int i = 0; i < 3 && total < 100; i++) {\n"
        "    if (i == 1 || total > 50) {\n"
        "        continue;\n"
        "    }\n"
        "    total += i;\n"
        "}\n"
        "while (!(total > 4)) {\n"
        "    total++;\n"
        "}\n"
        "take_int(total);\n");
    ASSERT_TRUE(compiled.ok());
    auto execution = interpreter.start(*compiled.program);
    std::vector<std::uint32_t> lines;
    while (!execution.finished() && lines.size() < 100) {
        lines.push_back(execution.current_location().begin.line);
        execution.step_line();
    }
    const std::vector<std::uint32_t> expected = {1, 2, 3, 6, 2, 3, 4, 2, 3, 6, 2, 8, 9, 8, 9, 8, 9, 8, 11};
    EXPECT_EQ(lines, expected);
    EXPECT_EQ(recorder.calls, std::vector<std::string>{"take_int(5)"});
}
