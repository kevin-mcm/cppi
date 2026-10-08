// Minimal host: what a game's integration layer looks like.
#include <cppi/Cppi.hpp>

#include <cstdlib>
#include <iostream>

int main() {
    int harvested = 0;

    cppi::HostRegistry host;
    host.function("harvest").cost(5).bind([&harvested](cppi::HostCall&) {
        ++harvested;
        return cppi::Value::void_value();
    });

    const cppi::Interpreter interpreter(std::move(host));
    const auto compiled = interpreter.compile("harvest();\nharvest();\n");
    if (!compiled.program) {
        std::cerr << "compile failed\n";
        return EXIT_FAILURE;
    }
    const auto result = interpreter.run(*compiled.program);
    std::cout << "cppi " << cppi::kVersionString << ": harvested " << harvested << " in " << result.operations
              << " operations\n";
    return result.ok() && harvested == 2 ? EXIT_SUCCESS : EXIT_FAILURE;
}
