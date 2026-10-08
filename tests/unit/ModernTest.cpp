/// @file ModernTest.cpp
/// @brief End-to-end tests of C++ beyond the procedural core: operator
/// overloading, namespaces, templates, the standard library prelude, lambdas,
/// structured bindings, and how objects are copied and destroyed.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "support/ProgramRunner.hpp"

#include <gtest/gtest.h>

#include <string>

using namespace cppi;
using cppi_test::Calls;
using cppi_test::compile_error;
using cppi_test::output;
using cppi_test::printed;
using cppi_test::run;
using cppi_test::runtime_error;
using cppi_test::standard;

namespace {

const Options cpp11 = standard(Standard::Cpp11);
const Options cpp17 = standard(Standard::Cpp17);

}  // namespace

TEST(Modern, OperatorsCanBeOverloaded) {
    const char* program = R"(
        struct Vec {
            int x, y;
            Vec(int a, int b) : x(a), y(b) {}
            Vec operator+(const Vec& o) const { return Vec(x + o.x, y + o.y); }
            Vec& operator+=(const Vec& o) { x += o.x; y += o.y; return *this; }
            bool operator==(const Vec& o) const { return x == o.x && y == o.y; }
            int& operator[](int i) { return i == 0 ? x : y; }
            Vec operator-() const { return Vec(-x, -y); }
            Vec& operator++() { ++x; return *this; }
        };
        Vec operator*(const Vec& v, int k) { return Vec(v.x * k, v.y * k); }
        struct Adder { int base; int operator()(int v) const { return base + v; } };
        Vec a(1, 2);
        Vec c = a + Vec(3, 4) * 2;
        c += Vec(1, 1);
        c[1] = c[1] + 100;
        Vec d = -c;
        ++d;
        Adder add5 = {5};
        take_int(c.x); take_int(c.y); take_int(d.x); take_bool(a + a == Vec(2, 4)); take_int(add5(10));
    )";
    EXPECT_TRUE(
        (output(program) == Calls{"take_int(8)", "take_int(111)", "take_int(-7)", "take_bool(true)", "take_int(15)"}));
}

TEST(Modern, NamespacesAndUsing) {
    const char* program = R"(
        namespace farm {
            int crops = 3;
            int twice(int x) { return x * 2; }
            namespace deep { int level = 9; }
        }
        namespace farm { int more = 4; }
        take_int(farm::twice(farm::more));
        take_int(farm::deep::level);
        using namespace farm;
        take_int(crops + more);
    )";
    EXPECT_TRUE((output(program) == Calls{"take_int(8)", "take_int(9)", "take_int(7)"}));
    EXPECT_EQ(compile_error("namespace a { int x; } take_int(x);"), DiagCode::UnknownIdentifier);
}

TEST(Modern, FunctionAndClassTemplates) {
    const char* program = R"(
        template <typename T> T biggest(T a, T b) { return a > b ? a : b; }
        template <typename T> T power(T b, int e) { return e == 0 ? 1 : b * power(b, e - 1); }
        template <class T> struct Box {
            T value;
            Box(T v) : value(v) {}
            Box<T> twice() const { return Box<T>(value + value); }
        };
        template <class T> struct Node { T v; Node<T>* next; };
        take_int(biggest(3, 7));
        take_double(biggest(2.5, 1.5));
        take_int(biggest<int>(3, 9));
        take_long(power(2L, 10));
        Box<int> b(21);
        take_int(b.twice().value);
        Node<int> n2 = {2, 0};
        Node<int> n1 = {1, &n2};
        take_int(n1.next->v);
    )";
    EXPECT_TRUE((output(program) == Calls{"take_int(7)", "take_double(2.5)", "take_int(9)", "take_long(1024)",
                                          "take_int(42)", "take_int(2)"}));
    EXPECT_EQ(compile_error("template <class T> T same(T a, T b) { return a; } same(1, 2.5);"),
              DiagCode::TemplateDeduction);
}

