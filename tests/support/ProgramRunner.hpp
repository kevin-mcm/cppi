#pragma once

/// @file ProgramRunner.hpp
/// @brief Helpers for end-to-end tests: compile a program against the recording
/// host, run it, and look at what it reported (host calls and std::cout).
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "support/RecordingHost.hpp"
#include "support/Require.hpp"

#include <gtest/gtest.h>

#include <cppi/Cppi.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace cppi_test {

/// Host calls in order, rendered as "move(East)".
using Calls = std::vector<std::string>;

/// Everything one test run produced.
struct Run {
    /// The compile result.
    cppi::CompileResult compiled;
    /// The run result (empty if compilation failed).
    cppi::RunResult result;
    /// Host calls made.
    Calls calls;
};

/// Compiles `source` against a RecordingHost and runs it if it compiles.
inline Run run(std::string_view source, cppi::Options options = {}, cppi::RunOptions run_options = {}) {
    RecordingHost recorder;
    cppi::Interpreter interpreter(recorder.make_registry(), options);
    Run out;
    out.compiled = interpreter.compile(source);
    if (out.compiled.program) {
        out.result = interpreter.run(*out.compiled.program, run_options);
    }
    out.calls = recorder.calls;
    return out;
}

/// Runs a program that must compile and complete without warnings at runtime.
inline Run completes(std::string_view source, cppi::Options options = {}) {
    auto r = run(source, options);
    SCOPED_TRACE(::testing::Message() << "program: " << source);
    for (const auto& d : r.compiled.diagnostics) {
        SCOPED_TRACE(::testing::Message() << cppi::to_debug_string(d));
        EXPECT_NE(d.severity, cppi::Severity::Error);
    }
    CPPI_REQUIRE(r.compiled.ok());
    for (const auto& d : r.result.diagnostics) {
        SCOPED_TRACE(::testing::Message() << cppi::to_debug_string(d));
        EXPECT_NE(d.severity, cppi::Severity::Error);
    }
    EXPECT_EQ(r.result.status, cppi::RunStatus::Completed);
    return r;
}

/// What a program reported through the host functions.
inline Calls output(std::string_view source, cppi::Options options = {}) {
    return completes(source, options).calls;
}

/// What a program printed with std::cout.
inline std::string printed(std::string_view source, cppi::Options options = {}) {
    auto r = completes(source, options);
    EXPECT_TRUE(r.result.diagnostics.empty());  // no leaks either
    return r.result.output;
}

/// The first compile error of a program.
inline cppi::DiagCode compile_error(std::string_view source, cppi::Options options = {}) {
    auto r = run(source, options);
    SCOPED_TRACE(::testing::Message() << "program: " << source);
    CPPI_REQUIRE(!r.compiled.ok());
    for (const auto& d : r.compiled.diagnostics) {
        if (d.severity == cppi::Severity::Error) {
            return d.code;
        }
    }
    ADD_FAILURE() << "no error reported";
    throw RequirementFailed();
}

/// The diagnostic a program stops with at runtime.
inline cppi::Diagnostic runtime_error(std::string_view source, cppi::Options options = {}) {
    auto r = run(source, options);
    SCOPED_TRACE(::testing::Message() << "program: " << source);
    for (const auto& d : r.compiled.diagnostics) {
        SCOPED_TRACE(::testing::Message() << cppi::to_debug_string(d));
    }
    CPPI_REQUIRE(r.compiled.ok());
    CPPI_REQUIRE(r.result.status == cppi::RunStatus::RuntimeError);
    CPPI_REQUIRE(!r.result.diagnostics.empty());
    return r.result.diagnostics.back();
}

/// Options selecting standard `s`, nothing locked.
inline cppi::Options standard(cppi::Standard s) {
    cppi::Options o;
    o.standard = s;
    return o;
}

}  // namespace cppi_test
