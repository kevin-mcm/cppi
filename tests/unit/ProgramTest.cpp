/// @file ProgramTest.cpp
/// @brief Unit tests of compiled programs: disassembly, constant deduplication,
/// lifetime and running one Program on many threads.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "support/RecordingHost.hpp"

#include "codegen/ProgramData.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <optional>
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
