# cppi conformance, library and C++23 — Implementation Plan

> **For agentic workers:** implement this plan task by task with the ECC orchestration commands. Run `/ecc:plan-orchestrate docs/plans/2026-11-17-conformance-library-cpp23.md` to get the ready-to-paste command for each task, then run them one at a time, in the order below:
> - new capability (C1, C5–C7, C9–C17 with C16b, C20, C21): `/ecc:orch-add-feature`;
> - wrong behavior (C2, C4, C4b): `/ecc:orch-fix-defect`;
> - a working behavior that must change (C19): `/ecc:orch-change-feature`;
> - verification only (C3): write the tests yourself. Use `/ecc:orch-fix-defect` only if one fails;
> - phase checkpoints (C8, C18) and the 1.0.0 release (C22): by hand, following `docs/ci-cd.md`. Tagging and publishing are not delegated.
>
> Each command writes the tests first, implements, reviews and commits. Use `/ecc:cpp-review` for an extra C++ review and `/ecc:cpp-build` when the build breaks. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make cppi behave like real C++ where the game relies on it, add the library and syntax its new levels need, and add a first C++23 chapter's worth of features. One release, **1.0.0**, built in three phases:
- **Phase 1:** conformance fixes and containers;
- **Phase 2:** modern initialization and syntax;
- **Phase 3:** library names follow the standard, and C++23.

1.0.0 is cppi's first stable release. From then on, SemVer forbids breaking the public API without a 2.0.0. The public API is the host registry, the diagnostic codes and their arguments, and which programs each standard accepts.

**Why:** cppi-farm's plan `docs/plans/2026-11-17-improvements-and-new-levels.md` (tasks B1–B3 and L23–L27) waits for this release. Its review found a level that teaches false C++ (`sorting`) only because cppi 0.3.1 lacks pair comparisons. That level stays wrong until 1.0.0 ships: the cost of releasing once. cppi-farm's plan names 0.4.0, 0.5.0 and 0.6.0; C22 has it changed to 1.0.0.

**Architecture:**
- The library stays C++ in `src/library/Prelude.cpp`, compiled with the player's code ([ADR 0008](../adr/0008-standard-library-prelude.md)).
- Syntax goes through the usual pipeline: `src/parse` (tree-sitter → AST), `src/sema`, `src/codegen`, `src/vm`.
- Diagnostics are data ([ADR 0004](../adr/0004-diagnostics-as-data.md)): a new code goes into `include/cppi/DiagCode.hpp`, `docs/diagnostics.md`, and both catalogs of `tools/cppi-run` (`EnglishCatalog.cpp`, `SpanishCatalog.cpp`).

**Tech Stack:** C++20, CMake presets, Conan 2, tree-sitter, GoogleTest.

## Facts checked against 0.3.1 (2026-11-20)

Read from the sources while reviewing the cppi-farm plan:
- `std::pair` has no comparison operators (`Prelude.cpp`, `struct pair`): C1.
- `std::vector` grows 0 → 4 → 8 → 16 by doubling. It stores elements in `new T[n]`, so it default-constructs every slot of its capacity and never destroys an element on `pop_back`, `clear`, `erase` or shrinking `resize`: C4.
- `std::map::find`, `begin` and `end` return `pair<K, V>*`, so `it->second` and `it == m.end()` already work. `operator[]` inserts `V()`.
- `std::min_element` and `std::max_element` exist, though `docs/roadmap.md` does not list them: C8 fixes the roadmap.
- `std::queue`, `std::priority_queue`, `std::stack`, `std::less` and `std::greater` do not exist: C5–C7.
- Declarations in conditions (`while (auto x = f())`) are supported. `if (init; cond)` is not (E0303).
- `noexcept` and `noexcept(false)` are parsed and ignored.
- E0512 `invalid-pointer` covers reading a local of a function that already returned.
- E0304 is unused.

Checked in the code while planning (2026-11-20):
- `string` keeps its own `new char[]` buffer (`Prelude.cpp`, `class string`). Only `map` is built on `vector` (`items_`).
- Intrinsics have fixed signatures (`Intrinsics::info`, `src/sema/Intrinsics.hpp`). None is typed by a template argument; the nearest precedent is `__cppi_caught`, whose type comes from the catch clause.
- There are no non-type template parameters (`TemplateEngine.cpp` reports E0303 "non-type template parameter"; `TreeConverter.cpp` drops them), no variadic templates, no partial specialization, and no function types as template arguments. The prelude uses fixed-arity overloads (`make_unique`) and defaulted parameters (`variant<A, B, C = …, D = …>`) instead.
- The AST already has `ast::InitList`, which `InitializerBinder::initialize_list` binds for aggregates and arrays.
- The parser sets `captures_this` for `[this]`, but sema rejects it with E0303 "capturing this" (`ExpressionBinder::lambda`). `[*this]` is not converted (`TreeConverter.cpp`).
- E0513's `what` argument is the class name without `std::` (`"optional"`).

Baseline on 0.3.1 (2026-11-20, g++ 13.3, Debug): 170 tests, all passing; one is skipped (`NumberFormat.AGlobalLocaleWithADecimalCommaChangesNothing`). The Release benchmarks (`cppi_bench`, built and run as in `.github/workflows/benchmarks.yml`) charge these `operations`; C4 and the checkpoints compare against them:

| Benchmark | `operations` | `executed` | CPU time |
|---|---|---|---|
| `BM_Level_ShortestPath` | 9222 | 20221 | 191 µs |
| `BM_Level_TravellingSalesman` | 150188 | 337225 | 3000 µs |
| `BM_Level_Memoization` | 6012 | 21057 | 193 µs |

The level benchmarks use `std::vector` and `push_back`, so they measure C4's cost. CI fails a pull request whose times regress by more than 50%. The operation counts are what the game's budgets use, so any rise in them must be explained in the changelog.

## Global Constraints

