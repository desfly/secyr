#include "homeguard/bounded_write.hpp"
#include <cstdlib>
#include <string>
#define CHECK(x) do { if (!(x)) std::abort(); } while(false)
int main() {
    const std::string input(85049, 'x');
    std::string output;
    std::int64_t now = 0;
    auto clock = [&] { return now; };
    auto send = [&](const char* data, std::size_t size) {
        CHECK(size <= 1024);
        const auto count = size > 31 ? 31 : size;
        output.append(data, count);
        return static_cast<int>(count);
    };
    CHECK(hg::bounded_write(input.data(), input.size(), 100, send, clock));
    CHECK(output == input);
    auto zero = [](const char*, std::size_t) { return 0; };
    CHECK(!hg::bounded_write(input.data(), input.size(), 100, zero, clock));
    auto error = [](const char*, std::size_t) { return -3; };
    CHECK(!hg::bounded_write(input.data(), input.size(), 100, error, clock));
    unsigned calls = 0;
    auto slow = [&](const char*, std::size_t size) { ++calls; now += 60; return static_cast<int>(size); };
    CHECK(!hg::bounded_write(input.data(), input.size(), 100, slow, clock));
    CHECK(calls == 2);
}
