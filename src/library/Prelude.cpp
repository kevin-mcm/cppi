/// @file Prelude.cpp
/// @brief Implementation of Prelude.hpp.
///
/// @author kevin-mcm <kevincardenasmiranda9@gmail.com>
/// @date 2026-10-08

#include "library/Prelude.hpp"

namespace cppi::library {

namespace {

// Kept free of string literals: they would cost operations at startup.
constexpr std::string_view kSource = R"cpp(
    namespace std {

    // --- Output -------------------------------------------------------------------

    struct ostream {};
    struct __endl_t {};
    ostream cout;
    __endl_t endl;

    ostream& operator<<(ostream& os, int v) {
        __cppi_write_long(v);
        return os;
    }
    ostream& operator<<(ostream& os, long v) {
        __cppi_write_long(v);
        return os;
    }
    ostream& operator<<(ostream& os, unsigned int v) {
        __cppi_write_long(v);
        return os;
    }
    ostream& operator<<(ostream& os, unsigned long v) {
        __cppi_write_ulong(v);
        return os;
    }
    ostream& operator<<(ostream& os, short v) {
        __cppi_write_long(v);
        return os;
    }
    ostream& operator<<(ostream& os, unsigned short v) {
        __cppi_write_long(v);
        return os;
    }
    ostream& operator<<(ostream& os, unsigned char v) {
        __cppi_write_char(static_cast<char>(v));
        return os;
    }
    ostream& operator<<(ostream& os, double v) {
        __cppi_write_double(v);
        return os;
    }
    ostream& operator<<(ostream& os, char v) {
        __cppi_write_char(v);
        return os;
    }
    ostream& operator<<(ostream& os, bool v) {
        __cppi_write_long(v ? 1 : 0);
        return os;
    }
    ostream& operator<<(ostream& os, const char* s) {
        __cppi_write_string(s);
        return os;
    }
    ostream& operator<<(ostream& os, __endl_t) {
        __cppi_write_char('\n');
        return os;
    }

    // --- Concepts (C++20) ------------------------------------------------------------

    template <class T, class U>
    concept same_as = __cppi_is_same<T, U>();
    template <class T>
    concept integral = __cppi_is_integral<T>();
    template <class T>
    concept signed_integral = __cppi_is_integral<T>() && __cppi_is_signed<T>();
    template <class T>
    concept unsigned_integral = __cppi_is_integral<T>() && !__cppi_is_signed<T>();
    template <class T>
    concept floating_point = __cppi_is_floating_point<T>();
    template <class From, class To>
    concept convertible_to = __cppi_is_convertible<From, To>();
    template <class Derived, class Base>
    concept derived_from = __cppi_is_class<Derived>() && __cppi_is_base_of<Base, Derived>();

    // --- Small helpers ------------------------------------------------------------

    template <class T>
    T min(T a, T b) {
        return b < a ? b : a;
    }
    template <class T>
    T max(T a, T b) {
        return a < b ? b : a;
    }
    template <class T>
    void swap(T& a, T& b) {
        T t = a;
        a = b;
        b = t;
    }
    int abs(int x) {
        return x < 0 ? -x : x;
    }
    long abs(long x) {
        return x < 0 ? -x : x;
    }
    double abs(double x) {
        return x < 0 ? -x : x;
    }

    template <class A, class B>
    struct pair {
        A first;
        B second;
        pair() : first(), second() {}
        pair(const A& a, const B& b) : first(a), second(b) {}
    };
    template <class A, class B>
    pair<A, B> make_pair(A a, B b) {
        return pair<A, B>(a, b);
    }

    // --- Algorithms on ranges given by pointers --------------------------------------

    template <class T>
    void sort(T* first, T* last) {
        for (T* i = first + 1; i < last; ++i) {
            T value = *i;
            T* j = i;
            while (j > first && value < *(j - 1)) {
                *j = *(j - 1);
                --j;
            }
            *j = value;
        }
    }
    template <class T, class Less>
    void sort(T* first, T* last, Less less) {
        for (T* i = first + 1; i < last; ++i) {
            T value = *i;
            T* j = i;
            while (j > first && less(value, *(j - 1))) {
                *j = *(j - 1);
                --j;
            }
            *j = value;
        }
    }
    template <class T>
    void reverse(T* first, T* last) {
        while (first < last) {
            --last;
            T t = *first;
            *first = *last;
            *last = t;
            ++first;
        }
    }
    template <class T>
    T* find(T* first, T* last, const T& value) {
        for (; first != last; ++first) {
            if (*first == value) {
                return first;
            }
        }
        return last;
    }
    template <class T>
    int count(T* first, T* last, const T& value) {
        int n = 0;
        for (; first != last; ++first) {
            if (*first == value) {
                ++n;
            }
        }
        return n;
    }
    template <class T>
    T accumulate(T* first, T* last, T init) {
        for (; first != last; ++first) {
            init = init + *first;
        }
        return init;
    }

