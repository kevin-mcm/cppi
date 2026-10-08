# cppi

Embeddable interpreter for a **subset of C++ that unlocks in stages**, designed for educational programming games: the player writes real C++, and the game decides which standard (C++98 → C++26) and which features (loops, variables, classes...) are available at each level.

`cppi` knows nothing about the game. The game (the *host*) registers its functions (`move`, `harvest`...) and the interpreter only invokes them, counts operations, and returns diagnostics as **codes with data**, so the game can show them in the player's language.

> **Status:** phases 1–5 done, plus C++11/14/17 and C++20 concepts. The player can write procedural C++ (variables, operators, control flow, functions, recursion, arrays, pointers, references, `struct`, `enum`, `switch`), object-oriented C++ (classes, constructors and destructors, static members, operator overloading, single, multiple and virtual inheritance, virtual functions), exceptions, templates (with member templates and defaults), and modern C++ (`auto`, lambdas including generic ones, range-for, rvalue references and `std::move`, `constexpr` functions, structured bindings, `if constexpr`, concepts). Every integer type is supported, `unsigned` included. A small standard library is included: `std::cout`, `std::string`, `std::vector`, `std::map`, `std::pair`, `std::sort` and other algorithms, `std::unique_ptr`, `std::shared_ptr`, `std::optional`, `std::variant`, the standard exceptions and concepts. Undefined behavior (overflow, out-of-bounds access, uninitialized reads, dangling pointers...) stops the program with an explanation instead of crashing. There is a debugger API (breakpoints, stepping by line, variables per frame). Anything not supported yet is recognized and explained ("loops are not unlocked yet", "lambdas require C++11", "macros are not implemented yet"). See [docs/roadmap.md](docs/roadmap.md).

## Example

```cpp
// Player code (level 1, C++98)
harvest();
move(East);
harvest();
```

```cpp
// Game code
#include <cppi/Cppi.hpp>

cppi::HostRegistry host;
const auto direction = host.add_enum("Direction", {"North", "East", "South", "West"});

host.function("move").param(direction).cost(5).bind([&](cppi::HostCall& call) {
    automaton.move(call.arg(0).as_enum());
    return cppi::Value::void_value();
});
host.function("harvest").returns(cppi::types::Bool).cost(5).bind([&](cppi::HostCall&) {
    return cppi::Value::from_bool(automaton.harvest());
});

cppi::Options level;
level.standard = cppi::Standard::Cpp98;
level.locked.add(cppi::Feature::Loops);  // not unlocked yet

const cppi::Interpreter interpreter(std::move(host), level);
const auto compiled = interpreter.compile(player_code);
if (!compiled.program) {
    for (const auto& d : compiled.diagnostics) show_translated(d);  // codes, not text
    return;
}

cppi::RunOptions run;
run.budget = 10'000;  // operations available at this level
const auto result = interpreter.run(*compiled.program, run);
```

To animate the automaton tick by tick: `interpreter.start(program)` returns an `Execution` with `step()`.

## Building

Requirements: CMake ≥ 3.23, a C++20 compiler (GCC 11+, Clang 14+, AppleClang 15+, MSVC 19.30+), and [Conan 2](https://conan.io).

```bash
conan profile detect                      # once
conan install . -s build_type=Debug -s compiler.cppstd=20 --build=missing
cmake --preset conan-debug                # on Windows: cmake --preset conan-default
cmake --build --preset conan-debug
ctest --preset conan-debug --output-on-failure
```

Try a program with the console tool:

```bash
./build/Debug/tools/cppi-run/cppi-run --lang es examples/levels/01-first-steps.cpp
./build/Debug/tools/cppi-run/cppi-run --lang es --lock loops examples/levels/03-locked-loop.cpp
./build/Debug/tools/cppi-run/cppi-run --trace --budget 30 examples/levels/06-budget.cpp
./build/Debug/tools/cppi-run/cppi-run --std c++11 examples/levels/10-classes.cpp
./build/Debug/tools/cppi-run/cppi-run --lang es examples/levels/09-undefined-behavior.cpp
```

Useful build options:

| CMake option | Purpose |
|---|---|
| `-DCPPI_WARNINGS_AS_ERRORS=ON` | Warnings as errors (CI) |
| `-DCPPI_SANITIZERS="address;undefined"` | ASan + UBSan |
| `-DCPPI_ENABLE_COVERAGE=ON` | Coverage with gcov/gcovr |
| `-DCPPI_BUILD_FUZZERS=ON` | libFuzzer target (Clang) |
| `-DCPPI_ENABLE_CLANG_TIDY=ON` | clang-tidy during the build |

Benchmarks: `conan install . -s build_type=Release -o "&:with_benchmarks=True" ...` and then `build/Release/bench/cppi_bench`.

## Using it from another project

```python
# the game's conanfile.py
def requirements(self):
    self.requires("cppi/0.1.0")
```

```cmake
find_package(cppi REQUIRED CONFIG)
target_link_libraries(game_core PRIVATE cppi::cppi)
```

There are Android and iOS profiles in [`conan/profiles`](conan/profiles). See [docs/integrating.md](docs/integrating.md).

## Layout

```
include/cppi/      Public API (the only thing the game sees), one header per class
src/api/           Implementation of the public classes
src/pipeline/      CompilerPipeline: parse → sema → codegen
src/support/       HostRegistry, features, DiagnosticFactory
src/ast/           AST node types
src/parse/         tree-sitter adapter → own AST
src/sema/          Semantic analysis: features, names, types, overloads, templates
src/library/       The standard library prelude (C++ source compiled with the player's code)
src/codegen/       Bytecode generation
src/vm/            Stack virtual machine: budget, checked memory, debugger support
tools/cppi-run/    Test console with a simulated farm and es/en messages
tests/             Unit tests (GoogleTest) and golden tests (full cppi-run output)
bench/ fuzz/       Benchmarks (Google Benchmark) and fuzzing (libFuzzer)
docs/              Architecture, decisions (ADR), CI/CD, diagnostics, roadmap
```

## Documentation

- [Architecture and patterns](docs/architecture.md)
- API reference: run `doxygen` at the repository root (HTML in `build/doxygen/html`)
- [Design decisions (ADR)](docs/adr/)
- [Diagnostics catalog](docs/diagnostics.md) (for translators)
- [Integrating with the game](docs/integrating.md)
- [CI/CD, releases, and deployment](docs/ci-cd.md)
- [Roadmap](docs/roadmap.md)
- [How to contribute](CONTRIBUTING.md)

## License

MIT. See [LICENSE](LICENSE).
