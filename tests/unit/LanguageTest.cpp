/// @file LanguageTest.cpp
/// @brief End-to-end tests of the language cppi accepts: each program runs on
/// the recording host, and observable values go out through take_int(),
/// take_long(), take_double() and take_bool().
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "support/ProgramRunner.hpp"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace cppi;
using cppi_test::Calls;
using cppi_test::compile_error;
using cppi_test::output;
using cppi_test::run;
using cppi_test::runtime_error;
using cppi_test::standard;

// =============================================================================
// Phase 2: variables, operators, control flow
// =============================================================================

TEST(Language, VariablesAndArithmeticFollowC) {
    EXPECT_TRUE(
        (output("int a = 7; int b = 2; take_int(a / b); take_int(a % b); take_int(-a / b); take_int(a - b * 3);") ==
         Calls{"take_int(3)", "take_int(1)", "take_int(-3)", "take_int(1)"}));
    EXPECT_EQ(output("int x = 5; x += 3; x *= 2; x -= 1; x /= 3; take_int(x);"), Calls{"take_int(5)"});
    EXPECT_TRUE((output("int i = 0; int j = i++; int k = ++i; take_int(i); take_int(j); take_int(k);") ==
                 Calls{"take_int(2)", "take_int(0)", "take_int(2)"}));
    EXPECT_TRUE(
        (output("int b = 6 & 3; int o = 6 | 3; int x = 6 ^ 3; take_int(b); take_int(o); take_int(x); take_int(1 << 4); "
                "take_int(-16 >> 2); take_int(~0);") ==
         Calls{"take_int(2)", "take_int(7)", "take_int(5)", "take_int(16)", "take_int(-4)", "take_int(-1)"}));
}

TEST(Language, BitwiseOperatorsOnVariablesOfEveryIntegerType) {
    // Variables, not constants: these run in the VM rather than being folded.
    EXPECT_TRUE((output("int a = 12; int s = 2; take_int(a << s); take_int(a >> s); take_int(a & 10); take_int(a | 3); "
                        "take_int(a ^ 5);") ==
                 Calls{"take_int(48)", "take_int(3)", "take_int(8)", "take_int(15)", "take_int(9)"}));
    EXPECT_TRUE((output("long b = 1; long t = 40; take_long(b << t); take_long((b << t) >> 38);") ==
                 Calls{"take_long(1099511627776)", "take_long(4)"}));
    EXPECT_TRUE((output("unsigned u = 12; unsigned s = 2; take_long(u << s); take_long(u >> s); take_long(u & 10u); "
                        "take_long(u | 3u); take_long(u ^ 5u);") ==
                 Calls{"take_long(48)", "take_long(3)", "take_long(8)", "take_long(15)", "take_long(9)"}));
    EXPECT_EQ(runtime_error("int a = 1; int s = 40; take_int(a << s);").code, DiagCode::InvalidShift);
    EXPECT_EQ(runtime_error("unsigned u = 1; int s = -1; take_long(u >> s);").code, DiagCode::InvalidShift);
}

TEST(Language, ComparisonsLogicAndShortCircuit) {
    EXPECT_EQ(output("take_bool(3 < 4 && 4 <= 4 && 5 > 4 && 4 >= 5 == false && 1 != 2);"), Calls{"take_bool(true)"});
    // The right side of && is not evaluated when the left is false.
    EXPECT_EQ(output("bool b = false && harvest(); take_bool(b);"), Calls{"take_bool(false)"});
    EXPECT_EQ(output("bool b = true || harvest(); take_bool(!b);"), Calls{"take_bool(false)"});
    EXPECT_EQ(output("int x = 3; take_int(x > 2 ? 10 : 20);"), Calls{"take_int(10)"});
}

