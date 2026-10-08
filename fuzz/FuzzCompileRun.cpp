/// @file FuzzCompileRun.cpp
/// @brief libFuzzer target: any byte sequence must compile (or be rejected with
/// diagnostics) and, if it compiles, run within a small budget, without
/// crashes, hangs, leaks or undefined behavior.
///
/// Build with sanitizers.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include <cppi/Cppi.hpp>

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace {

const cppi::Interpreter& interpreter() {
    static const cppi::Interpreter instance = [] {
        cppi::HostRegistry host;
        const auto dir = host.add_enum("Direction", {"North", "East", "South", "West"});
        host.function("move").param(dir).bind([](cppi::HostCall&) { return cppi::Value::void_value(); });
        host.function("harvest").returns(cppi::types::Bool).bind([](cppi::HostCall&) {
            return cppi::Value::from_bool(true);
        });
        host.function("take").param(cppi::types::Int).returns(cppi::types::Int).bind([](cppi::HostCall& call) {
            return cppi::Value::from_int(call.arg(0).as_int());
        });
        host.function("fail").bind([](cppi::HostCall& call) {
            call.fail(1, "fuzz");
            return cppi::Value::void_value();
        });
        cppi::Options options;
        options.standard = cppi::Standard::Cpp26;  // exercise every gating path
        return cppi::Interpreter(std::move(host), options);
    }();
    return instance;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const std::string_view source(reinterpret_cast<const char*>(data), size);
    const auto compiled = interpreter().compile(source);
    if (compiled.ok()) {
        cppi::RunOptions options;
        options.budget = 10'000;
        (void)interpreter().run(*compiled.program, options);
        (void)compiled.program->disassemble();
    }
    for (const auto& d : compiled.diagnostics) {
        (void)cppi::to_debug_string(d);
    }
    return 0;
}
