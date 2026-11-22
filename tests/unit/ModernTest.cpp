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

TEST(Modern, DefaultCapturesFindTheVariablesUsedAnywhereInTheBody) {
    // Each local is used in one kind of statement or expression only, so
    // missing any of them leaves it uncaptured (an unknown identifier).
    const char* program = R"(
        #include <vector>
        struct Point { int x; int y; };
        void run() {
            int a = 1, b = 2, c = 3, d = 4, e = 5, f = 6, g = 7, h = 8, k = 9, n = 3;
            int arr[3] = {10, 20, 30};
            Point p = {4, 5};
            std::vector<int> v;
            v.push_back(1); v.push_back(2); v.push_back(3);
            int* q = &a;
            auto by_value = [=]() {
                int sum = -a + (b > 0 ? c : d);
                if (int t = e) { sum += t; } else { sum -= 1; }
                int i = 0;
                while (i < n) { ++i; }
                do { sum += v[1]; } while (false);
                for (int j = 0; j < f; j++) { sum += 1; }
                for (int x : v) { sum += x; }
                switch (g) { case 7: sum += p.x; break; default: break; }
                try { if (h < 0) throw h; } catch (int) { sum = 0; }
                sum += static_cast<int>(sizeof(k)) + *q;
                int* heap = new int(k);
                sum += *heap;
                delete heap;
                auto inner = [&, i]() { return i + sum; };
                return inner();
            };
            int total = 0;
            auto by_reference = [&]() { for (int x : v) { total += x * k; } total += arr[1]; };
            by_reference();
            take_int(by_value());
            take_int(total);
        }
        run();
    )";
    EXPECT_TRUE((output(program, cpp17) == Calls{"take_int(42)", "take_int(74)"}));
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
    // std::sort needs operator<; std::pair's compares the members, so a pair
    // of a struct without < cannot be sorted: the error names the operator and
    // the member's type, at the player's call, once, and a note says which
    // library function needed it.
    auto pairs = errors(R"(#include <vector>
#include <algorithm>
struct P { int x; };
std::vector<std::pair<P, int>> v;
std::sort(v.begin(), v.end());
)");
    ASSERT_EQ(pairs.size(), 2U);
    EXPECT_EQ(pairs[0].code, DiagCode::NoOperator);
    EXPECT_EQ(pairs[0].arg_text("op"), "<");
    EXPECT_EQ(pairs[0].arg_text("type"), "P");
    EXPECT_EQ(pairs[0].range.begin.line, 5);
    EXPECT_EQ(pairs[1].code, DiagCode::InstantiatedFrom);
    EXPECT_EQ(pairs[1].severity, Severity::Note);
    EXPECT_EQ(pairs[1].arg_text("function"), "std::sort<pair<P, int>>");
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

TEST(Modern, FreeOperatorTemplatesAreFound) {
    // An operator written as a function template is deduced like any other
    // function template, also from the namespace of the operands' class.
    EXPECT_EQ(printed(R"(
        template <class A, class B> struct Pr { A first; B second; Pr(const A& a, const B& b) : first(a), second(b) {} };
        template <class A, class B> bool operator<(const Pr<A, B>& x, const Pr<A, B>& y) {
            return x.first < y.first || (!(y.first < x.first) && x.second < y.second);
        }
        template <class A, class B> bool operator==(const Pr<A, B>& x, const Pr<A, B>& y) {
            return x.first == y.first && x.second == y.second;
        }
        namespace geo {
            template <class T> struct V { T x; };
            template <class T> bool operator<(const V<T>& a, const V<T>& b) { return a.x < b.x; }
        }
        Pr<int, int> a(1, 5);
        Pr<int, int> b(1, 6);
        geo::V<int> p;
        p.x = 1;
        geo::V<int> q;
        q.x = 2;
        std::cout << (a < b) << (b < a) << (a == a) << (a == b) << " " << (p < q) << (q < p);
    )"),
              "1010 10");

    // A function template may share its name with ordinary functions, declared
    // before or after them; on equal conversions the ordinary function wins.
    EXPECT_EQ(printed(R"(
        int f(int) { return 1; }
        template <class T> int f(T) { return 2; }
        template <class T> int g(T) { return 2; }
        int g(int) { return 1; }
        std::cout << f(1) << f(2.5) << f<int>(1) << g(1) << g('c');
    )"),
              "12212");

    // A template whose parameters cannot be deduced from the operands is not a candidate.
    auto r = run(R"(template <class A, class B> struct Pr { A first; B second; };
template <class A, class B> bool operator<(const Pr<A, B>& x, const Pr<A, B>& y) { return x.first < y.first; }
template <class T> struct Box { T v; };
Box<int> x;
Box<int> y;
bool r = x < y;
)");
    ASSERT_FALSE(r.compiled.ok());
    EXPECT_EQ(r.compiled.diagnostics[0].code, DiagCode::NoOperator);
    EXPECT_EQ(r.compiled.diagnostics[0].arg_text("type"), "Box<int>");
    EXPECT_EQ(r.compiled.diagnostics[0].range.begin.line, 6);
}

