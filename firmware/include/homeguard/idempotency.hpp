#pragma once
#include <cstddef>
#include <array>
#include <cstdint>
namespace hg {
class IdempotencyCache {
public:
 static constexpr size_t capacity=32;
 bool accept(uint64_t request_id, uint64_t issued_at_ms, uint64_t now_ms, uint32_t ttl_ms);
 [[nodiscard]] size_t size() const { return size_; }
private:
 struct Entry { uint64_t id{}; uint64_t seen_at{}; bool used{}; };
 std::array<Entry,capacity> entries_{}; size_t cursor_{}; size_t size_{};
};
}
