/// @file CompletenessTest.cpp
/// @brief End-to-end tests of the parts of C++ added last: every integer type,
/// delegating constructors, static members, constexpr functions, member
/// templates, exceptions, std::variant, concepts and modules.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "support/ProgramRunner.hpp"

#include <gtest/gtest.h>

#include <string>

using namespace cppi;
using cppi_test::compile_error;
using cppi_test::printed;
using cppi_test::run;
using cppi_test::runtime_error;
using cppi_test::standard;

namespace {

const Options cpp11 = standard(Standard::Cpp11);
const Options cpp17 = standard(Standard::Cpp17);
const Options cpp20 = standard(Standard::Cpp20);

}  // namespace

TEST(Completeness, UnsignedIntegersWrapAround) {
    EXPECT_EQ(printed(R"(
        unsigned int u = 0;
        u = u - 1;
        std::cout << u << std::endl;
        unsigned long big = 18446744073709551615u;
        std::cout << big << " " << big + 1 << std::endl;
        unsigned x = 3000000000u;
        std::cout << x * 2 << std::endl;
    )"),
              "4294967295\n18446744073709551615 0\n1705032704\n");
}

TEST(Completeness, SignedAndUnsignedMixLikeInC) {
    // -1 converts to unsigned in the comparison: the classic surprise.
    EXPECT_EQ(printed(R"(
        int i = -1;
        unsigned int u = 1;
        std::cout << (i < u) << " " << (i < static_cast<int>(u)) << std::endl;
        long l = -1;
        std::cout << (l < u) << std::endl;
        std::cout << -1u << " " << 7u / 2 << " " << 7u % 4 << std::endl;
        std::cout << (0xFFFFFFFFu >> 28) << " " << (~0u) << std::endl;
    )"),
              "0 1\n1\n4294967295 3 3\n15 4294967295\n");
}

TEST(Completeness, ShortAndCharTypesArePromotedAndWrapOnStore) {
    EXPECT_EQ(printed(R"(
        short s = 32767;
        s++;
        std::cout << s << std::endl;
        unsigned short us = 65535;
        us += 2;
        std::cout << us << std::endl;
        unsigned char c = 250;
        c = c + 10;
        std::cout << static_cast<int>(c) << std::endl;
        signed char sc = -5;
        std::cout << sc + 0 << " " << sizeof(short) << " " << sizeof(unsigned long long) << std::endl;
        short a = 200, b = 300;
        std::cout << a * b << std::endl;
    )"),
              "-32768\n1\n4\n-5 2 8\n60000\n");
}

TEST(Completeness, IntegerLiteralTypesFollowTheStandard) {
    EXPECT_EQ(printed(R"(
        auto a = 0xFFFFFFFF;
        auto b = 4294967295;
        auto c = 1u;
        auto d = 1ul;
        std::cout << sizeof(a) << sizeof(b) << sizeof(c) << sizeof(d) << std::endl;
        std::cout << a + 1 << " " << b + 1 << std::endl;
    )",
                      cpp11),
              "4848\n0 4294967296\n");
    EXPECT_EQ(compile_error("long x = 99999999999999999999;"), DiagCode::IntegerOutOfRange);
}

TEST(Completeness, UnsignedDivisionByZeroAndBadShiftsAreStillUndefined) {
    EXPECT_EQ(runtime_error("unsigned a = 1; unsigned b = 0; unsigned c = a / b;").code, DiagCode::DivisionByZero);
    EXPECT_EQ(runtime_error("unsigned a = 1; int n = 32; unsigned c = a << n;").code, DiagCode::InvalidShift);
    EXPECT_EQ(runtime_error("double d = -5.0; unsigned u = d;").code, DiagCode::IntegerOverflow);
}

TEST(Completeness, OverloadsDistinguishIntegerTypes) {
    EXPECT_EQ(printed(R"(
        void f(int) { std::cout << "int "; }
        void f(unsigned int) { std::cout << "unsigned "; }
        void f(long) { std::cout << "long "; }
        short s = 1;
        unsigned char uc = 1;
        f(s); f(uc); f(1u); f(1L);
        std::cout << std::endl;
    )"),
              "int int unsigned long \n");
}