TEST(Modern, PairComparesFirstThenSecond) {
    EXPECT_EQ(printed(R"(
        std::pair<int, int> a(1, 5);
        std::pair<int, int> b(2, 0);
        std::pair<int, int> c(1, 6);
        std::cout << (a < b) << " " << (a < c) << " " << (a == std::make_pair(1, 5)) << " "
                  << (a != std::make_pair(1, 5)) << " " << (b >= a) << " " << (c > a) << (a <= a) << (b > c);
    )"),
              "1 1 1 0 1 111");
}

TEST(Modern, SortingPairsNeedsNoComparator) {
    EXPECT_EQ(printed(R"(
        std::vector<std::pair<int, int> > v;
        v.push_back(std::make_pair(2, 1));
        v.push_back(std::make_pair(1, 5));
        v.push_back(std::make_pair(1, 2));
        std::sort(v.begin(), v.end());
        for (int i = 0; i < 3; ++i) {
            std::cout << v[i].first << "," << v[i].second << " ";
        }
    )"),
              "1,2 1,5 2,1 ");
}

TEST(Modern, PairOfAClassWithoutLessHasNoLess) {
    // std::pair's operator< compares the members with <: a member type without
    // it is reported at the player's comparison, naming that type.
    auto r = run(R"(struct P { int x; };
std::pair<P, int> a;
std::pair<P, int> b;
bool less = a < b;
)");
    ASSERT_FALSE(r.compiled.ok());
    EXPECT_EQ(r.compiled.diagnostics[0].code, DiagCode::NoOperator);
    EXPECT_EQ(r.compiled.diagnostics[0].arg_text("op"), "<");
    EXPECT_EQ(r.compiled.diagnostics[0].arg_text("type"), "P");
    EXPECT_EQ(r.compiled.diagnostics[0].range.begin.line, 4);
    // Once, although the operator compares `first` twice, and without a
    // warning about the operator's own return: only the note follows.
    ASSERT_EQ(r.compiled.diagnostics.size(), 2U);
    EXPECT_EQ(r.compiled.diagnostics[1].code, DiagCode::InstantiatedFrom);
    EXPECT_EQ(r.compiled.diagnostics[1].arg_text("function"), "std::operator< <P, int>");
}

TEST(Modern, AReturnWithAnErrorIsNotAMissingReturn) {
    // The error is the missing operator; E0222 `missing-return` would only add noise.
    auto r = run("struct P { int x; };\nbool less(P a, P b) { return a < b; }\n");
    ASSERT_EQ(r.compiled.diagnostics.size(), 1U);
    EXPECT_EQ(r.compiled.diagnostics[0].code, DiagCode::NoOperator);
    // A function that really can end without returning still gets the warning.
    auto w = run("int f(int x) { if (x > 0) { return 1; } }\nint y = f(1);\n");
    ASSERT_EQ(w.compiled.diagnostics.size(), 1U);
    EXPECT_EQ(w.compiled.diagnostics[0].code, DiagCode::MissingReturn);
}