- **Tests first.** Each task starts with failing tests in `tests/unit/`, in the existing style: `printed`, `compile_error`, `runtime_error` and `standard(Standard::CppXX)` from `tests/support/ProgramRunner.hpp`; `CPPI_REQUIRE` in helpers that return a value. Language tests go in `LanguageTest.cpp` (C++98/11) or `ModernTest.cpp` (C++14 and later).
- **Build and test:** `conan install . -s build_type=Debug -s compiler.cppstd=20 --build=missing`, `cmake --preset conan-debug -DCPPI_WARNINGS_AS_ERRORS=ON`, `cmake --build --preset conan-debug`, `ctest --preset conan-debug --output-on-failure`.
- **Formatting.** CI checks `clang-format-18` (`.github/workflows/ci.yml`), so every commit is formatted with it first (`docs/ci-cd.md`, "Reproducing CI locally").
- **Golden tests.** The `golden` tests (`ctest -L golden`) compare cppi-run's output, messages included. A task that adds or changes a message (C10 if it adds `narrowing`, C19's E0304) regenerates them on purpose with `CPPI_UPDATE_GOLDEN=1` (`docs/ci-cd.md`). Each regenerated file is reviewed before the commit, never accepted blindly.
- **Real C++ is the oracle.** Every program a test expects to compile and print must compile and print the same with `g++ -std=c++XX` (or clang) at the test's standard. Every program a test expects to fail must fail in real C++ too, at compile time or as undefined behavior. Check new tests this way before writing the implementation.
- **Standards gate features.** A new language feature has a `Feature` entry (`include/cppi/Feature.hpp`) with the standard that introduced it, so E0301 `feature-requires-standard` keeps explaining it below that standard. Library names get the same check from C19 on.
  - The enumerator order is part of the ABI (`Feature.hpp`), so new features go at the end, before `Count`. 1.0.0 freezes that order.
  - Each new feature also gets its standard in `introduced_in` (`src/support/Feature.cpp`) and its tree-sitter nodes in `src/parse/NodeFeatureMap.cpp`.
- **Cost model.** Library code runs as player code and costs operations, so keep library functions simple: the game's budgets measure them.
- **Changelog.** Every task adds its lines under `## [Unreleased]` in `CHANGELOG.md` (Added / Changed / Fixed). The checkpoints leave them there; C22 moves them all under `## [1.0.0]`.
- **Docs.** `docs/roadmap.md` and `docs/diagnostics.md` change with the code that changes them.
- **Git.** One branch, `release-1.0.0`, for all three phases. The checkpoints (C8, C18) are verified commits on it, not tags. Commits follow Conventional Commits, as the user's own git identity, and end with the trailer `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Order and dependencies

- **Phase 1:** C1–C8, with C4b. Independent, except that C6 needs C1 (its `priority_queue` test compares pairs), C4b and C5's `std::less`/`std::greater`, and C8 is the phase's checkpoint.
- **Phase 2:** C9–C18, with C16b. C10 and C15 need C9; C16 needs C9 and C10; C17 needs C16b; C18 is the phase's checkpoint. C15 (non-type template parameters) and C17 (function types as template arguments, partial specialization) extend the template engine, so they are the phase's largest tasks.
- **Phase 3:** C19–C22. C20 needs C19's table, member entries included; C22 is the 1.0.0 release, after all three phases.

---

## Phase 1: conformance and containers

### Task C1: `std::pair` comparisons

**Files:** `src/library/Prelude.cpp`; `tests/unit/ModernTest.cpp`; `CHANGELOG.md`.

**Interfaces:** free function templates `operator==`, `!=`, `<`, `<=`, `>`, `>=` for `pair<A, B>`, comparing `first`, then `second`, written only with `<` and `==` of the members, as the standard specifies before C++20.

**Found while implementing (2026-11-20):** sema had to change first, as its own `fix(sema)` commit.
- Operators written as function templates were never found: `free_operators` only took ordinary functions.
- A name could not hold both ordinary functions and function templates (E0208), and `std` already has `operator<` for `string`.
- The fix adds `TemplateEngine::function_candidates` and mixed overload sets. On a tie, an ordinary function beats a template instance.
- It also stops repeating one missing operator inside a library function, drops E0222 after an error in a `return`, and names template operators `operator< <…>`.

- [ ] **Step 1: Failing tests.**
  - `PairComparesFirstThenSecond`: `std::pair<int,int>(1,5) < std::pair<int,int>(2,0)`, `(1,5) < (1,6)`, `==`, `!=`, `>=` print `1 1 1 0 1`.
  - `SortingPairsNeedsNoComparator`: `std::sort` of a `std::vector<std::pair<int,int>>` without a comparator prints the pairs in order.
  - `PairOfAClassWithoutLessHasNoLess`: a `pair<P, int>` with `struct P { int x; };` and `a < b` gives E0239 `no-operator` at the player's line.
- [ ] **Step 2: Run them and see them fail.**
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run all tests.** Expected: pass.
- [ ] **Step 5: Commit** `feat(library): std::pair comparison operators`.

### Task C2: virtual calls during construction and destruction