TEST(Completeness, ConstructorsCanDelegateToAnotherConstructor) {
    EXPECT_EQ(printed(R"(
        struct Point {
            int x, y;
            Point(int a, int b) : x(a), y(b) { std::cout << "full "; }
            Point() : Point(1, 2) { std::cout << "default "; }
            explicit Point(int v) : Point(v, v) {}
        };
        Point p;
        Point q(5);
        std::cout << p.x << p.y << q.x << q.y << std::endl;
    )",
                      cpp11),
              "full default full 1255\n");
    EXPECT_EQ(compile_error("struct A { int x; A(int v) : x(v) {} A() : A(0) {} };"),
              DiagCode::FeatureRequiresStandard);
    EXPECT_EQ(compile_error("struct A { int x; A(int v) : x(v) {} A() : A(0), x(1) {} };", cpp11),
              DiagCode::UnsupportedSyntax);
}

TEST(Completeness, StaticDataMembersAreSharedByEveryObject) {
    EXPECT_EQ(printed(R"(
        class Counter {
        public:
            Counter() { ++count; ++total; }
            ~Counter() { --count; }
            static int count;
            static int created() { return total; }
        private:
            static int total;
        };
        int Counter::count = 0;
        int Counter::total;
        {
            Counter a, b;
            std::cout << Counter::count << " " << a.count << std::endl;
        }
        Counter c;
        std::cout << Counter::count << " " << Counter::created() << " " << c.created() << std::endl;
    )"),
              "2 2\n1 3 3\n");
}

TEST(Completeness, StaticConstantsCanSizeArrays) {
    EXPECT_EQ(printed(R"(
        struct Grid {
            static const int size = 3;
            int cells[size];
        };
        struct Limits { static const int max; };
        const int Limits::max = 7;
        int other[Limits::max];
        Grid g;
        std::cout << sizeof(g.cells) / sizeof(int) << " " << sizeof(other) / sizeof(int) << std::endl;
    )"),
              "3 7\n");
    EXPECT_EQ(printed(R"(
        struct Config { inline static int level = 4; static constexpr double scale = 1.5; };
        Config::level++;
        std::cout << Config::level * Config::scale << std::endl;
    )",
                      cpp17),
              "7.5\n");
}

TEST(Completeness, StaticMemberFunctionsHaveNoThis) {
    EXPECT_EQ(printed(R"(
        struct Math {
            static int square(int x) { return x * x; }
            static int twice_square(int x) { return 2 * square(x); }
            int value = 3;
            int cube() const { return value * square(value); }
        };
        Math m;
        std::cout << Math::square(4) << " " << Math::twice_square(3) << " " << m.cube() << std::endl;
    )",
                      cpp11),
              "16 18 27\n");
    EXPECT_EQ(compile_error("struct A { int x; static int f() { return x; } };"), DiagCode::DeclarationNotAllowed);
    EXPECT_EQ(compile_error("struct A { int f(); static int g() { return f(); } };"), DiagCode::DeclarationNotAllowed);
}

TEST(Completeness, StaticMembersFollowCRules) {
    EXPECT_EQ(compile_error("class A { static int secret; }; int A::secret = 1; int x = A::secret;"),
              DiagCode::InaccessibleMember);
    EXPECT_EQ(compile_error("struct A { static int n = 1; };"), DiagCode::DeclarationNotAllowed);
    EXPECT_EQ(compile_error("struct A { static int n; }; int A::n = 1; int A::n = 2;"), DiagCode::Redefinition);
    EXPECT_EQ(compile_error("struct A { static int n; }; int A::m = 1;"), DiagCode::UnknownIdentifier);
    EXPECT_EQ(compile_error("struct P { P(int) {} }; struct A { static P p; };"), DiagCode::UndefinedFunction);
    EXPECT_EQ(compile_error("void f() { struct L { static int n; }; }"), DiagCode::DeclarationNotAllowed);
}