    template <class T>
    T* min_element(T* first, T* last) {
        T* best = first;
        for (; first != last; ++first) {
            if (*first < *best) {
                best = first;
            }
        }
        return best;
    }
    template <class T>
    T* max_element(T* first, T* last) {
        T* best = first;
        for (; first != last; ++first) {
            if (*best < *first) {
                best = first;
            }
        }
        return best;
    }
    template <class T>
    void fill(T* first, T* last, const T& value) {
        for (; first != last; ++first) {
            *first = value;
        }
    }

    // --- std::string -------------------------------------------------------------------

    class string {
    public:
        string() : data_(0), size_(0), capacity_(0) { reserve(1); }
        string(const char* text) : data_(0), size_(0), capacity_(0) {
            reserve(1);
            for (int i = 0; text[i] != 0; ++i) {
                push_back(text[i]);
            }
        }
        string(int n, char c) : data_(0), size_(0), capacity_(0) {
            reserve(n + 1);
            for (int i = 0; i < n; ++i) {
                push_back(c);
            }
        }
        string(const string& other) : data_(0), size_(0), capacity_(0) {
            reserve(other.size_ + 1);
            for (int i = 0; i < other.size_; ++i) {
                push_back(other.data_[i]);
            }
        }
        ~string() { delete[] data_; }
        string& operator=(const string& other) {
            if (this != &other) {
                size_ = 0;
                data_[0] = 0;
                for (int i = 0; i < other.size_; ++i) {
                    push_back(other.data_[i]);
                }
            }
            return *this;
        }

        int size() const { return size_; }
        int length() const { return size_; }
        bool empty() const { return size_ == 0; }
        const char* c_str() const { return data_; }

        char& operator[](int i) {
            __cppi_check_index(i, size_);
            return data_[i];
        }
        const char& operator[](int i) const {
            __cppi_check_index(i, size_);
            return data_[i];
        }
        char& at(int i);  // defined after std::out_of_range
        char* begin() { return data_; }
        char* end() { return data_ + size_; }
        const char* begin() const { return data_; }
        const char* end() const { return data_ + size_; }

        void push_back(char c) {
            reserve(size_ + 2);
            data_[size_] = c;
            ++size_;
            data_[size_] = 0;
        }
        void pop_back() {
            __cppi_check_index(size_ - 1, size_);
            --size_;
            data_[size_] = 0;
        }
        void clear() {
            size_ = 0;
            data_[0] = 0;
        }
        string& operator+=(const string& other) {
            for (int i = 0; i < other.size_; ++i) {
                push_back(other.data_[i]);
            }
            return *this;
        }
        string& operator+=(char c) {
            push_back(c);
            return *this;
        }
        string substr(int position, int length) const {
            string out;
            for (int i = position; i < size_ && i < position + length; ++i) {
                out.push_back(data_[i]);
            }
            return out;
        }
        string substr(int position) const { return substr(position, size_ - position); }
        int find(char c) const {
            for (int i = 0; i < size_; ++i) {
                if (data_[i] == c) {
                    return i;
                }
            }
            return -1;
        }

    private:
        void reserve(int n) {
            if (n <= capacity_) {
                return;
            }
            int wanted = capacity_ == 0 ? 8 : capacity_ * 2;
            if (wanted < n) {
                wanted = n;
            }
            char* bigger = new char[wanted];
            for (int i = 0; i < size_; ++i) {
                bigger[i] = data_[i];
            }
            bigger[size_] = 0;
            delete[] data_;
            data_ = bigger;
            capacity_ = wanted;
        }

        char* data_;
        int size_;
        int capacity_;
    };

