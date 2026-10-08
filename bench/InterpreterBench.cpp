// Performance baselines for the interpreter. CI runs these on every push to
// main and fails a pull request when a benchmark gets markedly slower
// (see .github/workflows/benchmarks.yml).

#include <cppi/Cppi.hpp>

#include <benchmark/benchmark.h>

#include <cstdint>
#include <string>

namespace {

cppi::HostRegistry make_host(std::int64_t& sink) {
    cppi::HostRegistry host;
    const auto dir = host.add_enum("Direction", {"North", "East", "South", "West"});
    host.function("move").param(dir).cost(5).bind([&sink](cppi::HostCall& call) {
        sink += call.arg(0).as_enum();
        return cppi::Value::void_value();
    });
    host.function("harvest").returns(cppi::types::Bool).cost(5).bind([&sink](cppi::HostCall&) {
        ++sink;
        return cppi::Value::from_bool(true);
    });
    return host;
}

/// N statements alternating harvest() and move(<direction>).
std::string make_source(std::int64_t statements) {
    static const char* const kDirections[] = {"North", "East", "South", "West"};
    std::string source;
    source.reserve(static_cast<std::size_t>(statements) * 14);
    for (std::int64_t i = 0; i < statements; ++i) {
        if (i % 2 == 0) {
            source += "harvest();\n";
        } else {
            source += "move(";
            source += kDirections[(i / 2) % 4];
            source += ");\n";
        }
    }
    return source;
}

void BM_Compile(benchmark::State& state) {
    std::int64_t sink = 0;
    const cppi::Interpreter interpreter(make_host(sink));
    const std::string source = make_source(state.range(0));
    for (auto _ : state) {
        auto result = interpreter.compile(source);
        benchmark::DoNotOptimize(result);
    }
    state.SetBytesProcessed(static_cast<std::int64_t>(state.iterations()) * static_cast<std::int64_t>(source.size()));
    state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_Compile)->RangeMultiplier(10)->Range(10, 10'000)->Unit(benchmark::kMicrosecond);

void BM_Run(benchmark::State& state) {
    std::int64_t sink = 0;
    const cppi::Interpreter interpreter(make_host(sink));
    const auto program = *interpreter.compile(make_source(state.range(0))).program;
    cppi::RunOptions options;
    options.budget = UINT64_MAX;
    for (auto _ : state) {
        auto result = interpreter.run(program, options);
        benchmark::DoNotOptimize(result);
    }
    benchmark::DoNotOptimize(sink);
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(program.instruction_count()));
    state.counters["instructions"] = static_cast<double>(program.instruction_count());
}
BENCHMARK(BM_Run)->RangeMultiplier(10)->Range(10, 100'000)->Unit(benchmark::kMicrosecond);

void BM_RunWithObserver(benchmark::State& state) {
    struct Counter final : cppi::ExecutionObserver {
        std::uint64_t steps = 0;
        void on_step(cppi::SourceRange, std::uint64_t) override { ++steps; }
    };
    std::int64_t sink = 0;
    const cppi::Interpreter interpreter(make_host(sink));
    const auto program = *interpreter.compile(make_source(state.range(0))).program;
    Counter counter;
    cppi::RunOptions options;
    options.budget = UINT64_MAX;
    options.observer = &counter;
    for (auto _ : state) {
        auto result = interpreter.run(program, options);
        benchmark::DoNotOptimize(result);
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(program.instruction_count()));
}
BENCHMARK(BM_RunWithObserver)->Arg(10'000)->Unit(benchmark::kMicrosecond);

/// Cost of driving execution one instruction at a time (game loop / debugger).
void BM_Step(benchmark::State& state) {
    std::int64_t sink = 0;
    const cppi::Interpreter interpreter(make_host(sink));
    const auto program = *interpreter.compile(make_source(1'000)).program;
    cppi::RunOptions options;
    options.budget = UINT64_MAX;
    for (auto _ : state) {
        auto execution = interpreter.start(program, options);
        while (execution.step() == cppi::RunStatus::Running) {
        }
        benchmark::DoNotOptimize(execution.operations());
    }
    state.SetItemsProcessed(state.iterations() * static_cast<std::int64_t>(program.instruction_count()));
}
BENCHMARK(BM_Step)->Unit(benchmark::kMicrosecond);

}  // namespace