TEST(Modern, AVectorDoesNotConstructItsCapacity) {
    EXPECT_EQ(printed(R"(
        struct C { C() { std::cout << "c"; } };
        std::vector<C> v;
        v.reserve(8);
        std::cout << v.size() << v.capacity();
    )",
                      cpp17),
              "08");
}

TEST(Modern, AVectorNeedsNoDefaultConstructor) {
    EXPECT_EQ(printed(R"(
        struct N { int x; N(int v) : x(v) {} };
        std::vector<N> v;
        v.push_back(N(1));
        v.push_back(N(2));
        std::cout << v[0].x << v[1].x << v.size();
    )",
                      cpp17),
              "122");
}

TEST(Modern, PopBackAndClearDestroyElements) {
    // Each temporary D(i) dies after its push_back; pop_back and clear destroy
    // the elements they remove, and the vector's end has nothing left to destroy.
    EXPECT_EQ(printed(R"(
        struct D { int id; D(int i) : id(i) {} ~D() { std::cout << "~" << id; } };
        {
            std::vector<D> v;
            v.reserve(3);
            v.push_back(D(1));
            v.push_back(D(2));
            v.push_back(D(3));
            std::cout << "|";
            v.pop_back();
            std::cout << "|";
            v.clear();
            std::cout << "|";
        }
    )",
                      cpp17),
              "~1~2~3|~3|~1~2|");
}

TEST(Modern, GrowingMovesOrCopiesEachElementOnce) {
    // 5 copies made by push_back of an lvalue, then 4 relocations at the
    // 4 -> 8 growth: moves, as the move constructor is noexcept.
    EXPECT_EQ(printed(R"(
        int copies = 0;
        int moves = 0;
        struct M { M() {} M(const M&) { ++copies; } M(M&&) noexcept { ++moves; } };
        std::vector<M> v;
        v.reserve(4);
        M m;
        for (int i = 0; i < 5; ++i) {
            v.push_back(m);
        }
        std::cout << copies << moves << (copies + moves);
    )",
                      cpp17),
              "549");
}

TEST(Modern, AReferenceIntoAVectorDiesWhenItGrows) {
    // cppi-farm's iterator-invalidation level: growing frees the old storage.
    auto d = runtime_error(R"(std::vector<int> v;
v.push_back(1);
int& r = v[0];
v.push_back(2);
v.push_back(3);
v.push_back(4);
v.push_back(5);
int x = r;
)",
                           cpp17);
    EXPECT_EQ(d.code, DiagCode::UseAfterFree);
    EXPECT_EQ(d.range.begin.line, 8);
}

TEST(Modern, AnUnusedMemberIsNotInstantiated) {
    // [temp.inst]: a class template instance defines a member function only
    // when it is used, so W<P> exists although W's operator< cannot compare P.
    EXPECT_EQ(printed(R"(
        template <class A> struct W { A v; bool operator<(const W& o) const { return v < o.v; } };
        struct P { int x; };
        W<P> a;
        W<int> b;
        b.v = 0;
        W<int> c;
        c.v = 1;
        std::vector<P> ps;
        ps.push_back(P());
        std::map<char, P> m;
        m['a'].x = 3;
        std::cout << (b < c) << ps.size() << m['a'].x;
    )",
                      cpp17),
              "113");
}

TEST(Modern, AUsedMemberReportsItsError) {
    // Using the member reports its error, at the use for the standard library.
    auto r = run(R"(template <class A> struct W { A v; bool operator<(const W& o) const { return v < o.v; } };
struct P { int x; };
W<P> a;
W<P> b;
bool less = a < b;
)",
                 cpp17);
    ASSERT_FALSE(r.compiled.ok());
    EXPECT_EQ(r.compiled.diagnostics[0].code, DiagCode::NoOperator);
    EXPECT_EQ(r.compiled.diagnostics[0].arg_text("type"), "P");

    auto resize = run("struct N { int x; N(int v) : x(v) {} };\nstd::vector<N> v;\nv.resize(3);\n", cpp17);
    ASSERT_FALSE(resize.compiled.ok());
    EXPECT_EQ(resize.compiled.diagnostics[0].code, DiagCode::NoDefaultConstructor);
    EXPECT_EQ(resize.compiled.diagnostics[0].range.begin.line, 3);
}