TEST(Modern, StdCoutPrintsText) {
    EXPECT_EQ(printed("#include <iostream>\nusing namespace std;\ncout << \"x=\" << 42 << ' ' << 2.5 << endl;"),
              "x=42 2.5\n");
    EXPECT_EQ(printed("std::cout << true << false << std::endl;"), "10\n");
}

TEST(Modern, StdVectorGrowsChecksIndicesAndFreesItsMemory) {
    const char* program = R"(
        #include <vector>
        #include <iostream>
        std::vector<int> v;
        for (int i = 5; i > 0; --i) v.push_back(i * 3);
        std::sort(v.begin(), v.end());
        for (int x : v) std::cout << x << " ";
        std::cout << v.size() << " " << v.front() << " " << v.back();
    )";
    EXPECT_EQ(printed(program, cpp11), "3 6 9 12 15 5 3 15");
    const auto bad = runtime_error("std::vector<int> v(3); take_int(v[3]);");
    EXPECT_EQ(bad.code, DiagCode::OutOfBounds);
    EXPECT_EQ(bad.arg_text("index"), "3");
    EXPECT_EQ(bad.arg_text("size"), "3");
    EXPECT_EQ(bad.range.begin.line, 1);  // reported in the player's code, not inside the library
}

TEST(Modern, StdStringBehavesLikeAValue) {
    const char* program = R"(
        #include <string>
        #include <iostream>
        using namespace std;
        string name = "Ada";
        string copy = name;
        name += " Lovelace";
        cout << name << "|" << copy << "|" << name.size() << "|" << (copy == "Ada") << "|" << to_string(-120);
        string longest = "";
        string words[3] = {"pear", "fig", "banana"};
        for (int i = 0; i < 3; ++i) if (words[i].size() > longest.size()) longest = words[i];
        cout << "|" << longest << "|" << name.substr(4, 4);
    )";
    EXPECT_EQ(printed(program), "Ada Lovelace|Ada|12|1|-120|banana|Love");
}

TEST(Modern, TemporariesAndCopiesRunConstructorsAndDestructors) {
    // A class that owns memory: every copy must allocate, every object must free.
    const char* program = R"(
        #include <string>
        #include <vector>
        std::vector<std::string> words;
        words.push_back("pear");
        words.push_back(std::string("apple"));
        std::vector<std::string> copy = words;
        copy[0] = "plum";
        std::pair<int, std::string> p = std::make_pair(3, std::string("x"));
        take_int(words[0].size());
        take_int(copy[0].size());
    )";
    auto r = run(program);
    ASSERT_TRUE(r.compiled.ok());
    EXPECT_EQ(r.result.status, RunStatus::Completed);
    EXPECT_TRUE(r.result.diagnostics.empty());  // no leaks, no double frees
    EXPECT_TRUE((r.calls == Calls{"take_int(4)", "take_int(4)"}));

    // The rule of three: a raw-pointer member copied by the implicit copy
    // constructor is freed twice.
    const char* shallow = R"(
        struct Buffer {
            int* data;
            Buffer() : data(new int[4]) {}
            ~Buffer() { delete[] data; }
        };
        void use(Buffer b) {}
        Buffer original;
        use(original);
    )";
    EXPECT_EQ(runtime_error(shallow).code, DiagCode::DoubleFree);
}

TEST(Modern, LambdasCaptureByValueAndByReference) {
    const char* program = R"(
        #include <vector>
        int total = 0;
        auto add = [](int a, int b) { return a + b; };
        auto bump = [&](int k) { total += k; };
        bump(3); bump(add(1, 3));
        take_int(total);
        void run() {
            int counter = 0;
            int step = 5;
            auto next = [&counter, step]() -> int { counter += step; return counter; };
            next(); next();
            take_int(counter);
            int m = 7;
            auto shout = [m]() mutable { m++; return m; };
            take_int(shout()); take_int(shout()); take_int(m);
        }
        run();
        std::vector<int> v;
        v.push_back(3); v.push_back(1); v.push_back(2);
        std::sort(v.begin(), v.end(), [](int a, int b) { return a > b; });
        take_int(v[0] * 100 + v[1] * 10 + v[2]);
    )";
    EXPECT_TRUE((output(program, cpp11) ==
                 Calls{"take_int(7)", "take_int(10)", "take_int(8)", "take_int(9)", "take_int(7)", "take_int(321)"}));
    EXPECT_EQ(compile_error("auto f = [](){};"), DiagCode::FeatureRequiresStandard);
    EXPECT_EQ(compile_error("void g() { int hidden = 1; auto f = []() { return hidden; }; }", cpp11),
              DiagCode::UnknownIdentifier);  // not captured
}

