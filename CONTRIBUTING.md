# How to contribute

## Setting up the environment

```bash
pip install "conan>=2.4,<3"
conan profile detect
conan install . -s build_type=Debug -s compiler.cppstd=20 --build=missing
cmake --preset conan-debug -DCPPI_WARNINGS_AS_ERRORS=ON
cmake --build --preset conan-debug
ctest --preset conan-debug --output-on-failure
```

## Code rules

- **Identifiers, comments, commit messages, and project documentation are in English.** The only Spanish text allowed is the Spanish message catalog in `tools/cppi-run/SpanishCatalog.cpp` and the `-es` golden cases.
- **One class per file**, named after the class, and every source file name starts with an uppercase letter (`HostRegistry.hpp`, `HostRegistry.cpp`, `ParserTest.cpp`). Small plain-data structs that only make sense together may share a file (e.g. `src/ast/Expr.hpp`).
- Format with `clang-format` 18, the version CI uses (`pip install clang-format==18.1.8`); configuration in `.clang-format`. Other versions format some lines differently, and CI rejects unformatted code.
- `clang-tidy` without warnings in `src/` and `tools/`.
- Nothing in `include/cppi/` may include tree-sitter headers or headers from `src/`.
- Compiling and running player code **never throws exceptions**: problems are `Diagnostic`s.
- A new diagnostic code is added at the end of its group, in `diag_key()` (`src/support/DiagCode.cpp`), in a `DiagnosticFactory` method, in `docs/diagnostics.md`, and in every catalog in `tools/cppi-run/` (`EnglishCatalog.cpp`, `SpanishCatalog.cpp`).
- A new `Feature` is added to its standard group and to the table in `src/support/FeatureTable.hpp` (a `static_assert` checks the order).
- Standard library additions go in `src/library/Prelude.cpp`, written in the C++ subset cppi supports. Only add a VM intrinsic (`__cppi_*`) for what cannot be expressed in C++. See [ADR 0008](docs/adr/0008-standard-library-prelude.md).
- Undefined behavior is never executed: if a new operation can misbehave, the VM checks it and reports a 5xx diagnostic.

## Tests

- **Unit** (`tests/unit/`): each stage separately. They check diagnostic codes and arguments, never text. Language tests (`LanguageTest.cpp`, `ModernTest.cpp`, `DebuggerTest.cpp`) use the helpers in `tests/support/ProgramRunner.hpp` (`output`, `compile_error`, `runtime_error`...). Tests use GoogleTest; helpers that return a value use `CPPI_REQUIRE` (`tests/support/Require.hpp`) instead of `ASSERT_*`, which only works in functions that return void. Suites named `*Threads` start threads and get the ctest label `threads`. Run a subset with `ctest --preset conan-debug -R Parser` or `build/Debug/tests/cppi_tests --gtest_filter='Parser.*'`.
- **Golden** (`tests/golden/`): the full output of `cppi-run` for an example program. To create a case, add `NAME.args` and generate the expectations with `CPPI_UPDATE_GOLDEN=1` (see `docs/ci-cd.md`). Review the diff before committing.
- **Benchmarks** (`bench/`): if you touch the VM or the parser, compare before and after.
- **Fuzzing** (`fuzz/`): if you find a crash, add the minimal input to `fuzz/corpus/` and a unit test.

## Commits and PRs

- [Conventional Commits](https://www.conventionalcommits.org/): `feat(vm): ...`, `fix(parser): ...`, `perf: ...`, `docs: ...`, `test: ...`, `ci: ...`.
- One PR, one purpose. CI must be green.
- Relevant design decisions go in a new ADR in `docs/adr/`.