TEST(Completeness, StaticObjectsAreBuiltByTheirDefinitionAndDestroyedAtTheEnd) {
    EXPECT_EQ(printed(R"(
        struct Clock {
            int fps;
            Clock(int f) : fps(f) { std::cout << "start "; }
            ~Clock() { std::cout << "stop" << std::endl; }
        };
        struct Game { static Clock clock; };
        Clock Game::clock(60);
        std::cout << Game::clock.fps << " ";
    )"),
              "start 60 stop\n");
}

TEST(Completeness, ConstexprFunctionsRunAtCompileTime) {
    EXPECT_EQ(printed(R"(
        constexpr int square(int x) { return x * x; }
        constexpr int factorial(int n) { return n <= 1 ? 1 : n * factorial(n - 1); }
        constexpr int sum_to(int n) {
            int total = 0;
            for (int i = 1; i <= n; ++i) {
                if (i % 2 == 0) continue;
                total += i;
            }
            return total;
        }
        constexpr int fib(int n) {
            int a = 0, b = 1;
            while (n-- > 0) { int t = a + b; a = b; b = t; }
            return a;
        }
        int grid[square(3)];
        static_assert(factorial(5) == 120, "5! is 120");
        constexpr int odd = sum_to(9);
        int x = 2;
        switch (x) { case square(1) + 1: std::cout << "case "; break; default: break; }
        std::cout << sizeof(grid) / sizeof(int) << " " << odd << " " << fib(10) << " " << square(x) << std::endl;
    )",
                      cpp11),
              "case 9 25 55 4\n");
}

TEST(Completeness, ConstexprEvaluationStopsAtWhatIsNotConstant) {
    // Not constexpr, or not constant for these arguments: an array size needs a constant.
    EXPECT_EQ(compile_error("int f(int x) { return x; } int a[f(2)];"), DiagCode::NotConstant);
    EXPECT_EQ(compile_error("constexpr int f(int x) { return 10 / x; } int a[f(0)];", cpp11), DiagCode::NotConstant);
    EXPECT_EQ(compile_error("constexpr int loop() { while (true) {} return 1; } int a[loop()];", cpp11),
              DiagCode::NotConstant);
    EXPECT_EQ(compile_error("constexpr int deep(int n) { return deep(n + 1); } int a[deep(0)];", cpp11),
              DiagCode::NotConstant);
    // At run time a constexpr function is an ordinary function.
    EXPECT_EQ(
        printed("constexpr int twice(int x) { return 2 * x; } int n = 21; std::cout << twice(n) << std::endl;", cpp11),
        "42\n");
}

TEST(Completeness, MemberFunctionsCanBeTemplates) {
    EXPECT_EQ(printed(R"(
        struct Printer {
            int prefix = 0;
            template <class T>
            void show(T value) const { std::cout << prefix << ":" << value << " "; }
            template <class T>
            T twice(T value) { return value + value; }
            template <class To, class From>
            static To convert(From value) { return static_cast<To>(value); }
            template <class T>
            T clamp_to(T value, T limit);
            void all() { show(twice(2)); show<double>(1); }
        };
        template <class U>
        U Printer::clamp_to(U value, U limit) { return value > limit ? limit : value; }
        Printer p;
        p.prefix = 7;
        p.show(1);
        p.show(2.5);
        p.show('c');
        std::cout << p.twice(21) << " " << p.twice(1.25) << " " << Printer::convert<int>(3.9) << " ";
        std::cout << p.clamp_to(15, 10) << std::endl;
        p.all();
        std::cout << std::endl;
    )",
                      cpp11),
              "7:1 7:2.5 7:c 42 2.5 3 10\n7:4 7:1 \n");
}

