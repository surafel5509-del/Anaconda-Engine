// ============================================================================
//  tests/test_framework.hpp — a deliberately tiny assertion framework.
//
//  Why not GoogleTest? The Phase-1 test surface is small and host-only; a
//  100-line framework keeps `ctest` runs dependency-free and instant.
//  Swap to gtest in Month 3 if suites grow (documented in ROADMAP).
//
//  Usage:
//      TEST(my_case) { CHECK(1 + 1 == 2); CHECK_NEAR(0.1f + 0.2f, 0.3f, 1e-6f); }
//      int main() { return anaconda::test::runAll(); }
// ============================================================================
#pragma once

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace anaconda::test {

struct Case {
    const char* name;
    void (*fn)();
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

inline int& failureCount() {
    static int failures = 0;
    return failures;
}

inline const char*& currentCase() {
    static const char* name = "";
    return name;
}

struct Registrar {
    Registrar(const char* n, void (*f)()) { registry().push_back({n, f}); }
};

#define TEST(name)                                              \
    static void test_##name();                                  \
    static ::anaconda::test::Registrar reg_##name(#name,        \
                                                  &test_##name); \
    static void test_##name()

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            ++::anaconda::test::failureCount();                            \
            std::fprintf(stderr, "  [FAIL] %s:%d  %s  (in %s)\n",          \
                         __FILE__, __LINE__, #cond,                        \
                         ::anaconda::test::currentCase());                 \
        }                                                                  \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                              \
    do {                                                                   \
        const double _va = static_cast<double>(a);                         \
        const double _vb = static_cast<double>(b);                         \
        if (std::fabs(_va - _vb) > static_cast<double>(eps)) {             \
            ++::anaconda::test::failureCount();                            \
            std::fprintf(stderr,                                           \
                         "  [FAIL] %s:%d  |%s - %s| = %g > %g  (in %s)\n",  \
                         __FILE__, __LINE__, #a, #b, std::fabs(_va - _vb),  \
                         static_cast<double>(eps),                         \
                         ::anaconda::test::currentCase());                 \
        }                                                                  \
    } while (0)

inline int runAll() {
    int ran = 0;
    for (const Case& c : registry()) {
        currentCase() = c.name;
        std::printf("[ RUN  ] %s\n", c.name);
        const int before = failureCount();
        c.fn();
        ++ran;
        if (failureCount() == before) {
            std::printf("[  OK  ] %s\n", c.name);
        } else {
            std::printf("[ FAIL ] %s\n", c.name);
        }
    }
    std::printf("--------\n%d test(s), %d check failure(s)\n", ran,
                failureCount());
    return failureCount() == 0 ? 0 : 1;
}

}  // namespace anaconda::test