TEST(Language, LongDoubleCharAndBoolConversions) {
    EXPECT_EQ(output("long big = 3000000000; take_long(big * 2);"), Calls{"take_long(6000000000)"});
    EXPECT_TRUE((output("double d = 7 / 2; double e = 7 / 2.0; take_double(d); take_double(e);") ==
                 Calls{"take_double(3)", "take_double(3.5)"}));
    EXPECT_TRUE((output("int i = 3.9; take_int(i); take_int((int)-2.5);") == Calls{"take_int(3)", "take_int(-2)"}));
    EXPECT_TRUE((output("char c = 'A'; c += 2; take_int(c); char d = 200; take_int(d);") ==
                 Calls{"take_int(67)", "take_int(-56)"}));
    EXPECT_EQ(output("bool b = 5; take_int(b + b);"), Calls{"take_int(2)"});
    EXPECT_TRUE((output("double d = 5; d /= 2; take_double(d); d += 1; d *= 2; d -= 0.5; take_double(d);") ==
                 Calls{"take_double(2.5)", "take_double(6.5)"}));
    EXPECT_TRUE((output("int i = 2; i += 1.5; take_int(i); double d = 1; d += i; take_double(d);") ==
                 Calls{"take_int(3)", "take_double(4)"}));
    EXPECT_TRUE((output("take_long(sizeof(int)); take_long(sizeof(double)); take_long(sizeof(char));") ==
                 Calls{"take_long(4)", "take_long(8)", "take_long(1)"}));
}

TEST(Language, IfElseAndNestedBlocks) {
    EXPECT_TRUE(
        (output("int x = 5; if (x > 3) { take_int(1); } else { take_int(2); } if (x > 10) take_int(3); else if (x > 4) "
                "take_int(4);") == Calls{"take_int(1)", "take_int(4)"}));
    EXPECT_TRUE((output("int x = 1; { int x = 2; take_int(x); } take_int(x);") == Calls{"take_int(2)", "take_int(1)"}));
    EXPECT_TRUE((output("int n = count(); if (int m = n * 10) take_int(m);") == Calls{"count()", "take_int(10)"}));
}

TEST(Language, LoopsWhileDoWhileForBreakAndContinue) {
    EXPECT_TRUE((output("int i = 0; while (i < 3) { take_int(i); i++; }") ==
                 Calls{"take_int(0)", "take_int(1)", "take_int(2)"}));
    EXPECT_EQ(output("int i = 10; do { take_int(i); } while (i < 3);"), Calls{"take_int(10)"});
    EXPECT_TRUE((output("for (int i = 0; i < 10; ++i) { if (i % 2 == 0) continue; if (i > 5) break; take_int(i); }") ==
                 Calls{"take_int(1)", "take_int(3)", "take_int(5)"}));
    EXPECT_EQ(output("int s = 0; for (int i = 1; i <= 3; i++) for (int j = 1; j <= 3; j++) s += i * j; take_int(s);"),
              Calls{"take_int(36)"});
    EXPECT_EQ(output("for (;;) { take_int(1); break; }"), Calls{"take_int(1)"});
}

TEST(Language, SwitchWithFallThroughAndDefault) {
    const char* program = R"(
        void f(int x) {
            switch (x) {
                case 1: take_int(1);
                case 2: take_int(2); break;
                case 3: take_int(3); break;
                default: take_int(0);
            }
        }
        f(1); f(3); f(9);
    )";
    EXPECT_TRUE((output(program) == Calls{"take_int(1)", "take_int(2)", "take_int(3)", "take_int(0)"}));
    EXPECT_TRUE(
        (output("switch (next_direction()) { case North: take_int(0); break; case South: take_int(2); break; }") ==
         Calls{"next_direction()", "take_int(2)"}));
}

namespace {

/// Whether compiling `program` warns that a function may end without returning.
bool warns_missing_return(const char* program) {
    auto r = run(program);
    EXPECT_TRUE(r.compiled.ok()) << program;
    for (const auto& d : r.compiled.diagnostics) {
        if (d.code == DiagCode::MissingReturn) {
            EXPECT_EQ(d.arg_text("function"), "opposite");
            return true;
        }
    }
    return false;
}

}  // namespace