TEST(Modern, StructuredBindings) {
    const char* program = R"(
        struct Point { int x; int y; };
        Point p = {3, 4};
        auto [a, b] = p;
        auto& [rx, ry] = p;
        rx = 10;
        int arr[2] = {5, 6};
        auto [first, second] = arr;
        Point pts[2] = {{1, 2}, {3, 4}};
        int sum = 0;
        for (auto [px, py] : pts) sum += px * py;
        take_int(a + b); take_int(p.x); take_int(first * second); take_int(sum);
    )";
    EXPECT_TRUE((output(program, cpp17) == Calls{"take_int(7)", "take_int(10)", "take_int(30)", "take_int(14)"}));
    EXPECT_EQ(compile_error("struct P { int x; int y; }; P p; auto [a, b] = p;", cpp11),
              DiagCode::FeatureRequiresStandard);
}

TEST(Modern, TheLibraryIsFreeAndItsInternalsStayHidden) {
    // Library code costs nothing by default: a push_back costs its call.
    auto r = run("std::vector<int> v; v.push_back(1);");
    ASSERT_TRUE(r.compiled.ok());
    EXPECT_TRUE(r.result.operations < 20);

    // Programs that do not use the library carry none of its code.
    auto small = run("harvest();");
    ASSERT_TRUE(small.compiled.ok());
    EXPECT_EQ(small.compiled.program->instruction_count(), 3);
}

TEST(Modern, StdMapKeepsKeysSorted) {
    const char* program = R"(
        #include <map>
        #include <string>
        #include <iostream>
        std::map<std::string, int> stock;
        stock["wheat"] += 3;
        stock["corn"] = 5;
        stock["wheat"] += 4;
        for (auto& [crop, amount] : stock) std::cout << crop << "=" << amount << " ";
        std::cout << stock.size() << stock.count("rice");
        int nums[5] = {4, 9, 1, 7, 3};
        std::cout << " " << *std::max_element(nums, nums + 5) << *std::min_element(nums, nums + 5);
    )";
    EXPECT_EQ(printed(program, cpp17), "corn=5 wheat=7 20 91");
}

TEST(Modern, AggregatesWithLibraryMembersStillUseBraces) {
    const char* program = R"(
        #include <string>
        #include <iostream>
        struct Person { std::string name; int age; };
        Person ann = {"Ann", 31};
        Person copy = ann;
        copy.name += "ie";
        std::cout << ann.name << " " << copy.name << " " << copy.age;
    )";
    EXPECT_EQ(printed(program), "Ann Annie 31");
}

