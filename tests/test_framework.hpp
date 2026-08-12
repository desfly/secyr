#pragma once

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace testfw {
inline int checks = 0;

inline void check(bool value, std::string_view expr, const char* file, int line) {
    ++checks;
    if (!value) {
        std::cerr << file << ":" << line << " CHECK failed: " << expr << "\n";
        std::exit(1);
    }
}

// Compatibility for Build-0051/0052 tests written before CHECK became the
// single canonical assertion API. Keep these wrappers so the full historical
// regression suite remains executable instead of silently falling out of CI.
inline void require(bool value, std::string_view message) {
    check(value, message, "legacy-test", 0);
}

inline void expect(bool value, std::string_view message) {
    check(value, message, "legacy-test", 0);
}
}  // namespace testfw

#define CHECK(x) ::testfw::check(static_cast<bool>(x), #x, __FILE__, __LINE__)
#define TEST_CHECK(x) CHECK(x)