TEST(Modern, SortWithStdGreater) {
    EXPECT_EQ(printed(R"(#include <functional>
        std::vector<int> v;
        v.push_back(2);
        v.push_back(5);
        v.push_back(1);
        std::sort(v.begin(), v.end(), std::greater<int>());
        std::cout << v[0] << v[1] << v[2] << " ";
        std::sort(v.begin(), v.end(), std::less<int>());
        std::cout << v[0] << v[1] << v[2] << " ";
        std::less<int> lt;
        std::equal_to<char> eq;
        std::greater<std::pair<int, int>> gp;
        std::cout << lt(1, 2) << lt(2, 1) << eq('a', 'a') << gp(std::make_pair(2, 0), std::make_pair(1, 9));
    )",
                      cpp17),
              "521 125 1011");
}

TEST(Modern, QueueIsFirstInFirstOut) {
    EXPECT_EQ(printed(R"(#include <queue>
        std::queue<int> q;
        for (int i = 1; i <= 6; ++i) {
            q.push(i);
        }
        std::cout << q.front() << q.back() << q.size();
        q.pop();
        q.pop();
        q.push(7);
        while (!q.empty()) {
            std::cout << q.front();
            q.pop();
        }
    )",
                      cpp17),
              "16634567");
}

TEST(Modern, QueuePopDestroysTheFront) {
    EXPECT_EQ(printed(R"(#include <queue>
        struct D { int id; D(int i) : id(i) {} ~D() { std::cout << "~" << id; } };
        {
            std::queue<D> q;
            q.push(D(1));
            q.push(D(2));
            std::cout << "|";
            q.pop();
            std::cout << "|";
        }
    )",
                      cpp17),
              "~1~2|~1|~2");
}

TEST(Modern, StackIsLastInFirstOut) {
    EXPECT_EQ(printed(R"(#include <stack>
        std::stack<int> s;
        s.push(1);
        s.push(2);
        s.push(3);
        std::cout << s.top() << s.size();
        s.pop();
        std::cout << s.top();
    )",
                      cpp17),
              "332");
}

TEST(Modern, PriorityQueueTopIsTheLargest) {
    EXPECT_EQ(printed(R"(#include <queue>
        std::priority_queue<int> pq;
        pq.push(3);
        pq.push(9);
        pq.push(1);
        pq.push(5);
        while (!pq.empty()) {
            std::cout << pq.top();
            pq.pop();
        }
    )",
                      cpp17),
              "9531");
}

TEST(Modern, PriorityQueueWithGreaterTopIsTheSmallest) {
    // Dijkstra's open set: (cost, tile) pairs, cheapest first.
    EXPECT_EQ(printed(R"(#include <queue>
        std::priority_queue<std::pair<int, int>, std::vector<std::pair<int, int>>, std::greater<std::pair<int, int>>> open;
        open.push(std::make_pair(4, 1));
        open.push(std::make_pair(2, 7));
        open.push(std::make_pair(2, 3));
        open.push(std::make_pair(9, 0));
        while (!open.empty()) {
            std::cout << open.top().first << open.top().second << ",";
            open.pop();
        }
    )",
                      cpp17),
              "23,27,41,90,");
}

