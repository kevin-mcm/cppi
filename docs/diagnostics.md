# Diagnostics catalog

Reference for whoever translates the game's messages. **Codes and keys are stable**: they are never renumbered or reused. Arguments in `[brackets]` are optional.

Each diagnostic also carries a severity (`error`, `warning`, `note`) and a range in the code (line and column, 1-based, columns in UTF-8 bytes).

## 1xx · Syntax

| Code | Key | Arguments | When |
|---|---|---|---|
| E0100 | `syntax-error` | — | The text is not valid C++. |
| E0101 | `missing-token` | `token`, [`line`] | A symbol is missing, for example `;`, `)` or `}`. The range is empty and sits right after the last token before it. `line` is set when the token belongs at the end of that line (`int a = 5` ⏎ → "missing ';' at the end of line 1"). |
| E0102 | `nesting-too-deep` | `limit` | Expressions nested more than `limit` levels deep. |

## 2xx · Semantics

| Code | Key | Arguments | When |
|---|---|---|---|
| E0200 | `unknown-function` | `name`, `[suggestion]` | A function that does not exist is called. |
| E0201 | `unknown-identifier` | `name`, `[suggestion]` | A name that does not exist is used. |
| E0202 | `argument-count-mismatch` | `function`, `expected`, `actual` | Wrong number of arguments. |
| E0203 | `argument-type-mismatch` | `function`, `index`, `expected`, `actual` | An argument has the wrong type. `index` starts at 1. |
| E0204 | `not-callable` | `name` | Something that is not a function is called (`East()`). |
| E0205 | `integer-out-of-range` | `literal` | A number does not fit in 64 bits. |
| E0206 | `function-not-called` | `name` | A function is used without `()`. *Warning* if it is a statement (`harvest;`), error if it is a value. |
| E0207 | `expression-has-no-effect` | — | *Warning*: a statement that does nothing (`42;`). |
| E0208 | `redefinition` | `name` | A name is declared twice in the same scope. |
| E0209 | `not-assignable` | `[name]` | Assigning to something that is not a modifiable lvalue (a constant, a literal, a `const` object). |
| E0210 | `invalid-operands` | `op`, `left`, `[right]` | An operator does not accept those types (`"a" * 2`). |
| E0211 | `cannot-convert` | `from`, `to` | No conversion between the two types. |
| E0212 | `misplaced-jump` | `statement` | `break` or `continue` outside a loop (or `switch`). |
| E0213 | `return-type-mismatch` | `function`, `expected`, `actual` | `return` with a value of the wrong type, or without a value. |
| E0214 | `unknown-type` | `name`, `[suggestion]` | A type name that does not exist. |
| E0215 | `invalid-array-size` | `[size]` | Array size that is zero, negative or too large. |
| E0216 | `not-constant` | — | A constant expression is required (array size, `case` label, template argument). |
| E0217 | `duplicate-case` | `value` | Two `case` labels with the same value. |
| E0218 | `no-member` | `type`, `member`, `[suggestion]` | The class has no member with that name. |
| E0219 | `not-subscriptable` | `type` | `[]` on something that is not an array, pointer or class with `operator[]`. |
| E0220 | `too-many-initializers` | `expected`, `actual` | More values in `{...}` than elements. |
| E0221 | `uninitialized-const` | `name` | A `const` variable without a value. |
| E0222 | `missing-return` | `function` | *Warning*: a non-`void` function can end without `return`. |
| E0223 | `inaccessible-member` | `member`, `class`, `access` | Using a `private` or `protected` member from outside. `access` is `private` or `protected`. |
| E0224 | `ambiguous-name` | `name` | The name is found in more than one base class. |
| E0225 | `abstract-class` | `class`, `[function]` | Creating an object of a class with pure virtual functions; `function` is one of them. |
| E0226 | `no-matching-function` | `function`, `arguments` | No overload accepts those arguments. `arguments` is a list of types (`int, double`). |
| E0227 | `static-assertion-failed` | `[message]` | A `static_assert` is false. |
| E0228 | `assignment-in-condition` | — | *Warning*: `if (x = 1)`; probably `==` was meant. |
| E0229 | `reference-needs-lvalue` | `type` | Binding a non-`const` reference to a temporary. |
| E0230 | `unsupported-type` | `name` | A type (or use of a type) this version does not support yet. |
| E0231 | `declaration-not-allowed` | `construct` | A declaration that cannot appear there (e.g. a static data member). |
| E0232 | `no-default-constructor` | `class` | An object is created without arguments but the class has no default constructor. |
| E0233 | `nothing-to-override` | `function` | `override` on a function that does not override a virtual one. |
| E0234 | `template-deduction` | `function` | The template arguments cannot be deduced from the call. |
| E0235 | `ambiguous-call` | `function`, `arguments` | Two or more overloads match equally well. |
| E0236 | `undefined-function` | `function` | A function is declared and called but never defined. |
| E0237 | `deleted-function` | `function` | Calling a function declared `= delete` (e.g. copying a `unique_ptr`). |
| E0238 | `constraints-not-satisfied` | `function`, `[constraint]` | The template arguments do not satisfy a concept (`Number<std::string>`) or a `requires` clause (`constraint` = `requires-clause`). |
| E0239 | `no-operator` | `op`, `type` | An operator is used on a class that has none for those operands (`a < b` with `struct P { int x; };`, or `std::sort` of a `std::vector<P>` without a comparator). `op` is `<`, `==`..., `type` the class. Inside the standard library it is reported at the player's call, followed by E0240. |
| E0240 | `instantiated-from` | `function` | *Note* after an error inside the standard library: the library function (`std::sort<pair<int, int>>`) the player's code at this location used. |

