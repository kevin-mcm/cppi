# Changelog

Format based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/). Versioning: [SemVer](https://semver.org/).

## [Unreleased]

## [0.3.0] - 2026-10-09

### Fixed
- A class that implements every pure virtual function of its base can be created, including with its own constructor, a default member initializer, or when passed by value (`struct D : B { int f() override { return 1; } }; D d;`), instead of failing with E0225 `abstract-class` for the base. Bases with constructors were never affected.

## [0.2.0] - 2026-10-09

### Added
- Language phases 2–5: variables, operators and control flow; functions, overloading, recursion, arrays, pointers, references, `new`/`delete`, `struct`, `enum`, `switch`, namespaces; classes with constructors, destructors, copies, operator overloading, conversion operators, single/multiple/virtual inheritance and virtual functions; class and function templates.
- Modern C++: `auto`, lambdas, range-for, `nullptr`, `enum class`, rvalue references and `std::move`, `= delete`/`= default`, `explicit`, structured bindings, `if constexpr`, binary literals and digit separators.
- Standard library prelude written in C++: `std::cout`, `std::string`, `std::vector`, `std::map`, `std::pair`, algorithms, `std::unique_ptr`, `std::shared_ptr`, `std::optional` (ADR 0008).
- Undefined behavior detection as 5xx diagnostics with status `RuntimeError` (ADR 0009); `cppi::is_undefined_behavior()`.
- E0239 `no-operator` (`op`, `type`): a class used with an operator it does not have, such as `std::sort` of `std::pair`s or of a struct without a comparator (it was E0203 `argument-type-mismatch` with `expected=string`, from the only `operator<` in sight, std::string's) or `a == b` on a struct (E0210). Inside the standard library it is reported at the player's call and followed by the note E0240 `instantiated-from` (`function`, e.g. `std::sort<pair<int, int>>`). Messages in cppi-run's English and Spanish catalogs.
- Diagnostics E0208–E0237, E0402 `stack-overflow`, E0403 `out-of-memory`, E0500–E0513.
- Debugger API in `Execution`: breakpoints, `step_line()`, `call_stack()`, `globals()`, `StackFrame`, `Variable`, `RunStatus::Paused`.
- The debugger shows `std::vector` by its elements (`{4, 7}`, with one child per element) and `std::string` by its text (`"hay"`) instead of their internal fields.
- A name used without its namespace suggests the qualified one: `greet()` → "did you mean 'english::greet'?", `cout` → `std::cout`.
- `CostModel::Unit::Statement` (opt-in; `Instruction` stays the default): each statement the player wrote costs `CostModel::statement` every time it runs, and so does each evaluation of the condition of an `if`, a loop or a `switch`, so "one line, one operation" and the budget does not depend on code generation (ADR 0011).
- Per-line operation profile: `RunResult::line_operations` and `Execution::line_operations()` (`LineOperations{line, operations}`), with library, implicit and host costs charged to the player's calling line.
- `ProgramCache`, `RunResult::output`/`Execution::output()`/`ExecutionObserver::on_output()`, `CostModel::library_instruction`, `RunOptions::max_call_depth`.
- Script mode accepts `int main()` and `#include`, and statements that tree-sitter rejects at the top level (ADR 0010).
- Examples and golden cases for exceptions, `std::cout`, undefined behavior and classes.
- Every integer type: `short`, `unsigned` and `signed` forms, with standard literal types and wrapping unsigned arithmetic.
- Delegating constructors (feature key `delegating-constructors`), static data members and static member functions, `inline` static members.
- `constexpr` functions evaluated at compile time (`ConstexprInterpreter`).
- Member function templates, default template arguments, generic lambdas, deduced `auto` return types; `if constexpr` discards the other branch.
- Exceptions: `throw`, `try`/`catch`, rethrow, unwinding with destructors, the standard exception classes; E0404–E0406. `vector::at`, `string::at` and `optional::value` throw.
- `std::variant` with `get`, `get_if`, `holds_alternative` and `visit`.
- C++20 concepts: `concept`, `requires` clauses and expressions, constrained parameters, abbreviated function templates, ordering by constraints, standard concepts; E0238 `constraints-not-satisfied`.
- Base classes named with a scope or as template instances (`struct E : std::exception`, `struct B : Box<int>`).

### Changed
- Code generation emits fewer instructions for the same program, so the same solution costs fewer operations with the default `CostModel` (and runs faster): conditions become jumps (`&&`, `||`, `!`, and a fused compare-and-jump `JUMP_CMP`), loops test at the bottom, `i++` on an `int`/`long` variable is one `INC`, `x += e` skips the address, constant expressions and `const` variables are folded, no-op conversions (`RETYPE`) are gone, and jumps are threaded. cppi-farm's shortest-path solution goes from 12,973 to 9,222 operations, the travelling salesman from 208,122 to 150,188, memoization from 6,911 to 6,012 (`bench/LevelBench.cpp`). Diagnostics and the lines the debugger steps through do not change. Levels calibrated with exact operation counts need new numbers.
- Unit tests use GoogleTest (`gtest/1.17.0`) instead of Catch2. Suites are named after their file (`Parser`, `Compile`...); threaded tests are in `*Threads` suites, labelled `threads` in ctest.
- **Breaking:** public headers renamed to PascalCase, one per class (`<cppi/cppi.hpp>` → `<cppi/Cppi.hpp>`, `<cppi/host.hpp>` → `<cppi/HostRegistry.hpp>`, ...). `HostRegistry::FunctionBuilder` is now `cppi::FunctionBuilder`.
- Internals split into one class per file: `CompilerPipeline`, `DiagnosticFactory`, `FeatureGate`, `ExpressionBinder`, `ImplicitConversions`, `NameSuggester`, `TreeConverter`, `SyntaxChecker`, `CodeGenerator`, `ConstantPool`, `Disassembler`, `OperationBudget`, `HostCallInvoker`.
- `cppi-run`: one `MessageCatalog` per language (`EnglishCatalog`, `SpanishCatalog`), `FarmHostBindings` separates the farm from the interpreter, `Application` and `CommandLineParser` replace the logic in `main`.

### Fixed
- A compound assignment to a `double` (`d /= 2`, `d += x`) converted the variable's value as if it were an integer before operating, so `double d = 5; d /= 2;` gave `4.88191e+18`; inside a `constexpr` function it made the call not a constant.
- CI: every job past Format was skipped while Format failed; once it ran, these surfaced and are fixed: GCC 13 Release false positives (`-Wstringop-overflow` in `DeclarationBinder`, `-Wnull-dereference` in a test), a 32-bit sign conversion in `Memory`, MSVC's C4996 on `getenv` in cppi-run, clang-tidy `readability-qualified-auto` in `Feature.cpp`, and the coverage report's missing output folder.
- WebAssembly: test executables get a 5 MB stack, as in Godot's web export (Emscripten's default is 64 KiB), and constexpr calls nest at most 256 deep instead of 512, which overflowed the ~1 MB machine stack of Node.js and browsers.
- Builds with Clang 14 (explicit deduction guide for `Overloaded`; no `std::ranges::reverse_view`, which Clang 14 cannot use with libstdc++ 11).
- Number formatting no longer depends on the C locale (`NumberFormat::shortest`).
- Floating-point literals are read with `std::from_chars` and doubles are printed with `std::to_chars` (`%g`, 6 significant digits, exactly as `std::cout`), without iostreams or the C++ locale. In cppi-farm, a Godot extension with a static libstdc++ next to the system's (loaded by the Vulkan driver) crashed with SIGSEGV in `basic_istream::_M_extract` on any literal such as `5.0`, because the two libraries' locale facets got mixed. Standard libraries without floating-point `<charconv>` (libc++ before 20) use an exact parser for common literals, `strtod` and `snprintf`, adjusted for the C locale's decimal point. `src/` no longer uses `<sstream>` or `<locale>`.
- `Guard g(n);` inside a function is an object, not a function declaration (the most vexing parse).
- `nullptr` and `NULL` with newer tree-sitter-cpp grammars (node kind `null`).
- `std::vector<bool> seen(kWidth * kHeight, false);` and similar direct initializations whose arguments all start with a name (a variable, an enumerator, `true`/`false`/`nullptr`, or expressions such as `a * b`, `a & b`, `a[i]`) are objects again instead of function declarations that failed with E0214 `unknown-type`. A prototype whose parameters can all be types (`int f(int, Direction);`, `T x(U);`) is still a function.
- `new T` and `new T[n]` for a struct without constructors now construct its members (a `std::string`, default member initializers): `std::vector` of an aggregate such as `struct R { std::string n; int s; };` failed with E0502 `uninitialized-read` on the first `push_back`, because `reserve` assigned to strings that were never constructed.
- A forgotten `;`, `)` or `}` is reported as E0101 `missing-token` right after the last token before it, also where tree-sitter only produced an ERROR (`int a =-5.0` ⏎ `std::cout << a;` was E0100 at 1:8) or put its MISSING node after the next token (`move(East)` ⏎ `}` was reported after the `}`). The parser tries inserting each token at the boundaries before the error and keeps the one that lets it parse further; up to three forgotten tokens are reported. E0101 gets an optional `line` argument when the token belongs at the end of that line, and cppi-run says "missing ';' at the end of line 1" (`missing-token-at-line-end` in its catalogs).
- No false E0222 `missing-return` warning for a function that ends in a `switch` with a `default` whose cases all return (with or without falling through), such as `Direction opposite(Direction d) { switch (d) { case North: return South; ... default: return East; } }`. A `break` after a `return` no longer counts as leaving a `switch` or an endless loop, and a `break` inside a `try` does.
- A function left without its closing `}` at the end of the program is a syntax error again: the script-mode reading closed it with the `}` it adds around the program.
- Runtime errors inside implicit copies, assignments and default constructors (and calls made from them, such as a stack overflow) are reported at the player's line that caused them, not at 0:0.
- `new T(other)` for classes without constructors.
- String literals are written into static storage before the program starts, with no instructions (stepping starts at the player's first line).
- CI: `clang-format` 18 is the reference version; the benchmark workflow creates the `gh-pages` history branch on the first run on `master`.

## [0.1.0] - 2026-10-01

### Added
- Full pipeline: parser (tree-sitter-cpp) → semantic analysis → bytecode → virtual machine.
- Language phase 1: host function calls with literals, enums, and nested calls; C++ implicit conversions.
- `HostRegistry` with a fluent builder for functions and enums.
- Feature locking by standard (C++98–C++26) and by level.
- Diagnostics with stable codes and suggestions.
- Operation budget, step-by-step execution, and observers.
- `cppi-run`: console with a simulated farm and messages in Spanish and English.
- Unit and golden tests, benchmarks, fuzzing, CI/CD for five platforms, and a Conan package.