    string operator+(const string& a, const string& b) {
        string out = a;
        out += b;
        return out;
    }
    string operator+(const string& a, char c) {
        string out = a;
        out += c;
        return out;
    }
    bool operator==(const string& a, const string& b) {
        if (a.size() != b.size()) {
            return false;
        }
        for (int i = 0; i < a.size(); ++i) {
            if (a[i] != b[i]) {
                return false;
            }
        }
        return true;
    }
    bool operator!=(const string& a, const string& b) {
        return !(a == b);
    }
    bool operator<(const string& a, const string& b) {
        for (int i = 0; i < a.size() && i < b.size(); ++i) {
            if (a[i] != b[i]) {
                return a[i] < b[i];
            }
        }
        return a.size() < b.size();
    }
    bool operator>(const string& a, const string& b) {
        return b < a;
    }
    ostream& operator<<(ostream& os, const string& s) {
        __cppi_write_string(s.c_str());
        return os;
    }

    string to_string(long value) {
        if (value == 0) {
            return string(1, '0');
        }
        bool negative = value < 0;
        string digits;
        while (value != 0) {
            long digit = value % 10;
            if (digit < 0) {
                digit = -digit;
            }
            digits.push_back(static_cast<char>('0' + digit));
            value = value / 10;
        }
        string out;
        if (negative) {
            out.push_back('-');
        }
        for (int i = digits.size() - 1; i >= 0; --i) {
            out.push_back(digits[i]);
        }
        return out;
    }
    string to_string(int value) {
        return to_string(static_cast<long>(value));
    }

    // --- Exceptions -------------------------------------------------------------------------

    class exception {
    public:
        exception() : __text("std::exception") {}
        exception(const char* message) : __text(message) {}
        exception(const string& message) : __text(message) {}
        virtual ~exception() {}
        virtual const char* what() const { return __text.c_str(); }
        string __text;  // read by the interpreter to explain an uncaught exception
    };
    class logic_error : public exception {
    public:
        explicit logic_error(const char* message) : exception(message) {}
        explicit logic_error(const string& message) : exception(message) {}
    };
    class runtime_error : public exception {
    public:
        explicit runtime_error(const char* message) : exception(message) {}
        explicit runtime_error(const string& message) : exception(message) {}
    };
    class out_of_range : public logic_error {
    public:
        explicit out_of_range(const char* message) : logic_error(message) {}
        explicit out_of_range(const string& message) : logic_error(message) {}
    };
    class invalid_argument : public logic_error {
    public:
        explicit invalid_argument(const char* message) : logic_error(message) {}
        explicit invalid_argument(const string& message) : logic_error(message) {}
    };
    class length_error : public logic_error {
    public:
        explicit length_error(const char* message) : logic_error(message) {}
        explicit length_error(const string& message) : logic_error(message) {}
    };
    class overflow_error : public runtime_error {
    public:
        explicit overflow_error(const char* message) : runtime_error(message) {}
        explicit overflow_error(const string& message) : runtime_error(message) {}
    };
    class bad_optional_access : public exception {
    public:
        bad_optional_access() : exception("bad optional access") {}
    };

    char& string::at(int i) {
        if (i < 0 || i >= size_) {
            throw out_of_range("string::at: index out of range");
        }
        return data_[i];
    }

    // --- std::vector ------------------------------------------------------------------

    template <class T>
    class vector {
    public:
        vector() : data_(0), size_(0), capacity_(0) {}
        vector(int n) : data_(0), size_(0), capacity_(0) { resize(n); }
        vector(int n, const T& value) : data_(0), size_(0), capacity_(0) {
            reserve(n);
            for (int i = 0; i < n; ++i) {
                push_back(value);
            }
        }
        vector(const vector<T>& other) : data_(0), size_(0), capacity_(0) {
            reserve(other.size_);
            for (int i = 0; i < other.size_; ++i) {
                push_back(other.data_[i]);
            }
        }
        ~vector() { delete[] data_; }
        vector<T>& operator=(const vector<T>& other) {
            if (this != &other) {
                clear();
                reserve(other.size_);
                for (int i = 0; i < other.size_; ++i) {
                    push_back(other.data_[i]);
                }
            }
            return *this;
        }

        int size() const { return size_; }
        bool empty() const { return size_ == 0; }
        int capacity() const { return capacity_; }