TEST(Modern, MoveSemanticsDeletedFunctionsAndSmartPointers) {
    const char* program = R"(
        #include <memory>
        #include <iostream>
        using namespace std;
        struct Robot {
            int energy;
            Robot(int e) : energy(e) { cout << "on "; }
            ~Robot() { cout << "off "; }
        };
        unique_ptr<Robot> r = make_unique<Robot>(5);
        r->energy--;
        unique_ptr<Robot> other = std::move(r);
        cout << (r ? "has " : "empty ") << (*other).energy << " ";
        shared_ptr<Robot> a = make_shared<Robot>(9);
        {
            shared_ptr<Robot> b = a;
            cout << a.use_count() << " ";
        }
        cout << a.use_count() << " ";
    )";
    auto r = cppi_test::completes(program, cpp11);
    EXPECT_EQ(r.result.output, "on empty 4 on 2 1 off off ");
    EXPECT_TRUE(r.result.diagnostics.empty());

    EXPECT_EQ(
        compile_error("#include <memory>\nstd::unique_ptr<int> a(new int(1));\nstd::unique_ptr<int> b = a;", cpp11),
        DiagCode::DeletedFunction);

    const char* moves = R"(
        struct Tracker {
            int moves = 0;
            Tracker() {}
            Tracker(const Tracker& o) : moves(o.moves) { take_int(1); }
            Tracker(Tracker&& o) : moves(o.moves + 1) { take_int(2); }
        };
        Tracker make() { Tracker t; return t; }
        Tracker a;
        Tracker b = a;             // copy
        Tracker c = std::move(a);  // move
        Tracker d = make();        // the local is moved out; the result is built in place
        take_int(d.moves);
    )";
    EXPECT_TRUE((output(moves, cpp11) == Calls{"take_int(1)", "take_int(2)", "take_int(2)", "take_int(1)"}));
}

TEST(Modern, StdOptionalAndConversionOperators) {
    const char* program = R"(
        #include <optional>
        #include <iostream>
        struct Meters {
            double value;
            operator double() const { return value; }
        };
        std::optional<int> found;
        std::optional<int> five = 5;
        std::cout << found.has_value() << five.value() << five.value_or(1) << found.value_or(7);
        if (five) std::cout << " yes";
        Meters m = {2.5};
        double twice = m * 2;
        std::cout << " " << twice;
    )";
    EXPECT_EQ(printed(program, cpp17), "0557 yes 5");
    // value() throws std::bad_optional_access; `*none` is undefined behavior.
    EXPECT_EQ(runtime_error("#include <optional>\nstd::optional<int> none;\ntake_int(none.value());", cpp17).code,
              DiagCode::UncaughtException);
    EXPECT_EQ(runtime_error("#include <optional>\nstd::optional<int> none;\ntake_int(*none);", cpp17).code,
              DiagCode::BadAccess);
}

TEST(Modern, DirectInitializationWithNamesIsAnObjectNotAFunction) {
    // tree-sitter reads these as prototypes; a "parameter" that cannot be a
    // type (a variable, an enumerator, an expression, a literal) makes them objects.
    const char* globals = R"(
        #include <vector>
        #include <iostream>
        const int kWidth = 3;
        const int kHeight = 2;
        std::vector<bool> seen(kWidth * kHeight, false);
        std::vector<Direction> dirs(kWidth, North);
        std::vector<int> masks(kWidth & kHeight, kWidth);
        std::cout << seen.size() << seen[0] << " " << dirs.size() << (dirs[2] == North) << " " << masks.size()
                  << masks[1] << std::endl;
    )";
    EXPECT_EQ(printed(globals), "60 31 23\n");
    const char* locals = R"(
        #include <vector>
        #include <iostream>
        int main() {
            const int kTiles = 4;
            int sizes[2] = {2, 5};
            bool flag = true;
            std::vector<bool> a(kTiles, false);
            std::vector<bool> b(kTiles, true);
            std::vector<Direction> c(kTiles, West);
            std::vector<int> d(sizes[1], kTiles);
            std::vector<bool> e(kTiles && flag, flag);
            std::vector<Direction> f(sizes[0], Direction::East);
            std::cout << a.size() << a[3] << " " << b[0] << " " << (c[3] == West) << " " << d.size() << d[4] << " "
                      << e.size() << e[0] << " " << f.size() << (f[1] == East) << std::endl;
            return 0;
        }
    )";
    EXPECT_EQ(printed(locals, cpp11), "40 1 1 54 11 21\n");
}

