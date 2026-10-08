# 0008. The standard library as a C++ prelude

**Status:** Accepted

## Context

From the first object-oriented levels the player expects `std::vector`, `std::string`, `std::cout` and friends. Implementing them natively in the VM (as special types with their own opcodes) would duplicate the language: every container would need its own rules for copies, destructors, iterators, bounds checks and diagnostics, and every bug in them would bypass the checks the VM already does for player code.

## Decision

The standard library is written in the subset of C++ that cppi supports (`src/library/Prelude.cpp`) and compiled together with the player's program:

- It is parsed once per thread (`thread_local`) and bound before the player's code, under a `LibraryScope`. While binding it, features are not gated (a C++98 level can use `std::vector` even if templates are locked) and any diagnostic is reported at the player's call site.
- Code is generated only for the functions the program can reach, so an unused `std::map` costs nothing at run time.
- What C++ cannot express in the subset is a VM *intrinsic* (`__cppi_write_long`, `__cppi_check_index`, `__cppi_bad_access`...). Names starting with `__cppi_` are reserved.
- Library instructions carry no source location. They cost `CostModel::library_instruction` (0 by default), the debugger hides their frames, and runtime errors inside them are attributed to the player's line.

## Consequences

- The library automatically benefits from every memory check: an out-of-range `v[10]` is reported as E0503 at the player's line, a dangling `unique_ptr` as a use after free.
- Adding a container or an algorithm is writing C++, not changing the VM. Its limits are the interpreter's limits (for example, no member templates yet, so `emplace_back` is missing).
- Each compilation pays to bind the prelude (a few milliseconds in Debug). `ProgramCache` hides it when the code does not change; if it ever matters, the bound prelude could be cached per `HostRegistry`.
- The library is a simplified, teaching-oriented subset. It does not try to match the standard's complexity guarantees or every overload.