TEST(Modern, PoppingAnEmptyQueueIsOutOfBounds) {
    // Undefined behavior in real C++: cppi stops with E0503, as vector::back does.
    EXPECT_EQ(runtime_error("std::queue<int> q;\nq.pop();\n", cpp17).code, DiagCode::OutOfBounds);
    EXPECT_EQ(runtime_error("std::queue<int> q;\nint x = q.front();\n", cpp17).code, DiagCode::OutOfBounds);
    EXPECT_EQ(runtime_error("std::stack<int> s;\nint x = s.top();\n", cpp17).code, DiagCode::OutOfBounds);
    EXPECT_EQ(runtime_error("std::priority_queue<int> p;\np.pop();\n", cpp17).code, DiagCode::OutOfBounds);
    auto d = runtime_error("std::queue<int> q;\nq.push(1);\nq.pop();\nq.pop();\n", cpp17);
    EXPECT_EQ(d.code, DiagCode::OutOfBounds);
    EXPECT_EQ(d.range.begin.line, 4);
}

TEST(Modern, ADataMemberThatIsAFunctionObjectCanBeCalled) {
    // `f(n)` inside the class and `h.f(5)`, `p->f(6)` outside call the member's operator().
    EXPECT_EQ(printed(R"(
        struct Twice { int operator()(int x) const { return 2 * x; } };
        struct Holder {
            Twice f;
            int n;
            int run() { return f(n); }
        };
        Holder h;
        h.n = 4;
        std::cout << h.run() << h.f(5);
        Holder* p = &h;
        std::cout << p->f(6);
    )",
                      cpp17),
              "81012");
    // A data member that is not a function object is still not callable.
    EXPECT_EQ(compile_error("struct S { int x; };\nS s;\ns.x(1);\n"), DiagCode::NotCallable);
}

TEST(Modern, AlgorithmsTheGameTeaches) {
    // count_if, find_if, find_if_not, all_of, any_of, none_of, iota,
    // min_element/max_element with a comparator and stable_sort, as in g++.
    EXPECT_EQ(printed(R"(#include <algorithm>
#include <numeric>
        auto even = [](int x) { return x % 2 == 0; };
        std::vector<int> v(6);
        std::iota(v.begin(), v.end(), 3);
        std::cout << v[0] << v[5] << " ";
        std::cout << std::count_if(v.begin(), v.end(), even) << " ";
        auto e = std::find_if(v.begin(), v.end(), even);
        auto o = std::find_if_not(v.begin(), v.end(), [](int x) { return x < 6; });
        std::cout << *e << *o << (std::find_if(v.begin(), v.end(), [](int x) { return x > 99; }) == v.end()) << " ";
        std::cout << std::all_of(v.begin(), v.end(), [](int x) { return x > 2; })
                  << std::any_of(v.begin(), v.end(), even)
                  << std::none_of(v.begin(), v.end(), [](int x) { return x > 8; }) << " ";
        std::vector<std::pair<int, int>> p;
        p.push_back(std::make_pair(1, 9));
        p.push_back(std::make_pair(5, 2));
        p.push_back(std::make_pair(3, 2));
        auto bySecond = [](const std::pair<int, int>& a, const std::pair<int, int>& b) { return a.second < b.second; };
        std::cout << std::min_element(p.begin(), p.end(), bySecond)->first
                  << std::max_element(p.begin(), p.end(), bySecond)->first << " ";
        std::stable_sort(p.begin(), p.end(), bySecond);
        for (int i = 0; i < 3; ++i) {
            std::cout << p[i].first;
        }
        std::cout << " ";
        std::vector<int> w(3);
        w[0] = 3;
        w[1] = 1;
        w[2] = 2;
        std::stable_sort(w.begin(), w.end());
        std::cout << w[0] << w[1] << w[2];
    )",
                      cpp11),
              "38 3 461 111 51 531 123");
}

