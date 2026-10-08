# Architecture decision records (ADR)

Every important decision is written down with its context and consequences, so that a year from now it is clear *why* the code is the way it is. Format: Context → Decision → Consequences. A new decision that replaces another does not delete the old one: it marks it as *Superseded*.

| # | Decision | Status |
|---|---|---|
| [0001](0001-own-interpreter.md) | Own interpreter instead of a compiler | Accepted |
| [0002](0002-tree-sitter.md) | tree-sitter-cpp as the parser | Accepted |
| [0003](0003-bytecode-vm.md) | Bytecode virtual machine | Accepted |
| [0004](0004-diagnostics-as-data.md) | Diagnostics as codes with arguments | Accepted |
| [0005](0005-conan.md) | Conan 2 for dependencies and distribution | Accepted |
| [0006](0006-script-mode.md) | Script mode: top-level statements | Accepted |
| [0007](0007-cpp20-for-implementation.md) | C++20 to implement the interpreter | Accepted |
| [0008](0008-standard-library-prelude.md) | The standard library as a C++ prelude | Accepted |
| [0009](0009-undefined-behavior-as-a-mechanic.md) | Undefined behavior as a game mechanic | Accepted |
| [0010](0010-script-mode-parsing-fallback.md) | Script mode: re-parsing failed top-level items as a block | Accepted |
| [0011](0011-statement-cost-unit.md) | Charging per statement as well as per instruction | Accepted |