TEST(Language, ASwitchWhoseCasesAllReturnNeedsNoReturnAfterIt) {
    // Every case returns and there is a default: the end cannot be reached.
    const char* every_case_returns = R"(
        enum Dir { N, E, S, W };
        Dir opposite(Dir d) {
            switch (d) {
                case N: return S;
                case E: return W;
                case S: return N;
                default: return E;
            }
        }
        take_int(opposite(E));
        take_int(opposite(W));
    )";
    EXPECT_FALSE(warns_missing_return(every_case_returns));
    EXPECT_TRUE((output(every_case_returns) == Calls{"take_int(3)", "take_int(1)"}));
    // Without a default, a value may match no case.
    EXPECT_TRUE(warns_missing_return(R"(
        enum Dir { N, E, S, W };
        Dir opposite(Dir d) {
            switch (d) {
                case N: return S;
                case E: return W;
                case S: return N;
                case W: return E;
            }
        }
    )"));
    // A break leaves the switch and reaches the end.
    EXPECT_TRUE(warns_missing_return(R"(
        enum Dir { N, E, S, W };
        Dir opposite(Dir d) {
            switch (d) {
                case N: return S;
                case E: if (d == E) { break; } return W;
                default: return E;
            }
        }
    )"));
    // Falling through from case to case ends in a return.
    EXPECT_FALSE(warns_missing_return(R"(
        enum Dir { N, E, S, W };
        Dir opposite(Dir d) {
            switch (d) {
                case N:
                case E: take_int(1);
                case S: take_int(2);
                default: return E;
            }
        }
    )"));
    // ...but not when the last section falls off the end.
    EXPECT_TRUE(warns_missing_return(R"(
        enum Dir { N, E, S, W };
        Dir opposite(Dir d) {
            switch (d) {
                case N: return S;
                default: take_int(0);
            }
        }
    )"));
    // A break after a return never runs, and a nested loop's break stays in the loop.
    EXPECT_FALSE(warns_missing_return(R"(
        enum Dir { N, E, S, W };
        Dir opposite(Dir d) {
            switch (d) {
                case N: return S; break;
                case E: for (;;) { break; } return W;
                default: return E;
            }
        }
    )"));
}

TEST(Language, OperationsCostBudgetLikeAnyOtherInstruction) {
    RunOptions limited;
    limited.budget = 50;
    auto r = run("while (true) { harvest(); }", {}, limited);
    EXPECT_EQ(r.result.status, RunStatus::BudgetExhausted);
    EXPECT_TRUE(r.calls.size() >= 5);
}

// =============================================================================
// Undefined behavior is caught, not executed
// =============================================================================

TEST(Language, UndefinedBehaviorStopsTheProgramWithAnExplanation) {
    EXPECT_EQ(runtime_error("int big = 2147483647; big++;").code, DiagCode::IntegerOverflow);
    EXPECT_EQ(runtime_error("int z = 0; take_int(10 / z);").code, DiagCode::DivisionByZero);
    EXPECT_EQ(runtime_error("int s = 1 << 40;").code, DiagCode::InvalidShift);

    const auto uninit = runtime_error("void f() { int hp; take_int(hp); } f();");
    EXPECT_EQ(uninit.code, DiagCode::UninitializedRead);
    EXPECT_EQ(uninit.arg_text("name"), "hp");

    const auto bounds = runtime_error("int a[3]; for (int i = 0; i <= 3; i++) a[i] = i;");
    EXPECT_EQ(bounds.code, DiagCode::OutOfBounds);
    EXPECT_EQ(bounds.arg_text("index"), "3");
    EXPECT_EQ(bounds.arg_text("size"), "3");

    EXPECT_EQ(runtime_error("int* p = 0; take_int(*p);").code, DiagCode::NullDereference);
    EXPECT_EQ(runtime_error("int f(int n) { return f(n + 1); } f(0);").code, DiagCode::StackOverflow);
    EXPECT_EQ(runtime_error("int g(int n) { if (n > 0) return 1; } take_int(g(0));").code, DiagCode::FlowOffEnd);
}

TEST(Language, GlobalsStartAtZeroLocalsDoNot) {
    EXPECT_EQ(output("int counter; counter++; take_int(counter);"), Calls{"take_int(1)"});
    EXPECT_EQ(runtime_error("void f() { int c; c++; } f();").code, DiagCode::UninitializedRead);
}

// =============================================================================
// Phase 3: functions, arrays, structs, enums
// =============================================================================

TEST(Language, FunctionsRecursionOverloadingDefaultsReferences) {
    EXPECT_EQ(output("int fact(int n) { return n <= 1 ? 1 : n * fact(n - 1); } take_int(fact(5));"),
              Calls{"take_int(120)"});
    EXPECT_TRUE(
        (output("void show(int x) { take_int(x); } void show(double x) { take_double(x); } show(1); show(2.5);") ==
         Calls{"take_int(1)", "take_double(2.5)"}));
    EXPECT_TRUE((output("int add(int a, int b = 10) { return a + b; } take_int(add(1)); take_int(add(1, 2));") ==
                 Calls{"take_int(11)", "take_int(3)"}));
    EXPECT_TRUE((output("void swap(int& a, int& b) { int t = a; a = b; b = t; } int x = 1; int y = 2; swap(x, y); "
                        "take_int(x); take_int(y);") == Calls{"take_int(2)", "take_int(1)"}));
    EXPECT_EQ(output("int twice(const int& v) { return v * 2; } take_int(twice(21));"), Calls{"take_int(42)"});
    // A prototype lets a function be called before its definition.
    EXPECT_EQ(output("int later(int); take_int(later(4)); int later(int x) { return x + 1; }"), Calls{"take_int(5)"});
}

TEST(Language, IntMainRunsAfterTheTopLevelStatements) {
    EXPECT_TRUE((output("take_int(1); int main() { take_int(2); return 0; }") == Calls{"take_int(1)", "take_int(2)"}));
}

TEST(Language, ArraysInitializersMultipleDimensionsDecay) {
    EXPECT_TRUE(
        (output("int a[5] = {1, 2}; int s = 0; for (int i = 0; i < 5; ++i) s += a[i]; take_int(s); take_int(a[4]);") ==
         Calls{"take_int(3)", "take_int(0)"}));
    EXPECT_EQ(output("int g[2][3] = {{1, 2, 3}, {4, 5, 6}}; take_int(g[1][2]);"), Calls{"take_int(6)"});
    EXPECT_EQ(output("int sum(int v[], int n) { int s = 0; for (int i = 0; i < n; i++) s += v[i]; return s; } "
                     "int d[] = {3, 4, 5}; take_int(sum(d, 3));"),
              Calls{"take_int(12)"});
    // Bounds travel with the pointer into the function.
    EXPECT_EQ(runtime_error("int sum(int v[], int n) { int s = 0; for (int i = 0; i < n; i++) s += v[i]; return s; } "
                            "int d[3] = {3, 4, 5}; take_int(sum(d, 4));")
                  .code,
              DiagCode::OutOfBounds);
}

TEST(Language, StructsAggregatesCopiesMembers) {
    const char* program = R"(
        struct Point { int x; int y; };
        struct Robot { Point at; int energy; };
        Point move_right(Point p) { p.x++; return p; }
        Robot r = {{1, 2}, 100};
        Point q = move_right(r.at);
        take_int(r.at.x); take_int(q.x); take_int(q.y);
        Robot copy = r;
        copy.energy -= 30;
        take_int(r.energy); take_int(copy.energy);
    )";
    EXPECT_TRUE(
        (output(program) == Calls{"take_int(1)", "take_int(2)", "take_int(2)", "take_int(100)", "take_int(70)"}));
}

TEST(Language, PointersAndTheHeap) {
    EXPECT_TRUE(
        (output("int x = 5; int* p = &x; *p = 7; take_int(x); int a[3] = {1, 2, 3}; int* q = a; q++; take_int(*q); "
                "take_long(&a[2] - a);") == Calls{"take_int(7)", "take_int(2)", "take_long(2)"}));
    EXPECT_TRUE((output("int* p = new int(4); take_int(*p); delete p; int* v = new int[3]; v[2] = 9; take_int(v[2]); "
                        "delete[] v;") == Calls{"take_int(4)", "take_int(9)"}));
    EXPECT_EQ(runtime_error("int* p = new int(1); delete p; delete p;").code, DiagCode::DoubleFree);
    EXPECT_EQ(runtime_error("int* p = new int(1); delete p; take_int(*p);").code, DiagCode::UseAfterFree);
    EXPECT_EQ(runtime_error("int* p = new int[2]; delete p;").code, DiagCode::InvalidDelete);

    auto leak = run("int* p = new int(1);");
    EXPECT_EQ(leak.result.status, RunStatus::Completed);
    ASSERT_EQ(leak.result.diagnostics.size(), 1);
    EXPECT_EQ(leak.result.diagnostics[0].code, DiagCode::MemoryLeak);
    EXPECT_EQ(leak.result.diagnostics[0].severity, Severity::Warning);
}

TEST(Language, EnumsScopedEnumsAndConstants) {
    EXPECT_TRUE((output("enum Crop { Wheat, Corn = 5, Rice }; Crop c = Rice; take_int(c); take_int(Corn + 1);") ==
                 Calls{"take_int(6)", "take_int(6)"}));
    EXPECT_EQ(output("enum class Season { Spring, Summer }; Season s = Season::Summer; take_bool(s == Season::Summer);",
                     standard(Standard::Cpp11)),
              Calls{"take_bool(true)"});
    EXPECT_EQ(compile_error("enum class Season { Spring }; int x = Season::Spring;", standard(Standard::Cpp11)),
              DiagCode::CannotConvert);
    EXPECT_EQ(output("const int N = 3; int a[N]; take_long(sizeof(a));"), Calls{"take_long(12)"});
}

// =============================================================================
// Phase 5: classes, inheritance, virtual functions
// =============================================================================

TEST(Language, ConstructorsAndDestructorsRunInCOrder) {
    const char* program = R"(
        struct Tracker {
            int id;
            Tracker(int i) : id(i) { take_int(id); }
            ~Tracker() { take_int(-id); }
        };
        void scope() {
            Tracker a(1);
            Tracker b(2);
            for (int i = 0; i < 2; ++i) {
                Tracker c(10 + i);
                if (i == 1) break;
            }
        }
        scope();
    )";
    EXPECT_TRUE((output(program) == Calls{"take_int(1)", "take_int(2)", "take_int(10)", "take_int(-10)", "take_int(11)",
                                          "take_int(-11)", "take_int(-2)", "take_int(-1)"}));
}

TEST(Language, ClassesMembersMethodsAccessControl) {
    const char* program = R"(
        class Counter {
        public:
            Counter() : value(0) {}
            void add(int n) { value += n; }
            int get() const { return value; }
        private:
            int value;
        };
        Counter c;
        c.add(3);
        c.add(4);
        take_int(c.get());
    )";
    EXPECT_EQ(output(program), Calls{"take_int(7)"});
    EXPECT_EQ(compile_error("class Secret { int code; }; Secret s; s.code = 1;"), DiagCode::InaccessibleMember);
    EXPECT_EQ(compile_error("struct S { int v; void set(int x) const { v = x; } };"), DiagCode::NotAssignable);
    EXPECT_EQ(compile_error("struct P { P(int) {} }; P p;"), DiagCode::NoDefaultConstructor);
}

TEST(Language, VirtualFunctionsDispatchOnTheDynamicType) {
    const char* program = R"(
        struct Animal {
            virtual ~Animal() {}
            virtual int legs() const { return 0; }
            int describe() const { return legs() * 10; }
        };
        struct Bird : Animal { int legs() const override { return 2; } };
        struct Dog : Animal { int legs() const { return 4; } };
        Animal* zoo[3];
        zoo[0] = new Bird();
        zoo[1] = new Dog();
        zoo[2] = new Animal();
        for (int i = 0; i < 3; i++) { take_int(zoo[i]->describe()); delete zoo[i]; }
        Dog d;
        Animal& a = d;
        take_int(a.legs());
    )";
    EXPECT_TRUE((output(program, standard(Standard::Cpp11)) ==
                 Calls{"take_int(20)", "take_int(40)", "take_int(0)", "take_int(4)"}));
    EXPECT_EQ(compile_error("struct Shape { virtual int area() = 0; }; Shape s;"), DiagCode::AbstractClass);
    // A concrete class whose base is abstract and has no constructors.
    const char* const concrete = R"(
        struct Shape { virtual int area() = 0; };
        struct Square : Shape { int area() override { return 9; } };
        struct A { virtual int f() = 0; };
        struct B : virtual A { int f() override { return 3; } };
        struct C : virtual A {};
        struct D : B, C {};
        Square s;
        Shape& r = s;
        take_int(r.area());
        D d;
        A& a = d;
        take_int(a.f());
    )";
    EXPECT_TRUE((output(concrete, standard(Standard::Cpp11)) == Calls{"take_int(9)", "take_int(3)"}));
    // ...with a constructor, a default member initializer, and passed by value.
    const char* const derived = R"(
        struct Plot { virtual int crop() = 0; };
        struct Wheat : Plot { int n; Wheat(int k) : n(k) {} int crop() override { return n; } };
        struct Corn : Plot { int n = 4; int crop() override { return n; } };
        int tend(Corn c) { return c.crop(); }
        Wheat w(7);
        Corn c;
        take_int(w.crop());
        take_int(tend(c));
    )";
    EXPECT_TRUE((output(derived, standard(Standard::Cpp11)) == Calls{"take_int(7)", "take_int(4)"}));
    EXPECT_EQ(compile_error("struct S { virtual int a() = 0; virtual int b() = 0; };"
                            "struct D : S { int a() { return 1; } }; D d;"),
              DiagCode::AbstractClass);
    EXPECT_EQ(
        compile_error("struct B { void f() {} }; struct D : B { void f() override {} };", standard(Standard::Cpp11)),
        DiagCode::NothingToOverride);
}