TEST(Modern, BracedListsAndInitializerList) {
    // C++11 list-initialization: initializer-list constructors win, a braced
    // argument or right side of = is a temporary of the parameter's type, and
    // a range-for over {..} iterates a std::initializer_list.
    EXPECT_EQ(printed(R"(#include <initializer_list>
        struct Order { int x; int y; };
        int sum(std::initializer_list<int> l) {
            int s = 0;
            for (int v : l) {
                s += v;
            }
            return s;
        }
        std::vector<int> v = {1, 2, 3};
        std::cout << v.size() << v[2] << " ";
        v = {4, 5};
        std::cout << v.size() << v[0] << v[1] << " ";
        std::map<char, int> m = {{'b', 2}, {'a', 1}, {'b', 9}};
        std::cout << m.size() << m['a'] << m['b'] << " ";
        for (int x : {7, 8, 9}) {
            std::cout << x;
        }
        std::cout << " ";
        std::vector<Order> orders;
        orders.push_back({5, 2});
        std::cout << orders[0].x << orders[0].y << " ";
        std::initializer_list<int> l = {1, 2, 3, 4};
        std::cout << l.size() << sum(l) << sum({10, 20}) << " ";
        std::vector<int> e = {};
        std::vector<int> one{3};
        std::vector<int> three(3);
        std::cout << e.size() << one.size() << one[0] << three.size() << " ";
        std::vector<std::string> words = {"ab", "c"};
        for (std::string w : {std::string("x"), std::string("yz")}) {
            std::cout << w;
        }
        std::cout << words[0] << words[1] << words.size();
    )",
                      cpp11),
              "33 245 212 789 52 41030 0133 xyzabc2");
}

TEST(Modern, InitializerListsAreCpp11) {
    // C++98 has aggregates and arrays in braces, but no std::initializer_list.
    EXPECT_EQ(compile_error("std::vector<int> v = {1, 2};\n", standard(Standard::Cpp98)),
              DiagCode::FeatureRequiresStandard);
    EXPECT_EQ(printed("struct P { int x; int y; };\nP p = {1, 2};\nint a[] = {3, 4};\nstd::cout << p.y << a[1];\n",
                      standard(Standard::Cpp98)),
              "24");
}

TEST(Modern, ARangeForDestroysItsRangeTemporariesAfterTheLoop) {
    // [stmt.ranged]: a temporary range (a returned vector, the array behind
    // {..}) lives until the loop ends, then is destroyed.
    EXPECT_EQ(printed(R"(
        int live = 0;
        struct C { C() { ++live; } C(const C&) { ++live; } ~C() { --live; } };
        std::vector<C> make() {
            std::vector<C> v;
            v.push_back(C());
            v.push_back(C());
            return v;
        }
        int n = 0;
        for (const C& c : make()) {
            ++n;
            std::cout << live;
        }
        std::cout << " " << n << live;
        for (const C& c : {C(), C(), C()}) {
            ++n;
        }
        std::cout << " " << n << live;
    )",
                      cpp11),
              "22 20 50");
}

TEST(Modern, DirectListInitialization) {
    // `T{...}`: an aggregate, a constructor call, a scalar, a library class.
    EXPECT_EQ(printed(R"(
        struct Order { int x; int y; };
        struct Point { int a; int b; Point(int x, int y) : a(x * 10), b(y) {} };
        int sum(Order o) { return o.x + o.y; }
        Order o = Order{5, 2};
        std::cout << o.x << o.y << sum(Order{3, 4}) << " ";
        auto p = Point{1, 2};
        std::cout << p.a << p.b << Point{3, 4}.a << " ";
        std::cout << int{3} << int{} << char{65} << " ";
        std::cout << std::vector<int>{1, 2, 3}.size() << Order{}.y;
    )",
                      cpp11),
              "527 10230 30A 30");
    EXPECT_EQ(compile_error("struct Order { int x; };\nOrder o = Order{1};\n", standard(Standard::Cpp98)),
              DiagCode::FeatureRequiresStandard);
}

