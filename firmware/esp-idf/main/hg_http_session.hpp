#pragma once

#include "homeguard/local_api.hpp"
#include "esp_random.h"
#include "esp_timer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

namespace homeguard::idf::http_session {

inline constexpr std::int64_t kLifetimeUs = 15LL * 60LL * 1'000'000LL;
inline constexpr std::size_t kCapacity = 4U;
inline std::array<hg::BearerTokenVerifier, kCapacity> g_tokens{};
inline std::array<std::int64_t, kCapacity> g_issued_us{};
inline std::size_t g_next{};
inline std::mutex g_mutex{};

inline std::string issue() {
    std::array<std::uint8_t, 32> random{};
    esp_fill_random(random.data(), random.size());
    static constexpr char hex[] = "0123456789abcdef";
    std::string raw(random.size() * 2U, '\0');
    for (std::size_t i = 0; i < random.size(); ++i) {
        raw[i * 2U] = hex[(random[i] >> 4U) & 0x0fU];
        raw[i * 2U + 1U] = hex[random[i] & 0x0fU];
    }

    std::scoped_lock lock(g_mutex);
    g_tokens[g_next].reset(raw);
    g_issued_us[g_next] = esp_timer_get_time();
    g_next = (g_next + 1U) % kCapacity;
    return raw;
}

inline bool authorized(std::string_view authorization) {
    if (!authorization.starts_with("Bearer ")) return false;
    const auto now = esp_timer_get_time();
    std::scoped_lock lock(g_mutex);
    for (std::size_t i = 0; i < g_tokens.size(); ++i) {
        auto& token = g_tokens[i];
        if (!token.configured()) continue;
        const auto issued = g_issued_us[i];
        if (issued <= 0 || now < issued || now - issued > kLifetimeUs) {
            token.clear();
            g_issued_us[i] = 0;
            continue;
        }
        if (token.authorized(authorization)) return true;
    }
    return false;
}

inline bool revoke(std::string_view authorization) {
    if (!authorization.starts_with("Bearer ")) return false;
    std::scoped_lock lock(g_mutex);
    for (std::size_t i = 0; i < g_tokens.size(); ++i) {
        auto& token = g_tokens[i];
        if (!token.configured()) continue;
        if (token.authorized(authorization)) {
            token.clear();
            g_issued_us[i] = 0;
            return true;
        }
    }
    return false;
}

inline void revoke_all() {
    std::scoped_lock lock(g_mutex);
    for (auto& token : g_tokens) token.clear();
    g_issued_us.fill(0);
    g_next = 0U;
}

}  // namespace homeguard::idf::http_session