TEST(Completeness, MemberTemplatesOfClassTemplates) {
    EXPECT_EQ(printed(R"(
        template <class T>
        struct Box {
            T value;
            template <class F>
            Box<T> map(F f) const { Box<T> out; out.value = f(value); return out; }
        };
        Box<int> b;
        b.value = 5;
        auto doubled = b.map([](int x) { return x * 2; });
        std::cout << doubled.value << std::endl;
    )",
                      cpp11),
              "10\n");
    EXPECT_EQ(compile_error("struct A { template <class T> void f(T) {} }; A a; a.f<int, int>(1);", cpp11),
              DiagCode::TemplateDeduction);
}

TEST(Completeness, ExceptionsUnwindTheStackAndRunDestructors) {
    EXPECT_EQ(printed(R"(
        struct Guard {
            int id;
            Guard(int i) : id(i) {}
            ~Guard() { std::cout << "~" << id << " "; }
        };
        int risky(int n) {
            Guard g(n);
            if (n > 2) throw n * 10;
            return n;
        }
        try {
            Guard outer(0);
            risky(1);
            risky(3);
            std::cout << "never ";
        } catch (int e) {
            std::cout << "caught " << e << std::endl;
        }
        std::cout << "after" << std::endl;
    )"),
              "~1 ~3 ~0 caught 30\nafter\n");
}

TEST(Completeness, CatchClausesMatchByTypeBaseClassOrAnything) {
    EXPECT_EQ(printed(R"(
        struct Shape { virtual ~Shape() {} virtual const char* name() const { return "shape"; } };
        struct Circle : Shape { const char* name() const override { return "circle"; } };
        void test(int which) {
            try {
                if (which == 0) throw 1.5;
                if (which == 1) throw Circle();
                if (which == 2) throw "text";
                throw 'x';
            } catch (int) {
                std::cout << "int ";
            } catch (double d) {
                std::cout << "double " << d << " ";
            } catch (const Shape& s) {
                std::cout << s.name() << " ";
            } catch (const char* message) {
                std::cout << message << " ";
            } catch (...) {
                std::cout << "something ";
            }
        }
        for (int i = 0; i < 4; ++i) test(i);
        std::cout << std::endl;
    )",
                      cpp11),
              "double 1.5 circle text something \n");
}

TEST(Completeness, ExceptionsCanBeRethrownAndNested) {
    EXPECT_EQ(printed(R"(
        int depth = 0;
        void inner() {
            try {
                throw 42;
            } catch (int& e) {
                e = e + 1;
                std::cout << "inner ";
                throw;
            }
        }
        try {
            inner();
        } catch (int e) {
            std::cout << "outer " << e << std::endl;
        }
        try {
            try { throw 1; } catch (int) { throw 2; }
        } catch (int e) {
            std::cout << "replaced by " << e << std::endl;
        }
    )"),
              "inner outer 43\nreplaced by 2\n");
}

TEST(Completeness, TheStandardExceptions) {
    EXPECT_EQ(printed(R"(
        std::vector<int> v(3);
        try {
            v.at(5) = 1;
        } catch (const std::out_of_range& e) {
            std::cout << "out of range: " << e.what() << std::endl;
        }
        try {
            throw std::runtime_error(std::string("disk ") + "full");
        } catch (const std::exception& e) {
            std::cout << e.what() << std::endl;
        }
        std::optional<int> none;
        try {
            none.value();
        } catch (const std::bad_optional_access& e) {
            std::cout << e.what() << std::endl;
        }
        struct MyError : std::logic_error {
            MyError() : std::logic_error("mine") {}
        };
        try { throw MyError(); } catch (const std::logic_error& e) { std::cout << e.what() << std::endl; }
    )",
                      cpp17),
              "out of range: vector::at: index out of range\ndisk full\nbad optional access\nmine\n");
}