TEST(Modern, NarrowingInsideBracesIsAnError) {
    // C++11 forbids conversions that may lose information inside braces.
    for (const char* source : {"int i = int{2.5};\n", "int a[] = {2.5};\n", "char c{300};\n",
                               "double d = 1;\nint i{d};\n", "std::vector<int> v = {1.5};\n"}) {
        auto r = run(source, cpp11);
        ASSERT_FALSE(r.compiled.ok()) << source;
        EXPECT_EQ(r.compiled.diagnostics[0].code, DiagCode::Narrowing) << source;
    }
    auto d = run("int i = int{2.5};\n", cpp11).compiled.diagnostics[0];
    EXPECT_EQ(d.arg_text("from"), "double");
    EXPECT_EQ(d.arg_text("to"), "int");
    // A constant that fits is fine, and C++98 allows the conversion.
    EXPECT_EQ(printed("char c{65};\nlong l{2};\ndouble x{3};\nstd::cout << c << l << x;\n", cpp11), "A23");
    EXPECT_EQ(printed("int a[] = {2.5};\nstd::cout << a[0];\n", standard(Standard::Cpp98)), "2");
}

TEST(Modern, DesignatedInitializers) {
    // C++20: members named by designators; the others are value-initialized
    // (or take their default member initializer).
    const Options cpp20 = standard(Standard::Cpp20);
    EXPECT_EQ(printed(R"(
        struct Order { int x; int y; int z = 7; };
        Order o = {.x = 5, .y = 2};
        Order p = Order{.y = 3};
        Order q{.x = 1, .z = 9};
        std::cout << o.x << o.y << o.z << " " << p.x << p.y << p.z << " " << q.x << q.y << q.z;
    )",
                      cpp20),
              "527 037 109");
    const char* order = "struct Order { int x; int y; };\n";
    for (const char* bad :
         {"Order o = {.y = 2, .x = 5};\n", "Order o = {.x = 1, .x = 2};\n", "Order o = {.x = 1, 2};\n"}) {
        auto r = run(std::string(order) + bad, cpp20);
        ASSERT_FALSE(r.compiled.ok()) << bad;
        EXPECT_EQ(r.compiled.diagnostics[0].code, DiagCode::DesignatorOrder) << bad;
    }
    EXPECT_EQ(compile_error(std::string(order) + "Order o = {.w = 1};\n", cpp20), DiagCode::NoMember);
    EXPECT_EQ(compile_error(std::string(order) + "Order o = {.x = 5};\n", cpp17), DiagCode::FeatureRequiresStandard);
}

TEST(Modern, ThreeWayComparisonAndDefaultedComparisons) {
    // C++20: operator<=> (defaulted member by member, or on numbers), the
    // rewritten <, <=, >, >= and !=, and the operator== a defaulted <=> declares.
    EXPECT_EQ(printed(R"(#include <compare>
        struct P {
        int x;
        int y;
        auto operator<=>(const P&) const = default;
        };
        struct Q {
        int x;
        bool operator==(const Q&) const = default;
        };
        struct D {
        double v;
        auto operator<=>(const D&) const = default;
        };
        P a{1, 2};
        P b{1, 3};
        std::cout << (a < b) << (a <= b) << (a > b) << (a >= b) << (a == b) << (a != b) << (a == a) << " ";
        std::vector<P> ps;
        ps.push_back(P{2, 0});
        ps.push_back(P{1, 5});
        ps.push_back(P{1, 4});
        std::sort(ps.begin(), ps.end());
        std::cout << ps[0].y << ps[1].y << ps[2].x << " ";
        std::strong_ordering r = a <=> b;
        int i = 3;
        int j = 7;
        std::cout << (r < 0) << ((i <=> j) < 0) << ((j <=> i) > 0) << ((i <=> i) == 0) << ((2.5 <=> 1.0) > 0) << " ";
        Q q1{4};
        Q q2{5};
        std::cout << (q1 == q2) << (q1 != q2) << " ";
        D d1{1.5};
        D d2{2.0};
        std::cout << (d1 < d2) << (d2 < d1);
    )",
                      standard(Standard::Cpp20)),
              "1100011 452 11111 01 10");
    EXPECT_EQ(compile_error("int a = 1;\nbool r = (a <=> a) == 0;\n", cpp17), DiagCode::FeatureRequiresStandard);
    EXPECT_EQ(compile_error("struct P {\n    int x;\n    bool operator==(const P&) const = default;\n};\n", cpp17),
              DiagCode::FeatureRequiresStandard);
}

