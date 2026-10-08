#include "Application.hpp"

#include "CommandLineParser.hpp"
#include "DiagnosticPrinter.hpp"
#include "ExitCode.hpp"
#include "FarmHostBindings.hpp"
#include "FarmWorld.hpp"
#include "SourceReader.hpp"
#include "TraceObserver.hpp"

#include <cppi/Interpreter.hpp>

#include <iostream>
#include <utility>

namespace cppi_run {

int Application::run(std::span<const std::string_view> args) const {
    const auto outcome = CommandLineParser(std::cout, std::cerr).parse(args);
    if (!outcome.options) {
        return outcome.exit_code;
    }
    const Messages messages(outcome.options->language);
    return execute(*outcome.options, messages);
}

int Application::execute(const CommandLineOptions& options, const Messages& messages) {
    const auto source = SourceReader::read(options.file);
    if (!source) {
        std::cerr << "cppi-run: " << messages.ui("cannot-read") << " '" << options.file << "'\n";
        return exit_code::kNoInput;
    }
    const DiagnosticPrinter printer(std::cerr, messages, options.file, *source);

    FarmWorld world(options.size);
    cppi::HostRegistry host;
    FarmHostBindings::register_api(world, host);

    cppi::Options rules;
    rules.standard = options.standard;
    rules.locked = options.locked;
    const cppi::Interpreter interpreter(std::move(host), rules);

    const auto compiled = interpreter.compile(*source);
    for (const auto& d : compiled.diagnostics) {
        printer.print(d);
    }
    if (!compiled.program) {
        return exit_code::kCompileError;
    }
    const cppi::Program& program = *compiled.program;

    if (options.dump_bytecode) {
        std::cout << messages.ui("bytecode") << ":\n" << program.disassemble() << '\n';
    }
    if (!options.quiet) {
        std::cout << messages.ui("initial-world") << ":\n";
        world.render(std::cout);
        std::cout << '\n';
    }

    TraceObserver tracer(std::cout, interpreter.host());
    cppi::RunOptions run_options;
    run_options.budget = options.budget;
    run_options.observer = options.trace ? &tracer : nullptr;

    const auto result = interpreter.run(program, run_options);
    if (options.trace) {
        std::cout << '\n';
    }
    for (const auto& d : result.diagnostics) {
        printer.print(d);
    }

    if (!result.output.empty()) {
        std::cout << messages.ui("output") << ":\n" << result.output;
        if (result.output.back() != '\n') {
            std::cout << '\n';
        }
        std::cout << '\n';
    }
    if (!options.quiet) {
        std::cout << messages.ui("final-world") << ":\n";
        world.render(std::cout);
        std::cout << '\n';
    }
    std::cout << messages.ui("result") << ": " << messages.status(result.status) << '\n'
              << messages.ui("operations") << ": " << result.operations << " / " << options.budget << '\n'
              << messages.ui("hay") << ": " << world.hay() << '\n'
              << messages.ui("position") << ": (" << world.x() << ", " << world.y() << ")\n";

    return result.ok() ? exit_code::kOk : exit_code::kRuntimeStop;
}

}  // namespace cppi_run