TEST(Completeness, UncaughtExceptionsStopTheProgramAndSayWhatHappened) {
    auto d = runtime_error(R"(
        void check(int hp) {
            if (hp < 0) throw std::invalid_argument("negative health");
        }
        check(5);
        check(-1);
    )");
    EXPECT_EQ(d.code, DiagCode::UncaughtException);
    EXPECT_EQ(d.range.begin.line, 3);
    EXPECT_NE(cppi::to_debug_string(d).find("std::invalid_argument"), std::string::npos);
    EXPECT_NE(cppi::to_debug_string(d).find("negative health"), std::string::npos);
    EXPECT_EQ(runtime_error("throw 7;").code, DiagCode::UncaughtException);
    EXPECT_EQ(runtime_error("throw;").code, DiagCode::NoActiveException);
    EXPECT_EQ(runtime_error(R"(
        struct Bad { ~Bad() { throw 2; } };
        try { Bad b; throw 1; } catch (...) {}
    )")
                  .code,
              DiagCode::ExceptionDuringUnwind);
}

TEST(Completeness, ExceptionObjectsAreFreed) {
    // printed() checks that nothing leaks.
    EXPECT_EQ(printed(R"(
        struct Message { std::string text; };
        for (int i = 0; i < 3; ++i) {
            try {
                Message m;
                m.text = "boom";
                throw m;
            } catch (Message& caught) {
                std::cout << caught.text << " ";
            }
        }
        try {
            try { throw std::runtime_error("again"); } catch (...) { throw; }
        } catch (const std::exception& e) {
            std::cout << e.what() << std::endl;
        }
    )"),
              "boom boom boom again\n");
}

TEST(Completeness, ExceptionsAreAC98FeatureThatCanBeLocked) {
    Options locked;
    locked.locked.add(Feature::Exceptions);
    EXPECT_EQ(compile_error("try { throw 1; } catch (int) {}", locked), DiagCode::FeatureLocked);
}

TEST(Completeness, TemplateParametersCanHaveDefaults) {
    EXPECT_EQ(printed(R"(
        template <class T, class U = T>
        struct Pair { T first; U second; };
        template <class T = int>
        T zero() { return T(); }
        Pair<double> p;
        p.second = 2.5;
        Pair<int, char> q;
        q.second = 'z';
        std::cout << p.second << q.second << zero() << zero<double>() << std::endl;
    )"),
              "2.5z00\n");
}

TEST(Completeness, IfConstexprDiscardsTheOtherBranch) {
    EXPECT_EQ(printed(R"(
        template <class T>
        void describe(T value) {
            if constexpr (__cppi_is_integral<T>()) {
                std::cout << "integer " << value % 2 << " ";
            } else {
                std::cout << "other " << value << " ";
            }
        }
        describe(7);
        describe(2.5);
        std::cout << std::endl;
    )",
                      cpp17),
              "integer 1 other 2.5 \n");
}

TEST(Completeness, GenericLambdasAndDeducedReturnTypes) {
    EXPECT_EQ(printed(R"(
        auto twice = [](auto x) { return x + x; };
        int base = 10;
        auto add = [base](const auto& a, auto b) { return a + b + base; };
        auto half(double x) { return x / 2; }
        std::vector<int> v;
        v.push_back(1);
        v.push_back(3);
        v.push_back(2);
        std::sort(v.begin(), v.end(), [](auto a, auto b) { return a > b; });
        std::cout << twice(2) << " " << twice(1.25) << " " << add(1, 2) << " " << half(5) << " " << v[0] << std::endl;
    )",
                      standard(Standard::Cpp14)),
              "4 2.5 13 2.5 3\n");
}

TEST(Completeness, StdVariantHoldsOneOfSeveralTypes) {
    EXPECT_EQ(printed(R"(
        #include <variant>
        std::variant<int, double, std::string> value = 3;
        std::cout << value.index() << std::holds_alternative<int>(value) << std::get<int>(value) << " ";
        value = 2.5;
        std::cout << std::get<double>(value) << " ";
        value = std::string("text");
        std::visit([](const auto& x) { std::cout << "[" << x << "] "; }, value);
        std::cout << (std::get_if<int>(&value) == nullptr) << " ";
        try {
            std::get<int>(value);
        } catch (const std::bad_variant_access& e) {
            std::cout << e.what();
        }
        std::variant<int, char> small;
        std::variant<int, char> copy = small;
        std::cout << " " << std::get<int>(copy) << std::endl;
    )",
                      cpp17),
              "013 2.5 [text] 1 bad variant access 0\n");
}