## 3xx · Language features

| Code | Key | Arguments | When |
|---|---|---|---|
| E0300 | `feature-locked` | `feature` | The game has not unlocked that feature yet. |
| E0301 | `feature-requires-standard` | `feature`, `required`, `current` | The feature belongs to a later standard. |
| E0302 | `feature-not-implemented` | `feature` | Allowed, but this version of the interpreter cannot run it yet. |
| E0303 | `unsupported-syntax` | `construct` | A construct that does not match any known feature. |

`feature` is a stable feature key:

| Key | Standard | | Key | Standard |
|---|---|---|---|---|
| `function-calls` | C++98 | | `auto` | C++11 |
| `variables` | C++98 | | `range-for` | C++11 |
| `operators` | C++98 | | `lambdas` | C++11 |
| `conditionals` | C++98 | | `nullptr` | C++11 |
| `loops` | C++98 | | `enum-class` | C++11 |
| `blocks` | C++98 | | `static-assert` | C++11 |
| `user-functions` | C++98 | | `constexpr` | C++11 |
| `arrays` | C++98 | | `type-aliases` | C++11 |
| `strings` | C++98 | | `binary-literals` | C++14 |
| `floating-point` | C++98 | | `digit-separators` | C++14 |
| `structs` | C++98 | | `structured-bindings` | C++17 |
| `classes` | C++98 | | `if-constexpr` | C++17 |
| `inheritance` | C++98 | | `concepts` | C++20 |
| `enums` | C++98 | | `coroutines` | C++20 |
| `pointers` | C++98 | | `modules` | C++20 |
| `references` | C++98 | | `delegating-constructors` | C++11 |
| `templates` | C++98 | | | |
| `namespaces` | C++98 | | | |
| `exceptions` | C++98 | | | |
| `preprocessor` | C++98 | | | |

`concepts` also covers constrained template parameters (`template <Number T>`) and `auto` parameters of functions (`void f(auto x)`), which are C++20 abbreviated templates. `strings` (the old feature key for string literals) and `preprocessor` (`#define` and friends) are recognized but not implemented yet; `#include` is accepted and ignored.

## 4xx · Execution

| Code | Key | Arguments | When |
|---|---|---|---|
| E0400 | `budget-exhausted` | `budget` | The next action would exceed the operation budget. |
| E0401 | `host-error` | `function`, `error`, `detail` | A game function failed (`HostCall::fail`). `error` is a game-defined code; `4294967295` indicates an exception or an invalid return value from the host. |
| E0402 | `stack-overflow` | `depth` | Calls nested deeper than `RunOptions::max_call_depth` (runaway recursion). |
| E0403 | `out-of-memory` | `cells` | The program asked for more memory than the interpreter allows. |
| E0404 | `uncaught-exception` | `type`, `[what]` | An exception nobody caught; `what` is the message of a `std::exception`. Reported at the `throw`. |
| E0405 | `exception-during-unwind` | `type` | An exception was thrown while another one was still unwinding (e.g. from a destructor). |
| E0406 | `no-active-exception` | — | `throw;` with no exception being handled. |

## 5xx · Undefined behavior

In real C++ these would silently corrupt the program or crash it; here they stop it with an explanation, at the exact line. The run ends with status `RuntimeError`. Use `cppi::is_undefined_behavior(code)` to tell them apart.

| Code | Key | Arguments | When |
|---|---|---|---|
| E0500 | `division-by-zero` | — | `x / 0` or `x % 0`. |
| E0501 | `integer-overflow` | `type` | A signed integer operation does not fit its type. |
| E0502 | `uninitialized-read` | `[name]` | Reading a variable that was never given a value. |
| E0503 | `out-of-bounds` | `index`, `size` | Array, `vector` or `string` index outside `[0, size)`. |
| E0504 | `null-dereference` | — | Using a null pointer (`*p`, `p->x`). |
| E0505 | `use-after-free` | — | Using memory after `delete`. |
| E0506 | `double-free` | — | `delete` twice on the same memory. |
| E0507 | `invalid-delete` | — | `delete` on memory that did not come from `new` (or `delete` vs `delete[]` mismatch). |
| E0508 | `flow-off-end` | `function` | A non-`void` function reached its end without `return`. |
| E0509 | `invalid-shift` | `amount` | Shifting by a negative amount or by at least the width of the type. |
| E0510 | `memory-leak` | `count` | *Warning* at the end of the run: `count` blocks from `new` were never deleted. |
| E0511 | `pure-virtual-call` | `function` | A pure virtual function is called (from a constructor or destructor). |
| E0512 | `invalid-pointer` | — | Using a pointer outside the object it points into, or to a local variable of a function that already returned. |
| E0513 | `bad-access` | `what` | `*opt` on an empty `std::optional` (`what` = `std::optional`). `opt.value()` throws `std::bad_optional_access` instead. |

## Translation example

Templates from `tools/cppi-run/EnglishCatalog.cpp` and `tools/cppi-run/SpanishCatalog.cpp`:

| Key | Spanish | English |
|---|---|---|
| `feature-locked` | Aún no desbloqueaste {feature} | You haven't unlocked {feature} yet |
| `feature-requires-standard` | Para usar {feature} necesitas {required} (estás usando {current}) | Using {feature} requires {required} (you are using {current}) |
| `unknown-function` | No existe ninguna función llamada '{name}' | There is no function called '{name}' |