        T& operator[](int i) {
            __cppi_check_index(i, size_);
            return data_[i];
        }
        const T& operator[](int i) const {
            __cppi_check_index(i, size_);
            return data_[i];
        }
        T& at(int i) {
            if (i < 0 || i >= size_) {
                throw out_of_range("vector::at: index out of range");
            }
            return data_[i];
        }
        const T& at(int i) const {
            if (i < 0 || i >= size_) {
                throw out_of_range("vector::at: index out of range");
            }
            return data_[i];
        }
        T& front() {
            __cppi_check_index(0, size_);
            return data_[0];
        }
        T& back() {
            __cppi_check_index(size_ - 1, size_);
            return data_[size_ - 1];
        }

        T* begin() { return data_; }
        T* end() { return data_ + size_; }
        const T* begin() const { return data_; }
        const T* end() const { return data_ + size_; }

        void push_back(const T& value) {
            if (size_ == capacity_) {
                reserve(capacity_ == 0 ? 4 : capacity_ * 2);
            }
            data_[size_] = value;
            ++size_;
        }
        void pop_back() {
            __cppi_check_index(size_ - 1, size_);
            --size_;
        }
        void clear() { size_ = 0; }
        void resize(int n) {
            reserve(n);
            for (int i = size_; i < n; ++i) {
                data_[i] = T();
            }
            size_ = n;
        }
        void reserve(int n) {
            if (n <= capacity_) {
                return;
            }
            T* bigger = new T[n];
            for (int i = 0; i < size_; ++i) {
                bigger[i] = data_[i];
            }
            delete[] data_;
            data_ = bigger;
            capacity_ = n;
        }
        T* insert(T* position, const T& value) {
            int index = position - data_;
            push_back(value);
            for (int i = size_ - 1; i > index; --i) {
                data_[i] = data_[i - 1];
            }
            data_[index] = value;
            return data_ + index;
        }
        T* erase(T* position) {
            int index = position - data_;
            __cppi_check_index(index, size_);
            for (int i = index; i + 1 < size_; ++i) {
                data_[i] = data_[i + 1];
            }
            --size_;
            return data_ + index;
        }

    private:
        T* data_;
        int size_;
        int capacity_;
    };

    // --- std::map (a sorted vector of pairs) -------------------------------------------

    template <class K, class V>
    class map {
    public:
        int size() const { return items_.size(); }
        bool empty() const { return items_.empty(); }
        void clear() { items_.clear(); }

        V& operator[](const K& key) {
            int i = lower_bound(key);
            if (i == items_.size() || key < items_[i].first) {
                items_.insert(items_.begin() + i, pair<K, V>(key, V()));
            }
            return items_[i].second;
        }
        V& at(const K& key) {
            int i = lower_bound(key);
            __cppi_check_index(i < items_.size() && !(key < items_[i].first) ? 0 : -1, 1);
            return items_[i].second;
        }
        int count(const K& key) const {
            int i = lower_bound(key);
            return i < items_.size() && !(key < items_[i].first) ? 1 : 0;
        }
        pair<K, V>* find(const K& key) {
            int i = lower_bound(key);
            if (i < items_.size() && !(key < items_[i].first)) {
                return items_.begin() + i;
            }
            return items_.end();
        }
        int erase(const K& key) {
            int i = lower_bound(key);
            if (i < items_.size() && !(key < items_[i].first)) {
                items_.erase(items_.begin() + i);
                return 1;
            }
            return 0;
        }
        pair<K, V>* begin() { return items_.begin(); }
        pair<K, V>* end() { return items_.end(); }
        const pair<K, V>* begin() const { return items_.begin(); }
        const pair<K, V>* end() const { return items_.end(); }

    private:
        int lower_bound(const K& key) const {
            int low = 0;
            int high = items_.size();
            while (low < high) {
                int middle = (low + high) / 2;
                if (items_[middle].first < key) {
                    low = middle + 1;
                } else {
                    high = middle;
                }
            }
            return low;
        }
        vector<pair<K, V>> items_;
    };

    // --- Smart pointers (C++11) ----------------------------------------------------------