TEST(Completeness, NullptrAndNULL) {
    EXPECT_EQ(printed(R"(
        int* p = nullptr;
        int* q = NULL;
        std::cout << (p == nullptr) << (q == 0) << std::endl;
    )",
                      cpp11),
              "11\n");
}

TEST(Completeness, ConceptsConstrainTemplates) {
    EXPECT_EQ(printed(R"(
        template <class T>
        concept Number = std::integral<T> || std::floating_point<T>;
        template <class T>
        concept Sized = requires(const T& c) {
            c.size();
            { c.size() } -> std::convertible_to<long>;
        };
        template <Number T>
        T twice(T x) { return x + x; }
        template <class T>
            requires Sized<T>
        int length_of(const T& c) { return c.size(); }
        template <class T>
        T half(T x) requires std::floating_point<T> { return x / 2; }
        template <class T>
        int describe(T) { return 0; }
        template <std::integral T>
        int describe(T) { return 1; }
        static_assert(Number<int> && !Number<std::string>);
        static_assert(Sized<std::vector<int>> && !Sized<int>);
        static_assert(std::same_as<int, int> && std::derived_from<std::out_of_range, std::exception>);
        std::vector<int> v(3);
        std::cout << twice(2) << " " << twice(1.5) << " " << length_of(v) << " " << half(3.0) << " ";
        std::cout << describe(1) << describe(1.5) << std::endl;
    )",
                      cpp20),
              "4 3 3 1.5 10\n");
}

TEST(Completeness, UnsatisfiedConstraintsAreExplained) {
    const char* number = "template <class T> concept Number = std::integral<T> || std::floating_point<T>;\n";
    EXPECT_EQ(compile_error(std::string(number) + "template <Number T> T twice(T x) { return x + x; }\n"
                                                  "std::string s = twice(std::string(\"a\"));",
                            cpp20),
              DiagCode::ConstraintsNotSatisfied);
    EXPECT_EQ(
        compile_error(std::string(number) + "template <Number T> struct Box { T value; };\nBox<std::string> b;", cpp20),
        DiagCode::ConstraintsNotSatisfied);
    EXPECT_EQ(compile_error("template <class T> requires (sizeof(T) > 100) void big(T) {}\nbig(1);", cpp20),
              DiagCode::ConstraintsNotSatisfied);
    EXPECT_EQ(compile_error("template <class T> concept C = true;\nint C = 1;", cpp20), DiagCode::Redefinition);
    EXPECT_EQ(compile_error("template <class T> concept C = true;", standard(Standard::Cpp17)),
              DiagCode::FeatureRequiresStandard);
    // A requires-expression is only true or false: errors inside are not reported.
    EXPECT_EQ(printed("bool ok = requires(int x) { x.size(); };\nstd::cout << ok << std::endl;", cpp20), "0\n");
}

TEST(Completeness, AbbreviatedFunctionTemplates) {
    EXPECT_EQ(printed(R"(
        void show(auto x) { std::cout << x << " "; }
        int next(std::integral auto x) { return x + 1; }
        auto scaled = [](std::floating_point auto x) { return x * 2; };
        show(1);
        show(2.5);
        std::cout << next(4) << " " << scaled(1.25) << std::endl;
    )",
                      cpp20),
              "1 2.5 5 2.5\n");
    EXPECT_EQ(compile_error("int next(std::integral auto x) { return x + 1; }\nint n = next(1.5);", cpp20),
              DiagCode::ConstraintsNotSatisfied);
    EXPECT_EQ(compile_error("void show(auto x) {}", standard(Standard::Cpp17)), DiagCode::FeatureRequiresStandard);
}
