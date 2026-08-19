#pragma once

#include "homeguard/sha256.hpp"

#include <string_view>

namespace hg {

class BearerTokenVerifier {
public:
    BearerTokenVerifier() = default;
    explicit BearerTokenVerifier(std::string_view token) { reset(token); }

    void reset(std::string_view token) {
        configured_ = token.size() >= 32U && token.size() <= 256U;
        digest_ = configured_ ? sha256(token) : Sha256Digest{};
    }

    void clear() {
        digest_.fill(0);
        configured_ = false;
    }

    [[nodiscard]] bool configured() const { return configured_; }

    [[nodiscard]] bool authorized(std::string_view authorization_header) const {
        constexpr std::string_view prefix = "Bearer ";
        if (!configured_ || !authorization_header.starts_with(prefix)) return false;
        const auto token = authorization_header.substr(prefix.size());
        if (token.empty() || token.find_first_of("\r\n\t ") != std::string_view::npos) return false;
        return constant_time_equal(digest_, sha256(token));
    }

private:
    Sha256Digest digest_{};
    bool configured_{};
};

}  // namespace hg