TEST(Modern, ConstevalFunctionsRunAtCompileTime) {
    // C++20 consteval: every call is a constant (usable as an array bound), and
    // a call inside another consteval function is evaluated with it.
    const Options cpp20 = standard(Standard::Cpp20);
    EXPECT_EQ(printed(R"(
        consteval int sq(int x) { return x * x; }
        consteval int quad(int x) { return sq(x) * sq(x); }
        constexpr int a = sq(4);
        int arr[sq(2)];
        std::cout << a << " " << sq(5) << " " << quad(2) << " " << sizeof(arr) / sizeof(int);
    )",
                      cpp20),
              "16 25 16 4");
    auto r = run("consteval int sq(int x) { return x * x; }\nint n = 3;\nint r = sq(n);\n", cpp20);
    ASSERT_FALSE(r.compiled.ok());
    EXPECT_EQ(r.compiled.diagnostics[0].code, DiagCode::NotConstant);
    EXPECT_EQ(r.compiled.diagnostics[0].range.begin.line, 3);
    EXPECT_EQ(compile_error("consteval int sq(int x) { return x * x; }\n", cpp17), DiagCode::FeatureRequiresStandard);
}

TEST(Modern, LambdasCaptureThis) {
    // [this] and [=] reach the members through the object's address; [*this]
    // (C++17) holds a copy, so writing it leaves the object alone.
    EXPECT_EQ(printed(R"(
        struct Counter {
            int n = 0;
            int step = 1;
            int twice() const { return 2 * n; }
            void run() {
                auto add = [this](int k) { n += k * step; };
                add(3);
                auto peek = [=]() { return n + twice(); };
                auto me = [this]() { return this->n; };
                auto copy = [*this]() mutable { n = 100; return n; };
                std::cout << n << peek() << me() << copy() << n << " ";
            }
        };
        Counter c;
        c.run();
        std::cout << c.n;
    )",
                      cpp17),
              "3931003 3");
    // The captured address dies with the object.
    auto d = runtime_error(R"(struct S {
    int v = 5;
    int test() {
        auto f = [this]() { return v; };
        delete this;
        return f();
    }
};
S* s = new S;
int x = s->test();
)",
                           cpp17);
    EXPECT_EQ(d.code, DiagCode::UseAfterFree);
    EXPECT_EQ(d.range.begin.line, 4);
    EXPECT_EQ(compile_error("struct S {\n    int v = 0;\n    void f() { auto g = [*this]() { return v; }; }\n};\n",
                            standard(Standard::Cpp14)),
              DiagCode::FeatureRequiresStandard);
}

TEST(Modern, NonTypeTemplateParameters) {
    // `template <int N>`: N is a constant inside the template (an array bound
    // too), deduced from Buf<int, N>, and instances differing only in N are
    // distinct types.
    EXPECT_EQ(printed(R"(
        template <int N> int twice() { return 2 * N; }
        template <class T, int N> struct Buf { T cells[N]; int size() const { return N; } };
        template <int N> int count(const Buf<int, N>& b) { return b.size() + N; }
        Buf<int, 3> a;
        Buf<int, 4> b;
        constexpr int K = 5;
        std::cout << twice<3>() << twice<K>() << " " << a.size() << b.size() << sizeof(a) / sizeof(int) << " " << count(a);
    )",
                      cpp17),
              "610 343 6");
    auto r =
        run("template <class T, int N> struct Buf { T cells[N]; };\nBuf<int, 3> a;\nBuf<int, 4> b;\na = b;\n", cpp17);
    EXPECT_FALSE(r.compiled.ok());
    // C++98 has them too.
    EXPECT_EQ(printed("template <int N> int twice() { return 2 * N; }\nstd::cout << twice<4>();\n",
                      standard(Standard::Cpp98)),
              "8");
}
