// this file is my small test framework (no library needed).
// CHECK(cond) -> fails if cond is false, CHECK_EQ(a, b) -> fails if a != b and prints both,
// CHECK_THROWS(expr) -> fails if expr does not throw
#pragma once // this prevents multiple inclusions of the header file.

#include <exception>
#include <iostream>

namespace test {

// inline -> lets these variables be in a header used by many .cpp files (C++17)
inline int passed = 0;
inline int failed = 0;

// counts pass / fail and prints where it failed
inline void report(bool ok, const char* expr, const char* file, int line) {
    if (ok) {
        ++passed;
    } else {
        ++failed;
        std::cout << "  FAIL " << file << ":" << line << "  " << expr << "\n";
    }
}

// runs one test, if it crashes with an exception we count it as failed and continue
inline void runCase(const char* name, void (*fn)()) {
    try {
        fn();
    } catch (const std::exception& e) {
        ++failed;
        std::cout << "  CRASHED in '" << name << "': " << e.what() << "\n";
    } catch (...) {
        ++failed;
        std::cout << "  CRASHED in '" << name << "': unknown exception\n";
    }
}

// prints a heading like [DynamicArray]
inline void section(const char* title) { std::cout << "[" << title << "]\n"; }

// prints the final result. returns 0 if all passed (ctest checks this)
inline int testSummary() {
    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}

}

// macros are used bcuz only a macro can get __FILE__, __LINE__ and the condition text
#define CHECK(cond) test::report(static_cast<bool>(cond), #cond, __FILE__, __LINE__)

// do { } while (0) makes the macro work like one statement
#define CHECK_EQ(a, b)                                                                  \
    do {                                                                                \
        auto check_a_ = (a);                                                            \
        auto check_b_ = (b);                                                            \
        bool check_ok_ = (check_a_ == check_b_);                                        \
        test::report(check_ok_, #a " == " #b, __FILE__, __LINE__);                      \
        if (!check_ok_) std::cout << "       got " << check_a_ << " vs " << check_b_ << "\n"; \
    } while (0)

#define CHECK_THROWS(expr)                                                \
    do {                                                                  \
        bool threw_ = false;                                              \
        try {                                                             \
            (void)(expr);                                                 \
        } catch (...) {                                                   \
            threw_ = true;                                                \
        }                                                                 \
        test::report(threw_, "throws: " #expr, __FILE__, __LINE__);      \
    } while (0)

// same as CHECK_THROWS but the exception must be of this type
#define CHECK_THROWS_AS(expr, ExceptionType)                                     \
    do {                                                                         \
        bool right_ = false;                                                     \
        try {                                                                    \
            (void)(expr);                                                        \
        } catch (const ExceptionType&) {                                         \
            right_ = true;                                                       \
        } catch (...) {                                                          \
        }                                                                        \
        test::report(right_, "throws " #ExceptionType ": " #expr, __FILE__, __LINE__); \
    } while (0)
