# Roadmap

Each phase delivers something that works end to end, with tests, and unlocks new levels in the game.

## ✅ Phase 0: project foundations

Conan 2, CMake with presets, GoogleTest, Google Benchmark, libFuzzer, clang-format, clang-tidy, sanitizers, coverage, and GitHub Actions for Linux, macOS, Windows, Android, and iOS.

## ✅ Phase 1: "hello, automaton"

- Call statements in script mode: `harvest(); move(East);`
- Functions and enums registered by the host (fluent builder).
- Integer literals (decimal, octal, hexadecimal; binary and digit separators with C++14) and booleans.
- Nested calls and C++ implicit conversions (`bool`/enum → `int`, `int`/enum → `bool`).
- Operation budget with a cost model.
- Diagnostics with codes and "did you mean...?" suggestions.
- Recognition of every feature from C++98 to C++20 to explain why something is not accepted.
- Step-by-step execution and observers.
- `cppi-run` with a simulated farm and messages in Spanish and English.

## ✅ Phase 2: basic logic

- Variables of every integer type (`bool`, `char`, `signed`/`unsigned char`, `short`, `int`, `long`, `long long` and their `unsigned` forms), `float` and `double`, `const`, assignment, and block scope. Literals get their standard types (`1u`, `0xFFFFFFFF`, `5000000000`), signed and unsigned values mix with the usual C++ conversions, and unsigned arithmetic wraps around.
- Arithmetic, bitwise, comparison, logical (short-circuiting), compound assignment, increment/decrement, comma and conditional operators.
- `if`/`else`, `while`, `do`/`while`, `for`, `break`, `continue`.
- Uninitialized reads and integer overflow are detected at run time (E0502, E0501).

## ✅ Phase 3: structure

- Player-defined functions, overloading, default arguments, recursion with a depth limit (E0402).
- Fixed-size and multidimensional arrays with bounds checking (E0503).
- Pointers (with pointer arithmetic checked against the object they point into), references, `new`/`delete` with leak detection (E0510).
- `struct`, `switch`, `enum` and `enum class`, `typedef`/`using`, namespaces, `static_assert`, `constexpr` variables and `constexpr` functions evaluated at compile time (loops, locals and recursion included).
- Script mode coexists with `int main()`: top-level declarations are globals and `main` runs after the loose statements.

## ✅ Phase 4: teaching tools

- Debugger API: breakpoints by line, `step_line()`, call stack with the variables of every frame, globals (`Execution`).
- Undefined behavior as a mechanic: E05xx diagnostics with an explanation and the name of the variable involved.
- `ProgramCache`: compiled programs keyed by source, rules and host registry (LRU, thread-safe).
- `std::cout` output, captured in `RunResult::output`.

## ✅ Phase 5: object-oriented C++98

- Classes with access control, constructors (member initializer lists, delegating constructors), static data members and static member functions, destructors (run at end of scope, on `delete`, and for temporaries), implicit and user-defined copy constructor and `operator=`.
- Operator overloading (member and free functions, found by argument-dependent lookup), conversion operators, `operator->`, `operator[]`, `operator()`.
- Single, multiple and virtual inheritance (including the diamond), virtual functions, pure virtual functions and abstract classes, `override`/`final`.
- Class and function templates with deduction, default template arguments, and member function templates.
- Exceptions: `throw`, `try`/`catch` by type, by base class and `catch (...)`, `throw;`, destructors run while unwinding, uncaught exceptions explained (E0404).

## ✅ Standard library (minimal)

Written in C++ and compiled together with the player's code (see [ADR 0008](adr/0008-standard-library-prelude.md)): `std::cout`/`std::endl`, `std::string`, `std::to_string`, `std::vector`, `std::map`, `std::pair`/`std::make_pair`, `std::min`/`std::max`/`std::abs`/`std::swap`, `std::sort`, `std::reverse`, `std::find`, `std::count`, `std::fill`, `std::accumulate`, `std::unique_ptr`/`std::make_unique`, `std::shared_ptr`/`std::make_shared`, `std::optional`/`std::nullopt`, `std::variant` (up to four alternatives, with `std::get`, `std::get_if`, `std::holds_alternative`, `std::visit`), the exception hierarchy (`std::exception`, `logic_error`, `runtime_error`, `out_of_range`, `invalid_argument`, `length_error`, `overflow_error`, `bad_optional_access`, `bad_variant_access`), and the concepts `same_as`, `integral`, `signed_integral`, `unsigned_integral`, `floating_point`, `convertible_to`, `derived_from`.

## ✅ Modern C++

- C++11: `auto`, lambdas (captures by value and by reference), range-for (arrays and classes with `begin`/`end`), `nullptr`, `enum class`, rvalue references and `std::move`, `= delete`/`= default`, `explicit`, delegating constructors, type aliases.
- C++14: binary literals, digit separators, generic lambdas (`[](auto x)`), deduced return types (`auto f()`).
- C++17: structured bindings, `if constexpr` (the discarded branch is not compiled), `inline` static members, `std::optional`, `std::variant`.
- C++20: concepts (`concept`, `requires` clauses and expressions, constrained template parameters, `Concept auto` parameters and abbreviated function templates, overloads ordered by constraints), with E0238 explaining which constraint failed.

## Next

- Preprocessor macros (`#define`): recognized, reported as not implemented (E0302).
- `std::initializer_list`, capturing `this` in lambdas, `std::variant` with more than four alternatives.
- More containers (`set`, `deque`) and algorithms; ranges.
- C++20 coroutines and modules; threads as several automatons.

## Known technical debt

- Host function overloading.
- Columns in characters as well as bytes (for editors that count characters).
- Thread tests on WebAssembly (requires building with pthreads).
- Doxygen API documentation published on GitHub Pages.
