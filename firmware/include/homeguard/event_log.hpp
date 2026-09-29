#pragma once
#include <cstddef>
#include "homeguard/types.hpp"
#include <array>
#include <cstdint>
#include <string_view>
namespace hg {
struct Event { uint64_t sequence{}; uint64_t timestamp_ms{}; Severity severity{Severity::Info}; uint16_t code{}; uint16_t source_id{}; int32_t value{}; std::array<char,48> text{}; };
class EventLog {
public:
    static constexpr size_t capacity=128;
    void append(uint64_t timestamp_ms, Severity severity, uint16_t code, std::string_view text, uint16_t source_id = 0, int32_t value = 0);
    [[nodiscard]] size_t size() const { return size_; }
    [[nodiscard]] uint64_t next_sequence() const { return next_sequence_; }
    [[nodiscard]] const Event& at_oldest(size_t index) const;
private:
    std::array<Event,capacity> events_{}; size_t head_{}; size_t size_{}; uint64_t next_sequence_{1};
};
}
