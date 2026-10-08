#include "support/ProgramRunner.hpp"
#include "support/RecordingHost.hpp"

#include "codegen/OpCode.hpp"
#include "codegen/ProgramData.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <optional>
#include <set>
#include <string_view>
#include <thread>
#include <vector>

using namespace cppi;

TEST(Program, ProgramsCanBeDisassembled) {
    cppi_test::RecordingHost recorder;
    Interpreter interpreter(recorder.make_registry());
    auto result = interpreter.compile("move(East);\ntake_int(true);");
    ASSERT_TRUE(result.ok());
    const auto listing = result.program->disassemble();
    EXPECT_NE(listing.find("PUSH_CONST  East : Direction"), std::string::npos);
    EXPECT_NE(listing.find("CALL_HOST   move (1 args)"), std::string::npos);
    EXPECT_NE(listing.find("PUSH_CONST  1 : int"), std::string::npos);  // constant folded bool -> int
    EXPECT_NE(listing.find("HALT"), std::string::npos);
    EXPECT_EQ(result.program->instruction_count(), 5);
    EXPECT_EQ(result.program->max_stack_depth(), 1);
}

TEST(Program, TheListingNamesFunctionsTargetsAndKinds) {
    Interpreter interpreter(HostRegistry{}, cppi_test::standard(Standard::Cpp17));
    auto result = interpreter.compile(R"(
        struct Shape { virtual int area() const { return 0; } };
        struct Square : Shape { int side = 2; int area() const override { return side * side; } };
        long twice(long x) { return x * 2; }
        double half(double x) { return x / 2; }
        int total = 0;
        for (int i = 0; i < 3; i++) { total += i; }
        int values[2] = {1, 2};
        int* p = values;
        p++;
        Square square;
        const Shape& shape = square;
        total += shape.area() + static_cast<int>(twice(3) + half(4.0)) + *p + values[1] - static_cast<int>(p - values);
        if (total > 1 && total < 100) { total--; }
    )");
    ASSERT_TRUE(result.ok());
    const auto listing = result.program->disassemble();
    for (const char* expected :
         {"      ; twice\n", "      ; Square::area\n", "Shape::area (1 args) slot 0", "CALL        twice (1 args)",
          "JUMP_CMP    if LT int -> 0005", "JUMP_CMP    if !GT int -> ", "MUL         long", "DIV         double",
          "INDEX       x1 of 2", "MEMBER      +1", "PTR_ADD     1", "PTR_DIFF    1", "INC         [0] +1 int",
          "INC         global[", "] -1 int", "INIT_HEADERS"}) {
        EXPECT_NE(listing.find(expected), std::string::npos) << expected << "\n" << listing;
    }
}

TEST(Program, EveryOpCodeHasItsOwnName) {
    std::set<std::string_view> names;
    for (unsigned n = 0; n <= static_cast<unsigned>(detail::OpCode::Halt); ++n) {
        const std::string_view name = detail::to_string(static_cast<detail::OpCode>(n));
        EXPECT_NE(name, "???") << n;
        EXPECT_TRUE(names.insert(name).second) << "repeated name " << name;
        EXPECT_EQ(name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"), std::string_view::npos) << name;
    }
}

TEST(Program, ConstantsAreDeduplicated) {
    cppi_test::RecordingHost recorder;
    Interpreter interpreter(recorder.make_registry());
    auto result = interpreter.compile("move(East);\nmove(East);\nmove(East);");
    ASSERT_TRUE(result.ok());
    EXPECT_EQ(result.program->data().constants.size(), 1);
}

TEST(Program, AProgramCanBeRunManyTimesAndOutlivesItsInterpreter) {
    cppi_test::RecordingHost recorder;
    std::optional<Program> program;
    {
        Interpreter interpreter(recorder.make_registry());
        program = interpreter.compile("harvest();").program;
    }
    ASSERT_TRUE(program.has_value());
    Interpreter other(cppi::HostRegistry{});
    for (int i = 0; i < 3; ++i) {
        EXPECT_TRUE(other.run(*program).ok());
    }
    EXPECT_EQ(recorder.calls.size(), 3);
}

TEST(ProgramThreads, OneCompiledProgramRunsSafelyOnManyThreads) {
    // Each thread gets its own counter-backed host function; the Program
    // (bytecode + registry) is shared and immutable.
    std::atomic<int> total{0};
    HostRegistry host;
    host.function("tick").bind([&total](HostCall&) {
        total.fetch_add(1, std::memory_order_relaxed);
        return Value::void_value();
    });
    Interpreter interpreter(std::move(host));
    auto compiled = interpreter.compile("tick();\ntick();\ntick();\ntick();");
    ASSERT_TRUE(compiled.ok());
    const Program program = *compiled.program;

    std::vector<std::thread> threads;
    std::atomic<int> failures{0};
    for (int t = 0; t < 8; ++t) {
        threads.emplace_back([&] {
            for (int i = 0; i < 250; ++i) {
                if (!interpreter.run(program).ok()) {
                    failures.fetch_add(1);
                }
                // Compiling concurrently is also safe (thread-local parsers).
                if (!interpreter.compile("tick();").ok()) {
                    failures.fetch_add(1);
                }
            }
        });
    }
    for (auto& th : threads) {
        th.join();
    }
    EXPECT_EQ(failures, 0);
    EXPECT_EQ(total, 8 * 250 * 4);
}