    template <class T>
    class unique_ptr {
    public:
        unique_ptr() : p_(0) {}
        explicit unique_ptr(T* p) : p_(p) {}
        unique_ptr(const unique_ptr<T>& other) = delete;
        unique_ptr(unique_ptr<T>&& other) : p_(other.p_) { other.p_ = 0; }
        ~unique_ptr() { delete p_; }
        unique_ptr<T>& operator=(const unique_ptr<T>& other) = delete;
        unique_ptr<T>& operator=(unique_ptr<T>&& other) {
            if (this != &other) {
                delete p_;
                p_ = other.p_;
                other.p_ = 0;
            }
            return *this;
        }
        T& operator*() const { return *p_; }
        T* operator->() const { return p_; }
        T* get() const { return p_; }
        T* release() {
            T* out = p_;
            p_ = 0;
            return out;
        }
        void reset() {
            delete p_;
            p_ = 0;
        }
        void reset(T* p) {
            delete p_;
            p_ = p;
        }
        explicit operator bool() const { return p_ != 0; }

    private:
        T* p_;
    };

    template <class T>
    unique_ptr<T> make_unique() {
        return unique_ptr<T>(new T());
    }
    template <class T, class A>
    unique_ptr<T> make_unique(A a) {
        return unique_ptr<T>(new T(a));
    }
    template <class T, class A, class B>
    unique_ptr<T> make_unique(A a, B b) {
        return unique_ptr<T>(new T(a, b));
    }
    template <class T, class A, class B, class C>
    unique_ptr<T> make_unique(A a, B b, C c) {
        return unique_ptr<T>(new T(a, b, c));
    }

    template <class T>
    class shared_ptr {
    public:
        shared_ptr() : p_(0), count_(0) {}
        explicit shared_ptr(T* p) : p_(p), count_(new int(1)) {}
        shared_ptr(const shared_ptr<T>& other) : p_(other.p_), count_(other.count_) {
            if (count_ != 0) {
                ++*count_;
            }
        }
        ~shared_ptr() { drop(); }
        shared_ptr<T>& operator=(const shared_ptr<T>& other) {
            if (this != &other) {
                drop();
                p_ = other.p_;
                count_ = other.count_;
                if (count_ != 0) {
                    ++*count_;
                }
            }
            return *this;
        }
        T& operator*() const { return *p_; }
        T* operator->() const { return p_; }
        T* get() const { return p_; }
        int use_count() const { return count_ == 0 ? 0 : *count_; }
        void reset() {
            drop();
            p_ = 0;
            count_ = 0;
        }
        explicit operator bool() const { return p_ != 0; }

    private:
        void drop() {
            if (count_ != 0) {
                --*count_;
                if (*count_ == 0) {
                    delete p_;
                    delete count_;
                }
            }
        }
        T* p_;
        int* count_;
    };

    template <class T>
    shared_ptr<T> make_shared() {
        return shared_ptr<T>(new T());
    }
    template <class T, class A>
    shared_ptr<T> make_shared(A a) {
        return shared_ptr<T>(new T(a));
    }
    template <class T, class A, class B>
    shared_ptr<T> make_shared(A a, B b) {
        return shared_ptr<T>(new T(a, b));
    }
    template <class T, class A, class B, class C>
    shared_ptr<T> make_shared(A a, B b, C c) {
        return shared_ptr<T>(new T(a, b, c));
    }

    // --- std::optional (C++17) -------------------------------------------------------------

    struct nullopt_t {};
    nullopt_t nullopt;

    template <class T>
    class optional {
    public:
        optional() : has_(false), value_() {}
        optional(nullopt_t) : has_(false), value_() {}
        optional(const T& value) : has_(true), value_(value) {}
        bool has_value() const { return has_; }
        explicit operator bool() const { return has_; }
        T& value() {
            if (!has_) {
                throw bad_optional_access();
            }
            return value_;
        }
        const T& value() const {
            if (!has_) {
                throw bad_optional_access();
            }
            return value_;
        }
        T& operator*() {
            __cppi_bad_access(has_);
            return value_;
        }
        T* operator->() {
            __cppi_bad_access(has_);
            return &value_;
        }
        T value_or(const T& fallback) const { return has_ ? value_ : fallback; }
        void reset() { has_ = false; }

    private:
        bool has_;
        T value_;
    };

    // --- std::variant (C++17): up to four alternatives -------------------------------------
    // The active alternative lives on the heap, so only it is ever constructed.

    struct __variant_none_c {};
    struct __variant_none_d {};

