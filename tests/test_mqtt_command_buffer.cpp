#include "hg_mqtt_command_buffer.hpp"
#include <cassert>
#include <iostream>
#include <random>
#include <string>
using homeguard::idf::MqttCommandBuffer;
int main() {
    MqttCommandBuffer b;
    auto feed = [&](const char* topic, int topic_size, const char* data, int size,
                    int total, int offset, int id = 7) {
        return b.append("cmd", topic, topic_size, data, size, total, offset, id);
    };
    assert(feed("cmd", 3, "abc", 3, 3, 0).value() == "abc");
    assert(!feed("cmd", 3, "ab", 2, 4, 0));
    assert(feed(nullptr, 0, "cd", 2, 4, 2).value() == "abcd");
    assert(!feed(nullptr, 0, "cd", 2, 4, 2)); // duplicate final
    assert(!feed("bad", 3, "ab", 2, 4, 0));
    assert(!feed(nullptr, 0, "cd", 2, 4, 2));
    for (int variant = 0; variant < 8; ++variant) {
        assert(!feed("cmd", 3, "a", 1, 4, 0));
        switch (variant) {
        case 0: assert(!feed(nullptr, 0, "bc", 2, 4, 2)); break; // gap
        case 1: assert(!feed(nullptr, 0, "b", 1, 3, 1)); break; // changed total
        case 2: assert(!feed(nullptr, 0, "b", 1, 4, 1, 8)); break;
        case 3: assert(!feed("bad", 3, "b", 1, 4, 1)); break;
        case 4: assert(!feed(nullptr, 0, nullptr, 1, 4, 1)); break;
        case 5: assert(!feed(nullptr, -1, "b", 1, 4, 1)); break;
        case 6: assert(!feed(nullptr, 0, "b", 1, 4, -1)); break;
        case 7: b.reset(); break; // disconnect
        }
        assert(!feed(nullptr, 0, "bcd", 3, 4, 1));
    }
    assert(!feed("cmd", 3, "a", 1, 4097, 0));
    assert(!feed("cmd", 3, "a", -1, 1, 0));
    assert(!feed("cmd", 3, "a", 2, 1, 0));
    assert(!feed(nullptr, 3, "a", 1, 1, 0));
    assert(!feed("cmd", 3, "x", 1, 5, 0));
    assert(feed("cmd", 3, "new", 3, 3, 0).value() == "new");
    std::mt19937 rng(20261022);
    for (int trial = 0; trial < 3000; ++trial) {
        const int total = trial == 0 ? 4096 : 1 + static_cast<int>(rng() % 4096);
        std::string payload(static_cast<std::size_t>(total), '\0');
        for (char& c : payload) c = static_cast<char>(rng() % 256);
        for (int offset = 0; offset < total;) {
            const int size = 1 + static_cast<int>(rng() % static_cast<unsigned>(total - offset));
            const bool has_topic = offset == 0 || rng() % 2;
            const auto result = feed(has_topic ? "cmd" : nullptr, has_topic ? 3 : 0,
                payload.data() + offset, size, total, offset);
            offset += size;
            if (offset == total) assert(result.value() == payload);
            else assert(!result);
        }
    }
    std::cout << "MQTT buffer: malformed, disconnect, replacement and 3000 fragmentation scenarios PASS\n";
}
