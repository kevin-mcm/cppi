# Integrating with the game

This guide describes how a game (for example, a Godot project with GDExtension) uses `cppi`.

## 1. Dependency

```python
# conanfile.py of the game's native module
def requirements(self):
    self.requires("cppi/0.1.0")
```

For mobile, use the profiles in `conan/profiles/` as a base (`android-arm64`, `ios-arm64`). They disable `cppi-run`, which makes no sense on a phone.

## 2. Exposing the game world

Register everything the player can use once, when the level loads:

```cpp
cppi::HostRegistry host;
const auto dir = host.add_enum("Direction", {"North", "East", "South", "West"});

host.function("move").param(dir).cost(5).bind([this](cppi::HostCall& call) {
    if (!automaton_.can_move(call.arg(0).as_enum())) {
        call.fail(kErrorBlocked, "wall");   // the game translates kErrorBlocked
        return cppi::Value::void_value();
    }
    automaton_.move(call.arg(0).as_enum());
    return cppi::Value::void_value();
});
```

- `cost` is the price in operations of each call: this is how the game is balanced.
- Names follow C++ rules and cannot repeat; a mistake here throws `std::invalid_argument` at startup.
- The lambdas capture game state: that state must outlive the `Interpreter`.

## 3. Level rules

```cpp
cppi::Options rules;
rules.standard = save.current_era();            // C++98, C++11, ...
rules.locked = save.locked_features();          // what is not unlocked yet
interpreter.set_options(rules);
```

## 4. Compiling and showing errors

```cpp
cppi::ProgramCache cache;   // optional: pressing "Run" again without edits does not recompile
const auto compiled = cache.compile(interpreter, editor.text());   // or interpreter.compile(...)
for (const auto& d : compiled.diagnostics) {
    editor.mark(d.range, tr(cppi::diag_key(d.code), d.args));   // your translation catalog
}
```

`tools/cppi-run/` has a complete reference implementation (`Messages` plus one `MessageCatalog` per language: `EnglishCatalog.cpp`, `SpanishCatalog.cpp`). In Godot, the natural choice is a translation CSV with the keys from `docs/diagnostics.md`.

## 5. Running

**All at once** (simulation, checking the solution):

```cpp
const auto result = interpreter.run(*compiled.program, {.budget = level.budget});
```

**Tick by tick** (visible animation of the automaton):

```cpp
execution_ = interpreter.start(*compiled.program, {.budget = level.budget});

void Game::_process(double) {
    if (!execution_ || execution_->finished()) return;
    // Advance to the automaton's next visible action.
    const auto before = automaton_.action_count();
    while (execution_->step() == cppi::RunStatus::Running && automaton_.action_count() == before) {
    }
    editor.highlight_line(execution_->current_location().begin.line);
}
```

An `ExecutionObserver` receives every host call before it runs: useful for logging, debugging, or triggering animations without touching the game's functions.

### Costs and limits

```cpp
cppi::RunOptions run;
run.budget = 10'000;               // operations available
run.cost.instruction = 1;          // each instruction of the player's code
run.cost.library_instruction = 0;  // inside std::vector, std::string...: free by default
// Or charge per statement: each statement, and each test of an if or a loop, costs 1.
// run.cost.unit = cppi::CostModel::Unit::Statement;
run.max_call_depth = 1000;         // deeper recursion stops with E0402 stack-overflow
```

With `CostModel::Unit::Statement` the budget reads as "lines run": `for (int i = 0; i < 10; i++) { a(); b(); }` costs 32 (the init, eleven tests, twenty statements) plus what `a` and `b` cost as host functions, and the number does not change between cppi versions. The default unit, `Instruction`, charges bytecode instructions, which is finer but depends on the code generator. See docs/architecture.md ("Operations").

Host functions add their own `cost`. Because the standard library is free by default, `v.push_back(x)` costs the same as any other call: the player pays for what they write, not for how the library is implemented.

`RunResult::line_operations` (and `Execution::line_operations()` while stepping) says where the operations went: one `LineOperations{line, operations}` per line that spent any, sorted by line, adding up to `operations`. What the library, the compiler's implicit member functions and host functions cost is charged to the player's line that called them. Use it to show the "hot lines" of a solution, or to balance a level.

### Output

What the program prints with `std::cout` is collected in `RunResult::output` (and `Execution::output()` while stepping). Show it in the game's console. To show it as it is printed, override `ExecutionObserver::on_output(text)`.

### Runtime errors (undefined behavior)

A status of `RuntimeError` means the program did something that is undefined behavior in real C++ (dividing by zero, reading an uninitialized variable, indexing out of bounds, using a dangling pointer...) or hit a resource limit. The diagnostic (5xx, or E0402/E0403) points at the exact line and is meant to be shown as part of the game: "the automaton tripped over a dangling pointer". `cppi::is_undefined_behavior(d.code)` tells them apart. A leak is reported as a *warning* (E0510) when the program ends.

## 6. Debugger

`Execution` doubles as a debugger backend:

```cpp
auto execution = interpreter.start(*compiled.program, run);
execution.set_breakpoint(12);                       // line, 1-based
if (execution.run().status == cppi::RunStatus::Paused) {
    for (const cppi::StackFrame& frame : execution.call_stack()) {   // innermost first
        panel.add(frame.function, frame.location.begin.line);
        for (const cppi::Variable& v : frame.locals) {
            panel.add_variable(v.name, v.type, v.initialized ? v.value : "?", v.children);
        }
    }
}
execution.step_line();   // "next line"; step() runs a single instruction
execution.run();         // continue to the next breakpoint or the end
```

- `function` is `"<script>"` for the loose top-level statements.
- `Variable::children` holds array elements and class members, so they can be shown as a tree.
- The standard library is invisible: its frames are not listed and stepping never stops inside it.

## 7. Threads

Compile on whichever thread you prefer; a `Program` can run from several threads, and `ProgramCache` is thread-safe. Each `Execution` belongs to one thread, and host functions run on the thread that calls `run()` or `step()`.