    class bad_variant_access : public exception {
    public:
        bad_variant_access() : exception("bad variant access") {}
    };

    template <class A, class B, class C = __variant_none_c, class D = __variant_none_d>
    class variant {
    public:
        variant() : index_(0), a_(new A()), b_(0), c_(0), d_(0) {}
        variant(const A& value) : index_(0), a_(new A(value)), b_(0), c_(0), d_(0) {}
        variant(const B& value) : index_(1), a_(0), b_(new B(value)), c_(0), d_(0) {}
        variant(const C& value) : index_(2), a_(0), b_(0), c_(new C(value)), d_(0) {}
        variant(const D& value) : index_(3), a_(0), b_(0), c_(0), d_(new D(value)) {}
        variant(const variant& other) : index_(0), a_(0), b_(0), c_(0), d_(0) { copy_from(other); }
        ~variant() { reset(); }
        variant& operator=(const variant& other) {
            if (this != &other) {
                reset();
                copy_from(other);
            }
            return *this;
        }

        int index() const { return index_; }

        // Used by get, get_if, holds_alternative and visit: the pointer's type picks the alternative.
        int __index_of(const A*) const { return 0; }
        int __index_of(const B*) const { return 1; }
        int __index_of(const C*) const { return 2; }
        int __index_of(const D*) const { return 3; }
        A& __get(const A*) { return *a_; }
        B& __get(const B*) { return *b_; }
        C& __get(const C*) { return *c_; }
        D& __get(const D*) { return *d_; }
        const A& __get(const A*) const { return *a_; }
        const B& __get(const B*) const { return *b_; }
        const C& __get(const C*) const { return *c_; }
        const D& __get(const D*) const { return *d_; }

    private:
        void reset() {
            delete a_;
            delete b_;
            delete c_;
            delete d_;
            a_ = 0;
            b_ = 0;
            c_ = 0;
            d_ = 0;
        }
        void copy_from(const variant& other) {
            index_ = other.index_;
            if (other.a_ != 0) {
                a_ = new A(*other.a_);
            }
            if (other.b_ != 0) {
                b_ = new B(*other.b_);
            }
            if (other.c_ != 0) {
                c_ = new C(*other.c_);
            }
            if (other.d_ != 0) {
                d_ = new D(*other.d_);
            }
        }

        int index_;
        A* a_;
        B* b_;
        C* c_;
        D* d_;
    };

    template <class T, class A, class B, class C, class D>
    bool holds_alternative(const variant<A, B, C, D>& v) {
        return v.index() == v.__index_of(static_cast<const T*>(0));
    }
    template <class T, class A, class B, class C, class D>
    T& get(variant<A, B, C, D>& v) {
        if (v.index() != v.__index_of(static_cast<const T*>(0))) {
            throw bad_variant_access();
        }
        return v.__get(static_cast<const T*>(0));
    }
    template <class T, class A, class B, class C, class D>
    T* get_if(variant<A, B, C, D>* v) {
        if (v == 0 || v->index() != v->__index_of(static_cast<const T*>(0))) {
            return 0;
        }
        return &v->__get(static_cast<const T*>(0));
    }
    template <class F, class A, class B, class C, class D>
    auto visit(F f, const variant<A, B, C, D>& v) {
        if constexpr (__cppi_is_same<C, __variant_none_c>()) {
            if (v.index() == 0) {
                return f(v.__get(static_cast<const A*>(0)));
            }
            return f(v.__get(static_cast<const B*>(0)));
        } else if constexpr (__cppi_is_same<D, __variant_none_d>()) {
            if (v.index() == 0) {
                return f(v.__get(static_cast<const A*>(0)));
            }
            if (v.index() == 1) {
                return f(v.__get(static_cast<const B*>(0)));
            }
            return f(v.__get(static_cast<const C*>(0)));
        } else {
            if (v.index() == 0) {
                return f(v.__get(static_cast<const A*>(0)));
            }
            if (v.index() == 1) {
                return f(v.__get(static_cast<const B*>(0)));
            }
            if (v.index() == 2) {
                return f(v.__get(static_cast<const C*>(0)));
            }
            return f(v.__get(static_cast<const D*>(0)));
        }
    }

    }  // namespace std
)cpp";

}  // namespace

std::string_view Prelude::source() noexcept {
    return kSource;
}

}  // namespace cppi::library
