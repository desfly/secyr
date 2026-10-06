#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
namespace hg {
// Transport writes are bounded independently of HTTP framing. No chunk encoding.
template<class Send, class Clock>
bool bounded_write(const char* data, std::size_t size, std::int64_t deadline,
                   Send&& send, Clock&& clock) {
    while (size != 0U) {
        if (clock() >= deadline) return false;
        const auto count = std::min<std::size_t>(size, 1024U);
        const int sent = send(data, count);
        if (sent <= 0 || static_cast<std::size_t>(sent) > count) return false;
        data += sent;
        size -= static_cast<std::size_t>(sent);
    }
    return true;
}
} // namespace hg
