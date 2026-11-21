// TestFramework.h - a tiny test framework (no external libraries).
//
// How it works:
//   CHECK(cond)         -> if cond is false, print file:line and the condition, count a failure.
//   CHECK_THROWS(expr)  -> expr must throw an exception, otherwise it is a failure.
//   CHECK_EQ(a, b)      -> like CHECK(a == b) but prints both values when they differ.
//   runCase(name, fn)   -> runs one test case. If it throws (for example a TODO stub),
//                          the case is reported as CRASHED and the next case still runs.
//   testSummary()       -> prints "N passed, M failed" and returns the exit code for ctest
//                          (0 = everything passed, 1 = something failed).
//
// Why macros and not functions? Only a macro can capture __FILE__, __LINE__ and the
// source text of the condition (#cond), which makes failure messages useful.
#pragma once

#include <exception>
#include <iostream>

namespace test {

// "inline" lets these variables live in a header that several .cpp files include
// without breaking the one-definition rule (C++17 feature).
inline int passed = 0;
inline int failed = 0;

inline void report(bool ok, const char* expr, const char* file, int line) {
    if (ok) {
        ++passed;
    } else {
        ++failed;
        std::cout << "  FAIL " << file << ":" << line << "  " << expr << "\n";
    }
}

// Runs one named test case and survives exceptions thrown inside it.
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

inline void section(const char* title) { std::cout << "[" << title << "]\n"; }

inline int testSummary() {
    std::cout << "\n" << passed << " passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}

}  // namespace test end

#define CHECK(cond) test::report(static_cast<bool>(cond), #cond, __FILE__, __LINE__)

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

// Stricter: expr must throw exactly this exception type (or a subclass).
// A TODO stub throws std::logic_error, so it will NOT accidentally pass
// a CHECK_THROWS_AS(..., std::out_of_range).
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