TEST(Language, MultipleInheritanceAndTheDiamondProblem) {
    EXPECT_EQ(compile_error("struct A { int a; }; struct B : A {}; struct C : A {}; struct D : B, C {}; D d; d.a = 1;"),
              DiagCode::AmbiguousName);
    const char* virtual_diamond = R"(
        struct A { int a = 1; };
        struct B : virtual A { int b = 2; };
        struct C : virtual A { int c = 3; };
        struct D : B, C { int d = 4; };
        D dd;
        dd.a = 7;
        B& b = dd;
        C& c = dd;
        take_int(b.a); take_int(c.a); take_int(dd.b + dd.c + dd.d);
    )";
    EXPECT_TRUE(
        (output(virtual_diamond, standard(Standard::Cpp11)) == Calls{"take_int(7)", "take_int(7)", "take_int(9)"}));
}

// =============================================================================
// Errors that teach
// =============================================================================

TEST(Language, CompileErrorsForCommonMistakes) {
    EXPECT_EQ(compile_error("int x = 1; int x = 2;"), DiagCode::Redefinition);
    EXPECT_EQ(compile_error("const int k = 1; k = 2;"), DiagCode::NotAssignable);
    EXPECT_EQ(compile_error("const int k;"), DiagCode::UninitializedConst);
    EXPECT_EQ(compile_error("break;"), DiagCode::MisplacedJump);
    EXPECT_EQ(compile_error("void f() { return 1; }"), DiagCode::ReturnTypeMismatch);
    EXPECT_EQ(compile_error("int a[2] = {1, 2, 3};"), DiagCode::TooManyInitializers);
    EXPECT_EQ(compile_error("struct P { int x; }; P p; p.y = 1;"), DiagCode::NoMember);
    EXPECT_EQ(compile_error("Pointt p;"), DiagCode::UnknownType);
    EXPECT_EQ(compile_error("int n = 3; int a[n];"), DiagCode::NotConstant);
    EXPECT_EQ(compile_error("int x = 1; switch (x) { case 1: break; case 1: break; }"), DiagCode::DuplicateCase);
    EXPECT_EQ(compile_error("void f(int& r) {} f(3);"), DiagCode::ReferenceNeedsLvalue);
    EXPECT_EQ(compile_error("int later(int); take_int(later(1));"), DiagCode::UndefinedFunction);
    EXPECT_EQ(compile_error("wchar_t w = 1;"), DiagCode::UnsupportedType);
    EXPECT_EQ(compile_error("static_assert(sizeof(int) == 8, \"ints are 8 bytes\");", standard(Standard::Cpp11)),
              DiagCode::StaticAssertionFailed);

    auto warned = run("int x = 1; if (x = 2) take_int(x);");
    ASSERT_TRUE(warned.compiled.ok());
    EXPECT_EQ(warned.compiled.diagnostics.front().code, DiagCode::AssignmentInCondition);

    auto missing = run("int g(int n) { if (n) return 1; }");
    ASSERT_TRUE(missing.compiled.ok());
    EXPECT_EQ(missing.compiled.diagnostics.front().code, DiagCode::MissingReturn);
}