TEST(Modern, PrototypesThatOnlyNameTypesStayFunctions) {
    // `int f(int, Direction);` declares a function, at the top level...
    EXPECT_TRUE((output(R"(
        int f(int, Direction);
        int f(int n, Direction d) { return n + (d == East ? 10 : 0); }
        take_int(f(1, East));
    )") == Calls{"take_int(11)"}));
    // ...and inside a block, where cppi does not allow it: it is still not a variable.
    EXPECT_EQ(compile_error("int main() { void g(int x); return 0; }"), DiagCode::DeclarationNotAllowed);
    EXPECT_EQ(compile_error("int main() { int n = 1; void g(Direction); return n; }"), DiagCode::DeclarationNotAllowed);
    // `T x(U);` with U a type is a function returning T, as in C++ (the most vexing parse).
    EXPECT_TRUE((output(R"(
        struct U {};
        struct T { int v; T() : v(7) {} T(U) : v(3) {} };
        T x(U);
        T x(U) { T t; return t; }
        take_int(x(U()).v);
    )") == Calls{"take_int(7)"}));
}

TEST(Modern, VectorsOfAggregatesWithLibraryMembers) {
    // `new R[n]` (vector::reserve) runs the implicit default constructor, so
    // the std::string member exists before the elements are assigned.
    const char* program = R"(
        #include <vector>
        #include <string>
        #include <iostream>
        #include <algorithm>
        struct R { std::string n; int s; };
        std::vector<R> v;
        R r;
        r.n = "a";
        r.s = 1;
        v.push_back(r);
        v.push_back(r);
        for (int i = 0; i < 9; ++i) {  // grows past 4 and 8
            r.n += "b";
            r.s = 10 - i;
            v.push_back(r);
        }
        std::cout << v.size() << " " << v[10].n << " " << v[10].s << std::endl;
        R first;
        first.n = "z";
        first.s = 99;
        v.insert(v.begin(), first);
        v.erase(v.begin() + 1);
        std::cout << v.size() << " " << v[0].n << v[0].s << " " << v[1].n << v[1].s << std::endl;
        std::vector<R> copy = v;
        std::vector<R> assigned;
        assigned = v;
        copy[0].n = "changed";
        std::cout << copy.size() << assigned.size() << " " << v[0].n << " " << assigned[0].n << std::endl;
        std::sort(v.begin(), v.end(), [](const R& a, const R& b) { return a.s < b.s; });
        for (const R& x : v) std::cout << x.s << ",";
        std::cout << " " << v[0].n << std::endl;
    )";
    EXPECT_EQ(printed(program, cpp11),
              "11 abbbbbbbbb 2\n"
              "11 z99 a1\n"
              "1111 z z\n"
              "1,2,3,4,5,6,7,8,9,10,99, a\n");
    // Plain `new` too, with default member initializers.
    EXPECT_EQ(printed(R"(
        struct R { std::string n; int s = 4; };
        R* one = new R;
        R* many = new R[3];
        many[2].n = "x";
        std::cout << one->n.size() << one->s << many[1].s << many[2].n;
        delete one;
        delete[] many;
    )",
                      cpp11),
              "044x");
}

TEST(Modern, ErrorsInsideImplicitMemberFunctionsPointAtThePlayersLine) {
    // Implicit copies and assignments have no lines of their own: whatever
    // fails inside them (here, the call stack) is reported at the call.
    const char* program =
        "struct R { std::string n; int s; };\n"
        "std::vector<R> v;\n"
        "R r;\n"
        "r.s = 1;\n"
        "v.push_back(r);\n";
    for (std::uint32_t depth = 1; depth <= 8; ++depth) {
        RunOptions limits;
        limits.max_call_depth = depth;
        cppi_test::RecordingHost host;
        Interpreter interpreter(host.make_registry());
        auto compiled = interpreter.compile(program);
        ASSERT_TRUE(compiled.ok());
        auto result = interpreter.run(*compiled.program, limits);
        if (result.status == RunStatus::Completed) {
            continue;
        }
        ASSERT_EQ(result.diagnostics.size(), 1U);
        EXPECT_EQ(result.diagnostics[0].code, DiagCode::StackOverflow);
        EXPECT_GE(result.diagnostics[0].range.begin.line, 2) << "depth " << depth;
    }
}

TEST(Modern, AClassWithoutTheOperatorSaysSo) {
    const auto errors = [](const char* source) {
        auto r = run(source, cpp17);
        EXPECT_FALSE(r.compiled.ok()) << source;
        return r.compiled.diagnostics;
    };
    // std::sort needs operator<, which cppi's std::pair does not have: the
    // error names the operator and the type, at the player's call, and a note
    // says which library function needed it.
    auto pairs = errors(R"(#include <vector>
#include <algorithm>
std::vector<std::pair<int, int>> v;
v.push_back(std::make_pair(2, 1));
std::sort(v.begin(), v.end());
)");
    ASSERT_EQ(pairs.size(), 2U);
    EXPECT_EQ(pairs[0].code, DiagCode::NoOperator);
    EXPECT_EQ(pairs[0].arg_text("op"), "<");
    EXPECT_EQ(pairs[0].arg_text("type"), "pair<int, int>");
    EXPECT_EQ(pairs[0].range.begin.line, 5);
    EXPECT_EQ(pairs[1].code, DiagCode::InstantiatedFrom);
    EXPECT_EQ(pairs[1].severity, Severity::Note);
    EXPECT_EQ(pairs[1].arg_text("function"), "std::sort<pair<int, int>>");
    EXPECT_EQ(pairs[1].range, pairs[0].range);

    // A struct of the player's, in the library and in the player's own code.
    auto own = errors(R"(struct Order { int x; int y; };
std::vector<Order> orders;
std::sort(orders.begin(), orders.end());
)");
    ASSERT_EQ(own.size(), 2U);
    EXPECT_EQ(own[0].code, DiagCode::NoOperator);
    EXPECT_EQ(own[0].arg_text("type"), "Order");
    EXPECT_EQ(own[0].range.begin.line, 3);
    auto direct = errors("struct Order { int x; };\nOrder a;\nOrder b;\nbool same = a == b;\n");
    ASSERT_EQ(direct.size(), 1U);  // no note outside the library
    EXPECT_EQ(direct[0].code, DiagCode::NoOperator);
    EXPECT_EQ(direct[0].arg_text("op"), "==");
    EXPECT_EQ(direct[0].range.begin.line, 4);

    // With a comparator, or an operator< of its own, it sorts.
    EXPECT_EQ(printed(R"(
        std::vector<std::pair<int, int>> v;
        v.push_back(std::make_pair(2, 1));
        v.push_back(std::make_pair(1, 5));
        std::sort(v.begin(), v.end(), [](const std::pair<int, int>& a, const std::pair<int, int>& b) {
            return a.first < b.first;
        });
        struct Order {
            int x;
            bool operator<(const Order& other) const { return x < other.x; }
        };
        std::vector<Order> orders;
        Order o;
        o.x = 3;
        orders.push_back(o);
        o.x = 1;
        orders.push_back(o);
        std::sort(orders.begin(), orders.end());
        std::cout << v[0].first << v[1].first << " " << orders[0].x << orders[1].x;
    )",
                      cpp17),
              "12 13");
}

TEST(Modern, NoOperatorOnlyWhenTheOperatorIsMissing) {
    // A free postfix operator++ takes a dummy int: it is the operator c++ uses.
    EXPECT_EQ(printed(R"(
        struct C { int v; };
        C operator++(C& c, int) { C old = c; c.v++; return old; }
        C c;
        c.v = 1;
        C before = c++;
        std::cout << before.v << c.v;
    )"),
              "12");
    // Pointer arithmetic with a class is wrong operands, not a missing operator+.
    EXPECT_EQ(compile_error("struct P { int x; };\nP p;\nint* ptr = 0;\nptr + p;"), DiagCode::InvalidOperands);
    // A member that cannot be default-constructed by `new` is reported at the `new`.
    auto r = run("struct B { B(int) {} };\nstruct A { B b; };\nA* p = new A;\n");
    ASSERT_FALSE(r.compiled.ok());
    EXPECT_EQ(r.compiled.diagnostics[0].code, DiagCode::NoDefaultConstructor);
    EXPECT_EQ(r.compiled.diagnostics[0].range.begin.line, 3);
}
