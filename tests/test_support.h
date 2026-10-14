#pragma once

#include <iostream>
#include <string_view>

namespace fps::test {

inline int& FailureCount() {
    static int failures = 0;
    return failures;
}

inline void ExpectTrue(bool condition, std::string_view expression, std::string_view file, int line) {
    if (!condition) {
        ++FailureCount();
        std::cerr << file << ':' << line << ": expected true: " << expression << '\n';
    }
}

template <typename Actual, typename Expected>
inline void ExpectEq(
    const Actual& actual,
    const Expected& expected,
    std::string_view actual_expression,
    std::string_view expected_expression,
    std::string_view file,
    int line) {
    if (!(actual == expected)) {
        ++FailureCount();
        std::cerr << file << ':' << line << ": expected equality: "
                  << actual_expression << " == " << expected_expression << '\n';
    }
}

}  // namespace fps::test

#define FPS_EXPECT_TRUE(expr) \
    ::fps::test::ExpectTrue(static_cast<bool>(expr), #expr, __FILE__, __LINE__)

#define FPS_EXPECT_EQ(actual, expected) \
    ::fps::test::ExpectEq((actual), (expected), #actual, #expected, __FILE__, __LINE__)