TEST(Language, FeatureGatingAppliesToEveryStatement) {
    Options no_loops;
    no_loops.locked.add(Feature::Loops);
    auto r = run("take_int(1);\nfor (int i = 0; i < 3; i++) {}\ntake_int(2);", no_loops);
    ASSERT_EQ(r.compiled.diagnostics.size(), 1);
    EXPECT_EQ(r.compiled.diagnostics[0].code, DiagCode::FeatureLocked);
    EXPECT_EQ(r.compiled.diagnostics[0].range.begin.line, 2);

    // A rejected declaration does not cascade into "unknown name" errors.
    Options no_arrays;
    no_arrays.locked.add(Feature::Arrays);
    auto quiet = run("int a[3];\na[0] = 1;\ntake_int(a[0]);", no_arrays);
    EXPECT_EQ(quiet.compiled.diagnostics.size(), 3);
    for (const auto& d : quiet.compiled.diagnostics) {
        EXPECT_EQ(d.code, DiagCode::FeatureLocked);
    }

    EXPECT_EQ(compile_error("auto x = 1;"), DiagCode::FeatureRequiresStandard);
    EXPECT_EQ(output("auto x = 1.5; take_double(x);", standard(Standard::Cpp11)), Calls{"take_double(1.5)"});
    EXPECT_EQ(
        output("int a[3] = {1, 2, 3}; int s = 0; for (int v : a) s += v; take_int(s);", standard(Standard::Cpp11)),
        Calls{"take_int(6)"});
}
