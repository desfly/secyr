#pragma once

#include <array>
#include <cstddef>
#include <cstring>
#include <optional>
#include <string_view>

namespace homeguard::idf {

// Owned by the MQTT event task. Never expose a partial command to the parser.
class MqttCommandBuffer {
public:
    static constexpr std::size_t capacity = 4096;
    void reset() { received_ = 0; total_ = 0; active_ = false; }

    std::optional<std::string_view> append(std::string_view expected_topic,
        const char* topic, int topic_size, const char* data, int size,
        int total, int offset, int message_id)
    {
        if (offset == 0) reset();
        if (topic_size < 0 || size <= 0 || total <= 0 || offset < 0 ||
            total > static_cast<int>(capacity) || offset > total || size > total - offset ||
            data == nullptr || (topic_size > 0 && topic == nullptr)) {
            reset();
            return std::nullopt;
        }
        const std::string_view incoming_topic = topic_size > 0
            ? std::string_view(topic, static_cast<std::size_t>(topic_size)) : std::string_view{};
        if (offset == 0) {
            if (incoming_topic != expected_topic) return std::nullopt;
            active_ = true;
            total_ = static_cast<std::size_t>(total);
            message_id_ = message_id;
        }
        if (!active_ || total_ != static_cast<std::size_t>(total) ||
            received_ != static_cast<std::size_t>(offset) || message_id_ != message_id ||
            (!incoming_topic.empty() && incoming_topic != expected_topic)) {
            reset();
            return std::nullopt;
        }
        std::memcpy(bytes_.data() + received_, data, static_cast<std::size_t>(size));
        received_ += static_cast<std::size_t>(size);
        if (received_ != total_) return std::nullopt;
        const auto result = std::string_view(bytes_.data(), received_);
        reset();
        return result; // Valid until the next append; consumer must use synchronously.
    }

private:
    std::array<char, capacity> bytes_{};
    std::size_t received_{};
    std::size_t total_{};
    int message_id_{};
    bool active_{};
};

} // namespace homeguard::idf