**Files:** `src/codegen/` and/or `src/vm/` (wherever the object's dynamic class is stored and read by virtual calls); `tests/unit/LanguageTest.cpp`; `CHANGELOG.md`.

**Rule (C++ [class.cdtor]):** while a base's constructor or destructor runs, a virtual call on the object calls the base's version, not the derived class's. E0511 `pure-virtual-call` already covers the pure case.

- [ ] **Step 1: Failing tests.**
  - `AVirtualCallInABaseConstructorCallsTheBase`: `struct B { B() { std::cout << name(); } virtual const char* name() { return "B"; } }; struct D : B { const char* name() override { return "D"; } }; D d; std::cout << d.name();` prints `BD`.
  - `AVirtualCallInABaseDestructorCallsTheBase`: the same with the call in `~B()`, printing `DB` (D's call first, then B's destructor).
  - `ACallThroughAMemberFunctionInTheConstructorAlsoCallsTheBase`: `B()` calls a non-virtual `hello()`, which calls `name()`: still `B`.
- [ ] **Step 2: Run them.** If they already pass, keep the tests, record "already conformant" in the changelog's Added (tests), and skip Step 3.
- [ ] **Step 3: Implement.** The object's dynamic class is set to each class as its constructor body starts (after the bases), and reset to each class as its destructor body ends, the way vtable pointers work.
- [ ] **Step 4: Run all tests**, including the existing virtual and abstract-class ones.
- [ ] **Step 5: Commit** `fix(vm): virtual calls in constructors and destructors call the class being built`.

**Done (2026-11-20):** the fix is in sema and codegen, not the VM.
- **Constructors:** each one writes its own class's headers (`BInitHeaders`) once its bases are built. Implicit constructors do the same in `default_initialize`.
- **Destructors:** each one writes them as it starts.
- **Cost:** these writes are `unpriced`: they have no source location, so they cost nothing and start no statement. Operation counts stay as in 0.3.1.
- **Limitation:** classes with virtual bases keep the old behavior during their own construction and destruction, because their layout is the complete object's. Once built, the complete object's headers are written again, including after `new`. Lifting this needs headers relative to the complete object; record it in the roadmap if a level ever needs it.

### Task C3: value-initialized enums

**Files:** `tests/unit/HostRegistryTest.cpp`, `tests/unit/LanguageTest.cpp`; source only if a test fails.

cppi-farm's `std-map` level relies on `std::map<char, Direction>::operator[]` inserting `Direction()`, the host enum's first enumerator.
- [ ] **Step 1: Tests.**
  - `AValueInitializedEnumIsZero`: `enum E { A, B }; E e = E(); std::cout << (e == A);` prints `1`, and the same for `enum class`.
  - `AValueInitializedHostEnumIsItsFirstValue`: with `add_enum("Direction", {"North", "East"})`, `std::map<char, Direction> m; std::cout << (m['x'] == North);` prints `1`.
- [ ] **Step 2: Run them.** Fix the value-initialization of enums in sema/codegen if one fails.
- [ ] **Step 3: Commit** `test(sema): value-initialized enums, host enums included`.

### Task C4: `std::vector` constructs and destroys only the elements it holds

**Files:** `src/library/Prelude.cpp` (`vector`); the new intrinsics in `src/sema` (prelude intrinsics, next to `__cppi_check_index`) and `src/vm`; `tests/unit/ModernTest.cpp`; `CHANGELOG.md`.

**Problem:** `new T[n]` in `reserve` default-constructs the whole capacity. So `std::vector<T>` needs a default constructor even for `push_back`, constructs objects nobody added (their constructors run and print), and never destroys removed elements (their destructors never run).

**Interfaces (prelude-only intrinsics, rejected outside the prelude).** Today's intrinsics have fixed signatures, so these need a new kind: an intrinsic typed by its template argument or by its pointer argument's pointee type.
- `T* __cppi_allocate<T>(int n)`: storage for `n` objects, not constructed. Leak and double-free tracking as for `new[]`.
- `void __cppi_deallocate(T* p)`: frees that storage without running destructors.
- `void __cppi_construct(T* p, const T& value)` (copy) and `void __cppi_construct_moved(T* p, T& value)` (move, used when growing); reading an unconstructed slot is E0502 `uninitialized-read`.
- `void __cppi_destroy(T* p)`: runs `~T()` and marks the slot unconstructed.

`vector` uses them in `push_back`, `pop_back`, `clear`, `resize`, `insert`, `erase`, `reserve`, the copy operations and the destructor. `resize(n)` value-initializes new elements; `resize(n, value)` copies.

- [ ] **Step 1: Failing tests.**
  - `AVectorDoesNotConstructItsCapacity`: a class with a printing default constructor; `std::vector<C> v; v.reserve(8);` prints nothing.
  - `AVectorNeedsNoDefaultConstructor`: a class with only `C(int)`; `push_back(C(1))` compiles and runs.
  - `PopBackAndClearDestroyElements`: a class with a printing destructor; `pop_back()` prints once, `clear()` once per element, and the vector's end prints nothing more.
  - `GrowingMovesOrCopiesEachElementOnce`: `v.reserve(4)`, then push 5 elements, counting the copies and moves the vector makes (not those of the argument temporaries): exactly 4 relocations at the 4 → 8 growth. The `reserve` keeps the count the same under g++ and clang, whose vectors grow 1 → 2 → 4 → 8 (7 relocations without it). The class's move constructor is `noexcept`, because real C++ copies instead of moving otherwise (`move_if_noexcept`). The test counts copies and moves together, so it does not depend on that choice.
  - `AReferenceIntoAVectorDiesWhenItGrows` (keeps cppi-farm's `iterator-invalidation` level honest): `int& r = v[0];` then 4 more `push_back`s, then reading `r` gives E0505 `use-after-free`.
- [ ] **Step 2: Run them and see them fail.**
- [ ] **Step 3: Implement the intrinsics, then rewrite `vector` with them.**
  - First, intrinsics whose signature follows `T`, in `Intrinsics.hpp` and the binder. `__cppi_caught`, typed by its context, is the nearest precedent.
  - Then the five intrinsics, in sema and the VM.
  - Then `vector`. `map` is built on `vector` (`items_`), so run its tests too. `string` keeps its own `char` buffer and does not change.
- [ ] **Step 4: Run all tests and the benchmarks** (`bench/`): a large rise in the operation counts of `push_back` loops is a regression to fix, because game budgets depend on them.
- [ ] **Step 5: Commit** `fix(library): vector constructs and destroys only the elements it holds`.

**Done (2026-11-20), differently from the plan:** there are no VM intrinsics.
- Sema lowers the five operations, prelude only (`library_depth > 0`), to nodes that already exist:
  - `__cppi_allocate<T>(n)` → `BNew` without a constructor or headers;
  - `__cppi_deallocate(p)` → `BDelete[]` without a destructor;
  - `__cppi_construct(p, v)` → `InitializerBinder::construct`, the copy constructor;
  - `__cppi_construct_moved(p, v)` → the same, with `v` expiring, so the move constructor is used if there is one;
  - `__cppi_construct(p)` → value-initialization;
  - `__cppi_destroy(p)` → `InitializerBinder::destroy` + `BUninit`.
- The pointer is kept in a frame cell so the target `*cell` can be cloned.
- It needed C4b first: done in the same change.
- Measured side by side with 0.3.1 under the same load, the operations are identical (9222 / 150188 / 6012) and CPU times are within noise (−6 % to +9 %).
- The whole suite also passes under AddressSanitizer.

### Task C4b: class template members are instantiated only when used

**Files:** `src/sema/TemplateEngine.cpp` (`instantiate_class`) and the binding of member function bodies; `tests/unit/ModernTest.cpp`; `CHANGELOG.md`.

**Problem (found during C1):** instantiating a class template binds every member function body at once. C++ ([temp.inst]) instantiates a member only when it is used. So `template <class A> struct W { A v; bool operator<(const W& o) const { return v < o.v; } }; struct P { int x; }; W<P> a;` gives E0239 in cppi but compiles with g++. C6 (`priority_queue`), C16 (`tuple`) and C20 (`expected`) add class templates whose members not every type supports, so this task comes before them.

A related limitation: cppi instantiates every function template candidate whose arguments deduce before choosing one, so a body that is invalid for these types reports errors even when overload resolution would not choose it. Fix it here if the same mechanism applies; otherwise record it for later.

- [ ] **Step 1: Failing tests.**
  - `AnUnusedMemberIsNotInstantiated`: the `W<P>` program compiles, and `W<int>` still compares.
  - `AUsedMemberReportsItsError`: calling `a < b` on `W<P>` gives E0239 at the call.
  - `std::vector<P>` and `std::map<char, P>` of a struct without `<` or `==` still compile.
- [ ] **Step 2: Implement.** Declare the members when the class is instantiated, and bind each body on its first use (call, address taken, virtual: the vtable needs virtual members at once).
- [ ] **Step 3: Run all tests and the benchmarks.**
- [ ] **Step 4: Commit** `fix(sema): class template members are instantiated only when used`.

**Done (2026-11-20), before C4, which needed it** (`std::vector<N>` with only `N(int)`):
- `bind_record` defers the bodies of non-virtual members written in the class into `DeclarationBinder::deferred_bodies`. Virtual and implicit members are still defined with the class.
- `TemplateEngine::define_if_pending` binds a deferred body on first use: from `make_call`, and from the constructor of `new` and the destructor of `delete`. It does so inside the template's scope, with the injected class name and the standard library's site. During a muted trial it waits for a real use.
- Not done: the related limitation (every deducible function template candidate is instantiated before overload resolution) stays.

### Task C5: function objects `std::less`, `std::greater`, `std::equal_to`

**Files:** `src/library/Prelude.cpp`; `tests/unit/ModernTest.cpp`; `CHANGELOG.md`.

- `template <class T> struct less { bool operator()(const T& a, const T& b) const { return a < b; } };`, and the same for `greater` and `equal_to`.
- [ ] **Step 1: Failing test** `SortWithStdGreater`: `std::sort(v.begin(), v.end(), std::greater<int>())` sorts descending.
- [ ] **Step 2: Implement; run all tests.**
- [ ] **Step 3: Commit** `feat(library): std::less, std::greater and std::equal_to`.

### Task C6: `std::queue`, `std::stack`, `std::priority_queue`

**Files:** `src/library/Prelude.cpp`; `tests/unit/ModernTest.cpp`; `CHANGELOG.md`; `docs/roadmap.md`.

**Interfaces:**
- `queue<T>`: `push`, `pop`, `front`, `back`, `empty`, `size`. A vector plus a head index that compacts when half is spent, so `pop` is O(1) amortized.
- `stack<T>`: `push`, `pop`, `top`, `empty`, `size` over `vector`.
- `priority_queue<T, Container = vector<T>, Compare = less<T>>`: `push`, `pop`, `top`, `empty`, `size`, as a binary heap (largest on top with `less`).
- `front`, `back`, `top` and `pop` on an empty adaptor: E0503 `out-of-bounds` through `__cppi_check_index`, the way `vector::back` reports today. (Real C++ leaves it undefined.)
- [ ] **Step 1: Failing tests:**
  - `QueueIsFirstInFirstOut`;
  - `StackIsLastInFirstOut`;
  - `PriorityQueueTopIsTheLargest`;
  - `PriorityQueueWithGreaterTopIsTheSmallest`: pairs `(cost, tile)`, which needs C1;
  - `PoppingAnEmptyQueueIsOutOfBounds`.
- [ ] **Step 2: Implement; run all tests.**
- [ ] **Step 3: Commit** `feat(library): std::queue, std::stack and std::priority_queue`.

**Done (2026-11-20):**
- `std::queue` is a ring buffer of constructed slots (C4's raw storage), not a vector with a head index. g++'s `pop` destroys the front at once, and a head index would destroy it only when compacting.
- `std::priority_queue` needed a sema fix first, in its own commit: a data member that is a function object can be called (`comp_(a, b)`, `obj.comp(a, b)`).

### Task C7: algorithms the game teaches

**Files:** `src/library/Prelude.cpp`; `tests/unit/ModernTest.cpp`; `CHANGELOG.md`.

Add `std::count_if`, `std::find_if`, `std::find_if_not`, `std::all_of`, `std::any_of`, `std::none_of`, `std::min_element`/`std::max_element` with a comparator, `std::iota`, and `std::stable_sort`. They are templates over pointers, like the existing ones.
- [ ] **Step 1: Failing tests**, one per name, each checked against g++.
- [ ] **Step 2: Implement; run all tests.**
- [ ] **Step 3: Commit** `feat(library): more algorithms (count_if, find_if, all_of...)`.

**Done (2026-11-20):** one test covers every name and matches g++. `stable_sort` reuses `sort`, an insertion sort, which is already stable.

**Found:** cppi has no function pointers at all. Passing a function's name (`std::count_if(b, e, even)`) gives E0206 `function-not-called`, so the C7 test passes a lambda. Task C16b adds function pointers, before C17 (`std::function` from a free function needs them).

### Task C8: Phase 1 checkpoint

No version bump, tag or release. The changelog lines stay under `## [Unreleased]`.
- [ ] `docs/roadmap.md` lists Phase 1's library names, `min_element`/`max_element` included.
- [ ] Full verification as listed in C22, and the benchmarks compared with 0.3.1: C4 changes `vector`'s operation counts, which game budgets depend on.
- [ ] Commit `docs(roadmap): phase 1 library names`.

**Done (2026-11-20), at `0803e81`:**
- The roadmap was updated with C6 and C7, so no separate roadmap commit was needed.
- Debug (gcc 13): 197 tests pass.
- Release (gcc 13, `BUILD_TESTING=ON`): 197 pass.
- clang 18 with `linux-clang` and `CPPI_SANITIZERS=address;undefined`: 197 pass, with no sanitizer reports.
- `conan create . -s compiler.cppstd=20`: the `test_package` runs ("harvested 2 in 12 operations").
- `clang-format-18`: clean.
- Level benchmarks: operations 9222 / 150188 / 6012, identical to 0.3.1. CPU times were within noise of 0.3.1 measured side by side (C4).

---

## Phase 2: initialization and syntax

### Task C9: braced lists and `std::initializer_list`

**Files:** `src/parse/TreeConverter.cpp`, `src/sema`, `src/codegen`; `src/library/Prelude.cpp` (`initializer_list<T>` with `begin`, `end`, `size`; `vector(initializer_list<T>)`, `vector& operator=(initializer_list<T>)`, `map(initializer_list<pair<K,V>>)`); `tests/unit/ModernTest.cpp`; `include/cppi/Feature.hpp` (`InitializerLists`, C++11).

`ast::InitList` and `InitializerBinder::initialize_list` already handle `= {…}` for aggregates and arrays. This task extends them to classes with an `initializer_list` constructor, to assignment, to range-for, and to function arguments. It does not start a new path.

- [ ] **Step 1: Failing tests:**
  - `std::vector<int> v = {1, 2, 3};`
  - `v = {4, 5};`
  - `std::map<char, int> m = {{'a', 1}};`
  - `for (int x : {1, 2, 3})`
  - `v.push_back({5, 2})` for an aggregate element.
  At C++98, `std::vector<int> v = {1, 2};` gives E0301.
- [ ] **Step 2: Implement; run all tests.**
- [ ] **Step 3: Commit** `feat: braced lists and std::initializer_list`.

**Done (2026-11-22):**
- **Prelude:** `initializer_list<T>` holds `__first`/`__size`, which sema fills. Sema puts the elements in a temporary array and registers it for destruction.
- **Expressions:** a braced list used as an expression is a placeholder (`BExpr::braced`). `OverloadResolver::braced_rank` ranks it: `initializer_list<E>` is Exact, a class is UserDefined, a scalar takes at most 1 element, and it never binds `T&`. `prepare` turns it into a `list_temporary` of the parameter's type. `to_rvalue` rejects it anywhere else.
- **Range-for:** `for (x : {...})` deduces `E` from the elements, binding them in a muted trial.
- **Also fixed:** a range-for never destroyed its range's temporaries (a returned vector leaked).
- **Not done:** `auto x = {1, 2};` (deducing `std::initializer_list` from `auto`) is not supported; it reports an error. Assigning a braced list to a scalar (`int x; x = {5};`) is not supported either.

### Task C10: direct-list-initialization `T{...}`

Today `Order{5, 2}` gives E0303 `unsupported-syntax`.
- [ ] **Step 1: Failing tests:**
  - `Order{5, 2}` for aggregates, as a temporary and as an argument;
  - `Point{1, 2}` calling a constructor;
  - `int{3}`;
  - narrowing `int{2.5}` gives a compile error (E0211 `cannot-convert`, or a new `narrowing` code if the message must differ);
  - `auto p = Point{1, 2};`.
- [ ] **Step 2: Implement; run all tests.**
- [ ] **Step 3: Commit** `feat: direct-list-initialization T{...}`.

**Done (2026-11-22):**
- **Parser:** `compound_literal_expression` becomes a `CastExpr` with `CastKind::List` whose operand is the `InitList`.
- **Sema:**
  - a class or array is a `list_temporary`;
  - a scalar takes at most one element, without narrowing, and `{}` is 0;
  - gated by `initializer-lists`.
- **Narrowing** got its own code, E0241 `narrowing` (`from`, `to`), because E0211 "cannot convert" would be wrong: the conversion exists outside braces. It applies from C++11 to every braced scalar element: `T{x}`, arrays, aggregates, `initializer_list` elements and braced arguments.
- **Not checked:** narrowing on arguments a braced list passes to a constructor (`Point{1, 2.5}` with `Point(int, int)`).

### Task C11: designated initializers (C++20)

- [ ] **Tests:**
  - `Order o = {.x = 5, .y = 2};` and `Order{.y = 2}`; the other members are value-initialized;
  - out-of-order designators give a compile error.
  - Feature `DesignatedInitializers`, C++20.
- [ ] **Implement, run, commit** `feat: designated initializers`.

**Done (2026-11-22):**
- `ast::InitList::designators` gives one name per element, filled from tree-sitter's `initializer_pair`.
- `InitializerBinder::designate` maps the names to fields.
- E0242 `designator-order` covers out-of-order, repeated and mixed designators; an unknown name is E0218.
- Nested (`.a.b`) and array (`[0] =`) designators are C, not C++: they stay E0303.

### Task C12: three-way comparison and defaulted comparisons (C++20)

- [ ] **Tests:**
  - `auto operator<=>(const P&) const = default;` gives `<`, `<=`, `>`, `>=`;
  - `bool operator==(const P&) const = default;` gives `==` and `!=`;
  - `std::sort` of `P`s works without a comparator;
  - `(a <=> b) < 0` with `std::strong_ordering` (the program has `#include <compare>`, which g++ needs to name `std::strong_ordering`; cppi accepts that include);
  - `<=>` on `int`s.
  - Feature `ThreeWayComparison`, C++20.
- [ ] **Implement** (library `strong_ordering`/`weak_ordering`/`partial_ordering` as classes compared with `0`; sema synthesizes the defaulted operators member by member), **run, commit** `feat: operator<=> and defaulted comparisons`.

**Done (2026-11-22):**
- **Prelude:** the orderings are aggregates with `__value`, compared with `int`, with conversion operators to the weaker categories. `__cppi_three_way` and `__cppi_three_way_floating` compute `<=>` on numbers.
- **Sema:**
  - `DeclarationBinder::synthesize_comparison` writes a defaulted body as AST (`this->m == __other.m && ...`, or a chain of `R(this->m <=> __other.m) != 0 ? ... : R{0}`) and binds it like the player's code;
  - a defaulted `<=>` declared `auto` gets its category when declared: strong, or partial with a `double` member;
  - rewriting happens in `ExpressionBinder::binary` at C++20.
- **Also fixed:**
  - a defaulted `operator==` had an empty body;
  - `define_body` treated every implicit member as a copy assignment;
  - a cast to a class type was rejected.
- **Not done:**
  - reversed candidates (`0 < (a <=> b)`, `b == a` across types);
  - `std::strong_ordering::less` and the other static members;
  - defaulted comparisons of classes with bases (E0303);
  - a member whose own `<=>` is weak or partial still counts as strong when the category is chosen.

### Task C13: `consteval`

- [ ] **Tests:**
  - a `consteval` function called with constants is evaluated at compile time;
  - called with a runtime value, it gives E0216 `not-constant`.
  - Feature `Consteval`, C++20.
- [ ] **Implement, run, commit** `feat: consteval functions`.

**Done (2026-11-22):**
- `consteval` is parsed as `constexpr` plus `TypeSpec::is_consteval`.
- `make_call` evaluates each call with `ctx_.constants.eval` and replaces it with a constant. Calls inside another consteval function are left to their caller.
- Only scalar results are folded; a consteval function returning a class still runs as an ordinary call.

### Task C14: capturing `this`

**Files:** `src/sema/ExpressionBinder.cpp` (`lambda`), `src/sema/CaptureAnalysis.cpp`, `src/parse/TreeConverter.cpp`, `src/codegen`; `tests/unit/ModernTest.cpp`; `CHANGELOG.md`.

Today the parser sets `captures_this` for `[this]`, and `ExpressionBinder::lambda` rejects it with E0303 "capturing this". The task removes that rejection and implements the capture. `[=]` captures `this` implicitly when the body names a member. `[*this]` also needs the parser to convert it: `TreeConverter` leaves it unsupported today.

- [ ] **Tests:**
  - `[this]` and `[=]` in a member function read and write members. The `[=]` test runs at C++17, because C++20 deprecates capturing `this` implicitly with `[=]` and g++ warns. At C++20, the same test uses `[=, this]`;
  - `[*this]` (C++17) copies the object;
  - a lambda that captured `this` and runs after its object was freed with `delete` gives E0505 `use-after-free`. (If the object was a local of a function that returned, cppi reports E0512 `invalid-pointer` instead, so the test uses `new`/`delete`.)
- [ ] **Implement, run, commit** `feat: lambdas capture this`.

**Done (2026-11-22):**
- The closure gets a field `__this` (the address) or `__self` (the copy, `[*this]`).
- `ExpressionBinder::captured_this` gives the enclosing object's address in the body. The `this` keyword, unqualified fields and member function calls fall back to it when they are not the closure's own captures.
- A default capture in a member function always holds `this`, which costs one cell even when the body does not use it.
- `[*this]` got its own feature, `lambda-copies-this` (C++17).
- The plan's "lambda runs after the object is destroyed" test uses `delete this` inside the member, because returning a closure from a function (a deduced class return type) is still unsupported.

### Task C15: `std::array`

- [ ] **Tests:**
  - `std::array<int, 3> a = {1, 2, 3};`, `a.size()`, `a[i]` with E0503 out of range;
  - `a.at(i)` throws `out_of_range`;
  - range-for;
  - `std::sort(a.begin(), a.end())`;
  - copying by value.
- [ ] **Non-type template parameters first** (missing today), as their own commit `feat(sema): non-type template parameters`:
  - the parser keeps `template <class T, int N>` and arguments such as `<int, 3>` (`TreeConverter.cpp` drops both);
  - the template engine accepts them instead of reporting E0303, and substitutes `N` as a constant;
  - the type table tells `array<int, 3>` from `array<int, 4>`.
  - Tests: a user template `template <int N> int twice() { return 2 * N; }`; `N` used as an array size; two instantiations that differ only in `N` are distinct types.
- [ ] **Implement `std::array`** (an aggregate with a `T elems[N]` member), **run, commit** `feat(library): std::array`.

**Done (2026-11-22):**
- **Values as types:** a non-type argument is a `TypeKind::TemplateValue` type (its value in `count`/`decl`). Instances stay keyed by `vector<TypeRef>`, and deduction (`Buf<int, N>`) works unchanged. `in_template_scope` binds `N` as a Constant.
- **Parser:** expression arguments go into `TypeDesc::value`. A plain name (`N`, `K`) is read from its symbol.
- **Scanner:** it no longer flags `template <X N>` as a concept. `collect_constraints` checks the concepts' standard instead.
- **`std::array`:** an aggregate with `T __elems[N]`, plus brace elision for an aggregate whose only member is an array.
- **Not done:** defaults for non-type parameters (`template <int N = 3>`), and deducing `N` from a built-in array parameter (`int (&a)[N]`).

### Task C16: `std::tuple`

- [ ] **Tests:**
  - `std::tuple<int, char, bool>`, `std::make_tuple`, `std::get<0>(t)`;
  - structured bindings `auto [a, b, c] = t;`;
  - `std::tie(a, b) = std::make_pair(1, 2);`;
  - comparisons.
- [ ] **Implement** (up to 4 elements, like `variant`, unless variadic templates exist), **run, commit** `feat(library): std::tuple`.

**Done (2026-11-22):**
- `tuple<A, B, C, D>` defaults its unused elements to `__tuple_none`. Structured bindings skip those.
- `std::get<I>` on a tuple or a pair is lowered by sema to the field (`ExpressionBinder::tuple_get`): its type depends on `I`, and the prelude cannot express that without partial specialization (C17).
- `std::tie` returns a struct of pointers (`__tie2`, `__tie3`), assignable from a `pair<A, B>` or a `tuple` of the tied types, not `std::tuple<A&, B&>`.

### Task C16b: function pointers (found in C7)

**Problem:** a function's name is not a value in cppi. `std::count_if(b, e, even)`, `int (*f)(int) = &twice;`, `f(3)` and passing `f` on all fail, with E0206 `function-not-called` or E0214. These are C++98 and common in teaching, and C17's `std::function` from a free function needs them.

**Files:** `src/sema` (types: pointer to function; `&f` and the implicit conversion of a function name, choosing an overload by the target type; calls through a pointer), `src/codegen`/`src/vm` (an indirect call), `include/cppi/Feature.hpp` only if a feature gate is wanted (C++98, so probably none); `tests/unit/LanguageTest.cpp`; `CHANGELOG.md`.

- [ ] **Step 1: Failing tests** (checked against g++):
  - `int (*f)(int) = twice; f(3)` and `(*f)(3)`;
  - `&twice`;
  - passing `even` to `std::count_if` and `std::sort`'s comparator;
  - an overloaded name picked by the pointer type;
  - a null function pointer called gives E0504 `null-dereference`;
  - comparing two pointers with `==`.
- [ ] **Step 2: Implement; run all tests and the benchmarks.**
- [ ] **Step 3: Commit** `feat: function pointers`.

**Done (2026-11-22):**
- **Types:** `TypeKind::Function`, with interned signatures in the type table; `pointer_to(function)` is `int (*)(int)`.
- **Parser:** `int (*f)(int)` gets a `DeclaratorKind::Function` part, only under parentheses, so a prototype is untouched.
- **Value and calls:** the value is the function's id + 1 (0 is null). `BCall::indirect` and the opcode `CallIndirect` call it; a null pointer is E0504, a non-function E0512. Reachability counts function-pointer constants.
- **Not done:** pointers to member functions; a function pointer returning a class or a reference (E0303); picking an overload when assigning (`p = f;`) or passing (`apply(f, 1)`) rather than initializing.

### Task C17: `std::function`

**Status (2026-11-22): blocked; work parked on the local branch `wip/c17-std-function` (`9a0cae5`).**
- **Done there:**
  - the parser reads a bare function type as a template argument (`std::function<int(int)>`);
  - sema maps `std::function<R(A...)>` to the prelude's `__function<R, A1, A2, A3>`, instead of the partial specialization planned below; the prelude stays C++;
  - the prelude has `__callable_base`/`__callable`, type erasure with `if constexpr` on the arity, and `bad_function_call`.
- **Blocker:** `__function` converts from any callable through a constructor template, `template <class F> __function(F f)`. cppi rejects constructor templates: `convert_method_template` refuses constructors, and construction and implicit conversions only consider ordinary constructors.
- **Next:** a task C16c, constructor templates:
  - in the parser;
  - as construction candidates (`InitializerBinder::construct`);
  - in implicit conversions, so `v.push_back(lambda)` converts.
  Then rebase the WIP branch and finish C17's tests.

- [ ] **Tests:**
  - `std::function<int(int)> f = [](int x) { return x + 1; };`;
  - a free function;
  - a capturing lambda;
  - copying the function object;
  - `std::vector<std::function<void()>>`;
  - calling an empty one throws `std::bad_function_call`.
- [ ] **Template engine first**, as its own commit `feat(sema): function types as template arguments and partial specialization`. `std::function` stays C++ in the prelude (ADR 0008), so the engine must express it:
  - the parser and type resolver accept a function type such as `int(int)` as a template argument;
  - partial specialization of class templates: `template <class R, class A> class function<R(A)>`, chosen by matching the argument against each specialization's pattern;
  - tests: a user `template <class T> struct Arity;` specialized for `R()` and `R(A)`, chosen correctly, and the primary template used otherwise.
- [ ] **Implement `std::function`** for 0–3 arguments (no variadic templates):
  - one partial specialization per arity;
  - type erasure through an abstract `__callable_base<R, A…>` with virtual `call` and `clone`, and a derived `__callable<F, R, A…>` holding the callable;
  - copying clones the held callable.
- [ ] **Run, commit** `feat(library): std::function`.

### Task C18: Phase 2 checkpoint

No version bump, tag or release.
- [ ] `docs/roadmap.md` lists Phase 2's syntax and library names.
- [ ] Full verification as listed in C22.
- [ ] Commit `docs(roadmap): phase 2 syntax and library`.

---

## Phase 3: library names follow the standard, and C++23

### Task C19: E0304 `library-requires-standard`

**Problem:** `std::optional` compiles at C++11, `std::make_unique` at C++11, and so on. The game's levels promise a standard, so a name newer than it must not compile.

**Files:** `include/cppi/DiagCode.hpp` (`LibraryRequiresStandard = 304`, args `name`, `standard`); a table of library names and their standards (`src/library/LibraryStandards.cpp`); the name-lookup step in `src/sema` that resolves `std::` names; `docs/diagnostics.md`; both cppi-run catalogs; `tests/unit/DiagnosticsTest.cpp`; `CHANGELOG.md` (Changed: programs that compiled before may not).

- The table, at least:
  - C++11: `unique_ptr`, `shared_ptr`, `make_shared`, `to_string`, `array`, `tuple`, `function`, `initializer_list`, `iota`, `all_of`/`any_of`/`none_of`, `find_if_not`;
  - C++14: `make_unique`;
  - C++17: `optional`, `nullopt`, `variant`, `visit`, `get_if`, `holds_alternative`;
  - C++20: the concepts, `strong_ordering`;
  - C++23: `expected`, `unexpected`, `to_underlying`, and the members `optional::and_then`, `optional::transform`, `optional::or_else`.
- Member entries (`class::member`) are checked at member lookup on library classes, because a member is not a `std::` name. The diagnostic's `name` is `std::optional::and_then`.
- Messages:
  - en: "'{name}' is from {standard}; this program is compiled as an earlier standard";
  - es: "'{name}' es de {standard}; este programa se compila con un estándar anterior".
- [ ] **Step 1: Failing tests:**
  - `std::optional<int>` at C++14 gives E0304 with `name=std::optional`, `standard=C++17`;
  - `std::make_unique` at C++11 gives E0304; at C++14 it compiles;
  - a member entry: with a test-only table entry for a member of an existing class (for example `optional::value_or` at C++23), calling it at C++20 gives E0304 with `name=std::optional::value_or`;
  - the prelude's own use of those names never reports.
- [ ] **Step 2: Implement; run all tests.** Fix tests that used newer names at older standards by raising their standard, not by weakening the check.
- [ ] **Step 3: Commit** `feat(sema): E0304, library names follow the standard`.

### Task C20: C++23 library: `std::expected`, monadic `std::optional`, `std::to_underlying`

**Files:** `src/library/Prelude.cpp`; `tests/unit/ModernTest.cpp`; `CHANGELOG.md`.

**Interfaces:**
- `expected<T, E>`: `has_value`, `operator bool`, `value()` (throws `bad_expected_access<E>`, derived from `std::exception`), `error()`, `operator*` (E0513 `bad-access` on an error, `what=expected`, without `std::`, like today's `what=optional`), `value_or`; `unexpected<E>`.
- `optional<T>::and_then(f)`, `transform(f)`, `or_else(f)`.
- `to_underlying(e)` for enums, host enums included.
- [ ] **Step 1: Failing tests:**
  - an `expected` holding a value and one holding an error;
  - `.value()` on an error is an uncaught `bad_expected_access`, E0404 at top level;
  - `*e` on an error gives E0513;
  - `find().and_then(path).transform(first)` on full and empty optionals;
  - `std::to_underlying(Problem::Empty)` is `1`;
  - each name at C++20 gives E0304: `std::expected`, `std::unexpected` and `std::to_underlying` through name lookup, and `and_then`, `transform` and `or_else` through C19's member entries.
- [ ] **Step 2: Implement; run all tests.**
- [ ] **Step 3: Commit** `feat(library): std::expected, monadic optional and std::to_underlying (C++23)`.

### Task C21: multidimensional subscript (C++23)

- [ ] **Step 1: Failing tests:**
  - `struct M { int operator[](int r, int c) const { return r * 10 + c; } }; M m; std::cout << m[2, 3];` prints `23` at C++23;
  - at C++20 it gives E0301 (feature `MultidimensionalSubscript`, C++23);
  - a comma expression inside `[]` at C++20 keeps its old meaning. (C++20 deprecates it and g++ warns with `-Wcomma-subscript`; the oracle check accepts that warning.)
- [ ] **Step 2: Implement** (the parser keeps every subscript argument; sema looks for an `operator[]` taking that many), **run all tests, commit** `feat: multidimensional subscript operator (C++23)`.

### Task C22: release 1.0.0

- [ ] **Stability review.** Before the version changes, read the public API that 1.0.0 freezes:
  - the host registry in `include/cppi/`;
  - `DiagCode.hpp` (codes and their arguments);
  - the `Feature` table and C19's library table.
  Fix anything that should not be promised now, because changing it after 1.0.0 means a 2.0.0.
- [ ] **Version and docs:**
  - `CMakeLists.txt` `project(cppi VERSION 1.0.0)`;
  - `CHANGELOG.md` moves every `## [Unreleased]` line under `## [1.0.0] - <date>`, with Changed noting that E0304 rejects programs that 0.3.1 accepted;
  - `docs/roadmap.md` is complete for the three phases;
  - `docs/ci-cd.md` says that from 1.0.0 on releases are not pre-releases and the public API follows SemVer.
- [ ] **Full verification:**
  - Debug and Release;
  - sanitizers, as `docs/ci-cd.md` runs them: the `conan/profiles/linux-clang` profile and `-DCPPI_SANITIZERS=address;undefined`;
  - `test_package` (`conan create . --build=missing`);
  - `clang-format-18` clean;
  - the benchmarks compared with the 0.3.1 baseline above.
- [ ] **Publish:** tag `v1.0.0` and publish the release the way 0.3.1 was (`docs/ci-cd.md`), not marked pre-release, including the `cppi-1.0.0-conan-all.tgz` cache bundle that cppi-farm's `tools/local_ci.py setup cppi` downloads.
- [ ] **Tell cppi-farm:** B1–B3 and L23–L27 can all start, and its plan's references to 0.4.0, 0.5.0 and 0.6.0 become 1.0.0.

---

## After this plan

- The roadmap's "Next" keeps: preprocessor macros, `set`/`deque`, ranges, coroutines and modules, and threads.
- `if (init; cond)` (C++17) stays unsupported. It is a small, separate task if a level ever needs it.
